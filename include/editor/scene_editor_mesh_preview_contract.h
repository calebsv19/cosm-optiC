#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "core_mesh_asset.h"
#include "core_mesh_preview.h"

// RayTracing-owned editor meanings. Shared core supplies geometry only.
typedef enum SceneEditorMeshDisplayMode {
    SCENE_EDITOR_MESH_DISPLAY_BOUNDS = 0,
    SCENE_EDITOR_MESH_DISPLAY_WIRE = 1,
    SCENE_EDITOR_MESH_DISPLAY_SOLID = 2,
    SCENE_EDITOR_MESH_DISPLAY_MATERIAL = 3,
    SCENE_EDITOR_MESH_DISPLAY_COUNT = 4
} SceneEditorMeshDisplayMode;

typedef enum SceneEditorMeshPreviewInvalidation {
    SCENE_EDITOR_MESH_PREVIEW_INVALIDATION_NONE = 0,
    SCENE_EDITOR_MESH_PREVIEW_INVALIDATION_GEOMETRY = 1u << 0,
    SCENE_EDITOR_MESH_PREVIEW_INVALIDATION_VIEW_DIRECTION = 1u << 1,
    SCENE_EDITOR_MESH_PREVIEW_INVALIDATION_PROJECTION = 1u << 2,
    SCENE_EDITOR_MESH_PREVIEW_INVALIDATION_APPEARANCE = 1u << 3,
    SCENE_EDITOR_MESH_PREVIEW_INVALIDATION_HOVER = 1u << 4,
    SCENE_EDITOR_MESH_PREVIEW_INVALIDATION_SELECTION = 1u << 5
} SceneEditorMeshPreviewInvalidation;

static inline SceneEditorMeshDisplayMode SceneEditorMeshDisplayModeClamp(int mode) {
    return (mode >= 0 && mode < SCENE_EDITOR_MESH_DISPLAY_COUNT)
               ? (SceneEditorMeshDisplayMode)mode
               : SCENE_EDITOR_MESH_DISPLAY_BOUNDS;
}

static inline const char* SceneEditorMeshDisplayModeName(SceneEditorMeshDisplayMode mode) {
    switch (SceneEditorMeshDisplayModeClamp((int)mode)) {
        case SCENE_EDITOR_MESH_DISPLAY_WIRE: return "Wire";
        case SCENE_EDITOR_MESH_DISPLAY_SOLID: return "Solid";
        case SCENE_EDITOR_MESH_DISPLAY_MATERIAL: return "Material";
        case SCENE_EDITOR_MESH_DISPLAY_BOUNDS:
        default: return "Bounds";
    }
}

static inline SceneEditorMeshDisplayMode SceneEditorMeshDisplayModeCycle(
    SceneEditorMeshDisplayMode mode,
    int direction) {
    int next = (int)SceneEditorMeshDisplayModeClamp((int)mode);
    next = (next + (direction < 0 ? -1 : 1) + SCENE_EDITOR_MESH_DISPLAY_COUNT) %
           SCENE_EDITOR_MESH_DISPLAY_COUNT;
    return (SceneEditorMeshDisplayMode)next;
}

static inline bool SceneEditorMeshDisplayModeDrawsStructuralWire(
    SceneEditorMeshDisplayMode mode) {
    const SceneEditorMeshDisplayMode clamped = SceneEditorMeshDisplayModeClamp((int)mode);
    return clamped == SCENE_EDITOR_MESH_DISPLAY_WIRE ||
           clamped == SCENE_EDITOR_MESH_DISPLAY_MATERIAL;
}

static inline bool SceneEditorMeshPreviewInvalidationResetsQuality(unsigned invalidation) {
    return (invalidation & (SCENE_EDITOR_MESH_PREVIEW_INVALIDATION_GEOMETRY |
                            SCENE_EDITOR_MESH_PREVIEW_INVALIDATION_VIEW_DIRECTION)) != 0u;
}

static inline bool SceneEditorMeshPreviewBuildLod(
    const CoreMeshAssetRuntimeDocument* document,
    size_t target_triangles,
    CoreMeshPreviewLodMesh* out_lod) {
    return core_mesh_preview_build_lod_mesh(document, target_triangles, out_lod).code == CORE_OK;
}

static inline void SceneEditorMeshPreviewFreeLod(CoreMeshPreviewLodMesh* lod) {
    core_mesh_preview_lod_mesh_free(lod);
}
