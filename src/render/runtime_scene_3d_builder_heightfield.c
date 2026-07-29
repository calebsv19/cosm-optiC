#include "render/runtime_scene_3d_builder_internal.h"

static bool runtime_scene_3d_builder_heightfield_sample_is_dry(
    const RuntimeScene3DHeightfieldSurfaceDesc* desc,
    uint32_t x,
    uint32_t z) {
    const double threshold =
        desc ? desc->dry_height + desc->dry_height_epsilon : 0.0;
    if (!desc || !desc->heights_y || x >= desc->grid_w ||
        z >= desc->grid_d) {
        return true;
    }
    return desc->heights_y[(size_t)z * (size_t)desc->grid_w + (size_t)x] <=
           threshold;
}

static bool runtime_scene_3d_builder_heightfield_sample_is_perimeter(
    const RuntimeScene3DHeightfieldSurfaceDesc* desc,
    uint32_t x,
    uint32_t z) {
    return desc && (x == 0u || z == 0u || x + 1u == desc->grid_w ||
                    z + 1u == desc->grid_d);
}

static bool runtime_scene_3d_builder_heightfield_sample_blocks_quad(
    const RuntimeScene3DHeightfieldSurfaceDesc* desc,
    uint32_t x,
    uint32_t z) {
    if (!runtime_scene_3d_builder_heightfield_sample_is_dry(desc, x, z)) {
        return false;
    }
    return !desc->close_dry_perimeter ||
           !runtime_scene_3d_builder_heightfield_sample_is_perimeter(desc,
                                                                     x,
                                                                     z);
}

static bool runtime_scene_3d_builder_heightfield_quad_is_dry(
    const RuntimeScene3DHeightfieldSurfaceDesc* desc,
    uint32_t x,
    uint32_t z) {
    if (!desc || !desc->heights_y || desc->grid_w == 0u) return true;
    if (!desc->skip_dry_quads) return false;
    return runtime_scene_3d_builder_heightfield_sample_blocks_quad(desc, x, z) ||
           runtime_scene_3d_builder_heightfield_sample_blocks_quad(desc,
                                                                    x + 1u,
                                                                    z) ||
           runtime_scene_3d_builder_heightfield_sample_blocks_quad(desc,
                                                                    x + 1u,
                                                                    z + 1u) ||
           runtime_scene_3d_builder_heightfield_sample_blocks_quad(desc,
                                                                    x,
                                                                    z + 1u);
}

static int runtime_scene_3d_builder_heightfield_quad_count(
    const RuntimeScene3DHeightfieldSurfaceDesc* desc) {
    int count = 0;
    if (!desc || desc->grid_w < 2u || desc->grid_d < 2u || !desc->heights_y) return 0;
    for (uint32_t z = 0u; z + 1u < desc->grid_d; ++z) {
        for (uint32_t x = 0u; x + 1u < desc->grid_w; ++x) {
            if (!runtime_scene_3d_builder_heightfield_quad_is_dry(desc, x, z)) {
                count += 1;
            }
        }
    }
    return count;
}

static Vec3 runtime_scene_3d_builder_heightfield_point(
    const RuntimeScene3DHeightfieldSurfaceDesc* desc,
    uint32_t x,
    uint32_t z) {
    const size_t index = (size_t)z * (size_t)desc->grid_w + (size_t)x;
    const double sample_x = desc->sample_origin_x + ((double)x * desc->sample_spacing_x);
    const double sample_z = desc->sample_origin_z + ((double)z * desc->sample_spacing_z);
    double height_y = desc->heights_y[index];
    if (desc->close_dry_perimeter &&
        runtime_scene_3d_builder_heightfield_sample_is_perimeter(desc, x, z) &&
        runtime_scene_3d_builder_heightfield_sample_is_dry(desc, x, z)) {
        height_y = desc->closed_perimeter_height;
    }
    if (desc->map_y_height_to_scene_z) {
        return vec3(sample_x, sample_z, height_y);
    }
    return vec3(sample_x, height_y, sample_z);
}

static bool runtime_scene_3d_builder_heightfield_desc_valid(
    const RuntimeScene3DHeightfieldSurfaceDesc* desc) {
    if (!desc || !desc->heights_y) return false;
    if (desc->scene_object_index < 0 || desc->grid_w < 2u || desc->grid_d < 2u) return false;
    if (!(desc->sample_spacing_x > 0.0) || !(desc->sample_spacing_z > 0.0)) return false;
    if (!isfinite(desc->sample_origin_x) ||
        !isfinite(desc->sample_origin_z) ||
        !isfinite(desc->sample_spacing_x) ||
        !isfinite(desc->sample_spacing_z) ||
        !isfinite(desc->dry_height) ||
        !isfinite(desc->dry_height_epsilon) ||
        (desc->close_dry_perimeter &&
         !isfinite(desc->closed_perimeter_height))) {
        return false;
    }
    for (uint64_t i = 0u; i < (uint64_t)desc->grid_w * (uint64_t)desc->grid_d; ++i) {
        if (!isfinite(desc->heights_y[i])) return false;
    }
    return true;
}

