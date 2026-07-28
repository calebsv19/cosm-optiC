#ifndef RUNTIME_MESH_ASSET_LOADER_AUTHORED_MATERIAL_H
#define RUNTIME_MESH_ASSET_LOADER_AUTHORED_MATERIAL_H

#include "import/runtime_mesh_asset_loader.h"

#include <json-c/json.h>

bool runtime_mesh_asset_load_procedural_solid_authored_material_ref(
    const char *runtime_scene_path,
    json_object *object,
    RayTracingRuntimeMeshAsset *asset,
    char *out_diagnostics,
    size_t out_diagnostics_size);

bool runtime_mesh_asset_procedural_solid_authored_dependencies_match(
    const RayTracingRuntimeMeshAsset *asset);

const ProceduralSolidAuthoredMaterialV1 *
runtime_mesh_asset_resolve_procedural_solid_authored_material(
    const RayTracingRuntimeMeshAsset *asset,
    const char *region_id);

#endif
