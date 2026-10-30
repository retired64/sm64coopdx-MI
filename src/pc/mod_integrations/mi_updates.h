#ifndef MI_UPDATES_H
#define MI_UPDATES_H
#include "mi_catalog.h"
#ifdef __cplusplus
extern "C" {
#endif
struct MiUpdates;
enum MiUpdateDecision { MI_UPDATE_NONE, MI_UPDATE_AVAILABLE, MI_UPDATE_UNKNOWN };
struct MiUpdateCandidate {
    struct MiCatalogFile file;
    char installedVersion[96], destination[256];
};
enum MiUpdatesState { MI_UPDATES_RUNNING, MI_UPDATES_DONE, MI_UPDATES_FAILED, MI_UPDATES_CANCELLED };
enum MiUpdatesError { MI_UPDATES_OK, MI_UPDATES_STORAGE, MI_UPDATES_BUSY, MI_UPDATES_INVALID };
struct MiUpdatesInfo {
    enum MiUpdatesState state;
    enum MiUpdatesError error;
    size_t checked, missing, uncertain, available;
};
/* Pure conservative comparison. IDs are opaque; no remote producer knowledge. */
enum MiUpdateDecision mi_updates_compare(const struct MiCatalog* catalog, size_t modIndex,
    const char* versionId, const char* fileId, struct MiUpdateCandidate* candidate);
/* Copies catalog/root; worker performs read-only IO. No directory/file creation.
 * Poll/get copy results. close cancels/releases without blocking or DJUI callbacks. */
struct MiUpdates* mi_updates_start(const char* userRoot,const struct MiCatalog* catalog);
bool mi_updates_poll(struct MiUpdates* request,struct MiUpdatesInfo* info);
bool mi_updates_get(struct MiUpdates* request,const char* modId,struct MiUpdateCandidate* candidate);
void mi_updates_close(struct MiUpdates* request);
#ifdef __cplusplus
}
#endif
#endif
