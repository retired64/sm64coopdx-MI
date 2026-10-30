#ifndef DJUI_PANEL_LAYOUT_H
#define DJUI_PANEL_LAYOUT_H

/* Pure geometry shared by native DJUI and standalone viewport regression tests. */
struct DjuiPanelLayout {
    float header, body, footer, controls, scrollMax;
};

static inline float djui_panel_scroll_clamp(float offset, float maximum) {
    if (offset < 0) return 0;
    return offset > maximum ? maximum : offset;
}

static inline struct DjuiPanelLayout djui_panel_layout(float height, float header, float content,
                                                     float footer, int scrollable) {
    struct DjuiPanelLayout layout = { 0 };
    if (height <= 0) return layout;
    header = header > 0 ? header : 0;
    footer = footer > 0 ? footer : 0;
    content = content > 0 ? content : 0;
    float controls = scrollable && header + content + footer > height ? 44 : 0;
    if (controls > height / 3) controls = height / 3;
    layout.controls = controls;
    footer += controls;
    float largest = header > footer ? header : footer;
    layout.header = header;
    layout.body = content;
    layout.footer = footer;
    if (header + footer >= height) {
        layout.header = height * header / (header + footer);
        layout.footer = height - layout.header;
        layout.controls = 0; /* No useful body viewport at this size. */
        layout.body = 0;
    } else if (header + content + footer >= height) {
        layout.body = height - header - footer;
    } else if (largest * 2 + content >= height) {
        if (header > footer) layout.footer = height - header - content;
        else layout.header = height - footer - content;
    } else {
        layout.header = layout.footer = (height - content) / 2;
    }
    layout.scrollMax = scrollable && layout.body > 0 && content > layout.body ? content - layout.body : 0;
    return layout;
}
#endif
