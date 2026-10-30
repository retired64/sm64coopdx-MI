#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "pc/fs/fs.h"
#include "pc/djui/djui.h"
#include "pc/djui/djui_panel.h"
#include "pc/djui/djui_panel_menu.h"
#include "mi_catalog.h"
#include "mi_catalog_panel.h"
#include "mi_catalog_strings.h"
#include "mi_catalog_request.h"

#define MI_PAGE_SIZE 5

/* Worker results are polled only by the main thread. Closing the panel drops
 * request ownership immediately; no worker ever sees a DJUI pointer.
 * Own page controls avoid the shared static controls in djui_paginated.c:
 * the game's existing mod panel remains alive underneath this one. */
struct MiCatalogView {
    struct MiCatalog* catalog;
    struct MiCatalogRequest* request;
    struct MiCatalogFetchInfo fetch;
    uint64_t revision;
    struct DjuiText* checkedAt;
    struct DjuiInputbox* search;
    struct DjuiText* status;
    struct DjuiText* pageText;
    struct DjuiButton* rows[MI_PAGE_SIZE];
    struct DjuiButton* previous;
    struct DjuiButton* next;
    struct DjuiButton* retry;
    size_t page;
};

static void mi_view_destroy(struct DjuiBase* base) {
    struct MiCatalogView* view = (struct MiCatalogView*)(intptr_t)base->tag;
    mi_catalog_request_close(view->request);
    mi_catalog_free(view->catalog);
    free(view);
    free(base);
}

static struct MiCatalogView* mi_view_for(struct DjuiBase* base) {
    while (base && base->destroy != mi_view_destroy) base = base->parent;
    return base ? (struct MiCatalogView*)(intptr_t)base->tag : NULL;
}

