/* Pinned SQLite compiled into both native build paths; no Android private APIs. */
#define SQLITE_OMIT_LOAD_EXTENSION 1
#define SQLITE_DQS 0
#define SQLITE_THREADSAFE 1
#define SQLITE_DEFAULT_MEMSTATUS 0
#define SQLITE_MAX_MMAP_SIZE 0
#include "pc/utils/sqlite/sqlite3.c"
