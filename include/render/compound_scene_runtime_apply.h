#pragma once

#include <stdbool.h>

#include "import/compound_scene_ingestion.h"
#include "import/runtime_mesh_asset_loader.h"
#include "render/runtime_scene_3d.h"

/* Applies an already validated I-1 result to a freshly built, request-local
 * RuntimeScene3D.  The base scene is never changed.  Every bound mesh and
 * room plane must already exist, keeping source art and materials RayTracing-
 * owned. */
bool ray_compound_scene_runtime_apply_exact(
    RuntimeScene3D* scene,
    const RayTracingRuntimeMeshAssetSet* mesh_assets,
    const RayCompoundSceneIngestionDescriptor* descriptor,
    const RayCompoundSceneIngestionResult* result,
    char* diagnostics,
    size_t diagnostics_size);
