#include "mi_updates.h"
#include "mi_install.h"
#include "mi_sha256.h"
#include "pc/utils/json.hpp"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <pthread.h>
#include <set>
#include <string>
#include <vector>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace fs=std::filesystem;
using nlohmann::json;
namespace {
struct Date {int64_t first=0,last=0;bool valid=false;};
int digits(const std::string& s,size_t offset,size_t count){
    if(offset+count>s.size())return -1;int value=0;
    for(size_t i=offset;i<offset+count;i++){if(s[i]<'0'||s[i]>'9')return -1;value=value*10+s[i]-'0';}
    return value;
}
Date date(const std::string& s){
    if(s.size()<10||s[4]!='-'||s[7]!='-')return {};
    int y=digits(s,0,4),m=digits(s,5,2),d=digits(s,8,2);
    static const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    if(y<1970||y>9999||m<1||m>12||d<1||d>days[m-1]+(m==2&&y%4==0&&(y%100!=0||y%400==0)))return {};
    struct tm tm{};tm.tm_year=y-1900;tm.tm_mon=m-1;tm.tm_mday=d;
    int64_t start=timegm(&tm);
    if(s.size()==10)return {start,start+86399,true};
    if(s.size()<20||s[10]!='T'||s[13]!=':'||s[16]!=':')return {};
    int h=digits(s,11,2),minute=digits(s,14,2),second=digits(s,17,2);
    if(h<0||h>23||minute<0||minute>59||second<0||second>59)return {};
    size_t zone=19;
    if(s[zone]=='.') {zone++;size_t begin=zone;while(zone<s.size()&&s[zone]>='0'&&s[zone]<='9')zone++;if(zone==begin)return {};}
    int offset=0;
    if(zone+1==s.size()&&s[zone]=='Z'){}
    else if(zone+6==s.size()&&(s[zone]=='+'||s[zone]=='-')&&s[zone+3]==':'){
        int hours=digits(s,zone+1,2),minutes=digits(s,zone+4,2);
        if(hours<0||hours>14||minutes<0||minutes>59||(hours==14&&minutes))return {};
        offset=(hours*60+minutes)*60*(s[zone]=='+'?1:-1);
    }else return {};
    // Fractional seconds deliberately retain second precision: ties are ambiguous.
    start+=h*3600+minute*60+second-offset;return {start,start,true};
}
Date date(const MiCatalogVersion& v){
    if(!v.exact)return {};
    if(v.timestamp[0])return date(v.timestamp);
    if(v.date[0])return date(v.date);
    return date(v.providerDate);
}
std::string label(const char* raw){
    std::string s;
    for(unsigned char c:std::string(raw?raw:""))if(c!=' '&&c!='\t'&&c!='\r'&&c!='\n')s+=c>='A'&&c<='Z'?c+32:c;
    if(s.size()>1&&s[0]=='v'&&s[1]>='0'&&s[1]<='9')s.erase(0,1);
    return s;
}
bool missing_label(const std::string& s){return s.empty()||s=="unknown"||s=="n/a"||s=="none"||s=="untitled";}
bool component(const std::string& s){
    return !s.empty()&&s.size()<=240&&s[0]!='.'&&std::none_of(s.begin(),s.end(),[](unsigned char c){return c<32||c==127||c=='/'||c=='\\'||c==':';});
}
bool identity(const std::string& s,size_t cap){
    return !s.empty()&&s.size()<cap&&std::none_of(s.begin(),s.end(),[](unsigned char c){return c<32||c==127;});
}
std::string folded(std::string s){for(char& c:s)if(c>='A'&&c<='Z')c+=32;return s;}
}
extern "C" MiUpdateDecision mi_updates_compare(const MiCatalog* catalog,size_t index,const char* versionId,const char* fileId,MiUpdateCandidate* out){
    try{
        auto* entry=mi_catalog_entry(catalog,index);
        if(!entry||!entry->versionExact||!versionId||!fileId||!out)return MI_UPDATE_UNKNOWN;
        const MiCatalogVersion* installed=nullptr;
        std::vector<const MiCatalogVersion*> possible;
        std::string current=label(entry->version);
        if(missing_label(current))return MI_UPDATE_UNKNOWN;
        for(size_t i=0;i<mi_catalog_version_count(catalog,index);i++){
            auto* version=mi_catalog_version(catalog,index,i);
            if(version->id==std::string(versionId))installed=version;
            if(label(version->label)==current)possible.push_back(version);
        }
        if(!installed||!installed->exact||possible.empty())return MI_UPDATE_UNKNOWN;
        const MiCatalogVersion* latest=nullptr;
        for(auto* candidate:possible){
            Date value=date(*candidate);if(!value.valid)return MI_UPDATE_UNKNOWN;
            bool newest=true;
            for(auto* other:possible)if(other!=candidate){Date b=date(*other);if(!b.valid||value.first<=b.last){newest=false;break;}}
            if(newest){if(latest)return MI_UPDATE_UNKNOWN;latest=candidate;}
        }
        if(!latest||!latest->resolved)return MI_UPDATE_UNKNOWN;
        if(!strcmp(latest->id,versionId))return MI_UPDATE_NONE;
        Date old=date(*installed),next=date(*latest);
        if(!old.valid||!next.valid)return MI_UPDATE_UNKNOWN;
        if(next.last<old.first)return MI_UPDATE_NONE; // No downgrade when overview is stale.
        if(next.first<=old.last)return MI_UPDATE_UNKNOWN;
        const MiCatalogFile* original=nullptr;const MiCatalogFile* target=nullptr;
        for(size_t i=0;i<mi_catalog_file_count(catalog,index);i++){
            auto* file=mi_catalog_file(catalog,index,i);
            if(!strcmp(file->versionId,versionId)&&!strcmp(file->fileId,fileId))original=file;
        }
        if(!original||!original->role[0])return MI_UPDATE_UNKNOWN;
        for(size_t i=0;i<mi_catalog_file_count(catalog,index);i++){
            auto* file=mi_catalog_file(catalog,index,i);
            if(strcmp(file->versionId,latest->id)||strcmp(file->role,original->role))continue;
            if(!file->supported||(!strcmp(file->role,"primary")&&!file->recommended))return MI_UPDATE_UNKNOWN;
            if(target)return MI_UPDATE_UNKNOWN;target=file;
        }
        if(!target)return MI_UPDATE_UNKNOWN;
        *out={};out->file=*target;snprintf(out->installedVersion,sizeof(out->installedVersion),"%s",original->version);
        return MI_UPDATE_AVAILABLE;
    }catch(...){return MI_UPDATE_UNKNOWN;}
}

