#include "render/runtime_dynamic_geometry_accel_3d.h"

#include "render/runtime_triangle_bvh_3d.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static RuntimeDynamicGeometryWaterCacheDiagnostics3D gRuntimeDynamicGeometryWaterCache;
static RuntimeTriangleMesh3D gRuntimeDynamicGeometryWaterMeshCache;

static double runtime_dynamic_geometry_accel_3d_elapsed_ms_since(
    const struct timespec* start_time) {
    struct timespec now = {0};
    double elapsed = 0.0;
    if (!start_time) return 0.0;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0.0;
    elapsed = (double)(now.tv_sec - start_time->tv_sec) * 1000.0;
    elapsed += (double)(now.tv_nsec - start_time->tv_nsec) / 1000000.0;
    return elapsed < 0.0 ? 0.0 : elapsed;
}

static void runtime_dynamic_geometry_apply_source_ref(const RuntimeScene3D* scene,
                                                      HitInfo3D* hit) {
    if (!scene || !hit) return;
    if (hit->primitiveIndex >= 0 && hit->primitiveIndex < scene->primitiveCount) {
        hit->source = scene->primitives[hit->primitiveIndex].source;
        hit->sceneObjectIndex = scene->primitives[hit->primitiveIndex].source.sceneObjectIndex;
    }
}

static bool runtime_dynamic_geometry_water_cache_topology_matches(
    const RuntimeDynamicGeometryAcceleration3DClassification* classification,
    int appended_triangle_count) {
    if (!classification || !gRuntimeDynamicGeometryWaterCache.cacheReady) {
        return false;
    }
    return gRuntimeDynamicGeometryWaterCache.cachedGridW ==
               classification->water_surface_first_grid_w &&
           gRuntimeDynamicGeometryWaterCache.cachedGridD ==
               classification->water_surface_first_grid_d &&
           gRuntimeDynamicGeometryWaterCache.cachedSampleCount ==
               classification->water_surface_first_sample_count &&
           gRuntimeDynamicGeometryWaterCache.cachedTriangleCount ==
               appended_triangle_count;
}

static void runtime_dynamic_geometry_water_cache_store_topology(
    const RuntimeDynamicGeometryAcceleration3DClassification* classification,
    uint64_t frame_index,
    int appended_triangle_count) {
    if (!classification) return;
    gRuntimeDynamicGeometryWaterCache.cacheReady = true;
    gRuntimeDynamicGeometryWaterCache.lastFrameIndex = frame_index;
    gRuntimeDynamicGeometryWaterCache.cachedGridW =
        classification->water_surface_first_grid_w;
    gRuntimeDynamicGeometryWaterCache.cachedGridD =
        classification->water_surface_first_grid_d;
    gRuntimeDynamicGeometryWaterCache.cachedSampleCount =
        classification->water_surface_first_sample_count;
    gRuntimeDynamicGeometryWaterCache.cachedTriangleCount = appended_triangle_count;
}

static bool runtime_dynamic_geometry_water_topology_comparable(
    const RuntimeDynamicGeometryAcceleration3DInput* input) {
    if (!input || !input->water_surface_loaded) return false;
    if (input->water_surface_first_grid_w < 2u ||
        input->water_surface_first_grid_d < 2u ||
        input->water_surface_first_sample_count == 0u) {
        return false;
    }
    if (!input->water_surface_frame_selection_dynamic) return true;
    return input->water_surface_last_grid_w >= 2u &&
           input->water_surface_last_grid_d >= 2u &&
           input->water_surface_last_sample_count > 0u;
}

static bool runtime_dynamic_geometry_water_topology_stable(
    const RuntimeDynamicGeometryAcceleration3DInput* input) {
    if (!input) return false;
    if (!input->water_surface_frame_selection_dynamic) return true;
    return input->water_surface_first_grid_w == input->water_surface_last_grid_w &&
           input->water_surface_first_grid_d == input->water_surface_last_grid_d &&
           input->water_surface_first_sample_count ==
               input->water_surface_last_sample_count;
}

