#ifndef SCENE_EDITOR_OBJECT_MOVE_GIZMO_H
#define SCENE_EDITOR_OBJECT_MOVE_GIZMO_H

#include <stdbool.h>
#include <SDL2/SDL.h>
#include "editor/scene_editor_digest_overlay.h"
#include "editor/scene_editor_document.h"

typedef enum SceneEditorObjectTransformMode {
    SCENE_EDITOR_OBJECT_TRANSFORM_MOVE,
    SCENE_EDITOR_OBJECT_TRANSFORM_ROTATE,
    SCENE_EDITOR_OBJECT_TRANSFORM_SCALE
} SceneEditorObjectTransformMode;
SceneEditorObjectTransformMode SceneEditorObjectTransformModeGet(void);
void SceneEditorObjectTransformModeSet(SceneEditorObjectTransformMode mode);
bool SceneEditorObjectTransformPreview(int object_index,
    SceneEditorDocumentTransform* original, SceneEditorDocumentTransform* preview);
/* Legacy entry-point names retained for the shared Move/Rotate/Scale transaction. */
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
