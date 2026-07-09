#ifndef RENDER_RUNTIME_SCENE_3D_BUILDER_INTERNAL_H
#define RENDER_RUNTIME_SCENE_3D_BUILDER_INTERNAL_H

#include "render/runtime_scene_3d_builder.h"

#include "render/runtime_mesh_blas_cache_3d.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_accel_3d.h"
#include "render/runtime_scene_3d_samples.h"
#include "render/runtime_triangle_bvh_3d.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

RuntimeScene3DBuilderTimingStats* runtime_scene_3d_builder_timing_mutable(void);
double runtime_scene_3d_builder_elapsed_ms_since(const struct timespec* start_time);
void runtime_scene_3d_builder_set_diag(const char* message);
RuntimePrimitive3DKind runtime_scene_3d_builder_map_kind(RuntimeSceneBridgePrimitiveKind kind);
int runtime_scene_3d_builder_triangle_count_for_kind(RuntimePrimitive3DKind kind);
bool runtime_scene_3d_builder_rebuild_bvh(RuntimeScene3D* scene);
bool runtime_scene_3d_builder_rebuild_tlas(RuntimeScene3D* scene);
bool runtime_scene_3d_builder_rebuild_prepared_accel(
    RuntimeScene3D* scene,
    const RayTracingRuntimeMeshAssetSet* mesh_assets);
bool runtime_scene_3d_builder_should_build_flattened_bvh(void);
void runtime_scene_3d_builder_resolve_basis(Vec3* io_axis_u,
                                            Vec3* io_axis_v,
                                            Vec3* io_normal);
bool runtime_scene_3d_builder_reserve_primitives(RuntimeScene3D* scene,
                                                 int primitive_capacity);
bool runtime_scene_3d_builder_reserve_triangles(RuntimeScene3D* scene,
                                                int triangle_capacity);
bool runtime_scene_3d_builder_append_triangle_internal(RuntimeScene3D* scene,
                                                       int primitive_index,
                                                       int scene_object_index,
                                                       Vec3 p0,
                                                       Vec3 p1,
                                                       Vec3 p2,
                                                       Vec3 expected_normal,
                                                       int local_triangle_index_override,
                                                       bool two_sided,
                                                       bool has_object_texture_coords,
                                                       Vec3 object_texture0,
                                                       Vec3 object_texture1,
                                                       Vec3 object_texture2);
bool runtime_scene_3d_builder_append_quad(RuntimeScene3D* scene,
                                          int primitive_index,
                                          int scene_object_index,
                                          Vec3 p0,
                                          Vec3 p1,
                                          Vec3 p2,
                                          Vec3 p3,
                                          Vec3 expected_normal,
                                          bool two_sided);
bool runtime_scene_3d_builder_append_triangles(RuntimeScene3D* scene,
                                               int primitive_index,
                                               const RuntimePrimitive3D* primitive);
bool runtime_scene_3d_builder_append_mesh_asset_set(
    RuntimeScene3D* scene,
    const RayTracingRuntimeMeshAssetSet* mesh_assets,
    bool require_ready_bvh);
void runtime_scene_3d_builder_fill_primitive(RuntimePrimitive3D* primitive,
                                             const RuntimeSceneBridgePrimitiveSeed* seed);

#endif
