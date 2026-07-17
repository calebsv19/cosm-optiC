#include "editor/scene_editor_digest_overlay_internal.h"

#include <math.h>
#include <stdint.h>

#include "config/config_manager.h"
#include "core_screen_pick.h"
#include "import/runtime_mesh_asset_loader.h"

static CoreScreenPickIndex g_object_pick_index;
static bool g_object_pick_index_initialized = false;

static double scene_editor_object_pick_view_depth(
    const SceneEditorDigestOverlayProjector* projector,
    double world_x,
    double world_y,
    double world_z) {
    const double dx = world_x - projector->center_x;
    const double dy = world_y - projector->center_y;
    const double dz = world_z - projector->center_z;
    const double cy = cos(projector->yaw_rad);
    const double sy = sin(projector->yaw_rad);
    const double cp = cos(projector->pitch_rad);
    const double sp = sin(projector->pitch_rad);
    const double rotated_y = dx * sy + dy * cy;
    return rotated_y * cp - dz * sp;
}

static bool scene_editor_object_pick_add_candidate(
    const SceneEditorDigestOverlayProjector* projector,
    int scene_object_index,
    double world_x,
    double world_y,
    double world_z,
    bool* present,
    CoreScreenPickCandidate* candidates,
    size_t* candidate_count) {
    CoreScreenPickCandidate* candidate = NULL;
    int screen_x = 0;
    int screen_y = 0;

    if (!projector || !present || !candidates || !candidate_count ||
        scene_object_index < 0 || scene_object_index >= sceneSettings.objectCount ||
        scene_object_index >= MAX_OBJECTS) {
        return false;
    }
    if (!SceneEditorDigestOverlayProjectPoint(
            projector, world_x, world_y, world_z, &screen_x, &screen_y)) {
        return false;
    }

    if (present[scene_object_index]) return true;
    candidate = &candidates[*candidate_count];
    *candidate_count += 1;
    present[scene_object_index] = true;
    if (!candidate) return false;

    candidate->stable_key = (uint64_t)(scene_object_index + 1);
    candidate->payload = (int64_t)scene_object_index;
    candidate->screen_x = (double)screen_x;
    candidate->screen_y = (double)screen_y;
    candidate->view_depth = scene_editor_object_pick_view_depth(
        projector, world_x, world_y, world_z);
    return true;
}

static bool scene_editor_object_pick_rebuild(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest) {
    RuntimeSceneBridge3DPrimitiveSeedState seeds = {0};
    const RayTracingRuntimeMeshAssetSet* mesh_assets = NULL;
    CoreScreenPickCandidate candidates[MAX_OBJECTS];
    bool present[MAX_OBJECTS] = {false};
    CoreScreenPickConfig config = core_screen_pick_config_default();
    CoreResult result = {0};
    size_t candidate_count = 0;
    int i = 0;

    if (!projector || !digest) return false;
    if (!g_object_pick_index_initialized) {
        result = core_screen_pick_index_init(&g_object_pick_index, config);
        if (result.code != CORE_OK) return false;
        g_object_pick_index_initialized = true;
    }

    for (i = 0; i < digest->primitive_count; ++i) {
        const RuntimeSceneBridgePrimitiveDigest* primitive = &digest->primitives[i];
        scene_editor_object_pick_add_candidate(projector,
                                               primitive->scene_object_index,
                                               primitive->origin_x,
                                               primitive->origin_y,
                                               primitive->origin_z,
                                               present,
                                               candidates,
                                               &candidate_count);
    }

    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);
    if (seeds.valid) {
        for (i = 0; i < seeds.primitive_count; ++i) {
            const RuntimeSceneBridgePrimitiveSeed* primitive = &seeds.primitives[i];
            scene_editor_object_pick_add_candidate(projector,
                                                   primitive->scene_object_index,
                                                   primitive->origin_x,
                                                   primitive->origin_y,
                                                   primitive->origin_z,
                                                   present,
                                                   candidates,
                                                   &candidate_count);
        }
    }

    mesh_assets = ray_tracing_runtime_mesh_assets_last();
    if (mesh_assets) {
        for (i = 0; i < mesh_assets->instance_count; ++i) {
            const RayTracingRuntimeMeshAssetInstance* instance = &mesh_assets->instances[i];
            scene_editor_object_pick_add_candidate(projector,
                                                   instance->scene_object_index,
                                                   instance->position_x,
                                                   instance->position_y,
                                                   instance->position_z,
                                                   present,
                                                   candidates,
                                                   &candidate_count);
        }
        for (i = 0; i < mesh_assets->skipped_instance_count; ++i) {
            const RayTracingRuntimeMeshAssetInstance* instance =
                &mesh_assets->skipped_instances[i].preview_instance;
            scene_editor_object_pick_add_candidate(projector,
                                                   instance->scene_object_index,
                                                   instance->position_x,
                                                   instance->position_y,
                                                   instance->position_z,
                                                   present,
                                                   candidates,
                                                   &candidate_count);
        }
    }

    result = core_screen_pick_index_rebuild(
        &g_object_pick_index, candidates, candidate_count,
        g_object_pick_index.revision + 1);
    return result.code == CORE_OK;
}

int SceneEditorDigestOverlayPickObjectIndex(const SceneEditorDigestOverlayProjector* projector,
                                            const RuntimeSceneBridge3DDigestState* digest,
                                            int mx,
                                            int my) {
    CoreScreenPickResult pick = {0};
    CoreResult result = {0};

    if (!scene_editor_object_pick_rebuild(projector, digest)) return -1;
    result = core_screen_pick_query_nearest(
        &g_object_pick_index, (double)mx, (double)my, &pick);
    if (result.code != CORE_OK || !pick.found) return -1;
    return (int)pick.payload;
}