static struct DjuiText* mi_text(struct DjuiBase* body, const char* text, float height) {
    struct DjuiText* label = djui_text_create(body, text);
    struct DjuiColor color = gDjuiThemes[configDjuiTheme]->interactables.textColor;
    djui_base_set_size_type(&label->base, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
    djui_base_set_size(&label->base, 1, height);
    djui_base_set_color(&label->base, color.r, color.g, color.b, color.a);
    djui_text_set_font_scale(label, label->font->defaultFontScale * 0.8f);
    djui_text_set_drop_shadow(label, 64, 64, 64, 100);
    return label;
}

static void mi_field(struct DjuiBase* body, char* format, const char* value) {
    char text[512] = { 0 };
    djui_language_replace(format, text, sizeof(text), '@',
        (char*)(value[0] ? value : mi_catalog_text("UNKNOWN")));
    mi_text(body, text, 30);
}

static void mi_description_fit(struct DjuiBase* base, UNUSED bool* skip) {
    djui_base_compute(base);
    struct DjuiText* text = (struct DjuiText*)base;
    djui_base_set_size(base, 1, fmaxf(30, djui_text_count_lines(text, 128) * text->font->charHeight * text->fontScale));
}

static void mi_details(struct DjuiBase* caller) {
    struct MiCatalogView* view = mi_view_for(caller);
    if (!view || caller->tag < 0) return;
    const struct MiCatalogEntry* entry = mi_catalog_entry(view->catalog, (size_t)caller->tag);
    if (!entry) return;
    struct DjuiThreePanel* panel = djui_panel_menu_create(mi_catalog_text("DETAILS"), false);
    struct DjuiBase* body = djui_three_panel_get_body(panel);
    mi_text(body, entry->title, 64);
    mi_field(body, mi_catalog_text("AUTHOR"), entry->author);
    mi_field(body, mi_catalog_text("VERSION"), entry->version);
    mi_field(body, mi_catalog_text("CATEGORY"), entry->category);
    struct DjuiText* description = mi_text(body, entry->description[0] ? entry->description : mi_catalog_text("NO_DESCRIPTION"), 160);
    description->base.on_render_pre = mi_description_fit;
    mi_text(body, mi_catalog_text("INSTALL_PENDING"), 56);
    struct DjuiButton* install = djui_button_create(body, mi_catalog_text("INSTALL"), DJUI_BUTTON_STYLE_NORMAL, NULL);
    djui_base_set_size(&install->base, 1, 40);
    djui_base_set_enabled(&install->base, false);
    struct DjuiButton* back = djui_button_create(body, DLANG(MENU, BACK), DJUI_BUTTON_STYLE_BACK, djui_panel_menu_back);
    djui_base_set_size(&back->base, 1, 40);
    /* DJUI copies text. No borrowed DTO or catalog pointers in the detail panel. */
    if (!djui_panel_add(caller, panel, &back->base)) djui_base_destroy(&panel->base);
}

static void mi_view_update(struct MiCatalogView* view) {
    const char* query = view->search->buffer;
    size_t count = mi_catalog_match_count(view->catalog, query);
    size_t pages = count ? (count + MI_PAGE_SIZE - 1) / MI_PAGE_SIZE : 1;
    if (view->page >= pages) view->page = pages - 1;
    bool ready = mi_catalog_status(view->catalog) == MI_CATALOG_READY;
    const char* message;
    if (view->fetch.running) message = mi_catalog_text(ready ? "UPDATING" : "LOADING");
    else if (view->fetch.error == MI_FETCH_TLS) message = mi_catalog_text("TLS_ERROR");
    else if (view->fetch.error == MI_FETCH_HTTP) message = mi_catalog_text("HTTP_ERROR");
    else if (view->fetch.error == MI_FETCH_INVALID || view->fetch.error == MI_FETCH_UNSUPPORTED) message = mi_catalog_text("INVALID_REMOTE");
    else if (view->fetch.error == MI_FETCH_STORAGE) message = mi_catalog_text("CACHE_ERROR");
    else if (view->fetch.error != MI_FETCH_OK || !ready) message = mi_catalog_text("NETWORK_ERROR");
    else message = mi_catalog_text(view->fetch.fromCache ? "CACHED" : "ONLINE");
    char status[768] = { 0 };
    char http[24];
    snprintf(http, sizeof(http), "%ld", view->fetch.httpStatus);
    djui_language_replace((char*)message, status, sizeof(status), '@', http);
    if (ready && !view->fetch.running && view->fetch.error != MI_FETCH_OK && view->fetch.error != MI_FETCH_STORAGE) {
        size_t used = strlen(status);
        snprintf(status + used, sizeof(status) - used, " %s", mi_catalog_text("USING_SAVED"));
    }
    if (ready && !view->fetch.running && view->fetch.error == MI_FETCH_OK && !mi_catalog_count(view->catalog))
        snprintf(status, sizeof(status), "%s", mi_catalog_text("EMPTY"));
    else if (ready && !count && mi_catalog_count(view->catalog))
        snprintf(status, sizeof(status), "%s", mi_catalog_text("NO_RESULTS"));
    djui_text_set_text(view->status, status);
    char checked[128] = { 0 };
    if (ready && view->fetch.savedAt > 0) {
        time_t timestamp = (time_t)view->fetch.savedAt;
        struct tm local;
        char date[40];
        if (localtime_r(&timestamp, &local) && strftime(date, sizeof(date), "%Y-%m-%d %H:%M", &local))
            djui_language_replace(mi_catalog_text("LAST_CHECKED"), checked, sizeof(checked), '@', date);
    }
    djui_text_set_text(view->checkedAt, checked);
    djui_base_set_visible(&view->checkedAt->base, checked[0] != '\0');
    for (size_t slot = 0; slot < MI_PAGE_SIZE; ++slot) {
        size_t index = mi_catalog_match_index(view->catalog, query, view->page * MI_PAGE_SIZE + slot);
        const struct MiCatalogEntry* entry = mi_catalog_entry(view->catalog, index);
        view->rows[slot]->base.tag = entry ? (s64)index : -1;
        djui_base_set_visible(&view->rows[slot]->base, entry != NULL);
        djui_base_set_enabled(&view->rows[slot]->base, entry != NULL);
        if (entry) djui_text_set_text(view->rows[slot]->text, entry->title);
    }
    char page[96];
    snprintf(page, sizeof(page), "%zu / %zu  (%zu)", view->page + 1, pages, count);
    djui_text_set_text(view->pageText, page);
    djui_base_set_enabled(&view->previous->base, ready && view->page > 0);
    djui_base_set_enabled(&view->next->base, ready && view->page + 1 < pages);
    djui_text_set_text(view->retry->text, mi_catalog_text(view->fetch.error == MI_FETCH_OK ? "REFRESH" : "RETRY"));
    djui_base_set_enabled(&view->retry->base, !view->fetch.running);
}

static void mi_search_changed(struct DjuiBase* caller) {
    struct MiCatalogView* view = mi_view_for(caller);
    if (!view) return;
    view->page = 0;
    mi_view_update(view);
}

static void mi_page_changed(struct DjuiBase* caller) {
    struct MiCatalogView* view = mi_view_for(caller);
    if (!view) return;
    if (caller == &view->next->base) ++view->page;
    else if (view->page) --view->page;
    mi_view_update(view);
}

static void mi_begin_request(struct MiCatalogView* view, bool forceRefresh) {
    mi_catalog_request_close(view->request);
    view->revision = 0;
    const char* language = strcmp(configLanguage, "Spanish") == 0 ? "es" :
        (strcmp(configLanguage, "Portuguese") == 0 ? "pt" : "en");
    char directory[SYS_MAX_PATH];
    int written = snprintf(directory, sizeof(directory), "%s/mi", fs_writepath);
    view->request = written >= 0 && (size_t)written < sizeof(directory) ?
        (forceRefresh ? mi_catalog_request_refresh(directory, language) : mi_catalog_request_start(directory, language)) : NULL;
    view->fetch.running = view->request != NULL;
    view->fetch.error = view->request ? MI_FETCH_OK : MI_FETCH_NETWORK;
}

static void mi_poll_request(struct DjuiBase* base, UNUSED bool* skip) {
    struct MiCatalogView* view = (struct MiCatalogView*)(intptr_t)base->tag;
    struct MiCatalog* snapshot = NULL;
    struct MiCatalogFetchInfo info;
    if (!mi_catalog_request_poll(view->request, view->revision, &snapshot, &info)) return;
    if (snapshot) {
        mi_catalog_free(view->catalog);
        view->catalog = snapshot;
    }
    if (!info.savedAt && mi_catalog_status(view->catalog) == MI_CATALOG_READY)
        info.savedAt = view->fetch.savedAt;
    view->fetch = info;
    view->revision = info.revision;
    mi_view_update(view);
}

static void mi_retry(struct DjuiBase* caller) {
    struct MiCatalogView* view = mi_view_for(caller);
    if (!view || view->fetch.running) return;
    mi_begin_request(view, true);
    mi_view_update(view);
}

struct DjuiPanel* mi_catalog_panel_create(struct DjuiBase* caller) {
    struct MiCatalogView* view = calloc(1, sizeof(*view));
    if (!view) { djui_popup_create(mi_catalog_text("ERROR"), 2); return NULL; }
    mi_begin_request(view, false);
    struct DjuiThreePanel* panel = djui_panel_menu_create(mi_catalog_text("TITLE"), false);
    panel->base.tag = (s64)(intptr_t)view;
    panel->base.destroy = mi_view_destroy;
    panel->base.on_render_pre = mi_poll_request;
    struct DjuiBase* body = djui_three_panel_get_body(panel);
    struct DjuiSearchbox* search = djui_searchbox_create(body, mi_search_changed);
    view->search = search->inputbox;
    view->status = mi_text(body, mi_catalog_text("LOADING"), 88);
    view->checkedAt = mi_text(body, "", 30);
    for (size_t slot = 0; slot < MI_PAGE_SIZE; ++slot) {
        view->rows[slot] = djui_button_create(body, "", DJUI_BUTTON_STYLE_NORMAL, mi_details);
        djui_base_set_size(&view->rows[slot]->base, 1, 40);
    }
    struct DjuiRect* navigation = djui_rect_container_create(body, 36);
    view->previous = djui_button_left_create(&navigation->base, "<", DJUI_BUTTON_STYLE_NORMAL, mi_page_changed);
    djui_base_set_size(&view->previous->base, 0.20f, 36);
    view->next = djui_button_right_create(&navigation->base, ">", DJUI_BUTTON_STYLE_NORMAL, mi_page_changed);
    djui_base_set_size(&view->next->base, 0.20f, 36);
    view->pageText = mi_text(&navigation->base, "", 36);
    djui_base_set_size(&view->pageText->base, 0.60f, 36);
    djui_base_set_alignment(&view->pageText->base, DJUI_HALIGN_CENTER, DJUI_VALIGN_TOP);
    djui_text_set_alignment(view->pageText, DJUI_HALIGN_CENTER, DJUI_VALIGN_CENTER);
    view->retry = djui_button_create(body, mi_catalog_text("RETRY"), DJUI_BUTTON_STYLE_NORMAL, mi_retry);
    djui_base_set_size(&view->retry->base, 1, 36);
    struct DjuiButton* back = djui_button_create(body, DLANG(MENU, BACK), DJUI_BUTTON_STYLE_BACK, djui_panel_menu_back);
    djui_base_set_size(&back->base, 1, 40);
    mi_view_update(view);
    struct DjuiPanel* added = djui_panel_add(caller, panel, &view->search->base);
    if (!added) djui_base_destroy(&panel->base);
    return added;
}
