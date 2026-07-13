#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config/config_manager.h"
#include "import/runtime_mesh_asset_loader.h"
#include "import/runtime_scene_bridge.h"
#include "render/runtime_mesh_accel_pack_3d.h"
#include "render/runtime_mesh_blas_cache_3d.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_accel_3d.h"
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

void runtime_scene_bridge_get_last_3d_scaffold_state(
    RuntimeSceneBridge3DScaffoldState* out_state) {
    if (!out_state) return;
    memset(out_state, 0, sizeof(*out_state));
    out_state->valid = true;
}

void runtime_scene_bridge_get_last_3d_light_seed_state(
    RuntimeSceneBridge3DLightSeedState* out_state) {
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

static void assert_hit_identity_matches(const char* prefix,
                                        const HitInfo3D* expected,
                                        const HitInfo3D* actual) {
    char label[128];
    snprintf(label, sizeof(label), "%s_triangle", prefix);
    assert_true(label, expected->triangleIndex == actual->triangleIndex);
    snprintf(label, sizeof(label), "%s_local_triangle", prefix);
    assert_true(label, expected->localTriangleIndex == actual->localTriangleIndex);
    snprintf(label, sizeof(label), "%s_primitive", prefix);
    assert_true(label, expected->primitiveIndex == actual->primitiveIndex);
    snprintf(label, sizeof(label), "%s_scene_object", prefix);
    assert_true(label, expected->sceneObjectIndex == actual->sceneObjectIndex);
    snprintf(label, sizeof(label), "%s_source_object", prefix);
    assert_true(label, strcmp(expected->source.objectId, actual->source.objectId) == 0);
    snprintf(label, sizeof(label), "%s_t", prefix);
    assert_near(label, actual->t, expected->t, 1e-8);
    snprintf(label, sizeof(label), "%s_bary_u", prefix);
    assert_near(label, actual->baryU, expected->baryU, 1e-8);
    snprintf(label, sizeof(label), "%s_bary_v", prefix);
    assert_near(label, actual->baryV, expected->baryV, 1e-8);
    snprintf(label, sizeof(label), "%s_bary_w", prefix);
    assert_near(label, actual->baryW, expected->baryW, 1e-8);
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
    const char* saved_mode = getenv("RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_MODE");
    char saved_mode_copy[64] = {0};
    RayTracingRuntimeMeshAssetSet set;
    RuntimeScene3D first_scene;
    RuntimeScene3D second_scene;
    RuntimeSceneAcceleration3DDiagnostics stats;
    char diagnostics[256] = {0};
    bool ok = false;

    if (saved_mode && saved_mode[0]) {
        strncpy(saved_mode_copy, saved_mode, sizeof(saved_mode_copy) - 1);
        saved_mode_copy[sizeof(saved_mode_copy) - 1] = '\0';
    }
    setenv("RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_MODE", "off", 1);

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
    if (saved_mode_copy[0]) {
        setenv("RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_MODE", saved_mode_copy, 1);
    } else {
        unsetenv("RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_MODE");
    }
    return 0;
}

static int test_mesh_blas_persistent_cache_reuses_after_reset(void) {
    const char* cache_root = "/private/tmp/ray_tracing_mrt3_blas_persistent_cache";
    const char* cache_dir =
        "/private/tmp/ray_tracing_mrt3_blas_persistent_cache/ray_tracing_runtime_mesh_asset_accel_cache";
    const char* saved_root = getenv("RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT");
    const char* saved_mode = getenv("RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_MODE");
    char saved_root_copy[PATH_MAX] = {0};
    char saved_mode_copy[64] = {0};
    RayTracingRuntimeMeshAssetSet set;
    RuntimeScene3D first_scene;
    RuntimeScene3D second_scene;
    RuntimeSceneAcceleration3DDiagnostics stats;
    char diagnostics[256] = {0};
    bool ok = false;

    if (saved_root && saved_root[0]) {
        strncpy(saved_root_copy, saved_root, sizeof(saved_root_copy) - 1);
        saved_root_copy[sizeof(saved_root_copy) - 1] = '\0';
    }
    if (saved_mode && saved_mode[0]) {
        strncpy(saved_mode_copy, saved_mode, sizeof(saved_mode_copy) - 1);
        saved_mode_copy[sizeof(saved_mode_copy) - 1] = '\0';
    }

    mkdir(cache_root, 0777);
    setenv("RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT", cache_root, 1);
    setenv("RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_MODE", "read_write", 1);

    ray_tracing_runtime_mesh_asset_set_init(&set);
    RuntimeScene3D_Init(&first_scene);
    RuntimeScene3D_Init(&second_scene);
    RuntimeMeshBLASCache3D_ResetForTests();
    RuntimeSceneAcceleration3D_ResetTLASForTests();

    ok = ray_tracing_runtime_mesh_assets_load_scene_file(kMrt0ScenePath,
                                                        &set,
                                                        diagnostics,
                                                        sizeof(diagnostics));
    assert_true("mrt3_blas_persistent_load_fixture", ok);
    if (ok) {
        ok = RuntimeScene3DBuilder_AppendMeshAssetSet(&first_scene, &set);
        assert_true("mrt3_blas_persistent_first_append", ok);
        snapshot_accel_stats(&stats);
        assert_true("mrt3_blas_persistent_first_rebuilds", stats.blasFullRebuilds == 3u);
        assert_true("mrt3_blas_persistent_first_writes",
                    stats.blasPersistentCacheWrites == 3u);
        assert_true("mrt3_blas_persistent_first_no_hits",
                    stats.blasPersistentCacheHits == 0u);

        RuntimeMeshBLASCache3D_ResetForTests();
        RuntimeSceneAcceleration3D_ResetTLASForTests();
        ok = RuntimeScene3DBuilder_AppendMeshAssetSet(&second_scene, &set);
        assert_true("mrt3_blas_persistent_second_append", ok);
        snapshot_accel_stats(&stats);
        assert_true("mrt3_blas_persistent_second_inprocess_misses",
                    stats.blasCacheMisses == 3u);
        assert_true("mrt3_blas_persistent_second_hits",
                    stats.blasPersistentCacheHits == 3u);
        assert_true("mrt3_blas_persistent_second_no_rebuilds",
                    stats.blasFullRebuilds == 0u);
        assert_true("mrt3_blas_persistent_second_cached_assets",
                    stats.blasCachedAssetCount == 3u);
    }

    for (int i = 0; i < set.asset_count; ++i) {
        char cache_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
        if (RuntimeMeshAccelPack3D_CachePathForSource(cache_dir,
                                                      set.assets[i].path,
                                                      cache_path,
                                                      sizeof(cache_path))) {
            remove(cache_path);
        }
    }
    RuntimeScene3D_Free(&second_scene);
    RuntimeScene3D_Free(&first_scene);
    ray_tracing_runtime_mesh_asset_set_free(&set);
    RuntimeMeshBLASCache3D_ResetForTests();
    RuntimeSceneAcceleration3D_ResetTLASForTests();
    rmdir(cache_dir);
    rmdir(cache_root);
    if (saved_root_copy[0]) {
        setenv("RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT", saved_root_copy, 1);
    } else {
        unsetenv("RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT");
    }
    if (saved_mode_copy[0]) {
        setenv("RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_MODE", saved_mode_copy, 1);
    } else {
        unsetenv("RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_MODE");
    }
    return 0;
}

static int test_mesh_blas_cache_builds_once_for_repeated_instances(void) {
    RayTracingRuntimeMeshAssetSet set;
    RuntimeScene3D scene;
    CoreMeshAssetRuntimeDocument* document = NULL;
    RuntimeSceneAcceleration3DDiagnostics stats;
    RuntimeSceneAcceleration3DTraceStats trace_stats;
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
    document->vertices[0].position.z = -1.0;
    document->vertices[1].position.x = 1.0;
    document->vertices[1].position.y = 0.0;
    document->vertices[1].position.z = 1.0;
    document->vertices[2].position.x = 0.0;
    document->vertices[2].position.y = 1.0;
    document->vertices[2].position.z = 1.0;
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
    {
        RuntimeMeshBLASCache3DView blas_view;
        HitInfo3D flat_hit = {0};
        HitInfo3D accel_hit = {0};
        HitInfo3D route_hit = {0};
        Ray3D ray = RuntimeRay3D_Make(vec3(1.25, 0.25, -1.0),
                                      vec3(0.0, 0.0, 1.0));
        RuntimeSceneAcceleration3DTraceStatus trace_status;
        RuntimeRay3DRouteStats route_stats;
        bool flat_found = false;
        bool route_found = false;

        assert_true("mrt3_blas_lookup_repeated_asset",
                    RuntimeMeshBLASCache3D_FindAsset(&set.assets[0], &blas_view));
        assert_true("mrt3_blas_lookup_repeated_ready", blas_view.ready);
        assert_true("mrt3_blas_lookup_repeated_triangle_count",
                    blas_view.localMesh && blas_view.localMesh->triangleCount == 1);

        RuntimeRay3D_SetTraceRouteForTests(RUNTIME_RAY_3D_TRACE_ROUTE_FLATTENED_BVH);
        flat_found = RuntimeRay3D_TraceSceneFirstHit(&scene,
                                                     &ray,
                                                     0.001,
                                                     10.0,
                                                     &flat_hit);
        RuntimeRay3D_SetTraceRouteForTests(RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS);
        RuntimeRay3D_ResetRouteStats();
        RuntimeSceneAcceleration3D_ResetTraceStats();
        trace_status = RuntimeSceneAcceleration3D_TraceFirstHit(&scene,
                                                                &ray,
                                                                0.001,
                                                                10.0,
                                                                &accel_hit);
        assert_true("mrt3_accel_repeated_flat_hit", flat_found);
        assert_true("mrt3_accel_repeated_trace_hit",
                    trace_status == RUNTIME_SCENE_ACCEL_3D_TRACE_HIT);
        if (flat_found && trace_status == RUNTIME_SCENE_ACCEL_3D_TRACE_HIT) {
            assert_hit_identity_matches("mrt3_accel_repeated_identity",
                                        &flat_hit,
                                        &accel_hit);
            assert_true("mrt3_accel_repeated_second_object",
                        accel_hit.sceneObjectIndex == 21);
        }
        RuntimeSceneAcceleration3D_SnapshotTraceStats(&trace_stats);
        assert_true("mrt3_accel_repeated_trace_calls", trace_stats.traceCalls == 1u);
        assert_true("mrt3_accel_repeated_trace_hits", trace_stats.traceHits == 1u);
        assert_true("mrt3_accel_repeated_blas_calls", trace_stats.blasTraceCalls > 0u);
        assert_true("mrt3_accel_repeated_blas_hits", trace_stats.blasTraceHits == 1u);
        assert_true("mrt3_accel_repeated_remap_map_hit",
                    trace_stats.identityRemapMapHits == 1u);
        assert_true("mrt3_accel_repeated_no_remap_scan",
                    trace_stats.identityRemapFallbackScans == 0u);
        assert_true("mrt3_accel_repeated_remap_ok",
                    trace_stats.identityRemapFailures == 0u);

        RuntimeRay3D_SetTraceRouteForTests(RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS_PARITY);
        RuntimeRay3D_ResetRouteStats();
        route_found = RuntimeRay3D_TraceSceneFirstHit(&scene,
                                                      &ray,
                                                      0.001,
                                                      10.0,
                                                      &route_hit);
        assert_true("mrt3_route_parity_hit", route_found);
        if (route_found && flat_found) {
            assert_hit_identity_matches("mrt3_route_parity_identity",
                                        &flat_hit,
                                        &route_hit);
        }
        RuntimeRay3D_SnapshotRouteStats(&route_stats);
        assert_true("mrt3_route_parity_active",
                    route_stats.activeRoute ==
                        RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS_PARITY);
        assert_true("mrt3_route_parity_checked", route_stats.parityCheckedRays == 1u);
        assert_true("mrt3_route_parity_no_mismatch", route_stats.parityMismatches == 0u);
        assert_true("mrt3_route_parity_tlas_call", route_stats.tlasTraceCalls == 1u);
        assert_true("mrt3_route_parity_flattened_call",
                    route_stats.flattenedTraceCalls == 1u);

        HitInfo3D_Reset(&route_hit);
        RuntimeRay3D_SetTraceRouteForTests(RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS);
        RuntimeRay3D_ResetRouteStats();
        route_found = RuntimeRay3D_TraceSceneFirstHit(&scene,
                                                      &ray,
                                                      0.001,
                                                      10.0,
                                                      &route_hit);
        assert_true("mrt3_route_tlas_hit", route_found);
        if (route_found && flat_found) {
            assert_hit_identity_matches("mrt3_route_tlas_identity",
                                        &flat_hit,
                                        &route_hit);
        }
        RuntimeRay3D_SnapshotRouteStats(&route_stats);
        assert_true("mrt3_route_tlas_active",
                    route_stats.activeRoute == RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS);
        assert_true("mrt3_route_tlas_trace_call", route_stats.tlasTraceCalls == 1u);
        assert_true("mrt3_route_tlas_hit_count", route_stats.tlasTraceHits == 1u);
        assert_true("mrt3_route_tlas_no_fallback",
                    route_stats.flattenedFallbackCalls == 0u);

        {
            HitInfo3D bounded_hit = {0};
            bool bounded_found = false;

            RuntimeRay3D_ResetRouteStats();
            bounded_found = RuntimeRay3D_TraceSceneFirstHit(&scene,
                                                            &ray,
                                                            0.001,
                                                            0.5,
                                                            &bounded_hit);
            assert_true("mrt3_route_tlas_bounded_far_hit_is_miss", !bounded_found);
            RuntimeRay3D_SnapshotRouteStats(&route_stats);
            assert_true("mrt3_route_tlas_bounded_miss_count",
                        route_stats.tlasTraceMisses == 1u);
            assert_true("mrt3_route_tlas_bounded_no_error",
                        route_stats.tlasTraceErrors == 0u);
            assert_true("mrt3_route_tlas_bounded_no_fallback",
                        route_stats.flattenedFallbackCalls == 0u);
        }

        {
            RuntimeScene3D equivalent_scene = scene;
            HitInfo3D equivalent_hit = {0};
            HitInfo3D original_hit_after_equivalent_bind = {0};

            assert_true("mrt3_route_equivalent_scene_bind",
                        RuntimeSceneAcceleration3D_BindPreparedSceneForTracing(
                            &equivalent_scene));
            RuntimeRay3D_ResetRouteStats();
            assert_true("mrt3_route_equivalent_scene_hit",
                        RuntimeRay3D_TraceSceneFirstHit(&equivalent_scene,
                                                       &ray,
                                                       0.001,
                                                       10.0,
                                                       &equivalent_hit));
            assert_true("mrt3_route_original_scene_remains_compatible",
                        RuntimeRay3D_TraceSceneFirstHit(&scene,
                                                       &ray,
                                                       0.001,
                                                       10.0,
                                                       &original_hit_after_equivalent_bind));
            RuntimeRay3D_SnapshotRouteStats(&route_stats);
            assert_true("mrt3_route_equivalent_scene_tlas_calls",
                        route_stats.tlasTraceCalls == 2u);
            assert_true("mrt3_route_equivalent_scene_no_unready",
                        route_stats.tlasTraceUnready == 0u);
            assert_true("mrt3_route_equivalent_scene_no_fallback",
                        route_stats.flattenedFallbackCalls == 0u);
        }
        RuntimeRay3D_ResetTraceRouteForTests();
        RuntimeRay3D_ResetRouteStats();
    }

    RuntimeScene3D_Free(&scene);
    ray_tracing_runtime_mesh_asset_set_free(&set);
    RuntimeMeshBLASCache3D_ResetForTests();
    RuntimeSceneAcceleration3D_ResetTLASForTests();
    return 0;
}

static int test_tlas_route_traces_mixed_supported_and_primitive_instances(void) {
    RayTracingRuntimeMeshAssetSet set;
    RuntimeScene3D scene;
    CoreMeshAssetRuntimeDocument* document = NULL;
    RuntimePrimitive3D* primitives = NULL;
    RuntimeTriangle3D* triangles = NULL;
    RuntimeSceneAcceleration3DTraceStatus trace_status;
    RuntimeRay3DRouteStats route_stats;
    HitInfo3D accel_hit = {0};
    HitInfo3D route_hit = {0};
    Ray3D ray = RuntimeRay3D_Make(vec3(0.75, 0.75, -1.0), vec3(0.0, 0.0, 1.0));
    Ray3D closer_primitive_ray =
        RuntimeRay3D_Make(vec3(0.25, 0.25, -1.0), vec3(0.0, 0.0, 1.0));
    const int primitive_plane_index = 1;
    const int primitive_triangle_index = 1;
    bool ok = false;
    bool route_found = false;

    ray_tracing_runtime_mesh_asset_set_init(&set);
    RuntimeScene3D_Init(&scene);
    RuntimeMeshBLASCache3D_ResetForTests();
    RuntimeSceneAcceleration3D_ResetTLASForTests();

    set.asset_count = 1;
    snprintf(set.assets[0].asset_id, sizeof(set.assets[0].asset_id), "asset_mixed_triangle");
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

    set.instance_count = 1;
    snprintf(set.instances[0].object_id,
             sizeof(set.instances[0].object_id),
             "obj_mixed_supported_triangle");
    snprintf(set.instances[0].asset_id,
             sizeof(set.instances[0].asset_id),
             "asset_mixed_triangle");
    set.instances[0].asset_index = 0;
    set.instances[0].scene_object_index = 30;
    set.instances[0].scale_x = 1.0;
    set.instances[0].scale_y = 1.0;
    set.instances[0].scale_z = 1.0;

    ok = RuntimeScene3DBuilder_AppendMeshAssetSet(&scene, &set);
    assert_true("mrt3_mixed_tlas_append_supported_mesh", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        ray_tracing_runtime_mesh_asset_set_free(&set);
        RuntimeMeshBLASCache3D_ResetForTests();
        RuntimeSceneAcceleration3D_ResetTLASForTests();
        return 0;
    }

    primitives = (RuntimePrimitive3D*)realloc(
        scene.primitives,
        (size_t)(scene.primitiveCount + 1) * sizeof(*scene.primitives));
    assert_true("mrt3_mixed_tlas_realloc_primitives", primitives != NULL);
    if (!primitives) {
        RuntimeScene3D_Free(&scene);
        ray_tracing_runtime_mesh_asset_set_free(&set);
        RuntimeMeshBLASCache3D_ResetForTests();
        RuntimeSceneAcceleration3D_ResetTLASForTests();
        return 0;
    }
    scene.primitives = primitives;
    triangles = (RuntimeTriangle3D*)realloc(
        scene.triangleMesh.triangles,
        (size_t)(scene.triangleMesh.triangleCount + 1) *
            sizeof(*scene.triangleMesh.triangles));
    assert_true("mrt3_mixed_tlas_realloc_triangles", triangles != NULL);
    if (!triangles) {
        RuntimeScene3D_Free(&scene);
        ray_tracing_runtime_mesh_asset_set_free(&set);
        RuntimeMeshBLASCache3D_ResetForTests();
        RuntimeSceneAcceleration3D_ResetTLASForTests();
        return 0;
    }
    scene.triangleMesh.triangles = triangles;
    scene.primitiveCapacity = scene.primitiveCount + 1;
    scene.triangleMesh.triangleCapacity = scene.triangleMesh.triangleCount + 1;

    memset(&scene.primitives[primitive_plane_index],
           0,
           sizeof(scene.primitives[primitive_plane_index]));
    scene.primitives[primitive_plane_index].kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[primitive_plane_index].source.kind =
        RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[primitive_plane_index].source.sceneObjectIndex = 77;
    snprintf(scene.primitives[primitive_plane_index].source.objectId,
             sizeof(scene.primitives[primitive_plane_index].source.objectId),
             "obj_mixed_primitive_plane");

    memset(&scene.triangleMesh.triangles[primitive_triangle_index],
           0,
           sizeof(scene.triangleMesh.triangles[primitive_triangle_index]));
    scene.triangleMesh.triangles[primitive_triangle_index].p0 = vec3(0.0, 0.0, -0.5);
    scene.triangleMesh.triangles[primitive_triangle_index].p1 = vec3(1.5, 0.0, -0.5);
    scene.triangleMesh.triangles[primitive_triangle_index].p2 = vec3(0.0, 1.5, -0.5);
    scene.triangleMesh.triangles[primitive_triangle_index].normal = vec3(0.0, 0.0, 1.0);
    scene.triangleMesh.triangles[primitive_triangle_index].primitiveIndex =
        primitive_plane_index;
    scene.triangleMesh.triangles[primitive_triangle_index].sceneObjectIndex = 77;
    scene.triangleMesh.triangles[primitive_triangle_index].localTriangleIndex = 0;
    scene.primitiveCount += 1;
    scene.triangleMesh.triangleCount += 1;
    scene.triangleMesh.bvhDirty = true;

    ok = RuntimeSceneAcceleration3D_RebuildPreparedFromSceneAndMeshAssets(&scene, &set);
    assert_true("mrt3_mixed_tlas_rebuild_prepared", ok);
    if (ok) {
        RuntimeSceneAcceleration3D_ResetTraceStats();
        trace_status = RuntimeSceneAcceleration3D_TraceFirstHit(&scene,
                                                                &ray,
                                                                0.001,
                                                                10.0,
                                                                &accel_hit);
        assert_true("mrt3_mixed_tlas_traces_primitive_for_incomplete_miss",
                    trace_status == RUNTIME_SCENE_ACCEL_3D_TRACE_HIT);
        if (trace_status == RUNTIME_SCENE_ACCEL_3D_TRACE_HIT) {
            assert_true("mrt3_mixed_tlas_primitive_object",
                        accel_hit.sceneObjectIndex == 77);
            assert_true("mrt3_mixed_tlas_primitive_index",
                        accel_hit.primitiveIndex == primitive_plane_index);
        }

        RuntimeRay3D_SetTraceRouteForTests(RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS);
        RuntimeRay3D_ResetRouteStats();
        route_found = RuntimeRay3D_TraceSceneFirstHit(&scene,
                                                      &ray,
                                                      0.001,
                                                      10.0,
                                                      &route_hit);
        assert_true("mrt3_mixed_tlas_route_primitive_hit", route_found);
        if (route_found) {
            assert_true("mrt3_mixed_tlas_route_primitive_object",
                        route_hit.sceneObjectIndex == 77);
            assert_true("mrt3_mixed_tlas_route_primitive_index",
                        route_hit.primitiveIndex == primitive_plane_index);
        }
        RuntimeRay3D_SnapshotRouteStats(&route_stats);
        assert_true("mrt3_mixed_tlas_route_no_unsupported",
                    route_stats.tlasTraceUnsupported == 0u);
        assert_true("mrt3_mixed_tlas_route_no_fallback",
                    route_stats.flattenedFallbackCalls == 0u);
        assert_true("mrt3_mixed_tlas_route_no_flattened",
                    route_stats.flattenedTraceCalls == 0u);
        RuntimeRay3D_ResetTraceRouteForTests();
        RuntimeRay3D_ResetRouteStats();

        HitInfo3D_Reset(&accel_hit);
        HitInfo3D_Reset(&route_hit);
        RuntimeSceneAcceleration3D_ResetTraceStats();
        trace_status =
            RuntimeSceneAcceleration3D_TraceFirstHit(&scene,
                                                    &closer_primitive_ray,
                                                    0.001,
                                                    10.0,
                                                    &accel_hit);
        assert_true("mrt3_mixed_tlas_traces_primitive_before_far_blas_hit",
                    trace_status == RUNTIME_SCENE_ACCEL_3D_TRACE_HIT);
        if (trace_status == RUNTIME_SCENE_ACCEL_3D_TRACE_HIT) {
            assert_true("mrt3_mixed_tlas_closer_primitive_object",
                        accel_hit.sceneObjectIndex == 77);
            assert_true("mrt3_mixed_tlas_closer_primitive_index",
                        accel_hit.primitiveIndex == primitive_plane_index);
            assert_true("mrt3_mixed_tlas_closer_primitive_before_supported",
                        accel_hit.t < 1.0);
        }

        RuntimeRay3D_SetTraceRouteForTests(RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS);
        RuntimeRay3D_ResetRouteStats();
        route_found = RuntimeRay3D_TraceSceneFirstHit(&scene,
                                                      &closer_primitive_ray,
                                                      0.001,
                                                      10.0,
                                                      &route_hit);
        assert_true("mrt3_mixed_tlas_route_closer_primitive_hit", route_found);
        if (route_found) {
            assert_true("mrt3_mixed_tlas_route_closer_primitive_object",
                        route_hit.sceneObjectIndex == 77);
            assert_true("mrt3_mixed_tlas_route_closer_primitive_index",
                        route_hit.primitiveIndex == primitive_plane_index);
            assert_true("mrt3_mixed_tlas_route_closer_primitive_before_supported",
                        route_hit.t < 1.0);
        }
        RuntimeRay3D_SnapshotRouteStats(&route_stats);
        assert_true("mrt3_mixed_tlas_route_closer_no_unsupported",
                    route_stats.tlasTraceUnsupported == 0u);
        assert_true("mrt3_mixed_tlas_route_closer_no_fallback",
                    route_stats.flattenedFallbackCalls == 0u);
        assert_true("mrt3_mixed_tlas_route_closer_no_flattened",
                    route_stats.flattenedTraceCalls == 0u);
        RuntimeRay3D_ResetTraceRouteForTests();
        RuntimeRay3D_ResetRouteStats();
    }

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
    RuntimeSceneAcceleration3DTraceStats trace_stats;
    bool ok = false;

    ray_tracing_runtime_mesh_asset_set_init(&set);
    RuntimeScene3D_Init(&scene);
    RuntimeMeshBLASCache3D_ResetForTests();
    RuntimeSceneAcceleration3D_ResetTLASForTests();

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
        HitInfo3D accel_hit = {0};
        Ray3D ray = RuntimeRay3D_Make(vec3(0.25, 0.25, -1.0),
                                      vec3(0.0, 0.0, 1.0));
        RuntimeSceneAcceleration3DTraceStatus trace_status;

        assert_true("mrt3_append_degenerate_mix_local_triangle_index",
                    scene.triangleMesh.triangles[0].localTriangleIndex == 1);
        assert_true("mrt3_append_degenerate_mix_scene_object_index",
                    scene.triangleMesh.triangles[0].sceneObjectIndex == 7);

        RuntimeSceneAcceleration3D_ResetTraceStats();
        trace_status = RuntimeSceneAcceleration3D_TraceFirstHit(&scene,
                                                                &ray,
                                                                0.001,
                                                                10.0,
                                                                &accel_hit);
        assert_true("mrt3_append_degenerate_mix_tlas_hit",
                    trace_status == RUNTIME_SCENE_ACCEL_3D_TRACE_HIT);
        if (trace_status == RUNTIME_SCENE_ACCEL_3D_TRACE_HIT) {
            assert_true("mrt3_append_degenerate_mix_hit_triangle",
                        accel_hit.triangleIndex == 0);
            assert_true("mrt3_append_degenerate_mix_hit_local_triangle",
                        accel_hit.localTriangleIndex == 1);
        }
        RuntimeSceneAcceleration3D_SnapshotTraceStats(&trace_stats);
        assert_true("mrt3_append_degenerate_mix_remap_map_hit",
                    trace_stats.identityRemapMapHits == 1u);
        assert_true("mrt3_append_degenerate_mix_no_remap_scan",
                    trace_stats.identityRemapFallbackScans == 0u);
        assert_true("mrt3_append_degenerate_mix_no_remap_failure",
                    trace_stats.identityRemapFailures == 0u);
    }

    RuntimeScene3D_Free(&scene);
    ray_tracing_runtime_mesh_asset_set_free(&set);
    RuntimeMeshBLASCache3D_ResetForTests();
    RuntimeSceneAcceleration3D_ResetTLASForTests();
    return 0;
}