struct MiUpdates {
    std::atomic<unsigned> owners{1};std::atomic<bool> cancel{false};std::mutex mutex;
    MiUpdatesInfo info{MI_UPDATES_RUNNING,MI_UPDATES_OK,0,0,0,0};
    std::string root;std::unique_ptr<MiCatalog,decltype(&mi_catalog_free)> catalog{nullptr,mi_catalog_free};
    std::vector<MiUpdateCandidate> candidates;
};
namespace {
void release(MiUpdates* op){if(op->owners.fetch_sub(1)==1)delete op;}
struct Error{MiUpdatesError code;};
void check(bool ok,MiUpdatesError code=MI_UPDATES_STORAGE){if(!ok)throw Error{code};}
void cancelled(MiUpdates* op){if(op->cancel)throw Error{MI_UPDATES_OK};}
json read(const fs::path& p,size_t& budget){
    int fd=open(p.c_str(),O_RDONLY|O_NOFOLLOW);check(fd>=0);
    std::unique_ptr<int,void(*)(int*)> owner(&fd,[](int* x){close(*x);});
    struct stat st{};check(fstat(fd,&st)==0&&S_ISREG(st.st_mode)&&st.st_size>0&&st.st_size<=4*1024*1024&&size_t(st.st_size)<=budget);
    budget-=size_t(st.st_size);std::string text(size_t(st.st_size),'\0');size_t offset=0;
    while(offset<text.size()){ssize_t n=::read(fd,&text[offset],text.size()-offset);check(n>0);offset+=size_t(n);}
    return json::parse(text);
}
bool present(const fs::path& target,const json& files){
    check(!fs::is_symlink(target));check(files.is_array()&&!files.empty()&&files.size()<=4096);
    for(const auto& f:files){
        std::string relative=f.at("path");check(relative.size()<=512&&relative.find('\0')==std::string::npos&&relative.find('\\')==std::string::npos&&relative.find("//")==std::string::npos);
        check(relative.empty()||relative.front()!='/');fs::path path=target;unsigned depth=0;
        for(const auto& part:fs::path(relative)){std::string name=part.string();check(name!="."&&name!=".."&&!name.empty()&&++depth<=24);path/=part;check(!fs::is_symlink(path));}
        if(!fs::is_regular_file(path))return false;
    }
    return true;
}
bool journal_pending(const fs::path& root){
    auto p=root/"mi/transactions";if(!fs::exists(p))return false;check(!fs::is_symlink(p)&&fs::is_directory(p));
    for(const auto& item:fs::directory_iterator(p))if(item.path().extension()==".json")return true;
    return false;
}
void* work(void* data){
    auto* op=static_cast<MiUpdates*>(data);MiUpdatesInfo info{MI_UPDATES_DONE,MI_UPDATES_OK,0,0,0,0};std::vector<MiUpdateCandidate> results;
    uint64_t generation=mi_install_revision();
    try{
        cancelled(op);check(mi_catalog_status(op->catalog.get())==MI_CATALOG_READY,MI_UPDATES_INVALID);
        check(!mi_install_is_busy(),MI_UPDATES_BUSY);fs::path root=fs::canonical(op->root);check(fs::is_directory(root));
        check(!journal_pending(root),MI_UPDATES_BUSY);
        auto directory=root/"mi/receipts";
        if(fs::exists(directory)){
            check(!fs::is_symlink(root/"mi")&&!fs::is_symlink(directory)&&fs::is_directory(directory));
            check(!fs::is_symlink(root/"mods"));
            struct Receipt{std::string mod,version,file,destination;};std::vector<Receipt> receipts;
            std::map<std::string,unsigned> destinations,mods;size_t budget=32*1024*1024,count=0;
            for(const auto& item:fs::directory_iterator(directory)){
                cancelled(op);if(item.path().extension()!=".json")continue;check(++count<=4096);
                try{
                    json record=read(item.path(),budget);check(record.at("schema")==1);
                    Receipt receipt{record.at("mod_id"),record.at("version_id"),record.at("file_id"),record.at("destination")};
                    check(identity(receipt.mod,64)&&identity(receipt.version,160)&&identity(receipt.file,160)&&component(receipt.destination));
                    MiSha256 hash;std::string key=receipt.mod+"\n"+receipt.version+"\n"+receipt.file;hash.update(key.data(),key.size());
                    check(item.path().filename()==hash.finish()+".json");info.checked++;
                    auto target=root/"mods"/receipt.destination;
                    if(!fs::exists(target)&&!fs::is_symlink(target)){info.missing++;continue;}
                    if(!present(target,record.at("files"))){info.uncertain++;continue;}
                    destinations[folded(receipt.destination)]++;mods[receipt.mod]++;receipts.push_back(receipt);
                }catch(...){info.uncertain++;}
            }
            for(const auto& receipt:receipts){
                cancelled(op);
                if(destinations[folded(receipt.destination)]!=1||mods[receipt.mod]!=1){info.uncertain++;continue;}
                size_t index=0;for(;index<mi_catalog_count(op->catalog.get());index++)if(mi_catalog_entry(op->catalog.get(),index)->id==receipt.mod)break;
                MiUpdateCandidate candidate{};auto decision=mi_updates_compare(op->catalog.get(),index,receipt.version.c_str(),receipt.file.c_str(),&candidate);
                if(decision==MI_UPDATE_AVAILABLE){snprintf(candidate.destination,sizeof(candidate.destination),"%s",receipt.destination.c_str());results.push_back(candidate);}
                else if(decision==MI_UPDATE_UNKNOWN)info.uncertain++;
            }
        }
        cancelled(op);check(!mi_install_is_busy()&&generation==mi_install_revision()&&!journal_pending(root),MI_UPDATES_BUSY);
        info.available=results.size();
    }catch(const Error& e){info.state=op->cancel?MI_UPDATES_CANCELLED:MI_UPDATES_FAILED;info.error=e.code;results.clear();info.available=0;}
    catch(...){info.state=op->cancel?MI_UPDATES_CANCELLED:MI_UPDATES_FAILED;info.error=MI_UPDATES_STORAGE;results.clear();info.available=0;}
    op->catalog.reset();
    {std::lock_guard<std::mutex> lock(op->mutex);op->info=info;op->candidates=std::move(results);}
    release(op);return nullptr;
}
}
extern "C" MiUpdates* mi_updates_start(const char* root,const MiCatalog* catalog){
    if(!root||!catalog)return nullptr;auto* op=new(std::nothrow)MiUpdates;if(!op)return nullptr;
    try{op->root=root;op->catalog.reset(mi_catalog_clone(catalog));if(!op->catalog){delete op;return nullptr;}}catch(...){delete op;return nullptr;}
    pthread_attr_t attr;int error=pthread_attr_init(&attr);
    if(!error){error=pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
        if(!error){op->owners++;pthread_t thread;error=pthread_create(&thread,&attr,work,op);if(error)release(op);}pthread_attr_destroy(&attr);}
    if(error){op->info.state=MI_UPDATES_FAILED;op->info.error=MI_UPDATES_STORAGE;}
    return op;
}
extern "C" bool mi_updates_poll(MiUpdates* op,MiUpdatesInfo* info){if(!op||!info)return false;std::lock_guard<std::mutex> lock(op->mutex);*info=op->info;return true;}
extern "C" bool mi_updates_get(MiUpdates* op,const char* id,MiUpdateCandidate* candidate){
    if(!op||!id||!candidate)return false;std::lock_guard<std::mutex> lock(op->mutex);
    if(op->info.state!=MI_UPDATES_DONE)return false;
    for(const auto& item:op->candidates)if(!strcmp(item.file.modId,id)){*candidate=item;return true;}
    return false;
}
extern "C" void mi_updates_close(MiUpdates* op){if(op){op->cancel=true;release(op);}}
