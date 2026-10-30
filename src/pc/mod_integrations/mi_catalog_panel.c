#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "pc/fs/fs.h"
#include "pc/djui/djui.h"
#include "pc/djui/djui_panel.h"
#include "pc/djui/djui_panel_layout.h"
#include "pc/djui/djui_panel_menu.h"
#include "mi_catalog.h"
#include "mi_catalog_layout.h"
#include "mi_catalog_panel.h"
#include "mi_catalog_strings.h"
#include "mi_catalog_request.h"
#include "mi_install.h"
#include "mi_updates.h"
#include "mi_thumbnail.h"
#include "mi_thumbnail_texture.h"
#include "pc/djui/djui_image.h"
#include "pc/djui/djui_progress_bar.h"

#define MI_PAGE_SIZE 4

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
    struct DjuiSearchbox* searchbox;
    struct DjuiText* status;
    struct DjuiText* pageText;
    struct DjuiButton* rows[MI_PAGE_SIZE];
    struct DjuiText* authors[MI_PAGE_SIZE];
    struct DjuiText* categories[MI_PAGE_SIZE];
    struct DjuiRect* imageFrames[MI_PAGE_SIZE];
    struct DjuiImage* images[MI_PAGE_SIZE];
    struct DjuiText* imageLabels[MI_PAGE_SIZE];
    struct MiThumbnail* thumbnails[MI_PAGE_SIZE];
    enum MiThumbnailState thumbnailState[MI_PAGE_SIZE];
    char imageUrls[MI_PAGE_SIZE][2048];
    int imageSlots[MI_PAGE_SIZE];
    uint8_t thumbnailPixels[MI_THUMB_BYTES];
    struct DjuiButton* previous;
    struct DjuiButton* next;
    struct DjuiButton* retry;
    struct DjuiButton* checkUpdates;
    struct MiUpdates* updates;
    struct MiUpdatesInfo updatesInfo;
    bool updatesWanted, updatesOnly;
    uint64_t installRevision;
    size_t page;
    size_t pageSize;
};

static void mi_view_destroy(struct DjuiBase* base) {
    struct MiCatalogView* view = (struct MiCatalogView*)(intptr_t)base->tag;
    mi_catalog_request_close(view->request);
    mi_updates_close(view->updates);
    mi_catalog_free(view->catalog);
    for (size_t i = 0; i < MI_PAGE_SIZE; i++) {
        mi_thumbnail_close(view->thumbnails[i]);
        mi_thumbnail_texture_release(view->imageSlots[i]);
    }
    free(view);
    free(base);
}

static struct MiCatalogView* mi_view_for(struct DjuiBase* base) {
    while (base && base->destroy != mi_view_destroy) base = base->parent;
    return base ? (struct MiCatalogView*)(intptr_t)base->tag : NULL;
}

static void mi_updates_invalidate(struct MiCatalogView* view) {
    mi_updates_close(view->updates); view->updates = NULL;
    view->updatesOnly = false;
    view->updatesInfo = (struct MiUpdatesInfo){0};
}

static size_t mi_view_match_index(struct MiCatalogView* view, size_t offset) {
    for (size_t i = 0; i < mi_catalog_count(view->catalog); i++) {
        const struct MiCatalogEntry* entry = mi_catalog_entry(view->catalog, i);
        struct MiUpdateCandidate candidate;
        if (!mi_catalog_matches(entry, view->search->buffer)) continue;
        if (view->updatesOnly && !mi_updates_get(view->updates, entry->id, &candidate)) continue;
        if (!offset--) return i;
    }
    return SIZE_MAX;
}

static size_t mi_view_match_count(struct MiCatalogView* view) {
    size_t count = 0;
    for (size_t i = 0; i < mi_catalog_count(view->catalog); i++) {
        const struct MiCatalogEntry* entry = mi_catalog_entry(view->catalog, i);
        struct MiUpdateCandidate candidate;
        if (mi_catalog_matches(entry, view->search->buffer) &&
            (!view->updatesOnly || mi_updates_get(view->updates, entry->id, &candidate))) count++;
    }
    return count;
}

