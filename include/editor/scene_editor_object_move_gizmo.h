#ifndef SCENE_EDITOR_OBJECT_MOVE_GIZMO_H
#define SCENE_EDITOR_OBJECT_MOVE_GIZMO_H

#include <stdbool.h>
#include <SDL2/SDL.h>
#include "editor/scene_editor_digest_overlay.h"

/* Scene-only object translation. A drag previews in the viewport and commits
 * through the retained document once on release, yielding one Undo step. */
bool SceneEditorObjectMoveGizmoHandleEvent(const SDL_Event* event, SDL_Window* window);
void SceneEditorObjectMoveGizmoRender(SDL_Renderer* renderer,
                                     const SceneEditorDigestOverlayProjector* projector,
                                     const RuntimeSceneBridge3DDigestState* digest,
                                     int selected_object_index);
void SceneEditorObjectMoveGizmoReset(void);
SceneEditorBezier3DGizmoAxis SceneEditorObjectMoveGizmoActiveAxis(void);
SceneEditorBezier3DGizmoAxis SceneEditorObjectMoveGizmoHoverAxis(void);
/* Read-only presentation offset; never mutates the document or runtime geometry. */
bool SceneEditorObjectMoveGizmoPreviewProjector(int object_index,
    const SceneEditorDigestOverlayProjector* source, SceneEditorDigestOverlayProjector* display);

#endif
