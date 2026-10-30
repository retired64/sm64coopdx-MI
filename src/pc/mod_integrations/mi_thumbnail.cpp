#include "mi_thumbnail.h"
#include "mi_network.h"
#include "mi_sha256.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <filesystem>
#include <mutex>
#include <pthread.h>
#include <vector>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctime>

// Private decoder: engine decoder globals and settings are never shared.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-parameter"
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#include <stb/stb_image.h>
#pragma clang diagnostic pop

namespace fs=std::filesystem;
namespace {
constexpr size_t MAX_DOWNLOAD=2*1024*1024,MAX_CACHE=64;
constexpr time_t CACHE_AGE=7*24*3600;
std::mutex decoderMutex,cacheMutex;
struct Error{};
void check(bool value){if(!value)throw Error{};}
struct File{int fd;~File(){if(fd>=0)close(fd);}};
std::string hash(const void* data,size_t bytes){MiSha256 h;h.update(data,bytes);return h.finish();}
bool regular(const fs::path& p){struct stat st{};return lstat(p.c_str(),&st)==0&&S_ISREG(st.st_mode);}
void directory(const fs::path& p){check(mkdir(p.c_str(),0700)==0||errno==EEXIST);struct stat st{};check(lstat(p.c_str(),&st)==0&&S_ISDIR(st.st_mode));}
}
struct MiThumbnail {
    std::atomic<unsigned> owners{1};std::atomic<bool> cancel{false};std::mutex mutex;
    MiThumbnailState state=MI_THUMB_LOADING;bool taken=false;
    std::string root,url;std::vector<uint8_t> pixels;
};
namespace {
void release(MiThumbnail* op){if(op->owners.fetch_sub(1)==1)delete op;}
void cancelled(MiThumbnail* op){check(!op->cancel);}
bool cache_load(const fs::path& path,std::vector<uint8_t>& pixels){
    try{
        std::lock_guard<std::mutex> guard(cacheMutex);File file{open(path.c_str(),O_RDONLY|O_NOFOLLOW)};if(file.fd<0)return false;
        struct stat st{};check(fstat(file.fd,&st)==0&&S_ISREG(st.st_mode)&&st.st_size==MI_THUMB_BYTES+72);
        time_t now=time(nullptr);check(st.st_mtime<=now&&now-st.st_mtime<CACHE_AGE);
        std::vector<uint8_t> body(MI_THUMB_BYTES+72);size_t offset=0;
        while(offset<body.size()){ssize_t n=read(file.fd,body.data()+offset,body.size()-offset);check(n>0);offset+=size_t(n);}
        check(!memcmp(body.data(),"MITHUMB1",8));
        check(hash(body.data()+72,MI_THUMB_BYTES)==std::string(reinterpret_cast<char*>(body.data()+8),64));
        pixels.assign(body.begin()+72,body.end());return true;
    }catch(...){return false;}
}
void cache_save(const fs::path& root,const fs::path& path,const std::vector<uint8_t>& pixels){
    std::string temporary;
    try{
        std::lock_guard<std::mutex> guard(cacheMutex);
        directory(root/"mi");directory(root/"mi/thumbnails");
        temporary=path.string()+".tmp-XXXXXX";File file{mkstemp(&temporary[0])};check(file.fd>=0);
        std::string header="MITHUMB1"+hash(pixels.data(),pixels.size());
        for(auto pair:{std::make_pair(static_cast<const void*>(header.data()),header.size()),std::make_pair(static_cast<const void*>(pixels.data()),pixels.size())}){
            size_t offset=0;while(offset<pair.second){ssize_t n=write(file.fd,static_cast<const uint8_t*>(pair.first)+offset,pair.second-offset);check(n>0);offset+=size_t(n);}
        }
        check(fsync(file.fd)==0&&rename(temporary.c_str(),path.c_str())==0);temporary.clear();
        std::vector<std::pair<time_t,fs::path>> entries;size_t count=0;
        for(const auto& item:fs::directory_iterator(path.parent_path())){
            if(++count>256)break;auto name=item.path().filename().string();
            if(name.size()!=69||name.substr(64)!=".rgba"||!std::all_of(name.begin(),name.begin()+64,[](char c){return(c>='a'&&c<='f')||(c>='0'&&c<='9');})||!regular(item.path()))continue;
            struct stat st{};if(lstat(item.path().c_str(),&st)==0)entries.push_back({st.st_mtime,item.path()});
        }
        std::sort(entries.begin(),entries.end());
        for(size_t i=0;i+MAX_CACHE<entries.size();i++)unlink(entries[i].second.c_str());
    }catch(...){if(!temporary.empty())unlink(temporary.c_str());} // Cache is optional.
}
struct Transfer {MiThumbnail* op;std::vector<uint8_t> bytes;};
size_t receive(char* data,size_t size,size_t count,void* user){
    auto* transfer=static_cast<Transfer*>(user);
    if(transfer->op->cancel|| (size&&count>MAX_DOWNLOAD/size))return 0;
    size_t length=size*count;if(length>MAX_DOWNLOAD-transfer->bytes.size())return 0;
    try{transfer->bytes.insert(transfer->bytes.end(),data,data+length);return length;}catch(...){return 0;}
}
int progress(void* user,curl_off_t,curl_off_t,curl_off_t,curl_off_t){return static_cast<MiThumbnail*>(user)->cancel?1:0;}
std::vector<uint8_t> download(MiThumbnail* op){
    bool loopback=op->url.compare(0,17,"http://127.0.0.1:")==0;
    check(loopback||op->url.compare(0,8,"https://")==0);check(mi_curl_init()==CURLE_OK);
    std::unique_ptr<CURL,decltype(&curl_easy_cleanup)> curl(curl_easy_init(),curl_easy_cleanup);check(bool(curl));Transfer transfer{op,{}};
    curl_easy_setopt(curl.get(),CURLOPT_URL,op->url.c_str());curl_easy_setopt(curl.get(),CURLOPT_USERAGENT,"sm64coopdx-MI/0.3 thumbnails");
    curl_easy_setopt(curl.get(),CURLOPT_WRITEFUNCTION,receive);curl_easy_setopt(curl.get(),CURLOPT_WRITEDATA,&transfer);
    curl_easy_setopt(curl.get(),CURLOPT_XFERINFOFUNCTION,progress);curl_easy_setopt(curl.get(),CURLOPT_XFERINFODATA,op);
    curl_easy_setopt(curl.get(),CURLOPT_NOPROGRESS,0L);curl_easy_setopt(curl.get(),CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(curl.get(),CURLOPT_CONNECTTIMEOUT,8L);curl_easy_setopt(curl.get(),CURLOPT_TIMEOUT,20L);
    curl_easy_setopt(curl.get(),CURLOPT_LOW_SPEED_LIMIT,128L);curl_easy_setopt(curl.get(),CURLOPT_LOW_SPEED_TIME,8L);
    curl_easy_setopt(curl.get(),CURLOPT_MAXFILESIZE_LARGE,curl_off_t(MAX_DOWNLOAD));curl_easy_setopt(curl.get(),CURLOPT_FOLLOWLOCATION,1L);
    curl_easy_setopt(curl.get(),CURLOPT_MAXREDIRS,3L);curl_easy_setopt(curl.get(),CURLOPT_FAILONERROR,1L);
    curl_easy_setopt(curl.get(),CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(curl.get(),CURLOPT_SSL_VERIFYHOST,2L);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    curl_easy_setopt(curl.get(),CURLOPT_PROTOCOLS,loopback?CURLPROTO_HTTP:CURLPROTO_HTTPS);
    curl_easy_setopt(curl.get(),CURLOPT_REDIR_PROTOCOLS,loopback?CURLPROTO_HTTP:CURLPROTO_HTTPS);
#pragma clang diagnostic pop
#ifdef __ANDROID__
    std::string certificates=mi_system_certificates();check(!certificates.empty());
    curl_blob roots{&certificates[0],certificates.size(),CURL_BLOB_COPY};
    curl_easy_setopt(curl.get(),CURLOPT_CAINFO,static_cast<const char*>(nullptr));curl_easy_setopt(curl.get(),CURLOPT_CAPATH,static_cast<const char*>(nullptr));
    check(curl_easy_setopt(curl.get(),CURLOPT_CAINFO_BLOB,&roots)==CURLE_OK);
#endif
    check(curl_easy_perform(curl.get())==CURLE_OK);long http=0;curl_easy_getinfo(curl.get(),CURLINFO_RESPONSE_CODE,&http);
    check(http>=200&&http<300&&!transfer.bytes.empty());cancelled(op);return std::move(transfer.bytes);
}
std::vector<uint8_t> decode(const std::vector<uint8_t>& bytes){
    // Restrict formats and dimensions before any full image allocation.
    check((bytes.size()>=8&&!memcmp(bytes.data(),"\x89PNG\r\n\x1a\n",8))||(bytes.size()>=3&&bytes[0]==255&&bytes[1]==216&&bytes[2]==255));
    std::lock_guard<std::mutex> guard(decoderMutex);int width=0,height=0,channels=0;
    check(stbi_info_from_memory(bytes.data(),int(bytes.size()),&width,&height,&channels)&&width>0&&height>0&&width<=1024&&height<=1024);
    std::unique_ptr<stbi_uc,decltype(&stbi_image_free)> image(stbi_load_from_memory(bytes.data(),int(bytes.size()),&width,&height,&channels,4),stbi_image_free);
    check(bool(image)&&width>0&&height>0&&width<=1024&&height<=1024);
    std::vector<uint8_t> pixels(MI_THUMB_BYTES);int crop=std::min(width,height),left=(width-crop)/2,top=(height-crop)/2;
    for(int y=0;y<MI_THUMB_EDGE;y++)for(int x=0;x<MI_THUMB_EDGE;x++){
        float sx=left+(x+0.5f)*crop/MI_THUMB_EDGE-0.5f,sy=top+(y+0.5f)*crop/MI_THUMB_EDGE-0.5f;
        sx=std::max(0.0f,std::min(float(width-1),sx));sy=std::max(0.0f,std::min(float(height-1),sy));
        int x0=int(sx),y0=int(sy),x1=std::min(x0+1,width-1),y1=std::min(y0+1,height-1);float fx=sx-x0,fy=sy-y0;
        for(int c=0;c<4;c++){
            float a=image.get()[(y0*width+x0)*4+c]*(1-fx)+image.get()[(y0*width+x1)*4+c]*fx;
            float b=image.get()[(y1*width+x0)*4+c]*(1-fx)+image.get()[(y1*width+x1)*4+c]*fx;
            pixels[(y*MI_THUMB_EDGE+x)*4+c]=uint8_t(a*(1-fy)+b*fy+0.5f);
        }
    }
    return pixels;
}
void work(MiThumbnail* op){
    MiThumbnailState state=MI_THUMB_FAILED;std::vector<uint8_t> pixels;
    try{
        cancelled(op);fs::path root;try{root=fs::canonical(op->root);}catch(...){}
        fs::path path=root/"mi/thumbnails"/(hash(op->url.data(),op->url.size())+".rgba");
        bool safe=false;
        try{safe=!root.empty()&&!fs::is_symlink(root/"mi")&&!fs::is_symlink(root/"mi/thumbnails");}catch(...){}
        if(!safe||!cache_load(path,pixels)){
            auto bytes=download(op);cancelled(op);pixels=decode(bytes);cancelled(op);
            if(safe)cache_save(root,path,pixels);
        }
        cancelled(op);state=MI_THUMB_READY;
    }catch(...){if(op->cancel)state=MI_THUMB_CANCELLED;}
    {std::lock_guard<std::mutex> guard(op->mutex);op->state=state;op->pixels=std::move(pixels);}
    release(op);
}
struct Queue {
    std::mutex mutex;std::condition_variable changed;std::deque<MiThumbnail*> pending;unsigned workers=0;
    static void* run(void* context){
        auto* queue=static_cast<Queue*>(context);
        for(;;){MiThumbnail* op;{std::unique_lock<std::mutex> lock(queue->mutex);queue->changed.wait(lock,[&]{return !queue->pending.empty();});op=queue->pending.front();queue->pending.pop_front();}work(op);}
        return nullptr;
    }
    Queue(){
        pthread_attr_t attr;if(pthread_attr_init(&attr))return;
        if(!pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED))for(unsigned i=0;i<2;i++){pthread_t thread;if(!pthread_create(&thread,&attr,run,this))workers++;}
        pthread_attr_destroy(&attr);
    }
    bool add(MiThumbnail* op){
        std::lock_guard<std::mutex> guard(mutex);
        for(auto i=pending.begin();i!=pending.end();)if((*i)->cancel){release(*i);i=pending.erase(i);}else ++i;
        if(!workers||pending.size()>=16)return false;
        pending.push_back(op);op->owners++;changed.notify_one();return true;
    }
};
// Process-lifetime queue: two idle workers, no engine/UI references or exit join.
Queue* queue(){static Queue* value=new(std::nothrow)Queue;return value;}
}
extern "C" MiThumbnail* mi_thumbnail_start(const char* root,const char* url){
    if(!root||!url||strlen(url)>2047)return nullptr;auto* op=new(std::nothrow)MiThumbnail;if(!op)return nullptr;
    try{op->root=root;op->url=url;auto* jobs=queue();if(!jobs||!jobs->add(op))op->state=MI_THUMB_FAILED;}
    catch(...){op->state=MI_THUMB_FAILED;}
    return op;
}
extern "C" MiThumbnailState mi_thumbnail_state(MiThumbnail* op){if(!op)return MI_THUMB_FAILED;std::lock_guard<std::mutex> guard(op->mutex);return op->state;}
extern "C" bool mi_thumbnail_take(MiThumbnail* op,uint8_t* pixels,size_t capacity){
    if(!op||!pixels||capacity<MI_THUMB_BYTES)return false;std::lock_guard<std::mutex> guard(op->mutex);
    if(op->state!=MI_THUMB_READY||op->taken||op->pixels.size()!=MI_THUMB_BYTES)return false;
    memcpy(pixels,op->pixels.data(),MI_THUMB_BYTES);op->taken=true;op->pixels.clear();return true;
}
extern "C" void mi_thumbnail_close(MiThumbnail* op){if(op){op->cancel=true;release(op);}}
