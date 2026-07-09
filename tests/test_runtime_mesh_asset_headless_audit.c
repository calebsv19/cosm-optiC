#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config/config_manager.h"
#include "import/runtime_mesh_asset_loader.h"
#include "import/runtime_scene_bridge.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_scene_3d_builder.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char* kMrt0ScenePath =
    "tests/fixtures/mesh_asset_runtime_spheres/scene_runtime.json";
static const char* kImportedTetrahedronScenePath =
    "tests/fixtures/mesh_asset_runtime_imported_tetrahedron/scene_runtime.json";
static const char* kLineDrawingGeneratedSceneEnv =
    "RAY_TRACING_LINE_DRAWING_IMPORTED_MESH_SCENE";

AnimationConfig animSettings;

static int g_failures = 0;

typedef struct MeshAuditEntry {
    const char* object_id;
    int scene_object_index;
    int expected_triangles;
    int triangle_count;
    int primary_hit_pixels;
} MeshAuditEntry;

static void assert_true(const char* name, bool condition) {
    if (!condition) {
        printf("FAIL %-56s condition=false\n", name);
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

static MeshAuditEntry* find_audit_entry(MeshAuditEntry* entries,
                                        int entry_count,
                                        int scene_object_index) {
    for (int i = 0; i < entry_count; ++i) {
        if (entries[i].scene_object_index == scene_object_index) return &entries[i];
    }
    return NULL;
}

static void count_triangles_by_object(const RuntimeScene3D* scene,
                                      MeshAuditEntry* entries,
                                      int entry_count) {
    if (!scene || !entries) return;
    for (int i = 0; i < scene->triangleMesh.triangleCount; ++i) {
        MeshAuditEntry* entry =
            find_audit_entry(entries, entry_count, scene->triangleMesh.triangles[i].sceneObjectIndex);
        if (entry) entry->triangle_count += 1;
    }
}

static void count_primary_hits_by_object(const RuntimeScene3D* scene,
                                         MeshAuditEntry* entries,
                                         int entry_count) {
    RuntimeCamera3D camera = {0};
    RuntimeCameraProjector3D projector = {0};
    const int width = 160;
    const int height = 96;
    if (!scene || !entries) return;

    camera.position = vec3(0.0, -6.0, 1.45);
    camera.rotation = M_PI;
    camera.lookPitch = -0.10;
    camera.zoom = 1.0;
    camera.nearPlane = 0.1;
    assert_true("mrt4_projector_build",
                RuntimeCameraProjector3D_Build(&camera, width, height, &projector));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            HitInfo3D hit = {0};
            Ray3D ray = RuntimeCameraProjector3D_MakePrimaryRay(&projector, (double)x, (double)y);
            MeshAuditEntry* entry = NULL;
            if (!RuntimeRay3D_TraceSceneFirstHit(scene, &ray, 1e-6, 1.0e9, &hit)) {
                continue;
            }
            entry = find_audit_entry(entries, entry_count, hit.sceneObjectIndex);
            if (entry) entry->primary_hit_pixels += 1;
        }
    }
}

static int test_runtime_mesh_asset_headless_audit(void) {
    RayTracingRuntimeMeshAssetSet set;
    RuntimeScene3D scene;
    char diagnostics[256] = {0};
    MeshAuditEntry entries[] = {
        {"obj_sphere_low", 3, 48, 0, 0},
        {"obj_sphere_medium", 4, 224, 0, 0},
        {"obj_sphere_high", 5, 960, 0, 0},
    };
    const int entry_count = (int)(sizeof(entries) / sizeof(entries[0]));
    bool ok = false;

    ray_tracing_runtime_mesh_asset_set_init(&set);
    RuntimeScene3D_Init(&scene);
    ok = ray_tracing_runtime_mesh_assets_load_scene_file(kMrt0ScenePath,
                                                        &set,
                                                        diagnostics,
                                                        sizeof(diagnostics));
    assert_true("mrt4_load_fixture_mesh_assets", ok);
    if (ok) {
        ok = RuntimeScene3DBuilder_AppendMeshAssetSet(&scene, &set);
        assert_true("mrt4_append_mesh_asset_set", ok);
    }
    if (ok) {
        count_triangles_by_object(&scene, entries, entry_count);
        count_primary_hits_by_object(&scene, entries, entry_count);
        for (int i = 0; i < entry_count; ++i) {
            char triangle_name[96] = {0};
            char hit_name[96] = {0};
            snprintf(triangle_name,
                     sizeof(triangle_name),
                     "mrt4_%s_triangle_count",
                     entries[i].object_id);
            snprintf(hit_name,
                     sizeof(hit_name),
                     "mrt4_%s_primary_hit_pixels",
                     entries[i].object_id);
            assert_true(triangle_name,
                        entries[i].triangle_count == entries[i].expected_triangles);
            assert_true(hit_name, entries[i].primary_hit_pixels > 0);
            printf("MRT4_AUDIT object_id=%s scene_object_index=%d triangle_count=%d primary_hit_pixels=%d\n",
                   entries[i].object_id,
                   entries[i].scene_object_index,
                   entries[i].triangle_count,
                   entries[i].primary_hit_pixels);
        }
    }

    RuntimeScene3D_Free(&scene);
    ray_tracing_runtime_mesh_asset_set_free(&set);
    return 0;
}

