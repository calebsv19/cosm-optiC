#include "test_water_surface_runtime.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "render/runtime_dynamic_geometry_accel_3d.h"
#include "import/water_surface_import.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_scene_3d_builder.h"
#include "render/runtime_water_body_mesh_3d.h"
#include "render/runtime_water_body_prepare_3d.h"
#include "test_support.h"

static int test_water_surface_runtime_appends_heightfield_surface(void) {
    RuntimeScene3D scene;
    RuntimeScene3DHeightfieldSurfaceDesc desc = {0};
    float heights[9] = {
        0.0f, 0.0f, 0.0f,
        0.0f, 0.45f, 0.48f,
        0.0f, 0.50f, 0.60f
    };
    int appended_triangle_count = 0;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    desc.object_id = "water_surface";
    desc.scene_object_index = 7;
    desc.grid_w = 3u;
    desc.grid_d = 3u;
    desc.heights_y = heights;
    desc.sample_origin_x = -1.0;
    desc.sample_origin_z = -2.0;
    desc.sample_spacing_x = 0.5;
    desc.sample_spacing_z = 0.25;
    desc.dry_height = 0.0;
    desc.dry_height_epsilon = 1e-6;
    desc.skip_dry_quads = true;
    desc.two_sided = true;

    ok = RuntimeScene3DBuilder_AppendHeightfieldSurface(&scene,
                                                        &desc,
                                                        &appended_triangle_count);
    assert_true("water_surface_runtime_heightfield_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }
    assert_true("water_surface_runtime_heightfield_primitive_count",
                scene.primitiveCount == 1);
    assert_true("water_surface_runtime_heightfield_triangle_count",
                scene.triangleMesh.triangleCount == 2);
    assert_true("water_surface_runtime_heightfield_reported_count",
                appended_triangle_count == 2);
    assert_true("water_surface_runtime_heightfield_source_index",
                scene.primitives[0].source.sceneObjectIndex == 7);
    assert_true("water_surface_runtime_heightfield_object_id",
                strcmp(scene.primitives[0].source.objectId, "water_surface") == 0);
    assert_true("water_surface_runtime_heightfield_triangle_kind",
                scene.primitives[0].kind == RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH);
    assert_true("water_surface_runtime_heightfield_y_up",
                scene.triangleMesh.triangles[0].normal.y > 0.0);
    assert_true("water_surface_runtime_heightfield_two_sided",
                scene.triangleMesh.triangles[0].twoSided);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_water_surface_runtime_skips_cutout_boundary_quads(void) {
    RuntimeScene3D scene;
    RuntimeScene3DHeightfieldSurfaceDesc desc = {0};
    float heights[9] = {
        0.80f, 0.82f, 0.81f,
        0.79f, 0.00f, 0.83f,
        0.78f, 0.80f, 0.82f
    };
    int appended_triangle_count = 0;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    desc.object_id = "water_surface";
    desc.scene_object_index = 5;
    desc.grid_w = 3u;
    desc.grid_d = 3u;
    desc.heights_y = heights;
    desc.sample_origin_x = 0.0;
    desc.sample_origin_z = 0.0;
    desc.sample_spacing_x = 0.25;
    desc.sample_spacing_z = 0.25;
    desc.dry_height = 0.0;
    desc.dry_height_epsilon = 1e-6;
    desc.skip_dry_quads = true;
    desc.two_sided = true;

    ok = RuntimeScene3DBuilder_AppendHeightfieldSurface(&scene,
                                                        &desc,
                                                        &appended_triangle_count);
    assert_true("water_surface_runtime_cutout_boundary_ok", ok);
    assert_true("water_surface_runtime_cutout_boundary_no_primitive",
                scene.primitiveCount == 0);
    assert_true("water_surface_runtime_cutout_boundary_no_triangles",
                scene.triangleMesh.triangleCount == 0);
    assert_true("water_surface_runtime_cutout_boundary_reported_count",
                appended_triangle_count == 0);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_water_surface_runtime_maps_physics_y_height_to_scene_z(void) {
    RuntimeScene3D scene;
    RuntimeScene3DHeightfieldSurfaceDesc desc = {0};
    float heights[4] = {
        0.20f, 0.24f,
        0.31f, 0.36f
    };
    int appended_triangle_count = 0;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    desc.object_id = "water_surface";
    desc.scene_object_index = 3;
    desc.grid_w = 2u;
    desc.grid_d = 2u;
    desc.heights_y = heights;
    desc.sample_origin_x = 10.0;
    desc.sample_origin_z = 20.0;
    desc.sample_spacing_x = 0.5;
    desc.sample_spacing_z = 0.25;
    desc.dry_height = 0.0;
    desc.dry_height_epsilon = 1e-6;
    desc.skip_dry_quads = false;
    desc.two_sided = true;
    desc.map_y_height_to_scene_z = true;

    ok = RuntimeScene3DBuilder_AppendHeightfieldSurface(&scene,
                                                        &desc,
                                                        &appended_triangle_count);
    assert_true("water_surface_runtime_scene_z_mapping_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    assert_true("water_surface_runtime_scene_z_mapping_triangle_count",
                scene.triangleMesh.triangleCount == 2);
    assert_true("water_surface_runtime_scene_z_mapping_reported_count",
                appended_triangle_count == 2);
    assert_close("water_surface_runtime_scene_z_mapping_p0_x",
                 scene.triangleMesh.triangles[0].p0.x,
                 10.0,
                 1e-9);
    assert_close("water_surface_runtime_scene_z_mapping_p0_y_uses_sample_z",
                 scene.triangleMesh.triangles[0].p0.y,
                 20.0,
                 1e-9);
    assert_close("water_surface_runtime_scene_z_mapping_p0_z_uses_height",
                 scene.triangleMesh.triangles[0].p0.z,
                 0.20,
                 1e-6);
    assert_true("water_surface_runtime_scene_z_mapping_z_up",
                scene.triangleMesh.triangles[0].normal.z > 0.0);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_water_surface_runtime_closes_authored_dry_perimeter(void) {
    RuntimeScene3D scene;
    RuntimeScene3DHeightfieldSurfaceDesc desc = {0};
    float heights[9] = {
        0.0f, 0.0f, 0.0f,
        0.0f, 0.52f, 0.0f,
        0.0f, 0.0f, 0.0f
    };
    int appended_triangle_count = 0;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    desc.object_id = "water_surface";
    desc.scene_object_index = 4;
    desc.grid_w = 3u;
    desc.grid_d = 3u;
    desc.heights_y = heights;
    desc.sample_origin_x = -1.0;
    desc.sample_origin_z = -1.0;
    desc.sample_spacing_x = 1.0;
    desc.sample_spacing_z = 1.0;
    desc.dry_height = 0.0;
    desc.dry_height_epsilon = 1e-6;
    desc.skip_dry_quads = true;
    desc.close_dry_perimeter = true;
    desc.closed_perimeter_height = 0.4;
    desc.two_sided = false;
    desc.map_y_height_to_scene_z = true;

    ok = RuntimeScene3DBuilder_AppendHeightfieldSurface(
        &scene, &desc, &appended_triangle_count);
    assert_true("water_surface_runtime_closed_perimeter_ok", ok);
    assert_true("water_surface_runtime_closed_perimeter_triangle_count",
                scene.triangleMesh.triangleCount == 8 &&
                    appended_triangle_count == 8);
    if (ok && scene.triangleMesh.triangleCount > 0) {
        assert_close("water_surface_runtime_closed_perimeter_height",
                     scene.triangleMesh.triangles[0].p0.z,
                     0.4,
                     1e-9);
        assert_true("water_surface_runtime_closed_perimeter_outward_only",
                    !scene.triangleMesh.triangles[0].twoSided &&
                        scene.triangleMesh.triangles[0].normal.z > 0.0);
    }
    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_water_surface_runtime_builds_dynamic_closed_volume(void) {
    RuntimeScene3D scene;
    RuntimeScene3DHeightfieldSurfaceDesc desc = {0};
    float heights[9] = {
        0.0f, 0.0f, 0.0f,
        0.0f, 0.52f, 0.61f,
        0.0f, 0.57f, 0.66f
    };
    int appended_triangle_count = 0;
    bool ok = false;
    bool found_dynamic_wall_top = false;
    bool found_bottom = false;

    RuntimeScene3D_Init(&scene);
    desc.object_id = "water_surface";
    desc.scene_object_index = 4;
    desc.grid_w = 3u;
    desc.grid_d = 3u;
    desc.heights_y = heights;
    desc.sample_origin_x = -1.0;
    desc.sample_origin_z = -1.0;
    desc.sample_spacing_x = 1.0;
    desc.sample_spacing_z = 1.0;
    desc.dry_height = 0.0;
    desc.dry_height_epsilon = 1e-6;
    desc.skip_dry_quads = true;
    desc.extend_dry_perimeter_from_interior = true;
    desc.close_volume_to_bottom = true;
    desc.closed_volume_bottom_height = 0.08;
    desc.two_sided = false;
    desc.map_y_height_to_scene_z = true;

    ok = RuntimeScene3DBuilder_AppendHeightfieldSurface(
        &scene, &desc, &appended_triangle_count);
    assert_true("water_surface_runtime_dynamic_closed_volume_ok", ok);
    assert_true("water_surface_runtime_dynamic_closed_volume_triangle_count",
                scene.triangleMesh.triangleCount == 26 &&
                    appended_triangle_count == 26);
    for (int i = 0; ok && i < scene.triangleMesh.triangleCount; ++i) {
        const RuntimeTriangle3D* triangle = &scene.triangleMesh.triangles[i];
        const double max_z = fmax(triangle->p0.z, fmax(triangle->p1.z, triangle->p2.z));
        const double min_z = fmin(triangle->p0.z, fmin(triangle->p1.z, triangle->p2.z));
        if (max_z > 0.60 && min_z <= 0.0800001 && fabs(triangle->normal.z) < 0.5) {
            found_dynamic_wall_top = true;
        }
        if (fabs(triangle->p0.z - 0.08) < 1e-9 &&
            fabs(triangle->p1.z - 0.08) < 1e-9 &&
            fabs(triangle->p2.z - 0.08) < 1e-9 && triangle->normal.z < 0.0) {
            found_bottom = true;
        }
    }
    assert_true("water_surface_runtime_dynamic_closed_volume_wall_follows_surface",
                found_dynamic_wall_top);
    assert_true("water_surface_runtime_dynamic_closed_volume_bottom_cap",
                found_bottom);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_water_surface_runtime_cache_preserves_scene_triangle_index(void) {
    RuntimeScene3D scene;
    Ray3D ray;
    HitInfo3D hit;
    RuntimeDynamicGeometryAcceleration3DInput input = {0};
    RuntimeDynamicGeometryAcceleration3DClassification classification = {0};
    RuntimeDynamicGeometryWaterCacheDiagnostics3D cache_before = {0};
    RuntimeDynamicGeometryWaterCacheDiagnostics3D cache_after = {0};

    RuntimeScene3D_Init(&scene);
    scene.primitiveCapacity = 2;
    scene.primitiveCount = 2;
    scene.primitives = calloc(2u, sizeof(*scene.primitives));
    scene.triangleMesh.triangleCapacity = 3;
    scene.triangleMesh.triangleCount = 3;
    scene.triangleMesh.triangles =
        calloc(3u, sizeof(*scene.triangleMesh.triangles));
    assert_true("water_surface_runtime_cache_scene_allocated",
                scene.primitives && scene.triangleMesh.triangles);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    scene.primitives[1].source.sceneObjectIndex = 7;
    snprintf(scene.primitives[1].source.objectId,
             sizeof(scene.primitives[1].source.objectId),
             "%s",
             "water_surface");
    scene.triangleMesh.triangles[2].p0 = vec3(-1.0, -1.0, 1.0);
    scene.triangleMesh.triangles[2].p1 = vec3(1.0, -1.0, 1.0);
    scene.triangleMesh.triangles[2].p2 = vec3(0.0, 1.0, 1.0);
    scene.triangleMesh.triangles[2].normal = vec3(0.0, 0.0, 1.0);
    scene.triangleMesh.triangles[2].twoSided = true;
    scene.triangleMesh.triangles[2].primitiveIndex = 1;
    scene.triangleMesh.triangles[2].sceneObjectIndex = 7;
    scene.triangleMesh.triangles[2].localTriangleIndex = 0;

    RuntimeDynamicGeometryAcceleration3D_ResetWaterCacheLifecycle();
    input.water_surface_source_found = true;
    input.water_surface_loaded = true;
    input.water_surface_frame_selection_built = true;
    input.water_surface_mesh_attached = true;
    input.water_surface_first_grid_w = 2u;
    input.water_surface_first_grid_d = 2u;
    input.water_surface_first_sample_count = 4u;
    input.water_surface_last_grid_w = 2u;
    input.water_surface_last_grid_d = 2u;
    input.water_surface_last_sample_count = 4u;
    input.water_surface_triangle_count = 1;
    RuntimeDynamicGeometryAcceleration3D_Classify(&input, &classification);
    (void)RuntimeDynamicGeometryAcceleration3D_RecordWaterSurfaceFrame(
        &classification, 0u, 1);
    assert_true("water_surface_runtime_cache_store",
                RuntimeDynamicGeometryAcceleration3D_StoreWaterSurfaceMeshFromScene(
                    &scene, 2, 1));
    RuntimeDynamicGeometryAcceleration3D_SnapshotWaterCacheDiagnostics(
        &cache_before);
    assert_true("water_surface_runtime_cache_geometry_key",
                cache_before.geometryKey != 0u);
    ray = RuntimeRay3D_Make(vec3(0.0, 0.0, 2.0), vec3(0.0, 0.0, -1.0));
    HitInfo3D_Reset(&hit);
    assert_true("water_surface_runtime_cache_trace",
                RuntimeDynamicGeometryAcceleration3D_TraceWaterSurfaceFirstHit(
                    &scene, &ray, 1.0e-6, 100.0, &hit));
    assert_true("water_surface_runtime_cache_global_triangle_index",
                hit.triangleIndex == 2 && hit.localTriangleIndex == 0 &&
                    hit.primitiveIndex == 1 && hit.sceneObjectIndex == 7);
    scene.triangleMesh.triangles[2].p0.x -= 0.25;
    assert_true("water_surface_runtime_cache_store_changed_geometry",
                RuntimeDynamicGeometryAcceleration3D_StoreWaterSurfaceMeshFromScene(
                    &scene, 2, 1));
    RuntimeDynamicGeometryAcceleration3D_SnapshotWaterCacheDiagnostics(
        &cache_after);
    assert_true("water_surface_runtime_cache_geometry_key_changes",
                cache_after.geometryKey != 0u &&
                    cache_after.geometryKey != cache_before.geometryKey);

    RuntimeDynamicGeometryAcceleration3D_ResetWaterCacheLifecycle();
    RuntimeScene3D_Free(&scene);
    return 0;
}

typedef struct WaterBodyFixtureResult {
    const char* fixture_id;
    double corner_height;
    RuntimeWaterBodyMesh3DReport report;
} WaterBodyFixtureResult;

static bool water_body_fixture_side_contains_point(const RuntimeScene3D* scene,
                                                   const RuntimeWaterBodyMesh3DReport* report,
                                                   Vec3 expected) {
    const int first_side = report ? report->top_triangle_count : 0;
    const int last_side = report ? first_side + report->side_triangle_count : 0;
    if (!scene || !report) return false;
    for (int i = first_side; i < last_side; ++i) {
        const RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[i];
        const Vec3 points[3] = {triangle->p0, triangle->p1, triangle->p2};
        for (int corner = 0; corner < 3; ++corner) {
            if (fabs(points[corner].x - expected.x) <= 1e-9 &&
                fabs(points[corner].y - expected.y) <= 1e-9 &&
                fabs(points[corner].z - expected.z) <= 1e-6) {
                return true;
            }
        }
    }
    return false;
}

static WaterBodyFixtureResult water_body_run_fixture(const char* fixture_id,
                                                     float corner_height) {
    enum { GRID_W = 16, GRID_D = 16 };
    RuntimeScene3D scene;
    RuntimeWaterBodyMesh3DDesc desc = {0};
    WaterBodyFixtureResult result = {0};
    float heights[GRID_W * GRID_D];
    bool ok = false;

    for (int i = 0; i < GRID_W * GRID_D; ++i) heights[i] = 1.0f;
    heights[0] = corner_height;
    RuntimeScene3D_Init(&scene);
    desc.object_id = "fixture_unified_water_body";
    desc.material_id = "fixture_water_material";
    desc.medium_id = "fixture_water_medium";
    desc.scene_object_index = 11;
    desc.grid_w = GRID_W;
    desc.grid_d = GRID_D;
    desc.heights_y = heights;
    desc.sample_origin_x = -1.5;
    desc.sample_origin_z = -0.75;
    desc.sample_spacing_x = 0.2;
    desc.sample_spacing_z = 0.1;
    desc.bottom_height = 0.2;
    desc.map_y_height_to_scene_z = true;

    result.fixture_id = fixture_id;
    result.corner_height = corner_height;
    ok = RuntimeWaterBodyMesh3D_Append(&scene, &desc, &result.report);
    assert_true(fixture_id, ok);
    if (ok) {
        const Vec3 expected_corner = vec3(desc.sample_origin_x,
                                          desc.sample_origin_z,
                                          corner_height);
        assert_true("water_body_one_object", result.report.object_count == 1);
        assert_true("water_body_one_component",
                    result.report.connected_component_count == 1);
        assert_true("water_body_zero_boundary_edges",
                    result.report.boundary_edge_count == 0);
        assert_true("water_body_zero_nonmanifold_edges",
                    result.report.nonmanifold_edge_count == 0);
        assert_true("water_body_positive_signed_volume",
                    result.report.signed_volume > 0.0);
        assert_true("water_body_consistent_winding", result.report.winding_consistent);
        assert_true("water_body_topology_valid", result.report.topology_valid);
        assert_true("water_body_perimeter_seam",
                    result.report.max_perimeter_seam_error <= 1e-6);
        assert_true("water_body_top_triangle_partition",
                    result.report.top_triangle_count == 450);
        assert_true("water_body_side_triangle_partition",
                    result.report.side_triangle_count == 120);
        assert_true("water_body_bottom_triangle_partition",
                    result.report.bottom_triangle_count == 450);
        assert_true("water_body_total_triangle_partition",
                    result.report.total_triangle_count == 1020);
        assert_true("water_body_single_scene_primitive", scene.primitiveCount == 1);
        assert_true("water_body_single_scene_object_index",
                    scene.primitives[0].source.sceneObjectIndex == 11);
        assert_true("water_body_object_identity",
                    strcmp(result.report.object_id, "fixture_unified_water_body") == 0);
        assert_true("water_body_material_identity",
                    strcmp(result.report.material_id, "fixture_water_material") == 0);
        assert_true("water_body_medium_identity",
                    strcmp(result.report.medium_id, "fixture_water_medium") == 0);
        assert_true("water_body_wall_follows_current_corner",
                    water_body_fixture_side_contains_point(&scene,
                                                           &result.report,
                                                           expected_corner));
    }
    RuntimeScene3D_Free(&scene);
    return result;
}

static void water_body_write_geometry_report(const WaterBodyFixtureResult* results,
                                             size_t count,
                                             bool topology_stable) {
    const char* path = getenv("WATER_BODY_GEOMETRY_REPORT");
    FILE* file = NULL;
    if (!path || !path[0] || !results || count == 0u) return;
    file = fopen(path, "wb");
    assert_true("water_body_geometry_report_open", file != NULL);
    if (!file) return;
    fprintf(file,
            "{\n  \"schema\": \"ray_tracing_water_body_geometry_report_v1\",\n"
            "  \"proof_scope\": \"local_geometry_only\",\n"
            "  \"grid\": {\"width\": 16, \"depth\": 16},\n"
            "  \"topology_stable\": %s,\n  \"fixtures\": [\n",
            topology_stable ? "true" : "false");
    for (size_t i = 0u; i < count; ++i) {
        const RuntimeWaterBodyMesh3DReport* report = &results[i].report;
        fprintf(file,
                "    {\"id\": \"%s\", \"corner_height\": %.9g, "
                "\"object_count\": %d, \"component_count\": %d, "
                "\"boundary_edges\": %d, \"nonmanifold_edges\": %d, "
                "\"signed_volume\": %.12g, \"winding_consistent\": %s, "
                "\"top_triangles\": %d, \"side_triangles\": %d, "
                "\"bottom_triangles\": %d, \"total_triangles\": %d, "
                "\"max_seam_error_m\": %.12g, \"topology_signature\": \"%016llx\", "
                "\"object_id\": \"%s\", \"material_id\": \"%s\", "
                "\"medium_id\": \"%s\", \"topology_valid\": %s}%s\n",
                results[i].fixture_id,
                results[i].corner_height,
                report->object_count,
                report->connected_component_count,
                report->boundary_edge_count,
                report->nonmanifold_edge_count,
                report->signed_volume,
                report->winding_consistent ? "true" : "false",
                report->top_triangle_count,
                report->side_triangle_count,
                report->bottom_triangle_count,
                report->total_triangle_count,
                report->max_perimeter_seam_error,
                (unsigned long long)report->topology_signature,
                report->object_id,
                report->material_id,
                report->medium_id,
                report->topology_valid ? "true" : "false",
                i + 1u < count ? "," : "");
    }
    fprintf(file, "  ]\n}\n");
    assert_true("water_body_geometry_report_close", fclose(file) == 0);
}

static int test_water_body_runtime_flat_raised_depressed_topology(void) {
    WaterBodyFixtureResult results[3];
    bool topology_stable = false;
    results[0] = water_body_run_fixture("water_body_flat_current_baseline", 1.0f);
    results[1] = water_body_run_fixture("water_body_raised_corner", 1.25f);
    results[2] = water_body_run_fixture("water_body_depressed_corner", 0.75f);
    topology_stable = results[0].report.topology_valid &&
                      results[1].report.topology_valid &&
                      results[2].report.topology_valid &&
                      results[0].report.topology_signature ==
                          results[1].report.topology_signature &&
                      results[0].report.topology_signature ==
                          results[2].report.topology_signature &&
                      results[0].report.total_triangle_count ==
                          results[1].report.total_triangle_count &&
                      results[0].report.total_triangle_count ==
                          results[2].report.total_triangle_count;
    assert_true("water_body_deterministic_topology_stable", topology_stable);
    water_body_write_geometry_report(results, 3u, topology_stable);
    return 0;
}

static int test_water_body_boundary_import_contract(void) {
    RuntimeWaterSurfaceFrame frame;
    char diagnostics[256] = {0};
    bool found = false;
    bool ok = false;

    RuntimeWaterSurfaceFrame_Init(&frame);
    ok = RuntimeWaterSurfaceImport_LoadSourceAtFrame(
        "tests/fixtures/water_body_boundary/manifest_legacy.json",
        0,
        &frame,
        &found,
        diagnostics,
        sizeof(diagnostics));
    assert_true("water_body_boundary_legacy_loads", ok && found && frame.valid);
    assert_true("water_body_boundary_legacy_absent",
                !frame.water_body_boundary.present);
    RuntimeWaterSurfaceFrame_Reset(&frame);

    ok = RuntimeWaterSurfaceImport_LoadSourceAtFrame(
        "tests/fixtures/water_body_boundary/manifest_valid.json",
        0,
        &frame,
        &found,
        diagnostics,
        sizeof(diagnostics));
    assert_true("water_body_boundary_valid_loads", ok && found && frame.valid);
    assert_true("water_body_boundary_valid_present", frame.water_body_boundary.present);
    assert_true("water_body_boundary_valid_closure_mode",
                strcmp(frame.water_body_boundary.closure_mode, "heightfield_volume") == 0);
    assert_true("water_body_boundary_valid_identity",
                strcmp(frame.water_body_boundary.body_id,
                       frame.water_body_boundary.object_id) == 0);
    RuntimeWaterSurfaceFrame_Reset(&frame);

    ok = RuntimeWaterSurfaceImport_LoadSourceAtFrame(
        "tests/fixtures/water_body_boundary/manifest_invalid_bounds.json",
        0,
        &frame,
        &found,
        diagnostics,
        sizeof(diagnostics));
    assert_true("water_body_boundary_invalid_rejected", !ok);
    assert_true("water_body_boundary_invalid_diag_exact",
                strcmp(diagnostics,
                       "water_body_boundary_v1 container bounds inverted") == 0);
    assert_true("water_body_boundary_invalid_reset", !frame.valid && !frame.heights_y);
    ok = RuntimeWaterSurfaceImport_LoadSourceAtFrame(
        "tests/fixtures/water_body_boundary/manifest_invalid_classification.json",
        0,
        &frame,
        &found,
        diagnostics,
        sizeof(diagnostics));
    assert_true("water_body_boundary_ambiguous_classification_rejected", !ok);
    assert_true("water_body_boundary_ambiguous_classification_diag_exact",
                strcmp(diagnostics,
                       "water_body_boundary_v1 classification metadata unsupported") == 0);
    RuntimeWaterSurfaceFrame_Free(&frame);
    return 0;
}

static void water_body_boundary_seed_scene(RuntimeScene3D* scene) {
    RuntimeScene3D_Init(scene);
    scene->primitives = (RuntimePrimitive3D*)calloc(2u, sizeof(*scene->primitives));
    assert_true("water_body_boundary_seed_alloc", scene->primitives != NULL);
    if (!scene->primitives) return;
    scene->primitiveCount = 2;
    scene->primitiveCapacity = 2;
    scene->primitives[0].kind = RUNTIME_PRIMITIVE_3D_KIND_RECT_PRISM;
    scene->primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_RECT_PRISM;
    snprintf(scene->primitives[0].source.objectId,
             sizeof(scene->primitives[0].source.objectId),
             "%s",
             "fixture_glass_container");
    scene->primitives[1].kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    scene->primitives[1].source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    snprintf(scene->primitives[1].source.objectId,
             sizeof(scene->primitives[1].source.objectId),
             "%s",
             "fixture_legacy_water_shell");
}

static void water_body_boundary_write_report(const RuntimeWaterBodyPrepare3DReport* report) {
    const char* path = getenv("WATER_BODY_BOUNDARY_REPORT");
    FILE* file = NULL;
    if (!path || !path[0] || !report) return;
    file = fopen(path, "wb");
    assert_true("water_body_boundary_report_open", file != NULL);
    if (!file) return;
    fprintf(file,
            "{\n  \"schema\": \"ray_tracing_water_body_boundary_report_v1\",\n"
            "  \"proof_scope\": \"local_contract_and_geometry_only\",\n"
            "  \"closure_mode\": \"%s\",\n  \"body_id\": \"%s\",\n"
            "  \"container_id\": \"%s\",\n  \"material_id\": \"%s\",\n"
            "  \"medium_id\": \"%s\",\n  \"selected_frame\": %llu,\n"
            "  \"bounds_m\": {\"min_x\": %.9g, \"max_x\": %.9g, "
            "\"min_y\": %.9g, \"max_y\": %.9g, \"min_z\": %.9g, \"max_z\": %.9g},\n"
            "  \"sample_classification\": {\"wet\": %u, \"dry_container\": %u, "
            "\"solid_occluder\": %u},\n"
            "  \"legacy_shell_suppressed\": %s,\n"
            "  \"material_parity_valid\": %s,\n"
            "  \"topology\": {\"components\": %d, \"boundary_edges\": %d, "
            "\"nonmanifold_edges\": %d, \"signed_volume\": %.12g, "
            "\"max_seam_error_m\": %.12g, \"valid\": %s}\n}\n",
            report->closure_mode,
            report->body_id,
            report->container_id,
            report->material_id,
            report->medium_id,
            (unsigned long long)report->selected_frame_index,
            report->min_x, report->max_x, report->min_y, report->max_y,
            report->min_z, report->max_z,
            report->wet_sample_count,
            report->dry_container_sample_count,
            report->solid_occluder_sample_count,
            report->legacy_shell_suppressed ? "true" : "false",
            report->material_parity_valid ? "true" : "false",
            report->geometry.connected_component_count,
            report->geometry.boundary_edge_count,
            report->geometry.nonmanifold_edge_count,
            report->geometry.signed_volume,
            report->geometry.max_perimeter_seam_error,
            report->geometry.topology_valid ? "true" : "false");
    assert_true("water_body_boundary_report_close", fclose(file) == 0);
}

static int test_water_body_boundary_closed_route_and_fail_closed(void) {
    RuntimeWaterSurfaceFrame frame;
    RuntimeScene3D scene;
    RuntimeWaterBodyPrepare3DReport report = {0};
    char diagnostics[256] = {0};
    bool found = false;
    bool ok = false;

    RuntimeWaterSurfaceFrame_Init(&frame);
    ok = RuntimeWaterSurfaceImport_LoadSourceAtFrame(
        "tests/fixtures/water_body_boundary/manifest_valid.json",
        0,
        &frame,
        &found,
        diagnostics,
        sizeof(diagnostics));
    assert_true("water_body_boundary_prepare_fixture_load", ok && found && frame.valid);
    frame.solid_columns = 7u;
    water_body_boundary_seed_scene(&scene);
    if (ok && scene.primitives) {
        ok = RuntimeWaterBodyPrepare3D_Append(&scene,
                                             &frame,
                                             17,
                                             &report,
                                             diagnostics,
                                             sizeof(diagnostics));
        assert_true("water_body_boundary_prepare_ok", ok);
        assert_true("water_body_boundary_one_body_component",
                    report.geometry.connected_component_count == 1);
        assert_true("water_body_boundary_closed_topology",
                    report.geometry.topology_valid &&
                    report.geometry.boundary_edge_count == 0 &&
                    report.geometry.nonmanifold_edge_count == 0);
        assert_true("water_body_boundary_shell_suppressed",
                    report.legacy_shell_suppressed && scene.primitiveCount == 2);
        assert_true("water_body_boundary_classification",
                    report.wet_sample_count == 15u &&
                    report.dry_container_sample_count == 1u &&
                    report.solid_occluder_sample_count == 0u);
        assert_true("water_body_boundary_legacy_solid_stat_is_overlapping",
                    frame.solid_columns == 7u);
        assert_true("water_body_boundary_material_parity", report.material_parity_valid);
        assert_true("water_body_boundary_perimeter_reaches_bounds",
                    report.geometry.max_perimeter_seam_error <= 1e-6);
        water_body_boundary_write_report(&report);
    }
    RuntimeScene3D_Free(&scene);

    water_body_boundary_seed_scene(&scene);
    if (scene.primitives) {
        const int primitive_count = scene.primitiveCount;
        const int triangle_count = scene.triangleMesh.triangleCount;
        frame.heights_y[1] = 2.0f;
        ok = RuntimeWaterBodyPrepare3D_Append(&scene,
                                             &frame,
                                             17,
                                             &report,
                                             diagnostics,
                                             sizeof(diagnostics));
        assert_true("water_body_boundary_runtime_invalid_rejected", !ok);
        assert_true("water_body_boundary_runtime_invalid_diag_exact",
                    strcmp(diagnostics,
                           "water body sample height outside validated container") == 0);
        assert_true("water_body_boundary_runtime_invalid_no_partial_mutation",
                    scene.primitiveCount == primitive_count &&
                    scene.triangleMesh.triangleCount == triangle_count);
    }
    RuntimeScene3D_Free(&scene);
    RuntimeWaterSurfaceFrame_Free(&frame);
    return 0;
}

int run_test_water_surface_runtime_tests(void) {
    int before = test_support_failures();
    test_water_surface_runtime_appends_heightfield_surface();
    test_water_surface_runtime_skips_cutout_boundary_quads();
    test_water_surface_runtime_maps_physics_y_height_to_scene_z();
    test_water_surface_runtime_closes_authored_dry_perimeter();
    test_water_surface_runtime_builds_dynamic_closed_volume();
    test_water_surface_runtime_cache_preserves_scene_triangle_index();
    test_water_body_runtime_flat_raised_depressed_topology();
    test_water_body_boundary_import_contract();
    test_water_body_boundary_closed_route_and_fail_closed();
    return test_support_failures() - before;
}
