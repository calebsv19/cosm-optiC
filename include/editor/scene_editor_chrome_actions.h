#ifndef SCENE_EDITOR_CHROME_ACTIONS_H
#define SCENE_EDITOR_CHROME_ACTIONS_H

#include <stdbool.h>

#include <SDL2/SDL.h>

#include "editor/scene_editor.h"
#include "editor/scene_editor_digest_overlay.h"
#include "editor/scene_editor_input_router.h"

typedef struct SceneEditorChromeActionsEnvironment {
    const SceneEditorPaneLayout* pane_layout;
    bool pane_layout_valid;
    SceneEditorDigestOverlayNavState* viewport_nav_state;
    int* digest_hover_object_index;
    SceneEditorBezier3DGizmoState* bezier_gizmo_state;
    SceneEditorCamera3DGizmoState* camera_gizmo_state;
    bool (*handle_viewport_navigation)(SceneEditor* editor,
                                       const SceneEditorPaneCommand* command,
                                       SceneEditorInputRoutingResult* result);
    void (*initialize_editor_mode)(SceneEditor* editor);
    void (*resume_after_preview)(SceneEditor* editor);
} SceneEditorChromeActionsEnvironment;

bool SceneEditorChromeActionsResolve(const SDL_Event* event, SceneEditorChromeAction* out_action);
void SceneEditorChromeActionsApply(SceneEditor* editor,
                                   const SceneEditorChromeAction* action,
                                   const SceneEditorChromeActionsEnvironment* env);
void SceneEditorChromeActionsRoutePaneEvent(SceneEditor* editor,
                                            const SceneEditorChromeActionsEnvironment* env,
                                            const SceneEditorPaneCommand* command,
                                            SceneEditorInputRoutingResult* result);

#endif
