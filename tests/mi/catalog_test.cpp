#include "pc/mod_integrations/mi_catalog.h"
#include "pc/utils/json.hpp"
#include <cassert>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using nlohmann::json;

static MiCatalog* parse(const json& value) {
    const auto raw = value.dump();
    return mi_catalog_parse(raw.data(), raw.size());
}
static void invalid(const json& value) {
    auto catalog = parse(value);
    assert(mi_catalog_status(catalog) != MI_CATALOG_READY);
    assert(mi_catalog_count(catalog) == 0);
    mi_catalog_free(catalog);
}

int main(int argc, char** argv) {
    assert(argc == 2);
    std::ifstream input(argv[1]);
    json sample; input >> sample;
    auto catalog = mi_catalog_sample();
    assert(mi_catalog_status(catalog) == MI_CATALOG_READY);
    assert(mi_catalog_count(catalog) == 8);
    assert(std::strcmp(mi_catalog_entry(catalog, 0)->id, "900001") == 0);
    assert(mi_catalog_match_count(catalog, "UTILITY") == 2);
    assert(mi_catalog_match_count(catalog, "fixture team") == 8);
    assert(mi_catalog_match_count(catalog, "standalone") == 1);
    assert(mi_catalog_match_count(catalog, "not a catalog result") == 0);
    assert(mi_catalog_match_index(catalog, "GAMEPLAY", 0) == 5);
    assert(mi_catalog_match_index(catalog, "utility", 1) == 7);
    assert(mi_catalog_match_index(catalog, "utility", 2) == SIZE_MAX);
    assert(mi_catalog_entry(catalog, SIZE_MAX) == nullptr);
    mi_catalog_free(catalog);

    auto installable = sample;
    auto& source = installable["mods"][0]["versions"][0]["files"][0];
    source["filename"] = "test.lua"; source["download_url"] = "https://example.invalid/test.lua";
    source["requires_primary"] = false;
    catalog = parse(installable);
    assert(mi_catalog_file_count(catalog, 0) > 0);
    assert(mi_catalog_file(catalog, 0, 0)->supported);
    assert(std::strcmp(mi_catalog_file(catalog, 0, 0)->filename, "test.lua") == 0);
    assert(mi_catalog_file(catalog, SIZE_MAX, 0) == nullptr);
    assert(mi_catalog_file(catalog, 0, SIZE_MAX) == nullptr);
    mi_catalog_free(catalog);
    source["requires_primary"] = true;
    catalog = parse(installable); assert(!mi_catalog_file(catalog, 0, 0)->supported); mi_catalog_free(catalog);
    source["requires_primary"] = false; source["sha256"] = "invalid";
    catalog = parse(installable); assert(!mi_catalog_file(catalog, 0, 0)->supported); mi_catalog_free(catalog);
    source["sha256"] = nullptr; source["download_url"] = "https://example.invalid/" + std::string(2100, 'x');
    catalog = parse(installable); assert(!mi_catalog_file(catalog, 0, 0)->supported); mi_catalog_free(catalog);

    auto bad = sample;
    bad["schema_version"] = 3;
    catalog = parse(bad);
    assert(mi_catalog_status(catalog) == MI_CATALOG_UNSUPPORTED);
    mi_catalog_free(catalog);
    bad = sample; bad["catalog_status"] = "partial"; invalid(bad);
    bad = sample; bad["catalog_id"] = "bad"; invalid(bad);
    bad = sample; bad["mod_count"] = 1; invalid(bad);
    bad = sample; bad["mods"][1]["id"] = bad["mods"][0]["id"]; invalid(bad);
    bad = sample; bad["mods"][0]["id"] = std::string(64, 'x'); invalid(bad);
    bad = sample; bad["mods"][0]["name"] = ""; invalid(bad);
    bad = sample; bad["mods"][1] = nullptr; invalid(bad);
    bad = sample; bad["mods"][0]["versions"].push_back(bad["mods"][0]["versions"][0]); invalid(bad);
    bad = sample; bad["mods"][0]["versions"][0]["files"].push_back(bad["mods"][0]["versions"][0]["files"][0]); invalid(bad);
    bad = sample; bad["mods"][0]["versions"][0]["files"] = json::array(); invalid(bad);
    bad = sample; bad["mods"][0]["description_translations"] = "invalid"; invalid(bad);
    auto translated = sample;
    translated["mods"][0]["description_translations"] = {{"es", "Descripción traducida"}};
    auto rawTranslated = translated.dump();
    catalog = mi_catalog_parse_language(rawTranslated.data(), rawTranslated.size(), "es");
    assert(std::strcmp(mi_catalog_entry(catalog, 0)->description, "Descripción traducida") == 0);
    mi_catalog_free(catalog);

    catalog = mi_catalog_parse("{broken", 7);
    assert(mi_catalog_status(catalog) == MI_CATALOG_INVALID);
    mi_catalog_free(catalog);
    catalog = mi_catalog_parse(nullptr, 0);
    assert(mi_catalog_count(catalog) == 0);
    mi_catalog_free(catalog);

    auto minimal = sample;
    minimal["mods"] = json::array({{{"id", 12}, {"name", "Minimal"}, {"versions", json::array()}}});
    minimal["mod_count"] = 1;
    catalog = parse(minimal);
    assert(mi_catalog_status(catalog) == MI_CATALOG_READY);
    assert(std::strcmp(mi_catalog_entry(catalog, 0)->id, "12") == 0);
    assert(mi_catalog_entry(catalog, 0)->description[0] == '\0');
    mi_catalog_free(catalog);

    auto empty = sample; empty["mods"] = json::array(); empty["mod_count"] = 0;
    catalog = parse(empty);
    assert(mi_catalog_status(catalog) == MI_CATALOG_READY);
    assert(mi_catalog_match_count(catalog, "") == 0);
    mi_catalog_free(catalog);

    auto display = minimal;
    display["mods"][0]["name"] = "\\#ff0000\\Visible\\";
    display["mods"][0]["description"] = std::string(2046, 'a') + "ñ";
    catalog = parse(display);
    assert(std::strcmp(mi_catalog_entry(catalog, 0)->title, "Visible") == 0);
    assert(std::strlen(mi_catalog_entry(catalog, 0)->description) == 2046);
    mi_catalog_free(catalog);

    auto object = minimal;
    object["mods"] = {{"12", minimal["mods"][0]}};
    catalog = parse(object);
    assert(mi_catalog_count(catalog) == 1);
    mi_catalog_free(catalog);
    std::cout << "MI catalog: sample, schema, malformed input, identity, search, UTF-8 and empty states passed.\n";
}
