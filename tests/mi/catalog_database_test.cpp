#include "pc/mod_integrations/mi_catalog.cpp"
#include <cassert>
#include <fstream>
#include <iostream>
static std::string read(const char *p) {
  std::ifstream f(p, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(f), {});
}
static void equal(const MiCatalog *a, const MiCatalog *b) {
  assert(mi_catalog_status(a) == MI_CATALOG_READY &&
         mi_catalog_status(b) == MI_CATALOG_READY);
  assert(a->entries.size() == b->entries.size());
  for (size_t i = 0; i < a->entries.size(); ++i) {
    assert(!memcmp(&a->entries[i], &b->entries[i], sizeof(MiCatalogEntry)));
    assert(a->versions[i].size() == b->versions[i].size());
    assert(a->files[i].size() == b->files[i].size());
    for (size_t j = 0; j < a->versions[i].size(); ++j)
      assert(!memcmp(&a->versions[i][j], &b->versions[i][j],
                     sizeof(MiCatalogVersion)));
    for (size_t j = 0; j < a->files[i].size(); ++j)
      assert(!memcmp(&a->files[i][j], &b->files[i][j], sizeof(MiCatalogFile)));
  }
}
int main(int argc, char **argv) {
  assert(argc == 3);
  auto source = read(argv[1]), db = read(argv[2]);
  for (const char *lang : {"en", "es", "pt"}) {
    auto *json = mi_catalog_parse_language(source.data(), source.size(), lang);
    auto *sql = mi_catalog_bundled(lang);
    equal(json, sql);
    mi_catalog_free(json);
    mi_catalog_free(sql);
  }
  for (size_t size : {size_t(0), size_t(16), db.size() / 2}) {
    auto *c = mi_catalog_parse_database(
        reinterpret_cast<const unsigned char *>(db.data()), size, "es");
    assert(mi_catalog_status(c) != MI_CATALOG_READY);
    mi_catalog_free(c);
  }
  auto invalid = db;
  invalid[0] = '!';
  auto *c = mi_catalog_parse_database(
      reinterpret_cast<const unsigned char *>(invalid.data()), invalid.size(),
      "es");
  assert(mi_catalog_status(c) == MI_CATALOG_INVALID);
  mi_catalog_free(c);
  for (int mode = 0; mode < 6; ++mode) {
    sqlite3 *h = nullptr;
    assert(sqlite3_open(":memory:", &h) == SQLITE_OK);
    auto *memory = static_cast<unsigned char *>(sqlite3_malloc64(db.size()));
    assert(memory);
    memcpy(memory, db.data(), db.size());
    assert(sqlite3_deserialize(h, "main", memory, db.size(), db.size(),
                               SQLITE_DESERIALIZE_FREEONCLOSE) == SQLITE_OK);
    const char *changes[] = {
        "PRAGMA user_version=99",
        "UPDATE metadata SET value='0' WHERE key='mod_count'",
        "UPDATE versions SET mod=999999 WHERE mod=0",
        "UPDATE mods SET id='' WHERE ordinal=0",
        "UPDATE metadata SET value='1' WHERE key='schema_version'",
        "UPDATE metadata SET value='\"partial\"' WHERE key='catalog_status'"};
    const char *sql = changes[mode];
    assert(sqlite3_exec(h, sql, nullptr, nullptr, nullptr) == SQLITE_OK);
    sqlite3_int64 bytes = 0;
    auto *serialized = sqlite3_serialize(h, "main", &bytes, 0);
    assert(serialized);
    c = mi_catalog_parse_database(serialized, size_t(bytes), "es");
    assert(mi_catalog_status(c) != MI_CATALOG_READY);
    mi_catalog_free(c);
    sqlite3_free(serialized);
    sqlite3_close(h);
  }
  std::cout
      << "SQLite catalog: EN/ES/PT full DTO parity, truncation, corruption, "
         "schema/count/orphan rejection and read-only input passed.\n";
}