static struct DjuiText* mi_text(struct DjuiBase* body, const char* text, float height) {
    struct DjuiText* label = djui_text_create(body, text);
    struct DjuiColor color = gDjuiThemes[configDjuiTheme]->interactables.textColor;
    djui_base_set_size_type(&label->base, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
    djui_base_set_size(&label->base, 1, height);
    djui_base_set_color(&label->base, color.r, color.g, color.b, color.a);
    djui_text_set_font_scale(label, label->font->defaultFontScale * 1.1f);
    djui_text_set_drop_shadow(label, 64, 64, 64, 100);
    return label;
}

/* Run after native three-panel geometry, before its body is rendered. */
static void mi_anchor_workspace(struct DjuiBase* head, UNUSED bool* skip) {
    struct DjuiThreePanel* panel = (struct DjuiThreePanel*)head->parent;
    struct DjuiBase* base = &panel->base;
    float height = fmaxf(0, base->comp.height);
    float controls = panel->scrollControls && panel->scrollControls->base.visible ? 44 : 0;
    float header = fminf(panel->minHeaderSize.value, height);
    float footer = fminf(panel->minFooterSize.value, fmaxf(0, height - header));
    float bodyHeight = fmaxf(0, height - header - footer - controls);
    struct DjuiBase* body = djui_three_panel_get_body(panel);
    struct DjuiBase* foot = djui_three_panel_get_footer(panel);
    djui_base_set_size(head, 1, header);
    djui_base_set_location(body, 0, header);
    djui_base_set_size(body, 1, bodyHeight);
    djui_base_set_location(foot, 0, header + bodyHeight);
    djui_base_set_size(foot, 1, footer);
    panel->scrollMax = fmaxf(0, panel->contentHeight - bodyHeight);
    panel->scrollOffset = djui_panel_scroll_clamp(panel->scrollOffset, panel->scrollMax);
    if (panel->scrollControls) djui_base_set_location(&panel->scrollControls->base, 0, header + bodyHeight + footer);
}

/* Keep the native panel stack, theme, clipping and focus navigation. Only the
 * MI workspace expands; no global scale or user preference is changed. */
static struct DjuiThreePanel* mi_workspace(char* title, float headerHeight, float footerHeight) {
    struct DjuiThreePanel* panel = djui_three_panel_create(&gDjuiRoot->base, headerHeight, 0, footerHeight);
    struct DjuiThreePanelTheme theme = gDjuiThemes[configDjuiTheme]->threePanels;
    djui_base_set_size_type(&panel->base, DJUI_SVT_RELATIVE, DJUI_SVT_RELATIVE);
    djui_base_set_size(&panel->base, 0.96f, 1.0f);
    djui_base_set_alignment(&panel->base, DJUI_HALIGN_CENTER, DJUI_VALIGN_TOP);
    djui_base_set_padding(&panel->base, 12, 12, 12, 12);
    djui_base_set_border_width(&panel->base, 4);
    djui_base_set_color(&panel->base, theme.rectColor.r, theme.rectColor.g, theme.rectColor.b, fmaxf(242, theme.rectColor.a));
    djui_base_set_border_color(&panel->base, theme.borderColor.r, theme.borderColor.g, theme.borderColor.b, theme.borderColor.a);
    struct DjuiRect* header = djui_rect_container_create(&panel->base, headerHeight);
    header->base.on_render_pre = mi_anchor_workspace;
    struct DjuiText* heading = mi_text(&header->base, title, 44);
    djui_text_set_font_scale(heading, heading->font->defaultFontScale * 1.2f);
    struct DjuiFlowLayout* body = djui_flow_layout_create(&panel->base);
    djui_base_set_color(&body->base, 0, 0, 0, 0);
    djui_base_set_size_type(&body->base, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
    djui_base_set_size(&body->base, 1, 0);
    djui_flow_layout_set_margin(body, 8);
    djui_rect_container_create(&panel->base, footerHeight);
    panel->scrollable = true;
    return panel;
}

static void mi_place(struct DjuiBase* base, float x, float y, float width, float height) {
    djui_base_set_location_type(base, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
    djui_base_set_location(base, x, y);
    djui_base_set_size_type(base, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
    djui_base_set_size(base, width, height);
}

/* Allow translated action labels to wrap within their touch target. */
static void mi_action_label_fit(struct DjuiBase* base, UNUSED bool* skip) {
    struct DjuiText* text = (struct DjuiText*)base;
    djui_base_compute(base);
    float scale = text->font->defaultFontScale * 1.25f;
    do {
        djui_text_set_font_scale(text, scale);
        if (djui_text_count_lines(text, 8) * text->font->charHeight * scale <= base->comp.height) break;
        scale -= text->font->defaultFontScale * 0.05f;
    } while (scale >= text->font->defaultFontScale * 0.79f);
}

static void mi_description_fit(struct DjuiBase* base, bool* skip);

static void mi_field(struct DjuiBase* body, char* format, const char* value) {
    char text[512] = { 0 };
    djui_language_replace(format, text, sizeof(text), '@',
        (char*)(value[0] ? value : mi_catalog_text("UNKNOWN")));
    struct DjuiText* field = mi_text(body, text, 40);
    field->base.on_render_pre = mi_description_fit;
}

static void mi_description_fit(struct DjuiBase* base, UNUSED bool* skip) {
    djui_base_compute(base);
    struct DjuiText* text = (struct DjuiText*)base;
    djui_base_set_size(base, 1, fmaxf(30, djui_text_count_lines(text, 128) * text->font->charHeight * text->fontScale));
}

static void mi_reading_width(struct DjuiBase* base, UNUSED bool* skip) {
    djui_base_compute(base);
    float inset = fmaxf(0, (base->comp.width - 1000) / 2);
    djui_base_set_padding(base, 0, inset, 0, inset);
}

struct MiInstallView {
    struct MiCatalogFile* files;
    size_t count, selected;
    struct MiInstall* operation;
    struct MiUpdateCandidate update;
    bool hasUpdate;
    struct DjuiText* fileText;
    struct DjuiText* status;
    struct DjuiButton* install;
    struct DjuiButton* back;
    struct DjuiButton* previous;
    struct DjuiButton* next;
    struct DjuiProgressBar* bar;
    struct DjuiProgressBar* pulse;
    float progress, animation;
    enum MiInstallState phase;
};

static bool mi_install_active(enum MiInstallState state) {
    return state == MI_INSTALL_PENDING || state == MI_INSTALL_DOWNLOADING ||
        state == MI_INSTALL_EXTRACTING || state == MI_INSTALL_VERIFYING ||
        state == MI_INSTALL_COMMITTING || state == MI_INSTALL_CANCELLING;
}

static void mi_install_view_destroy(struct DjuiBase* base) {
    struct MiInstallView* view = (struct MiInstallView*)(intptr_t)base->tag;
    mi_install_close(view->operation);
    free(view->files);
    free(view);
    free(base);
}

static struct MiInstallView* mi_install_view_for(struct DjuiBase* base) {
    while (base && base->destroy != mi_install_view_destroy) base = base->parent;
    return base ? (struct MiInstallView*)(intptr_t)base->tag : NULL;
}

static const char* mi_install_error_text(enum MiInstallError error) {
    switch (error) {
        case MI_INSTALL_NETWORK: return mi_catalog_text("INSTALL_NETWORK");
        case MI_INSTALL_HTTP: return mi_catalog_text("INSTALL_HTTP");
        case MI_INSTALL_TLS: return mi_catalog_text("TLS_ERROR");
        case MI_INSTALL_STORAGE: return mi_catalog_text("INSTALL_STORAGE");
        case MI_INSTALL_UNSAFE: return mi_catalog_text("INSTALL_UNSAFE");
        case MI_INSTALL_LIMIT: return mi_catalog_text("INSTALL_LIMIT");
        case MI_INSTALL_EXISTS: return mi_catalog_text("INSTALL_EXISTS");
        case MI_INSTALL_BUSY: return mi_catalog_text("INSTALL_BUSY");
        case MI_INSTALL_INTEGRITY: return mi_catalog_text("INSTALL_INTEGRITY");
        case MI_INSTALL_RECOVERY: return mi_catalog_text("INSTALL_RECOVERY");
        default: return mi_catalog_text("INSTALL_FORMAT");
    }
}

static void mi_install_view_update(struct DjuiBase* base, UNUSED bool* skip) {
    struct MiInstallView* view = mi_install_view_for(base);
    struct MiInstallInfo info = {0};
    bool hasOperation = mi_install_poll(view->operation, &info);
    bool active = hasOperation && mi_install_active(info.state);
    bool supported = view->count && view->files[view->selected].supported;
    bool newer = view->hasUpdate && view->count &&
        !strcmp(view->files[view->selected].versionId, view->update.file.versionId) &&
        !strcmp(view->files[view->selected].fileId, view->update.file.fileId);
    const char* message = mi_catalog_text(newer ? "UPDATE_PREVIEW" : (supported ? "INSTALL_READY" : "INSTALL_UNSUPPORTED"));
    if (hasOperation) {
        switch (info.state) {
            case MI_INSTALL_PENDING: message = mi_catalog_text("INSTALL_WAIT"); break;
            case MI_INSTALL_DOWNLOADING: message = mi_catalog_text("INSTALL_DOWNLOAD"); break;
            case MI_INSTALL_EXTRACTING: message = mi_catalog_text("INSTALL_EXTRACT"); break;
            case MI_INSTALL_VERIFYING: message = mi_catalog_text("INSTALL_VERIFY"); break;
            case MI_INSTALL_COMMITTING: message = mi_catalog_text("INSTALL_FINISH"); break;
            case MI_INSTALL_CANCELLING: message = mi_catalog_text("INSTALL_CANCELLING"); break;
            case MI_INSTALL_CANCELLED: message = mi_catalog_text("INSTALL_CANCELLED"); break;
            case MI_INSTALL_COMPLETE: message = mi_catalog_text("INSTALL_DONE"); break;
            case MI_INSTALL_FAILED: message = mi_install_error_text(info.error); break;
        }
    }
    char status[1024];
    if (active && info.progress >= 0) snprintf(status, sizeof(status), "%s (%u%%)", message, (unsigned)(fminf(1, fmaxf(0, info.progress)) * 100));
    else if (hasOperation && info.error == MI_INSTALL_HTTP) snprintf(status, sizeof(status), "%s (HTTP %ld)", message, info.httpStatus);
    else snprintf(status, sizeof(status), "%s", message);
    if (newer && !hasOperation) snprintf(status, sizeof(status), "%s\n%s -> %s", message,
        view->update.installedVersion, view->update.file.version);
    size_t used = strlen(status);
    const char* filename = view->count ? view->files[view->selected].filename : "";
    if (active && info.state == MI_INSTALL_DOWNLOADING) {
        if (info.total) snprintf(status + used, sizeof(status) - used, "\n%s | %.1f / %.1f KiB", filename, (double)info.bytes / 1024, (double)info.total / 1024);
        else snprintf(status + used, sizeof(status) - used, "\n%s | %.1f KiB", filename, (double)info.bytes / 1024);
    } else if (hasOperation && info.state == MI_INSTALL_COMPLETE) {
        snprintf(status + used, sizeof(status) - used, "\nmods/%s", info.destination);
    } else if (active) snprintf(status + used, sizeof(status) - used, "\n%s", filename);
    djui_text_set_text(view->status, status);
    djui_text_set_text(view->install->text, mi_catalog_text(newer ? "UPDATE_AVAILABLE" : "INSTALL"));
    djui_base_set_enabled(&view->install->base, !active && supported && !newer && !(hasOperation && info.state == MI_INSTALL_COMPLETE));
    djui_base_set_enabled(&view->previous->base, !active && view->selected > 0);
    djui_base_set_enabled(&view->next->base, !active && view->selected + 1 < view->count);
    djui_text_set_text(view->back->text, active ? mi_catalog_text("INSTALL_CANCEL") : DLANG(MENU, BACK));
    djui_base_set_enabled(&view->back->base, !active || (info.state != MI_INSTALL_COMMITTING && info.state != MI_INSTALL_CANCELLING));
    djui_base_set_visible(&view->bar->base, active && info.progress >= 0);
    djui_base_set_visible(&view->pulse->base, active && info.progress < 0);
    if (hasOperation && info.state != view->phase) { view->bar->smoothValue = 0; view->phase = info.state; }
    view->progress = fminf(1, fmaxf(0, info.progress));
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    view->animation = (float)(now.tv_sec % 2) * 0.5f + (float)now.tv_nsec / 2000000000.0f;
}

static bool mi_install_view_back(struct DjuiBase* base) {
    struct MiInstallView* view = mi_install_view_for(base);
    struct MiInstallInfo info;
    if (mi_install_poll(view->operation, &info) && mi_install_active(info.state)) {
        mi_install_cancel(view->operation);
        return true;
    }
    return false;
}

static void mi_install_choose(struct DjuiBase* caller) {
    struct MiInstallView* view = mi_install_view_for(caller);
    if (!view || !view->count) return;
    struct MiInstallInfo info;
    if (mi_install_poll(view->operation, &info) && mi_install_active(info.state)) return;
    mi_install_close(view->operation); view->operation = NULL;
    if (caller == &view->previous->base && view->selected) view->selected--;
    if (caller == &view->next->base && view->selected + 1 < view->count) view->selected++;
    const struct MiCatalogFile* file = &view->files[view->selected];
    char text[768];
    snprintf(text, sizeof(text), "%zu / %zu | %s\n%s", view->selected + 1, view->count,
        file->version[0] ? file->version : mi_catalog_text("UNKNOWN"), file->filename[0] ? file->filename : mi_catalog_text("UNKNOWN"));
    djui_text_set_text(view->fileText, text);
}

static void mi_install_begin(struct DjuiBase* caller) {
    struct MiInstallView* view = mi_install_view_for(caller);
    struct MiInstallInfo info;
    if (!view || !view->count || (mi_install_poll(view->operation, &info) && mi_install_active(info.state))) return;
    mi_install_close(view->operation);
    view->operation = mi_install_start(fs_get_write_path(""), &view->files[view->selected]);
    if (!view->operation) djui_text_set_text(view->status, mi_catalog_text("INSTALL_STORAGE"));
}

static void mi_details(struct DjuiBase* caller) {
    struct MiCatalogView* view = mi_view_for(caller);
    if (!view || caller->tag < 0) return;
    const struct MiCatalogEntry* entry = mi_catalog_entry(view->catalog, (size_t)caller->tag);
    if (!entry) return;
    if (view->installRevision != mi_install_revision()) {
        mi_updates_invalidate(view); view->installRevision = mi_install_revision();
    }
    struct DjuiThreePanel* panel = mi_workspace(mi_catalog_text("DETAILS"), 56, 200);
    struct DjuiBase* body = djui_three_panel_get_body(panel);
    body->on_render_pre = mi_reading_width;
    struct DjuiText* title = mi_text(body, entry->title, 72);
    djui_text_set_font_scale(title, title->font->defaultFontScale * 1.4f);
    title->base.on_render_pre = mi_description_fit;
    mi_field(body, mi_catalog_text("AUTHOR"), entry->author);
    mi_field(body, mi_catalog_text("VERSION"), entry->version);
    mi_field(body, mi_catalog_text("CATEGORY"), entry->category);
    struct DjuiText* description = mi_text(body, entry->description[0] ? entry->description : mi_catalog_text("NO_DESCRIPTION"), 160);
    description->base.on_render_pre = mi_description_fit;
    struct MiInstallView* detail = calloc(1, sizeof(*detail));
    if (!detail) { djui_base_destroy(&panel->base); return; }
    detail->count = mi_catalog_file_count(view->catalog, (size_t)caller->tag);
    detail->files = calloc(detail->count, sizeof(*detail->files));
    if (detail->count && !detail->files) { free(detail); djui_base_destroy(&panel->base); return; }
    for (size_t i = 0; i < detail->count; i++) {
        detail->files[i] = *mi_catalog_file(view->catalog, (size_t)caller->tag, i);
    }
    bool chosen = false;
    for (size_t i = 0; i < detail->count; i++) if (detail->files[i].supported &&
        (!chosen || detail->files[i].recommended)) { detail->selected = i; chosen = true; if (detail->files[i].recommended) break; }
    detail->hasUpdate = mi_updates_get(view->updates, entry->id, &detail->update);
    if (detail->hasUpdate) for (size_t i = 0; i < detail->count; i++) {
        if (!strcmp(detail->files[i].versionId, detail->update.file.versionId) &&
            !strcmp(detail->files[i].fileId, detail->update.file.fileId)) { detail->selected = i; break; }
    }
    panel->base.tag = (intptr_t)detail;
    panel->base.destroy = mi_install_view_destroy;
    panel->base.on_render_pre = mi_install_view_update;
    panel->on_back = mi_install_view_back;
    detail->fileText = mi_text(body, "", 80);
    detail->fileText->base.on_render_pre = mi_description_fit;
    struct DjuiRect* choices = djui_rect_container_create(body, 64);
    detail->previous = djui_button_left_create(&choices->base, mi_catalog_text("PREVIOUS_FILE"), DJUI_BUTTON_STYLE_NORMAL, mi_install_choose);
    detail->next = djui_button_right_create(&choices->base, mi_catalog_text("NEXT_FILE"), DJUI_BUTTON_STYLE_NORMAL, mi_install_choose);
    mi_place(&detail->previous->base, 0, 0, 0.485f, 64);
    /* mi_place uses offsets from the left; right_create instead interprets x
     * as an inset from the right and would overlap the previous button. */
    djui_base_set_alignment(&detail->next->base, DJUI_HALIGN_LEFT, DJUI_VALIGN_TOP);
    mi_place(&detail->next->base, 0.515f, 0, 0.485f, 64);
    detail->previous->text->base.on_render_pre = mi_action_label_fit;
    detail->next->text->base.on_render_pre = mi_action_label_fit;
    struct DjuiBase* footer = djui_three_panel_get_footer(panel);
    struct DjuiButton* install = djui_button_right_create(footer, mi_catalog_text("INSTALL"), DJUI_BUTTON_STYLE_NORMAL, mi_install_begin);
    djui_base_set_alignment(&install->base, DJUI_HALIGN_LEFT, DJUI_VALIGN_TOP);
    mi_place(&install->base, 0.515f, 0, 0.485f, 64);
    install->text->base.on_render_pre = mi_action_label_fit;
    struct DjuiButton* back = djui_button_left_create(footer, DLANG(MENU, BACK), DJUI_BUTTON_STYLE_BACK, djui_panel_menu_back);
    mi_place(&back->base, 0, 0, 0.485f, 64);
    back->text->base.on_render_pre = mi_action_label_fit;
    detail->install = install; detail->back = back;
    /* Keep operation/error/result visible even when the description is scrolled. */
    detail->status = mi_text(footer, "", 88);
    mi_place(&detail->status->base, 0, 104, 1, 88);
    detail->status->base.on_render_pre = mi_action_label_fit;
    detail->bar = djui_progress_bar_create(footer, &detail->progress, 0, 1, false);
    detail->pulse = djui_progress_bar_create(footer, &detail->animation, 0, 1, true);
    mi_place(&detail->bar->base, 0, 76, 1, 20);
    mi_place(&detail->pulse->base, 0, 76, 1, 20);
    mi_install_choose(&install->base);
    mi_install_view_update(&panel->base, NULL);
    /* DJUI copies text. No borrowed DTO or catalog pointers in the detail panel. */
    if (!djui_panel_add(caller, panel, &back->base)) djui_base_destroy(&panel->base);
}

/* Button styles may move their label; restore absolute card coordinates after
 * style changes, while retaining native border/focus/hit testing and themes. */
static void mi_card_place(struct DjuiBase* base,float x,float y,float width,float height) {
    djui_base_set_alignment(base,DJUI_HALIGN_LEFT,DJUI_VALIGN_TOP);
    djui_base_set_location_type(base,DJUI_SVT_ABSOLUTE,DJUI_SVT_ABSOLUTE);
    djui_base_set_size_type(base,DJUI_SVT_ABSOLUTE,DJUI_SVT_ABSOLUTE);
    djui_base_set_location(base,x,y);djui_base_set_size(base,width,height);
}
static void mi_card_render(struct DjuiBase* base,UNUSED bool* skip) {
    struct MiCatalogView* view=mi_view_for(base);if(!view)return;
    size_t i=0;while(i<MI_PAGE_SIZE&&base!=&view->rows[i]->base)i++;if(i==MI_PAGE_SIZE)return;
    djui_base_compute(base);
    struct MiCardLayout card=mi_card_layout(fmaxf(0,base->comp.width-4),fmaxf(0,base->comp.height-4));
    struct DjuiButton* row=view->rows[i];
    mi_card_place(&view->imageFrames[i]->base,card.imageX,card.imageY,card.imageSize,card.imageSize);
    mi_card_place(&row->text->base,card.textX,card.textY,card.textWidth,88);
    mi_card_place(&view->authors[i]->base,card.textX,card.textY+88,card.textWidth,56);
    mi_card_place(&view->categories[i]->base,card.textX,card.textY+144,card.textWidth,56);
    djui_text_set_font_scale(row->text,row->text->font->defaultFontScale*1.1f);
    struct DjuiColor background=row->rect->base.color;
    djui_base_set_color(&row->rect->base,background.r,background.g,background.b,245);
    bool light=(background.r*299+background.g*587+background.b*114)>145000;
    if(light){
        djui_base_set_color(&row->text->base,105,63,0,255);
        djui_base_set_color(&view->authors[i]->base,0,65,110,255);
        djui_base_set_color(&view->categories[i]->base,0,83,50,255);
    }else{
        djui_base_set_color(&row->text->base,255,222,156,255);
        djui_base_set_color(&view->authors[i]->base,158,215,248,255);
        djui_base_set_color(&view->categories[i]->base,166,226,191,255);
    }
}
static void mi_thumbnail_update(struct MiCatalogView* view,size_t slot,const char* url) {
    if (!strcmp(view->imageUrls[slot],url))return;
    mi_thumbnail_close(view->thumbnails[slot]);view->thumbnails[slot]=NULL;
    snprintf(view->imageUrls[slot],sizeof(view->imageUrls[slot]),"%s",url);
    djui_base_set_visible(&view->images[slot]->base,false);
    djui_base_set_visible(&view->imageLabels[slot]->base,true);
    djui_text_set_text(view->imageLabels[slot],mi_catalog_text("THUMB_PLACEHOLDER"));
    view->thumbnailState[slot]=MI_THUMB_FAILED;
    if(url[0]&&view->imageSlots[slot]>=0){
        view->thumbnails[slot]=mi_thumbnail_start(fs_writepath,url);
        view->thumbnailState[slot]=MI_THUMB_LOADING;
        djui_text_set_text(view->imageLabels[slot],"...");
    }
}
static void mi_thumbnail_poll_all(struct MiCatalogView* view) {
    for(size_t i=0;i<MI_PAGE_SIZE;i++){
        if(mi_thumbnail_take(view->thumbnails[i],view->thumbnailPixels,sizeof(view->thumbnailPixels))&&
            mi_thumbnail_texture_publish(view->imageSlots[i],view->thumbnailPixels)){
            djui_base_set_visible(&view->images[i]->base,true);
            djui_base_set_visible(&view->imageLabels[i]->base,false);
        }
        enum MiThumbnailState state=mi_thumbnail_state(view->thumbnails[i]);
        if(state!=view->thumbnailState[i]){
            view->thumbnailState[i]=state;
            if(state==MI_THUMB_FAILED||state==MI_THUMB_CANCELLED)
                djui_text_set_text(view->imageLabels[i],mi_catalog_text("THUMB_PLACEHOLDER"));
        }
    }
}

static void mi_view_update(struct MiCatalogView* view) {
    size_t count = mi_view_match_count(view);
    size_t pages = count ? (count + view->pageSize - 1) / view->pageSize : 1;
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
    if (view->updatesWanted || (view->updates && view->updatesInfo.state == MI_UPDATES_RUNNING)) {
        snprintf(status, sizeof(status), "%s", mi_catalog_text("CHECKING_UPDATES"));
    } else if (view->updates && view->updatesInfo.state == MI_UPDATES_DONE) {
        char counts[128];
        snprintf(counts, sizeof(counts), "%zu / %zu / %zu", view->updatesInfo.available,
            view->updatesInfo.uncertain, view->updatesInfo.missing);
        djui_language_replace(mi_catalog_text("UPDATES_SUMMARY"), status, sizeof(status), '@', counts);
        if (view->fetch.fromCache || view->fetch.error != MI_FETCH_OK) {
            size_t used = strlen(status); snprintf(status + used, sizeof(status) - used, " %s", mi_catalog_text("USING_SAVED"));
        }
    } else if (view->updates && view->updatesInfo.state == MI_UPDATES_FAILED) {
        snprintf(status, sizeof(status), "%s", mi_catalog_text(view->updatesInfo.error == MI_UPDATES_BUSY ? "UPDATES_BUSY" : "UPDATES_ERROR"));
    }
    djui_text_set_text(view->checkUpdates->text, mi_catalog_text(view->updatesOnly ? "SHOW_ALL_MODS" : "CHECK_UPDATES"));
    djui_base_set_enabled(&view->checkUpdates->base, !view->updatesWanted && !view->fetch.running &&
        !(view->updates && view->updatesInfo.state == MI_UPDATES_RUNNING));
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
    djui_base_set_visible(&view->checkedAt->base, checked[0] != '\0' && gDjuiRoot->base.comp.width >= 1500);
    for (size_t slot = 0; slot < MI_PAGE_SIZE; ++slot) {
        size_t index = slot < view->pageSize ? mi_view_match_index(view, view->page * view->pageSize + slot) : SIZE_MAX;
        const struct MiCatalogEntry* entry = mi_catalog_entry(view->catalog, index);
        view->rows[slot]->base.tag = entry ? (s64)index : -1;
        djui_base_set_visible(&view->rows[slot]->base, entry != NULL);
        djui_base_set_enabled(&view->rows[slot]->base, entry != NULL);
        mi_thumbnail_update(view,slot,entry?entry->imageUrl:"");
        if (entry) {
            djui_text_set_text(view->rows[slot]->text, entry->title);
            char metadata[512];
            djui_language_replace(mi_catalog_text("AUTHOR"), metadata, sizeof(metadata), '@',
                (char*)(entry->author[0] ? entry->author : mi_catalog_text("UNKNOWN")));
            djui_text_set_text(view->authors[slot], metadata);
            djui_language_replace(mi_catalog_text("CATEGORY"), metadata, sizeof(metadata), '@',
                (char*)(entry->category[0] ? entry->category : mi_catalog_text("UNKNOWN")));
            djui_text_set_text(view->categories[slot], metadata);

        }
    }
    char page[96];
    snprintf(page, sizeof(page), "%zu / %zu\n(%zu)", view->page + 1, pages, count);
    djui_text_set_text(view->pageText, page);
    djui_base_set_enabled(&view->previous->base, ready && view->page > 0);
    djui_base_set_enabled(&view->next->base, ready && view->page + 1 < pages);
    djui_text_set_text(view->retry->text, mi_catalog_text(view->fetch.error == MI_FETCH_OK ? "REFRESH" : "RETRY"));
    djui_base_set_enabled(&view->retry->base, !view->fetch.running && !view->updatesWanted &&
        !(view->updates && view->updatesInfo.state == MI_UPDATES_RUNNING));
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
    mi_updates_invalidate(view);
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
    struct DjuiThreePanel* panel = (struct DjuiThreePanel*)base;
    struct MiCatalogLayout layout = mi_catalog_layout(base->parent->comp.width, base->parent->comp.height);
    panel->bodySize.value = layout.rows * layout.cardHeight + (layout.rows - 1) * 12;
    for (size_t i = 0; i < MI_PAGE_SIZE; ++i) {
        float width = layout.columns == 2 ? 0.49f : 1;
        mi_place(&view->rows[i]->base, (i % layout.columns) * 0.51f,
            (i / layout.columns) * (layout.cardHeight + 12), width, layout.cardHeight);

    }
    if (view->pageSize != layout.pageSize) {
        view->page = view->page * view->pageSize / layout.pageSize;
        view->pageSize = layout.pageSize;
        mi_view_update(view);
    }
    bool changed = false;
    if (view->installRevision != mi_install_revision()) {
        mi_updates_invalidate(view); view->installRevision = mi_install_revision(); changed = true;
    }
    struct MiCatalog* snapshot = NULL;
    struct MiCatalogFetchInfo info;
    if (mi_catalog_request_poll(view->request, view->revision, &snapshot, &info)) {
        if (snapshot) {
            mi_updates_invalidate(view);
            mi_catalog_free(view->catalog); view->catalog = snapshot;
        }
        if (!info.savedAt && mi_catalog_status(view->catalog) == MI_CATALOG_READY) info.savedAt = view->fetch.savedAt;
        view->fetch = info; view->revision = info.revision; changed = true;
    }
    if (view->updatesWanted && !view->fetch.running) {
        view->updatesWanted = false;
        if (mi_catalog_status(view->catalog) == MI_CATALOG_READY) {
            view->updates = mi_updates_start(fs_get_write_path(""), view->catalog);
            view->updatesInfo.state = MI_UPDATES_RUNNING;
            if (!view->updates) djui_popup_create(mi_catalog_text("UPDATES_ERROR"), 2);
        }
        changed = true;
    }
    struct MiUpdatesInfo result;
    if (mi_updates_poll(view->updates, &result) && (result.state != view->updatesInfo.state ||
        result.error != view->updatesInfo.error || result.checked != view->updatesInfo.checked ||
        result.missing != view->updatesInfo.missing || result.uncertain != view->updatesInfo.uncertain ||
        result.available != view->updatesInfo.available)) {
        view->updatesInfo = result; changed = true;
        if (result.state == MI_UPDATES_DONE) { view->updatesOnly = result.available > 0; view->page = 0; }
    }
    if (changed) mi_view_update(view);
    mi_thumbnail_poll_all(view);
}

static void mi_check_updates(struct DjuiBase* caller) {
    struct MiCatalogView* view = mi_view_for(caller);
    if (!view || view->fetch.running || view->updatesWanted ||
        (view->updates && view->updatesInfo.state == MI_UPDATES_RUNNING)) return;
    if (view->updatesOnly) { view->updatesOnly = false; view->page = 0; mi_view_update(view); return; }
    mi_begin_request(view, true); view->updatesWanted = true;
    view->installRevision = mi_install_revision();
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
    for(size_t i=0;i<MI_PAGE_SIZE;i++){view->imageSlots[i]=-1;view->thumbnailState[i]=MI_THUMB_FAILED;}
    view->pageSize = MI_PAGE_SIZE;
    view->installRevision = mi_install_revision();
    mi_begin_request(view, false);
    struct DjuiThreePanel* panel = mi_workspace(mi_catalog_text("TITLE"), 156, 72);
    panel->base.tag = (s64)(intptr_t)view;
    panel->base.destroy = mi_view_destroy;
    panel->base.on_render_pre = mi_poll_request;
    struct DjuiBase* header = djui_three_panel_get_header(panel);
    struct DjuiBase* body = djui_three_panel_get_body(panel);
    struct DjuiBase* footer = djui_three_panel_get_footer(panel);
    body->on_child_render = NULL; /* Grid, not vertical flow. */
    struct DjuiSearchbox* search = djui_searchbox_create(header, mi_search_changed);
    mi_place(&search->base, 0, 48, 0.46f, 52);
    djui_base_set_size(&search->inputbox->base, 1, 52);
    djui_base_set_size(search->inputbox->base.parent, 1, 52);
    view->search = search->inputbox;
    view->searchbox = search;
    djui_text_set_text(search->text, mi_catalog_text("SEARCH"));
    view->retry = djui_button_create(header, mi_catalog_text("REFRESH"), DJUI_BUTTON_STYLE_NORMAL, mi_retry);
    mi_place(&view->retry->base, 0.48f, 48, 0.22f, 52);
    view->checkUpdates = djui_button_create(header, mi_catalog_text("CHECK_UPDATES"), DJUI_BUTTON_STYLE_NORMAL, mi_check_updates);
    mi_place(&view->checkUpdates->base, 0.72f, 48, 0.28f, 52);
    view->checkUpdates->text->base.on_render_pre = mi_action_label_fit;
    view->status = mi_text(header, mi_catalog_text("LOADING"), 48);
    mi_place(&view->status->base, 0, 104, 1, 52);
    djui_text_set_font_scale(view->status, view->status->font->defaultFontScale * 0.9f);
    view->checkedAt = mi_text(header, "", 22);
    mi_place(&view->checkedAt->base, 0.52f, 4, 0.48f, 36);
    djui_text_set_font_scale(view->checkedAt, view->checkedAt->font->defaultFontScale * 0.95f);
    for (size_t slot = 0; slot < MI_PAGE_SIZE; ++slot) {
        view->rows[slot] = djui_button_create(body, "", DJUI_BUTTON_STYLE_NORMAL, mi_details);
        djui_base_set_size(&view->rows[slot]->base, 1, 96);
        view->rows[slot]->base.on_render_pre=mi_card_render;
        struct DjuiText* title = view->rows[slot]->text;
        mi_place(&title->base, 0, 0, 0.97f, 66);
        djui_base_set_location_type(&title->base, DJUI_SVT_ABSOLUTE, DJUI_SVT_ABSOLUTE);
        djui_text_set_alignment(title, DJUI_HALIGN_LEFT, DJUI_VALIGN_CENTER);
        djui_text_set_font_scale(title, title->font->defaultFontScale);
        view->authors[slot] = mi_text(&view->rows[slot]->rect->base, "", 48);
        view->categories[slot] = mi_text(&view->rows[slot]->rect->base, "", 68);
        djui_text_set_font_scale(view->authors[slot],view->authors[slot]->font->defaultFontScale);
        djui_text_set_font_scale(view->categories[slot],view->categories[slot]->font->defaultFontScale);
        view->imageFrames[slot]=djui_rect_create(&view->rows[slot]->rect->base);
        djui_base_set_color(&view->imageFrames[slot]->base,25,32,42,255);
        djui_base_set_border_width(&view->imageFrames[slot]->base,2);
        djui_base_set_border_color(&view->imageFrames[slot]->base,128,144,160,255);
        djui_base_set_padding(&view->imageFrames[slot]->base,4,4,4,4);
        view->imageLabels[slot]=mi_text(&view->imageFrames[slot]->base,mi_catalog_text("THUMB_PLACEHOLDER"),44);
        djui_base_set_size_type(&view->imageLabels[slot]->base,DJUI_SVT_RELATIVE,DJUI_SVT_RELATIVE);
        djui_base_set_size(&view->imageLabels[slot]->base,1,1);
        djui_text_set_alignment(view->imageLabels[slot],DJUI_HALIGN_CENTER,DJUI_VALIGN_CENTER);
        djui_base_set_color(&view->imageLabels[slot]->base,195,205,220,255);
        view->imageSlots[slot]=mi_thumbnail_texture_acquire();
        view->images[slot]=djui_image_create(&view->imageFrames[slot]->base,
            (const Texture*)mi_thumbnail_texture_pixels(view->imageSlots[slot]),MI_THUMB_EDGE,MI_THUMB_EDGE,G_IM_FMT_RGBA,G_IM_SIZ_32b);
        djui_base_set_size_type(&view->images[slot]->base,DJUI_SVT_RELATIVE,DJUI_SVT_RELATIVE);
        djui_base_set_size(&view->images[slot]->base,1,1);
        djui_base_set_visible(&view->images[slot]->base,false);
    }
    struct DjuiButton* back = djui_button_create(footer, DLANG(MENU, BACK), DJUI_BUTTON_STYLE_BACK, djui_panel_menu_back);
    mi_place(&back->base, 0, 4, 0.22f, 64);
    view->previous = djui_button_create(footer, mi_catalog_text("PREVIOUS"), DJUI_BUTTON_STYLE_NORMAL, mi_page_changed);
    mi_place(&view->previous->base, 0.24f, 4, 0.22f, 64);
    view->pageText = mi_text(footer, "", 48);
    mi_place(&view->pageText->base, 0.47f, 4, 0.28f, 64);
    djui_text_set_alignment(view->pageText, DJUI_HALIGN_CENTER, DJUI_VALIGN_CENTER);
    djui_text_set_font_scale(view->pageText, view->pageText->font->defaultFontScale * 0.95f);
    view->next = djui_button_create(footer, mi_catalog_text("NEXT"), DJUI_BUTTON_STYLE_NORMAL, mi_page_changed);
    mi_place(&view->next->base, 0.77f, 4, 0.23f, 64);
    back->text->base.on_render_pre = mi_action_label_fit;
    view->retry->text->base.on_render_pre = mi_action_label_fit;
    view->previous->text->base.on_render_pre = mi_action_label_fit;
    view->next->text->base.on_render_pre = mi_action_label_fit;
    mi_view_update(view);
    struct DjuiPanel* added = djui_panel_add(caller, panel, &view->search->base);
    if (!added) djui_base_destroy(&panel->base);
    return added;
}