static int test_tlas_bind_accepts_copied_prepared_scene(void) {
    RayTracingRuntimeMeshAssetSet set;
    RuntimeScene3D scene;
    RuntimeScene3D copied_scene;
    CoreMeshAssetRuntimeDocument* document = NULL;
    RuntimeRay3DRouteStats route_stats;
    HitInfo3D route_hit = {0};
    Ray3D ray = RuntimeRay3D_Make(vec3(0.25, 0.25, -1.0), vec3(0.0, 0.0, 1.0));
    bool ok = false;
    bool route_found = false;

    ray_tracing_runtime_mesh_asset_set_init(&set);
    RuntimeScene3D_Init(&scene);
    RuntimeScene3D_Init(&copied_scene);
    RuntimeMeshBLASCache3D_ResetForTests();
    RuntimeSceneAcceleration3D_ResetTLASForTests();

    set.asset_count = 1;
    snprintf(set.assets[0].asset_id, sizeof(set.assets[0].asset_id), "asset_copy_bind");
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

    set.instance_count = 1;
    snprintf(set.instances[0].object_id,
             sizeof(set.instances[0].object_id),
             "obj_copy_bind_triangle");
    snprintf(set.instances[0].asset_id, sizeof(set.instances[0].asset_id), "asset_copy_bind");
    set.instances[0].asset_index = 0;
    set.instances[0].scene_object_index = 41;
    set.instances[0].scale_x = 1.0;
    set.instances[0].scale_y = 1.0;
    set.instances[0].scale_z = 1.0;

    ok = RuntimeScene3DBuilder_AppendMeshAssetSet(&scene, &set);
    assert_true("mrt3_tlas_bind_copy_append", ok);
    if (ok) {
        ok = RuntimeScene3D_CopyGeometryFrom(&copied_scene, &scene);
        assert_true("mrt3_tlas_bind_copy_geometry", ok);
    }
    if (ok) {
        ok = RuntimeSceneAcceleration3D_BindPreparedSceneForTracing(&copied_scene);
        assert_true("mrt3_tlas_bind_copy_ok", ok);
    }
    if (ok) {
        RuntimeRay3D_SetTraceRouteForTests(RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS);
        RuntimeRay3D_ResetRouteStats();
        route_found = RuntimeRay3D_TraceSceneFirstHit(&copied_scene,
                                                      &ray,
                                                      0.001,
                                                      10.0,
                                                      &route_hit);
        assert_true("mrt3_tlas_bind_copy_route_hit", route_found);
        if (route_found) {
            assert_true("mrt3_tlas_bind_copy_object", route_hit.sceneObjectIndex == 41);
            assert_true("mrt3_tlas_bind_copy_triangle", route_hit.triangleIndex == 0);
        }
        RuntimeRay3D_SnapshotRouteStats(&route_stats);
        assert_true("mrt3_tlas_bind_copy_no_unready",
                    route_stats.tlasTraceUnready == 0u);
        assert_true("mrt3_tlas_bind_copy_no_fallback",
                    route_stats.flattenedFallbackCalls == 0u);
        assert_true("mrt3_tlas_bind_copy_no_flattened",
                    route_stats.flattenedTraceCalls == 0u);
        RuntimeRay3D_ResetTraceRouteForTests();
        RuntimeRay3D_ResetRouteStats();
    }

    RuntimeScene3D_Free(&copied_scene);
    RuntimeScene3D_Free(&scene);
    ray_tracing_runtime_mesh_asset_set_free(&set);
    RuntimeMeshBLASCache3D_ResetForTests();
    RuntimeSceneAcceleration3D_ResetTLASForTests();
    return 0;
}

