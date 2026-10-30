#pragma once
#include <curl/curl.h>
#include <mutex>
#include <string>
#include <memory>
#include <dirent.h>
#include <cstdio>
#include <unistd.h>
inline CURLcode mi_curl_init() {
    static std::once_flag once; static CURLcode result = CURLE_FAILED_INIT;
    std::call_once(once, [] { result = curl_global_init(CURL_GLOBAL_DEFAULT); });
    return result;
}
#ifdef __ANDROID__
/* Android's hashed CA filenames are not reliably usable by this old OpenSSL
 * build. Feed the same read-only system roots as PEM in memory instead. */
inline std::string mi_system_certificates() {
    const char* directory = access("/apex/com.android.conscrypt/cacerts", R_OK) == 0 ?
        "/apex/com.android.conscrypt/cacerts" : "/system/etc/security/cacerts";
    DIR* handle = opendir(directory);
    if (!handle) return {};
    std::unique_ptr<DIR, decltype(&closedir)> owner(handle, closedir);
    std::string certificates;
    while (auto* entry = readdir(handle)) {
        if (entry->d_name[0] == '.') continue;
        std::string path = std::string(directory) + "/" + entry->d_name;
        FILE* file = fopen(path.c_str(), "rb");
        if (!file) continue;
        std::unique_ptr<FILE, decltype(&fclose)> stream(file, fclose);
        char buffer[4096];
        size_t read;
        size_t total = 0;
        while ((read = fread(buffer, 1, sizeof(buffer), file)) != 0) {
            total += read;
            if (total > 64 * 1024 || certificates.size() + read > 2 * 1024 * 1024) return {};
            certificates.append(buffer, read);
        }
        certificates += '\n';
    }
    return certificates;
}
#endif
