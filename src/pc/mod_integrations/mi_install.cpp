#include "mi_install.h"
#include "mi_network.h"
#include "mi_sha256.h"
#include "pc/utils/json.hpp"
#include "pc/utils/miniz/miniz.h"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <set>
#include <vector>
#include <algorithm>
#include <mutex>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <cerrno>
#include <ctime>

namespace fs = std::filesystem;
using nlohmann::json;
namespace {
constexpr uint64_t MAX_DOWNLOAD = 512ULL*1024*1024, MAX_EXPANDED = 1024ULL*1024*1024;
constexpr uint64_t MAX_FILE = 256ULL*1024*1024;
constexpr unsigned MAX_FILES = 4096;
std::atomic<bool> busy{false};
std::atomic<uint64_t> installRevision{0};
struct Failure { MiInstallError code; };
void require(bool ok, MiInstallError code) { if (!ok) throw Failure{code}; }
struct Descriptor {
    int fd;
    explicit Descriptor(int value):fd(value){}
    ~Descriptor(){if(fd>=0)close(fd);}
};
using Stream = std::unique_ptr<FILE, decltype(&fclose)>;
}
struct MiInstall {
    std::atomic<int> owners{1};
    std::atomic<bool> cancelled{false};
    std::mutex mutex;
    MiInstallInfo info{MI_INSTALL_PENDING,MI_INSTALL_OK,-1,0,0,0,{0}};
    MiCatalogFile source{};
    std::string root;
};
namespace {
void release(MiInstall* op) { if(op->owners.fetch_sub(1)==1)delete op; }
void cancelled(MiInstall* op) { require(!op->cancelled, MI_INSTALL_OK); }
void update(MiInstall* op, MiInstallState state, float progress=-1, uint64_t bytes=0,uint64_t total=0) {
    std::lock_guard<std::mutex> lock(op->mutex);
    if(op->cancelled && state<MI_INSTALL_COMPLETE)throw Failure{MI_INSTALL_OK};
    op->info.state=state;op->info.progress=progress;op->info.bytes=bytes;op->info.total=total;
}
std::string hash_text(const std::string& value) { MiSha256 hash;hash.update(value.data(),value.size());return hash.finish(); }
bool component(const std::string& value) {
    return !value.empty() && value.size()<=240 && value!="." && value!=".." && value[0]!='.' &&
        std::all_of(value.begin(),value.end(),[](unsigned char c){return c>=32&&c!=127&&c!='/'&&c!='\\'&&c!=':';});
}
bool relative_path(const std::string& path) {
    if(path.empty()||path.size()>512||path[0]=='/'||path.back()=='/'||path.find('\\')!=std::string::npos||path.find("//")!=std::string::npos)return false;
    unsigned depth=0;
    for(const auto& part:fs::path(path)) {
        std::string p=part.string();
        if(p.empty()||p=="."||p==".."||p.size()>240||++depth>24)return false;
        for(unsigned char c:p)if(c<32||c==127||c==':')return false;
    }
    return true;
}
void directory(const fs::path& path) {
    if(mkdir(path.c_str(),0700)!=0)require(errno==EEXIST,MI_INSTALL_STORAGE);
    struct stat st{};require(lstat(path.c_str(),&st)==0&&S_ISDIR(st.st_mode),MI_INSTALL_UNSAFE);
}
std::string hash_file(const fs::path& path, MiInstall* op=nullptr) {
    Descriptor fd(open(path.c_str(),O_RDONLY|O_NOFOLLOW));require(fd.fd>=0,MI_INSTALL_STORAGE);
    struct stat st{};require(fstat(fd.fd,&st)==0&&S_ISREG(st.st_mode)&&st.st_size>=0&&uint64_t(st.st_size)<=MAX_FILE,MI_INSTALL_LIMIT);
    MiSha256 hash;char buffer[65536];ssize_t n;
    while((n=read(fd.fd,buffer,sizeof(buffer)))>0){if(op)cancelled(op);hash.update(buffer,size_t(n));}
    require(n==0,MI_INSTALL_STORAGE);return hash.finish();
}
void atomic_json(const fs::path& path,const json& value) {
    std::string temp=path.string()+".tmp-XXXXXX";Descriptor fd(mkstemp(&temp[0]));require(fd.fd>=0,MI_INSTALL_STORAGE);
    try {
        std::string body=value.dump(2);size_t offset=0;
        while(offset<body.size()){ssize_t n=write(fd.fd,body.data()+offset,body.size()-offset);require(n>0,MI_INSTALL_STORAGE);offset+=size_t(n);}
        require(fsync(fd.fd)==0,MI_INSTALL_STORAGE);
        require(rename(temp.c_str(),path.c_str())==0,MI_INSTALL_STORAGE);
    } catch(...){unlink(temp.c_str());throw;}
}
json read_json(const fs::path& path) {
    Descriptor fd(open(path.c_str(),O_RDONLY|O_NOFOLLOW));require(fd.fd>=0,MI_INSTALL_RECOVERY);
    struct stat st{};require(fstat(fd.fd,&st)==0&&S_ISREG(st.st_mode)&&st.st_size>0&&st.st_size<=4*1024*1024,MI_INSTALL_RECOVERY);
    std::string body(size_t(st.st_size),'\0');size_t offset=0;
    while(offset<body.size()){ssize_t n=read(fd.fd,&body[offset],body.size()-offset);require(n>0,MI_INSTALL_RECOVERY);offset+=size_t(n);}
    return json::parse(body);
}
std::string receipt_key(const MiCatalogFile& file) {return hash_text(std::string(file.modId)+"\n"+file.versionId+"\n"+file.fileId);}
struct Transfer {MiInstall* op;FILE* file;uint64_t bytes=0;bool limit=false,storage=false;};
size_t receive(char* data,size_t size,size_t count,void* context) {
    auto& transfer=*static_cast<Transfer*>(context);
    if(transfer.op->cancelled)return 0;
    if(size && count>MAX_DOWNLOAD/size){transfer.limit=true;return 0;}
    size_t n=size*count;if(n>MAX_DOWNLOAD-transfer.bytes){transfer.limit=true;return 0;}
    size_t written=fwrite(data,1,n,transfer.file);transfer.bytes+=written;
    if(written!=n)transfer.storage=true;return written;
}
int transfer_progress(void* context,curl_off_t total,curl_off_t bytes,curl_off_t,curl_off_t) {
    auto* op=static_cast<MiInstall*>(context);if(op->cancelled)return 1;
    std::lock_guard<std::mutex> lock(op->mutex);
    op->info.bytes=bytes>0?uint64_t(bytes):0;op->info.total=total>0?uint64_t(total):0;
    op->info.progress=total>0?std::min(1.0f,float(bytes)/float(total)):-1;return 0;
}
void download(MiInstall* op,const std::string& url,const fs::path& target) {
    bool loopback=url.compare(0,17,"http://127.0.0.1:")==0;
    require(loopback||url.compare(0,8,"https://")==0,MI_INSTALL_UNSAFE);
    require(mi_curl_init()==CURLE_OK,MI_INSTALL_NETWORK);
    update(op,MI_INSTALL_DOWNLOADING);
    Descriptor fd(open(target.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600));require(fd.fd>=0,MI_INSTALL_STORAGE);
    FILE* file=fdopen(dup(fd.fd),"wb");require(file!=nullptr,MI_INSTALL_STORAGE);Stream output(file,fclose);
    std::unique_ptr<CURL,decltype(&curl_easy_cleanup)> handle(curl_easy_init(),curl_easy_cleanup);require(bool(handle),MI_INSTALL_NETWORK);
    Transfer transfer{op,file};
    curl_easy_setopt(handle.get(),CURLOPT_URL,url.c_str());
    curl_easy_setopt(handle.get(),CURLOPT_USERAGENT,"sm64coopdx-MI/0.2 installer");
    curl_easy_setopt(handle.get(),CURLOPT_WRITEFUNCTION,receive);curl_easy_setopt(handle.get(),CURLOPT_WRITEDATA,&transfer);
    curl_easy_setopt(handle.get(),CURLOPT_XFERINFOFUNCTION,transfer_progress);curl_easy_setopt(handle.get(),CURLOPT_XFERINFODATA,op);
    curl_easy_setopt(handle.get(),CURLOPT_NOPROGRESS,0L);curl_easy_setopt(handle.get(),CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(handle.get(),CURLOPT_CONNECTTIMEOUT,15L);curl_easy_setopt(handle.get(),CURLOPT_TIMEOUT,1800L);
    curl_easy_setopt(handle.get(),CURLOPT_LOW_SPEED_LIMIT,128L);curl_easy_setopt(handle.get(),CURLOPT_LOW_SPEED_TIME,30L);
    curl_easy_setopt(handle.get(),CURLOPT_FOLLOWLOCATION,1L);curl_easy_setopt(handle.get(),CURLOPT_MAXREDIRS,5L);
    curl_easy_setopt(handle.get(),CURLOPT_MAXFILESIZE_LARGE,curl_off_t(MAX_DOWNLOAD));
    curl_easy_setopt(handle.get(),CURLOPT_FAILONERROR,1L);curl_easy_setopt(handle.get(),CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(handle.get(),CURLOPT_SSL_VERIFYHOST,2L);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    curl_easy_setopt(handle.get(),CURLOPT_PROTOCOLS,loopback?CURLPROTO_HTTP:CURLPROTO_HTTPS);
    curl_easy_setopt(handle.get(),CURLOPT_REDIR_PROTOCOLS,loopback?CURLPROTO_HTTP:CURLPROTO_HTTPS);
#pragma GCC diagnostic pop
#ifdef __ANDROID__
    std::string certificates=mi_system_certificates();require(!certificates.empty(),MI_INSTALL_TLS);
    curl_blob roots{&certificates[0],certificates.size(),CURL_BLOB_COPY};
    curl_easy_setopt(handle.get(),CURLOPT_CAINFO,static_cast<const char*>(nullptr));curl_easy_setopt(handle.get(),CURLOPT_CAPATH,static_cast<const char*>(nullptr));
    require(curl_easy_setopt(handle.get(),CURLOPT_CAINFO_BLOB,&roots)==CURLE_OK,MI_INSTALL_TLS);
#endif
    CURLcode result=curl_easy_perform(handle.get());long http=0;curl_easy_getinfo(handle.get(),CURLINFO_RESPONSE_CODE,&http);
    {std::lock_guard<std::mutex> lock(op->mutex);op->info.httpStatus=http;}
    cancelled(op);
    require(!transfer.storage,MI_INSTALL_STORAGE);require(!transfer.limit&&result!=CURLE_FILESIZE_EXCEEDED,MI_INSTALL_LIMIT);
    require(result!=CURLE_PEER_FAILED_VERIFICATION&&result!=CURLE_SSL_CACERT_BADFILE&&result!=CURLE_SSL_CONNECT_ERROR,MI_INSTALL_TLS);
    require(!(result==CURLE_HTTP_RETURNED_ERROR||(http&&(http<200||http>=300))),MI_INSTALL_HTTP);
    require(result==CURLE_OK,MI_INSTALL_NETWORK);
    require(http>=200&&http<300,MI_INSTALL_HTTP);
    require(transfer.bytes>0,MI_INSTALL_FORMAT);
    require(fflush(file)==0&&fsync(fd.fd)==0,MI_INSTALL_STORAGE);
    require(output.release()!=nullptr,MI_INSTALL_STORAGE);require(fclose(file)==0,MI_INSTALL_STORAGE);
}

void validate_lua(const fs::path& path) {
    std::ifstream stream(path,std::ios::binary);require(bool(stream),MI_INSTALL_STORAGE);
    char sample[4096];stream.read(sample,sizeof(sample));std::string text(sample,size_t(stream.gcount()));
    require(!text.empty()&&text.find('\0')==std::string::npos&&uint8_t(text[0])!=0x1b,MI_INSTALL_FORMAT);
    std::string lower=text;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return c>='A'&&c<='Z'?c+32:c;});
    require(lower.find("<!doctype html")==std::string::npos&&lower.find("<html")==std::string::npos,MI_INSTALL_FORMAT);
    // This is a structural check, not Lua execution or gameplay certification.
    require(text.find("PK\x03\x04")!=0,MI_INSTALL_FORMAT);
}
struct ZipOwner {mz_zip_archive archive{};bool open=false;~ZipOwner(){if(open)mz_zip_reader_end(&archive);}};
struct Extract {MiInstall* op;FILE* file;uint64_t size=0,expected,completed,total;};
size_t extract_write(void* context,mz_uint64 offset,const void* data,size_t bytes) {
    auto& state=*static_cast<Extract*>(context);
    if(state.op->cancelled||offset!=state.size||bytes>state.expected-state.size)return 0;
    size_t written=fwrite(data,1,bytes,state.file);state.size+=written;
    {
        std::lock_guard<std::mutex> guard(state.op->mutex);
        state.op->info.bytes=state.completed+state.size;state.op->info.total=state.total;
        state.op->info.progress=state.total?float(state.completed+state.size)/float(state.total):-1;
    }
    return written;
}
fs::path unpack(MiInstall* op,const fs::path& payload,const fs::path& stage) {
    update(op,MI_INSTALL_EXTRACTING,0);
    ZipOwner zip;zip.open=mz_zip_reader_init_file(&zip.archive,payload.c_str(),0);require(zip.open,MI_INSTALL_FORMAT);
    unsigned count=mz_zip_reader_get_num_files(&zip.archive);require(count>0&&count<=MAX_FILES,MI_INSTALL_LIMIT);
    struct Entry{unsigned index;std::string name;bool directory;uint64_t bytes;};std::vector<Entry> entries;std::set<std::string> seen;uint64_t total=0;
    for(unsigned i=0;i<count;++i){
        cancelled(op);mz_zip_archive_file_stat st{};require(mz_zip_reader_file_stat(&zip.archive,i,&st),MI_INSTALL_FORMAT);
        require(mz_zip_reader_get_filename(&zip.archive,i,nullptr,0)==strlen(st.m_filename)+1,MI_INSTALL_UNSAFE);
        std::string name=st.m_filename;bool dir=mz_zip_reader_is_file_a_directory(&zip.archive,i);if(dir&&!name.empty()&&name.back()=='/')name.pop_back();
        require(relative_path(name),MI_INSTALL_UNSAFE);
        auto kind=(st.m_external_attr>>16)&S_IFMT;require(kind==0||kind==(dir?S_IFDIR:S_IFREG),MI_INSTALL_UNSAFE);
        require(!st.m_is_encrypted&&mz_zip_reader_is_file_supported(&zip.archive,i),MI_INSTALL_FORMAT);
        require(st.m_uncomp_size<=MAX_FILE&&st.m_uncomp_size<=MAX_EXPANDED-total,MI_INSTALL_LIMIT);total+=st.m_uncomp_size;
        require(st.m_uncomp_size<=2*1024*1024||st.m_uncomp_size<=std::max<uint64_t>(1,st.m_comp_size)*200,MI_INSTALL_LIMIT);
        std::string folded=name;std::transform(folded.begin(),folded.end(),folded.begin(),[](unsigned char c){return c>='A'&&c<='Z'?c+32:c;});
        require(seen.insert(folded).second,MI_INSTALL_UNSAFE);
        if(name=="__MACOSX"||name.compare(0,9,"__MACOSX/")==0||fs::path(name).filename()==".DS_Store")continue;
        entries.push_back({i,name,dir,st.m_uncomp_size});
    }
    fs::path tree=stage/"tree";directory(tree);uint64_t done=0;
    for(const auto& entry:entries){
        cancelled(op);fs::path dest=tree/entry.name;fs::create_directories(dest.parent_path());
        if(entry.directory){directory(dest);continue;}
        Descriptor fd(open(dest.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600));require(fd.fd>=0,MI_INSTALL_STORAGE);
        FILE* file=fdopen(dup(fd.fd),"wb");require(file!=nullptr,MI_INSTALL_STORAGE);Stream out(file,fclose);Extract writer{op,file,0,entry.bytes,done,total};
        bool ok=mz_zip_reader_extract_to_callback(&zip.archive,entry.index,extract_write,&writer,0);
        cancelled(op);require(ok&&writer.size==entry.bytes,MI_INSTALL_FORMAT);
        require(fflush(file)==0&&fsync(fd.fd)==0,MI_INSTALL_STORAGE);out.release();require(fclose(file)==0,MI_INSTALL_STORAGE);
        done+=entry.bytes;update(op,MI_INSTALL_EXTRACTING,total?float(done)/float(total):1,done,total);
    }
    fs::path candidate=tree;
    for(unsigned depth=0;depth<24;++depth){
        if(fs::is_regular_file(candidate/"main.lua")){validate_lua(candidate/"main.lua");return candidate;}
        std::vector<fs::path> children;for(const auto& child:fs::directory_iterator(candidate))children.push_back(child.path());
        if(children.size()==1&&fs::is_directory(children[0])){candidate=children[0];continue;}
        if(children.size()==1&&children[0].extension()==".lua"&&fs::is_regular_file(children[0])){validate_lua(children[0]);return children[0];}
        throw Failure{MI_INSTALL_FORMAT};
    }
    throw Failure{MI_INSTALL_LIMIT};
}
json manifest(MiInstall* op,const fs::path& candidate) {
    update(op,MI_INSTALL_VERIFYING,-1);json files=json::array();uint64_t total=0;
    auto add=[&](const fs::path& p,const std::string& relative){
        cancelled(op);require(!fs::is_symlink(p)&&fs::is_regular_file(p),MI_INSTALL_UNSAFE);
        auto size=fs::file_size(p);require(files.size()<MAX_FILES&&size<=MAX_FILE&&size<=MAX_EXPANDED-total,MI_INSTALL_LIMIT);total+=size;
        files.push_back({{"path",relative},{"size",size},{"sha256",hash_file(p,op)}});
    };
    if(fs::is_directory(candidate)){
        for(const auto& child:fs::recursive_directory_iterator(candidate)){
            require(!child.is_symlink(),MI_INSTALL_UNSAFE);if(child.is_regular_file())add(child.path(),child.path().lexically_relative(candidate).generic_string());
        }
    }else add(candidate,"");
    require(!files.empty(),MI_INSTALL_FORMAT);return files;
}
void verify_manifest(const fs::path& target,const json& files) {
    require(!fs::is_symlink(target),MI_INSTALL_RECOVERY);
    require(files.is_array()&&!files.empty()&&files.size()<=MAX_FILES,MI_INSTALL_RECOVERY);
    std::set<std::string> expected;
    for(const auto& file:files){
        std::string relative=file.at("path").get<std::string>();
        require(relative.empty()||relative_path(relative),MI_INSTALL_RECOVERY);
        require(expected.insert(relative).second,MI_INSTALL_RECOVERY);
        fs::path p=target;
        for(const auto& part:fs::path(relative)) {p/=part;require(!fs::is_symlink(p),MI_INSTALL_RECOVERY);}
        require(fs::is_regular_file(p),MI_INSTALL_RECOVERY);
        require(fs::file_size(p)==file.at("size").get<uint64_t>()&&hash_file(p)==file.at("sha256").get<std::string>(),MI_INSTALL_RECOVERY);
    }
    if(fs::is_directory(target)){
        size_t count=0;
        for(const auto& child:fs::recursive_directory_iterator(target)){
            require(!child.is_symlink(),MI_INSTALL_RECOVERY);
            if(child.is_regular_file()) {require(++count<=MAX_FILES&&expected.count(child.path().lexically_relative(target).generic_string()),MI_INSTALL_RECOVERY);}
            else require(child.is_directory(),MI_INSTALL_RECOVERY);
        }
        require(count==expected.size(),MI_INSTALL_RECOVERY);
    }else require(expected.size()==1&&expected.count(""),MI_INSTALL_RECOVERY);
}
void finish_receipt(const fs::path& receipt,const json& record) {
    if(fs::exists(receipt)){
        require(read_json(receipt)==record,MI_INSTALL_RECOVERY);
    }else atomic_json(receipt,record);
}
void recover(const fs::path& mi,const fs::path& mods) {
    for(const auto& item:fs::directory_iterator(mi/"transactions")){
        if(item.path().extension()!=".json")continue;
        json journal=read_json(item.path());require(journal.at("schema")==1,MI_INSTALL_RECOVERY);
        std::string destination=journal.at("receipt").at("destination");std::string stageName=journal.at("stage");std::string candidate=journal.at("candidate");
        std::string key=journal.at("key");
        require(component(destination)&&component(stageName)&&stageName.compare(0,6,"stage-")==0&&relative_path(candidate)&&key.size()==64&&
            std::all_of(key.begin(),key.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}),MI_INSTALL_RECOVERY);
        fs::path stage=mi/stageName,target=mods/destination;
        struct stat st{};
        if(lstat((stage/candidate).c_str(),&st)==0){
            require(uint64_t(st.st_ino)==journal.at("inode").get<uint64_t>()&&uint64_t(st.st_dev)==journal.at("device").get<uint64_t>(),MI_INSTALL_RECOVERY);
            fs::remove_all(stage);require(unlink(item.path().c_str())==0,MI_INSTALL_STORAGE);continue;
        }
        require(lstat(target.c_str(),&st)==0&&!S_ISLNK(st.st_mode)&&uint64_t(st.st_ino)==journal.at("inode").get<uint64_t>()&&
            uint64_t(st.st_dev)==journal.at("device").get<uint64_t>(),MI_INSTALL_RECOVERY);
        verify_manifest(target,journal.at("receipt").at("files"));
        finish_receipt(mi/"receipts"/(key+".json"),journal.at("receipt"));
        fs::remove_all(stage);require(unlink(item.path().c_str())==0,MI_INSTALL_STORAGE);
    }
}
void deliver(const fs::path& from,const fs::path& to) {
#ifdef SYS_renameat2
    if(syscall(SYS_renameat2,AT_FDCWD,from.c_str(),AT_FDCWD,to.c_str(),1)==0)return; // RENAME_NOREPLACE
    throw Failure{errno==EEXIST?MI_INSTALL_EXISTS:MI_INSTALL_STORAGE};
#else
    (void)from;(void)to;throw Failure{MI_INSTALL_STORAGE};
#endif
}
void* work(void* context) {
    auto* op=static_cast<MiInstall*>(context);fs::path stage,journalPath;bool delivered=false,complete=false;
    try{
        cancelled(op);require(op->source.supported,MI_INSTALL_FORMAT);
        fs::path root=fs::canonical(op->root),mi=root/"mi",mods=root/"mods";
        directory(mi);directory(mods);directory(mi/"receipts");directory(mi/"transactions");
        Descriptor lock(open((mi/"install.lock").c_str(),O_RDWR|O_CREAT|O_NOFOLLOW,0600));require(lock.fd>=0,MI_INSTALL_STORAGE);
        // Android emulated storage may return ENOSYS for flock. POSIX record
        // locks work there and are released by the kernel on close/process death.
        struct flock recordLock{};
        recordLock.l_type=F_WRLCK;recordLock.l_whence=SEEK_SET;
        if(fcntl(lock.fd,F_SETLK,&recordLock)!=0)
            throw Failure{errno==EACCES||errno==EAGAIN?MI_INSTALL_BUSY:MI_INSTALL_STORAGE};
        try{recover(mi,mods);}catch(...){throw Failure{MI_INSTALL_RECOVERY};}
        // The lock excludes active installers. Discard only our abandoned,
        // unjournalled staging directories; nothing under mods is removed.
        for(const auto& item:fs::directory_iterator(mi)){
            std::string name=item.path().filename().string();
            bool ours=name.size()==12&&name.compare(0,6,"stage-")==0&&
                std::all_of(name.begin()+6,name.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9');});
            if(ours&&!item.is_symlink()&&item.is_directory()&&!fs::exists(mi/"transactions"/(name+".json")))fs::remove_all(item.path());
        }
        cancelled(op);
        std::string key=receipt_key(op->source);fs::path receipt=mi/"receipts"/(key+".json");
        if(fs::exists(receipt)){
            auto prior=read_json(receipt);std::string dest=prior.at("destination");
            require(prior.at("schema")==1&&prior.at("mod_id")==op->source.modId&&prior.at("version_id")==op->source.versionId&&prior.at("file_id")==op->source.fileId&&component(dest),MI_INSTALL_RECOVERY);
            verify_manifest(mods/dest,prior.at("files"));
            {std::lock_guard<std::mutex> guard(op->mutex);snprintf(op->info.destination,sizeof(op->info.destination),"%s",dest.c_str());}
            complete=true;
        }else{
            std::string name=op->source.filename;require(component(name),MI_INSTALL_UNSAFE);
            std::string temp=(mi/"stage-XXXXXX").string();require(mkdtemp(&temp[0])!=nullptr,MI_INSTALL_STORAGE);stage=temp;
            fs::path payload=stage/"download";
            try{download(op,op->source.url,payload);}catch(const Failure& failure){
                bool fallback=failure.code==MI_INSTALL_NETWORK||failure.code==MI_INSTALL_HTTP||failure.code==MI_INSTALL_TLS;
                if(!fallback||!op->source.mirror[0]||op->cancelled)throw;
                fs::remove(payload);download(op,op->source.mirror,payload);
            }
            if(op->source.size)require(fs::file_size(payload)==op->source.size,MI_INSTALL_INTEGRITY);
            // Download cap is larger than individual extracted-file cap.
            if(op->source.sha256[0]){
                std::ifstream input(payload,std::ios::binary);require(bool(input),MI_INSTALL_STORAGE);MiSha256 hash;char b[65536];
                while(input){cancelled(op);input.read(b,sizeof(b));hash.update(b,size_t(input.gcount()));}
                require(input.eof()&&hash.finish()==op->source.sha256,MI_INSTALL_INTEGRITY);
            }
            fs::path candidate;
            if(fs::path(name).extension()==".lua"){
                require(fs::file_size(payload)<=MAX_FILE,MI_INSTALL_LIMIT);validate_lua(payload);candidate=payload;
            }else if(fs::path(name).extension()==".zip")candidate=unpack(op,payload,stage);
            else throw Failure{MI_INSTALL_FORMAT};
            bool folder=fs::is_directory(candidate);std::string destination=folder?fs::path(name).stem().string():
                (fs::path(name).extension()==".lua"?name:candidate.filename().string());
            require(component(destination),MI_INSTALL_UNSAFE);
            {std::lock_guard<std::mutex> guard(op->mutex);snprintf(op->info.destination,sizeof(op->info.destination),"%s",destination.c_str());}
            json files=manifest(op,candidate);cancelled(op);
            struct stat present{};require(lstat((mods/destination).c_str(),&present)!=0&&errno==ENOENT,MI_INSTALL_EXISTS);
            json record={{"schema",1},{"mod_id",op->source.modId},{"version_id",op->source.versionId},{"file_id",op->source.fileId},
                {"version",op->source.version},{"destination",destination},{"files",files},{"installed_at",time(nullptr)}};
            struct stat st{};require(lstat(candidate.c_str(),&st)==0,MI_INSTALL_STORAGE);
            update(op,MI_INSTALL_COMMITTING,-1); // Cancellation is no longer accepted during atomic delivery.
            journalPath=mi/"transactions"/(stage.filename().string()+".json");
            atomic_json(journalPath,{{"schema",1},{"key",key},{"stage",stage.filename().string()},
                {"candidate",candidate.lexically_relative(stage).generic_string()},{"inode",uint64_t(st.st_ino)},
                {"device",uint64_t(st.st_dev)},{"receipt",record}});
            deliver(candidate,mods/destination);delivered=true;
            try{finish_receipt(receipt,record);}catch(...){throw Failure{MI_INSTALL_RECOVERY};}
            require(unlink(journalPath.c_str())==0,MI_INSTALL_RECOVERY);journalPath.clear();complete=true;
        }
    }catch(const Failure& failure){
        std::lock_guard<std::mutex> guard(op->mutex);
        op->info.state=op->cancelled&&!delivered?MI_INSTALL_CANCELLED:MI_INSTALL_FAILED;
        op->info.error=delivered?MI_INSTALL_RECOVERY:failure.code;op->info.progress=-1;
    }catch(...){
        std::lock_guard<std::mutex> guard(op->mutex);op->info.state=op->cancelled&&!delivered?MI_INSTALL_CANCELLED:MI_INSTALL_FAILED;
        op->info.error=delivered?MI_INSTALL_RECOVERY:MI_INSTALL_STORAGE;op->info.progress=-1;
    }
    std::error_code error;
    if(!stage.empty())fs::remove_all(stage,error);
    if(!delivered&&!journalPath.empty())fs::remove(journalPath,error);
    {
        std::lock_guard<std::mutex> guard(op->mutex);
        installRevision++;busy=false;
        if(complete){op->info.state=MI_INSTALL_COMPLETE;op->info.error=MI_INSTALL_OK;op->info.progress=1;}
    }
    release(op);return nullptr;
}
}
extern "C" MiInstall* mi_install_start(const char* userRoot,const MiCatalogFile* source) {
    if(!userRoot||!source)return nullptr;
    auto* op=new(std::nothrow)MiInstall;if(!op)return nullptr;
    try{op->root=userRoot;op->source=*source;}catch(...){delete op;return nullptr;}
    if(busy.exchange(true)){op->info.state=MI_INSTALL_FAILED;op->info.error=MI_INSTALL_BUSY;return op;}
    installRevision++;
    pthread_attr_t attr;int result=pthread_attr_init(&attr);
    if(!result){result=pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
        if(!result){op->owners++;pthread_t thread;result=pthread_create(&thread,&attr,work,op);if(result)release(op);}
        pthread_attr_destroy(&attr);}
    if(result){installRevision++;busy=false;op->info.state=MI_INSTALL_FAILED;op->info.error=MI_INSTALL_STORAGE;}
    return op;
}
extern "C" bool mi_install_poll(MiInstall* op,MiInstallInfo* info) {
    if(!op||!info)return false;std::lock_guard<std::mutex> lock(op->mutex);*info=op->info;return true;
}
extern "C" void mi_install_cancel(MiInstall* op) {
    if(!op)return;std::lock_guard<std::mutex> lock(op->mutex);
    if(op->info.state<MI_INSTALL_COMMITTING){op->cancelled=true;op->info.state=MI_INSTALL_CANCELLING;}
}
extern "C" void mi_install_close(MiInstall* op) {if(op){mi_install_cancel(op);release(op);}}

extern "C" uint64_t mi_install_revision(void){return installRevision.load();}
extern "C" bool mi_install_is_busy(void){return busy.load();}
