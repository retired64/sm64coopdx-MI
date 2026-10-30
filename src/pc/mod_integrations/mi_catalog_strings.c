#include <string.h>
#include "pc/configfile.h"
#include "pc/djui/djui_language.h"
#include "mi_catalog_strings.h"

struct MiCatalogString { const char* key; const char* english; const char* spanish; const char* portuguese; };
static const struct MiCatalogString sStrings[] = {
#include "mi_catalog_strings.inc"
};

char* mi_catalog_text(const char* key) {
    char* external = djui_language_get("MI_CATALOG", key);
    if (external && strcmp(external, key) != 0) return external;
    for (size_t i = 0; i < sizeof(sStrings) / sizeof(sStrings[0]); ++i) {
        if (strcmp(sStrings[i].key, key) != 0) continue;
        if (strcmp(configLanguage, "Spanish") == 0) return (char*)sStrings[i].spanish;
        if (strcmp(configLanguage, "Portuguese") == 0) return (char*)sStrings[i].portuguese;
        return (char*)sStrings[i].english;
    }
    return (char*)key;
}
