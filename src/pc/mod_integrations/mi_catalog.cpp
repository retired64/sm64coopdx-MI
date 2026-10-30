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

using nlohmann::json;

struct MiCatalog {
    MiCatalogStatus status = MI_CATALOG_INVALID;
    std::vector<MiCatalogEntry> entries;
};

namespace {
constexpr size_t MAX_BYTES = 8 * 1024 * 1024;
constexpr size_t MAX_MODS = 5000;
const char SAMPLE[] =
#include "mi_catalog_sample.inc"
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

extern "C" struct MiCatalog* mi_catalog_parse(const char* text, size_t length) {
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
        for (const auto& mod : *mods) {
            if (!mod.is_object() || !mod.contains("id")) return catalog;
            const std::string modId = identity(mod["id"]);
            std::string title = field(mod, "name");
            if (title.empty()) title = field(mod, "title");
            if (!valid_id(modId) || !ids.insert(modId).second || title.empty()) return catalog;
            MiCatalogEntry entry = {};
            std::memcpy(entry.id, modId.c_str(), modId.size() + 1);
            display(entry.title, title);
            if (entry.title[0] == '\0') return catalog;
            display(entry.author, field(mod, "author"));
            display(entry.version, field(mod, "version"));
            display(entry.category, field(mod, "category"));
            display(entry.description, field(mod, "description"));
            entries.push_back(entry);
        }
        catalog->entries = std::move(entries);
        catalog->status = MI_CATALOG_READY;
    } catch (const std::exception&) {
        /* No C++ exception crosses the C/game boundary; no partial catalog. */
        catalog->entries.clear();
    }
    return catalog;
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
