// Local benchmark prototype only: no production DB reader or game integration.
#include "pc/mod_integrations/mi_catalog.cpp"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sqlite3.h>
#include <sys/resource.h>
using Clock = std::chrono::steady_clock;
static long peakRSS() {
  // VmHWM is reset by exec on Linux; ru_maxrss can retain the parent's fork
  // peak.
  std::ifstream status("/proc/self/status");
  std::string key, line;
  while (status >> key) {
    if (key == "VmHWM:") {
      long kib = 0;
      status >> kib;
      assert(kib > 0);
      return kib;
    }
    std::getline(status, line);
  }
  assert(false && "This benchmark requires Linux /proc/self/status");
  return 0;
}

static const char *text(sqlite3_stmt *q, int c) {
  auto p = sqlite3_column_text(q, c);
  return p ? reinterpret_cast<const char *>(p) : "";
}
static sqlite3_stmt *query(sqlite3 *db, const char *sql) {
  sqlite3_stmt *q = nullptr;
  assert(sqlite3_prepare_v2(db, sql, -1, &q, nullptr) == SQLITE_OK);
  return q;
}
static MiCatalog *load(sqlite3 *db) {
  auto *c = new MiCatalog;
  auto *q =
      query(db, "SELECT "
                "m.ordinal,m.id,m.name,m.author,m.category,m.description,m."
                "version,m.image_url,t.text FROM mods m LEFT JOIN translations "
                "t ON t.mod=m.ordinal AND t.locale='es' ORDER BY m.ordinal");
  while (sqlite3_step(q) == SQLITE_ROW) {
    MiCatalogEntry e{};
    assert(size_t(sqlite3_column_int(q, 0)) == c->entries.size());
    assert(exact(e.id, text(q, 1)));
    display(e.title, text(q, 2));
    display(e.author, text(q, 3));
    display(e.category, text(q, 4));
    display(e.description, *text(q, 8) ? text(q, 8) : text(q, 5));
    display(e.version, text(q, 6));
    e.versionExact = std::string(text(q, 6)) == e.version;
    if (std::string(text(q, 7)).compare(0, 8, "https://") == 0)
      exact(e.imageUrl, text(q, 7));
    c->entries.push_back(e);
    c->files.emplace_back();
    c->versions.emplace_back();
  }
  sqlite3_finalize(q);
  q = query(db, "SELECT mod,ordinal,id,label,date,stamp,provider_date,resolved "
                "FROM versions ORDER BY mod,ordinal");
  while (sqlite3_step(q) == SQLITE_ROW) {
    int mod = sqlite3_column_int(q, 0);
    MiCatalogVersion v{};
    v.exact = exact(v.id, text(q, 2)) && exact(v.label, text(q, 3)) &&
              exact(v.date, text(q, 4)) && exact(v.timestamp, text(q, 5)) &&
              exact(v.providerDate, text(q, 6));
    v.resolved = sqlite3_column_int(q, 7);
    c->versions.at(mod).push_back(v);
  }
  sqlite3_finalize(q);
  q = query(db,
            "SELECT "
            "mod,version,id,filename,label,role,recommended,requires_primary,"
            "url,status,size,sha256 FROM files ORDER BY mod,version,ordinal");
  while (sqlite3_step(q) == SQLITE_ROW) {
    int mod = sqlite3_column_int(q, 0), ver = sqlite3_column_int(q, 1);
    const auto &v = c->versions.at(mod).at(ver);
    MiCatalogFile f{};
    bool fields = exact(f.modId, c->entries.at(mod).id) &&
                  exact(f.versionId, v.id) && exact(f.fileId, text(q, 2)) &&
                  exact(f.filename, text(q, 3)) && exact(f.url, text(q, 8));
    display(f.version, v.label);
    display(f.label, text(q, 4));
    exact(f.role, text(q, 5));
    f.recommended = sqlite3_column_int(q, 6);
    f.size = sqlite3_column_int64(q, 10);
    std::string hash = text(q, 11);
    bool valid =
        hash.empty() ||
        (hash.size() == 64 && std::all_of(hash.begin(), hash.end(), [](char a) {
           return (a >= '0' && a <= '9') || (a >= 'a' && a <= 'f');
         }));
    if (valid)
      exact(f.sha256, hash);
    std::string name = f.filename;
    bool type = name.size() > 4 && (name.substr(name.size() - 4) == ".lua" ||
                                    name.substr(name.size() - 4) == ".zip");
    f.supported =
        fields && valid && type && v.resolved && !sqlite3_column_int(q, 7) &&
        std::string(text(q, 9)) == "available" &&
        std::string(f.url).compare(0, 30, "https://mods-cdn.sm64cdpy.org/") ==
            0 &&
        !contains_folded(c->entries.at(mod).category, "dynos");
    c->files.at(mod).push_back(f);
  }
  sqlite3_finalize(q);
  c->status = MI_CATALOG_READY;
  return c;
}
static sqlite3 *open(const char *p) {
  sqlite3 *db = nullptr;
  assert(sqlite3_open_v2(p, &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK);
  return db;
}
static MiCatalog *jsonload(const char *path) {
  std::ifstream f(path, std::ios::binary);
  std::string s((std::istreambuf_iterator<char>(f)), {});
  return mi_catalog_parse_language(s.data(), s.size(), "es");
}
static void equal(const MiCatalog *a, const MiCatalog *b) {
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
  assert(argc >= 4);
  std::string mode = argv[1];
  std::string jsonBytes;
  if (mode == "json-bundled") {
    std::ifstream input(argv[2], std::ios::binary);
    jsonBytes.assign(std::istreambuf_iterator<char>(input), {});
  }
  auto begin = Clock::now();
  size_t count = 0;
  double queryMs = 0;
  if (mode == "verify") {
    auto *a = jsonload(argv[2]);
    auto *db = open(argv[3]);
    auto *b = load(db);
    equal(a, b);
    mi_catalog_free(a);
    mi_catalog_free(b);
    sqlite3_close(db);
    puts("{\"equivalent\":true}");
    return 0;
  }
  if (mode == "json" || mode == "json-bundled" || mode == "sqlite-full" || mode == "sqlite-bundled") {
    MiCatalog *c = nullptr;
    if (mode == "json")
      c = jsonload(argv[2]);
    else if (mode == "json-bundled")
      c = mi_catalog_parse_language(jsonBytes.data(), jsonBytes.size(), "es");
    else if (mode == "sqlite-bundled") c = mi_catalog_bundled("es");
    else {
      auto *db = open(argv[3]);
      c = load(db);
      sqlite3_close(db);
    }
    count = mi_catalog_count(c);
    auto loaded = Clock::now();
    for (int i = 0; i < 100; ++i) {
      volatile size_t matches = mi_catalog_match_count(c, "mario");
      (void)matches;
      for (int j = 0; j < 4; ++j) {
        volatile auto index = mi_catalog_match_index(c, "mario", j);
        (void)index;
      }
    }
    queryMs = std::chrono::duration<double, std::milli>(Clock::now() - loaded)
                  .count() /
              100;
    auto elapsed =
        std::chrono::duration<double, std::milli>(loaded - begin).count();
    struct rusage r{};
    getrusage(RUSAGE_SELF, &r);
    std::cout << "{\"load_ms\":" << elapsed << ",\"query_ms\":" << queryMs
              << ",\"peak_kib\":" << peakRSS() << ",\"mods\":" << count
              << "}\n";
    mi_catalog_free(c);
    return 0;
  }
  auto *db = open(argv[3]);
  auto *q = query(db, "SELECT count(*) FROM mods");
  assert(sqlite3_step(q) == SQLITE_ROW);
  count = sqlite3_column_int(q, 0);
  sqlite3_finalize(q);
  q = query(
      db, "SELECT id,name,author,category FROM mods ORDER BY ordinal LIMIT 4");
  while (sqlite3_step(q) == SQLITE_ROW) {
  }
  sqlite3_finalize(q);
  auto loaded = Clock::now();
  for (int i = 0; i < 100; ++i) {
    q = query(db, "SELECT count(*) FROM mods m LEFT JOIN translations t ON "
                  "t.mod=m.ordinal AND t.locale='es' WHERE name LIKE '%mario%' "
                  "OR author LIKE '%mario%' OR category LIKE '%mario%' OR "
                  "COALESCE(t.text,description) LIKE '%mario%'");
    assert(sqlite3_step(q) == SQLITE_ROW);
    sqlite3_finalize(q);
    q = query(
        db,
        "SELECT id,name,author,category FROM mods m LEFT JOIN translations t "
        "ON t.mod=m.ordinal AND t.locale='es' WHERE name LIKE '%mario%' OR "
        "author LIKE '%mario%' OR category LIKE '%mario%' OR "
        "COALESCE(t.text,description) LIKE '%mario%' ORDER BY ordinal LIMIT 4");
    while (sqlite3_step(q) == SQLITE_ROW) {
    }
    sqlite3_finalize(q);
  }
  queryMs =
      std::chrono::duration<double, std::milli>(Clock::now() - loaded).count() /
      100;
  struct rusage r{};
  getrusage(RUSAGE_SELF, &r);
  std::cout << "{\"load_ms\":"
            << std::chrono::duration<double, std::milli>(loaded - begin).count()
            << ",\"query_ms\":" << queryMs << ",\"peak_kib\":" << peakRSS()
            << ",\"mods\":" << count << "}\n";
  sqlite3_close(db);
}
