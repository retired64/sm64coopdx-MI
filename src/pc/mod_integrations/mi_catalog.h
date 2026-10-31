#ifndef MI_CATALOG_H
#define MI_CATALOG_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Immutable snapshot owned by one screen. No game state, disk or network I/O. */
struct MiCatalog;
enum MiCatalogStatus { MI_CATALOG_READY, MI_CATALOG_INVALID, MI_CATALOG_UNSUPPORTED };
struct MiCatalogEntry {
    char id[64];
    char title[192];
    char author[128];
    char version[64];
    char category[96];
    char description[2048];
    char imageUrl[2048];
    bool versionExact;
};

/* Exact download fields: never display-truncate IDs or URLs. Unsupported or
 * dependent artifacts remain visible in selection but cannot be installed. */
struct MiCatalogFile {
    char modId[64], versionId[160], fileId[160];
    char version[96], filename[256], label[192];
    char role[24];
    char url[2048], mirror[2048], sha256[65]; /* url: CDN only; mirror reserved/empty. */
    uint64_t size;
    bool supported, recommended;
};
/* Optional public metadata; no assumption about producer or array ordering. */
struct MiCatalogVersion {
    char id[160], label[96], date[48], timestamp[64], providerDate[48];
    bool resolved, exact;
};
size_t mi_catalog_version_count(const struct MiCatalog* catalog, size_t index);
const struct MiCatalogVersion* mi_catalog_version(const struct MiCatalog* catalog, size_t index, size_t versionIndex);
/* Deep copy for independent read-only workers. Caller owns the result. */
struct MiCatalog* mi_catalog_clone(const struct MiCatalog* catalog);
size_t mi_catalog_file_count(const struct MiCatalog* catalog, size_t index);
const struct MiCatalogFile* mi_catalog_file(const struct MiCatalog* catalog, size_t index, size_t fileIndex);

struct MiCatalog* mi_catalog_parse(const char* json, size_t length);
struct MiCatalog* mi_catalog_parse_language(const char* json, size_t length, const char* language);
/* Read-only SQLite snapshot; never executes mod code or writes the input. */
struct MiCatalog* mi_catalog_parse_database(const unsigned char* bytes, size_t length, const char* language);
/* Validated public catalog included in the binary; caller owns snapshot. */
struct MiCatalog* mi_catalog_bundled(const char* language);
struct MiCatalog* mi_catalog_sample(void);
void mi_catalog_free(struct MiCatalog* catalog);
enum MiCatalogStatus mi_catalog_status(const struct MiCatalog* catalog);
size_t mi_catalog_count(const struct MiCatalog* catalog);
const struct MiCatalogEntry* mi_catalog_entry(const struct MiCatalog* catalog, size_t index);
bool mi_catalog_matches(const struct MiCatalogEntry* entry, const char* query);
size_t mi_catalog_match_count(const struct MiCatalog* catalog, const char* query);
/* SIZE_MAX when the requested filtered result does not exist. */
size_t mi_catalog_match_index(const struct MiCatalog* catalog, const char* query, size_t offset);

#ifdef __cplusplus
}
#endif
#endif
