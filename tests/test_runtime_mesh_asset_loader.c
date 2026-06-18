#include "test_runtime_mesh_asset_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

#include "import/runtime_mesh_asset_loader.h"
#include "config/config_manager.h"

static const char* kMrt0ScenePath =
    "tests/fixtures/mesh_asset_runtime_spheres/scene_runtime.json";

static int g_runtime_mesh_asset_loader_failures = 0;

static void assert_true(const char* name, bool cond) {
    if (!cond) {
        printf("FAIL %-48s condition=false\n", name);
        g_runtime_mesh_asset_loader_failures += 1;
    }
}

static void assert_near(const char* name, double actual, double expected, double epsilon) {
    if (fabs(actual - expected) > epsilon) {
        printf("FAIL %-48s actual=%f expected=%f\n", name, actual, expected);
        g_runtime_mesh_asset_loader_failures += 1;
    }
}

static bool write_text_file(const char* path, const char* text) {
    FILE* f = NULL;
    size_t len = 0u;
    size_t written = 0u;
    if (!path || !text) return false;
    f = fopen(path, "wb");
    if (!f) return false;
    len = strlen(text);
    written = fwrite(text, 1u, len, f);
    fclose(f);
    return written == len;
}

static char* read_text_file_alloc(const char* path) {
    FILE* f = NULL;
    long size = 0;
    char* text = NULL;
    size_t read_count = 0u;
    if (!path) return NULL;
    f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    text = (char*)malloc((size_t)size + 2u);
    if (!text) {
        fclose(f);
        return NULL;
    }
    read_count = fread(text, 1u, (size_t)size, f);
    fclose(f);
    if (read_count != (size_t)size) {
        free(text);
        return NULL;
    }
    text[size] = '\0';
    return text;
}

static int find_loaded_asset(const RayTracingRuntimeMeshAssetSet* set, const char* asset_id) {
    int i = 0;
    if (!set || !asset_id) return -1;
    for (i = 0; i < set->asset_count; ++i) {
        if (strcmp(set->assets[i].asset_id, asset_id) == 0) return i;
    }
    return -1;
}

static int test_runtime_mesh_asset_loader_resolves_and_loads_mrt0_fixture(void) {
    RayTracingRuntimeMeshAssetSet set;
    char diagnostics[256] = {0};
    char path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    int low_index = -1;
    int medium_index = -1;
    int high_index = -1;
    bool ok = ray_tracing_runtime_mesh_asset_resolve_path(kMrt0ScenePath,
                                                          "asset_sphere_16x8",
                                                          path,
                                                          sizeof(path),
                                                          diagnostics,
                                                          sizeof(diagnostics));
    assert_true("mrt2_resolve_medium_sphere", ok);
    if (ok) {
        assert_true("mrt2_resolve_medium_sphere_path",
                    strstr(path, "assets/mesh_assets/asset_sphere_16x8.runtime.json") != NULL);
    }

    ray_tracing_runtime_mesh_asset_set_init(&set);
    ok = ray_tracing_runtime_mesh_assets_load_scene_file(kMrt0ScenePath,
                                                        &set,
                                                        diagnostics,
                                                        sizeof(diagnostics));
    assert_true("mrt2_load_mrt0_scene_assets", ok);
    if (ok) {
        assert_true("mrt2_load_asset_count_three", set.asset_count == 3);
        assert_true("mrt2_load_instance_count_three", set.instance_count == 3);
        low_index = find_loaded_asset(&set, "asset_sphere_8x4");
        medium_index = find_loaded_asset(&set, "asset_sphere_16x8");
        high_index = find_loaded_asset(&set, "asset_sphere_32x16");
        assert_true("mrt2_load_low_asset_present", low_index >= 0);
        assert_true("mrt2_load_medium_asset_present", medium_index >= 0);
        assert_true("mrt2_load_high_asset_present", high_index >= 0);
        assert_true("mrt3_low_scene_object_index", set.instances[0].scene_object_index == 3);
        assert_true("mrt3_medium_scene_object_index", set.instances[1].scene_object_index == 4);
        assert_true("mrt3_high_scene_object_index", set.instances[2].scene_object_index == 5);
        assert_near("mrt3_low_position_x", set.instances[0].position_x, -2.0, 1e-9);
        assert_near("mrt3_low_position_y", set.instances[0].position_y, 0.3, 1e-9);
        assert_near("mrt3_low_position_z", set.instances[0].position_z, 0.75, 1e-9);
        assert_near("mrt3_low_scale_x", set.instances[0].scale_x, 0.65, 1e-9);
        assert_near("mrt3_medium_scale_y", set.instances[1].scale_y, 0.65, 1e-9);
        assert_near("mrt3_high_scale_z", set.instances[2].scale_z, 0.65, 1e-9);
        if (low_index >= 0) {
            assert_true("mrt2_low_vertices", set.assets[low_index].document.vertex_count == 26u);
            assert_true("mrt2_low_triangles", set.assets[low_index].document.triangle_count == 48u);
        }
        if (medium_index >= 0) {
            assert_true("mrt2_medium_vertices", set.assets[medium_index].document.vertex_count == 114u);
            assert_true("mrt2_medium_triangles", set.assets[medium_index].document.triangle_count == 224u);
        }
        if (high_index >= 0) {
            assert_true("mrt2_high_vertices", set.assets[high_index].document.vertex_count == 482u);
            assert_true("mrt2_high_triangles", set.assets[high_index].document.triangle_count == 960u);
        }
    }
    ray_tracing_runtime_mesh_asset_set_free(&set);
    return 0;
}

