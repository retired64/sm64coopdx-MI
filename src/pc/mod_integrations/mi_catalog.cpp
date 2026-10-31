#include "mi_catalog.h"
#include "pc/utils/json.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <new>
#include <set>
#include <string>
#include <stdexcept>
#include <vector>
#include <memory>
#include "pc/utils/sqlite/sqlite3.h"

using nlohmann::json;

struct MiCatalog {
    MiCatalogStatus status = MI_CATALOG_INVALID;
    std::vector<MiCatalogEntry> entries;
    std::vector<std::vector<MiCatalogFile>> files;
    std::vector<std::vector<MiCatalogVersion>> versions;
};

namespace {
constexpr size_t MAX_BYTES = 16 * 1024 * 1024;
constexpr size_t MAX_MODS = 5000;
const char SAMPLE[] =
#include "mi_catalog_sample.inc"
;

const char BUNDLED[] =
#include "mi_catalog_bundled.inc"
;

std::string field(const json& object, const char* key) {
    auto it = object.find(key);
    return it != object.end() && it->is_string() ? it->get<std::string>() : "";
}

std::string identity(const json& value) {
    if (value.is_string()) return value.get<std::string>();
    if (value.is_number_unsigned()) return std::to_string(value.get<uint64_t>());
    if (value.is_number_integer() && value.get<int64_t>() >= 0) return std::to_string(value.get<int64_t>());
    return "";
}

bool valid_id(const std::string& id) {
    return !id.empty() && id.size() < sizeof(MiCatalogEntry::id) &&
        std::all_of(id.begin(), id.end(), [](unsigned char c) { return c > 32 && c < 127; });
}

/* Display only: never let remote text inject DJUI color/alpha commands.
 * Preserve UTF-8 and never cut in the middle of a code point. */
template<size_t N> void display(char (&out)[N], const std::string& value) {
    std::string clean;
    for (size_t i = 0; i < value.size();) {
        unsigned char c = value[i];
        if (c == '\\') {
            if (i + 1 < value.size() && value[i + 1] == '#') {
                size_t end = value.find('\\', i + 2);
                if (end != std::string::npos) { i = end + 1; continue; }
            }
            ++i;
            continue;
        }
        if (c < 32 || c == 127) {
            if ((c == '\n' || c == '\t') && clean.size() + 1 < N) clean += ' ';
            ++i;
            continue;
        }
        size_t width = c < 128 ? 1 : (c < 224 ? 2 : (c < 240 ? 3 : 4));
        if (i + width > value.size() || clean.size() + width >= N) break;
        clean.append(value, i, width);
        i += width;
    }
    std::memcpy(out, clean.data(), clean.size());
    out[clean.size()] = '\0';
}

template<size_t N> bool exact(char (&out)[N], const std::string& value) {
    if(value.size()>=N||std::any_of(value.begin(),value.end(),[](unsigned char c){return c<32||c==127;}))return false;
    std::memcpy(out,value.c_str(),value.size()+1);return true;
}

unsigned char lower_ascii(unsigned char c) {
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

bool contains_folded(const char* text, const char* query) {
    if (!query || !*query) return true;
    for (const char* start = text; *start; ++start) {
        const char* a = start;
        const char* b = query;
        while (*a && *b && lower_ascii(*a) == lower_ascii(*b)) { ++a; ++b; }
        if (!*b) return true;
    }
    return false;
}
}

extern "C" struct MiCatalog* mi_catalog_parse_language(const char* text, size_t length, const char* language) {
    MiCatalog* catalog = new (std::nothrow) MiCatalog;
    if (!catalog) return nullptr;
    if (!text || length == 0 || length > MAX_BYTES) return catalog;
    try {
        auto root = json::parse(text, text + length, [](int depth, json::parse_event_t, json&) {
            if (depth > 32) throw std::length_error("catalog nesting");
            return true;
        }, false);
        if (root.is_discarded() || !root.is_object()) return catalog;
        const auto schema = root.find("schema_version");
        if (schema == root.end() || !schema->is_number_integer()) return catalog;
        if (*schema != 2) { catalog->status = MI_CATALOG_UNSUPPORTED; return catalog; }
        if (field(root, "catalog_status") != "complete") return catalog;
        const auto id = field(root, "catalog_id");
        if (id.size() != 71 || id.compare(0, 7, "sha256:") != 0 ||
            !std::all_of(id.begin() + 7, id.end(), [](char c) {
                return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
            })) return catalog;
        auto mods = root.find("mods");
        if (mods == root.end() || (!mods->is_array() && !mods->is_object()) || mods->size() > MAX_MODS) return catalog;
        auto count = root.find("mod_count");
        if (count == root.end() || !count->is_number_integer() || *count != mods->size()) return catalog;
        std::set<std::string> ids;
        std::vector<MiCatalogEntry> entries;
        entries.reserve(mods->size());
        std::vector<std::vector<MiCatalogFile>> sources;
        size_t fileTotal = 0, versionTotal = 0;
        std::vector<std::vector<MiCatalogVersion>> metadata;
        for (const auto& mod : *mods) {
            if (!mod.is_object() || !mod.contains("id")) return catalog;
            const std::string modId = identity(mod["id"]);
            std::string title = field(mod, "name");
            if (title.empty()) title = field(mod, "title");
            if (!valid_id(modId) || !ids.insert(modId).second || title.empty()) return catalog;
            const auto versions = mod.find("versions");
            if (versions == mod.end() || !versions->is_array()) return catalog;
            std::vector<MiCatalogFile> modFiles;
            std::vector<MiCatalogVersion> modVersions;
            std::set<std::string> versionIds;
            for (const auto& version : *versions) {
                if (!version.is_object()) return catalog;
                auto versionId = field(version, "version_id");
                if (versionId.empty() || !versionIds.insert(versionId).second) return catalog;
                auto files = version.find("files");
                auto resolved = version.find("resolved");
                if (files == version.end() || !files->is_array() || resolved == version.end() || !resolved->is_boolean()) return catalog;
                if (resolved->get<bool>() && files->empty()) return catalog;
                if(++versionTotal>20000)return catalog;
                MiCatalogVersion meta{};
                meta.exact=exact(meta.id,versionId)&&exact(meta.label,field(version,"version"))&&
                    exact(meta.date,field(version,"release_date"))&&exact(meta.timestamp,field(version,"release_timestamp"))&&
                    exact(meta.providerDate,field(version,"provider_release_date"));
                meta.resolved=resolved->get<bool>();modVersions.push_back(meta);
                std::set<std::string> fileIds;
                for (const auto& file : *files) {
                    if (!file.is_object()) return catalog;
                    auto fileId = field(file, "file_id");
                    auto url = field(file, "mirror_url");
                    const bool available = field(file, "mirror_status") == "available";
                    const bool cdn = url.compare(0, 30, "https://mods-cdn.sm64cdpy.org/") == 0;
                    if (fileId.empty() || !fileIds.insert(fileId).second || field(file, "filename").empty()) return catalog;
                    if (++fileTotal > 20000) return catalog;
                    MiCatalogFile source = {};
                    auto exact = [](auto& out, const std::string& value) {
                        if (value.size() >= sizeof(out) || value.find('\0') != std::string::npos) return false;
                        std::memcpy(out, value.c_str(), value.size() + 1); return true;
                    };
                    bool exactFields = exact(source.modId, modId) && exact(source.versionId, versionId) &&
                        exact(source.fileId, fileId) && exact(source.filename, field(file, "filename")) && exact(source.url, url);
                    display(source.version, field(version, "version"));
                    display(source.label, field(file, "display_name"));
                    exact(source.role, field(file, "role"));
                    auto hash = field(file, "sha256");
                    bool hashValid = hash.empty() || (hash.size() == 64 && std::all_of(hash.begin(), hash.end(),
                        [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }));
                    if (hashValid) exact(source.sha256, hash);
                    if (file.contains("mirror_size_bytes") && file["mirror_size_bytes"].is_number_unsigned()) source.size = file["mirror_size_bytes"].get<uint64_t>();
                    std::string filename = field(file, "filename");
                    bool type = filename.size() > 4 && (filename.substr(filename.size()-4) == ".lua" || filename.substr(filename.size()-4) == ".zip");
                    source.recommended = file.value("recommended", false);
                    source.supported = exactFields && hashValid && type && resolved->get<bool>() &&
                        !file.value("requires_primary", false) && available && cdn &&
                        !contains_folded(field(mod, "category").c_str(), "dynos");
                    modFiles.push_back(source);
                }
            }
            MiCatalogEntry entry = {};
            std::memcpy(entry.id, modId.c_str(), modId.size() + 1);
            display(entry.title, title);
            if (entry.title[0] == '\0') return catalog;
            std::string image = field(mod, "image_url");
            if (image.empty()) image = field(mod, "icon_url");
            if (image.compare(0, 8, "https://") == 0) exact(entry.imageUrl, image);
            display(entry.author, field(mod, "author"));
            display(entry.version, field(mod, "version"));
            entry.versionExact=field(mod,"version")==entry.version;
            display(entry.category, field(mod, "category"));
            std::string description = field(mod, "description");
            auto translations = mod.find("description_translations");
            if (translations != mod.end()) {
                if (!translations->is_object()) return catalog;
                for (const auto& translation : *translations) if (!translation.is_string()) return catalog;
                if (language && language[0]) {
                    auto translated = field(*translations, language);
                    if (!translated.empty()) description = translated;
                }
            }
            display(entry.description, description);
            entries.push_back(entry);
            sources.push_back(std::move(modFiles));
            metadata.push_back(std::move(modVersions));
        }
        catalog->entries = std::move(entries);
        catalog->files = std::move(sources);
        catalog->versions = std::move(metadata);
        catalog->status = MI_CATALOG_READY;
    } catch (const std::exception&) {
        /* No C++ exception crosses the C/game boundary; no partial catalog. */
        catalog->entries.clear();
    }
    return catalog;
}

extern "C" struct MiCatalog* mi_catalog_parse(const char* text, size_t length) {
    return mi_catalog_parse_language(text, length, "en");
}
#include "mi_catalog_database.inc"
extern "C" struct MiCatalog* mi_catalog_bundled(const char* language) {
    return mi_catalog_parse_database(reinterpret_cast<const unsigned char*>(BUNDLED), sizeof(BUNDLED) - 1, language);
}
extern "C" struct MiCatalog* mi_catalog_sample(void) { return mi_catalog_parse(SAMPLE, sizeof(SAMPLE) - 1); }
extern "C" void mi_catalog_free(struct MiCatalog* catalog) { delete catalog; }
extern "C" enum MiCatalogStatus mi_catalog_status(const struct MiCatalog* catalog) {
    return catalog ? catalog->status : MI_CATALOG_INVALID;
}
extern "C" size_t mi_catalog_count(const struct MiCatalog* catalog) {
    return catalog && catalog->status == MI_CATALOG_READY ? catalog->entries.size() : 0;
}
extern "C" const struct MiCatalogEntry* mi_catalog_entry(const struct MiCatalog* catalog, size_t index) {
    return index < mi_catalog_count(catalog) ? &catalog->entries[index] : nullptr;
}
extern "C" bool mi_catalog_matches(const struct MiCatalogEntry* entry, const char* query) {
    if (!entry) return false;
    return contains_folded(entry->title, query) || contains_folded(entry->author, query) ||
        contains_folded(entry->category, query) || contains_folded(entry->description, query);
}
extern "C" size_t mi_catalog_match_count(const struct MiCatalog* catalog, const char* query) {
    size_t count = 0;
    for (size_t i = 0; i < mi_catalog_count(catalog); ++i)
        if (mi_catalog_matches(mi_catalog_entry(catalog, i), query)) ++count;
    return count;
}
extern "C" size_t mi_catalog_match_index(const struct MiCatalog* catalog, const char* query, size_t offset) {
    for (size_t i = 0; i < mi_catalog_count(catalog); ++i)
        if (mi_catalog_matches(mi_catalog_entry(catalog, i), query) && offset-- == 0) return i;
    return SIZE_MAX;
}

extern "C" size_t mi_catalog_file_count(const struct MiCatalog* catalog, size_t index) {
    return index < mi_catalog_count(catalog) ? catalog->files[index].size() : 0;
}
extern "C" const struct MiCatalogFile* mi_catalog_file(const struct MiCatalog* catalog, size_t index, size_t fileIndex) {
    return fileIndex < mi_catalog_file_count(catalog, index) ? &catalog->files[index][fileIndex] : nullptr;
}

extern "C" size_t mi_catalog_version_count(const MiCatalog* catalog,size_t index) {
    return index<mi_catalog_count(catalog)?catalog->versions[index].size():0;
}
extern "C" const MiCatalogVersion* mi_catalog_version(const MiCatalog* catalog,size_t index,size_t versionIndex) {
    return versionIndex<mi_catalog_version_count(catalog,index)?&catalog->versions[index][versionIndex]:nullptr;
}
extern "C" MiCatalog* mi_catalog_clone(const MiCatalog* catalog) {
    try{return catalog?new MiCatalog(*catalog):nullptr;}catch(...){return nullptr;}
}
