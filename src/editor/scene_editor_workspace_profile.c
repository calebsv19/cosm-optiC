#include "editor/scene_editor_workspace_profile.h"
#include "editor/scene_editor_workspace_layout.h"
#include "editor/scene_editor_sidebar.h"
#include "editor/object_editor_selection_tracker.h"
#include "editor/scene_editor_chrome_shell.h"
static SceneEditorWorkspaceProfile active;
SceneEditorWorkspaceProfile SceneEditorWorkspaceProfileGet(void) { return active; }
void SceneEditorWorkspaceProfileReset(void) { active = SCENE_WORKSPACE_SCENE; }
const char* SceneEditorWorkspaceProfileLabel(int profile) {
    static const char* labels[] = {"Scene", "Materials", "Surface", "Atmos / Water", "Render"};
    return profile >= 0 && profile < SCENE_WORKSPACE_PROFILE_COUNT ? labels[profile] : "Scene";
}
void SceneEditorWorkspaceProfileSelect(SceneEditor* editor, SceneEditorWorkspaceProfile profile) {
    if (!editor || profile < 0 || profile >= SCENE_WORKSPACE_PROFILE_COUNT) return;
    int selected = ObjectEditorGetSelectedObjectIndex();
    active = profile;
    SetSceneMode(editor, profile == SCENE_WORKSPACE_MATERIALS ? EDITOR_MODE_MATERIAL :
        profile == SCENE_WORKSPACE_RENDER ? EDITOR_MODE_CAMERA : EDITOR_MODE_OBJECT);
    ObjectEditorSetSelectedObjectIndex(selected);
    SceneEditorSidebarReset();
}
bool SceneEditorWorkspaceProfileHandleEvent(SceneEditor* editor, const SDL_Event* event) {
    SceneEditorPaneLayout layout; SceneEditorWorkspaceChrome chrome;
    if (event->type != SDL_MOUSEBUTTONDOWN || event->button.button != SDL_BUTTON_LEFT ||
        !SceneEditorGetPaneLayout(&layout)) return false;
    SceneEditorWorkspaceLayoutChrome(&layout, &chrome);
    for (int i=0;i<SCENE_WORKSPACE_PROFILE_COUNT;++i) {
        SDL_Rect r = chrome.modes[i];
        if (event->button.x >= r.x && event->button.x < r.x+r.w &&
            event->button.y >= r.y && event->button.y < r.y+r.h) {
            SceneEditorWorkspaceProfileSelect(editor, i); return true;
        }
    }
    return false;
}

void SceneEditorWorkspaceProfileSyncMode(int mode) {
    if (mode == EDITOR_MODE_MATERIAL) active = SCENE_WORKSPACE_MATERIALS;
    else if (mode == EDITOR_MODE_CAMERA) active = SCENE_WORKSPACE_RENDER;
    else if (mode == EDITOR_MODE_PATH) active = SCENE_WORKSPACE_SCENE;
    else if (active == SCENE_WORKSPACE_MATERIALS || active == SCENE_WORKSPACE_RENDER) active = SCENE_WORKSPACE_SCENE;
}