void RuntimeDynamicGeometryAcceleration3D_Classify(
    const RuntimeDynamicGeometryAcceleration3DInput* input,
    RuntimeDynamicGeometryAcceleration3DClassification* out_classification) {
    RuntimeDynamicGeometryAcceleration3DClassification classification;
    bool water_frame_stamp_changed = false;

    if (!out_classification) return;
    memset(&classification, 0, sizeof(classification));
    if (!input) {
        classification.water_surface_decision =
            RUNTIME_DYNAMIC_GEOMETRY_WATER_ACCEL_NOT_PRESENT;
        *out_classification = classification;
        return;
    }

    classification.static_mesh_asset_present =
        input->static_mesh_loaded_instances > 0;
    classification.static_mesh_asset_blas_active =
        input->static_blas_cached_asset_count > 0u &&
        input->static_tlas_instance_count > 0u;
    classification.static_mesh_asset_loaded_assets =
        input->static_mesh_loaded_assets;
    classification.static_mesh_asset_loaded_instances =
        input->static_mesh_loaded_instances;
    classification.static_mesh_asset_loaded_triangles =
        input->static_mesh_loaded_triangles;

    classification.water_surface_present =
        input->water_surface_source_found || input->water_surface_mesh_attached;
    classification.water_surface_mesh_attached =
        input->water_surface_mesh_attached;
    classification.water_surface_frame_selection_dynamic =
        input->water_surface_frame_selection_dynamic;
    water_frame_stamp_changed =
        input->water_surface_frame_selection_dynamic ||
        input->water_surface_first_frame_index != input->water_surface_last_frame_index;
    classification.water_surface_frame_stamp_changed = water_frame_stamp_changed;
    classification.water_surface_topology_comparable =
        runtime_dynamic_geometry_water_topology_comparable(input);
    classification.water_surface_topology_stable =
        classification.water_surface_topology_comparable &&
        runtime_dynamic_geometry_water_topology_stable(input);
    classification.water_surface_first_grid_w = input->water_surface_first_grid_w;
    classification.water_surface_first_grid_d = input->water_surface_first_grid_d;
    classification.water_surface_first_sample_count =
        input->water_surface_first_sample_count;
    classification.water_surface_last_grid_w = input->water_surface_last_grid_w;
    classification.water_surface_last_grid_d = input->water_surface_last_grid_d;
    classification.water_surface_last_sample_count =
        input->water_surface_last_sample_count;
    classification.water_surface_triangle_count = input->water_surface_triangle_count;

    if (!classification.water_surface_present) {
        classification.water_surface_decision =
            RUNTIME_DYNAMIC_GEOMETRY_WATER_ACCEL_NOT_PRESENT;
    } else if (!classification.water_surface_mesh_attached ||
               !classification.water_surface_topology_comparable) {
        classification.water_surface_decision =
            RUNTIME_DYNAMIC_GEOMETRY_WATER_ACCEL_FLATTENED_FALLBACK;
    } else if (!water_frame_stamp_changed) {
        classification.water_surface_decision =
            RUNTIME_DYNAMIC_GEOMETRY_WATER_ACCEL_STATIC_FLATTENED_FRAME;
    } else if (classification.water_surface_topology_stable) {
        classification.water_surface_decision =
            RUNTIME_DYNAMIC_GEOMETRY_WATER_ACCEL_REFIT_CANDIDATE_PER_FRAME;
    } else {
        classification.water_surface_decision =
            RUNTIME_DYNAMIC_GEOMETRY_WATER_ACCEL_REBUILD_PER_FRAME;
    }

    classification.mesh_emissive_internal_present =
        input->mesh_emissive_light_count > 0 ||
        input->mesh_area_sampler_only_count > 0;
    classification.mesh_emissive_light_count = input->mesh_emissive_light_count;
    classification.mesh_area_sampler_only_count =
        input->mesh_area_sampler_only_count;

    *out_classification = classification;
}

void RuntimeDynamicGeometryAcceleration3D_ResetWaterCacheLifecycle(void) {
    RuntimeTriangleMesh3D_Free(&gRuntimeDynamicGeometryWaterMeshCache);
    memset(&gRuntimeDynamicGeometryWaterCache,
           0,
           sizeof(gRuntimeDynamicGeometryWaterCache));
    gRuntimeDynamicGeometryWaterCache.lastStatus =
        RUNTIME_DYNAMIC_GEOMETRY_WATER_CACHE_DISABLED;
    gRuntimeDynamicGeometryWaterCache.lastDecision =
        RUNTIME_DYNAMIC_GEOMETRY_WATER_ACCEL_NOT_PRESENT;
}