static int test_runtime_mesh_asset_loader_reports_missing_asset(void) {
    const char* dir = "/private/tmp/ray_tracing_mrt2_missing";
    const char* scene_path = "/private/tmp/ray_tracing_mrt2_missing/scene_runtime.json";
    const char* scene_json =
        "{"
        "\"objects\":[{"
        "\"object_id\":\"obj_missing_mesh\","
        "\"object_type\":\"mesh_asset_instance\","
        "\"geometry_ref\":{\"kind\":\"mesh_asset\",\"id\":\"asset_missing\"}"
        "}]"
        "}";
    RayTracingRuntimeMeshAssetSet set;
    char diagnostics[256] = {0};
    bool ok = false;

    mkdir(dir, 0777);
    assert_true("mrt2_missing_write_scene", write_text_file(scene_path, scene_json));
    ray_tracing_runtime_mesh_asset_set_init(&set);
    ok = ray_tracing_runtime_mesh_assets_load_scene_file(scene_path,
                                                        &set,
                                                        diagnostics,
                                                        sizeof(diagnostics));
    assert_true("mrt2_missing_asset_rejected", !ok);
    assert_true("mrt2_missing_asset_diag", strstr(diagnostics, "asset_missing") != NULL);
    assert_true("mrt2_missing_asset_diag_not_found", strstr(diagnostics, "not found") != NULL);
    ray_tracing_runtime_mesh_asset_set_free(&set);
    remove(scene_path);
    rmdir(dir);
    return 0;
}

static int test_runtime_mesh_asset_loader_reports_malformed_asset(void) {
    const char* dir = "/private/tmp/ray_tracing_mrt2_bad";
    const char* assets_dir = "/private/tmp/ray_tracing_mrt2_bad/assets";
    const char* mesh_dir = "/private/tmp/ray_tracing_mrt2_bad/assets/mesh_assets";
    const char* scene_path = "/private/tmp/ray_tracing_mrt2_bad/scene_runtime.json";
    const char* asset_path =
        "/private/tmp/ray_tracing_mrt2_bad/assets/mesh_assets/asset_bad.runtime.json";
    const char* scene_json =
        "{"
        "\"objects\":[{"
        "\"object_id\":\"obj_bad_mesh\","
        "\"object_type\":\"mesh_asset_instance\","
        "\"geometry_ref\":{\"kind\":\"mesh_asset\",\"id\":\"asset_bad\"}"
        "}]"
        "}";
    RayTracingRuntimeMeshAssetSet set;
    char diagnostics[256] = {0};
    bool ok = false;

    mkdir(dir, 0777);
    mkdir(assets_dir, 0777);
    mkdir(mesh_dir, 0777);
    assert_true("mrt2_bad_write_scene", write_text_file(scene_path, scene_json));
    assert_true("mrt2_bad_write_asset", write_text_file(asset_path, "{}"));
    ray_tracing_runtime_mesh_asset_set_init(&set);
    ok = ray_tracing_runtime_mesh_assets_load_scene_file(scene_path,
                                                        &set,
                                                        diagnostics,
                                                        sizeof(diagnostics));
    assert_true("mrt2_bad_asset_rejected", !ok);
    assert_true("mrt2_bad_asset_diag_id", strstr(diagnostics, "asset_bad") != NULL);
    assert_true("mrt2_bad_asset_diag_invalid", strstr(diagnostics, "invalid") != NULL);
    ray_tracing_runtime_mesh_asset_set_free(&set);
    remove(asset_path);
    remove(scene_path);
    rmdir(mesh_dir);
    rmdir(assets_dir);
    rmdir(dir);
    return 0;
}

