#include "mi_catalog_request.h"
#include "mi_network.h"
#include <curl/curl.h>
#include <zlib.h>
#include <pthread.h>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <new>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <ctime>
#include <dirent.h>

namespace {
constexpr size_t MAX_BYTES = 16 * 1024 * 1024;
constexpr const char* CATALOG_URL = "https://raw.githubusercontent.com/retired64/sm64cdpy.releases/r2-phase6-flutter-fallback/assets/db/database_sm64coopdx.json";
std::mutex commitMutex;
std::atomic<uint64_t> newestGeneration{0};
using CatalogPtr = std::unique_ptr<MiCatalog, decltype(&mi_catalog_free)>;
}

struct MiCatalogRequest {
    std::atomic<int> owners{1};
    std::atomic<bool> cancelled{false};
    std::mutex mutex;
    MiCatalog* pending = nullptr;
    MiCatalogFetchInfo info = {1, true, false, false, 0, 0, MI_FETCH_OK};
    std::string directory, language, url;
    uint64_t generation;
    bool forceRefresh = true;
    bool bundledOnly = false;
    ~MiCatalogRequest() { mi_catalog_free(pending); }
};

namespace {
void release(MiCatalogRequest* request) {
    if (request->owners.fetch_sub(1) == 1) delete request;
}
void publish(MiCatalogRequest* request, MiCatalog* catalog, bool running, bool cached,
             bool saved, int64_t savedAt, MiCatalogFetchError error, long http = 0) {
    std::lock_guard<std::mutex> lock(request->mutex);
    if (request->cancelled) { mi_catalog_free(catalog); return; }
    if (catalog) {
        mi_catalog_free(request->pending);
        request->pending = catalog;
    }
    request->info.running = running;
    request->info.fromCache = cached;
    request->info.cacheSaved = saved;
    request->info.savedAt = savedAt;
    request->info.error = error;
    request->info.httpStatus = http;
    ++request->info.revision;
}

uint32_t checksum(const std::string& bytes) {
    return static_cast<uint32_t>(crc32(0, reinterpret_cast<const Bytef*>(bytes.data()), static_cast<uInt>(bytes.size())));
}

struct Cache {
    std::string body;
    int64_t savedAt = 0;
    CatalogPtr catalog{nullptr, mi_catalog_free};
};

Cache read_cache(const std::string& path, const std::string& language) {
    Cache cache;
    int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW);
    if (fd < 0) return cache;
    FILE* file = fdopen(fd, "rb");
    if (!file) { close(fd); return cache; }
    std::unique_ptr<FILE, decltype(&fclose)> stream(file, fclose);
    struct stat statbuf;
    if (fstat(fd, &statbuf) != 0 || !S_ISREG(statbuf.st_mode) ||
        statbuf.st_size <= 0 || statbuf.st_size > static_cast<off_t>(MAX_BYTES + 128)) return cache;
    char header[128];
    if (!fgets(header, sizeof(header), file)) return cache;
    std::string magic;
    unsigned long crc;
    size_t size;
    int64_t savedAt;
    std::istringstream fields(header);
    if (!(fields >> magic >> savedAt >> crc >> size) || magic != "MI-CATALOG-1" || savedAt <= 0 || size == 0 || size > MAX_BYTES) return cache;
    std::string extra;
    if (fields >> extra) return cache;
    cache.body.resize(size);
    if (fread(&cache.body[0], 1, size, file) != size || fgetc(file) != EOF || ferror(file) || checksum(cache.body) != crc) {
        cache.body.clear(); return cache;
    }
    cache.catalog.reset(mi_catalog_parse_language(cache.body.data(), cache.body.size(), language.c_str()));
    if (mi_catalog_status(cache.catalog.get()) != MI_CATALOG_READY) { cache.catalog.reset(); cache.body.clear(); return cache; }
    cache.savedAt = savedAt;
    return cache;
}

bool atomic_cache(const std::string& path, const std::string& bytes, int64_t savedAt, uint64_t generation) {
    (void)generation;
    std::string temp = path + ".tmp-XXXXXX";
    int fd = mkstemp(&temp[0]);
    if (fd < 0) return false;
    FILE* file = fdopen(fd, "wb");
    if (!file) { close(fd); unlink(temp.c_str()); return false; }
    bool ok = fprintf(file, "MI-CATALOG-1 %lld %lu %zu\n", static_cast<long long>(savedAt),
                      static_cast<unsigned long>(checksum(bytes)), bytes.size()) > 0;
    ok = ok && fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    ok = ok && fflush(file) == 0;
    ok = ok && fsync(fd) == 0;
    if (fclose(file) != 0) ok = false;
    if (ok && rename(temp.c_str(), path.c_str()) != 0) ok = false;
    if (!ok) unlink(temp.c_str());
    return ok;
}

