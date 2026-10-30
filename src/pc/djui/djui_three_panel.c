#include "djui.h"
#include "djui_panel_layout.h"

bool djui_three_panel_render(struct DjuiBase* base);

struct DjuiBase* djui_three_panel_get_header(struct DjuiThreePanel* threePanel) {
    struct DjuiBase* children[3] = { NULL };
    struct DjuiBaseChild* child = threePanel->base.child;
    for (int i = 0; i < 3; i++) {
        if (child == NULL || child->base == NULL) { break; }
        children[i] = child->base;
        child = child->next;
    }
    return children[0];
}

struct DjuiBase* djui_three_panel_get_body(struct DjuiThreePanel* threePanel) {
    struct DjuiBase* children[3] = { NULL };
    struct DjuiBaseChild* child = threePanel->base.child;
    for (int i = 0; i < 3; i++) {
        if (child == NULL || child->base == NULL) { break; }
        children[i] = child->base;
        child = child->next;
    }
    return children[1];
}

struct DjuiBase* djui_three_panel_get_footer(struct DjuiThreePanel* threePanel) {
    struct DjuiBase* children[3] = { NULL };
    struct DjuiBaseChild* child = threePanel->base.child;
    for (int i = 0; i < 3; i++) {
        if (child == NULL || child->base == NULL) { break; }
        children[i] = child->base;
        child = child->next;
    }
    return children[2] == (struct DjuiBase*)threePanel->scrollControls ? NULL : children[2];
}

void djui_three_panel_set_min_header_size_type(struct DjuiThreePanel* threePanel, enum DjuiScreenValueType minHeaderSizeType) {
    threePanel->minHeaderSize.type = minHeaderSizeType;
}

void djui_three_panel_set_min_header_size(struct DjuiThreePanel* threePanel, f32 minHeaderSize) {
    threePanel->minHeaderSize.value = minHeaderSize;
}

void djui_three_panel_set_body_size_type(struct DjuiThreePanel* threePanel, enum DjuiScreenValueType bodySizeType) {
    threePanel->bodySize.type = bodySizeType;
}

void djui_three_panel_set_body_size(struct DjuiThreePanel* threePanel, f32 bodySize) {
    threePanel->bodySize.value = bodySize;
}

void djui_three_panel_set_min_footer_size_type(struct DjuiThreePanel* threePanel, enum DjuiScreenValueType minFooterSizeType) {
    threePanel->minFooterSize.type = minFooterSizeType;
}

void djui_three_panel_set_min_footer_size(struct DjuiThreePanel* threePanel, f32 minFooterSize) {
    threePanel->minFooterSize.value = minFooterSize;
}

void djui_three_panel_recalculate_body_size(struct DjuiThreePanel* threePanel) {
    struct DjuiFlowLayout* body = (struct DjuiFlowLayout*)djui_three_panel_get_body(threePanel);
    struct DjuiBaseChild* child = body->base.child;

    if (threePanel->bodySize.value != 0) { return; }

    if (body->margin.type != DJUI_SVT_ABSOLUTE) {
        return;
    }

    f32 bodyHeight = body->margin.value;
    while (child != NULL) {
        bodyHeight += body->margin.value;
        bodyHeight += child->base->height.value;
        if (child->base->height.type != DJUI_SVT_ABSOLUTE) {
            return;
        }
        child = child->next;
    }
    if (bodyHeight < 0) { bodyHeight = 0; }
    djui_three_panel_set_body_size(threePanel, bodyHeight);
}

/* Offset the content coordinates while retaining the body's viewport clip and
 * elem rectangle. Input hit testing therefore stays inside the visible body. */
static bool djui_three_panel_body_render(struct DjuiBase* base) {
    struct DjuiThreePanel* panel = (struct DjuiThreePanel*)base->parent;
    djui_rect_render(base);
    base->comp.y -= panel->scrollOffset;
    base->comp.height = fmax(base->comp.height, panel->contentHeight);
    return true;
}

static struct DjuiThreePanel* djui_three_panel_for_descendant(struct DjuiBase* base) {
    while (base) {
        if (base->render == djui_three_panel_render) {
            struct DjuiThreePanel* panel = (struct DjuiThreePanel*)base;
            return panel->scrollable ? panel : NULL;
        }
        base = base->parent;
    }
    return NULL;
}

