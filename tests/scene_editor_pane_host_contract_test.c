#include "editor/scene_editor_pane_host.h"

#include <assert.h>

static void test_pane_host_solves_left_center_right_shell(void) {
    SceneEditorPaneHost host = {0};
    const SceneEditorPaneLayout* layout = NULL;

    assert(scene_editor_pane_host_init(&host, 1280, 760));
    layout = scene_editor_pane_host_layout(&host);
    assert(layout != NULL);

    assert(layout->left_pane_rect.w >= 220);
    assert(layout->right_pane_rect.w >= 240);
    assert(layout->center_pane_rect.w >= 360);
    assert(layout->center_pane_rect.w > layout->left_pane_rect.w);
    assert(layout->center_pane_rect.w > layout->right_pane_rect.w);
    assert(layout->left_pane_rect.h == 760);
    assert(layout->center_pane_rect.h == 760);
    assert(layout->right_pane_rect.h == 760);
}

static void test_pane_host_rebuild_respects_targets_and_minima(void) {
    SceneEditorPaneHost host = {0};
    const SceneEditorPaneLayout* layout = NULL;

    assert(scene_editor_pane_host_init(&host, 1080, 680));
    scene_editor_pane_host_set_targets(&host, 320, 300);
    assert(scene_editor_pane_host_rebuild(&host, 1080, 680));
    layout = scene_editor_pane_host_layout(&host);
    assert(layout != NULL);

    assert(layout->left_pane_rect.w >= 220);
    assert(layout->right_pane_rect.w >= 240);
    assert(layout->center_pane_rect.w >= 360);
}

static void test_pane_host_splitter_drag_updates_shell_widths(void) {
    SceneEditorPaneHost host = {0};
    SceneEditorPaneLayout layout_before = {0};
    SceneEditorPaneLayout layout_after = {0};
    CorePaneRect splitter_rect = {0};
    bool hovered = false;
    bool active = false;
    float pointer_x = 0.0f;
    float pointer_y = 0.0f;

    assert(scene_editor_pane_host_init(&host, 1280, 760));
    assert(scene_editor_pane_host_layout(&host) != NULL);
    layout_before = *scene_editor_pane_host_layout(&host);

    pointer_x = (float)(layout_before.left_pane_rect.x + layout_before.left_pane_rect.w);
    pointer_y = (float)(layout_before.left_pane_rect.y + layout_before.left_pane_rect.h / 2);
    scene_editor_pane_host_update_pointer(&host, pointer_x, pointer_y);
    assert(scene_editor_pane_host_visible_splitter(&host, &splitter_rect, &hovered, &active));
    assert(hovered);
    assert(!active);
    assert(scene_editor_pane_host_begin_splitter_drag(&host, pointer_x, pointer_y));
    assert(scene_editor_pane_host_update_splitter_drag(&host, pointer_x + 48.0f, pointer_y));
    assert(scene_editor_pane_host_visible_splitter(&host, &splitter_rect, &hovered, &active));
    assert(hovered);
    assert(active);
    scene_editor_pane_host_end_splitter_drag(&host);

    assert(scene_editor_pane_host_layout(&host) != NULL);
    layout_after = *scene_editor_pane_host_layout(&host);
    assert(layout_after.left_pane_rect.w > layout_before.left_pane_rect.w);
    assert(layout_after.center_pane_rect.w < layout_before.center_pane_rect.w);
}

int main(void) {
    test_pane_host_solves_left_center_right_shell();
    test_pane_host_rebuild_respects_targets_and_minima();
    test_pane_host_splitter_drag_updates_shell_widths();
    return 0;
}
