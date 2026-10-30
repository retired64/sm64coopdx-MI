#include "pc/mod_integrations/mi_thumbnail.h"
#include "pc/mod_integrations/mi_sha256.h"
#include "pc/utils/json.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>
int main(int argc,char** argv){
    assert(argc>=3);auto* op=mi_thumbnail_start(argv[1],argv[2]);assert(op);
    if(argc>3){
        auto begin=std::chrono::steady_clock::now();mi_thumbnail_close(op);
        assert(std::chrono::steady_clock::now()-begin<std::chrono::milliseconds(50));
        for(int i=0;i<80;i++){auto* pending=mi_thumbnail_start(argv[1],argv[2]);assert(pending);mi_thumbnail_close(pending);}
        std::this_thread::sleep_for(std::chrono::milliseconds(1200));puts("{\"closed\":true}");return 0;
    }
    auto state=MI_THUMB_LOADING;
    for(int i=0;i<6000&&state==MI_THUMB_LOADING;i++){state=mi_thumbnail_state(op);std::this_thread::sleep_for(std::chrono::milliseconds(5));}
    std::vector<uint8_t> pixels(MI_THUMB_BYTES);bool got=mi_thumbnail_take(op,pixels.data(),pixels.size());
    if(got)assert(!mi_thumbnail_take(op,pixels.data(),pixels.size()));
    MiSha256 hash;hash.update(pixels.data(),pixels.size());
    nlohmann::json result={{"state",state},{"pixels",got},{"sha256",hash.finish()}};
    mi_thumbnail_close(op);puts(result.dump().c_str());return state==MI_THUMB_LOADING?3:0;
}