bool save_cache(MiCatalogRequest* request, const std::string& bytes, int64_t now) {
    std::lock_guard<std::mutex> lock(commitMutex);
    if (request->cancelled || request->generation != newestGeneration.load()) return false;
    if (mkdir(request->directory.c_str(), 0700) != 0 && errno != EEXIST) return false;
    struct stat directory;
    if (lstat(request->directory.c_str(), &directory) != 0 || !S_ISDIR(directory.st_mode)) return false;
    const std::string primary = request->directory + "/catalog-cdn-v2.cache";
    Cache old = read_cache(primary, request->language);
    if (old.catalog && !atomic_cache(request->directory + "/catalog-cdn-v2.last-good", old.body,
                                    old.savedAt, request->generation)) return false;
    if (request->cancelled || request->generation != newestGeneration.load()) return false;
    return atomic_cache(primary, bytes, now, request->generation);
}

struct Download { MiCatalogRequest* request; std::string body; bool limit = false; };
size_t receive(char* data, size_t size, size_t count, void* context) {
    auto* download = static_cast<Download*>(context);
    if (download->request->cancelled || (size != 0 && count > MAX_BYTES / size)) return 0;
    const size_t bytes = size * count;
    if (bytes > MAX_BYTES - download->body.size()) { download->limit = true; return 0; }
    try { download->body.append(data, bytes); }
    catch (...) { return 0; }
    return bytes;
}
int progress(void* context, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    return static_cast<MiCatalogRequest*>(context)->cancelled ? 1 : 0;
}