bool djui_three_panel_scroll_from(struct DjuiBase* descendant, f32 amount) {
    struct DjuiThreePanel* panel = djui_three_panel_for_descendant(descendant);
    if (!panel || panel->scrollMax <= 0 || !panel->base.enabled) return false;
    panel->scrollOffset = djui_panel_scroll_clamp(panel->scrollOffset + amount, panel->scrollMax);
    return true;
}

void djui_three_panel_reveal(struct DjuiBase* descendant) {
    struct DjuiThreePanel* panel = djui_three_panel_for_descendant(descendant);
    if (!panel || panel->scrollMax <= 0) return;
    struct DjuiBase* body = djui_three_panel_get_body(panel);
    struct DjuiBase* ancestor = descendant;
    while (ancestor && ancestor != body) ancestor = ancestor->parent;
    if (!ancestor || body->elem.height <= 0) return;
    f32 delta = 0;
    if (descendant->elem.y < body->elem.y) delta = descendant->elem.y - body->elem.y;
    else if (descendant->elem.y + descendant->elem.height > body->elem.y + body->elem.height)
        delta = descendant->elem.y + descendant->elem.height - body->elem.y - body->elem.height;
    panel->scrollOffset = djui_panel_scroll_clamp(panel->scrollOffset + delta, panel->scrollMax);
}

static void djui_three_panel_scroll_button(struct DjuiBase* caller) {
    struct DjuiThreePanel* panel = (struct DjuiThreePanel*)caller->parent->parent;
    struct DjuiBase* body = djui_three_panel_get_body(panel);
    f32 step = fmax(48, body->elem.height * 0.75f);
    djui_three_panel_scroll_from(caller, caller == &panel->scrollUp->base ? -step : step);
}

static void djui_three_panel_scroll_controls(struct DjuiThreePanel* panel) {
    panel->scrollControls = djui_rect_container_create(&panel->base, 44);
    panel->scrollUp = djui_button_left_create(&panel->scrollControls->base, "^", DJUI_BUTTON_STYLE_NORMAL, djui_three_panel_scroll_button);
    panel->scrollDown = djui_button_right_create(&panel->scrollControls->base, "v", DJUI_BUTTON_STYLE_NORMAL, djui_three_panel_scroll_button);
    djui_base_set_size(&panel->scrollUp->base, 0.485f, 44);
    djui_base_set_size(&panel->scrollDown->base, 0.485f, 44);
}

/* Measure visible absolute-height children of standard vertical menu bodies.
 * Custom layouts retain their explicit body-size contract and do not scroll. */
static f32 djui_three_panel_content_height(struct DjuiThreePanel* panel, struct DjuiBase* body, f32 fallback) {
    struct DjuiFlowLayout* flow = (struct DjuiFlowLayout*)body;
    if (!panel->scrollable || body->on_child_render == NULL || flow->flowDirection != DJUI_FLOW_DIR_DOWN ||
        flow->margin.type != DJUI_SVT_ABSOLUTE) return fallback;
    f32 height = flow->margin.value;
    for (struct DjuiBaseChild* child = body->child; child; child = child->next) {
        if (!child->base->visible) continue;
        if (child->base->height.type != DJUI_SVT_ABSOLUTE) return fallback;
        height += flow->margin.value + child->base->height.value;
    }
    return height;
}

  ////////////
 // events //
////////////