static int test_tlas_bind_rejects_same_count_geometry_drift(void) {
    RayTracingRuntimeMeshAssetSet set;
    RuntimeScene3D scene;
    RuntimeScene3D copied_scene;
    CoreMeshAssetRuntimeDocument* document = NULL;
    bool ok = false;

    ray_tracing_runtime_mesh_asset_set_init(&set);
    RuntimeScene3D_Init(&scene);
    RuntimeScene3D_Init(&copied_scene);
    RuntimeMeshBLASCache3D_ResetForTests();
    RuntimeSceneAcceleration3D_ResetTLASForTests();

    set.asset_count = 1;
    snprintf(set.assets[0].asset_id, sizeof(set.assets[0].asset_id), "asset_drift_bind");
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

    set.instance_count = 1;
    snprintf(set.instances[0].object_id,
             sizeof(set.instances[0].object_id),
             "obj_drift_bind_triangle");
    snprintf(set.instances[0].asset_id,
             sizeof(set.instances[0].asset_id),
             "asset_drift_bind");
    set.instances[0].asset_index = 0;
    set.instances[0].scene_object_index = 42;
    set.instances[0].scale_x = 1.0;
    set.instances[0].scale_y = 1.0;
    set.instances[0].scale_z = 1.0;

    ok = RuntimeScene3DBuilder_AppendMeshAssetSet(&scene, &set);
    assert_true("mrt3_tlas_bind_drift_append", ok);
    if (ok) {
        ok = RuntimeScene3D_CopyGeometryFrom(&copied_scene, &scene);
        assert_true("mrt3_tlas_bind_drift_copy", ok);
    }
    if (ok && copied_scene.triangleMesh.triangleCount > 0) {
        copied_scene.triangleMesh.triangles[0].p0.x += 0.125;
        ok = RuntimeSceneAcceleration3D_BindPreparedSceneForTracing(&copied_scene);
        assert_true("mrt3_tlas_bind_drift_rejected", !ok);
        assert_true("mrt3_tlas_bind_drift_diag",
                    strstr(RuntimeSceneAcceleration3D_LastDiagnostics(),
                           "geometry signature differs") != NULL);
    }

    RuntimeScene3D_Free(&copied_scene);
    RuntimeScene3D_Free(&scene);
    ray_tracing_runtime_mesh_asset_set_free(&set);
    RuntimeMeshBLASCache3D_ResetForTests();
    RuntimeSceneAcceleration3D_ResetTLASForTests();
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
    test_mesh_blas_persistent_cache_reuses_after_reset();
    test_mesh_blas_cache_builds_once_for_repeated_instances();
    test_tlas_route_traces_mixed_supported_and_primitive_instances();
    test_append_mesh_asset_set_skips_degenerate_triangles();
    test_tlas_bind_accepts_copied_prepared_scene();
    test_tlas_bind_rejects_same_count_geometry_drift();
    test_bridge_builder_consumes_retained_mesh_assets();
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
