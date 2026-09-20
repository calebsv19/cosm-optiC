#include "editor/editor_mode_router.h"
#include <float.h>
#include "editor/scene_editor_mesh_preview_render.h"
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
    return SceneEditorDigestOverlayViewDepth(projector, world_x, world_y, world_z);
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

/* Reuse the viewport projector and loaded preview geometry. Origins and last-frame
   hover are deliberately excluded: neither proves that geometry is under a click. */
int SceneEditorViewportPickObject(const SceneEditorDigestOverlayProjector* projector,int mx,int my) {
    if(!projector || mx<projector->viewport.x || my<projector->viewport.y ||
       mx>=projector->viewport.x+projector->viewport.w || my>=projector->viewport.y+projector->viewport.h) return -1;
    double best=-DBL_MAX;
    int picked=SceneEditorMeshPreviewPickObjectHit(projector,EDITOR_MODE_OBJECT,-1,mx,my,&best);
    RuntimeSceneBridge3DPrimitiveSeedState seeds={0};
    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);
    if(!seeds.valid) return picked;
    static const int faces[12][3]={{0,1,3},{0,3,2},{4,6,7},{4,7,5},
        {0,4,5},{0,5,1},{2,3,7},{2,7,6},{0,2,6},{0,6,4},{1,5,7},{1,7,3}};
    for(int i=0;i<seeds.primitive_count;++i) {
        const RuntimeSceneBridgePrimitiveSeed* seed=&seeds.primitives[i];
        bool plane=seed->kind==RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE;
        if(!seed->has_dimensions || seed->scene_object_index<0 || seed->guide_only ||
           (!plane && seed->kind!=RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM && seed->kind!=RUNTIME_SCENE_BRIDGE_PRIMITIVE_BOX)) continue;
        double x[8],y[8],z[8]; bool valid=true;
        for(int corner=0;corner<8;++corner) {
            double u=(corner&4 ? 1 : -1)*fmax(.05,fabs(seed->width)*.5);
            double v=(corner&2 ? 1 : -1)*fmax(.05,fabs(seed->height)*.5);
            double n=plane ? 0 : (corner&1 ? 1 : -1)*fmax(.05,fabs(seed->depth)*.5);
            double wx=seed->origin_x+u*seed->axis_u_x+v*seed->axis_v_x+n*seed->normal_x;
            double wy=seed->origin_y+u*seed->axis_u_y+v*seed->axis_v_y+n*seed->normal_y;
            double wz=seed->origin_z+u*seed->axis_u_z+v*seed->axis_v_z+n*seed->normal_z;
            valid &= SceneEditorDigestOverlayProjectPointF(projector,wx,wy,wz,&x[corner],&y[corner]);
            z[corner]=SceneEditorDigestOverlayViewDepth(projector,wx,wy,wz);
        }
        if(!valid) continue;
        for(int face=0;face<12;++face) {
            int a=faces[face][0],b=faces[face][1],c=faces[face][2];
            double denom=(y[b]-y[c])*(x[a]-x[c])+(x[c]-x[b])*(y[a]-y[c]);
            if(fabs(denom)<1e-9) continue;
            double u=((y[b]-y[c])*(mx+.5-x[c])+(x[c]-x[b])*(my+.5-y[c]))/denom;
            double v=((y[c]-y[a])*(mx+.5-x[c])+(x[a]-x[c])*(my+.5-y[c]))/denom;
            double w=1-u-v;
            if(u<0 || v<0 || w<0) continue;
            double depth=u*z[a]+v*z[b]+w*z[c];
            if(depth>best) {best=depth;picked=seed->scene_object_index;}
        }
    }
    return picked;
}
