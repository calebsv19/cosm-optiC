#include "render/runtime_scene_3d_builder_internal.h"

#include "import/runtime_scene_motion_bridge.h"

static bool runtime_scene_3d_builder_smooth_mesh_shading_enabled(void) {
    const char* mode = getenv("RAY_TRACING_MESH_SHADING_MODE");
    return !mode || !mode[0] ||
           (strcmp(mode, "flat") != 0 && strcmp(mode, "0") != 0);
}

static Vec3 runtime_scene_3d_builder_rotate_instance(Vec3 p,
                                                     const RayTracingRuntimeMeshAssetInstance* instance) {
    double cx = cos(instance->rotation_x);
    double sx = sin(instance->rotation_x);
    double cy = cos(instance->rotation_y);
    double sy = sin(instance->rotation_y);
    double cz = cos(instance->rotation_z);
    double sz = sin(instance->rotation_z);
    Vec3 q = p;
    Vec3 r = q;

    q.y = r.y * cx - r.z * sx;
    q.z = r.y * sx + r.z * cx;
    r = q;
    q.x = r.x * cy + r.z * sy;
    q.z = -r.x * sy + r.z * cy;
    r = q;
    q.x = r.x * cz - r.y * sz;
    q.y = r.x * sz + r.y * cz;
    return q;
}
static Vec3 runtime_scene_3d_builder_transform_mesh_vertex(
    const CoreMeshAssetRuntimeVertex* vertex,
    const RayTracingRuntimeMeshAssetInstance* instance,
    Vec3 pivot) {
    Vec3 p = vec3(vertex->position.x * instance->scale_x,
                  vertex->position.y * instance->scale_y,
                  vertex->position.z * instance->scale_z);
    p = vec3_sub(p, pivot);
    p = runtime_scene_3d_builder_rotate_instance(p, instance);
    p = vec3_add(p, pivot);
    return vec3_add(p,
                    vec3(instance->position_x,
                         instance->position_y,
                         instance->position_z));
}

static Vec3 runtime_scene_3d_builder_transform_mesh_normal(
    const CoreMeshAssetRuntimeVertex* vertex,
    const RayTracingRuntimeMeshAssetInstance* instance) {
    Vec3 normal;
    if (!vertex || !instance || fabs(instance->scale_x) <= 1e-18 ||
        fabs(instance->scale_y) <= 1e-18 || fabs(instance->scale_z) <= 1e-18) {
        return vec3(0.0, 0.0, 0.0);
    }
    normal = vec3(vertex->normal.x / instance->scale_x,
                  vertex->normal.y / instance->scale_y,
                  vertex->normal.z / instance->scale_z);
    normal = runtime_scene_3d_builder_rotate_instance(normal, instance);
    return vec3_normalize(normal);
}

typedef struct {
    Vec3 min;
    Vec3 max;
    Vec3 extent;
    bool valid;
} RuntimeScene3DBuilderMeshBounds;

static Vec3 runtime_scene_3d_builder_mesh_rotation_pivot(
    const CoreMeshAssetRuntimeDocument* document,
    const RayTracingRuntimeMeshAssetInstance* instance) {
    CoreObjectVec3 min = {0};
    CoreObjectVec3 max = {0};
    if (!document || !instance) {
        return vec3(0.0, 0.0, 0.0);
    }
    if (instance->rotation_pivot_policy ==
        RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_CUSTOM) {
        return vec3(instance->rotation_pivot_x * instance->scale_x,
                    instance->rotation_pivot_y * instance->scale_y,
                    instance->rotation_pivot_z * instance->scale_z);
    }
    if (document->vertex_count == 0u || !document->vertices ||
        instance->rotation_pivot_policy !=
            RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_BOUNDS_CENTER) {
        return vec3(0.0, 0.0, 0.0);
    }
    min = document->contract.local_bounds.min;
    max = document->contract.local_bounds.max;
    if (min.x == 0.0 && min.y == 0.0 && min.z == 0.0 &&
        max.x == 0.0 && max.y == 0.0 && max.z == 0.0) {
        min = document->vertices[0].position;
        max = document->vertices[0].position;
        for (size_t i = 1u; i < document->vertex_count; ++i) {
            const CoreObjectVec3 p = document->vertices[i].position;
            if (p.x < min.x) min.x = p.x;
            if (p.y < min.y) min.y = p.y;
            if (p.z < min.z) min.z = p.z;
            if (p.x > max.x) max.x = p.x;
            if (p.y > max.y) max.y = p.y;
            if (p.z > max.z) max.z = p.z;
        }
    }
    return vec3(((min.x + max.x) * 0.5) * instance->scale_x,
                ((min.y + max.y) * 0.5) * instance->scale_y,
                ((min.z + max.z) * 0.5) * instance->scale_z);
}


