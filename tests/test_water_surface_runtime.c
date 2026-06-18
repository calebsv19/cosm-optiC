#include "test_water_surface_runtime.h"

#include <string.h>

#include "render/runtime_scene_3d.h"
#include "render/runtime_scene_3d_builder.h"
#include "test_support.h"

static int test_water_surface_runtime_appends_heightfield_surface(void) {
    RuntimeScene3D scene;
    RuntimeScene3DHeightfieldSurfaceDesc desc = {0};
    float heights[9] = {
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.45f,
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
                scene.triangleMesh.triangleCount == 6);
    assert_true("water_surface_runtime_heightfield_reported_count",
                appended_triangle_count == 6);
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

int run_test_water_surface_runtime_tests(void) {
    int before = test_support_failures();
    test_water_surface_runtime_appends_heightfield_surface();
    test_water_surface_runtime_maps_physics_y_height_to_scene_z();
    return test_support_failures() - before;
}