static int test_imported_tetrahedron_runtime_mesh_asset_audit_for_scene(const char* scene_path,
                                                                        const char* test_prefix,
                                                                        const char* expected_asset_id,
                                                                        const char* expected_object_id,
                                                                        bool required) {
    RayTracingRuntimeMeshAssetSet set;
    RuntimeScene3D scene;
    char diagnostics[256] = {0};
    bool ok = false;

    if (!scene_path || scene_path[0] == '\0') {
        assert_true(test_prefix ? test_prefix : "mrt_imported_scene_path", !required);
        return 0;
    }

    ray_tracing_runtime_mesh_asset_set_init(&set);
    RuntimeScene3D_Init(&scene);
    ok = ray_tracing_runtime_mesh_assets_load_scene_file(scene_path,
                                                        &set,
                                                        diagnostics,
                                                        sizeof(diagnostics));
    {
        char name[128] = {0};
        snprintf(name, sizeof(name), "%s_load_mesh_assets", test_prefix);
        assert_true(name, ok);
    }
    if (ok) {
        char name[128] = {0};
        snprintf(name, sizeof(name), "%s_asset_count", test_prefix);
        assert_true(name, set.asset_count == 1);
        snprintf(name, sizeof(name), "%s_instance_count", test_prefix);
        assert_true(name, set.instance_count == 1);
        snprintf(name, sizeof(name), "%s_asset_id", test_prefix);
        assert_true(name, strcmp(set.assets[0].asset_id, expected_asset_id) == 0);
        snprintf(name, sizeof(name), "%s_runtime_vertex_count", test_prefix);
        assert_true(name, set.assets[0].document.vertex_count == 4u);
        snprintf(name, sizeof(name), "%s_runtime_triangle_count", test_prefix);
        assert_true(name, set.assets[0].document.triangle_count == 4u);
        snprintf(name, sizeof(name), "%s_surface_group", test_prefix);
        assert_true(name,
                    strcmp(set.assets[0].document.surface_groups[0].group_id,
                           "imported_surface") == 0);
        ok = RuntimeScene3DBuilder_AppendMeshAssetSet(&scene, &set);
        snprintf(name, sizeof(name), "%s_append_mesh_asset_set", test_prefix);
        assert_true(name, ok);
    }
    if (ok) {
        char name[128] = {0};
        snprintf(name, sizeof(name), "%s_primitive_count", test_prefix);
        assert_true(name, scene.primitiveCount == 1);
        snprintf(name, sizeof(name), "%s_triangle_count", test_prefix);
        assert_true(name, scene.triangleMesh.triangleCount == 4);
        snprintf(name, sizeof(name), "%s_scene_index", test_prefix);
        assert_true(name, scene.triangleMesh.triangles[0].sceneObjectIndex == 0);
        snprintf(name, sizeof(name), "%s_object_id", test_prefix);
        assert_true(name,
                    strcmp(scene.primitives[0].source.objectId, expected_object_id) == 0);
        printf("%s object_id=%s triangle_count=%d surface_group=%s scene_path=%s\n",
               strcmp(test_prefix, "mrt_line_drawing_generated") == 0
                   ? "MRT_LINE_DRAWING_GENERATED_AUDIT"
                   : "MRT_IMPORTED_AUDIT",
               expected_object_id,
               scene.triangleMesh.triangleCount,
               set.assets[0].document.surface_groups[0].group_id,
               scene_path);
    }

    RuntimeScene3D_Free(&scene);
    ray_tracing_runtime_mesh_asset_set_free(&set);
    return 0;
}

static int test_imported_tetrahedron_runtime_mesh_asset_audit(void) {
    return test_imported_tetrahedron_runtime_mesh_asset_audit_for_scene(
        kImportedTetrahedronScenePath,
        "mrt_imported_tetra",
        "asset_imported_tetrahedron_01",
        "obj_imported_tetrahedron",
        true);
}

static int test_line_drawing_generated_imported_mesh_audit(void) {
    return test_imported_tetrahedron_runtime_mesh_asset_audit_for_scene(
        getenv(kLineDrawingGeneratedSceneEnv),
        "mrt_line_drawing_generated",
        "asset_imported_tetrahedron_line_harness",
        "obj_imported_tetrahedron_harness",
        false);
}

int main(void) {
    test_runtime_mesh_asset_headless_audit();
    test_imported_tetrahedron_runtime_mesh_asset_audit();
    test_line_drawing_generated_imported_mesh_audit();
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