static RuntimeScene3DBuilderMeshBounds runtime_scene_3d_builder_mesh_bounds(
    const CoreMeshAssetRuntimeDocument* document,
    const RayTracingRuntimeMeshAssetInstance* instance) {
    RuntimeScene3DBuilderMeshBounds bounds = {0};
    Vec3 pivot = runtime_scene_3d_builder_mesh_rotation_pivot(document, instance);
    if (!document || !instance || document->vertex_count == 0u) {
        return bounds;
    }
    for (size_t i = 0; i < document->vertex_count; ++i) {
        Vec3 p =
            runtime_scene_3d_builder_transform_mesh_vertex(&document->vertices[i],
                                                          instance,
                                                          pivot);
        if (i == 0u) {
            bounds.min = p;
            bounds.max = p;
        } else {
            if (p.x < bounds.min.x) bounds.min.x = p.x;
            if (p.y < bounds.min.y) bounds.min.y = p.y;
            if (p.z < bounds.min.z) bounds.min.z = p.z;
            if (p.x > bounds.max.x) bounds.max.x = p.x;
            if (p.y > bounds.max.y) bounds.max.y = p.y;
            if (p.z > bounds.max.z) bounds.max.z = p.z;
        }
    }
    bounds.extent = vec3(bounds.max.x - bounds.min.x,
                         bounds.max.y - bounds.min.y,
                         bounds.max.z - bounds.min.z);
    bounds.valid = true;
    return bounds;
}

static double runtime_scene_3d_builder_normalize_axis(double value,
                                                      double min_value,
                                                      double extent) {
    if (extent <= 1e-9) return 0.5;
    return (value - min_value) / extent;
}

static Vec3 runtime_scene_3d_builder_object_texture_coord(
    Vec3 p,
    const RuntimeScene3DBuilderMeshBounds* bounds) {
    if (!bounds || !bounds->valid) return vec3(0.5, 0.5, 0.5);
    return vec3(runtime_scene_3d_builder_normalize_axis(p.x,
                                                        bounds->min.x,
                                                        bounds->extent.x),
                runtime_scene_3d_builder_normalize_axis(p.y,
                                                        bounds->min.y,
                                                        bounds->extent.y),
                runtime_scene_3d_builder_normalize_axis(p.z,
                                                        bounds->min.z,
                                                        bounds->extent.z));
}

static int runtime_scene_3d_builder_resolve_mesh_scene_object_index(
    const RayTracingRuntimeMeshAssetInstance* instance) {
    char object_id[RUNTIME_SCENE_3D_MAX_OBJECT_ID] = {0};
    if (!instance) return -1;
    if (instance->scene_object_index >= 0 &&
        runtime_scene_bridge_get_last_object_id_for_scene_index(instance->scene_object_index,
                                                                object_id,
                                                                sizeof(object_id)) &&
        strcmp(object_id, instance->object_id) == 0) {
        return instance->scene_object_index;
    }
    for (int i = 0; i < MAX_OBJECTS; ++i) {
        if (runtime_scene_bridge_get_last_object_id_for_scene_index(i,
                                                                    object_id,
                                                                    sizeof(object_id)) &&
            strcmp(object_id, instance->object_id) == 0) {
            return i;
        }
    }
    return instance->scene_object_index;
}

static void runtime_scene_3d_builder_apply_motion_to_mesh_instance(
    RayTracingRuntimeMeshAssetInstance* instance,
    double normalized_t) {
    RuntimeMotionTrack3DSample sample = {0};
    if (!instance || !runtime_scene_motion_bridge_sample_object(instance->object_id,
                                                                normalized_t,
                                                                &sample)) {
        return;
    }
    if (sample.has_position) {
        instance->position_x = sample.position_x;
        instance->position_y = sample.position_y;
        instance->position_z = sample.position_z;
    }
    if (sample.has_rotation) {
        instance->rotation_x = sample.pitch_radians;
        instance->rotation_y = sample.yaw_radians;
        instance->rotation_z = sample.roll_radians;
    }
}