RuntimeDynamicGeometryWaterCacheStatus3D
RuntimeDynamicGeometryAcceleration3D_RecordWaterSurfaceFrame(
    const RuntimeDynamicGeometryAcceleration3DClassification* classification,
    uint64_t frame_index,
    int appended_triangle_count) {
    RuntimeDynamicGeometryWaterCacheStatus3D status =
        RUNTIME_DYNAMIC_GEOMETRY_WATER_CACHE_DISABLED;
    const bool topology_matches =
        runtime_dynamic_geometry_water_cache_topology_matches(classification,
                                                              appended_triangle_count);
    const bool same_frame =
        gRuntimeDynamicGeometryWaterCache.cacheReady &&
        gRuntimeDynamicGeometryWaterCache.lastFrameIndex == frame_index;

    gRuntimeDynamicGeometryWaterCache.valid = true;
    gRuntimeDynamicGeometryWaterCache.observedFrames += 1u;
    if (classification) {
        gRuntimeDynamicGeometryWaterCache.lastDecision =
            classification->water_surface_decision;
    }

    if (!classification ||
        !classification->water_surface_present ||
        !classification->water_surface_mesh_attached ||
        !classification->water_surface_topology_comparable ||
        appended_triangle_count <= 0) {
        status = RUNTIME_DYNAMIC_GEOMETRY_WATER_CACHE_FALLBACK;
        gRuntimeDynamicGeometryWaterCache.fallbacks += 1u;
    } else if (!gRuntimeDynamicGeometryWaterCache.cacheReady || !topology_matches) {
        status = RUNTIME_DYNAMIC_GEOMETRY_WATER_CACHE_REBUILT;
        gRuntimeDynamicGeometryWaterCache.rebuilds += 1u;
        runtime_dynamic_geometry_water_cache_store_topology(classification,
                                                            frame_index,
                                                            appended_triangle_count);
    } else if (same_frame) {
        status = RUNTIME_DYNAMIC_GEOMETRY_WATER_CACHE_REUSED;
        gRuntimeDynamicGeometryWaterCache.reuses += 1u;
    } else {
        status = RUNTIME_DYNAMIC_GEOMETRY_WATER_CACHE_REFIT;
        gRuntimeDynamicGeometryWaterCache.refits += 1u;
        runtime_dynamic_geometry_water_cache_store_topology(classification,
                                                            frame_index,
                                                            appended_triangle_count);
    }

    gRuntimeDynamicGeometryWaterCache.lastStatus = status;
    return status;
}

void RuntimeDynamicGeometryAcceleration3D_SnapshotWaterCacheDiagnostics(
    RuntimeDynamicGeometryWaterCacheDiagnostics3D* out_diagnostics) {
    if (!out_diagnostics) return;
    *out_diagnostics = gRuntimeDynamicGeometryWaterCache;
}

