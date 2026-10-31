#ifndef MI_CATALOG_REQUEST_H
#define MI_CATALOG_REQUEST_H
#include "mi_catalog.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

struct MiCatalogRequest;
enum MiCatalogFetchError {
    MI_FETCH_OK, MI_FETCH_NETWORK, MI_FETCH_HTTP, MI_FETCH_TLS,
    MI_FETCH_INVALID, MI_FETCH_UNSUPPORTED, MI_FETCH_STORAGE, MI_FETCH_CANCELLED
};
struct MiCatalogFetchInfo {
    uint64_t revision;
    bool running;
    bool fromCache;
    bool cacheSaved;
    int64_t savedAt;
    long httpStatus;
    enum MiCatalogFetchError error;
};
/* cacheDirectory must be a dedicated MI directory under the game's write path.
 * Current product mode: start and refresh load only the bundled SQLite database,
 * ignoring remote caches and doing no catalog network IO. Explicit URL override
 * is reserved for transport regression tests, not wired to the product UI. */
struct MiCatalogRequest* mi_catalog_request_start(const char* cacheDirectory, const char* language);
struct MiCatalogRequest* mi_catalog_request_refresh(const char* cacheDirectory, const char* language);
struct MiCatalogRequest* mi_catalog_request_start_url(const char* cacheDirectory, const char* language, const char* url);
/* Transfers a new immutable snapshot, if any, to the caller; status is copied.
 * Worker threads never own or call a DJUI object. */
bool mi_catalog_request_poll(struct MiCatalogRequest* request, uint64_t lastRevision,
                             struct MiCatalog** snapshot, struct MiCatalogFetchInfo* info);
/* Cancels/releases UI ownership without waiting for DNS/network/file I/O. */
void mi_catalog_request_close(struct MiCatalogRequest* request);
#ifdef __cplusplus
}
#endif
#endif
