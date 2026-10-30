#include "pc/mod_integrations/mi_catalog_request.h"
#include <chrono>
#include <thread>
#include <cstring>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 4) return 2;
    auto* request = argc == 5 && std::strcmp(argv[4], "fresh") == 0 ?
        mi_catalog_request_start(argv[1], argv[3]) : mi_catalog_request_start_url(argv[1], argv[3], argv[2]);
    if (!request) return 3;
    if (argc == 5 && std::strcmp(argv[4], "cancel") == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto begin = std::chrono::steady_clock::now();
        mi_catalog_request_close(request);
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count();
        if (millis > 100) return 4;
        std::this_thread::sleep_for(std::chrono::milliseconds(1200));
        std::cout << "{\"cancelled\":true}\n";
        return 0;
    }
    uint64_t revision = 0;
    MiCatalog* catalog = nullptr;
    MiCatalogFetchInfo info = {};
    bool sawCache = false;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);
    while (std::chrono::steady_clock::now() < deadline) {
        MiCatalog* incoming = nullptr;
        if (mi_catalog_request_poll(request, revision, &incoming, &info)) {
            revision = info.revision;
            if (incoming) { mi_catalog_free(catalog); catalog = incoming; }
            if (info.fromCache && catalog) sawCache = true;
            if (!info.running) {
                const auto* first = mi_catalog_entry(catalog, 0);
                std::cout << "{\"count\":" << mi_catalog_count(catalog)
                          << ",\"error\":" << static_cast<int>(info.error)
                          << ",\"http\":" << info.httpStatus
                          << ",\"cached\":" << (info.fromCache ? "true" : "false")
                          << ",\"saved\":" << (info.cacheSaved ? "true" : "false")
                          << ",\"saw_cache\":" << (sawCache ? "true" : "false")
                          << ",\"title_is_sample\":" << (first && std::strcmp(first->title, "MI Sample Lua") == 0 ? "true" : "false")
                          << ",\"first_id\":\"" << (first ? first->id : "") << "\"}\n";
                mi_catalog_request_close(request);
                mi_catalog_free(catalog);
                return 0;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    mi_catalog_request_close(request);
    mi_catalog_free(catalog);
    return 5;
}