bool RuntimeScene3DBuilder_AppendHeightfieldSurface(
    RuntimeScene3D* scene,
    const RuntimeScene3DHeightfieldSurfaceDesc* desc,
    int* out_appended_triangle_count) {
    const int old_primitive_count = scene ? scene->primitiveCount : 0;
    const int old_triangle_count = scene ? scene->triangleMesh.triangleCount : 0;
    const int valid_quad_count =
        runtime_scene_3d_builder_heightfield_quad_count(desc);
    const int extra_triangle_count = valid_quad_count * 2;
    RuntimePrimitive3D* primitive = NULL;
    int primitive_index = 0;
    int appended_triangle_count = 0;

    if (out_appended_triangle_count) *out_appended_triangle_count = 0;
    if (!scene || !runtime_scene_3d_builder_heightfield_desc_valid(desc)) return false;
    if (valid_quad_count <= 0) return true;
    if (!runtime_scene_3d_builder_reserve_primitives(scene, scene->primitiveCount + 1) ||
        !runtime_scene_3d_builder_reserve_triangles(scene,
                                                    scene->triangleMesh.triangleCount +
                                                        extra_triangle_count)) {
        return false;
    }

    primitive_index = scene->primitiveCount;
    primitive = &scene->primitives[primitive_index];
    memset(primitive, 0, sizeof(*primitive));
    primitive->kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    primitive->source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    primitive->source.sceneObjectIndex = desc->scene_object_index;
    snprintf(primitive->source.objectId,
             sizeof(primitive->source.objectId),
             "%s",
             (desc->object_id && desc->object_id[0]) ? desc->object_id : "water_surface");

    for (uint32_t z = 0u; z + 1u < desc->grid_d; ++z) {
        for (uint32_t x = 0u; x + 1u < desc->grid_w; ++x) {
            Vec3 p00;
            Vec3 p10;
            Vec3 p11;
            Vec3 p01;
            const Vec3 expected_normal = desc->map_y_height_to_scene_z
                                             ? vec3(0.0, 0.0, 1.0)
                                             : vec3(0.0, 1.0, 0.0);
            if (runtime_scene_3d_builder_heightfield_quad_is_dry(desc, x, z)) {
                continue;
            }
            p00 = runtime_scene_3d_builder_heightfield_point(desc, x, z);
            p10 = runtime_scene_3d_builder_heightfield_point(desc, x + 1u, z);
            p11 = runtime_scene_3d_builder_heightfield_point(desc, x + 1u, z + 1u);
            p01 = runtime_scene_3d_builder_heightfield_point(desc, x, z + 1u);
            if (!runtime_scene_3d_builder_append_quad(scene,
                                                      primitive_index,
                                                      desc->scene_object_index,
                                                      p00,
                                                      p01,
                                                      p11,
                                                      p10,
                                                      expected_normal,
                                                      desc->two_sided)) {
                scene->primitiveCount = old_primitive_count;
                scene->triangleMesh.triangleCount = old_triangle_count;
                scene->triangleMesh.bvhDirty = true;
                (void)runtime_scene_3d_builder_rebuild_bvh(scene);
                return false;
            }
            appended_triangle_count += 2;
        }
    }

    if (appended_triangle_count <= 0) {
        scene->primitiveCount = old_primitive_count;
        scene->triangleMesh.triangleCount = old_triangle_count;
        scene->triangleMesh.bvhDirty = true;
        return runtime_scene_3d_builder_rebuild_bvh(scene);
    }

    scene->primitiveCount += 1;
    scene->scope.triangleMeshEnabled = true;
    if (!runtime_scene_3d_builder_rebuild_bvh(scene)) {
        scene->primitiveCount = old_primitive_count;
        scene->triangleMesh.triangleCount = old_triangle_count;
        scene->triangleMesh.bvhDirty = true;
        (void)runtime_scene_3d_builder_rebuild_bvh(scene);
        return false;
    }
    if (out_appended_triangle_count) {
        *out_appended_triangle_count = appended_triangle_count;
    }
    return true;
}