bool RuntimeDynamicGeometryAcceleration3D_StoreWaterSurfaceMeshFromScene(
    const RuntimeScene3D* scene,
    int first_triangle_index,
    int triangle_count) {
    RuntimeTriangleMesh3D mesh;
    RuntimeDynamicGeometryWaterCacheStatus3D status =
        gRuntimeDynamicGeometryWaterCache.lastStatus;
    struct timespec store_start = {0};
    struct timespec bvh_start = {0};

    (void)clock_gettime(CLOCK_MONOTONIC, &store_start);
    RuntimeTriangleMesh3D_Init(&mesh);
    if (!scene || first_triangle_index < 0 || triangle_count <= 0 ||
        first_triangle_index > scene->triangleMesh.triangleCount ||
        triangle_count > scene->triangleMesh.triangleCount - first_triangle_index ||
        !scene->triangleMesh.triangles) {
        gRuntimeDynamicGeometryWaterCache.geometryStoreFailures += 1u;
        gRuntimeDynamicGeometryWaterCache.geometryStoreMs +=
            runtime_dynamic_geometry_accel_3d_elapsed_ms_since(&store_start);
        return false;
    }

    mesh.triangles = (RuntimeTriangle3D*)malloc(sizeof(*mesh.triangles) *
                                                (size_t)triangle_count);
    if (!mesh.triangles) {
        gRuntimeDynamicGeometryWaterCache.geometryStoreFailures += 1u;
        gRuntimeDynamicGeometryWaterCache.geometryStoreMs +=
            runtime_dynamic_geometry_accel_3d_elapsed_ms_since(&store_start);
        return false;
    }
    memcpy(mesh.triangles,
           &scene->triangleMesh.triangles[first_triangle_index],
           sizeof(*mesh.triangles) * (size_t)triangle_count);
    mesh.triangleCount = triangle_count;
    mesh.triangleCapacity = triangle_count;
    mesh.bvhDirty = true;
    (void)clock_gettime(CLOCK_MONOTONIC, &bvh_start);
    if (!RuntimeTriangleMesh3D_BuildBVH(&mesh) ||
        !RuntimeTriangleMesh3D_HasReadyBVH(&mesh)) {
        gRuntimeDynamicGeometryWaterCache.geometryBVHBuildMs +=
            runtime_dynamic_geometry_accel_3d_elapsed_ms_since(&bvh_start);
        RuntimeTriangleMesh3D_Free(&mesh);
        gRuntimeDynamicGeometryWaterCache.geometryStoreFailures += 1u;
        gRuntimeDynamicGeometryWaterCache.geometryStoreMs +=
            runtime_dynamic_geometry_accel_3d_elapsed_ms_since(&store_start);
        return false;
    }
    gRuntimeDynamicGeometryWaterCache.geometryBVHBuildMs +=
        runtime_dynamic_geometry_accel_3d_elapsed_ms_since(&bvh_start);

    RuntimeTriangleMesh3D_Free(&gRuntimeDynamicGeometryWaterMeshCache);
    gRuntimeDynamicGeometryWaterMeshCache = mesh;
    gRuntimeDynamicGeometryWaterCache.geometryCacheReady = true;
    gRuntimeDynamicGeometryWaterCache.geometryBVHReady =
        RuntimeTriangleMesh3D_HasReadyBVH(&gRuntimeDynamicGeometryWaterMeshCache);
    gRuntimeDynamicGeometryWaterCache.geometryStores += 1u;
    if (status == RUNTIME_DYNAMIC_GEOMETRY_WATER_CACHE_REBUILT) {
        gRuntimeDynamicGeometryWaterCache.geometryRebuildStores += 1u;
    } else if (status == RUNTIME_DYNAMIC_GEOMETRY_WATER_CACHE_REFIT) {
        gRuntimeDynamicGeometryWaterCache.geometryRefitStores += 1u;
    } else if (status == RUNTIME_DYNAMIC_GEOMETRY_WATER_CACHE_REUSED) {
        gRuntimeDynamicGeometryWaterCache.geometryReuseStores += 1u;
    }
    gRuntimeDynamicGeometryWaterCache.geometryBVHNodeCount =
        RuntimeTriangleMesh3D_BVHNodeCount(&gRuntimeDynamicGeometryWaterMeshCache);
    gRuntimeDynamicGeometryWaterCache.geometryBVHLeafCount =
        RuntimeTriangleMesh3D_BVHLeafCount(&gRuntimeDynamicGeometryWaterMeshCache);
    gRuntimeDynamicGeometryWaterCache.geometryStoreMs +=
        runtime_dynamic_geometry_accel_3d_elapsed_ms_since(&store_start);
    return true;
}

bool RuntimeDynamicGeometryAcceleration3D_TraceWaterSurfaceFirstHit(
    const RuntimeScene3D* scene,
    const Ray3D* ray,
    double t_min,
    double t_max,
    HitInfo3D* out_hit) {
    RuntimeTriangleBVH3DTraceResult trace_result;
    HitInfo3D hit = {0};

    if (out_hit) HitInfo3D_Reset(out_hit);
    if (!scene || !ray || !out_hit) {
        gRuntimeDynamicGeometryWaterCache.routeTraceErrors += 1u;
        return false;
    }
    if (!gRuntimeDynamicGeometryWaterCache.valid) {
        return false;
    }
    gRuntimeDynamicGeometryWaterCache.routeTraceCalls += 1u;
    if (!gRuntimeDynamicGeometryWaterCache.geometryCacheReady ||
        !RuntimeTriangleMesh3D_HasReadyBVH(&gRuntimeDynamicGeometryWaterMeshCache)) {
        gRuntimeDynamicGeometryWaterCache.routeTraceUnavailable += 1u;
        return false;
    }

    trace_result = RuntimeTriangleBVH3D_TraceFirstHitStatus(
        &gRuntimeDynamicGeometryWaterMeshCache,
        ray,
        t_min,
        t_max,
        &hit);
    if (trace_result == RUNTIME_TRIANGLE_BVH_3D_TRACE_HIT) {
        runtime_dynamic_geometry_apply_source_ref(scene, &hit);
        *out_hit = hit;
        gRuntimeDynamicGeometryWaterCache.routeTraceHits += 1u;
        return true;
    }
    if (trace_result == RUNTIME_TRIANGLE_BVH_3D_TRACE_OVERFLOW) {
        gRuntimeDynamicGeometryWaterCache.routeTraceErrors += 1u;
        return false;
    }
    gRuntimeDynamicGeometryWaterCache.routeTraceMisses += 1u;
    return false;
}