static int test_runtime_mesh_asset_loader_uses_line_drawing_runtime_path_hint(void) {
    const char* dir = "/private/tmp/ray_tracing_mrt2_external_hint";
    const char* scene_path = "/private/tmp/ray_tracing_mrt2_external_hint/scene_runtime.json";
    const char* source_asset_path =
        "tests/fixtures/mesh_asset_runtime_spheres/assets/mesh_assets/asset_sphere_8x4.runtime.json";
    char source_asset_abs[PATH_MAX];
    char scene_json[1024];
    RayTracingRuntimeMeshAssetSet set;
    char diagnostics[256] = {0};
    bool ok = false;

    assert_true("mrt2_external_hint_source_realpath",
                realpath(source_asset_path, source_asset_abs) != NULL);
    if (!source_asset_abs[0]) {
        snprintf(source_asset_abs, sizeof(source_asset_abs), "%s", source_asset_path);
    }

    snprintf(scene_json,
             sizeof(scene_json),
             "{"
             "\"world_scale\":1.0,"
             "\"objects\":[{"
             "\"object_id\":\"obj_external_mesh\","
             "\"object_type\":\"mesh_asset_instance\","
             "\"geometry_ref\":{\"kind\":\"mesh_asset\",\"id\":\"asset_sphere_8x4\"},"
             "\"extensions\":{\"line_drawing\":{\"runtime_mesh_path\":\"%s\"}}"
             "}]"
             "}",
             source_asset_abs);

    mkdir(dir, 0777);
    assert_true("mrt2_external_hint_write_scene", write_text_file(scene_path, scene_json));
    ray_tracing_runtime_mesh_asset_set_init(&set);
    ok = ray_tracing_runtime_mesh_assets_load_scene_file(scene_path,
                                                        &set,
                                                        diagnostics,
                                                        sizeof(diagnostics));
    assert_true("mrt2_external_hint_loads", ok);
    if (ok) {
        assert_true("mrt2_external_hint_asset_count", set.asset_count == 1);
        assert_true("mrt2_external_hint_instance_count", set.instance_count == 1);
        assert_true("mrt2_external_hint_path",
                    strstr(set.assets[0].path, "asset_sphere_8x4.runtime.json") != NULL);
    }
    ray_tracing_runtime_mesh_asset_set_free(&set);
    remove(scene_path);
    rmdir(dir);
    return 0;
}

