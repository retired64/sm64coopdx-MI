#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "pc/mod_integrations/mi_catalog_strings.h"

char configLanguage[64] = "English";
static int customTranslation;
char* djui_language_get(const char* section, const char* key) {
    assert(strcmp(section, "MI_CATALOG") == 0);
    if (customTranslation && strcmp(key, "TITLE") == 0) return "Custom catalog";
    return (char*)key; /* Old external INI: no MI keys, even in English. */
}

int main(void) {
    assert(strcmp(mi_catalog_text("TITLE"), "Explore mods") == 0);
    assert(strcmp(mi_catalog_text("SEARCH"), "Search mods") == 0);
    strcpy(configLanguage, "Spanish");
    assert(strcmp(mi_catalog_text("TITLE"), "Explorar mods") == 0);
    assert(strcmp(mi_catalog_text("DETAILS"), "Detalles del mod") == 0);
    assert(strcmp(mi_catalog_text("SEARCH"), "Buscar mods") == 0);
    strcpy(configLanguage, "Portuguese");
    assert(strcmp(mi_catalog_text("DETAILS"), "Detalhes do mod") == 0);
    assert(strcmp(mi_catalog_text("SEARCH"), "Buscar mods") == 0);
    strcpy(configLanguage, "French");
    assert(strcmp(mi_catalog_text("TITLE"), "Explore mods") == 0);
    customTranslation = 1;
    assert(strcmp(mi_catalog_text("TITLE"), "Custom catalog") == 0);
    puts("MI strings: old external languages, EN/ES/PT fallback and overrides passed.");
}
