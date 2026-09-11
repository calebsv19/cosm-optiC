#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <SDL2/SDL_ttf.h>
#include "editor/scene_editor.h"
#include "editor/scene_editor_internal.h"
#include "editor/scene_editor_session_runtime.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_document.h"
#include "editor/object_editor_selection_tracker.h"
#include "vk_renderer.h"
#include "editor/scene_editor_viewport_nav.h"

static void click(SceneEditor* editor, SDL_Rect rect) {
    SDL_Event event = {0};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.windowID = SDL_GetWindowID(editor->window);
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = rect.x + rect.w / 2;
    event.button.y = rect.y + rect.h / 2;
    SceneEditorSessionRuntimeHandleEvent(editor, &event);
    event.type = SDL_MOUSEBUTTONUP;
    SceneEditorSessionRuntimeHandleEvent(editor, &event);
    SceneEditorSessionRuntimeRender(editor);
}

static void key(SceneEditor* editor, SDL_Keycode code) {
    SDL_Event event = {0};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = code;
    SceneEditorSessionRuntimeHandleEvent(editor, &event);
}

static void capture(SceneEditor* editor, const char* path) {
    VkRenderer* renderer = (VkRenderer*)editor->renderer;
    assert(vk_renderer_request_capture(renderer, path) == VK_SUCCESS);
    for (int i = 0; i < 8 && !renderer->debug_capture.dumped; ++i) {
        SDL_PumpEvents();
        SceneEditorSessionRuntimeRender(editor);
    }
    assert(renderer->debug_capture.dumped);
}

int main(int argc, char** argv) {
    SceneEditor editor;
    SceneEditorPaneLayout before, after;
    assert(argc == 3); /* Task-owned working directory and copied runtime scene. */
    assert(chdir(argv[1]) == 0);
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
    assert(TTF_Init() == 0);
    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    animSettings.spaceMode = SPACE_MODE_3D;
    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath), "%s", argv[2]);
    assert(InitializeSceneEditor(&editor));
    SDL_SetWindowTitle(editor.window, "optiC E0/E1 isolated source proof");
    SDL_SetWindowSize(editor.window, 1280, 800);
    SDL_PumpEvents();
    SceneEditorSessionRuntimeRender(&editor);
    assert(SceneEditorDocumentIsOpen());
    ObjectEditorSetSelectedObjectIndex(0);
    SceneEditorSessionRuntimeRender(&editor);
    assert(SceneEditorGetPaneLayout(&before));
    SceneEditorViewportNavFitDigestOverlayForTarget(SceneEditorGetViewportNavState(),
        &before.viewport_rect, true, EDITOR_MODE_OBJECT, 0);
    unsigned long long revision = SceneEditorDocumentRevision();
    int selected = ObjectEditorGetSelectedObjectIndex();
    capture(&editor, "workspace_scene.ppm");
    click(&editor, expandViewportButton);
    assert(SceneEditorGetPaneLayout(&after) && after.viewport_expanded);
    assert(after.viewport_rect.w > before.viewport_rect.w);
    assert(SceneEditorDocumentRevision() == revision);
    assert(ObjectEditorGetSelectedObjectIndex() == selected);
    capture(&editor, "workspace_expanded.ppm");
    click(&editor, expandViewportButton);
    assert(SceneEditorGetPaneLayout(&after) && !after.viewport_expanded);
    assert(after.left_pane_rect.w == before.left_pane_rect.w);
    assert(after.right_pane_rect.w == before.right_pane_rect.w);
    click(&editor, restoreWorkspaceButton);
    assert(SceneEditorDocumentRevision() == revision);
    assert(!SceneEditorDocumentIsDirty());
    SDL_SetWindowSize(editor.window, 1024, 640);
    SDL_PumpEvents();
    SceneEditorSessionRuntimeRender(&editor);
    capture(&editor, "workspace_compact.ppm");
    /* Exercise the actual inspector and Save action against the copied fixture. */
    assert(SceneEditorGetPaneLayout(&after));
    SceneEditorDocumentTransform original, edited, reopened;
    char diagnostics[256];
    assert(SceneEditorDocumentGetTransformForSceneIndex(selected, &original,
        diagnostics, sizeof(diagnostics)));
    SDL_Rect position_x = {after.right_content_rect.x,
        after.right_content_rect.y + 25, (after.right_content_rect.w - 8) / 3, 25};
    click(&editor, position_x);
    for (int i = 0; i < 32; ++i) key(&editor, SDLK_BACKSPACE);
    SDL_Event text = {0};
    text.type = SDL_TEXTINPUT;
    snprintf(text.text.text, sizeof(text.text.text), "%.6g", original.position[0] + 0.25);
    SceneEditorSessionRuntimeHandleEvent(&editor, &text);
    key(&editor, SDLK_RETURN);
    assert(SceneEditorDocumentGetTransformForSceneIndex(selected, &edited,
        diagnostics, sizeof(diagnostics)));
    assert(fabs(edited.position[0] - original.position[0] - 0.25) < 1e-6);
    assert(SceneEditorDocumentIsDirty());
    SceneEditorSessionRuntimeRender(&editor);
    SDL_Rect undo = {after.right_content_rect.x,
        after.right_content_rect.y + 25 + 4 * 29,
        (after.right_content_rect.w - 4) / 2, 25};
    click(&editor, undo);
    assert(SceneEditorDocumentGetTransformForSceneIndex(selected, &reopened,
        diagnostics, sizeof(diagnostics)));
    assert(fabs(reopened.position[0] - original.position[0]) < 1e-6);
    SDL_Rect redo = undo;
    redo.x += undo.w + 4;
    click(&editor, redo);
    assert(SceneEditorDocumentGetTransformForSceneIndex(selected, &reopened,
        diagnostics, sizeof(diagnostics)));
    assert(fabs(reopened.position[0] - edited.position[0]) < 1e-6);
    click(&editor, saveButton);
    assert(!SceneEditorDocumentIsDirty());
    assert(SceneEditorDocumentOpen(argv[2], diagnostics, sizeof(diagnostics)));
    assert(SceneEditorDocumentGetTransformForSceneIndex(selected, &reopened,
        diagnostics, sizeof(diagnostics)));
    assert(fabs(reopened.position[0] - edited.position[0]) < 1e-6);
    capture(&editor, "workspace_saved_edit.ppm");
    DestroySceneEditor(&editor);
    TTF_Quit();
    SDL_Quit();
    puts("Workspace source UI: expand/restore/resize preserves document revision and selection; inspector edit/undo/redo/save/reopen passed");
    return 0;
}