static int test_runtime_mesh_asset_loader_falls_back_from_stale_line_drawing_hint_to_asset_root(void) {
    const char* dir = "/private/tmp/ray_tracing_mrt2_stale_external_hint";
    const char* mesh_root = "/private/tmp/ray_tracing_mrt2_stale_external_hint/curated";
    const char* scene_path = "/private/tmp/ray_tracing_mrt2_stale_external_hint/scene_runtime.json";
    const char* asset_path =
        "/private/tmp/ray_tracing_mrt2_stale_external_hint/curated/asset_sphere_8x4.runtime.json";
    const char* stale_asset_path =
        "/private/tmp/ray_tracing_mrt2_stale_external_hint/missing/asset_sphere_8x4.runtime.json";
    const char* source_asset_path =
        "tests/fixtures/mesh_asset_runtime_spheres/assets/mesh_assets/asset_sphere_8x4.runtime.json";
    const char* saved_root = getenv("RAY_TRACING_MESH_ASSET_ROOT");
    char saved_root_copy[PATH_MAX] = {0};
    char scene_json[1024];
    char* asset_text = NULL;
    RayTracingRuntimeMeshAssetSet set;
    char diagnostics[256] = {0};
    bool ok = false;

    if (saved_root && saved_root[0]) {
        strncpy(saved_root_copy, saved_root, sizeof(saved_root_copy) - 1);
        saved_root_copy[sizeof(saved_root_copy) - 1] = '\0';
    }

    asset_text = read_text_file_alloc(source_asset_path);
    assert_true("mrt2_stale_hint_read_source_asset", asset_text != NULL);
    mkdir(dir, 0777);
    mkdir(mesh_root, 0777);
    assert_true("mrt2_stale_hint_write_asset", asset_text && write_text_file(asset_path, asset_text));
    free(asset_text);

    snprintf(scene_json,
             sizeof(scene_json),
             "{"
             "\"world_scale\":1.0,"
             "\"objects\":[{"
             "\"object_id\":\"obj_external_mesh\","
             "\"object_type\":\"mesh_asset_instance\","
             "\"geometry_ref\":{\"kind\":\"mesh_asset\",\"id\":\"asset_sphere_8x4\"},"
             "\"extensions\":{\"line_drawing\":{\"runtime_mesh_path\":\"%s\"}}"
             "}]"
             "}",
             stale_asset_path);

    assert_true("mrt2_stale_hint_write_scene", write_text_file(scene_path, scene_json));
    setenv("RAY_TRACING_MESH_ASSET_ROOT", mesh_root, 1);
    ray_tracing_runtime_mesh_asset_set_init(&set);
    ok = ray_tracing_runtime_mesh_assets_load_scene_file(scene_path,
                                                        &set,
                                                        diagnostics,
                                                        sizeof(diagnostics));
    assert_true("mrt2_stale_hint_fallback_loads", ok);
    if (ok) {
        assert_true("mrt2_stale_hint_fallback_asset_count", set.asset_count == 1);
        assert_true("mrt2_stale_hint_fallback_instance_count", set.instance_count == 1);
        assert_true("mrt2_stale_hint_fallback_path",
                    strcmp(set.assets[0].path, asset_path) == 0);
    }
    ray_tracing_runtime_mesh_asset_set_free(&set);
    if (saved_root_copy[0]) {
        setenv("RAY_TRACING_MESH_ASSET_ROOT", saved_root_copy, 1);
    } else {
        unsetenv("RAY_TRACING_MESH_ASSET_ROOT");
    }
    remove(asset_path);
    remove(scene_path);
    rmdir(mesh_root);
    rmdir(dir);
    return 0;
}

