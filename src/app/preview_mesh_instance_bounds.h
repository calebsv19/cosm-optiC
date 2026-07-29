#ifndef PREVIEW_MESH_INSTANCE_BOUNDS_H
#define PREVIEW_MESH_INSTANCE_BOUNDS_H

#include <stdbool.h>

#include "core_mesh_asset.h"
#include "import/runtime_mesh_asset_loader.h"

typedef struct PreviewMeshInstancePoint3 {
    double x;
    double y;
    double z;
} PreviewMeshInstancePoint3;

bool PreviewMeshInstanceTransformPoint(
    CoreObjectVec3 local,
    const CoreMeshAssetBounds3* local_bounds,
    const RayTracingRuntimeMeshAssetInstance* instance,
    PreviewMeshInstancePoint3* out_point);

bool PreviewMeshInstanceBuildBoundsCorners(
    const CoreMeshAssetBounds3* local_bounds,
    const RayTracingRuntimeMeshAssetInstance* instance,
    PreviewMeshInstancePoint3 out_corners[8]);

#endif
