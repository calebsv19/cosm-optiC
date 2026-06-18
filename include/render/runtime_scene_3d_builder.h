#ifndef RENDER_RUNTIME_SCENE_3D_BUILDER_H
#define RENDER_RUNTIME_SCENE_3D_BUILDER_H

#include <stdbool.h>
#include <stdint.h>

#include "import/runtime_mesh_asset_loader.h"
#include "import/runtime_scene_bridge.h"
#include "render/runtime_scene_3d.h"

typedef struct RuntimeScene3DHeightfieldSurfaceDesc {
    const char* object_id;
    int scene_object_index;
    uint32_t grid_w;
    uint32_t grid_d;
    const float* heights_y;
    double sample_origin_x;
    double sample_origin_z;
    double sample_spacing_x;
    double sample_spacing_z;
    double dry_height;
    double dry_height_epsilon;
    bool skip_dry_quads;
    bool two_sided;
    bool map_y_height_to_scene_z;
} RuntimeScene3DHeightfieldSurfaceDesc;

bool RuntimeScene3DBuilder_BuildFromPrimitiveSeedState(
    RuntimeScene3D* scene,
    const RuntimeSceneBridge3DPrimitiveSeedState* seed_state);
bool RuntimeScene3DBuilder_BuildFromPrimitiveSeedStateAtT(
    RuntimeScene3D* scene,
    const RuntimeSceneBridge3DPrimitiveSeedState* seed_state,
    double normalized_t);
bool RuntimeScene3DBuilder_BuildFromBridgeSeeds(RuntimeScene3D* scene);
bool RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(RuntimeScene3D* scene, double normalized_t);
bool RuntimeScene3DBuilder_BuildRouteProbeFromBridgeSeedsAtT(RuntimeScene3D* scene,
                                                             double normalized_t);
const char* RuntimeScene3DBuilder_LastDiagnostics(void);
bool RuntimeScene3DBuilder_AppendMeshAssetSet(
    RuntimeScene3D* scene,
    const RayTracingRuntimeMeshAssetSet* mesh_assets);
bool RuntimeScene3DBuilder_AppendHeightfieldSurface(
    RuntimeScene3D* scene,
    const RuntimeScene3DHeightfieldSurfaceDesc* desc,
    int* out_appended_triangle_count);

#endif