static int test_runtime_mesh_asset_loader_uses_config_mesh_asset_root(void) {
    const char* dir = "/private/tmp/ray_tracing_mrt2_config_mesh_root";
    const char* mesh_root = "/private/tmp/ray_tracing_mrt2_config_mesh_root/curated";
    const char* scene_path = "/private/tmp/ray_tracing_mrt2_config_mesh_root/scene_runtime.json";
    const char* asset_path =
        "/private/tmp/ray_tracing_mrt2_config_mesh_root/curated/asset_sphere_8x4.runtime.json";
    const char* stale_asset_path =
        "/private/tmp/ray_tracing_mrt2_config_mesh_root/missing/asset_sphere_8x4.runtime.json";
    const char* source_asset_path =
        "tests/fixtures/mesh_asset_runtime_spheres/assets/mesh_assets/asset_sphere_8x4.runtime.json";
    const char* saved_env_root = getenv("RAY_TRACING_MESH_ASSET_ROOT");
    char saved_env_root_copy[PATH_MAX] = {0};
    char saved_config_root[PATH_MAX] = {0};
    char scene_json[1024];
    char* asset_text = NULL;
    RayTracingRuntimeMeshAssetSet set;
    char diagnostics[256] = {0};
    bool ok = false;

    if (saved_env_root && saved_env_root[0]) {
        strncpy(saved_env_root_copy, saved_env_root, sizeof(saved_env_root_copy) - 1);
        saved_env_root_copy[sizeof(saved_env_root_copy) - 1] = '\0';
    }
    strncpy(saved_config_root, animSettings.meshAssetRoot, sizeof(saved_config_root) - 1);
    saved_config_root[sizeof(saved_config_root) - 1] = '\0';

    asset_text = read_text_file_alloc(source_asset_path);
    assert_true("mrt2_config_mesh_root_read_source_asset", asset_text != NULL);
    mkdir(dir, 0777);
    mkdir(mesh_root, 0777);
    assert_true("mrt2_config_mesh_root_write_asset", asset_text && write_text_file(asset_path, asset_text));
    free(asset_text);

    snprintf(scene_json,
             sizeof(scene_json),
             "{"
             "\"world_scale\":1.0,"
             "\"objects\":[{"
             "\"object_id\":\"obj_config_mesh_root\","
             "\"object_type\":\"mesh_asset_instance\","
             "\"geometry_ref\":{\"kind\":\"mesh_asset\",\"id\":\"asset_sphere_8x4\"},"
             "\"extensions\":{\"line_drawing\":{\"runtime_mesh_path\":\"%s\"}}"
             "}]"
             "}",
             stale_asset_path);

    assert_true("mrt2_config_mesh_root_write_scene", write_text_file(scene_path, scene_json));
    unsetenv("RAY_TRACING_MESH_ASSET_ROOT");
    snprintf(animSettings.meshAssetRoot, sizeof(animSettings.meshAssetRoot), "%s", mesh_root);

    ray_tracing_runtime_mesh_asset_set_init(&set);
    ok = ray_tracing_runtime_mesh_assets_load_scene_file(scene_path,
                                                        &set,
                                                        diagnostics,
                                                        sizeof(diagnostics));
    assert_true("mrt2_config_mesh_root_loads", ok);
    if (ok) {
        assert_true("mrt2_config_mesh_root_asset_count", set.asset_count == 1);
        assert_true("mrt2_config_mesh_root_instance_count", set.instance_count == 1);
        assert_true("mrt2_config_mesh_root_path",
                    strcmp(set.assets[0].path, asset_path) == 0);
    }
    ray_tracing_runtime_mesh_asset_set_free(&set);

    snprintf(animSettings.meshAssetRoot, sizeof(animSettings.meshAssetRoot), "%s", saved_config_root);
    if (saved_env_root_copy[0]) {
        setenv("RAY_TRACING_MESH_ASSET_ROOT", saved_env_root_copy, 1);
    } else {
        unsetenv("RAY_TRACING_MESH_ASSET_ROOT");
    }
    remove(asset_path);
    remove(scene_path);
    rmdir(mesh_root);
    rmdir(dir);
    return 0;
}

static int test_runtime_mesh_asset_loader_attaches_preview_metadata(void) {
    const char* dir = "/private/tmp/ray_tracing_mrt_preview_meta";
    const char* assets_dir = "/private/tmp/ray_tracing_mrt_preview_meta/assets";
    const char* mesh_dir = "/private/tmp/ray_tracing_mrt_preview_meta/assets/mesh_assets";
    const char* scene_path = "/private/tmp/ray_tracing_mrt_preview_meta/scene_runtime.json";
    const char* asset_path =
        "/private/tmp/ray_tracing_mrt_preview_meta/assets/mesh_assets/asset_sphere_8x4.runtime.json";
    const char* source_asset_path =
        "tests/fixtures/mesh_asset_runtime_spheres/assets/mesh_assets/asset_sphere_8x4.runtime.json";
    const char* scene_json =
        "{"
        "\"objects\":[{"
        "\"object_id\":\"obj_preview_mesh\","
        "\"object_type\":\"mesh_asset_instance\","
        "\"geometry_ref\":{\"kind\":\"mesh_asset\",\"id\":\"asset_sphere_8x4\"}"
        "}]"
        "}";
    RayTracingRuntimeMeshAssetSet set;
    CoreResult preview_result = core_result_ok();
    char diagnostics[256] = {0};
    char preview_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    char* asset_text = NULL;
    bool ok = false;

    mkdir(dir, 0777);
    mkdir(assets_dir, 0777);
    mkdir(mesh_dir, 0777);
    asset_text = read_text_file_alloc(source_asset_path);
    assert_true("mrt_preview_meta_read_source_asset", asset_text != NULL);
    assert_true("mrt_preview_meta_write_asset", asset_text && write_text_file(asset_path, asset_text));
    assert_true("mrt_preview_meta_write_scene", write_text_file(scene_path, scene_json));
    preview_result = core_mesh_preview_save_for_runtime_file(asset_path,
                                                            CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1,
                                                            32u,
                                                            preview_path,
                                                            sizeof(preview_path));
    assert_true("mrt_preview_meta_sidecar_write", preview_result.code == CORE_OK);

    ray_tracing_runtime_mesh_asset_set_init(&set);
    ok = ray_tracing_runtime_mesh_assets_load_scene_file(scene_path,
                                                        &set,
                                                        diagnostics,
                                                        sizeof(diagnostics));
    assert_true("mrt_preview_meta_load_scene", ok);
    if (ok) {
        const RayTracingRuntimeMeshPreviewInfo* preview = &set.assets[0].preview;
        assert_true("mrt_preview_meta_asset_count", set.asset_count == 1);
        assert_true("mrt_preview_meta_path_resolved", preview->preview_path_resolved);
        assert_true("mrt_preview_meta_file_exists", preview->preview_file_exists);
        assert_true("mrt_preview_meta_file_readable", preview->preview_file_readable);
        assert_true("mrt_preview_meta_schema_supported", preview->preview_schema_supported);
        assert_true("mrt_preview_meta_valid", preview->preview_metadata_valid);
        assert_true("mrt_preview_meta_drawable", preview->metadata.has_drawable_payload);
        assert_true("mrt_preview_meta_mode",
                    preview->metadata.mode == CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1);
        assert_true("mrt_preview_meta_source_triangles",
                    preview->metadata.source_triangle_count ==
                        set.assets[0].document.triangle_count);
        assert_true("mrt_preview_meta_budget", preview->metadata.max_budget == 32u);
    }
    ray_tracing_runtime_mesh_asset_set_free(&set);
    free(asset_text);
    remove(preview_path);
    remove(asset_path);
    remove(scene_path);
    rmdir(mesh_dir);
    rmdir(assets_dir);
    rmdir(dir);
    return 0;
}

