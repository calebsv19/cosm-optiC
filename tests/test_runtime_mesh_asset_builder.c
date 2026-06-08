#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config/config_manager.h"
#include "import/runtime_mesh_asset_loader.h"
#include "import/runtime_scene_bridge.h"
#include "render/runtime_scene_3d_builder.h"

static const char* kMrt0ScenePath =
    "tests/fixtures/mesh_asset_runtime_spheres/scene_runtime.json";

AnimationConfig animSettings;

static int g_failures = 0;

static void assert_true(const char* name, bool condition) {
    if (!condition) {
        printf("FAIL %-48s condition=false\n", name);
        g_failures += 1;
    }
}

static void assert_near(const char* name, double actual, double expected, double epsilon) {
    if (fabs(actual - expected) > epsilon) {
        printf("FAIL %-48s actual=%f expected=%f\n", name, actual, expected);
        g_failures += 1;
    }
}

int animation_config_environment_light_mode_clamp(int mode) {
    return mode;
}

bool RuntimeScene3DSampleAuthoredLight(double normalized_t, RuntimeLight3D* out_light) {
    (void)normalized_t;
    (void)out_light;
    return false;
}

bool RuntimeScene3DSampleAuthoredCamera(double normalized_t, RuntimeCamera3D* out_camera) {
    (void)normalized_t;
    (void)out_camera;
    return false;
}

void runtime_scene_bridge_get_last_3d_primitive_seed_state(
    RuntimeSceneBridge3DPrimitiveSeedState* out_state) {
    if (!out_state) return;
    memset(out_state, 0, sizeof(*out_state));
    out_state->valid = true;
}

bool runtime_scene_bridge_get_last_object_id_for_scene_index(int scene_index,
                                                             char* out_object_id,
                                                             size_t out_object_id_size) {
    const char* id = NULL;
    if (out_object_id && out_object_id_size > 0u) out_object_id[0] = '\0';
    if (scene_index == 3) {
        id = "obj_sphere_low";
    } else if (scene_index == 4) {
        id = "obj_sphere_medium";
    } else if (scene_index == 5) {
        id = "obj_sphere_high";
    }
    if (!id || !out_object_id || out_object_id_size == 0u) return false;
    snprintf(out_object_id, out_object_id_size, "%s", id);
    return true;
}

static void assert_low_sphere_bounds(const RuntimeScene3D* scene) {
    double min_x = 1000000.0;
    double max_x = -1000000.0;
    double min_z = 1000000.0;
    double max_z = -1000000.0;

    for (int i = 0; i < 48; ++i) {
        const RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[i];
        const Vec3 points[3] = {triangle->p0, triangle->p1, triangle->p2};
        for (int j = 0; j < 3; ++j) {
            if (points[j].x < min_x) min_x = points[j].x;
            if (points[j].x > max_x) max_x = points[j].x;
            if (points[j].z < min_z) min_z = points[j].z;
            if (points[j].z > max_z) max_z = points[j].z;
        }
    }

    assert_near("mrt3_low_sphere_min_x", min_x, -2.65, 1e-9);
    assert_near("mrt3_low_sphere_max_x", max_x, -1.35, 1e-9);
    assert_near("mrt3_low_sphere_min_z", min_z, 0.10, 1e-9);
    assert_near("mrt3_low_sphere_max_z", max_z, 1.40, 1e-9);
}

static void assert_low_sphere_object_texture_coords(const RuntimeScene3D* scene) {
    double min_x = 1000000.0;
    double max_x = -1000000.0;
    double min_y = 1000000.0;
    double max_y = -1000000.0;
    double min_z = 1000000.0;
    double max_z = -1000000.0;

    for (int i = 0; i < 48; ++i) {
        const RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[i];
        const Vec3 coords[3] = {
            triangle->objectTexture0,
            triangle->objectTexture1,
            triangle->objectTexture2
        };
        assert_true("mrt3_low_sphere_triangle_has_object_texture_coords",
                    triangle->hasObjectTextureCoords);
        for (int j = 0; j < 3; ++j) {
            assert_true("mrt3_low_sphere_object_texture_coord_in_range",
                        coords[j].x >= -1e-9 && coords[j].x <= 1.0 + 1e-9 &&
                            coords[j].y >= -1e-9 && coords[j].y <= 1.0 + 1e-9 &&
                            coords[j].z >= -1e-9 && coords[j].z <= 1.0 + 1e-9);
            if (coords[j].x < min_x) min_x = coords[j].x;
            if (coords[j].x > max_x) max_x = coords[j].x;
            if (coords[j].y < min_y) min_y = coords[j].y;
            if (coords[j].y > max_y) max_y = coords[j].y;
            if (coords[j].z < min_z) min_z = coords[j].z;
            if (coords[j].z > max_z) max_z = coords[j].z;
        }
    }

    assert_near("mrt3_low_sphere_object_texture_min_x", min_x, 0.0, 1e-9);
    assert_near("mrt3_low_sphere_object_texture_max_x", max_x, 1.0, 1e-9);
    assert_near("mrt3_low_sphere_object_texture_min_y", min_y, 0.0, 1e-9);
    assert_near("mrt3_low_sphere_object_texture_max_y", max_y, 1.0, 1e-9);
    assert_near("mrt3_low_sphere_object_texture_min_z", min_z, 0.0, 1e-9);
    assert_near("mrt3_low_sphere_object_texture_max_z", max_z, 1.0, 1e-9);
}

