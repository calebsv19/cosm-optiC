#include "render/runtime_water_body_prepare_3d.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "render/runtime_ray_3d.h"
#include "render/runtime_triangle_bvh_3d.h"

static RuntimeWaterBodyPrepare3DReport g_last_report;

static void water_body_prepare_diag(char* out, size_t out_size, const char* message) {
    if (out && out_size > 0u) snprintf(out, out_size, "%s", message ? message : "unknown");
}

static bool water_body_prepare_has_object(const RuntimeScene3D* scene, const char* object_id) {
    if (!scene || !object_id || !object_id[0]) return false;
    for (int i = 0; i < scene->primitiveCount; ++i) {
        if (strcmp(scene->primitives[i].source.objectId, object_id) == 0) return true;
    }
    return false;
}

static bool water_body_prepare_remove_object(RuntimeScene3D* scene, const char* object_id) {
    int* primitive_map = NULL;
    int write_primitive = 0;
    int write_triangle = 0;
    bool removed = false;
    if (!scene || !object_id || !object_id[0] || scene->primitiveCount <= 0) return false;
    primitive_map = (int*)malloc(sizeof(*primitive_map) * (size_t)scene->primitiveCount);
    if (!primitive_map) return false;
    for (int read = 0; read < scene->primitiveCount; ++read) {
        if (strcmp(scene->primitives[read].source.objectId, object_id) == 0) {
            primitive_map[read] = -1;
            removed = true;
            continue;
        }
        primitive_map[read] = write_primitive;
        if (write_primitive != read) scene->primitives[write_primitive] = scene->primitives[read];
        write_primitive += 1;
    }
    if (!removed) {
        free(primitive_map);
        return false;
    }
    for (int read = 0; read < scene->triangleMesh.triangleCount; ++read) {
        RuntimeTriangle3D triangle = scene->triangleMesh.triangles[read];
        if (triangle.primitiveIndex < 0 || triangle.primitiveIndex >= scene->primitiveCount ||
            primitive_map[triangle.primitiveIndex] < 0) {
            continue;
        }
        triangle.primitiveIndex = primitive_map[triangle.primitiveIndex];
        scene->triangleMesh.triangles[write_triangle++] = triangle;
    }
    scene->primitiveCount = write_primitive;
    scene->triangleMesh.triangleCount = write_triangle;
    scene->triangleMesh.bvhDirty = true;
    free(primitive_map);
    return true;
}

void RuntimeWaterBodyPrepare3D_ResetLastReport(void) {
    memset(&g_last_report, 0, sizeof(g_last_report));
}

bool RuntimeWaterBodyPrepare3D_GetLastReport(RuntimeWaterBodyPrepare3DReport* out_report) {
    if (!out_report || !g_last_report.active) return false;
    *out_report = g_last_report;
    return true;
}

