#ifndef RENDER_RUNTIME_DYNAMIC_GEOMETRY_ACCEL_3D_H
#define RENDER_RUNTIME_DYNAMIC_GEOMETRY_ACCEL_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_3d.h"

typedef enum RuntimeDynamicGeometryWaterAccelDecision3D {
    RUNTIME_DYNAMIC_GEOMETRY_WATER_ACCEL_NOT_PRESENT = 0,
    RUNTIME_DYNAMIC_GEOMETRY_WATER_ACCEL_STATIC_FLATTENED_FRAME,
    RUNTIME_DYNAMIC_GEOMETRY_WATER_ACCEL_REFIT_CANDIDATE_PER_FRAME,
    RUNTIME_DYNAMIC_GEOMETRY_WATER_ACCEL_REBUILD_PER_FRAME,
    RUNTIME_DYNAMIC_GEOMETRY_WATER_ACCEL_FLATTENED_FALLBACK
} RuntimeDynamicGeometryWaterAccelDecision3D;

typedef struct RuntimeDynamicGeometryAcceleration3DInput {
    int static_mesh_loaded_assets;
    int static_mesh_loaded_instances;
    uint64_t static_mesh_loaded_triangles;
    uint64_t static_blas_cached_asset_count;
    uint64_t static_tlas_instance_count;

    bool water_surface_source_found;
    bool water_surface_loaded;
    bool water_surface_frame_selection_built;
    bool water_surface_frame_selection_dynamic;
    bool water_surface_mesh_attached;
    uint64_t water_surface_first_frame_index;
    uint64_t water_surface_last_frame_index;
    uint32_t water_surface_first_grid_w;
    uint32_t water_surface_first_grid_d;
    uint64_t water_surface_first_sample_count;
    uint32_t water_surface_last_grid_w;
    uint32_t water_surface_last_grid_d;
    uint64_t water_surface_last_sample_count;
    int water_surface_triangle_count;

    int mesh_emissive_light_count;
    int mesh_area_sampler_only_count;
} RuntimeDynamicGeometryAcceleration3DInput;

typedef struct RuntimeDynamicGeometryAcceleration3DClassification {
    bool static_mesh_asset_present;
    bool static_mesh_asset_blas_active;
    int static_mesh_asset_loaded_assets;
    int static_mesh_asset_loaded_instances;
    uint64_t static_mesh_asset_loaded_triangles;

    RuntimeDynamicGeometryWaterAccelDecision3D water_surface_decision;
    bool water_surface_present;
    bool water_surface_mesh_attached;
    bool water_surface_frame_selection_dynamic;
    bool water_surface_frame_stamp_changed;
    bool water_surface_topology_comparable;
    bool water_surface_topology_stable;
    uint32_t water_surface_first_grid_w;
    uint32_t water_surface_first_grid_d;
    uint64_t water_surface_first_sample_count;
    uint32_t water_surface_last_grid_w;
    uint32_t water_surface_last_grid_d;
    uint64_t water_surface_last_sample_count;
    int water_surface_triangle_count;

    bool mesh_emissive_internal_present;
    int mesh_emissive_light_count;
    int mesh_area_sampler_only_count;
} RuntimeDynamicGeometryAcceleration3DClassification;

typedef enum RuntimeDynamicGeometryWaterCacheStatus3D {
    RUNTIME_DYNAMIC_GEOMETRY_WATER_CACHE_DISABLED = 0,
    RUNTIME_DYNAMIC_GEOMETRY_WATER_CACHE_REBUILT,
    RUNTIME_DYNAMIC_GEOMETRY_WATER_CACHE_REUSED,
    RUNTIME_DYNAMIC_GEOMETRY_WATER_CACHE_REFIT,
    RUNTIME_DYNAMIC_GEOMETRY_WATER_CACHE_FALLBACK
} RuntimeDynamicGeometryWaterCacheStatus3D;

typedef struct RuntimeDynamicGeometryWaterCacheDiagnostics3D {
    bool valid;
    bool cacheReady;
    RuntimeDynamicGeometryWaterCacheStatus3D lastStatus;
    RuntimeDynamicGeometryWaterAccelDecision3D lastDecision;
    uint64_t observedFrames;
    uint64_t rebuilds;
    uint64_t reuses;
    uint64_t refits;
    uint64_t fallbacks;
    uint64_t lastFrameIndex;
    uint32_t cachedGridW;
    uint32_t cachedGridD;
    uint64_t cachedSampleCount;
    int cachedTriangleCount;
    bool geometryCacheReady;
    bool geometryBVHReady;
    uint64_t geometryStores;
    uint64_t geometryRebuildStores;
    uint64_t geometryRefitStores;
    uint64_t geometryReuseStores;
    uint64_t geometryStoreFailures;
    int geometryBVHNodeCount;
    int geometryBVHLeafCount;
    double geometryStoreMs;
    double geometryBVHBuildMs;
    uint64_t routeTraceCalls;
    uint64_t routeTraceHits;
    uint64_t routeTraceMisses;
    uint64_t routeTraceErrors;
    uint64_t routeTraceUnavailable;
} RuntimeDynamicGeometryWaterCacheDiagnostics3D;

void RuntimeDynamicGeometryAcceleration3D_Classify(
    const RuntimeDynamicGeometryAcceleration3DInput* input,
    RuntimeDynamicGeometryAcceleration3DClassification* out_classification);
void RuntimeDynamicGeometryAcceleration3D_ResetWaterCacheLifecycle(void);
RuntimeDynamicGeometryWaterCacheStatus3D
RuntimeDynamicGeometryAcceleration3D_RecordWaterSurfaceFrame(
    const RuntimeDynamicGeometryAcceleration3DClassification* classification,
    uint64_t frame_index,
    int appended_triangle_count);
void RuntimeDynamicGeometryAcceleration3D_SnapshotWaterCacheDiagnostics(
    RuntimeDynamicGeometryWaterCacheDiagnostics3D* out_diagnostics);
bool RuntimeDynamicGeometryAcceleration3D_StoreWaterSurfaceMeshFromScene(
    const RuntimeScene3D* scene,
    int first_triangle_index,
    int triangle_count);
bool RuntimeDynamicGeometryAcceleration3D_TraceWaterSurfaceFirstHit(
    const RuntimeScene3D* scene,
    const Ray3D* ray,
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_min,
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_max,
    HitInfo3D* out_hit);

const char* RuntimeDynamicGeometryAcceleration3D_StaticMeshPolicyLabel(
    const RuntimeDynamicGeometryAcceleration3DClassification* classification);
const char* RuntimeDynamicGeometryAcceleration3D_WaterSurfacePolicyLabel(
    RuntimeDynamicGeometryWaterAccelDecision3D decision);
const char* RuntimeDynamicGeometryAcceleration3D_WaterSurfaceActionLabel(
    RuntimeDynamicGeometryWaterAccelDecision3D decision);
const char* RuntimeDynamicGeometryAcceleration3D_WaterCacheStatusLabel(
    RuntimeDynamicGeometryWaterCacheStatus3D status);
const char* RuntimeDynamicGeometryAcceleration3D_MeshEmissivePolicyLabel(
    const RuntimeDynamicGeometryAcceleration3DClassification* classification);
const char* RuntimeDynamicGeometryAcceleration3D_GeneratedRuntimeMeshPolicyLabel(void);
const char* RuntimeDynamicGeometryAcceleration3D_DeformingMeshPolicyLabel(void);
const char* RuntimeDynamicGeometryAcceleration3D_NextContractLabel(void);

#endif
