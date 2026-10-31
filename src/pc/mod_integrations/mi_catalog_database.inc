/* Read-only native database projection. Included after the common JSON helpers.
 */
namespace {
void db_require(bool ok) {
  if (!ok)
    throw std::runtime_error("Invalid catalog database");
}
struct DbStatement {
  sqlite3_stmt *value = nullptr;
  DbStatement(sqlite3 *db, const char *sql) {
    db_require(sqlite3_prepare_v2(db, sql, -1, &value, nullptr) == SQLITE_OK);
  }
  ~DbStatement() { sqlite3_finalize(value); }
  bool row() {
    int code = sqlite3_step(value);
    db_require(code == SQLITE_ROW || code == SQLITE_DONE);
    return code == SQLITE_ROW;
  }
  std::string text(int column) {
    int type = sqlite3_column_type(value, column);
    db_require(type == SQLITE_TEXT || type == SQLITE_NULL);
    if (type == SQLITE_NULL)
      return {};
    int size = sqlite3_column_bytes(value, column);
    db_require(size >= 0 && size <= 1024 * 1024);
    auto *p = sqlite3_column_text(value, column);
    db_require(p || !size);
    return std::string(reinterpret_cast<const char *>(p), size_t(size));
  }
  int64_t integer(int column) {
    db_require(sqlite3_column_type(value, column) == SQLITE_INTEGER);
    return sqlite3_column_int64(value, column);
  }
  bool boolean(int column) {
    auto n = integer(column);
    db_require(n == 0 || n == 1);
    return n == 1;
  }
};
} // namespace
extern "C" MiCatalog *mi_catalog_parse_database(const unsigned char *bytes,
                                                size_t length,
                                                const char *language) {
  std::unique_ptr<MiCatalog> catalog(new (std::nothrow) MiCatalog);
  if (!catalog)
    return nullptr;
  try {
    db_require(bytes && length >= 512 && length <= MAX_BYTES &&
               !memcmp(bytes, "SQLite format 3\0", 16));
    sqlite3 *raw = nullptr;
    int opened = sqlite3_open_v2(":memory:", &raw,
                                 SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                                     SQLITE_OPEN_NOMUTEX,
                                 nullptr);
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
    db_require(opened == SQLITE_OK);
    sqlite3_limit(raw, SQLITE_LIMIT_LENGTH, 1024 * 1024);
    sqlite3_limit(raw, SQLITE_LIMIT_SQL_LENGTH, 8192);
    sqlite3_limit(raw, SQLITE_LIMIT_COLUMN, 64);
    sqlite3_limit(raw, SQLITE_LIMIT_ATTACHED, 0);
    sqlite3_db_config(raw, SQLITE_DBCONFIG_DEFENSIVE, 1, nullptr);
    sqlite3_db_config(raw, SQLITE_DBCONFIG_TRUSTED_SCHEMA, 0, nullptr);
    db_require(sqlite3_deserialize(raw, "main",
                                   const_cast<unsigned char *>(bytes),
                                   sqlite3_int64(length), sqlite3_int64(length),
                                   SQLITE_DESERIALIZE_READONLY) == SQLITE_OK);
    db_require(sqlite3_exec(raw,
                            "PRAGMA query_only=ON; PRAGMA cache_size=-1024",
                            nullptr, nullptr, nullptr) == SQLITE_OK);
    {
      DbStatement q(raw, "PRAGMA application_id");
      db_require(q.row() && q.integer(0) == 1296646978);
    }
    {
      DbStatement q(raw, "PRAGMA user_version");
      db_require(q.row());
      if (q.integer(0) != 1) {
        catalog->status = MI_CATALOG_UNSUPPORTED;
        return catalog.release();
      }
    }
    {
      DbStatement q(raw, "PRAGMA quick_check(1)");
      db_require(q.row() && q.text(0) == "ok" && !q.row());
    }
    {
    DbStatement q(raw, "SELECT value FROM metadata WHERE key='schema_version'");
    db_require(q.row() && json::parse(q.text(0)) == 2);
  }
  {
    DbStatement q(raw, "SELECT value FROM metadata WHERE key='catalog_status'");
    db_require(q.row() && json::parse(q.text(0)) == "complete");
  }
  size_t expected = 0;
    {
      DbStatement q(raw, "SELECT value FROM metadata WHERE key='mod_count'");
      db_require(q.row());
      auto n = json::parse(q.text(0));
      db_require(n.is_number_unsigned());
      expected = n.get<size_t>();
      db_require(expected > 0 && expected <= MAX_MODS);
    }
    {
      DbStatement q(raw, "SELECT value FROM metadata WHERE key='catalog_id'");
      db_require(q.row());
      auto id = json::parse(q.text(0)).get<std::string>();
      db_require(id.size() == 71 && id.compare(0, 7, "sha256:") == 0 &&
                 std::all_of(id.begin() + 7, id.end(), [](char a) {
                   return (a >= '0' && a <= '9') || (a >= 'a' && a <= 'f');
                 }));
    }
    std::set<std::string> modIds;
    {
      DbStatement q(
          raw, "SELECT "
               "m.ordinal,m.id,m.name,m.author,m.category,m.description,m."
               "version,m.image_url,t.text FROM mods m LEFT JOIN translations "
               "t ON t.mod=m.ordinal AND t.locale=?1 ORDER BY m.ordinal");
      db_require(sqlite3_bind_text(q.value, 1, language ? language : "en", -1,
                                   SQLITE_TRANSIENT) == SQLITE_OK);
      while (q.row()) {
        db_require(catalog->entries.size() < expected &&
                   q.integer(0) == int64_t(catalog->entries.size()));
        MiCatalogEntry e{};
        auto id = q.text(1);
        db_require(valid_id(id) && modIds.insert(id).second && exact(e.id, id));
        display(e.title, q.text(2));
        db_require(e.title[0]);
        display(e.author, q.text(3));
        display(e.category, q.text(4));
        auto translation = q.text(8);
        display(e.description, translation.empty() ? q.text(5) : translation);
        auto version = q.text(6);
        display(e.version, version);
        e.versionExact = version == e.version;
        auto image = q.text(7);
        if (image.compare(0, 8, "https://") == 0)
          exact(e.imageUrl, image);
        catalog->entries.push_back(e);
        catalog->files.emplace_back();
        catalog->versions.emplace_back();
      }
    }
    db_require(catalog->entries.size() == expected);
    size_t versionCount = 0, fileCount = 0;
    std::vector<std::set<std::string>> versionIds(expected);
    std::vector<std::vector<std::set<std::string>>> fileIds(expected);
    {
      DbStatement q(
          raw, "SELECT mod,ordinal,id,label,date,stamp,provider_date,resolved "
               "FROM versions ORDER BY mod,ordinal");
      while (q.row()) {
        auto m = q.integer(0);
        db_require(m >= 0 && size_t(m) < expected && ++versionCount <= 20000);
        db_require(q.integer(1) ==
                   int64_t(catalog->versions[size_t(m)].size()));
        auto id = q.text(2);
        db_require(!id.empty() && versionIds[size_t(m)].insert(id).second);
        MiCatalogVersion v{};
        v.exact = exact(v.id, id) && exact(v.label, q.text(3)) &&
                  exact(v.date, q.text(4)) && exact(v.timestamp, q.text(5)) &&
                  exact(v.providerDate, q.text(6));
        v.resolved = q.boolean(7);
        catalog->versions[size_t(m)].push_back(v);
        fileIds[size_t(m)].emplace_back();
      }
    }
    {
      DbStatement q(
          raw, "SELECT "
               "f.mod,f.version,f.ordinal,f.id,f.filename,f.label,f.role,f."
               "recommended,f.requires_primary,f.url,f.status,f.size,f.sha256,"
               "v.id,v.label FROM files f JOIN versions v ON v.mod=f.mod AND "
               "v.ordinal=f.version ORDER BY f.mod,f.version,f.ordinal");
      while (q.row()) {
        auto m = q.integer(0), v = q.integer(1);
        db_require(m >= 0 && size_t(m) < expected && v >= 0 &&
                   size_t(v) < catalog->versions[size_t(m)].size() &&
                   ++fileCount <= 20000);
        auto &ids = fileIds[size_t(m)][size_t(v)];
        db_require(q.integer(2) == int64_t(ids.size()));
        auto id = q.text(3), name = q.text(4);
        db_require(!id.empty() && !name.empty() && ids.insert(id).second);
        MiCatalogFile f{};
        bool fields = exact(f.modId, catalog->entries[size_t(m)].id) &&
                      exact(f.versionId, q.text(13)) && exact(f.fileId, id) &&
                      exact(f.filename, name) && exact(f.url, q.text(9));
        display(f.version, q.text(14));
        display(f.label, q.text(5));
        exact(f.role, q.text(6));
        f.recommended = q.boolean(7);
        bool dependent = q.boolean(8);
        auto size = q.integer(11);
        db_require(size >= 0);
        f.size = uint64_t(size);
        auto hash = q.text(12);
        bool hashValid =
            hash.empty() || (hash.size() == 64 &&
                             std::all_of(hash.begin(), hash.end(), [](char a) {
                               return (a >= '0' && a <= '9') ||
                                      (a >= 'a' && a <= 'f');
                             }));
        if (hashValid)
          exact(f.sha256, hash);
        bool type =
            name.size() > 4 && (name.substr(name.size() - 4) == ".lua" ||
                                name.substr(name.size() - 4) == ".zip");
        f.supported =
            fields && hashValid && type &&
            catalog->versions[size_t(m)][size_t(v)].resolved && !dependent &&
            q.text(10) == "available" &&
            std::string(f.url).compare(0, 30,
                                       "https://mods-cdn.sm64cdpy.org/") == 0 &&
            !contains_folded(catalog->entries[size_t(m)].category, "dynos");
        catalog->files[size_t(m)].push_back(f);
      }
    }
    // Detect orphan rows hidden by the JOIN and resolved versions with no
    // files.
    {
      DbStatement q(raw, "SELECT count(*) FROM files");
      db_require(q.row() && q.integer(0) == int64_t(fileCount));
    }
    for (size_t m = 0; m < expected; ++m)
      for (size_t v = 0; v < fileIds[m].size(); ++v)
        db_require(!catalog->versions[m][v].resolved || !fileIds[m][v].empty());
    catalog->status = MI_CATALOG_READY;
  } catch (...) {
    catalog->entries.clear();
    catalog->files.clear();
    catalog->versions.clear();
  }
  return catalog.release();
}
