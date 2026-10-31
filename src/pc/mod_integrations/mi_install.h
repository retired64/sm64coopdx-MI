#ifndef MI_INSTALL_H
#define MI_INSTALL_H
#include "mi_catalog.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct MiInstall;
enum MiInstallState { MI_INSTALL_PENDING, MI_INSTALL_DOWNLOADING, MI_INSTALL_EXTRACTING,
    MI_INSTALL_VERIFYING, MI_INSTALL_COMMITTING, MI_INSTALL_COMPLETE, MI_INSTALL_FAILED,
    MI_INSTALL_CANCELLING, MI_INSTALL_CANCELLED, MI_INSTALL_ROLLING_BACK };
enum MiInstallError { MI_INSTALL_OK, MI_INSTALL_NETWORK, MI_INSTALL_HTTP, MI_INSTALL_TLS,
    MI_INSTALL_STORAGE, MI_INSTALL_FORMAT, MI_INSTALL_UNSAFE, MI_INSTALL_LIMIT,
    MI_INSTALL_EXISTS, MI_INSTALL_BUSY, MI_INSTALL_INTEGRITY, MI_INSTALL_RECOVERY,
    MI_INSTALL_CHANGED, MI_INSTALL_UPDATE_RESTORED };
struct MiInstallInfo {
    enum MiInstallState state;
    enum MiInstallError error;
    float progress; /* -1 = unknown; never use download 100% as success */
    uint64_t bytes, total;
    long httpStatus;
    char destination[256];
};
/* Copies identity/source. One process-wide installation, no worker DJUI access.
 * userRoot is a copied fs_writepath; tests must use isolated temporary roots. */
/* Changes at accepted install start/end; invalidates read-only update results. */
uint64_t mi_install_revision(void);
bool mi_install_is_busy(void);
struct MiInstall* mi_install_start(const char* userRoot, const struct MiCatalogFile* source);
/* Explicit replacement of an MI-owned installation, preserving its destination.
 * Both records and local contents are revalidated under the installer lock.
 * All arguments are copied. Close cancels preparation; commit/rollback cannot cancel. */
struct MiInstallPrevious { char versionId[160], fileId[160], destination[256]; };
struct MiInstall* mi_install_update_start(const char* userRoot, const struct MiCatalogFile* source,
    const struct MiInstallPrevious* previous);
/* Reconcile pending journals asynchronously without a download. No-op recovery
 * does not create folders. Call on catalog entry; same ownership/poll/close API. */
struct MiInstall* mi_install_recover_start(const char* userRoot);
bool mi_install_poll(struct MiInstall* operation, struct MiInstallInfo* info);
void mi_install_cancel(struct MiInstall* operation);
void mi_install_close(struct MiInstall* operation);
#ifdef __cplusplus
}
#endif
#endif
