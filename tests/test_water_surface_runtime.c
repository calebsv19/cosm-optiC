#include "test_water_surface_runtime.h"

#include <math.h>
#include <string.h>

#include "render/runtime_dynamic_geometry_accel_3d.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_scene_3d_builder.h"
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

int run_test_water_surface_runtime_tests(void) {
    int before = test_support_failures();
    test_water_surface_runtime_appends_heightfield_surface();
    test_water_surface_runtime_skips_cutout_boundary_quads();
    test_water_surface_runtime_maps_physics_y_height_to_scene_z();
    test_water_surface_runtime_closes_authored_dry_perimeter();
    test_water_surface_runtime_builds_dynamic_closed_volume();
    test_water_surface_runtime_cache_preserves_scene_triangle_index();
    return test_support_failures() - before;
}
