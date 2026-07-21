#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <SDL2/SDL.h>

#include "core_mesh_preview.h"

typedef struct SceneEditorMeshPreviewShadeNormal {
    double x;
    double y;
    double z;
} SceneEditorMeshPreviewShadeNormal;

double SceneEditorMeshPreviewShadeFactor(SceneEditorMeshPreviewShadeNormal normal);
SDL_Color SceneEditorMeshPreviewShadeColor(SDL_Color base,
                                           SceneEditorMeshPreviewShadeNormal normal);
bool SceneEditorMeshPreviewBuildSmoothNormals(
    const CoreMeshPreviewLodMesh* lod,
    CoreObjectVec3* out_normals,
    size_t normal_capacity);