MiCatalogFetchError fetch(MiCatalogRequest* request, std::string& body, long& http) {
    if (mi_curl_init() != CURLE_OK) return MI_FETCH_NETWORK;
    CURL* handle = curl_easy_init();
    if (!handle) return MI_FETCH_NETWORK;
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> handleOwner(handle, curl_easy_cleanup);
    Download download{request, {}, false};
    curl_easy_setopt(handle, CURLOPT_URL, request->url.c_str());
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "sm64coopdx-MI/0.1 catalog");
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, receive);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &download);
    curl_easy_setopt(handle, CURLOPT_XFERINFOFUNCTION, progress);
    curl_easy_setopt(handle, CURLOPT_XFERINFODATA, request);
    curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 40L);
    curl_easy_setopt(handle, CURLOPT_LOW_SPEED_LIMIT, 128L);
    curl_easy_setopt(handle, CURLOPT_LOW_SPEED_TIME, 15L);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 3L);
    curl_easy_setopt(handle, CURLOPT_MAXFILESIZE_LARGE, static_cast<curl_off_t>(MAX_BYTES));
    curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L);
    /* Legacy protocol options are intentional: Android ships libcurl 7.79. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    const long protocols = request->url.compare(0, 17, "http://127.0.0.1:") == 0 ? CURLPROTO_HTTP : CURLPROTO_HTTPS;
    curl_easy_setopt(handle, CURLOPT_PROTOCOLS, protocols);
    curl_easy_setopt(handle, CURLOPT_REDIR_PROTOCOLS, protocols);
#pragma GCC diagnostic pop
#ifdef __ANDROID__
    std::string certificates = mi_system_certificates();
    if (certificates.empty()) return MI_FETCH_TLS;
    struct curl_blob roots = { &certificates[0], certificates.size(), CURL_BLOB_NOCOPY };
    curl_easy_setopt(handle, CURLOPT_CAINFO, static_cast<const char*>(nullptr));
    curl_easy_setopt(handle, CURLOPT_CAPATH, static_cast<const char*>(nullptr));
    if (curl_easy_setopt(handle, CURLOPT_CAINFO_BLOB, &roots) != CURLE_OK) {
        return MI_FETCH_TLS;
    }
#endif
    CURLcode result = curl_easy_perform(handle);
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &http);
    curl_off_t expected = -1, received = -1;
    curl_easy_getinfo(handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &expected);
    curl_easy_getinfo(handle, CURLINFO_SIZE_DOWNLOAD_T, &received);
    if (request->cancelled) return MI_FETCH_CANCELLED;
    if (result == CURLE_PEER_FAILED_VERIFICATION || result == CURLE_SSL_CACERT_BADFILE || result == CURLE_SSL_CONNECT_ERROR) return MI_FETCH_TLS;
    if (http && (http < 200 || http >= 300)) return MI_FETCH_HTTP;
    if (download.limit || result == CURLE_FILESIZE_EXCEEDED) return MI_FETCH_INVALID;
    if (result != CURLE_OK || (expected >= 0 && received != expected)) return MI_FETCH_NETWORK;
    body = std::move(download.body);
    return MI_FETCH_OK;
}

void* work(void* context) {
    auto* request = static_cast<MiCatalogRequest*>(context);
    bool cached = false, savedCopy = false;
    int64_t savedAt = 0;
    try {
        // Product mode: the APK database is the sole catalog authority. No cache IO or fetch.
        if (request->bundledOnly) {
            CatalogPtr catalog(mi_catalog_bundled(request->language.c_str()), mi_catalog_free);
            bool ready = mi_catalog_status(catalog.get()) == MI_CATALOG_READY;
            publish(request, ready ? catalog.release() : nullptr, false, true, false, 0,
                ready ? MI_FETCH_OK : MI_FETCH_INVALID);
            release(request);
            return nullptr;
        }
        Cache cache = read_cache(request->directory + "/catalog-cdn-v2.cache", request->language);
        if (!cache.catalog) cache = read_cache(request->directory + "/catalog-cdn-v2.last-good", request->language);
        size_t previousCount = mi_catalog_count(cache.catalog.get());
        if (cache.catalog && !request->cancelled) {
            cached = true; savedCopy = true;
            savedAt = cache.savedAt;
            publish(request, cache.catalog.release(), true, true, true, savedAt, MI_FETCH_OK);
        }
        std::string().swap(cache.body);
        if (!cached && request->url == CATALOG_URL && !request->cancelled) {
            CatalogPtr bundled(mi_catalog_bundled(request->language.c_str()), mi_catalog_free);
            if (mi_catalog_status(bundled.get()) == MI_CATALOG_READY) {
                previousCount = mi_catalog_count(bundled.get()); cached = true;
                publish(request, bundled.release(), request->forceRefresh, true, false, 0, MI_FETCH_OK);
            }
        }
        if (!request->forceRefresh) {
            publish(request, nullptr, false, cached, savedCopy, savedAt, cached ? MI_FETCH_OK : MI_FETCH_INVALID);
        } else if (!request->cancelled) {
            std::string body;
            long http = 0;
            MiCatalogFetchError error = fetch(request, body, http);
            CatalogPtr catalog(nullptr, mi_catalog_free);
            if (error == MI_FETCH_OK) {
                catalog.reset(mi_catalog_parse_language(body.data(), body.size(), request->language.c_str()));
                if (mi_catalog_status(catalog.get()) == MI_CATALOG_UNSUPPORTED) error = MI_FETCH_UNSUPPORTED;
                else if (mi_catalog_status(catalog.get()) != MI_CATALOG_READY ||
                    (previousCount > 0 && mi_catalog_count(catalog.get()) * 4 < previousCount * 3)) error = MI_FETCH_INVALID;
            }
            if (error == MI_FETCH_OK && !request->cancelled) {
                const int64_t now = static_cast<int64_t>(time(nullptr));
                bool saved = save_cache(request, body, now);
                if (!request->cancelled)
                    publish(request, catalog.release(), false, false, saved, now, saved ? MI_FETCH_OK : MI_FETCH_STORAGE, http);
            } else {
                publish(request, nullptr, false, cached, savedCopy, savedAt, error, http);
            }
        }
    } catch (...) {
        publish(request, nullptr, false, cached, savedCopy, savedAt, MI_FETCH_NETWORK);
    }
    release(request);
    return nullptr;
}
}

static struct MiCatalogRequest* start_request(const char* directory, const char* language, const char* url, bool forceRefresh, bool bundledOnly = false) {
    if (!directory || !url) return nullptr;
    MiCatalogRequest* request = new (std::nothrow) MiCatalogRequest;
    if (!request) return nullptr;
    try {
        request->forceRefresh = forceRefresh;
        request->bundledOnly = bundledOnly;
        request->directory = directory;
        request->language = language ? language : "en";
        request->url = url;
        request->generation = newestGeneration.fetch_add(1) + 1;
    } catch (...) { delete request; return nullptr; }
    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes) != 0) { request->info.running = false; request->info.error = MI_FETCH_NETWORK; return request; }
    int result = pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
    pthread_t thread;
    if (result == 0) {
        request->owners.fetch_add(1);
        result = pthread_create(&thread, &attributes, work, request);
        if (result != 0) release(request);
    }
    pthread_attr_destroy(&attributes);
    if (result != 0) { request->info.running = false; request->info.error = MI_FETCH_NETWORK; }
    return request;
}
extern "C" struct MiCatalogRequest* mi_catalog_request_start(const char* directory, const char* language) {
    return start_request(directory, language, CATALOG_URL, false, true);
}
extern "C" bool mi_catalog_request_poll(struct MiCatalogRequest* request, uint64_t lastRevision,
                                         struct MiCatalog** snapshot, struct MiCatalogFetchInfo* info) {
    if (!request || !snapshot || !info) return false;
    std::lock_guard<std::mutex> lock(request->mutex);
    if (request->info.revision == lastRevision) return false;
    *info = request->info;
    *snapshot = request->pending;
    request->pending = nullptr;
    return true;
}
extern "C" void mi_catalog_request_close(struct MiCatalogRequest* request) {
    if (!request) return;
    request->cancelled = true;
    release(request);
}

extern "C" struct MiCatalogRequest* mi_catalog_request_refresh(const char* directory, const char* language) {
    return start_request(directory, language, CATALOG_URL, false, true);
}
extern "C" struct MiCatalogRequest* mi_catalog_request_start_url(const char* directory, const char* language, const char* url) {
    return start_request(directory, language, url, true);
}