static int test_runtime_mesh_asset_loader_reuses_cache_and_invalidates(void) {
    const char* dir = "/private/tmp/ray_tracing_mrt9_cache";
    const char* assets_dir = "/private/tmp/ray_tracing_mrt9_cache/assets";
    const char* mesh_dir = "/private/tmp/ray_tracing_mrt9_cache/assets/mesh_assets";
    const char* scene_path = "/private/tmp/ray_tracing_mrt9_cache/scene_runtime.json";
    const char* asset_path =
        "/private/tmp/ray_tracing_mrt9_cache/assets/mesh_assets/asset_sphere_8x4.runtime.json";
    const char* source_asset_path =
        "tests/fixtures/mesh_asset_runtime_spheres/assets/mesh_assets/asset_sphere_8x4.runtime.json";
    const char* scene_json =
        "{"
        "\"objects\":[{"
        "\"object_id\":\"obj_cached_mesh\","
        "\"object_type\":\"mesh_asset_instance\","
        "\"geometry_ref\":{\"kind\":\"mesh_asset\",\"id\":\"asset_sphere_8x4\"}"
        "}]"
        "}";
    RayTracingRuntimeMeshAssetSet set;
    char diagnostics[256] = {0};
    char* asset_text = NULL;
    char* mutated_asset_text = NULL;
    unsigned long long hits = 0u;
    unsigned long long misses = 0u;
    unsigned long long invalidations = 0u;
    int cached_assets = 0;
    bool ok = false;

    mkdir(dir, 0777);
    mkdir(assets_dir, 0777);
    mkdir(mesh_dir, 0777);
    asset_text = read_text_file_alloc(source_asset_path);
    assert_true("mrt9_cache_read_source_asset", asset_text != NULL);
    assert_true("mrt9_cache_write_scene", write_text_file(scene_path, scene_json));
    if (asset_text) {
        size_t len = strlen(asset_text);
        mutated_asset_text = (char*)malloc(len + 2u);
        if (mutated_asset_text) {
            memcpy(mutated_asset_text, asset_text, len);
            mutated_asset_text[len] = '\n';
            mutated_asset_text[len + 1u] = '\0';
        }
    }
    assert_true("mrt9_cache_write_asset", asset_text && write_text_file(asset_path, asset_text));

    ray_tracing_runtime_mesh_assets_reset_cache();
    ray_tracing_runtime_mesh_asset_set_init(&set);
    ok = ray_tracing_runtime_mesh_assets_load_scene_file(scene_path,
                                                        &set,
                                                        diagnostics,
                                                        sizeof(diagnostics));
    assert_true("mrt9_cache_first_load", ok);
    ray_tracing_runtime_mesh_asset_set_free(&set);
    ray_tracing_runtime_mesh_assets_cache_stats(&hits, &misses, &invalidations, &cached_assets);
    assert_true("mrt9_cache_first_miss", misses == 1u);
    assert_true("mrt9_cache_first_no_hits", hits == 0u);
    assert_true("mrt9_cache_first_cached_one", cached_assets == 1);

    ray_tracing_runtime_mesh_asset_set_init(&set);
    ok = ray_tracing_runtime_mesh_assets_load_scene_file(scene_path,
                                                        &set,
                                                        diagnostics,
                                                        sizeof(diagnostics));
    assert_true("mrt9_cache_second_load", ok);
    ray_tracing_runtime_mesh_asset_set_free(&set);
    ray_tracing_runtime_mesh_assets_cache_stats(&hits, &misses, &invalidations, &cached_assets);
    assert_true("mrt9_cache_second_hit", hits == 1u);
    assert_true("mrt9_cache_second_miss_stable", misses == 1u);

    assert_true("mrt9_cache_mutated_asset_ready", mutated_asset_text != NULL);
    if (mutated_asset_text) {
        assert_true("mrt9_cache_rewrite_asset", write_text_file(asset_path, mutated_asset_text));
        ray_tracing_runtime_mesh_asset_set_init(&set);
        ok = ray_tracing_runtime_mesh_assets_load_scene_file(scene_path,
                                                            &set,
                                                            diagnostics,
                                                            sizeof(diagnostics));
        assert_true("mrt9_cache_reload_after_change", ok);
        ray_tracing_runtime_mesh_asset_set_free(&set);
        ray_tracing_runtime_mesh_assets_cache_stats(&hits,
                                                   &misses,
                                                   &invalidations,
                                                   &cached_assets);
        assert_true("mrt9_cache_change_invalidates", invalidations == 1u);
        assert_true("mrt9_cache_change_misses_again", misses == 2u);
        assert_true("mrt9_cache_change_cached_one", cached_assets == 1);
    }

    ray_tracing_runtime_mesh_assets_reset_cache();
    free(asset_text);
    free(mutated_asset_text);
    remove(asset_path);
    remove(scene_path);
    rmdir(mesh_dir);
    rmdir(assets_dir);
    rmdir(dir);
    return 0;
}