static int test_append_mesh_asset_set_preserves_scene_object_lookup(void) {
    RayTracingRuntimeMeshAssetSet set;
    RuntimeScene3D scene;
    char diagnostics[256] = {0};
    bool ok = false;

    ray_tracing_runtime_mesh_asset_set_init(&set);
    RuntimeScene3D_Init(&scene);
    ok = ray_tracing_runtime_mesh_assets_load_scene_file(kMrt0ScenePath,
                                                        &set,
                                                        diagnostics,
                                                        sizeof(diagnostics));
    assert_true("mrt3_load_fixture_mesh_assets", ok);
    if (ok) {
        set.instances[0].scene_object_index = 99;
        ok = RuntimeScene3DBuilder_AppendMeshAssetSet(&scene, &set);
        assert_true("mrt3_append_mesh_asset_set", ok);
        assert_true("mrt3_append_primitive_count", scene.primitiveCount == 3);
        assert_true("mrt3_append_triangle_count", scene.triangleMesh.triangleCount == 1232);
        assert_true("mrt3_append_scope_enabled", scene.scope.triangleMeshEnabled);
        assert_true("mrt3_low_primitive_kind",
                    scene.primitives[0].kind == RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH);
        assert_true("mrt3_low_object_id",
                    strcmp(scene.primitives[0].source.objectId, "obj_sphere_low") == 0);
        assert_true("mrt3_low_scene_object_index",
                    scene.primitives[0].source.sceneObjectIndex == 3);
        assert_true("mrt3_medium_scene_object_index",
                    scene.primitives[1].source.sceneObjectIndex == 4);
        assert_true("mrt3_high_scene_object_index",
                    scene.primitives[2].source.sceneObjectIndex == 5);
        assert_true("mrt3_first_triangle_lookup",
                    scene.triangleMesh.triangles[0].primitiveIndex == 0 &&
                        scene.triangleMesh.triangles[0].sceneObjectIndex == 3);
        assert_true("mrt3_medium_first_triangle_lookup",
                    scene.triangleMesh.triangles[48].primitiveIndex == 1 &&
                        scene.triangleMesh.triangles[48].sceneObjectIndex == 4);
        assert_true("mrt3_high_last_triangle_lookup",
                    scene.triangleMesh.triangles[1231].primitiveIndex == 2 &&
                        scene.triangleMesh.triangles[1231].sceneObjectIndex == 5);
        assert_low_sphere_bounds(&scene);
        assert_low_sphere_object_texture_coords(&scene);
    }

    RuntimeScene3D_Free(&scene);
    ray_tracing_runtime_mesh_asset_set_free(&set);
    return 0;
}

static int test_bridge_builder_consumes_retained_mesh_assets(void) {
    RuntimeScene3D scene;
    char diagnostics[256] = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    ok = ray_tracing_runtime_mesh_assets_load_scene_file_to_last(kMrt0ScenePath,
                                                                diagnostics,
                                                                sizeof(diagnostics));
    assert_true("mrt3_load_fixture_to_last", ok);
    if (ok) {
        ok = RuntimeScene3DBuilder_BuildFromBridgeSeeds(&scene);
        assert_true("mrt3_bridge_builder_consumes_last", ok);
        assert_true("mrt3_bridge_builder_primitive_count", scene.primitiveCount == 3);
        assert_true("mrt3_bridge_builder_triangle_count",
                    scene.triangleMesh.triangleCount == 1232);
        assert_true("mrt3_bridge_builder_material_index",
                    scene.triangleMesh.triangles[48].sceneObjectIndex == 4);
    }

    RuntimeScene3D_Free(&scene);
    ray_tracing_runtime_mesh_assets_reset_last();
    return 0;
}

int main(void) {
    test_append_mesh_asset_set_preserves_scene_object_lookup();
    test_bridge_builder_consumes_retained_mesh_assets();
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