bool RuntimeWaterBodyPrepare3D_Append(RuntimeScene3D* scene,
                                      const RuntimeWaterSurfaceFrame* water,
                                      int scene_object_index,
                                      RuntimeWaterBodyPrepare3DReport* out_report,
                                      char* out_diagnostics,
                                      size_t out_diagnostics_size) {
    const RuntimeWaterBodyBoundaryV1* boundary = NULL;
    RuntimeWaterBodyMesh3DDesc desc = {0};
    RuntimeWaterBodyPrepare3DReport report = {0};
    RuntimeScene3D backup;
    float* resolved_heights = NULL;
    uint64_t sample_count = 0u;
    bool appended = false;

    RuntimeScene3D_Init(&backup);
    RuntimeWaterBodyPrepare3D_ResetLastReport();
    if (out_report) memset(out_report, 0, sizeof(*out_report));
    if (!scene || !water || !water->valid || scene_object_index < 0) {
        water_body_prepare_diag(out_diagnostics, out_diagnostics_size,
                                "water body prepare input invalid");
        return false;
    }
    boundary = &water->water_body_boundary;
    if (!boundary->present) {
        water_body_prepare_diag(out_diagnostics, out_diagnostics_size,
                                "water body boundary contract absent");
        return false;
    }
    if (!water->material.valid) {
        water_body_prepare_diag(out_diagnostics, out_diagnostics_size,
                                "water body material parity unavailable");
        return false;
    }
    if (!water_body_prepare_has_object(scene, boundary->container_id)) {
        water_body_prepare_diag(out_diagnostics, out_diagnostics_size,
                                "water body container object unresolved");
        return false;
    }
    if (!water_body_prepare_has_object(scene, boundary->legacy_shell_object_id)) {
        water_body_prepare_diag(out_diagnostics, out_diagnostics_size,
                                "water body legacy shell object unresolved");
        return false;
    }
    sample_count = (uint64_t)water->grid_w * (uint64_t)water->grid_d;
    if (!water->heights_y || sample_count == 0u || sample_count != water->sample_count) {
        water_body_prepare_diag(out_diagnostics, out_diagnostics_size,
                                "water body sample grid invalid");
        return false;
    }
    /* legacy_height_sentinel classifies the heightfield exclusively as wet or
       dry-container. solid_columns is an overlapping PhysicsSim audit count;
       ordinary_geometry_occlusion keeps those occluders in scene geometry. */
    if ((uint64_t)water->wet_columns + (uint64_t)water->dry_columns != sample_count) {
        water_body_prepare_diag(out_diagnostics, out_diagnostics_size,
                                "water body sample classification ambiguous");
        return false;
    }
    resolved_heights = (float*)malloc(sizeof(*resolved_heights) * (size_t)sample_count);
    if (!resolved_heights) {
        water_body_prepare_diag(out_diagnostics, out_diagnostics_size,
                                "water body resolved sample allocation failed");
        return false;
    }
    for (uint64_t i = 0u; i < sample_count; ++i) {
        const double input_height = water->heights_y[i];
        const bool dry = water->dry_columns > 0u &&
                         fabs(input_height - water->surface_min_y) <=
                             boundary->dry_height_epsilon_m;
        const double resolved_height = dry ? boundary->base_surface_height_m : input_height;
        if (!isfinite(input_height) || !isfinite(resolved_height) ||
            !(resolved_height > boundary->bottom_height_m) ||
            resolved_height < boundary->min_y || resolved_height > boundary->max_y) {
            free(resolved_heights);
            water_body_prepare_diag(out_diagnostics, out_diagnostics_size,
                                    "water body sample height outside validated container");
            return false;
        }
        resolved_heights[i] = (float)resolved_height;
        if (dry) report.dry_container_sample_count += 1u;
        else report.wet_sample_count += 1u;
    }
    if (report.wet_sample_count != water->wet_columns ||
        report.dry_container_sample_count != water->dry_columns) {
        free(resolved_heights);
        water_body_prepare_diag(out_diagnostics, out_diagnostics_size,
                                "water body sample classification counts mismatch");
        return false;
    }
    report.solid_occluder_sample_count = 0u;
    desc.object_id = boundary->object_id;
    desc.material_id = boundary->material_id;
    desc.medium_id = boundary->medium_id;
    desc.scene_object_index = scene_object_index;
    desc.grid_w = water->grid_w;
    desc.grid_d = water->grid_d;
    desc.heights_y = resolved_heights;
    desc.sample_origin_x = boundary->min_x + boundary->boundary_inset_m;
    desc.sample_origin_z = boundary->min_z + boundary->boundary_inset_m;
    desc.sample_spacing_x = (boundary->max_x - boundary->min_x -
                             2.0 * boundary->boundary_inset_m) /
                            (double)(water->grid_w - 1u);
    desc.sample_spacing_z = (boundary->max_z - boundary->min_z -
                             2.0 * boundary->boundary_inset_m) /
                            (double)(water->grid_d - 1u);
    desc.bottom_height = boundary->bottom_height_m;
    desc.map_y_height_to_scene_z = true;
    if (!RuntimeScene3D_CopyGeometryFrom(&backup, scene)) {
        free(resolved_heights);
        water_body_prepare_diag(out_diagnostics, out_diagnostics_size,
                                "water body transaction backup failed");
        return false;
    }
    appended = RuntimeWaterBodyMesh3D_Append(scene, &desc, &report.geometry);
    free(resolved_heights);
    if (!appended) {
        RuntimeScene3D_Free(&backup);
        water_body_prepare_diag(out_diagnostics, out_diagnostics_size,
                                "water body closed mesh append failed");
        return false;
    }
    if (!water_body_prepare_remove_object(scene, boundary->legacy_shell_object_id) ||
        (RuntimeRay3D_CurrentTraceRoute() != RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS &&
         !RuntimeTriangleMesh3D_BuildBVH(&scene->triangleMesh))) {
        (void)RuntimeScene3D_CopyGeometryFrom(scene, &backup);
        RuntimeScene3D_Free(&backup);
        water_body_prepare_diag(out_diagnostics, out_diagnostics_size,
                                "water body legacy shell suppression failed");
        return false;
    }
    RuntimeScene3D_Free(&backup);
    report.active = true;
    snprintf(report.closure_mode, sizeof(report.closure_mode), "%s", boundary->closure_mode);
    snprintf(report.body_id, sizeof(report.body_id), "%s", boundary->body_id);
    snprintf(report.container_id, sizeof(report.container_id), "%s", boundary->container_id);
    snprintf(report.material_id, sizeof(report.material_id), "%s", boundary->material_id);
    snprintf(report.medium_id, sizeof(report.medium_id), "%s", boundary->medium_id);
    report.selected_frame_index = water->frame_index;
    report.legacy_shell_suppressed = true;
    report.material_parity_valid = true;
    report.min_x = boundary->min_x;
    report.max_x = boundary->max_x;
    report.min_y = boundary->min_y;
    report.max_y = boundary->max_y;
    report.min_z = boundary->min_z;
    report.max_z = boundary->max_z;
    report.bottom_height_m = boundary->bottom_height_m;
    report.base_surface_height_m = boundary->base_surface_height_m;
    g_last_report = report;
    if (out_report) *out_report = report;
    water_body_prepare_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}
