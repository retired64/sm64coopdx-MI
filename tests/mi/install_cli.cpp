#include "pc/mod_integrations/mi_install.h"
#include "pc/mod_integrations/mi_sha256.h"
#include "pc/utils/json.hpp"
#include <cstdio>
#include <thread>
#include <chrono>
#include <cstring>
#include <cassert>
int main(int argc,char** argv){
    MiSha256 empty;assert(empty.finish()=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    MiSha256 abc;abc.update("a",1);abc.update("bc",2);assert(abc.finish()=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    MiSha256 million;for(int i=0;i<1000000;i++)million.update("a",1);assert(million.finish()=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    if(argc<4)return 2;
    MiCatalogFile file{};snprintf(file.modId,sizeof(file.modId),"test-mod");snprintf(file.versionId,sizeof(file.versionId),"v1");snprintf(file.fileId,sizeof(file.fileId),"file-1");
    snprintf(file.filename,sizeof(file.filename),"%s",argv[3]);snprintf(file.url,sizeof(file.url),"%s",argv[2]);file.supported=true;
    if(argc>5)snprintf(file.sha256,sizeof(file.sha256),"%s",argv[5]);
    MiInstall* op=mi_install_start(argv[1],&file);assert(op);MiInstallInfo info{};
    bool cancelled=false,busyTested=false,sawDownload=false;uint64_t maxBytes=0;
    for(int i=0;i<6000;++i){
        assert(mi_install_poll(op,&info));
        if(info.state==MI_INSTALL_DOWNLOADING){sawDownload=true;maxBytes=std::max(maxBytes,info.bytes);}
        if(argc>4&&strcmp(argv[4],"cancel")==0&&!cancelled&&info.state==MI_INSTALL_DOWNLOADING){mi_install_cancel(op);cancelled=true;}
        if(argc>4&&strcmp(argv[4],"busy")==0&&!busyTested&&info.state==MI_INSTALL_DOWNLOADING){
            auto* other=mi_install_start(argv[1],&file);MiInstallInfo second;assert(mi_install_poll(other,&second)&&second.error==MI_INSTALL_BUSY);mi_install_close(other);busyTested=true;
        }
        if(info.state==MI_INSTALL_COMPLETE||info.state==MI_INSTALL_FAILED||info.state==MI_INSTALL_CANCELLED)break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    auto result=nlohmann::json{{"state",info.state},{"error",info.error},{"destination",info.destination},{"http",info.httpStatus},{"saw_download",sawDownload},{"max_bytes",maxBytes}};
    mi_install_close(op);std::this_thread::sleep_for(std::chrono::milliseconds(50));puts(result.dump().c_str());
    return info.state==MI_INSTALL_COMPLETE||info.state==MI_INSTALL_FAILED||info.state==MI_INSTALL_CANCELLED?0:3;
}
