#include "render/runtime_scene_3d_builder_internal.h"

static char gRuntimeScene3DBuilderLastDiagnostics[4096] = "ok";
static RuntimeScene3DBuilderTimingStats gRuntimeScene3DBuilderTiming;

RuntimeScene3DBuilderTimingStats* runtime_scene_3d_builder_timing_mutable(void) {
    return &gRuntimeScene3DBuilderTiming;
}

double runtime_scene_3d_builder_elapsed_ms_since(const struct timespec* start_time) {
    struct timespec now = {0};
    double elapsed = 0.0;
    if (!start_time) return 0.0;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0.0;
    elapsed = (double)(now.tv_sec - start_time->tv_sec) * 1000.0;
    elapsed += (double)(now.tv_nsec - start_time->tv_nsec) / 1000000.0;
    return elapsed < 0.0 ? 0.0 : elapsed;
}

void runtime_scene_3d_builder_set_diag(const char* message) {
    snprintf(gRuntimeScene3DBuilderLastDiagnostics,
             sizeof(gRuntimeScene3DBuilderLastDiagnostics),
             "%s",
             (message && message[0]) ? message : "ok");
}

const char* RuntimeScene3DBuilder_LastDiagnostics(void) {
    return gRuntimeScene3DBuilderLastDiagnostics;
}

void RuntimeScene3DBuilder_TimingReset(void) {
    memset(&gRuntimeScene3DBuilderTiming, 0, sizeof(gRuntimeScene3DBuilderTiming));
}

void RuntimeScene3DBuilder_TimingSnapshot(RuntimeScene3DBuilderTimingStats* out_stats) {
    if (!out_stats) return;
    *out_stats = gRuntimeScene3DBuilderTiming;
}

bool runtime_scene_3d_builder_rebuild_bvh(RuntimeScene3D* scene) {
    struct timespec stage_start = {0};
    if (!scene) {
        runtime_scene_3d_builder_set_diag("bvh rebuild failed: scene missing");
        return false;
    }
    (void)clock_gettime(CLOCK_MONOTONIC, &stage_start);
    if (!RuntimeTriangleMesh3D_BuildBVH(&scene->triangleMesh)) {
        gRuntimeScene3DBuilderTiming.bvh_rebuild_wall_ms +=
            runtime_scene_3d_builder_elapsed_ms_since(&stage_start);
        char diag[2048];
        snprintf(diag,
                 sizeof(diag),
                 "bvh rebuild failed: %s",
                 RuntimeTriangleMesh3D_BVHLastDiagnostics());
        runtime_scene_3d_builder_set_diag(diag);
        return false;
    }
    gRuntimeScene3DBuilderTiming.bvh_rebuild_wall_ms +=
        runtime_scene_3d_builder_elapsed_ms_since(&stage_start);
    if (scene->triangleMesh.triangleCount > 0 &&
        !RuntimeTriangleMesh3D_HasReadyBVH(&scene->triangleMesh)) {
        runtime_scene_3d_builder_set_diag("bvh rebuild failed: BVH not ready after build");
        return false;
    }
    return true;
}

bool runtime_scene_3d_builder_rebuild_tlas(RuntimeScene3D* scene) {
    if (!RuntimeSceneAcceleration3D_RebuildTLASFromScene(scene)) {
        char diag[2048];
        snprintf(diag,
                 sizeof(diag),
                 "TLAS rebuild failed: %s",
                 RuntimeSceneAcceleration3D_LastDiagnostics());
        runtime_scene_3d_builder_set_diag(diag);
        return false;
    }
    return true;
}

bool runtime_scene_3d_builder_rebuild_prepared_accel(
    RuntimeScene3D* scene,
    const RayTracingRuntimeMeshAssetSet* mesh_assets) {
    if (!RuntimeSceneAcceleration3D_RebuildPreparedFromSceneAndMeshAssets(scene,
                                                                          mesh_assets)) {
        char diag[2048];
        snprintf(diag,
                 sizeof(diag),
                 "prepared acceleration rebuild failed: %s",
                 RuntimeSceneAcceleration3D_LastDiagnostics());
        runtime_scene_3d_builder_set_diag(diag);
        return false;
    }
    return true;
}

bool runtime_scene_3d_builder_should_build_flattened_bvh(void) {
    RuntimeRay3DTraceRoute route = RuntimeRay3D_CurrentTraceRoute();
    return route != RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS;
}
