#include "ui/menu_pane_host.h"

#include <assert.h>

static int rect_right(SDL_Rect rect) {
    return rect.x + rect.w;
}

static void test_menu_pane_host_solves_stable_three_leaf_shell(void) {
    RayTracingMenuPaneHost host;
    const RayTracingMenuPaneLayout* layout;

    assert(ray_tracing_menu_pane_host_init(&host, (SDL_Rect){30, 30, 1140, 758}));
    layout = ray_tracing_menu_pane_host_layout(&host);
    assert(layout != NULL);
    assert(layout->scene_rect.w >= 340);
    assert(layout->workspace_rect.w >= 280);
    assert(layout->health_rect.w >= 240);
    assert(layout->scene_rect.x == 30);
    assert(layout->scene_rect.y == 30);
    assert(layout->scene_rect.h == 758);
    assert(rect_right(layout->scene_rect) < layout->workspace_rect.x);
    assert(rect_right(layout->workspace_rect) < layout->health_rect.x);
}

static void test_menu_pane_host_rebuild_preserves_targets_and_minima(void) {
    RayTracingMenuPaneHost host;
    const RayTracingMenuPaneLayout* layout;

    assert(ray_tracing_menu_pane_host_init(&host, (SDL_Rect){30, 30, 1500, 900}));
    ray_tracing_menu_pane_host_set_targets(&host, 450, 420);
    assert(ray_tracing_menu_pane_host_rebuild(&host, (SDL_Rect){30, 30, 1020, 650}));
    layout = ray_tracing_menu_pane_host_layout(&host);
    assert(layout != NULL);
    assert(layout->scene_rect.w >= 340);
    /* The host trims half of each 18 px inter-pane gap from visible leaves. */
    assert(layout->workspace_rect.w >= 280);
    assert(layout->health_rect.w >= 240);
}

static void test_menu_pane_host_splitter_drag_updates_widths(void) {
    RayTracingMenuPaneHost host;
    RayTracingMenuPaneLayout before;
    const RayTracingMenuPaneLayout* after;
    RayTracingMenuPaneSplitterVisual visuals[RAY_TRACING_MENU_PANE_SPLITTER_CAP];
    uint32_t visual_count;
    float pointer_x;
    float pointer_y;

    assert(ray_tracing_menu_pane_host_init(&host, (SDL_Rect){30, 30, 1500, 800}));
    before = *ray_tracing_menu_pane_host_layout(&host);
    visual_count = ray_tracing_menu_pane_host_splitter_visuals(
        &host,
        visuals,
        RAY_TRACING_MENU_PANE_SPLITTER_CAP);
    assert(visual_count == 2u);

    pointer_x = (float)(before.scene_rect.x + before.scene_rect.w + 9);
    pointer_y = (float)(before.scene_rect.y + before.scene_rect.h / 2);
    ray_tracing_menu_pane_host_update_pointer(&host, pointer_x, pointer_y);
    assert(ray_tracing_menu_pane_host_begin_splitter_drag(&host, pointer_x, pointer_y));
    assert(ray_tracing_menu_pane_host_update_splitter_drag(&host,
                                                           pointer_x + 40.0f,
                                                           pointer_y));
    ray_tracing_menu_pane_host_end_splitter_drag(&host);
    after = ray_tracing_menu_pane_host_layout(&host);
    assert(after != NULL);
    assert(after->scene_rect.w > before.scene_rect.w);
    assert(after->workspace_rect.w < before.workspace_rect.w);
}

int main(void) {
    test_menu_pane_host_solves_stable_three_leaf_shell();
    test_menu_pane_host_rebuild_preserves_targets_and_minima();
    test_menu_pane_host_splitter_drag_updates_widths();
    return 0;
}
