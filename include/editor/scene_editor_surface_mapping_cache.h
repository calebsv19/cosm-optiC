#pragma once
#include "render/runtime_surface_mapping.h"
#include "editor/scene_editor_viewport_material.h"
typedef struct SceneEditorSurfaceMappingCache SceneEditorSurfaceMappingCache;
const SceneEditorSurfaceMappingCache* SceneEditorSurfaceMappingCachePrepare(int index);
/* Rest and world positions are interpolated before mapping. Derivatives are
 * corresponding neighboring pixel positions, not wrapped vertex UVs. */
bool SceneEditorSurfaceMappingCacheSample(const SceneEditorSurfaceMappingCache* cache,
    Vec3 world,Vec3 rest,Vec3 world_dx,Vec3 rest_dx,Vec3 world_dy,Vec3 rest_dy,
    RuntimeMaterialSurfaceEval* out);
unsigned long long SceneEditorSurfaceMappingCacheBuildCount(void);