bool djui_three_panel_render(struct DjuiBase* base) {
    struct DjuiThreePanel* threePanel = (struct DjuiThreePanel*)base;

    struct DjuiBase* children[3] = { NULL };
    struct DjuiBaseChild* child = base->child;
    for (int i = 0; i < 3; i++) {
        if (child == NULL || child->base == NULL) { break; }
        children[i] = child->base;
        child = child->next;
    }

    struct DjuiBase* head = children[0];
    struct DjuiBase* body = children[1];
    struct DjuiBase* foot = children[2] == (struct DjuiBase*)threePanel->scrollControls ? NULL : children[2];

    if (body == NULL) { return false; }

    struct DjuiBaseRect* parentComp = &base->comp;
    f32 tPad = (base->padding.top.type == DJUI_SVT_RELATIVE)    ? parentComp->height * base->padding.top.value    : base->padding.top.value;
    f32 bPad = (base->padding.bottom.type == DJUI_SVT_RELATIVE) ? parentComp->height * base->padding.bottom.value : base->padding.bottom.value;

    f32 myHeight = base->comp.height - tPad - bPad;
    f32 minHeaderSize = (threePanel->minHeaderSize.type == DJUI_SVT_RELATIVE) ? base->comp.height * threePanel->minHeaderSize.value : threePanel->minHeaderSize.value;
    f32 minFooterSize = (threePanel->minFooterSize.type == DJUI_SVT_RELATIVE) ? base->comp.height * threePanel->minFooterSize.value : threePanel->minFooterSize.value;
    f32 contentHeight = (threePanel->bodySize.type == DJUI_SVT_RELATIVE) ?
        base->comp.height * threePanel->bodySize.value : threePanel->bodySize.value;
    contentHeight = djui_three_panel_content_height(threePanel, body, contentHeight);
    struct DjuiPanelLayout layout = djui_panel_layout(myHeight, minHeaderSize, contentHeight,
                                                    minFooterSize, threePanel->scrollable);
    f32 headerSize = layout.header;
    f32 bodySize = layout.body;
    f32 footerSize = layout.footer;
    threePanel->contentHeight = contentHeight;
    threePanel->scrollMax = layout.scrollMax;
    threePanel->scrollOffset = djui_panel_scroll_clamp(threePanel->scrollOffset, layout.scrollMax);
    if (threePanel->scrollable) body->render = djui_three_panel_body_render;
    if (layout.controls > 0 && !threePanel->scrollControls) djui_three_panel_scroll_controls(threePanel);
    if (threePanel->scrollControls) {
        struct DjuiBase* controls = &threePanel->scrollControls->base;
        djui_base_set_visible(controls, layout.controls > 0);
        djui_base_set_location(controls, 0, headerSize + bodySize + footerSize - layout.controls);
        djui_base_set_size(controls, 1, layout.controls);
        djui_base_set_enabled(&threePanel->scrollUp->base, threePanel->scrollOffset > 0);
        djui_base_set_enabled(&threePanel->scrollDown->base, threePanel->scrollOffset < layout.scrollMax);
    }

    if (head != NULL) {
        //djui_base_set_location_type(head, DJUI_SVT_ABSOLUTE, DJUI_SVT_ABSOLUTE);
        //djui_base_set_location(head, 0, 0);
        djui_base_set_size_type(head, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
        djui_base_set_size(head, 1.0f, headerSize);
        djui_base_set_alignment(head, DJUI_HALIGN_LEFT, DJUI_VALIGN_TOP);
    }

    if (body != NULL) {
        djui_base_set_location_type(body, DJUI_SVT_ABSOLUTE, DJUI_SVT_ABSOLUTE);
        djui_base_set_location(body, 0, headerSize);
        djui_base_set_size_type(body, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
        djui_base_set_size(body, 1.0f, bodySize);
        djui_base_set_alignment(body, DJUI_HALIGN_LEFT, DJUI_VALIGN_TOP);
    }

    if (foot != NULL) {
        djui_base_set_location_type(foot, DJUI_SVT_ABSOLUTE, DJUI_SVT_ABSOLUTE);
        djui_base_set_location(foot, 0, headerSize + bodySize);
        djui_base_set_size_type(foot, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
        djui_base_set_size(foot, 1.0f, fmax(0, footerSize - layout.controls));
        djui_base_set_alignment(foot, DJUI_HALIGN_LEFT, DJUI_VALIGN_TOP);
    }

    djui_rect_render(base);
    return true;
}

static void djui_three_panel_destroy(struct DjuiBase* base) {
    struct DjuiThreePanel* threePanel = (struct DjuiThreePanel*)base;
    free(threePanel);
}

struct DjuiThreePanel* djui_three_panel_create(struct DjuiBase* parent, f32 minHeaderSize, f32 bodySize, f32 minFooterSize) {
    struct DjuiThreePanel* threePanel = calloc(1, sizeof(struct DjuiThreePanel));
    struct DjuiBase* base = &threePanel->base;

    djui_base_init(parent, base, djui_three_panel_render, djui_three_panel_destroy);
    djui_three_panel_set_min_header_size(threePanel, minHeaderSize);
    djui_three_panel_set_body_size(threePanel, bodySize);
    djui_three_panel_set_min_footer_size(threePanel, minFooterSize);

    return threePanel;
}
