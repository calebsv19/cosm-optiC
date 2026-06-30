#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config/config_manager.h"
#include "import/runtime_mesh_asset_loader.h"
#include "import/runtime_scene_bridge.h"
#include "render/runtime_mesh_blas_cache_3d.h"
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

static void snapshot_accel_stats(RuntimeSceneAcceleration3DDiagnostics* stats) {
    RuntimeMeshBLASCache3D_SnapshotDiagnostics(stats);
    RuntimeSceneAcceleration3D_AppendTLASDiagnostics(stats);
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

static int test_mesh_blas_cache_reuses_loaded_assets(void) {
    RayTracingRuntimeMeshAssetSet set;
    RuntimeScene3D first_scene;
    RuntimeScene3D second_scene;
    RuntimeSceneAcceleration3DDiagnostics stats;
    char diagnostics[256] = {0};
    bool ok = false;

    ray_tracing_runtime_mesh_asset_set_init(&set);
    RuntimeScene3D_Init(&first_scene);
    RuntimeScene3D_Init(&second_scene);
    RuntimeMeshBLASCache3D_ResetForTests();
    RuntimeSceneAcceleration3D_ResetTLASForTests();

    ok = ray_tracing_runtime_mesh_assets_load_scene_file(kMrt0ScenePath,
                                                        &set,
                                                        diagnostics,
                                                        sizeof(diagnostics));
    assert_true("mrt3_blas_cache_load_fixture", ok);
    if (ok) {
        ok = RuntimeScene3DBuilder_AppendMeshAssetSet(&first_scene, &set);
        assert_true("mrt3_blas_cache_first_append", ok);
        snapshot_accel_stats(&stats);
        assert_true("mrt3_blas_cache_first_enabled", stats.enabled);
        assert_true("mrt3_blas_cache_first_rebuilt",
                    stats.reuseStatus == RUNTIME_SCENE_ACCEL_3D_REUSE_REBUILT);
        assert_true("mrt3_blas_cache_first_prepare_call", stats.blasPrepareCalls == 1u);
        assert_true("mrt3_blas_cache_first_misses", stats.blasCacheMisses == 3u);
        assert_true("mrt3_blas_cache_first_rebuilds", stats.blasFullRebuilds == 3u);
        assert_true("mrt3_blas_cache_first_cached_assets",
                    stats.blasCachedAssetCount == 3u);
        assert_true("mrt3_tlas_first_instances", stats.tlasInstanceCount == 3u);
        assert_true("mrt3_tlas_first_nodes", stats.tlasNodeCount == 5u);
        assert_true("mrt3_tlas_first_rebuild", stats.tlasRebuilds == 1u);
        assert_true("mrt3_tlas_first_refits", stats.tlasRefits == 0u);

        ok = RuntimeScene3DBuilder_AppendMeshAssetSet(&second_scene, &set);
        assert_true("mrt3_blas_cache_second_append", ok);
        snapshot_accel_stats(&stats);
        assert_true("mrt3_blas_cache_second_reused",
                    stats.reuseStatus == RUNTIME_SCENE_ACCEL_3D_REUSE_REUSED);
        assert_true("mrt3_blas_cache_second_prepare_call", stats.blasPrepareCalls == 2u);
        assert_true("mrt3_blas_cache_second_hits", stats.blasCacheHits == 3u);
        assert_true("mrt3_blas_cache_second_misses_stable", stats.blasCacheMisses == 3u);
        assert_true("mrt3_blas_cache_second_rebuilds_stable",
                    stats.blasFullRebuilds == 3u);
        assert_true("mrt3_blas_cache_second_cached_assets",
                    stats.blasCachedAssetCount == 3u);
        assert_true("mrt3_tlas_second_instances", stats.tlasInstanceCount == 3u);
        assert_true("mrt3_tlas_second_nodes", stats.tlasNodeCount == 5u);
        assert_true("mrt3_tlas_second_rebuilds", stats.tlasRebuilds == 2u);
    }

    RuntimeScene3D_Free(&second_scene);
    RuntimeScene3D_Free(&first_scene);
    ray_tracing_runtime_mesh_asset_set_free(&set);
    RuntimeMeshBLASCache3D_ResetForTests();
    RuntimeSceneAcceleration3D_ResetTLASForTests();
    return 0;
}

static int test_mesh_blas_cache_builds_once_for_repeated_instances(void) {
    RayTracingRuntimeMeshAssetSet set;
    RuntimeScene3D scene;
    CoreMeshAssetRuntimeDocument* document = NULL;
    RuntimeSceneAcceleration3DDiagnostics stats;
    bool ok = false;

    ray_tracing_runtime_mesh_asset_set_init(&set);
    RuntimeScene3D_Init(&scene);
    RuntimeMeshBLASCache3D_ResetForTests();
    RuntimeSceneAcceleration3D_ResetTLASForTests();

    set.asset_count = 1;
    snprintf(set.assets[0].asset_id, sizeof(set.assets[0].asset_id), "asset_reused_triangle");
    document = &set.assets[0].document;
    core_mesh_asset_runtime_document_set_vertex_count(document, 3u);
    core_mesh_asset_runtime_document_set_triangle_count(document, 1u);
    document->vertices[0].position.x = 0.0;
    document->vertices[0].position.y = 0.0;
    document->vertices[0].position.z = 0.0;
    document->vertices[1].position.x = 1.0;
    document->vertices[1].position.y = 0.0;
    document->vertices[1].position.z = 0.0;
    document->vertices[2].position.x = 0.0;
    document->vertices[2].position.y = 1.0;
    document->vertices[2].position.z = 0.0;
    document->triangles[0].a = 0u;
    document->triangles[0].b = 1u;
    document->triangles[0].c = 2u;

    set.instance_count = 2;
    for (int i = 0; i < set.instance_count; ++i) {
        snprintf(set.instances[i].object_id,
                 sizeof(set.instances[i].object_id),
                 "obj_reused_triangle_%d",
                 i);
        snprintf(set.instances[i].asset_id,
                 sizeof(set.instances[i].asset_id),
                 "asset_reused_triangle");
        set.instances[i].asset_index = 0;
        set.instances[i].scene_object_index = 20 + i;
        set.instances[i].position_x = (double)i;
        set.instances[i].scale_x = 1.0;
        set.instances[i].scale_y = 1.0;
        set.instances[i].scale_z = 1.0;
    }

    ok = RuntimeScene3DBuilder_AppendMeshAssetSet(&scene, &set);
    assert_true("mrt3_blas_cache_repeated_instances_append", ok);
    assert_true("mrt3_blas_cache_repeated_instances_primitives",
                scene.primitiveCount == 2);
    assert_true("mrt3_blas_cache_repeated_instances_triangles",
                scene.triangleMesh.triangleCount == 2);
    snapshot_accel_stats(&stats);
    assert_true("mrt3_blas_cache_repeated_instances_one_prepare",
                stats.blasPrepareCalls == 1u);
    assert_true("mrt3_blas_cache_repeated_instances_one_miss",
                stats.blasCacheMisses == 1u);
    assert_true("mrt3_blas_cache_repeated_instances_one_rebuild",
                stats.blasFullRebuilds == 1u);
    assert_true("mrt3_blas_cache_repeated_instances_one_cached_asset",
                stats.blasCachedAssetCount == 1u);
    assert_true("mrt3_tlas_repeated_instances_instances", stats.tlasInstanceCount == 2u);
    assert_true("mrt3_tlas_repeated_instances_nodes", stats.tlasNodeCount == 3u);
    assert_true("mrt3_tlas_repeated_instances_rebuild", stats.tlasRebuilds == 1u);
    assert_true("mrt3_tlas_repeated_instances_refits", stats.tlasRefits == 0u);

    RuntimeScene3D_Free(&scene);
    ray_tracing_runtime_mesh_asset_set_free(&set);
    RuntimeMeshBLASCache3D_ResetForTests();
    RuntimeSceneAcceleration3D_ResetTLASForTests();
    return 0;
}

static int test_append_mesh_asset_set_skips_degenerate_triangles(void) {
    RayTracingRuntimeMeshAssetSet set;
    RuntimeScene3D scene;
    CoreMeshAssetRuntimeDocument* document = NULL;
    bool ok = false;

    ray_tracing_runtime_mesh_asset_set_init(&set);
    RuntimeScene3D_Init(&scene);

    set.asset_count = 1;
    snprintf(set.assets[0].asset_id, sizeof(set.assets[0].asset_id), "asset_degenerate_mix");
    document = &set.assets[0].document;
    core_mesh_asset_runtime_document_set_vertex_count(document, 4u);
    core_mesh_asset_runtime_document_set_triangle_count(document, 2u);

    document->vertices[0].position.x = 0.0;
    document->vertices[0].position.y = 0.0;
    document->vertices[0].position.z = 0.0;
    document->vertices[1].position.x = 0.0;
    document->vertices[1].position.y = 0.0;
    document->vertices[1].position.z = 0.0;
    document->vertices[2].position.x = 1.0;
    document->vertices[2].position.y = 0.0;
    document->vertices[2].position.z = 0.0;
    document->vertices[3].position.x = 0.0;
    document->vertices[3].position.y = 1.0;
    document->vertices[3].position.z = 0.0;

    document->triangles[0].a = 0u;
    document->triangles[0].b = 1u;
    document->triangles[0].c = 2u;
    document->triangles[1].a = 0u;
    document->triangles[1].b = 2u;
    document->triangles[1].c = 3u;

    set.instance_count = 1;
    snprintf(set.instances[0].object_id, sizeof(set.instances[0].object_id), "obj_degenerate_mix");
    snprintf(set.instances[0].asset_id, sizeof(set.instances[0].asset_id), "asset_degenerate_mix");
    set.instances[0].asset_index = 0;
    set.instances[0].scene_object_index = 7;
    set.instances[0].scale_x = 1.0;
    set.instances[0].scale_y = 1.0;
    set.instances[0].scale_z = 1.0;

    ok = RuntimeScene3DBuilder_AppendMeshAssetSet(&scene, &set);
    assert_true("mrt3_append_degenerate_mix_ok", ok);
    assert_true("mrt3_append_degenerate_mix_primitive_count", scene.primitiveCount == 1);
    assert_true("mrt3_append_degenerate_mix_triangle_count",
                scene.triangleMesh.triangleCount == 1);
    if (scene.triangleMesh.triangleCount == 1) {
        assert_true("mrt3_append_degenerate_mix_local_triangle_index",
                    scene.triangleMesh.triangles[0].localTriangleIndex == 1);
        assert_true("mrt3_append_degenerate_mix_scene_object_index",
                    scene.triangleMesh.triangles[0].sceneObjectIndex == 7);
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
    test_mesh_blas_cache_reuses_loaded_assets();
    test_mesh_blas_cache_builds_once_for_repeated_instances();
    test_append_mesh_asset_set_skips_degenerate_triangles();
    test_bridge_builder_consumes_retained_mesh_assets();
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
