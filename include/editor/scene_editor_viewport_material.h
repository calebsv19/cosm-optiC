#pragma once
#include "render/runtime_material_texture_stack_3d.h"
#include "editor/scene_editor_mesh_preview_shading.h"
#include "editor/scene_editor_digest_overlay.h"
/* Opaque, bounded cache of authored material samples, independent of camera. */
typedef struct SceneEditorViewportMaterial SceneEditorViewportMaterial;
const SceneEditorViewportMaterial* SceneEditorViewportMaterialPrepare(int object_index);
const SceneEditorViewportMaterial* SceneEditorViewportMaterialPrepareFace(int object_index, int face);
bool SceneEditorViewportMaterialSample(const SceneEditorViewportMaterial* material,
    double u,double v,RuntimeMaterialSurfaceEval* out);
SDL_Color SceneEditorViewportMaterialShade(const SceneEditorViewportMaterial* material,
    SceneEditorMeshPreviewShadeNormal normal, SceneEditorMeshPreviewShadeNormal view,
    double u, double v);
void SceneEditorViewportMaterialReset(void);
unsigned long long SceneEditorViewportMaterialBuildCount(void);
