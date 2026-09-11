#include "editor/scene_editor_pane_host.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "editor/scene_editor_workspace_layout.h"

#undef assert
#define assert(condition) do { if (!(condition)) { \
    fprintf(stderr,"Pane contract failed at %d: %s\n",__LINE__,#condition); \
    exit(EXIT_FAILURE); } } while (0)

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
    assert(layout->left_pane_rect.h == 760 - host.workspace_header_height);
    assert(layout->center_pane_rect.h == 760 - host.workspace_header_height);
    assert(layout->right_pane_rect.h == 760 - host.workspace_header_height);
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

static void test_pane_host_timeline_is_collapsed_then_resizable(void) {
    SceneEditorPaneHost host = {0};
    SceneEditorPaneLayout before = {0};
    SceneEditorPaneLayout opened = {0};
    SceneEditorPaneLayout resized = {0};
    float pointer_x = 0.0f;
    float pointer_y = 0.0f;
    assert(scene_editor_pane_host_init(&host, 1280, 760));
    before = *scene_editor_pane_host_layout(&host);
    assert(!before.timeline_visible);
    assert(before.timeline_rect.h == 0);
    assert(scene_editor_pane_host_set_timeline_visible(&host, true));
    opened = *scene_editor_pane_host_layout(&host);
    assert(opened.timeline_visible);
    assert(opened.timeline_rect.h >= 200);
    assert(opened.viewport_rect.h < before.viewport_rect.h);
    pointer_x = (float)(opened.timeline_rect.x + opened.timeline_rect.w / 2);
    pointer_y = (float)(opened.timeline_rect.y - 10);
    assert(scene_editor_pane_host_begin_splitter_drag(&host, pointer_x, pointer_y));
    assert(scene_editor_pane_host_update_splitter_drag(&host, pointer_x, pointer_y - 40.0f));
    scene_editor_pane_host_end_splitter_drag(&host);
    resized = *scene_editor_pane_host_layout(&host);
    assert(resized.timeline_rect.h > opened.timeline_rect.h);
    assert(scene_editor_pane_host_set_viewport_expanded(&host, true));
    assert(!scene_editor_pane_host_layout(&host)->timeline_visible);
    assert(host.timeline_visible);
    assert(scene_editor_pane_host_set_viewport_expanded(&host, false));
    assert(scene_editor_pane_host_layout(&host)->timeline_visible);
    assert(scene_editor_pane_host_layout(&host)->timeline_rect.h == resized.timeline_rect.h);
    assert(scene_editor_pane_host_set_timeline_visible(&host, false));
    assert(!scene_editor_pane_host_layout(&host)->timeline_visible);
    assert(scene_editor_pane_host_layout(&host)->viewport_rect.h == before.viewport_rect.h);
}


static void test_workspace_expand_restore_and_chrome_reachability(void) {
    const int sizes[][2] = {{1024, 640}, {1280, 800}, {1440, 900}, {2560, 1600}};
    for (unsigned i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        SceneEditorPaneHost host = {0};
        SceneEditorWorkspaceChrome chrome;
        assert(scene_editor_pane_host_init(&host, sizes[i][0], sizes[i][1]));
        scene_editor_pane_host_set_targets(&host, 280, 340);
        assert(scene_editor_pane_host_rebuild(&host, sizes[i][0], sizes[i][1]));
        SceneEditorPaneLayout before = host.layout;
        SceneEditorWorkspaceLayoutChrome(&host.layout, &chrome);
        for (int j = 0; j < SCENE_WORKSPACE_ACTION_COUNT; ++j) {
            SDL_Rect r = chrome.actions[j];
            assert(r.w >= 44 && r.h >= 24);
            assert(r.x >= 0 && r.x + r.w <= sizes[i][0]);
            assert(r.y + r.h < before.viewport_rect.y);
            if (j) assert(r.x > chrome.actions[j-1].x + chrome.actions[j-1].w);
        }
        assert(chrome.restore.x + chrome.restore.w <= sizes[i][0]);
        SDL_Rect document_controls[]={chrome.workspace,chrome.frame_all,chrome.frame_selected,chrome.undo,chrome.redo};
        for (int j=0;j<5;++j) {
            SDL_Rect r=document_controls[j];
            assert(r.w>=44 && r.h>=24);
            assert(r.x+r.w<=chrome.expand.x);
            if (j) assert(r.x>document_controls[j-1].x+document_controls[j-1].w);
        }
        for (int j=0;j<SCENE_WORKSPACE_MODE_COUNT;++j) {
            assert(chrome.modes[j].x>=0 && chrome.modes[j].x+chrome.modes[j].w<=sizes[i][0]);
            assert(chrome.modes[j].y+chrome.modes[j].h<=sizes[i][1]);
            if (j) assert(chrome.modes[j].y>=chrome.modes[j-1].y+chrome.modes[j-1].h);
        }
        assert(scene_editor_pane_host_set_viewport_expanded(&host, true));
        assert(host.layout.left_pane_rect.w == 0 && host.layout.right_pane_rect.w == 0);
        assert(host.layout.viewport_rect.w > before.viewport_rect.w);
        assert(!scene_editor_pane_host_begin_splitter_drag(&host, 300, 20));
        assert(scene_editor_pane_host_set_viewport_expanded(&host, false));
        assert(host.layout.left_pane_rect.w == before.left_pane_rect.w);
        assert(host.layout.right_pane_rect.w == before.right_pane_rect.w);
        assert(scene_editor_pane_host_restore_workspace(&host));
        assert(!host.viewport_expanded && !host.timeline_visible);
        host.workspace_header_height = 160;
        assert(scene_editor_pane_host_rebuild(&host, sizes[i][0], sizes[i][1]));
        assert(host.layout.viewport_rect.y >= 160);
        assert(host.layout.workspace_feedback_rect.y + host.layout.workspace_feedback_rect.h <= 160);
    }
}

int main(void) {
    test_workspace_expand_restore_and_chrome_reachability();
    test_pane_host_solves_left_center_right_shell();
    test_pane_host_rebuild_respects_targets_and_minima();
    test_pane_host_splitter_drag_updates_shell_widths();
    test_pane_host_timeline_is_collapsed_then_resizable();
    return 0;
}
