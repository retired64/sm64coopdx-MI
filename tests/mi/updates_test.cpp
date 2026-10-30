#include "pc/mod_integrations/mi_updates.h"
#include "pc/mod_integrations/mi_sha256.h"
#include "pc/utils/json.hpp"
#include <cassert>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
using nlohmann::json;
namespace fs=std::filesystem;
static std::atomic<bool> installing{false};
extern "C" bool mi_install_is_busy(void){return installing;}
extern "C" uint64_t mi_install_revision(void){return 0;}
using Catalog=std::unique_ptr<MiCatalog,decltype(&mi_catalog_free)>;
static Catalog parse(const json& value){auto text=value.dump();return Catalog(mi_catalog_parse(text.data(),text.size()),mi_catalog_free);}
static MiUpdateDecision compare(const json& value,MiUpdateCandidate* out=nullptr){
    auto catalog=parse(value);assert(mi_catalog_status(catalog.get())==MI_CATALOG_READY);
    MiUpdateCandidate candidate{};return mi_updates_compare(catalog.get(),0,"old","primary-old",out?out:&candidate);
}
static json version(const char* id,const char* label,const char* date){
    return {{"version_id",id},{"version",label},{"release_date",date},{"resolved",true},{"files",json::array({{
        {"file_id",std::string("primary-")+id},{"filename","mod.zip"},{"download_url","https://example.invalid/mod.zip"},
        {"role","primary"},{"recommended",true},{"requires_primary",false}}})}};
}
static MiUpdatesInfo scan(const fs::path& root,const json& value,MiUpdateCandidate* candidate=nullptr){
    auto catalog=parse(value);auto* request=mi_updates_start(root.c_str(),catalog.get());assert(request);
    catalog.reset(); // worker must own an independent deep copy
    MiUpdatesInfo result{};
    for(unsigned i=0;i<2000;i++){
        assert(mi_updates_poll(request,&result));if(result.state!=MI_UPDATES_RUNNING)break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    assert(result.state!=MI_UPDATES_RUNNING);
    if(candidate)assert(mi_updates_get(request,"900001",candidate));
    mi_updates_close(request);return result;
}
static std::string key(const std::string& file){
    MiSha256 hash;auto text="900001\nold\n"+file;hash.update(text.data(),text.size());return hash.finish();
}
static void receipt(const fs::path& root,const std::string& file="primary-old",const std::string& destination="mod"){
    fs::create_directories(root/"mi/receipts");fs::create_directories(root/"mods"/destination);
    std::ofstream(root/"mods"/destination/"main.lua")<<"-- name: Fixture\n";
    json record={{"schema",1},{"mod_id","900001"},{"version_id","old"},{"file_id",file},{"version","1.0"},{"destination",destination},
        {"files",json::array({{{"path","main.lua"},{"size",17},{"sha256",std::string(64,'0')}}})}};
    std::ofstream(root/"mi/receipts"/(key(file)+".json"))<<record.dump();
}
int main(int argc,char** argv){
    assert(argc==3);std::ifstream stream(argv[1]);json base;stream>>base;
    base["mods"]=json::array({base["mods"][0]});base["mod_count"]=1;
    auto& mod=base["mods"][0];mod["version"]="v2.0";
    mod["versions"]=json::array({version("old","1.0","2025-01-01"),version("new","2.0","2025-02-01")});
    MiUpdateCandidate candidate{};assert(compare(base,&candidate)==MI_UPDATE_AVAILABLE);assert(std::string(candidate.file.versionId)=="new");
    auto v=base;std::reverse(v["mods"][0]["versions"].begin(),v["mods"][0]["versions"].end());assert(compare(v)==MI_UPDATE_AVAILABLE);
    v=base;v["mods"][0]["version"]="1.0";assert(compare(v)==MI_UPDATE_NONE);
    v=base;v["mods"][0]["version"]="unknown";assert(compare(v)==MI_UPDATE_UNKNOWN);
    v=base;v["mods"][0]["versions"][1]["release_date"]="2025-01-01";assert(compare(v)==MI_UPDATE_UNKNOWN);
    v=base;v["mods"][0]["versions"][1]["release_date"]="2025-02-30";assert(compare(v)==MI_UPDATE_UNKNOWN);
    v=base;v["mods"][0]["versions"][1]["release_date"]="";assert(compare(v)==MI_UPDATE_UNKNOWN);
    v=base;v["mods"][0]["versions"][1]["release_timestamp"]="invalid";assert(compare(v)==MI_UPDATE_UNKNOWN);
    v=base;v["mods"][0]["versions"][1]["release_timestamp"]="2025-01-02T00:00:00+02:00";assert(compare(v)==MI_UPDATE_UNKNOWN);
    v=base;v["mods"][0]["versions"][1]["release_timestamp"]="2025-01-03T00:00:00+02:00";assert(compare(v)==MI_UPDATE_AVAILABLE);
    v=base;v["mods"][0]["versions"][1]["release_timestamp"]="2025-01-01T20:00:00Z";assert(compare(v)==MI_UPDATE_UNKNOWN);
    v=base;v["mods"][0]["versions"][1]["resolved"]=false;v["mods"][0]["versions"][1]["files"]=json::array();assert(compare(v)==MI_UPDATE_UNKNOWN);
    v=base;v["mods"][0]["versions"][1]["files"][0]["filename"]="mod.rar";assert(compare(v)==MI_UPDATE_UNKNOWN);
    v=base;v["mods"][0]["versions"][1]["files"][0]["role"]="lite";assert(compare(v)==MI_UPDATE_UNKNOWN);
    v=base;v["mods"][0]["versions"].push_back(version("duplicate","2.0","2025-02-01"));assert(compare(v)==MI_UPDATE_UNKNOWN);
    v["mods"][0]["versions"][2]["release_date"]="2025-03-01";assert(compare(v)==MI_UPDATE_AVAILABLE);
    v=base;v["mods"][0]["version"]="preview";assert(compare(v)==MI_UPDATE_UNKNOWN);
    v=base;v["mods"][0]["versions"][1]["release_date"]="2024-01-01";assert(compare(v)==MI_UPDATE_NONE);
    v=base;v["mods"][0]["versions"][0]["files"][0]["role"]="";assert(compare(v)==MI_UPDATE_UNKNOWN);
    fs::path root=argv[2];fs::create_directories(root);
    auto result=scan(root,base);assert(result.state==MI_UPDATES_DONE&&result.available==0&&result.uncertain==0);
    assert(fs::is_empty(root)); // read-only even without mi/ or receipts
    receipt(root);result=scan(root,base,&candidate);assert(result.available==1&&result.checked==1&&std::string(candidate.destination)=="mod");
    fs::remove_all(root/"mods/mod");result=scan(root,base);assert(result.missing==1&&result.available==0);assert(fs::exists(root/"mi/receipts"/(key("primary-old")+".json")));
    receipt(root);receipt(root,"other-file","mod");result=scan(root,base);assert(result.available==0&&result.uncertain==2);
    fs::remove(root/"mi/receipts"/(key("other-file")+".json"));
    std::ofstream(root/"mi/receipts/bad.json")<<"{bad";result=scan(root,base);assert(result.available==1&&result.uncertain==1);fs::remove(root/"mi/receipts/bad.json");
    fs::remove(root/"mods/mod/main.lua");result=scan(root,base);assert(result.available==0&&result.uncertain==1);receipt(root);
    // Same mod in two destinations remains ambiguous, never silently chooses one.
    receipt(root,"other-file","mod-copy");result=scan(root,base);assert(result.available==0&&result.uncertain==2);
    fs::remove(root/"mi/receipts"/(key("other-file")+".json"));
    fs::create_directories(root/"mi/transactions");std::ofstream(root/"mi/transactions/pending.json")<<"{}";
    result=scan(root,base);assert(result.state==MI_UPDATES_FAILED&&result.error==MI_UPDATES_BUSY);fs::remove(root/"mi/transactions/pending.json");
    installing=true;result=scan(root,base);assert(result.state==MI_UPDATES_FAILED&&result.error==MI_UPDATES_BUSY);installing=false;
    auto cat=parse(base);for(int i=0;i<20;i++){auto* request=mi_updates_start(root.c_str(),cat.get());assert(request);mi_updates_close(request);}
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout<<"MI updates: dates/UTC/precision, ordering, variants, unresolved/latest, receipts, deletion, conflicts, pending/busy, cancellation and read-only scanning passed.\n";
}