bool runtime_scene_3d_builder_append_mesh_asset_set_at_t(
    RuntimeScene3D* scene,
    const RayTracingRuntimeMeshAssetSet* mesh_assets,
    bool require_ready_bvh,
    double normalized_t) {
    int extra_primitives = 0;
    int extra_triangles = 0;
    struct timespec total_start = {0};
    struct timespec stage_start = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &total_start);
    runtime_scene_3d_builder_timing_mutable()->mesh_append_calls += 1;
    if (!scene || !mesh_assets) {
        runtime_scene_3d_builder_set_diag("append mesh assets failed: scene or mesh set missing");
        return false;
    }
    if (mesh_assets->instance_count <= 0) return true;
    if (!RuntimeMeshBLASCache3D_PrepareAssetSet(mesh_assets)) {
        char diag[2048];
        snprintf(diag,
                 sizeof(diag),
                 "append mesh assets failed: BLAS cache prepare failed: %s",
                 RuntimeMeshBLASCache3D_LastDiagnostics());
        runtime_scene_3d_builder_set_diag(diag);
        return false;
    }

    runtime_scene_3d_builder_timing_mutable()->mesh_append_assets += mesh_assets->asset_count;
    runtime_scene_3d_builder_timing_mutable()->mesh_append_instances += mesh_assets->instance_count;
    for (int i = 0; i < mesh_assets->instance_count; ++i) {
        const RayTracingRuntimeMeshAssetInstance* instance = &mesh_assets->instances[i];
        if (instance->asset_index < 0 || instance->asset_index >= mesh_assets->asset_count) {
            runtime_scene_3d_builder_set_diag("append mesh assets failed: invalid asset index");
            return false;
        }
        extra_primitives += 1;
        extra_triangles += (int)mesh_assets->assets[instance->asset_index].document.triangle_count;
    }
    runtime_scene_3d_builder_timing_mutable()->mesh_append_triangles_expected += extra_triangles;
    (void)clock_gettime(CLOCK_MONOTONIC, &stage_start);
    if (!runtime_scene_3d_builder_reserve_primitives(scene,
                                                     scene->primitiveCount + extra_primitives)) {
        runtime_scene_3d_builder_timing_mutable()->mesh_append_reserve_ms +=
            runtime_scene_3d_builder_elapsed_ms_since(&stage_start);
        runtime_scene_3d_builder_set_diag("append mesh assets failed: primitive reserve failed");
        return false;
    }
    if (!runtime_scene_3d_builder_reserve_triangles(
            scene,
            scene->triangleMesh.triangleCount + extra_triangles)) {
        runtime_scene_3d_builder_timing_mutable()->mesh_append_reserve_ms +=
            runtime_scene_3d_builder_elapsed_ms_since(&stage_start);
        runtime_scene_3d_builder_set_diag("append mesh assets failed: triangle reserve failed");
        return false;
    }
    runtime_scene_3d_builder_timing_mutable()->mesh_append_reserve_ms +=
        runtime_scene_3d_builder_elapsed_ms_since(&stage_start);

    (void)clock_gettime(CLOCK_MONOTONIC, &stage_start);
    for (int i = 0; i < mesh_assets->instance_count; ++i) {
        RayTracingRuntimeMeshAssetInstance sampled_instance = mesh_assets->instances[i];
        const RayTracingRuntimeMeshAssetInstance* instance = &sampled_instance;
        const RayTracingRuntimeMeshAsset* asset = &mesh_assets->assets[instance->asset_index];
        const CoreMeshAssetRuntimeDocument* document = &asset->document;
        RuntimePrimitive3D* primitive = &scene->primitives[scene->primitiveCount];
        int primitive_index = scene->primitiveCount;
        int scene_object_index =
            runtime_scene_3d_builder_resolve_mesh_scene_object_index(instance);
        RuntimeScene3DBuilderMeshBounds bounds =
            runtime_scene_3d_builder_mesh_bounds(document, instance);
        Vec3 pivot = runtime_scene_3d_builder_mesh_rotation_pivot(document, instance);
        int appended_triangle_count = 0;

        runtime_scene_3d_builder_apply_motion_to_mesh_instance(&sampled_instance,
                                                               normalized_t);
        instance = &sampled_instance;
        scene_object_index = runtime_scene_3d_builder_resolve_mesh_scene_object_index(instance);
        bounds = runtime_scene_3d_builder_mesh_bounds(document, instance);
        pivot = runtime_scene_3d_builder_mesh_rotation_pivot(document, instance);

        memset(primitive, 0, sizeof(*primitive));
        primitive->kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
        primitive->source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
        primitive->source.sceneObjectIndex = scene_object_index;
        snprintf(primitive->source.objectId,
                 sizeof(primitive->source.objectId),
                 "%s",
                 instance->object_id);

        for (size_t j = 0; j < document->triangle_count; ++j) {
            const CoreMeshAssetRuntimeTriangle* src = &document->triangles[j];
            Vec3 p0 = runtime_scene_3d_builder_transform_mesh_vertex(
                &document->vertices[src->a],
                instance,
                pivot);
            Vec3 p1 = runtime_scene_3d_builder_transform_mesh_vertex(
                &document->vertices[src->b],
                instance,
                pivot);
            Vec3 p2 = runtime_scene_3d_builder_transform_mesh_vertex(
                &document->vertices[src->c],
                instance,
                pivot);
            Vec3 tex0 = runtime_scene_3d_builder_object_texture_coord(p0, &bounds);
            Vec3 tex1 = runtime_scene_3d_builder_object_texture_coord(p1, &bounds);
            Vec3 tex2 = runtime_scene_3d_builder_object_texture_coord(p2, &bounds);
            Vec3 expected_normal = vec3_cross(vec3_sub(p1, p0), vec3_sub(p2, p0));
            double normal_len = vec3_length(expected_normal);
            if (normal_len <= 1e-18) continue;
            expected_normal = vec3_scale(expected_normal, 1.0 / normal_len);
            if (!runtime_scene_3d_builder_append_triangle_internal(scene,
                                                                   primitive_index,
                                                                   scene_object_index,
                                                                   p0,
                                                                   p1,
                                                                   p2,
                                                                   expected_normal,
                                                                   (int)j,
                                                                   false,
                                                                   bounds.valid,
                                                                   tex0,
                                                                   tex1,
                                                                   tex2)) {
                runtime_scene_3d_builder_set_diag("append mesh assets failed: triangle append failed");
                RuntimeScene3D_Reset(scene);
                return false;
            }
            if (runtime_scene_3d_builder_smooth_mesh_shading_enabled() &&
                document->vertex_normal_count == document->vertex_count) {
                RuntimeTriangle3D* appended =
                    &scene->triangleMesh.triangles[scene->triangleMesh.triangleCount - 1];
                appended->vertexNormal0 = runtime_scene_3d_builder_transform_mesh_normal(
                    &document->vertices[src->a], instance);
                appended->vertexNormal1 = runtime_scene_3d_builder_transform_mesh_normal(
                    &document->vertices[src->b], instance);
                appended->vertexNormal2 = runtime_scene_3d_builder_transform_mesh_normal(
                    &document->vertices[src->c], instance);
                appended->hasVertexNormals =
                    vec3_length(appended->vertexNormal0) > 1e-9 &&
                    vec3_length(appended->vertexNormal1) > 1e-9 &&
                    vec3_length(appended->vertexNormal2) > 1e-9;
            }
            appended_triangle_count += 1;
        }
        runtime_scene_3d_builder_timing_mutable()->mesh_append_triangles_appended +=
            appended_triangle_count;
        if (appended_triangle_count <= 0) continue;
        scene->primitiveCount += 1;
    }
    runtime_scene_3d_builder_timing_mutable()->mesh_append_expand_ms +=
        runtime_scene_3d_builder_elapsed_ms_since(&stage_start);
    scene->scope.triangleMeshEnabled = true;
    if (require_ready_bvh) {
        if (!runtime_scene_3d_builder_rebuild_prepared_accel(scene, mesh_assets) ||
            !runtime_scene_3d_builder_rebuild_bvh(scene)) {
            RuntimeScene3D_Reset(scene);
            return false;
        }
    }
    runtime_scene_3d_builder_timing_mutable()->mesh_append_total_ms +=
        runtime_scene_3d_builder_elapsed_ms_since(&total_start);
    return true;
}

bool runtime_scene_3d_builder_append_mesh_asset_set(
    RuntimeScene3D* scene,
    const RayTracingRuntimeMeshAssetSet* mesh_assets,
    bool require_ready_bvh) {
    return runtime_scene_3d_builder_append_mesh_asset_set_at_t(scene,
                                                              mesh_assets,
                                                              require_ready_bvh,
                                                              0.0);
}

bool RuntimeScene3DBuilder_AppendMeshAssetSet(
    RuntimeScene3D* scene,
    const RayTracingRuntimeMeshAssetSet* mesh_assets) {
    return runtime_scene_3d_builder_append_mesh_asset_set(scene, mesh_assets, true);
}
