#ifndef SCENE_EDITOR_OBJECT_TRANSFORM_PREVIEW_H
#define SCENE_EDITOR_OBJECT_TRANSFORM_PREVIEW_H
#include "editor/scene_editor_object_move_gizmo.h"
#include "import/runtime_mesh_asset_loader.h"
void SceneEditorObjectTransformHandleOrigin(int object_index, SceneEditorObjectTransformMode mode,
    const double fallback[3], double position[3]);
/* Copies only: the retained runtime and asset store never receive gesture state. */
bool SceneEditorObjectTransformPreviewMesh(const RayTracingRuntimeMeshAssetInstance* source,
                                          RayTracingRuntimeMeshAssetInstance* display);
bool SceneEditorObjectTransformPreviewPrimitive(const RuntimeSceneBridgePrimitiveSeed* source,
                                               RuntimeSceneBridgePrimitiveSeed* display);
#endif