static int test_runtime_mesh_asset_loader_preview_limited_skips_oversized_asset(void) {
    RayTracingRuntimeMeshAssetSet set;
    char diag[256] = {0};
    bool ok = false;

    ray_tracing_runtime_mesh_asset_set_init(&set);
    ok = ray_tracing_runtime_mesh_assets_load_scene_file_preview_limited(
        "tests/fixtures/mesh_asset_runtime_spheres/scene_runtime_pressure_mrt8.json",
        1024u * 1024u,
        &set,
        diag,
        sizeof(diag));
    assert_true("mrt2_preview_limited_load_ok", ok);
    if (ok) {
        assert_true("mrt2_preview_limited_asset_skipped", set.asset_count == 0);
        assert_true("mrt2_preview_limited_instance_skipped", set.instance_count == 0);
    }
    ray_tracing_runtime_mesh_asset_set_free(&set);
    return 0;
}

static int test_runtime_mesh_asset_loader_preview_limited_keeps_sidecar_metadata(void) {
    const char* dir = "/private/tmp/ray_tracing_mrt_preview_limited_meta";
    const char* assets_dir = "/private/tmp/ray_tracing_mrt_preview_limited_meta/assets";
    const char* mesh_dir = "/private/tmp/ray_tracing_mrt_preview_limited_meta/assets/mesh_assets";
    const char* scene_path = "/private/tmp/ray_tracing_mrt_preview_limited_meta/scene_runtime.json";
    const char* asset_path =
        "/private/tmp/ray_tracing_mrt_preview_limited_meta/assets/mesh_assets/asset_sphere_8x4.runtime.json";
    const char* source_asset_path =
        "tests/fixtures/mesh_asset_runtime_spheres/assets/mesh_assets/asset_sphere_8x4.runtime.json";
    const char* scene_json =
        "{"
        "\"objects\":[{"
        "\"object_id\":\"obj_preview_limited_mesh\","
        "\"object_type\":\"mesh_asset_instance\","
        "\"geometry_ref\":{\"kind\":\"mesh_asset\",\"id\":\"asset_sphere_8x4\"}"
        "}]"
        "}";
    RayTracingRuntimeMeshAssetSet set;
    CoreResult preview_result = core_result_ok();
    char diagnostics[256] = {0};
    char preview_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    char* asset_text = NULL;
    bool ok = false;

    mkdir(dir, 0777);
    mkdir(assets_dir, 0777);
    mkdir(mesh_dir, 0777);
    asset_text = read_text_file_alloc(source_asset_path);
    assert_true("mrt_preview_limited_meta_read_source_asset", asset_text != NULL);
    assert_true("mrt_preview_limited_meta_write_asset",
                asset_text && write_text_file(asset_path, asset_text));
    assert_true("mrt_preview_limited_meta_write_scene", write_text_file(scene_path, scene_json));
    preview_result = core_mesh_preview_save_for_runtime_file(asset_path,
                                                            CORE_MESH_PREVIEW_MODE_BOUNDS_PROXY_V1,
                                                            0u,
                                                            preview_path,
                                                            sizeof(preview_path));
    assert_true("mrt_preview_limited_meta_sidecar_write", preview_result.code == CORE_OK);

    ray_tracing_runtime_mesh_asset_set_init(&set);
    ok = ray_tracing_runtime_mesh_assets_load_scene_file_preview_limited(scene_path,
                                                                         1u,
                                                                         &set,
                                                                         diagnostics,
                                                                         sizeof(diagnostics));
    assert_true("mrt_preview_limited_meta_load_ok", ok);
    if (ok) {
        const RayTracingRuntimeMeshPreviewInfo* preview = &set.skipped_instances[0].preview;
        assert_true("mrt_preview_limited_meta_no_full_asset", set.asset_count == 0);
        assert_true("mrt_preview_limited_meta_no_full_instance", set.instance_count == 0);
        assert_true("mrt_preview_limited_meta_one_skipped", set.skipped_instance_count == 1);
        assert_true("mrt_preview_limited_meta_valid", preview->preview_metadata_valid);
        assert_true("mrt_preview_limited_meta_bounds_mode",
                    preview->metadata.mode == CORE_MESH_PREVIEW_MODE_BOUNDS_PROXY_V1);
        assert_true("mrt_preview_limited_meta_zero_edges",
                    preview->metadata.preview_edge_count == 0u);
        assert_true("mrt_preview_limited_meta_zero_triangles",
                    preview->metadata.preview_triangle_count == 0u);
        assert_true("mrt_preview_limited_meta_zero_vertices",
                    preview->metadata.preview_vertex_count == 0u);
        assert_true("mrt_preview_limited_meta_radius",
                    preview->metadata.bounding_sphere_radius > 0.0);
    }

    ray_tracing_runtime_mesh_asset_set_free(&set);
    free(asset_text);
    remove(preview_path);
    remove(asset_path);
    remove(scene_path);
    rmdir(mesh_dir);
    rmdir(assets_dir);
    rmdir(dir);
    return 0;
}

int run_test_runtime_mesh_asset_loader_tests(void) {
    int before = g_runtime_mesh_asset_loader_failures;

    test_runtime_mesh_asset_loader_resolves_and_loads_mrt0_fixture();
    test_runtime_mesh_asset_loader_reports_missing_asset();
    test_runtime_mesh_asset_loader_reports_malformed_asset();
    test_runtime_mesh_asset_loader_uses_line_drawing_runtime_path_hint();
    test_runtime_mesh_asset_loader_falls_back_from_stale_line_drawing_hint_to_asset_root();
    test_runtime_mesh_asset_loader_uses_config_mesh_asset_root();
    test_runtime_mesh_asset_loader_attaches_preview_metadata();
    test_runtime_mesh_asset_loader_reuses_cache_and_invalidates();
    test_runtime_mesh_asset_loader_preview_limited_skips_oversized_asset();
    test_runtime_mesh_asset_loader_preview_limited_keeps_sidecar_metadata();

    return g_runtime_mesh_asset_loader_failures - before;
}

#ifdef RAY_TRACING_RUNTIME_MESH_ASSET_LOADER_STANDALONE
int main(void) {
    return run_test_runtime_mesh_asset_loader_tests() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
#endif
