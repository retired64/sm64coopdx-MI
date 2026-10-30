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
};

/* Exact download fields: never display-truncate IDs or URLs. Unsupported or
 * dependent artifacts remain visible in selection but cannot be installed. */
struct MiCatalogFile {
    char modId[64], versionId[160], fileId[160];
    char version[96], filename[256], label[192];
    char url[2048], mirror[2048], sha256[65];
    uint64_t size;
    bool supported, recommended;
};
size_t mi_catalog_file_count(const struct MiCatalog* catalog, size_t index);
const struct MiCatalogFile* mi_catalog_file(const struct MiCatalog* catalog, size_t index, size_t fileIndex);

struct MiCatalog* mi_catalog_parse(const char* json, size_t length);
struct MiCatalog* mi_catalog_parse_language(const char* json, size_t length, const char* language);
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
