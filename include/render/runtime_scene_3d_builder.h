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

typedef struct RuntimeScene3DBuilderTimingStats {
    double total_ms;
    double primitive_seed_ms;
    double mesh_append_total_ms;
    double mesh_append_reserve_ms;
    double mesh_append_expand_ms;
    double bvh_rebuild_wall_ms;
    int mesh_append_calls;
    int mesh_append_assets;
    int mesh_append_instances;
    int mesh_append_triangles_expected;
    int mesh_append_triangles_appended;
} RuntimeScene3DBuilderTimingStats;

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
void RuntimeScene3DBuilder_TimingReset(void);
void RuntimeScene3DBuilder_TimingSnapshot(RuntimeScene3DBuilderTimingStats* out_stats);
bool RuntimeScene3DBuilder_AppendMeshAssetSet(
    RuntimeScene3D* scene,
    const RayTracingRuntimeMeshAssetSet* mesh_assets);
bool RuntimeScene3DBuilder_AppendHeightfieldSurface(
    RuntimeScene3D* scene,
    const RuntimeScene3DHeightfieldSurfaceDesc* desc,
    int* out_appended_triangle_count);

#endif