const char* RuntimeDynamicGeometryAcceleration3D_StaticMeshPolicyLabel(
    const RuntimeDynamicGeometryAcceleration3DClassification* classification) {
    (void)classification;
    return "static_blas_tlas";
}

const char* RuntimeDynamicGeometryAcceleration3D_WaterSurfacePolicyLabel(
    RuntimeDynamicGeometryWaterAccelDecision3D decision) {
    switch (decision) {
        case RUNTIME_DYNAMIC_GEOMETRY_WATER_ACCEL_STATIC_FLATTENED_FRAME:
            return "static_flattened_frame";
        case RUNTIME_DYNAMIC_GEOMETRY_WATER_ACCEL_REFIT_CANDIDATE_PER_FRAME:
            return "dynamic_flattened_refit_candidate_per_frame";
        case RUNTIME_DYNAMIC_GEOMETRY_WATER_ACCEL_REBUILD_PER_FRAME:
            return "dynamic_flattened_rebuild_per_frame";
        case RUNTIME_DYNAMIC_GEOMETRY_WATER_ACCEL_FLATTENED_FALLBACK:
            return "dynamic_flattened_fallback";
        case RUNTIME_DYNAMIC_GEOMETRY_WATER_ACCEL_NOT_PRESENT:
        default:
            return "not_present";
    }
}

const char* RuntimeDynamicGeometryAcceleration3D_WaterSurfaceActionLabel(
    RuntimeDynamicGeometryWaterAccelDecision3D decision) {
    switch (decision) {
        case RUNTIME_DYNAMIC_GEOMETRY_WATER_ACCEL_STATIC_FLATTENED_FRAME:
            return "reuse_selected_flattened_frame";
        case RUNTIME_DYNAMIC_GEOMETRY_WATER_ACCEL_REFIT_CANDIDATE_PER_FRAME:
            return "refit_candidate_when_dynamic_accel_cache_exists";
        case RUNTIME_DYNAMIC_GEOMETRY_WATER_ACCEL_REBUILD_PER_FRAME:
            return "rebuild_dynamic_flattened_surface_each_frame";
        case RUNTIME_DYNAMIC_GEOMETRY_WATER_ACCEL_FLATTENED_FALLBACK:
            return "flattened_scene_bvh_fallback";
        case RUNTIME_DYNAMIC_GEOMETRY_WATER_ACCEL_NOT_PRESENT:
        default:
            return "none";
    }
}

const char* RuntimeDynamicGeometryAcceleration3D_WaterCacheStatusLabel(
    RuntimeDynamicGeometryWaterCacheStatus3D status) {
    switch (status) {
        case RUNTIME_DYNAMIC_GEOMETRY_WATER_CACHE_REBUILT:
            return "rebuilt";
        case RUNTIME_DYNAMIC_GEOMETRY_WATER_CACHE_REUSED:
            return "reused";
        case RUNTIME_DYNAMIC_GEOMETRY_WATER_CACHE_REFIT:
            return "refit";
        case RUNTIME_DYNAMIC_GEOMETRY_WATER_CACHE_FALLBACK:
            return "fallback";
        case RUNTIME_DYNAMIC_GEOMETRY_WATER_CACHE_DISABLED:
        default:
            return "disabled";
    }
}

const char* RuntimeDynamicGeometryAcceleration3D_MeshEmissivePolicyLabel(
    const RuntimeDynamicGeometryAcceleration3DClassification* classification) {
    return (classification && classification->mesh_emissive_internal_present)
               ? "sampler_cache_not_blas_tlas"
               : "not_present";
}

const char* RuntimeDynamicGeometryAcceleration3D_GeneratedRuntimeMeshPolicyLabel(void) {
    return "file_backed_mesh_asset_static_when_sidecar_resolved";
}

const char* RuntimeDynamicGeometryAcceleration3D_DeformingMeshPolicyLabel(void) {
    return "not_present_pending_dynamic_accel_contract";
}

const char* RuntimeDynamicGeometryAcceleration3D_NextContractLabel(void) {
    return "classify_frame_stamp_topology_refit_rebuild_or_flattened_fallback";
}
