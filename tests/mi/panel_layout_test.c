#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "pc/djui/djui_panel_layout.h"
#include "pc/mod_integrations/mi_catalog_layout.h"

int main(void) {
    /* Short menus retain their original centered geometry, no scroll controls. */
    struct DjuiPanelLayout shortMenu = djui_panel_layout(700, 64, 200, 0, 1);
    assert(shortMenu.controls == 0 && shortMenu.scrollMax == 0);
    assert(shortMenu.header == 250 && shortMenu.footer == 250);
    /* Menu scales 0.5, 0.75, 1, 1.5 on typical landscape heights. */
    const float scales[] = {0.5f, 0.75f, 1, 1.5f};
    const float heights[] = {480, 720, 1080};
    for (unsigned h = 0; h < sizeof(heights)/sizeof(heights[0]); ++h) {
        for (unsigned s = 0; s < sizeof(scales)/sizeof(scales[0]); ++s) {
            float rootHeight = heights[h] / scales[s];
            const float widths[] = {640, 1280, 2048};
            for (unsigned w = 0; w < 3; ++w) {
                struct MiCatalogLayout grid = mi_catalog_layout(widths[w] / scales[s], rootHeight);
                assert(grid.columns >= 1 && grid.columns <= 2);
                assert(grid.rows >= 1 && grid.rows <= 2);
                assert(grid.pageSize == grid.columns * grid.rows && grid.pageSize <= 4);
                assert(grid.cardHeight >= 220 && grid.cardHeight <= 300);
                float content = grid.rows * grid.cardHeight + (grid.rows - 1) * 12;
                struct DjuiPanelLayout catalog = djui_panel_layout(rootHeight - 32, 156, content, 72, 1);
                assert(catalog.body > 0 && catalog.footer >= 72);
                if (content > catalog.body) assert(catalog.scrollMax > 0 && catalog.controls > 0);
                if (grid.rows == 2) assert(content <= rootHeight - 32 - 156 - 72);
            }
            float viewport = heights[h] / scales[s] * 0.921f - 32;
            struct DjuiPanelLayout layout = djui_panel_layout(viewport, 64, 700, 0, 1);
            assert(layout.header >= 0 && layout.body > 0 && layout.footer >= 0);
            assert(fabsf(layout.header + layout.body + layout.footer - viewport) < 0.01f);
            if (layout.scrollMax > 0) {
                assert(layout.controls > 0 && layout.controls <= layout.footer);
                assert(fabsf(layout.body + layout.scrollMax - 700) < 0.01f);
                /* The last action can be entirely exposed without changing scale. */
                float offset = djui_panel_scroll_clamp(100000, layout.scrollMax);
                assert(fabsf(700 - offset - layout.body) < 0.01f);
            }
        }
    }
    struct MiCatalogLayout wide = mi_catalog_layout(1280, 720);
    assert(wide.columns == 2 && wide.rows == 2 && wide.pageSize == 4);
    struct MiCatalogLayout narrow = mi_catalog_layout(640, 720);
    assert(narrow.columns == 1 && narrow.rows == 2 && narrow.pageSize == 2);
    struct DjuiPanelLayout fixed = djui_panel_layout(300, 64, 700, 0, 0);
    assert(fixed.scrollMax == 0 && fixed.controls == 0); /* custom panels opt out */
    struct DjuiPanelLayout tiny = djui_panel_layout(20, 64, 700, 64, 1);
    assert(tiny.body == 0 && tiny.controls == 0);
    assert(djui_panel_scroll_clamp(-5, 200) == 0);
    assert(djui_panel_scroll_clamp(80, 200) == 80);
    assert(djui_panel_scroll_clamp(500, 200) == 200);
    puts("DJUI viewport: scale/height matrix, overflow reachability and custom panels passed.");
}
