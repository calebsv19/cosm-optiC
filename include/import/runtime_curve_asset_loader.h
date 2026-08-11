#ifndef IMPORT_RUNTIME_CURVE_ASSET_LOADER_H
#define IMPORT_RUNTIME_CURVE_ASSET_LOADER_H

#include <stdbool.h>
#include <stddef.h>

#include "render/runtime_curve_primitive_3d.h"

#define RAY_TRACING_RUNTIME_CURVE_ASSET_MAX_ASSETS 64
#define RAY_TRACING_RUNTIME_CURVE_ASSET_MAX_INSTANCES 256
#define RAY_TRACING_RUNTIME_CURVE_ASSET_ID_MAX 64
#define RAY_TRACING_RUNTIME_CURVE_ASSET_PATH_MAX 4096

typedef struct RayTracingRuntimeCurveAsset {
    char asset_id[RAY_TRACING_RUNTIME_CURVE_ASSET_ID_MAX];
    char path[RAY_TRACING_RUNTIME_CURVE_ASSET_PATH_MAX];
    char sha256[65];
    size_t strand_count;
    size_t points_per_strand;
    RuntimeCurveAsset3D asset;
} RayTracingRuntimeCurveAsset;

typedef struct RayTracingRuntimeCurveAssetInstance {
    char object_id[64];
    char asset_id[RAY_TRACING_RUNTIME_CURVE_ASSET_ID_MAX];
    int asset_index;
    int scene_object_index;
    double position_x;
    double position_y;
    double position_z;
    double rotation_x;
    double rotation_y;
    double rotation_z;
    double uniform_scale;
} RayTracingRuntimeCurveAssetInstance;

typedef struct RayTracingRuntimeCurveAssetSet {
    int asset_count;
    RayTracingRuntimeCurveAsset
        assets[RAY_TRACING_RUNTIME_CURVE_ASSET_MAX_ASSETS];
    int instance_count;
    RayTracingRuntimeCurveAssetInstance
        instances[RAY_TRACING_RUNTIME_CURVE_ASSET_MAX_INSTANCES];
} RayTracingRuntimeCurveAssetSet;

void ray_tracing_runtime_curve_asset_set_init(
    RayTracingRuntimeCurveAssetSet *set);
void ray_tracing_runtime_curve_asset_set_free(
    RayTracingRuntimeCurveAssetSet *set);

bool ray_tracing_runtime_curve_assets_load_scene_file(
    const char *runtime_scene_path,
    RayTracingRuntimeCurveAssetSet *out_set,
    char *out_diagnostics,
    size_t out_diagnostics_size);

void ray_tracing_runtime_curve_assets_reset_last(void);
void ray_tracing_runtime_curve_assets_take_last_for_scene(
    const char *runtime_scene_path,
    RayTracingRuntimeCurveAssetSet *set);
const RayTracingRuntimeCurveAssetSet *
ray_tracing_runtime_curve_assets_last(void);
bool ray_tracing_runtime_curve_assets_last_matches_scene_file(
    const char *runtime_scene_path);

#endif
