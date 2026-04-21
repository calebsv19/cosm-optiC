#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "render/material_bsdf.h"
#include "material/material_manager.h"
#include "render/fast_rng.h"
#include "config/config_manager.h"
#include "app/data_paths.h"
#include "app/animation.h"
#include "editor/editor_mode_router.h"
#include "editor/scene_editor_control_surface.h"
#include "editor/scene_editor_runtime_scene_persistence.h"
#include "render/ray_tracing2.h"
#include "render/ray_tracing_mode_backend.h"
#include "render/integrator_common.h"
#include "render/integrators/direct_light_integrator.h"
#include "render/integrators/forward_light_integrator.h"
#include "render/integrators/hybrid/camera_path_integrator.h"
#include "render/uniform_grid.h"
#include "render/light_pdf.h"
#include "render/ray_types.h"
#include "render/render_helper.h"
#include "import/runtime_scene_bridge.h"
#include "path/path_system.h"
#include "core_scene_compile.h"
#include "ui/scene_source_catalog.h"
#include "ui/sdl_menu_state.h"
#include "fluid_pack_import_test.h"
#include "kit_viz_fluid_overlay_adapter_test.h"
#include "render_metrics_dataset_test.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int failures = 0;
static const char* kRuntimeSceneConfigPath = "data/runtime/scene_config.json";
static const char* kRuntimeAnimationConfigPath = "data/runtime/animation_config.json";

static void assert_close(const char* name, double a, double b, double tol) {
    if (fabs(a - b) > tol) {
        printf("FAIL %-32s got=%.6f expected=%.6f (tol=%.6f)\n", name, a, b, tol);
        failures++;
    }
}

static void assert_true(const char* name, bool cond) {
    if (!cond) {
        printf("FAIL %-32s condition=false\n", name);
        failures++;
    }
}

static char* read_text_file_alloc(const char* path, size_t* out_size) {
    if (out_size) *out_size = 0;
    if (!path) return NULL;
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    char* buffer = (char*)malloc((size_t)size + 1u);
    if (!buffer) {
        fclose(f);
        return NULL;
    }
    size_t read_bytes = fread(buffer, 1, (size_t)size, f);
    fclose(f);
    if (read_bytes != (size_t)size) {
        free(buffer);
        return NULL;
    }
    buffer[size] = '\0';
    if (out_size) *out_size = (size_t)size;
    return buffer;
}

static bool write_text_file(const char* path, const char* text) {
    if (!path || !text) return false;
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    size_t len = strlen(text);
    size_t written = fwrite(text, 1, len, f);
    fclose(f);
    return written == len;
}

static bool path_list_contains(const char *const *paths,
                               size_t count,
                               const char *needle) {
    if (!paths || !needle || !needle[0]) return false;
    for (size_t i = 0; i < count; ++i) {
        if (paths[i] && strcmp(paths[i], needle) == 0) {
            return true;
        }
    }
    return false;
}

static bool catalog_contains_path_source(const SceneSourceCatalogEntry *entries,
                                         size_t count,
                                         const char *path,
                                         int source) {
    char resolved[PATH_MAX];
    const char *needle = path;
    if (!entries || !path || !path[0]) return false;
    if (realpath(path, resolved)) {
        needle = resolved;
    }
    for (size_t i = 0; i < count; ++i) {
        if (entries[i].source == source && strcmp(entries[i].path, needle) == 0) {
            return true;
        }
    }
    return false;
}

static bool catalog_contains_path_any_source(const SceneSourceCatalogEntry *entries,
                                             size_t count,
                                             const char *path) {
    char resolved[PATH_MAX];
    const char *needle = path;
    if (!entries || !path || !path[0]) return false;
    if (realpath(path, resolved)) {
        needle = resolved;
    }
    for (size_t i = 0; i < count; ++i) {
        if (strcmp(entries[i].path, needle) == 0) {
            return true;
        }
    }
    return false;
}

static size_t catalog_count_source(const SceneSourceCatalogEntry *entries,
                                   size_t count,
                                   int source) {
    size_t total = 0;
    if (!entries) return 0;
    for (size_t i = 0; i < count; ++i) {
        if (entries[i].source == source) total += 1;
    }
    return total;
}

static int digest_count_kind(const RuntimeSceneBridge3DDigestState *digest,
                             RuntimeSceneBridgePrimitiveKind kind) {
    int total = 0;
    if (!digest) return 0;
    for (int i = 0; i < digest->primitive_count; ++i) {
        if (digest->primitives[i].kind == kind) total += 1;
    }
    return total;
}

static void restore_runtime_scene_config(char* backup, size_t backup_size) {
    if (backup) {
        FILE* f = fopen(kRuntimeSceneConfigPath, "wb");
        if (f) {
            fwrite(backup, 1, backup_size, f);
            fclose(f);
        }
        free(backup);
        return;
    }
    remove(kRuntimeSceneConfigPath);
}

static void restore_runtime_animation_config(char* backup, size_t backup_size) {
    if (backup) {
        FILE* f = fopen(kRuntimeAnimationConfigPath, "wb");
        if (f) {
            fwrite(backup, 1, backup_size, f);
            fclose(f);
        }
        free(backup);
        return;
    }
    remove(kRuntimeAnimationConfigPath);
}

static MaterialBSDF make_diffuse(double albedo) {
    MaterialBSDF m = {0};
    m.albedo = albedo;
    m.diffuseWeight = 1.0;
    m.specWeight = 0.0;
    m.reflectivity = 0.0;
    m.roughness = 0.5;
    m.weightSum = 1.0;
    m.model = MATERIAL_BSDF_LAMBERT;
    return m;
}

static int test_diffuse_evaluate(void) {
    MaterialBSDF m = make_diffuse(0.8);
    double nx = 0.0, ny = 1.0;
    double inX = 0.0, inY = 1.0;
    double outX = 0.0, outY = 1.0;
    double v = MaterialBSDFEvaluateCos(&m, nx, ny, inX, inY, outX, outY);
    assert_close("diffuse_evaluate_cos", v, 0.8 / M_PI, 1e-4);
    return 0;
}

static int test_diffuse_pdf(void) {
    MaterialBSDF m = make_diffuse(0.5);
    double nx = 0.0, ny = 1.0;
    double inX = 0.0, inY = 1.0;
    double outX = 0.0, outY = 1.0;
    double pdf = MaterialBSDFAngularPdf(&m, nx, ny, inX, inY, outX, outY);
    assert_close("diffuse_pdf", pdf, 1.0 / M_PI, 1e-6);
    return 0;
}

static int test_sample_diffuse_consistency(void) {
    MaterialBSDF m = make_diffuse(0.7);
    double nx = 0.0, ny = 1.0;
    double inX = 0.0, inY = 1.0;
    FastRNG rng;
    FastRNGSeed(&rng, 12345u, 6789u);
    BSDFSample s = {0};
    bool ok = MaterialBSDFSample(&m, nx, ny, inX, inY, 0.0, &rng, &s);
    assert_true("sample_diffuse_valid", ok);
    if (!ok) return 0;
    double dot = s.dirX * nx + s.dirY * ny;
    assert_true("sample_diffuse_hemisphere", dot > 0.0);
    assert_true("sample_diffuse_pdf_pos", s.pdf > 0.0);
    assert_true("sample_diffuse_weight_pos", s.weight > 0.0);
    if (s.pdf > 0.0) {
        double ratio = s.weight / s.pdf;
        assert_close("sample_diffuse_weight_over_pdf", ratio, m.albedo, 0.05);
    }
    return 0;
}

static int test_scene_object_z_roundtrip(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeSceneConfigPath, &backup_size);

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.windowWidth = 320;
    sceneSettings.windowHeight = 240;
    sceneSettings.camera.x = 10.0;
    sceneSettings.camera.y = 20.0;
    sceneSettings.camera.zoom = 1.0;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.cameraMargin = 24.0;
    sceneSettings.rays = 128;

    sceneSettings.objectCount = 2;

    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 12.0, 34.0, 5.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].z = 7.25;

    double tri[3][2] = {
        {-10.0, -10.0},
        {10.0, -10.0},
        {0.0, 10.0}
    };
    InitObject(&sceneSettings.sceneObjects[1], OBJECT_POLYGON, -40.0, 5.0, 0.0, 0.0, tri, 3);
    sceneSettings.sceneObjects[1].z = -2.5;

    sceneSettings.bezierPath.numPoints = 2;
    sceneSettings.bezierPath.mode = BEZIER_CUBIC;
    sceneSettings.bezierPath.points[0].x = 0.0;
    sceneSettings.bezierPath.points[0].y = 0.0;
    sceneSettings.bezierPath.points[1].x = 10.0;
    sceneSettings.bezierPath.points[1].y = 10.0;
    sceneSettings.cameraPath.numPoints = 1;
    sceneSettings.cameraPath.mode = BEZIER_CUBIC;
    sceneSettings.cameraPath.points[0].x = 10.0;
    sceneSettings.cameraPath.points[0].y = 20.0;

    SaveSceneConfig();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    LoadSceneConfig();

    assert_true("scene_z_roundtrip_object_count", sceneSettings.objectCount >= 2);
    if (sceneSettings.objectCount >= 2) {
        assert_close("scene_z_roundtrip_obj0", sceneSettings.sceneObjects[0].z, 7.25, 1e-6);
        assert_close("scene_z_roundtrip_obj1", sceneSettings.sceneObjects[1].z, -2.5, 1e-6);
    }

    restore_runtime_scene_config(backup, backup_size);
    return 0;
}

static int test_scene_object_z_missing_fallback(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeSceneConfigPath, &backup_size);

    // Ensure runtime lane exists, then inject a minimal scene file without object.z.
    SaveSceneConfig();

    const char* json_no_z =
        "{\n"
        "  \"window\": {\"width\": 200, \"height\": 120},\n"
        "  \"camera\": {\"x\": 0.0, \"y\": 0.0, \"zoom\": 1.0, \"rotation\": 0.0, \"margin\": 10.0},\n"
        "  \"rays\": 64,\n"
        "  \"objects\": [\n"
        "    {\"type\": \"circle\", \"x\": 1.0, \"y\": 2.0, \"radius\": 3.0, \"scale\": 1.0, \"rotation\": 0.0}\n"
        "  ],\n"
        "  \"path\": {\"mode\": \"BEZIER_CUBIC\", \"points\": [{\"x\": 0.0, \"y\": 0.0}, {\"x\": 2.0, \"y\": 2.0}]},\n"
        "  \"cameraPath\": {\"mode\": \"BEZIER_CUBIC\", \"points\": [{\"x\": 0.0, \"y\": 0.0}]}\n"
        "}\n";

    bool wrote = write_text_file(kRuntimeSceneConfigPath, json_no_z);
    assert_true("scene_z_missing_write_runtime", wrote);
    if (wrote) {
        memset(&sceneSettings, 0, sizeof(sceneSettings));
        LoadSceneConfig();
        assert_true("scene_z_missing_object_count", sceneSettings.objectCount >= 1);
        if (sceneSettings.objectCount >= 1) {
            assert_close("scene_z_missing_default_zero", sceneSettings.sceneObjects[0].z, 0.0, 1e-9);
        }
    }

    restore_runtime_scene_config(backup, backup_size);
    return 0;
}

static int test_animation_scene_source_legacy_migration(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char* json_legacy_fluid =
        "{\n"
        "  \"inputRoot\": \"config\",\n"
        "  \"outputRoot\": \"data/runtime\",\n"
        "  \"spaceMode\": 0,\n"
        "  \"useFluidScene\": true,\n"
        "  \"fluidManifest\": \"import/legacy_manifest.json\"\n"
        "}\n";
    const char* json_legacy_missing_manifest =
        "{\n"
        "  \"inputRoot\": \"config\",\n"
        "  \"outputRoot\": \"data/runtime\",\n"
        "  \"spaceMode\": 0,\n"
        "  \"useFluidScene\": true,\n"
        "  \"fluidManifest\": \"\"\n"
        "}\n";

    bool wrote = write_text_file(kRuntimeAnimationConfigPath, json_legacy_fluid);
    assert_true("scene_source_legacy_write_fluid", wrote);
    if (wrote) {
        LoadAnimationConfig();
        assert_true("scene_source_legacy_migrates_to_fluid",
                    animSettings.sceneSource == SCENE_SOURCE_FLUID_MANIFEST);
        assert_true("scene_source_legacy_use_fluid_true", animSettings.useFluidScene);
    }

    wrote = write_text_file(kRuntimeAnimationConfigPath, json_legacy_missing_manifest);
    assert_true("scene_source_legacy_write_missing_manifest", wrote);
    if (wrote) {
        LoadAnimationConfig();
        assert_true("scene_source_legacy_missing_manifest_fallback_2d",
                    animSettings.sceneSource == SCENE_SOURCE_CONFIG_2D);
        assert_true("scene_source_legacy_missing_manifest_use_fluid_false",
                    !animSettings.useFluidScene);
    }

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_scene_source_roundtrip_runtime_lane(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);

    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    animSettings.useFluidScene = true;
    strncpy(animSettings.fluidManifest, "import/should_not_drive_runtime_lane.json",
            sizeof(animSettings.fluidManifest) - 1);
    animSettings.fluidManifest[sizeof(animSettings.fluidManifest) - 1] = '\0';
    strncpy(animSettings.runtimeScenePath, "../shared/assets/scenes/trio_contract/scene_runtime_min.json",
            sizeof(animSettings.runtimeScenePath) - 1);
    animSettings.runtimeScenePath[sizeof(animSettings.runtimeScenePath) - 1] = '\0';

    SaveAnimationConfig();
    animSettings.sceneSource = SCENE_SOURCE_CONFIG_2D;
    animSettings.useFluidScene = false;
    animSettings.fluidManifest[0] = '\0';
    animSettings.runtimeScenePath[0] = '\0';
    LoadAnimationConfig();

    assert_true("scene_source_roundtrip_runtime_lane",
                animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE);
    assert_true("scene_source_roundtrip_runtime_not_fluid", !animSettings.useFluidScene);
    assert_true("scene_source_roundtrip_runtime_path",
                strcmp(animSettings.runtimeScenePath,
                       "../shared/assets/scenes/trio_contract/scene_runtime_min.json") == 0);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_scene_source_select_runtime_failure_rolls_back(void) {
    static const char *kFixtureRuntimePath = "../shared/assets/scenes/trio_contract/scene_runtime_min.json";
    static const char *kMissingRuntimePath = "/tmp/ray_tracing_missing_scene_source_contract.json";
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);

    remove(kMissingRuntimePath);
    MaterialManagerInit();
    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    animSettings.useFluidScene = false;
    animSettings.fluidManifest[0] = '\0';
    strncpy(animSettings.runtimeScenePath, kFixtureRuntimePath, sizeof(animSettings.runtimeScenePath) - 1);
    animSettings.runtimeScenePath[sizeof(animSettings.runtimeScenePath) - 1] = '\0';

    assert_true("scene_source_select_runtime_baseline_source_runtime",
                animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE);
    assert_true("scene_source_select_runtime_baseline_runtime_path",
                strcmp(animSettings.runtimeScenePath, kFixtureRuntimePath) == 0);

    assert_true("scene_source_select_runtime_missing_rejected",
                !AnimationSelectSceneSource(SCENE_SOURCE_RUNTIME_SCENE,
                                            kMissingRuntimePath,
                                            true));
    assert_true("scene_source_select_runtime_rollback_source_runtime",
                animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE);
    assert_true("scene_source_select_runtime_rollback_runtime_path",
                strcmp(animSettings.runtimeScenePath, kFixtureRuntimePath) == 0);
    assert_true("scene_source_select_runtime_rollback_not_fluid",
                !animSettings.useFluidScene);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_scene_source_select_runtime_persists_on_save(void) {
    static const char *kFixtureRuntimePath = "../shared/assets/scenes/trio_contract/scene_runtime_min.json";
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);

    MaterialManagerInit();
    assert_true("scene_source_select_runtime_apply_ok",
                AnimationSelectSceneSource(SCENE_SOURCE_RUNTIME_SCENE,
                                           kFixtureRuntimePath,
                                           false));
    assert_true("scene_source_select_runtime_apply_source_runtime",
                animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE);
    assert_true("scene_source_select_runtime_apply_runtime_path",
                strcmp(animSettings.runtimeScenePath, kFixtureRuntimePath) == 0);
    assert_true("scene_source_select_runtime_apply_not_fluid",
                !animSettings.useFluidScene);

    SaveAnimationConfig();
    animSettings.sceneSource = SCENE_SOURCE_CONFIG_2D;
    animSettings.useFluidScene = false;
    animSettings.fluidManifest[0] = '\0';
    animSettings.runtimeScenePath[0] = '\0';
    LoadAnimationConfig();

    assert_true("scene_source_select_runtime_persist_source_runtime",
                animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE);
    assert_true("scene_source_select_runtime_persist_runtime_path",
                strcmp(animSettings.runtimeScenePath, kFixtureRuntimePath) == 0);
    assert_true("scene_source_select_runtime_persist_not_fluid",
                !animSettings.useFluidScene);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_apply_active_scene_source_invalid_fluid_falls_back_2d(void) {
    static const char *kMissingFluidPath = "/tmp/ray_tracing_missing_fluid_manifest.json";
    static const char *kRuntimeFixturePath = "../shared/assets/scenes/trio_contract/scene_runtime_min.json";
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);

    remove(kMissingFluidPath);
    animSettings.sceneSource = SCENE_SOURCE_FLUID_MANIFEST;
    animSettings.useFluidScene = true;
    strncpy(animSettings.fluidManifest, kMissingFluidPath, sizeof(animSettings.fluidManifest) - 1);
    animSettings.fluidManifest[sizeof(animSettings.fluidManifest) - 1] = '\0';
    strncpy(animSettings.runtimeScenePath, kRuntimeFixturePath, sizeof(animSettings.runtimeScenePath) - 1);
    animSettings.runtimeScenePath[sizeof(animSettings.runtimeScenePath) - 1] = '\0';

    assert_true("scene_source_invalid_fluid_apply_rejected",
                !AnimationApplyActiveSceneSource());
    assert_true("scene_source_invalid_fluid_fallback_source_2d",
                animSettings.sceneSource == SCENE_SOURCE_CONFIG_2D);
    assert_true("scene_source_invalid_fluid_fallback_not_fluid",
                !animSettings.useFluidScene);
    assert_true("scene_source_invalid_fluid_fallback_fluid_path_cleared",
                animSettings.fluidManifest[0] == '\0');
    assert_true("scene_source_invalid_fluid_fallback_runtime_path_cleared",
                animSettings.runtimeScenePath[0] == '\0');

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_restore_active_scene_source_persists_fallback_correction(void) {
    static const char *kMissingRuntimePath = "/tmp/ray_tracing_missing_restore_runtime_scene.json";
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char *json_invalid_runtime_source =
        "{\n"
        "  \"inputRoot\": \"config\",\n"
        "  \"outputRoot\": \"data/runtime\",\n"
        "  \"spaceMode\": 1,\n"
        "  \"sceneSource\": 2,\n"
        "  \"useFluidScene\": false,\n"
        "  \"fluidManifest\": \"\",\n"
        "  \"runtimeScenePath\": \"/tmp/ray_tracing_missing_restore_runtime_scene.json\"\n"
        "}\n";

    remove(kMissingRuntimePath);
    assert_true("scene_source_restore_write_invalid_runtime",
                write_text_file(kRuntimeAnimationConfigPath, json_invalid_runtime_source));
    LoadAnimationConfig();

    assert_true("scene_source_restore_invalid_runtime_rejected",
                !AnimationRestoreActiveSceneSource(true));
    assert_true("scene_source_restore_invalid_runtime_fallback_source_2d",
                animSettings.sceneSource == SCENE_SOURCE_CONFIG_2D);
    assert_true("scene_source_restore_invalid_runtime_fallback_runtime_path_cleared",
                animSettings.runtimeScenePath[0] == '\0');

    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    animSettings.runtimeScenePath[0] = 'x';
    animSettings.runtimeScenePath[1] = '\0';
    LoadAnimationConfig();
    assert_true("scene_source_restore_persisted_source_2d",
                animSettings.sceneSource == SCENE_SOURCE_CONFIG_2D);
    assert_true("scene_source_restore_persisted_runtime_path_cleared",
                animSettings.runtimeScenePath[0] == '\0');

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_manifest_default_roots_expands_runtime_and_legacy_paths(void) {
    const char *input_env = getenv("RAY_TRACING_INPUT_ROOT");
    const char *output_env = getenv("RAY_TRACING_OUTPUT_ROOT");
    char input_backup[PATH_MAX] = {0};
    char output_backup[PATH_MAX] = {0};
    bool had_input_env = false;
    bool had_output_env = false;
    const char **roots = NULL;
    size_t root_count = 0;

    if (input_env && input_env[0]) {
        strncpy(input_backup, input_env, sizeof(input_backup) - 1);
        input_backup[sizeof(input_backup) - 1] = '\0';
        had_input_env = true;
    }
    if (output_env && output_env[0]) {
        strncpy(output_backup, output_env, sizeof(output_backup) - 1);
        output_backup[sizeof(output_backup) - 1] = '\0';
        had_output_env = true;
    }

    unsetenv("RAY_TRACING_INPUT_ROOT");
    unsetenv("RAY_TRACING_OUTPUT_ROOT");

    root_count = ray_tracing_manifest_default_roots(&roots);
    assert_true("manifest_roots_nonempty", root_count > 0 && roots != NULL);
    assert_true("manifest_roots_has_config_samples",
                path_list_contains(roots, root_count, "config/samples"));
    assert_true("manifest_roots_has_runtime_scenes",
                path_list_contains(roots, root_count, "data/runtime/scenes"));
    assert_true("manifest_roots_has_legacy_physics_samples",
                path_list_contains(roots, root_count, "../physics_sim/config/samples"));

    if (had_input_env) {
        setenv("RAY_TRACING_INPUT_ROOT", input_backup, 1);
    } else {
        unsetenv("RAY_TRACING_INPUT_ROOT");
    }
    if (had_output_env) {
        setenv("RAY_TRACING_OUTPUT_ROOT", output_backup, 1);
    } else {
        unsetenv("RAY_TRACING_OUTPUT_ROOT");
    }
    return 0;
}

static int test_scene_source_catalog_collect_admits_runtime_and_manifest_lanes(void) {
    char tmp_template[] = "/tmp/ray_tracing_catalog_s6_XXXXXX";
    char *tmp_root = mkdtemp(tmp_template);
    char runtime_path[PATH_MAX];
    char authoring_path[PATH_MAX];
    char manifest_path[PATH_MAX];
    const char *roots[1];
    SceneSourceCatalogEntry entries[16];
    size_t entry_count = 0;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_catalog_runtime\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[]"
        "}";
    const char *authoring_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_catalog_authoring\","
        "\"objects\":[]"
        "}";
    const char *manifest_json =
        "{"
        "\"schema\":\"fluid_manifest_v1\","
        "\"meta\":{\"name\":\"s6_catalog\"}"
        "}";

    assert_true("scene_catalog_tmpdir_created", tmp_root != NULL);
    if (!tmp_root) return 0;

    snprintf(runtime_path, sizeof(runtime_path), "%s/scene_runtime.json", tmp_root);
    snprintf(authoring_path, sizeof(authoring_path), "%s/scene_authoring.json", tmp_root);
    snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.json", tmp_root);

    assert_true("scene_catalog_write_runtime", write_text_file(runtime_path, runtime_json));
    assert_true("scene_catalog_write_authoring", write_text_file(authoring_path, authoring_json));
    assert_true("scene_catalog_write_manifest", write_text_file(manifest_path, manifest_json));

    roots[0] = tmp_root;
    entry_count = scene_source_catalog_collect(entries,
                                               sizeof(entries) / sizeof(entries[0]),
                                               roots,
                                               1,
                                               manifest_path,
                                               authoring_path);

    assert_true("scene_catalog_entry_count_min", entry_count >= 2);
    assert_true("scene_catalog_runtime_count_one",
                catalog_count_source(entries, entry_count, SCENE_SOURCE_RUNTIME_SCENE) == 1);
    assert_true("scene_catalog_manifest_present",
                catalog_contains_path_source(entries,
                                             entry_count,
                                             manifest_path,
                                             SCENE_SOURCE_FLUID_MANIFEST));
    assert_true("scene_catalog_runtime_present",
                catalog_contains_path_source(entries,
                                             entry_count,
                                             runtime_path,
                                             SCENE_SOURCE_RUNTIME_SCENE));
    assert_true("scene_catalog_reject_authoring_variant",
                !catalog_contains_path_any_source(entries, entry_count, authoring_path));

    remove(runtime_path);
    remove(authoring_path);
    remove(manifest_path);
    rmdir(tmp_root);
    return 0;
}

static int test_menu_state_manifest_option_visibility_matrix(void) {
    const int original_space_mode = animSettings.spaceMode;
    MenuRuntimeState state = {0};
    ManifestOption config_2d = {0};
    ManifestOption fluid_manifest = {0};
    ManifestOption runtime_scene = {0};

    config_2d.source = SCENE_SOURCE_CONFIG_2D;
    fluid_manifest.source = SCENE_SOURCE_FLUID_MANIFEST;
    runtime_scene.source = SCENE_SOURCE_RUNTIME_SCENE;

    animSettings.spaceMode = SPACE_MODE_2D;
    assert_true("menu_manifest_visible_2d_config",
                menu_state_manifest_option_visible(&state, &config_2d));
    assert_true("menu_manifest_visible_2d_fluid",
                menu_state_manifest_option_visible(&state, &fluid_manifest));
    assert_true("menu_manifest_visible_2d_runtime_hidden",
                !menu_state_manifest_option_visible(&state, &runtime_scene));

    animSettings.spaceMode = SPACE_MODE_3D;
    assert_true("menu_manifest_visible_3d_runtime",
                menu_state_manifest_option_visible(&state, &runtime_scene));
    assert_true("menu_manifest_visible_3d_config_hidden",
                !menu_state_manifest_option_visible(&state, &config_2d));
    assert_true("menu_manifest_visible_3d_fluid_hidden",
                !menu_state_manifest_option_visible(&state, &fluid_manifest));

    animSettings.spaceMode = original_space_mode;
    return 0;
}

static int test_depth_projection_scalars(void) {
    double scale_far = RenderHelper_DepthScaleForObjectZ(4.0);
    double scale_near = RenderHelper_DepthScaleForObjectZ(-4.0);
    double scale_zero = RenderHelper_DepthScaleForObjectZ(0.0);
    double yoff_far = RenderHelper_DepthYOffsetPixelsForObjectZ(3.0, 1.0);
    double yoff_near = RenderHelper_DepthYOffsetPixelsForObjectZ(-3.0, 1.0);

    assert_true("depth_scale_far_smaller_than_zero", scale_far < scale_zero);
    assert_true("depth_scale_near_larger_than_zero", scale_near > scale_zero);
    assert_true("depth_scale_positive", scale_far > 0.0 && scale_near > 0.0);
    assert_true("depth_yoff_far_positive", yoff_far > 0.0);
    assert_true("depth_yoff_near_negative", yoff_near < 0.0);
    return 0;
}

static int test_runtime_scene_bridge_preflight_accepts_runtime_contract(void) {
    RuntimeSceneBridgePreflight preflight;
    bool ok = runtime_scene_bridge_preflight_file("../shared/assets/scenes/trio_contract/scene_runtime_min.json",
                                                  &preflight);
    assert_true("runtime_scene_preflight_accept_fixture", ok);
    if (ok) {
        assert_true("runtime_scene_preflight_valid_contract", preflight.valid_contract);
        assert_true("runtime_scene_preflight_scene_id",
                    strcmp(preflight.scene_id, "scene_trio_min") == 0);
        assert_true("runtime_scene_preflight_object_count", preflight.object_count == 1);
    }
    return 0;
}

static int test_runtime_scene_bridge_rejects_authoring_variant(void) {
    RuntimeSceneBridgePreflight preflight;
    bool ok = runtime_scene_bridge_preflight_file("../shared/assets/scenes/trio_contract/scene_authoring_min.json",
                                                  &preflight);
    assert_true("runtime_scene_preflight_reject_authoring", !ok);
    if (!ok) {
        assert_true("runtime_scene_preflight_diag_schema_variant",
                    strstr(preflight.diagnostics, "scene_runtime_v1") != NULL);
    }
    return 0;
}

static int test_runtime_scene_bridge_rejects_malformed_runtime_payload(void) {
    const char *runtime_json_missing_variant =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_missing_variant\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[]"
        "}";
    RuntimeSceneBridgePreflight preflight;
    bool ok = runtime_scene_bridge_preflight_json(runtime_json_missing_variant, &preflight);
    assert_true("runtime_scene_preflight_reject_missing_variant", !ok);
    if (!ok) {
        assert_true("runtime_scene_preflight_diag_missing_variant",
                    strstr(preflight.diagnostics, "missing schema_variant") != NULL);
    }
    return 0;
}

static int test_runtime_scene_bridge_optional_lanes_default_deterministic(void) {
    const char *runtime_json_missing_optional_lanes =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_optional_lanes\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[{"
          "\"object_id\":\"obj_optional_1\","
          "\"object_type\":\"circle\","
          "\"transform\":{"
            "\"position\":{\"x\":3.0,\"y\":4.0,\"z\":0.0},"
            "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}"
          "}"
        "}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight preflight;
    RuntimeSceneBridgePreflight summary;
    bool ok = runtime_scene_bridge_preflight_json(runtime_json_missing_optional_lanes, &preflight);
    assert_true("runtime_scene_preflight_optional_lanes_ok", ok);
    if (ok) {
        assert_true("runtime_scene_preflight_optional_material_count_zero", preflight.material_count == 0);
        assert_true("runtime_scene_preflight_optional_light_count_zero", preflight.light_count == 0);
        assert_true("runtime_scene_preflight_optional_camera_count_zero", preflight.camera_count == 0);
        assert_true("runtime_scene_preflight_optional_object_count_one", preflight.object_count == 1);
    }

    ok = runtime_scene_bridge_apply_json(runtime_json_missing_optional_lanes, &summary);
    assert_true("runtime_scene_apply_optional_lanes_ok", ok);
    if (ok) {
        assert_true("runtime_scene_apply_optional_object_count_one", sceneSettings.objectCount == 1);
        assert_true("runtime_scene_apply_optional_material_count_zero", summary.material_count == 0);
        assert_true("runtime_scene_apply_optional_light_count_zero", summary.light_count == 0);
        assert_true("runtime_scene_apply_optional_camera_count_zero", summary.camera_count == 0);
        assert_close("runtime_scene_apply_optional_object_x", sceneSettings.sceneObjects[0].x, 3.0, 1e-9);
        assert_close("runtime_scene_apply_optional_object_y", sceneSettings.sceneObjects[0].y, 4.0, 1e-9);
        assert_close("runtime_scene_apply_optional_camera_x_default", sceneSettings.camera.x, 0.0, 1e-9);
        assert_close("runtime_scene_apply_optional_camera_y_default", sceneSettings.camera.y, 0.0, 1e-9);
        assert_close("runtime_scene_apply_optional_light_path_x_default",
                     sceneSettings.bezierPath.points[0].x,
                     0.0,
                     1e-9);
        assert_close("runtime_scene_apply_optional_light_path_y_default",
                     sceneSettings.bezierPath.points[0].y,
                     0.0,
                     1e-9);
    }
    return 0;
}

static int test_runtime_scene_bridge_rejects_noncanonical_unit_system(void) {
    const char *runtime_json_noncanonical_units =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_units_invalid\","
        "\"unit_system\":\"centimeters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[]"
        "}";
    RuntimeSceneBridgePreflight preflight;
    bool ok = runtime_scene_bridge_preflight_json(runtime_json_noncanonical_units, &preflight);
    assert_true("runtime_scene_preflight_reject_noncanonical_units", !ok);
    if (!ok) {
        assert_true("runtime_scene_preflight_diag_noncanonical_units",
                    strstr(preflight.diagnostics, "unit_system must be meters") != NULL);
    }
    return 0;
}

static int test_runtime_scene_bridge_apply_uses_world_scale_mapping(void) {
    const char *runtime_json_scaled =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_world_scale\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":2.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[{"
          "\"object_id\":\"obj_scale_1\","
          "\"object_type\":\"circle\","
          "\"transform\":{"
            "\"position\":{\"x\":2.0,\"y\":3.0,\"z\":4.0},"
            "\"scale\":{\"x\":1.0,\"y\":2.0,\"z\":3.0}"
          "}"
        "}],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":5.0,\"y\":6.0,\"z\":7.0}}],"
        "\"cameras\":[{\"position\":{\"x\":8.0,\"y\":9.0,\"z\":10.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary;
    bool ok = runtime_scene_bridge_apply_json(runtime_json_scaled, &summary);
    assert_true("runtime_scene_apply_world_scale_ok", ok);
    if (!ok) return 0;
    assert_true("runtime_scene_apply_world_scale_object_count_one", sceneSettings.objectCount == 1);
    assert_close("runtime_scene_apply_world_scale_object_x", sceneSettings.sceneObjects[0].x, 4.0, 1e-9);
    assert_close("runtime_scene_apply_world_scale_object_y", sceneSettings.sceneObjects[0].y, 6.0, 1e-9);
    assert_close("runtime_scene_apply_world_scale_object_z", sceneSettings.sceneObjects[0].z, 8.0, 1e-9);
    assert_close("runtime_scene_apply_world_scale_object_scale", sceneSettings.sceneObjects[0].scale, 4.0, 1e-9);
    assert_close("runtime_scene_apply_world_scale_light_x", sceneSettings.bezierPath.points[0].x, 10.0, 1e-9);
    assert_close("runtime_scene_apply_world_scale_light_y", sceneSettings.bezierPath.points[0].y, 12.0, 1e-9);
    assert_close("runtime_scene_apply_world_scale_camera_x", sceneSettings.camera.x, 16.0, 1e-9);
    assert_close("runtime_scene_apply_world_scale_camera_y", sceneSettings.camera.y, 18.0, 1e-9);
    return 0;
}

static int test_runtime_scene_bridge_apply_preserves_editor_mode_state(void) {
    const char *runtime_json_no_editor_mutation =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_editor_state\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary;
    bool ok = false;
    int original_editor_mode = animSettings.editorMode;
    animSettings.editorMode = 2;
    ok = runtime_scene_bridge_apply_json(runtime_json_no_editor_mutation, &summary);
    assert_true("runtime_scene_apply_preserve_editor_mode_apply_ok", ok);
    assert_true("runtime_scene_apply_preserve_editor_mode_unchanged", animSettings.editorMode == 2);
    animSettings.editorMode = original_editor_mode;
    return 0;
}

static int test_runtime_scene_bridge_apply_3d_primitives_scaffold(void) {
    const char *runtime_json_primitives =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_3d_primitives\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"obj_box\","
            "\"object_type\":\"box\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"obj_plane\","
            "\"object_type\":\"plane\","
            "\"transform\":{\"position\":{\"x\":5.0,\"y\":0.0,\"z\":0.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"obj_mesh\","
            "\"object_type\":\"triangle_mesh\","
            "\"transform\":{\"position\":{\"x\":10.0,\"y\":0.0,\"z\":-1.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":20.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary;
    RuntimeSceneBridge3DScaffoldState scaffold;
    bool ok = runtime_scene_bridge_apply_json(runtime_json_primitives, &summary);
    assert_true("runtime_scene_apply_3d_primitives_ok", ok);
    if (!ok) return 0;

    assert_true("runtime_scene_apply_3d_primitives_object_count", sceneSettings.objectCount == 3);
    assert_true("runtime_scene_apply_3d_primitives_box_polygon",
                sceneSettings.sceneObjects[0].numPoints == 4);
    assert_true("runtime_scene_apply_3d_primitives_plane_polygon",
                sceneSettings.sceneObjects[1].numPoints == 4);
    assert_true("runtime_scene_apply_3d_primitives_mesh_triangle",
                sceneSettings.sceneObjects[2].numPoints == 3);

    runtime_scene_bridge_get_last_3d_scaffold_state(&scaffold);
    assert_true("runtime_scene_apply_3d_primitives_scaffold_valid", scaffold.valid);
    assert_true("runtime_scene_apply_3d_primitives_scaffold_has_camera", scaffold.has_camera_seed);
    assert_close("runtime_scene_apply_3d_primitives_scaffold_camera_z", scaffold.camera_z, 20.0, 1e-9);
    assert_true("runtime_scene_apply_3d_primitives_scaffold_box_count", scaffold.box_count == 1);
    assert_true("runtime_scene_apply_3d_primitives_scaffold_plane_count", scaffold.plane_count == 1);
    assert_true("runtime_scene_apply_3d_primitives_scaffold_mesh_count", scaffold.triangle_mesh_count == 1);
    return 0;
}

static int test_runtime_scene_bridge_apply_ps4d_fixture_retains_digest_truth(void) {
    RuntimeSceneBridgePreflight summary;
    RuntimeSceneBridge3DScaffoldState scaffold;
    RuntimeSceneBridge3DDigestState digest;
    bool ok = runtime_scene_bridge_apply_file("../physics_sim/config/samples/ps4d_runtime_scene_visual_test.json",
                                              &summary);
    assert_true("runtime_scene_ps4d_apply_ok", ok);
    if (!ok) return 0;

    assert_true("runtime_scene_ps4d_summary_valid", summary.valid_contract);
    assert_true("runtime_scene_ps4d_object_count", sceneSettings.objectCount == 3);
    assert_true("runtime_scene_ps4d_space_mode_3d", animSettings.spaceMode == SPACE_MODE_3D);
    assert_true("runtime_scene_ps4d_light_path_empty_without_light_seed",
                sceneSettings.bezierPath.numPoints == 0);

    runtime_scene_bridge_get_last_3d_scaffold_state(&scaffold);
    assert_true("runtime_scene_ps4d_scaffold_valid", scaffold.valid);
    assert_true("runtime_scene_ps4d_scaffold_plane_count", scaffold.plane_count == 1);
    assert_true("runtime_scene_ps4d_scaffold_box_count", scaffold.box_count == 2);

    runtime_scene_bridge_get_last_3d_digest_state(&digest);
    assert_true("runtime_scene_ps4d_digest_valid", digest.valid);
    assert_true("runtime_scene_ps4d_digest_has_bounds", digest.has_scene_bounds);
    assert_true("runtime_scene_ps4d_digest_bounds_enabled", digest.bounds_enabled);
    assert_true("runtime_scene_ps4d_digest_bounds_clamp", digest.bounds_clamp_on_edit);
    assert_close("runtime_scene_ps4d_digest_bounds_min_x", digest.bounds_min_x, -6.0, 1e-9);
    assert_close("runtime_scene_ps4d_digest_bounds_min_y", digest.bounds_min_y, -5.0, 1e-9);
    assert_close("runtime_scene_ps4d_digest_bounds_min_z", digest.bounds_min_z, -2.5, 1e-9);
    assert_close("runtime_scene_ps4d_digest_bounds_max_x", digest.bounds_max_x, 6.0, 1e-9);
    assert_close("runtime_scene_ps4d_digest_bounds_max_y", digest.bounds_max_y, 5.0, 1e-9);
    assert_close("runtime_scene_ps4d_digest_bounds_max_z", digest.bounds_max_z, 4.0, 1e-9);
    assert_true("runtime_scene_ps4d_digest_has_construction_plane", digest.has_construction_plane);
    assert_true("runtime_scene_ps4d_digest_construction_mode",
                strcmp(digest.construction_plane_mode, "axis_aligned") == 0);
    assert_true("runtime_scene_ps4d_digest_construction_axis",
                strcmp(digest.construction_plane_axis, "xy") == 0);
    assert_close("runtime_scene_ps4d_digest_construction_offset",
                 digest.construction_plane_offset,
                 -1.0,
                 1e-9);
    assert_true("runtime_scene_ps4d_digest_primitive_count", digest.primitive_count == 3);
    assert_true("runtime_scene_ps4d_digest_plane_count", digest.plane_primitive_count == 1);
    assert_true("runtime_scene_ps4d_digest_prism_count", digest.rect_prism_primitive_count == 2);
    assert_true("runtime_scene_ps4d_digest_plane_kind_count",
                digest_count_kind(&digest, RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE) == 1);
    assert_true("runtime_scene_ps4d_digest_prism_kind_count",
                digest_count_kind(&digest, RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM) == 2);

    return 0;
}

static int test_scene_compile_and_preflight_roundtrip(void) {
    const char *authoring_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_compile_rt\","
        "\"space_mode_default\":\"3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"objects\":[{\"object_id\":\"obj_a\"}],"
        "\"hierarchy\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{\"ray_tracing\":{\"seed\":1}}"
        "}";
    char diagnostics[256];
    char *runtime_json = NULL;
    RuntimeSceneBridgePreflight preflight;
    CoreResult r = core_scene_compile_authoring_to_runtime(authoring_json,
                                                           &runtime_json,
                                                           diagnostics,
                                                           sizeof(diagnostics));
    assert_true("scene_compile_roundtrip_compile_ok", r.code == CORE_OK);
    if (r.code == CORE_OK && runtime_json) {
        bool ok = runtime_scene_bridge_preflight_json(runtime_json, &preflight);
        assert_true("scene_compile_roundtrip_preflight_ok", ok);
        if (ok) {
            assert_true("scene_compile_roundtrip_scene_id",
                        strcmp(preflight.scene_id, "scene_compile_rt") == 0);
            assert_true("scene_compile_roundtrip_objects", preflight.object_count == 1);
        }
    }
    free(runtime_json);
    return 0;
}

static int test_runtime_scene_bridge_apply_runtime_fixture(void) {
    RuntimeSceneBridgePreflight summary;
    RuntimeSceneBridgePreflight alias_summary;
    bool ok = runtime_scene_bridge_apply_file("../shared/assets/scenes/trio_contract/scene_runtime_min.json",
                                              &summary);
    assert_true("runtime_scene_apply_fixture_ok", ok);
    if (!ok) return 0;

    assert_true("runtime_scene_apply_valid_contract", summary.valid_contract);
    assert_true("runtime_scene_apply_object_count", sceneSettings.objectCount == 1);
    assert_close("runtime_scene_apply_object_x", sceneSettings.sceneObjects[0].x, 0.0, 1e-9);
    assert_close("runtime_scene_apply_object_y", sceneSettings.sceneObjects[0].y, 0.0, 1e-9);
    assert_close("runtime_scene_apply_object_z", sceneSettings.sceneObjects[0].z, 0.0, 1e-9);
    assert_true("runtime_scene_apply_color_from_material",
                sceneSettings.sceneObjects[0].color == 0xFFFFFF);
    assert_true("runtime_scene_apply_light_point_count_seeded",
                sceneSettings.bezierPath.numPoints == 1);
    assert_close("runtime_scene_apply_light_x", sceneSettings.bezierPath.points[0].x, 5.0, 1e-9);
    assert_close("runtime_scene_apply_light_y", sceneSettings.bezierPath.points[0].y, 8.0, 1e-9);
    assert_close("runtime_scene_apply_camera_x", sceneSettings.camera.x, 0.0, 1e-9);
    assert_close("runtime_scene_apply_camera_y", sceneSettings.camera.y, 0.0, 1e-9);
    assert_true("runtime_scene_apply_space_mode_2d", animSettings.spaceMode == SPACE_MODE_2D);
    assert_true("runtime_scene_apply_source_runtime_scene",
                animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE);
    assert_true("runtime_scene_apply_source_runtime_scene_path",
                strcmp(animSettings.runtimeScenePath,
                       "../shared/assets/scenes/trio_contract/scene_runtime_min.json") == 0);

    // Regression: menu apply path passes animSettings.runtimeScenePath as input.
    ok = runtime_scene_bridge_apply_file(animSettings.runtimeScenePath, &alias_summary);
    assert_true("runtime_scene_apply_alias_buffer_ok", ok);
    if (ok) {
        assert_true("runtime_scene_apply_alias_buffer_valid_contract", alias_summary.valid_contract);
        assert_true("runtime_scene_apply_alias_buffer_path_retained",
                    strcmp(animSettings.runtimeScenePath,
                           "../shared/assets/scenes/trio_contract/scene_runtime_min.json") == 0);
    }
    return 0;
}

static int test_path_eval_3d_uses_linear_handle_units(void) {
    Path path = {0};
    int original_space_mode = animSettings.spaceMode;
    Point p = {0};

    path.mode = BEZIER_CUBIC;
    path.numPoints = 2;
    path.points[0] = (Point){0.0, 0.0};
    path.points[1] = (Point){10.0, 0.0};
    path.handles[0][0] = (Velocity){0.0, 4.0};
    path.handles[0][1] = (Velocity){0.0, 4.0};

    animSettings.spaceMode = SPACE_MODE_2D;
    p = GetPositionAlongPath(&path, 0.5);
    assert_true("path_eval_2d_handle_compression_active", p.y < 0.5);

    animSettings.spaceMode = SPACE_MODE_3D;
    p = GetPositionAlongPath(&path, 0.5);
    assert_close("path_eval_3d_linear_handle_y", p.y, 3.0, 1e-9);

    animSettings.spaceMode = original_space_mode;
    return 0;
}

static int test_runtime_scene_bridge_apply_compile_output(void) {
    const char *authoring_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_apply_compile\","
        "\"space_mode_default\":\"3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"objects\":[{"
          "\"object_id\":\"obj_apply\","
          "\"object_type\":\"circle\","
          "\"transform\":{"
             "\"position\":{\"x\":4.0,\"y\":6.0,\"z\":2.5},"
             "\"rotation\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
             "\"scale\":{\"x\":1.2,\"y\":1.2,\"z\":1.2}"
          "},"
          "\"material_ref\":{\"id\":\"mat_apply\"}"
        "}],"
        "\"hierarchy\":[],"
        "\"materials\":[{\"material_id\":\"mat_apply\",\"albedo\":[0.25,0.5,1.0]}],"
        "\"lights\":[{\"position\":{\"x\":2.0,\"y\":3.0,\"z\":1.0}}],"
        "\"cameras\":[{\"position\":{\"x\":7.0,\"y\":8.0,\"z\":9.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary;
    char diagnostics[256];
    char *runtime_json = NULL;
    CoreResult r = core_scene_compile_authoring_to_runtime(authoring_json,
                                                           &runtime_json,
                                                           diagnostics,
                                                           sizeof(diagnostics));
    assert_true("runtime_scene_apply_compile_compile_ok", r.code == CORE_OK);
    if (r.code != CORE_OK || !runtime_json) {
        free(runtime_json);
        return 0;
    }

    {
        bool ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
        assert_true("runtime_scene_apply_compile_apply_ok", ok);
        if (ok) {
            assert_true("runtime_scene_apply_compile_count", sceneSettings.objectCount == 1);
            assert_close("runtime_scene_apply_compile_x", sceneSettings.sceneObjects[0].x, 4.0, 1e-9);
            assert_close("runtime_scene_apply_compile_y", sceneSettings.sceneObjects[0].y, 6.0, 1e-9);
            assert_close("runtime_scene_apply_compile_z", sceneSettings.sceneObjects[0].z, 2.5, 1e-9);
            assert_true("runtime_scene_apply_compile_space3d", animSettings.spaceMode == SPACE_MODE_3D);
            assert_true("runtime_scene_apply_compile_source_runtime_scene",
                        animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE);
            assert_true("runtime_scene_apply_compile_runtime_path_empty",
                        animSettings.runtimeScenePath[0] == '\0');
        }
    }

    free(runtime_json);
    return 0;
}

static int test_runtime_scene_bridge_writeback_overlay_preserves_non_ray_state(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_1\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[{\"object_id\":\"obj_base\"}],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"physics_sim\":{\"gravity\":9.81},"
          "\"custom_tool\":{\"foo\":1}"
        "},"
        "\"compile_meta\":{\"compiler\":\"core_scene_compile\"}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"ray_tracing\",\"logical_clock\":10},"
        "\"space_mode_default\":\"3d\","
        "\"extensions\":{"
          "\"ray_tracing\":{"
            "\"exposure\":1.25,"
            "\"integrator\":\"hybrid\""
          "}"
        "}"
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_overlay_ok", ok);
    if (!ok || !merged) {
        free(merged);
        return 0;
    }

    assert_true("runtime_scene_writeback_preserve_physics",
                strstr(merged, "\"physics_sim\"") != NULL);
    assert_true("runtime_scene_writeback_preserve_custom",
                strstr(merged, "\"custom_tool\"") != NULL);
    assert_true("runtime_scene_writeback_add_ray",
                strstr(merged, "\"ray_tracing\"") != NULL);
    assert_true("runtime_scene_writeback_update_space_mode",
                strstr(merged, "\"space_mode_default\":\"3d\"") != NULL);
    assert_true("runtime_scene_writeback_preserve_compile_meta",
                strstr(merged, "\"compile_meta\"") != NULL);
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_writeback_rejects_foreign_extension_namespace(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_2\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"ray_tracing\",\"logical_clock\":11},"
        "\"extensions\":{"
          "\"physics_sim\":{\"gravity\":9.81}"
        "}"
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_reject_foreign_namespace", !ok);
    if (!ok) {
        assert_true("runtime_scene_writeback_reject_foreign_diag",
                    strstr(diagnostics, "namespace not allowed") != NULL);
    }
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_writeback_rejects_forbidden_top_level_overlay_key(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_3\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"ray_tracing\",\"logical_clock\":12},"
        "\"objects\":[{\"object_id\":\"hack\"}]"
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_reject_forbidden_top_key", !ok);
    if (!ok) {
        assert_true("runtime_scene_writeback_reject_forbidden_top_key_diag",
                    strstr(diagnostics, "overlay key not allowed") != NULL);
    }
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_writeback_rejects_invalid_space_mode_value(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_4\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"ray_tracing\",\"logical_clock\":13},"
        "\"space_mode_default\":\"4d\""
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_reject_invalid_space_mode", !ok);
    if (!ok) {
        assert_true("runtime_scene_writeback_reject_invalid_space_mode_diag",
                    strstr(diagnostics, "space_mode_default must be '2d' or '3d'") != NULL);
    }
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_writeback_rejects_non_object_ray_extension_payload(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_5\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"ray_tracing\",\"logical_clock\":14},"
        "\"extensions\":{"
          "\"ray_tracing\":[1,2,3]"
        "}"
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_reject_non_object_ray_payload", !ok);
    if (!ok) {
        assert_true("runtime_scene_writeback_reject_non_object_ray_payload_diag",
                    strstr(diagnostics, "payload must be object") != NULL);
    }
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_writeback_rejects_missing_overlay_meta(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_6\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"extensions\":{\"ray_tracing\":{\"samples\":8}}"
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_reject_missing_meta", !ok);
    if (!ok) {
        assert_true("runtime_scene_writeback_reject_missing_meta_diag",
                    strstr(diagnostics, "overlay_meta object is required") != NULL);
    }
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_writeback_rejects_stale_logical_clock(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_7\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"overlay_merge\":{"
            "\"producer_clocks\":{\"ray_tracing\":20}"
          "}"
        "}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"ray_tracing\",\"logical_clock\":20},"
        "\"extensions\":{\"ray_tracing\":{\"samples\":16}}"
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_reject_stale_clock", !ok);
    if (!ok) {
        assert_true("runtime_scene_writeback_reject_stale_clock_diag",
                    strstr(diagnostics, "logical_clock is stale") != NULL);
    }
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_writeback_rejects_wrong_overlay_producer(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_8\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"physics_sim\",\"logical_clock\":1},"
        "\"extensions\":{\"ray_tracing\":{\"samples\":8}}"
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_reject_wrong_producer", !ok);
    if (!ok) {
        assert_true("runtime_scene_writeback_reject_wrong_producer_diag",
                    strstr(diagnostics, "producer not allowed") != NULL);
    }
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_writeback_rejects_negative_logical_clock(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_9\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"ray_tracing\",\"logical_clock\":-1},"
        "\"extensions\":{\"ray_tracing\":{\"samples\":8}}"
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_reject_negative_clock", !ok);
    if (!ok) {
        assert_true("runtime_scene_writeback_reject_negative_clock_diag",
                    strstr(diagnostics, "logical_clock must be >= 0") != NULL);
    }
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_writeback_rejects_runtime_core_unit_system_overlay(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_10\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"ray_tracing\",\"logical_clock\":15},"
        "\"unit_system\":\"centimeters\""
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_reject_unit_system_top_key", !ok);
    if (!ok) {
        assert_true("runtime_scene_writeback_reject_unit_system_top_key_diag",
                    strstr(diagnostics, "overlay key not allowed") != NULL);
    }
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_writeback_rejects_runtime_core_world_scale_overlay(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_11\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"ray_tracing\",\"logical_clock\":16},"
        "\"world_scale\":2.0"
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_reject_world_scale_top_key", !ok);
    if (!ok) {
        assert_true("runtime_scene_writeback_reject_world_scale_top_key_diag",
                    strstr(diagnostics, "overlay key not allowed") != NULL);
    }
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_writeback_space_mode_tiebreak_rejects_lexically_larger_producer(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_12\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"overlay_merge\":{"
            "\"space_mode_default\":{\"producer\":\"line_drawing\",\"logical_clock\":50}"
          "}"
        "}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"ray_tracing\",\"logical_clock\":50},"
        "\"space_mode_default\":\"3d\","
        "\"extensions\":{\"ray_tracing\":{\"samples\":8}}"
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_tiebreak_reject_larger_producer", !ok);
    if (!ok) {
        assert_true("runtime_scene_writeback_tiebreak_reject_larger_producer_diag",
                    strstr(diagnostics, "tie-break lost") != NULL);
    }
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_writeback_space_mode_tiebreak_accepts_lexically_smaller_producer(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_13\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"overlay_merge\":{"
            "\"space_mode_default\":{\"producer\":\"ray_tracing\",\"logical_clock\":50}"
          "}"
        "}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"ray_tracing\",\"logical_clock\":51},"
        "\"space_mode_default\":\"3d\","
        "\"extensions\":{\"ray_tracing\":{\"samples\":32}}"
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_tiebreak_accept_same_producer_newer_clock", ok);
    if (ok && merged) {
        assert_true("runtime_scene_writeback_tiebreak_accept_space_mode_updated",
                    strstr(merged, "\"space_mode_default\":\"3d\"") != NULL);
    }
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_trio_fixture_compile_writeback_apply(void) {
    size_t authoring_size = 0;
    size_t overlay_size = 0;
    char *authoring_json = read_text_file_alloc("../shared/assets/scenes/trio_contract/scene_authoring_interop_min.json",
                                                &authoring_size);
    char *overlay_json = read_text_file_alloc("../shared/assets/scenes/trio_contract/ray_overlay_min.json",
                                              &overlay_size);
    char diagnostics[256];
    char *runtime_json = NULL;
    char *merged_json = NULL;
    RuntimeSceneBridgePreflight preflight;
    CoreResult r;

    assert_true("trio_fixture_authoring_read_ok", authoring_json != NULL && authoring_size > 0);
    assert_true("trio_fixture_ray_overlay_read_ok", overlay_json != NULL && overlay_size > 0);
    if (!authoring_json || !overlay_json) {
        free(authoring_json);
        free(overlay_json);
        return 0;
    }

    r = core_scene_compile_authoring_to_runtime(authoring_json,
                                                &runtime_json,
                                                diagnostics,
                                                sizeof(diagnostics));
    assert_true("trio_fixture_compile_ok", r.code == CORE_OK);
    if (r.code != CORE_OK || !runtime_json) {
        free(authoring_json);
        free(overlay_json);
        free(runtime_json);
        return 0;
    }

    {
        bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                                  overlay_json,
                                                                  &merged_json,
                                                                  diagnostics,
                                                                  sizeof(diagnostics));
        assert_true("trio_fixture_ray_writeback_ok", ok);
    }

    if (!merged_json) {
        free(authoring_json);
        free(overlay_json);
        free(runtime_json);
        free(merged_json);
        return 0;
    }

    assert_true("trio_fixture_preserve_line_drawing_ext",
                strstr(merged_json, "\"line_drawing\"") != NULL);
    assert_true("trio_fixture_preserve_physics_ext",
                strstr(merged_json, "\"physics_sim\"") != NULL);
    assert_true("trio_fixture_has_ray_ext",
                strstr(merged_json, "\"ray_tracing\"") != NULL);

    {
        bool ok = runtime_scene_bridge_preflight_json(merged_json, &preflight);
        assert_true("trio_fixture_preflight_after_writeback_ok", ok);
    }
    {
        bool ok = runtime_scene_bridge_apply_json(merged_json, &preflight);
        assert_true("trio_fixture_apply_after_writeback_ok", ok);
    }

    free(authoring_json);
    free(overlay_json);
    free(runtime_json);
    free(merged_json);
    return 0;
}

static int test_runtime_scene_bridge_apply_hydrates_ray_authoring_paths(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_authoring_hydrate_1\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":2.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":1.0,\"y\":1.0,\"z\":2.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"ray_tracing\":{"
            "\"authoring\":{"
              "\"light_path\":{"
                "\"mode\":\"BEZIER_CUBIC\","
                "\"points\":["
                  "{"
                    "\"x\":1.25,"
                    "\"y\":-0.75,"
                    "\"rotation\":0.0,"
                    "\"handleLink\":false,"
                    "\"velocity1\":{\"vx\":0.5,\"vy\":0.25}"
                  "},"
                  "{"
                    "\"x\":2.50,"
                    "\"y\":1.00,"
                    "\"rotation\":0.0,"
                    "\"handleLink\":false,"
                    "\"velocity2\":{\"vx\":-0.5,\"vy\":-0.25}"
                  "}"
                "]"
              "},"
              "\"camera_path\":{"
                "\"mode\":\"BEZIER_CUBIC\","
                "\"points\":["
                  "{"
                    "\"x\":2.0,"
                    "\"y\":3.0,"
                    "\"rotation\":0.5,"
                    "\"handleLink\":false"
                  "}"
                "]"
              "}"
            "}"
          "}"
        "}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    bool ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_scene_authoring_hydrate_apply_ok", ok);
    if (!ok) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    assert_true("runtime_scene_authoring_hydrate_bezier_points",
                sceneSettings.bezierPath.numPoints == 2);
    assert_close("runtime_scene_authoring_hydrate_light_p0_x",
                 sceneSettings.bezierPath.points[0].x,
                 2.5,
                 1e-6);
    assert_close("runtime_scene_authoring_hydrate_light_p0_y",
                 sceneSettings.bezierPath.points[0].y,
                 -1.5,
                 1e-6);
    assert_close("runtime_scene_authoring_hydrate_light_handle_vx",
                 sceneSettings.bezierPath.handles[0][0].vx,
                 1.0,
                 1e-6);
    assert_close("runtime_scene_authoring_hydrate_light_handle_vy",
                 sceneSettings.bezierPath.handles[0][0].vy,
                 0.5,
                 1e-6);
    assert_true("runtime_scene_authoring_hydrate_camera_points",
                sceneSettings.cameraPath.numPoints == 1);
    assert_close("runtime_scene_authoring_hydrate_camera_p0_x",
                 sceneSettings.cameraPath.points[0].x,
                 4.0,
                 1e-6);
    assert_close("runtime_scene_authoring_hydrate_camera_p0_y",
                 sceneSettings.cameraPath.points[0].y,
                 6.0,
                 1e-6);
    assert_close("runtime_scene_authoring_hydrate_camera_x",
                 sceneSettings.camera.x,
                 4.0,
                 1e-6);
    assert_close("runtime_scene_authoring_hydrate_camera_y",
                 sceneSettings.camera.y,
                 6.0,
                 1e-6);
    assert_close("runtime_scene_authoring_hydrate_camera_rot",
                 sceneSettings.camera.rotation,
                 0.5,
                 1e-6);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_scene_editor_runtime_scene_persistence_roundtrip(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_path = "/tmp/ray_tracing_runtime_scene_authoring_roundtrip.json";
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_authoring_persist_1\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":2.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":2.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    char diagnostics[256];
    char *persisted_json = NULL;
    FILE *file = fopen(runtime_path, "wb");
    bool ok = false;

    assert_true("runtime_scene_authoring_persist_open_tmp", file != NULL);
    if (!file) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    fwrite(runtime_json, 1, strlen(runtime_json), file);
    fclose(file);

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath), "%s", runtime_path);
    animSettings.spaceMode = SPACE_MODE_3D;

    sceneSettings.bezierPath.mode = BEZIER_CUBIC;
    sceneSettings.bezierPath.numPoints = 2;
    sceneSettings.bezierPath.points[0].x = 6.5;
    sceneSettings.bezierPath.points[0].y = -1.5;
    sceneSettings.bezierPath.points[1].x = 8.0;
    sceneSettings.bezierPath.points[1].y = 3.0;
    sceneSettings.bezierPath.handles[0][0].vx = 1.25;
    sceneSettings.bezierPath.handles[0][0].vy = 0.75;
    sceneSettings.bezierPath.handles[0][1].vx = -0.5;
    sceneSettings.bezierPath.handles[0][1].vy = -1.0;

    sceneSettings.cameraPath.mode = BEZIER_CUBIC;
    sceneSettings.cameraPath.numPoints = 1;
    sceneSettings.cameraPath.points[0].x = 10.0;
    sceneSettings.cameraPath.points[0].y = 4.0;
    sceneSettings.cameraPath.rotations[0] = 0.25;
    sceneSettings.cameraPath.rotationSet[0] = true;
    sceneSettings.camera.x = 10.0;
    sceneSettings.camera.y = 4.0;
    sceneSettings.camera.rotation = 0.25;

    ok = SceneEditorRuntimeScenePersistAuthoring(diagnostics, sizeof(diagnostics));
    assert_true("runtime_scene_authoring_persist_ok", ok);
    if (!ok) {
        unlink(runtime_path);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    persisted_json = read_text_file_alloc(runtime_path, NULL);
    assert_true("runtime_scene_authoring_persist_readback_ok", persisted_json != NULL);
    if (persisted_json) {
        assert_true("runtime_scene_authoring_persist_has_authoring",
                    strstr(persisted_json, "\"authoring\"") != NULL);
        assert_true("runtime_scene_authoring_persist_has_light_path",
                    strstr(persisted_json, "\"light_path\"") != NULL);
        assert_true("runtime_scene_authoring_persist_has_camera_path",
                    strstr(persisted_json, "\"camera_path\"") != NULL);
    }

    assert_true("runtime_scene_authoring_persist_hydrated_light_points",
                sceneSettings.bezierPath.numPoints == 2);
    assert_close("runtime_scene_authoring_persist_light_p0_x",
                 sceneSettings.bezierPath.points[0].x,
                 6.5,
                 1e-6);
    assert_close("runtime_scene_authoring_persist_light_handle_vx",
                 sceneSettings.bezierPath.handles[0][0].vx,
                 1.25,
                 1e-6);
    assert_true("runtime_scene_authoring_persist_hydrated_camera_points",
                sceneSettings.cameraPath.numPoints == 1);
    assert_close("runtime_scene_authoring_persist_camera_p0_x",
                 sceneSettings.cameraPath.points[0].x,
                 10.0,
                 1e-6);
    assert_close("runtime_scene_authoring_persist_camera_rot",
                 sceneSettings.camera.rotation,
                 0.25,
                 1e-6);

    free(persisted_json);
    unlink(runtime_path);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_mode_backend_route_2d_defaults(void) {
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_2D;
    animSettings.integratorMode = 1;
    animSettings.useTiledRenderer = true;
    animSettings.tileSize = 12;
    animSettings.tilePreviewEnabled = true;

    RayTracingRuntimeRoute route = RayTracingModeBackend_ResolveRoute();

    assert_true("route2d_family_canonical",
                route.routeFamily == RAY_TRACING_ROUTE_CANONICAL_2D);
    assert_true("route2d_is_canonical_helper",
                RayTracingModeBackend_IsCanonical2D(&route));
    assert_true("route2d_not_compat_helper",
                !RayTracingModeBackend_IsCompat3DFallback(&route));
    assert_true("route2d_not_native_helper",
                !RayTracingModeBackend_IsNative3D(&route));
    assert_true("route2d_lane_canonical", route.backendLane == RAY_TRACING_BACKEND_CANONICAL_2D);
    assert_true("route2d_no_fallback", !route.fallbackTo2DProjection);
    assert_true("route2d_projection_2d", route.projectionMode == SPACE_MODE_2D);
    assert_true("route2d_no_runtime_3d_scaffold", !route.usesRuntime3DScaffold);
    assert_close("route2d_runtime_camera_z_zero", route.runtimeCameraZ, 0.0, 1e-9);
    assert_close("route2d_ray_origin_y_offset_zero", route.rayOriginYOffset, 0.0, 1e-9);
    assert_true("route2d_tiles_enabled", route.useTiles);
    assert_true("route2d_tile_preview_enabled", route.tilePreviewEnabled);
    assert_true("route2d_cache_enabled", route.buildIrradianceCache);
    return 0;
}

static int test_mode_backend_route_3d_controlled_lane(void) {
    const char *runtime_json_route_3d =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_route_3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"obj_box\","
            "\"object_type\":\"box\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"obj_plane\","
            "\"object_type\":\"plane\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"obj_mesh\","
            "\"object_type\":\"triangle_mesh\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":20.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary;
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = 2;
    animSettings.useTiledRenderer = true;
    animSettings.tileSize = 16;
    animSettings.tilePreviewEnabled = true;
    assert_true("route3d_seed_runtime_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json_route_3d, &summary));

    RayTracingRuntimeRoute route = RayTracingModeBackend_ResolveRoute();

    assert_true("route3d_family_compat_fallback",
                route.routeFamily == RAY_TRACING_ROUTE_COMPAT_3D_FALLBACK);
    assert_true("route3d_not_canonical_helper",
                !RayTracingModeBackend_IsCanonical2D(&route));
    assert_true("route3d_compat_helper",
                RayTracingModeBackend_IsCompat3DFallback(&route));
    assert_true("route3d_not_native_helper",
                !RayTracingModeBackend_IsNative3D(&route));
    assert_true("route3d_lane_controlled", route.backendLane == RAY_TRACING_BACKEND_CONTROLLED_3D);
    assert_true("route3d_fallback_projection", route.fallbackTo2DProjection);
    assert_true("route3d_projection_mode_2d", route.projectionMode == SPACE_MODE_2D);
    assert_true("route3d_runtime_scaffold_enabled", route.usesRuntime3DScaffold);
    assert_close("route3d_runtime_camera_z", route.runtimeCameraZ, 20.0, 1e-9);
    assert_true("route3d_ray_origin_y_offset_nonzero", fabs(route.rayOriginYOffset) > 0.0);
    assert_true("route3d_scaffold_primitive_count", route.scaffoldPrimitiveCount == 3);
    assert_true("route3d_integrator_preserved", route.integratorMode == 2);
    assert_true("route3d_tile_preview_off", !route.tilePreviewEnabled);
    return 0;
}

static int test_mode_backend_scene_digest_status_2d_canonical_empty(void) {
    RayTracingRuntimeRoute route;
    RayTracingSceneDigestStatus status;
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_2D;

    route = RayTracingModeBackend_ResolveRoute();
    status = RayTracingModeBackend_BuildSceneDigestStatus(&route);

    assert_true("digest2d_route_canonical",
                route.routeFamily == RAY_TRACING_ROUTE_CANONICAL_2D);
    assert_true("digest2d_status_invalid", !status.valid);
    assert_true("digest2d_primitive_count_zero", status.digestPrimitiveCount == 0);
    assert_true("digest2d_scaffold_count_zero", status.scaffoldPrimitiveCount == 0);
    return 0;
}

static int test_mode_backend_scene_digest_status_ps4d_fixture(void) {
    RuntimeSceneBridgePreflight summary;
    RayTracingRuntimeRoute route;
    RayTracingSceneDigestStatus status;

    memset(&animSettings, 0, sizeof(animSettings));
    assert_true("digestps4d_apply_file_ok",
                runtime_scene_bridge_apply_file("../physics_sim/config/samples/ps4d_runtime_scene_visual_test.json",
                                                &summary));

    route = RayTracingModeBackend_ResolveRoute();
    status = RayTracingModeBackend_BuildSceneDigestStatus(&route);

    assert_true("digestps4d_route_compat",
                route.routeFamily == RAY_TRACING_ROUTE_COMPAT_3D_FALLBACK);
    assert_true("digestps4d_status_valid", status.valid);
    assert_true("digestps4d_bounds_present", status.hasSceneBounds);
    assert_true("digestps4d_bounds_enabled", status.boundsEnabled);
    assert_true("digestps4d_bounds_clamp", status.boundsClampOnEdit);
    assert_close("digestps4d_bounds_min_x", status.boundsMinX, -6.0, 1e-9);
    assert_close("digestps4d_bounds_min_y", status.boundsMinY, -5.0, 1e-9);
    assert_close("digestps4d_bounds_min_z", status.boundsMinZ, -2.5, 1e-9);
    assert_close("digestps4d_bounds_max_x", status.boundsMaxX, 6.0, 1e-9);
    assert_close("digestps4d_bounds_max_y", status.boundsMaxY, 5.0, 1e-9);
    assert_close("digestps4d_bounds_max_z", status.boundsMaxZ, 4.0, 1e-9);
    assert_true("digestps4d_plane_present", status.hasConstructionPlane);
    assert_true("digestps4d_plane_mode_axis_aligned",
                strcmp(status.constructionPlaneMode, "axis_aligned") == 0);
    assert_true("digestps4d_plane_axis_xy",
                strcmp(status.constructionPlaneAxis, "xy") == 0);
    assert_close("digestps4d_plane_offset",
                 status.constructionPlaneOffset,
                 -1.0,
                 1e-9);
    assert_true("digestps4d_primitive_count_three", status.digestPrimitiveCount == 3);
    assert_true("digestps4d_plane_count_one", status.planePrimitiveCount == 1);
    assert_true("digestps4d_prism_count_two", status.rectPrismPrimitiveCount == 2);
    assert_true("digestps4d_scaffold_count_three", status.scaffoldPrimitiveCount == 3);
    return 0;
}

static int test_mode_backend_view_carrier_2d_defaults(void) {
    Camera camera = {0};
    RayTracingRuntimeRoute route;
    RayTracingViewCarrier carrier;

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_2D;
    camera.x = 10.0;
    camera.y = -6.0;
    camera.zoom = 1.5;

    route = RayTracingModeBackend_ResolveRoute();
    carrier = RayTracingModeBackend_BuildViewCarrier(&camera, 320, 200, &route);

    assert_true("carrier2d_family_canonical",
                carrier.family == RAY_TRACING_VIEW_CARRIER_CANONICAL_2D);
    assert_true("carrier2d_projection_mode_2d",
                carrier.viewContext.mode == SPACE_MODE_2D);
    assert_close("carrier2d_camera_x", carrier.cameraXY.x, 10.0, 1e-9);
    assert_close("carrier2d_camera_y", carrier.cameraXY.y, -6.0, 1e-9);
    assert_close("carrier2d_camera_z", carrier.cameraZ, 0.0, 1e-9);
    assert_close("carrier2d_origin_x", carrier.originX, 10.0, 1e-9);
    assert_close("carrier2d_origin_y", carrier.originY, -6.0, 1e-9);
    assert_close("carrier2d_origin_z", carrier.originZ, 0.0, 1e-9);
    assert_true("carrier2d_not_compat_fallback", !carrier.usesCompatProjectionFallback);
    return 0;
}

static int test_mode_backend_view_carrier_3d_compat_fallback(void) {
    const char *runtime_json_route_3d =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_route_3d_carrier\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[{\"position\":{\"x\":2.0,\"y\":3.0,\"z\":12.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary;
    Camera camera = {0};
    RayTracingRuntimeRoute route;
    RayTracingViewCarrier carrier;

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    assert_true("carrier3d_seed_runtime_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json_route_3d, &summary));

    camera.x = 2.0;
    camera.y = 3.0;
    camera.zoom = 1.0;

    route = RayTracingModeBackend_ResolveRoute();
    carrier = RayTracingModeBackend_BuildViewCarrier(&camera, 640, 480, &route);

    assert_true("carrier3d_route_is_compat_fallback",
                route.routeFamily == RAY_TRACING_ROUTE_COMPAT_3D_FALLBACK);
    assert_true("carrier3d_family_compat",
                carrier.family == RAY_TRACING_VIEW_CARRIER_COMPAT_3D);
    assert_true("carrier3d_projection_matches_route",
                carrier.viewContext.mode == route.projectionMode);
    assert_true("carrier3d_compat_fallback", carrier.usesCompatProjectionFallback);
    assert_true("carrier3d_has_scaffold_camera", carrier.hasRuntimeScaffoldCamera);
    assert_close("carrier3d_camera_x", carrier.cameraXY.x, 2.0, 1e-9);
    assert_close("carrier3d_camera_y", carrier.cameraXY.y, 3.0, 1e-9);
    assert_close("carrier3d_camera_z", carrier.cameraZ, 12.0, 1e-9);
    assert_close("carrier3d_origin_x", carrier.originX, 2.0, 1e-9);
    assert_true("carrier3d_origin_y_offset_nonzero", fabs(carrier.originY - 3.0) > 0.0);
    assert_close("carrier3d_origin_z", carrier.originZ, 12.0, 1e-9);
    return 0;
}

static int test_mode_backend_primitive_prep_plan_2d_defaults(void) {
    RayTracingRuntimeRoute route;
    RayTracingPrimitivePrepPlan plan;

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_2D;

    route = RayTracingModeBackend_ResolveRoute();
    plan = RayTracingModeBackend_BuildPrimitivePrepPlan(&route, 4);

    assert_true("prep2d_family_canonical",
                plan.family == RAY_TRACING_PRIMITIVE_PREP_CANONICAL_2D);
    assert_true("prep2d_uses_scene_objects", plan.usesLegacySceneObjects);
    assert_true("prep2d_not_compat_placeholders", !plan.usesCompatPlaceholderObjects);
    assert_true("prep2d_no_runtime_scaffold", !plan.hasRuntimeScaffoldPrimitives);
    assert_true("prep2d_scaffold_count_zero", plan.scaffoldPrimitiveCount == 0);
    assert_true("prep2d_mesh_enabled", plan.enableSurfaceMeshPrep);
    assert_true("prep2d_triangles_enabled", plan.enableTriangleMeshPrep);
    assert_true("prep2d_uniform_grid_enabled", plan.enableUniformGrid2D);
    assert_true("prep2d_ray2d_enabled", plan.enableRay2DIntersections);
    return 0;
}

static int test_mode_backend_primitive_prep_plan_3d_compat_placeholder(void) {
    const char *runtime_json_route_3d =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_route_3d_prep\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"obj_box\","
            "\"object_type\":\"box\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"obj_plane\","
            "\"object_type\":\"plane\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"obj_mesh\","
            "\"object_type\":\"triangle_mesh\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":20.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary;
    RayTracingRuntimeRoute route;
    RayTracingPrimitivePrepPlan plan;

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    assert_true("prep3d_seed_runtime_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json_route_3d, &summary));

    route = RayTracingModeBackend_ResolveRoute();
    plan = RayTracingModeBackend_BuildPrimitivePrepPlan(&route, sceneSettings.objectCount);

    assert_true("prep3d_family_compat_placeholder",
                plan.family == RAY_TRACING_PRIMITIVE_PREP_COMPAT_3D_PLACEHOLDER);
    assert_true("prep3d_uses_scene_objects", plan.usesLegacySceneObjects);
    assert_true("prep3d_uses_compat_placeholders", plan.usesCompatPlaceholderObjects);
    assert_true("prep3d_has_runtime_scaffold_primitives", plan.hasRuntimeScaffoldPrimitives);
    assert_true("prep3d_scaffold_count_three", plan.scaffoldPrimitiveCount == 3);
    assert_true("prep3d_mesh_enabled", plan.enableSurfaceMeshPrep);
    assert_true("prep3d_triangles_enabled", plan.enableTriangleMeshPrep);
    assert_true("prep3d_uniform_grid_enabled", plan.enableUniformGrid2D);
    assert_true("prep3d_ray2d_enabled", plan.enableRay2DIntersections);
    return 0;
}

static int test_mode_backend_primitive_prep_plan_native3d_placeholder_contract(void) {
    RayTracingRuntimeRoute route;
    RayTracingPrimitivePrepPlan plan;

    memset(&route, 0, sizeof(route));
    route.routeFamily = RAY_TRACING_ROUTE_NATIVE_3D;
    route.usesRuntime3DScaffold = true;
    route.scaffoldPrimitiveCount = 5;
    plan = RayTracingModeBackend_BuildPrimitivePrepPlan(&route, 7);

    assert_true("prepnative_family_native",
                plan.family == RAY_TRACING_PRIMITIVE_PREP_NATIVE_3D);
    assert_true("prepnative_not_legacy_scene_objects", !plan.usesLegacySceneObjects);
    assert_true("prepnative_not_compat_placeholders", !plan.usesCompatPlaceholderObjects);
    assert_true("prepnative_has_runtime_scaffold", plan.hasRuntimeScaffoldPrimitives);
    assert_true("prepnative_scaffold_count", plan.scaffoldPrimitiveCount == 5);
    assert_true("prepnative_mesh_disabled", !plan.enableSurfaceMeshPrep);
    assert_true("prepnative_triangles_disabled", !plan.enableTriangleMeshPrep);
    assert_true("prepnative_uniform_grid_disabled", !plan.enableUniformGrid2D);
    assert_true("prepnative_ray2d_disabled", !plan.enableRay2DIntersections);
    return 0;
}

static int test_editor_mode_router_capabilities_2d(void) {
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_2D;
    animSettings.integratorMode = 0;

    EditorModeCapabilities caps = EditorModeRouter_GetCapabilities();
    assert_true("router2d_not_controlled", !caps.isControlled3D);
    assert_true("router2d_no_projection_fallback", !caps.uses2DProjectionFallback);
    assert_true("router2d_can_edit_xy", caps.canEditXY);
    assert_true("router2d_no_edit_z", !caps.canEditZ);
    assert_true("router2d_no_3d_gizmos", !caps.canUse3DGizmos);
    assert_true("router2d_label_has_2d",
                strstr(EditorModeRouter_SpaceButtonLabel(), "2D") != NULL);
    return 0;
}

static int test_editor_mode_router_capabilities_3d_scaffold(void) {
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = 1;

    EditorModeCapabilities caps = EditorModeRouter_GetCapabilities();
    assert_true("router3d_controlled", caps.isControlled3D);
    assert_true("router3d_projection_fallback", caps.uses2DProjectionFallback);
    assert_true("router3d_can_edit_xy", caps.canEditXY);
    assert_true("router3d_no_edit_z", !caps.canEditZ);
    assert_true("router3d_no_free_camera3d", !caps.canUseFreeCamera3D);
    assert_true("router3d_label_compat_fallback",
                strstr(EditorModeRouter_SpaceButtonLabel(), "Compat Fallback") != NULL);
    assert_true("router3d_hint_compat_fallback",
                strstr(EditorModeRouter_RuntimeHintLabel(), "compat fallback") != NULL);
    return 0;
}

static int test_editor_mode_router_cycle_policy(void) {
    assert_true("router_clamp_normal", EditorModeRouter_ClampEditorMode(1, false) == 1);
    assert_true("router_clamp_invalid", EditorModeRouter_ClampEditorMode(7, false) == 0);
    assert_true("router_clamp_lock_scene_to_path",
                EditorModeRouter_ClampEditorMode(1, true) == 0);
    assert_true("router_next_unlocked_forward",
                EditorModeRouter_NextEditorMode(0, false, false) == 1);
    assert_true("router_next_unlocked_reverse",
                EditorModeRouter_NextEditorMode(0, true, false) == 2);
    assert_true("router_next_locked_forward",
                EditorModeRouter_NextEditorMode(0, false, true) == 2);
    assert_true("router_next_locked_reverse",
                EditorModeRouter_NextEditorMode(2, true, true) == 0);
    return 0;
}

static int test_scene_editor_control_surface_source_path_parity(void) {
    SceneEditorControlSurfaceInput input;
    SceneEditorControlSurfaceContract contract;

    memset(&input, 0, sizeof(input));
    memset(&contract, 0, sizeof(contract));
    input.requestedMode = 0;
    input.lockObjectMode = false;
    input.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    input.sourceLabel = "Runtime Scene";
    input.sourcePath = "";
    input.objectCount = 3;
    input.route.routeFamily = RAY_TRACING_ROUTE_CANONICAL_2D;

    SceneEditorControlSurfaceBuild(&input, &contract);

    assert_true("surface_runtime_source_label",
                strstr(contract.statusSource, "Runtime Scene") != NULL);
    assert_true("surface_runtime_source_path_default",
                strstr(contract.statusPath, "runtime scene: none selected") != NULL);
    assert_true("surface_runtime_route_label_2d",
                strstr(contract.statusRoute, "2D(canonical)") != NULL);
    assert_true("surface_runtime_tab_shared",
                contract.sharedKeyTabCycleEnabled);
    assert_true("surface_runtime_escape_shared",
                contract.sharedKeyEscapeEnabled);
    assert_true("surface_runtime_canvas_enabled",
                contract.laneCanvasEditEnabled);
    assert_true("surface_runtime_bezier_canvas_enabled",
                contract.laneBezierCanvasEditEnabled);
    assert_true("surface_runtime_object_canvas_enabled",
                contract.laneObjectCanvasEditEnabled);
    assert_true("surface_runtime_camera_canvas_enabled",
                contract.laneCameraCanvasEditEnabled);
    assert_true("surface_runtime_viewport_bezier_disabled",
                !contract.laneViewportBezierPlacementEnabled);
    assert_true("surface_runtime_viewport_pick_disabled",
                !contract.laneViewportObjectPickEnabled);
    assert_true("surface_runtime_orbit_disabled_in_2d",
                !contract.laneGestureOrbitEnabled);
    assert_true("surface_runtime_controls_hint_shared",
                strstr(contract.statusControls, "Shared TAB cycle ESC close") != NULL);

    memset(&input, 0, sizeof(input));
    memset(&contract, 0, sizeof(contract));
    input.requestedMode = 0;
    input.lockObjectMode = false;
    input.sceneSource = SCENE_SOURCE_CONFIG_2D;
    input.sourceLabel = "2D Config";
    input.sourcePath = "(default)";
    input.objectCount = 2;
    input.route.routeFamily = RAY_TRACING_ROUTE_CANONICAL_2D;

    SceneEditorControlSurfaceBuild(&input, &contract);

    assert_true("surface_2d_source_path_default",
                strstr(contract.statusPath, "default 2D config") != NULL);
    return 0;
}

static int test_scene_editor_control_surface_native_lane_parity(void) {
    SceneEditorControlSurfaceInput input;
    SceneEditorControlSurfaceContract contract;

    memset(&input, 0, sizeof(input));
    memset(&contract, 0, sizeof(contract));
    input.requestedMode = 2;
    input.lockObjectMode = false;
    input.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    input.sourceLabel = "Runtime Scene";
    input.sourcePath = "/tmp/runtime_scene_native_test.json";
    input.objectCount = 5;
    input.route.routeFamily = RAY_TRACING_ROUTE_NATIVE_3D;

    SceneEditorControlSurfaceBuild(&input, &contract);

    assert_true("surface_native_lane_enum",
                contract.lane == SCENE_EDITOR_CONTROL_SURFACE_LANE_NATIVE_3D_RESERVED);
    assert_true("surface_native_digest_reserved",
                strstr(contract.statusDigest, "native 3D lane reserved") != NULL);
    assert_true("surface_native_runtime_reserved",
                strstr(contract.statusRuntime, "native lane reserved") != NULL);
    assert_true("surface_native_preview_disabled", !contract.previewEnabled);
    assert_true("surface_native_frame_disabled", !contract.laneKeyFrameEnabled);
    assert_true("surface_native_orbit_disabled", !contract.laneGestureOrbitEnabled);
    assert_true("surface_native_wheel_disabled", !contract.laneWheelZoomEnabled);
    assert_true("surface_native_bezier_canvas_disabled", !contract.laneBezierCanvasEditEnabled);
    assert_true("surface_native_object_canvas_disabled", !contract.laneObjectCanvasEditEnabled);
    assert_true("surface_native_camera_canvas_disabled", !contract.laneCameraCanvasEditEnabled);
    assert_true("surface_native_viewport_bezier_disabled", !contract.laneViewportBezierPlacementEnabled);
    assert_true("surface_native_viewport_pick_disabled", !contract.laneViewportObjectPickEnabled);
    assert_true("surface_native_canvas_disabled", !contract.laneCanvasEditEnabled);
    assert_true("surface_native_controls_pending",
                strstr(contract.statusControls, "pending") != NULL);
    return 0;
}

static int test_scene_editor_control_surface_controlled_3d_interaction_parity(void) {
    SceneEditorControlSurfaceInput input;
    SceneEditorControlSurfaceContract contract;

    memset(&input, 0, sizeof(input));
    memset(&contract, 0, sizeof(contract));
    input.requestedMode = 2;
    input.lockObjectMode = false;
    input.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    input.sourceLabel = "Runtime Scene";
    input.sourcePath = "/tmp/runtime_scene_controlled_lane.json";
    input.objectCount = 3;
    input.route.routeFamily = RAY_TRACING_ROUTE_COMPAT_3D_FALLBACK;

    SceneEditorControlSurfaceBuild(&input, &contract);

    assert_true("surface_controlled3d_lane",
                contract.lane == SCENE_EDITOR_CONTROL_SURFACE_LANE_CONTROLLED_3D);
    assert_true("surface_controlled3d_preview_enabled",
                contract.previewEnabled);
    assert_true("surface_controlled3d_tab_shared",
                contract.sharedKeyTabCycleEnabled);
    assert_true("surface_controlled3d_escape_shared",
                contract.sharedKeyEscapeEnabled);
    assert_true("surface_controlled3d_frame_enabled",
                contract.laneKeyFrameEnabled);
    assert_true("surface_controlled3d_orbit_enabled",
                contract.laneGestureOrbitEnabled);
    assert_true("surface_controlled3d_wheel_enabled",
                contract.laneWheelZoomEnabled);
    assert_true("surface_controlled3d_bezier_canvas_disabled",
                !contract.laneBezierCanvasEditEnabled);
    assert_true("surface_controlled3d_object_canvas_disabled_in_camera_mode",
                !contract.laneObjectCanvasEditEnabled);
    assert_true("surface_controlled3d_camera_canvas_disabled",
                !contract.laneCameraCanvasEditEnabled);
    assert_true("surface_controlled3d_viewport_bezier_disabled_in_camera_mode",
                !contract.laneViewportBezierPlacementEnabled);
    assert_true("surface_controlled3d_viewport_pick_disabled_in_camera_mode",
                !contract.laneViewportObjectPickEnabled);
    assert_true("surface_controlled3d_canvas_disabled",
                !contract.laneCanvasEditEnabled);
    assert_true("surface_controlled3d_controls_has_orbit",
                strstr(contract.statusControls, "Alt+drag orbit") != NULL);
    return 0;
}

static int test_scene_editor_control_surface_controlled_3d_bezier_mode_enablement(void) {
    SceneEditorControlSurfaceInput input;
    SceneEditorControlSurfaceContract contract;

    memset(&input, 0, sizeof(input));
    memset(&contract, 0, sizeof(contract));
    input.requestedMode = 0;
    input.lockObjectMode = false;
    input.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    input.sourceLabel = "Runtime Scene";
    input.sourcePath = "/tmp/runtime_scene_controlled_lane.json";
    input.objectCount = 3;
    input.route.routeFamily = RAY_TRACING_ROUTE_COMPAT_3D_FALLBACK;

    SceneEditorControlSurfaceBuild(&input, &contract);

    assert_true("surface_controlled3d_bezier_mode_lane",
                contract.lane == SCENE_EDITOR_CONTROL_SURFACE_LANE_CONTROLLED_3D);
    assert_true("surface_controlled3d_bezier_mode_active_mode",
                contract.activeMode == 0);
    assert_true("surface_controlled3d_bezier_mode_canvas_disabled",
                !contract.laneBezierCanvasEditEnabled);
    assert_true("surface_controlled3d_bezier_mode_viewport_enabled",
                contract.laneViewportBezierPlacementEnabled);
    assert_true("surface_controlled3d_bezier_mode_object_pick_disabled",
                !contract.laneViewportObjectPickEnabled);
    assert_true("surface_controlled3d_bezier_mode_lane_canvas_enabled",
                contract.laneCanvasEditEnabled);
    assert_true("surface_controlled3d_bezier_mode_controls_hint",
                strstr(contract.statusControls, "LMB select bezier") != NULL);
    assert_true("surface_controlled3d_bezier_mode_shift_add_hint",
                strstr(contract.statusControls, "Shift+LMB add point") != NULL);
    assert_true("surface_controlled3d_bezier_mode_controls_smooth_drag",
                strstr(contract.statusControls, "Cmd+drag smooth") != NULL);
    return 0;
}

static int test_scene_editor_control_surface_controlled_3d_object_mode_canvas_enablement(void) {
    SceneEditorControlSurfaceInput input;
    SceneEditorControlSurfaceContract contract;

    memset(&input, 0, sizeof(input));
    memset(&contract, 0, sizeof(contract));
    input.requestedMode = 1;
    input.lockObjectMode = false;
    input.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    input.sourceLabel = "Runtime Scene";
    input.sourcePath = "/tmp/runtime_scene_controlled_lane.json";
    input.objectCount = 3;
    input.route.routeFamily = RAY_TRACING_ROUTE_COMPAT_3D_FALLBACK;

    SceneEditorControlSurfaceBuild(&input, &contract);

    assert_true("surface_controlled3d_object_mode_lane",
                contract.lane == SCENE_EDITOR_CONTROL_SURFACE_LANE_CONTROLLED_3D);
    assert_true("surface_controlled3d_object_mode_active_mode",
                contract.activeMode == 1);
    assert_true("surface_controlled3d_object_mode_bezier_canvas_disabled",
                !contract.laneBezierCanvasEditEnabled);
    assert_true("surface_controlled3d_object_mode_viewport_bezier_disabled",
                !contract.laneViewportBezierPlacementEnabled);
    assert_true("surface_controlled3d_object_mode_object_canvas_enabled",
                contract.laneObjectCanvasEditEnabled);
    assert_true("surface_controlled3d_object_mode_camera_canvas_disabled",
                !contract.laneCameraCanvasEditEnabled);
    assert_true("surface_controlled3d_object_mode_viewport_pick_enabled",
                contract.laneViewportObjectPickEnabled);
    assert_true("surface_controlled3d_object_mode_lane_canvas_enabled",
                contract.laneCanvasEditEnabled);
    assert_true("surface_controlled3d_object_mode_controls_hint",
                strstr(contract.statusControls, "LMB pick object") != NULL);
    return 0;
}

static int test_scene_editor_control_surface_selected_object_status(void) {
    SceneEditorControlSurfaceInput input;
    SceneEditorControlSurfaceContract contract;

    memset(&input, 0, sizeof(input));
    memset(&contract, 0, sizeof(contract));
    input.requestedMode = 1;
    input.lockObjectMode = false;
    input.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    input.sourceLabel = "Runtime Scene";
    input.sourcePath = "/tmp/runtime_scene_controlled_lane.json";
    input.objectCount = 3;
    input.hasSelectedObject = true;
    input.selectedObjectIndex = 2;
    input.route.routeFamily = RAY_TRACING_ROUTE_COMPAT_3D_FALLBACK;

    SceneEditorControlSurfaceBuild(&input, &contract);

    assert_true("surface_selected_status_has_index",
                strstr(contract.statusObjects, "Selected: #2") != NULL);
    return 0;
}

// Minimal deterministic scene harness (direct + forward + camera)
static void setup_tiny_scene(void) {
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.windowWidth = 64;
    sceneSettings.windowHeight = 64;
    sceneSettings.bezierPath.numPoints = 1;
    sceneSettings.bezierPath.points[0].x = 20.0;
    sceneSettings.bezierPath.points[0].y = 0.0;
    sceneSettings.camera.x = 0.0;
    sceneSettings.camera.y = 0.0;
    sceneSettings.camera.zoom = 1.0;

    sceneSettings.objectCount = 2;
    memset(sceneSettings.sceneObjects, 0, sizeof(sceneSettings.sceneObjects));
    strncpy(sceneSettings.sceneObjects[0].type, "circle", sizeof(sceneSettings.sceneObjects[0].type) - 1);
    sceneSettings.sceneObjects[0].x = 0.0;
    sceneSettings.sceneObjects[0].y = 0.0;
    sceneSettings.sceneObjects[0].radius = 5.0;
    sceneSettings.sceneObjects[0].scale = 1.0;
    sceneSettings.sceneObjects[0].color = 0xFFFFFF;
    sceneSettings.sceneObjects[0].opacity = 1.0f;
    sceneSettings.sceneObjects[0].reflectivity = 0.0f;
    sceneSettings.sceneObjects[0].roughness = 0.5f;

    strncpy(sceneSettings.sceneObjects[1].type, "circle", sizeof(sceneSettings.sceneObjects[1].type) - 1);
    sceneSettings.sceneObjects[1].x = -15.0;
    sceneSettings.sceneObjects[1].y = -5.0;
    sceneSettings.sceneObjects[1].radius = 3.0;
    sceneSettings.sceneObjects[1].scale = 1.0;
    sceneSettings.sceneObjects[1].color = 0x808080;
    sceneSettings.sceneObjects[1].opacity = 1.0f;
    sceneSettings.sceneObjects[1].reflectivity = 0.2f;
    sceneSettings.sceneObjects[1].roughness = 0.1f;

    animSettings.integratorMode = 0; // forward by default
    animSettings.spaceMode = SPACE_MODE_2D;
    animSettings.useTiledRenderer = false;
    animSettings.tileSize = 16;
    animSettings.cacheVarianceCutoff = 0.35;
    animSettings.cacheHaloRadius = 3.5;
    sceneSettings.rays = 64;
    animSettings.lightIntensity = 2.0;
}

static int sample_pixel_energy(const float* buffer, int w, int h, int x, int y, float* out) {
    if (!buffer || x < 0 || y < 0 || x >= w || y >= h) return 0;
    *out = buffer[(size_t)y * (size_t)w + (size_t)x];
    return 1;
}

static int test_deterministic_modes(void) {
    setup_tiny_scene();
    InitRayTracingScene();

    int w = sceneSettings.windowWidth;
    int h = sceneSettings.windowHeight;
    size_t count = (size_t)w * (size_t)h;
    float* scratch = (float*)malloc(count * sizeof(float));
    if (!scratch) return 0;

    // Build simple material table
    MaterialBSDF materials[sceneSettings.objectCount];
    for (int i = 0; i < sceneSettings.objectCount; i++) {
        MaterialBSDFInitFromSceneObject(&sceneSettings.sceneObjects[i], &materials[i]);
    }

    // Build a uniform grid for intersection tests
    UniformGrid grid = {0};
    UniformGridBuild(&grid,
                     sceneSettings.sceneObjects,
                     sceneSettings.objectCount,
                     NULL,
                     8.0);

    LightSource light = { .x = sceneSettings.bezierPath.points[0].x,
                          .y = sceneSettings.bezierPath.points[0].y,
                          .radius = 3.0 };

    // Direct-only mode
    IntegratorContext ctx = {
        .pixelBuffer = (Uint8*)malloc(count),
        .energyBuffer = scratch,
        .directEnergyBuffer = NULL,
        .width = w,
        .height = h,
        .objects = sceneSettings.sceneObjects,
        .objectCount = sceneSettings.objectCount,
        .tileGrid = NULL,
        .useTiles = false,
        .frameSeed = 1,
        .uniformGrid = (grid.objectCells || grid.triangleCells) ? &grid : NULL,
        .integratorMode = 2,
        .cache = NULL,
        .materials = materials,
        .materialCount = sceneSettings.objectCount,
        .mesh = NULL,
        .triangleMesh = NULL
    };
    memset(ctx.pixelBuffer, 0, count);
    memset(ctx.energyBuffer, 0, count * sizeof(float));
    DirectLightIntegratorRender(&ctx, &light);
    float directSample = 0.0f;
    sample_pixel_energy(ctx.energyBuffer, w, h, w / 2, h / 2, &directSample);

    // Forward mode (no tiles)
    memset(ctx.pixelBuffer, 0, count);
    memset(ctx.energyBuffer, 0, count * sizeof(float));
    animSettings.integratorMode = 0;
    ForwardLightIntegratorRender(&ctx, &light);
    float forwardSample = 0.0f;
    sample_pixel_energy(ctx.energyBuffer, w, h, w / 2, h / 2, &forwardSample);
    float forwardMax = 0.0f;
    for (size_t i = 0; i < count; i++) {
        if (ctx.energyBuffer[i] > forwardMax) forwardMax = ctx.energyBuffer[i];
    }

    // Camera-path mode uses cache-less run (no cache passed)
    memset(ctx.pixelBuffer, 0, count);
    memset(ctx.energyBuffer, 0, count * sizeof(float));
    animSettings.integratorMode = 1;
    CameraIntegratorSettings settings = {
        .directIntensityScale = animSettings.lightIntensity,
        .indirectVariance = animSettings.cacheVarianceCutoff,
        .indirectHaloRadius = animSettings.cacheHaloRadius,
        .blurEnabled = false,
        .brightnessBoost = 1.0
    };
    CameraPathIntegratorRenderFromContext(&ctx,
                                          &light,
                                          &settings,
                                          sceneSettings.camera.x,
                                          sceneSettings.camera.y);
    float cameraSample = 0.0f;
    sample_pixel_energy(ctx.energyBuffer, w, h, w / 2, h / 2, &cameraSample);

    assert_true("deterministic_direct_positive", directSample >= 0.0f);
    assert_true("deterministic_forward_nonzero", forwardMax > 0.0f || forwardSample > 0.0f);
    assert_true("deterministic_camera_nonnegative", cameraSample >= 0.0f);

    free(ctx.pixelBuffer);
    free(scratch);
    UniformGridFree(&grid);
    CleanupRayTracing();
    return 0;
}

// Debug: verify normal orientation and pdf validity on a single hit
static int test_hit_normal_and_pdfs(void) {
    setup_tiny_scene();
    InitRayTracingScene();

    int w = sceneSettings.windowWidth;
    int h = sceneSettings.windowHeight;
    LightSource light = { .x = sceneSettings.bezierPath.points[0].x,
                          .y = sceneSettings.bezierPath.points[0].y,
                          .radius = 3.0 };

    IntegratorContext ctx = {
        .pixelBuffer = NULL,
        .energyBuffer = NULL,
        .directEnergyBuffer = NULL,
        .width = w,
        .height = h,
        .objects = sceneSettings.sceneObjects,
        .objectCount = sceneSettings.objectCount,
        .tileGrid = NULL,
        .useTiles = false,
        .frameSeed = 1,
        .uniformGrid = NULL,
        .integratorMode = 1,
        .cache = NULL,
        .materials = NULL,
        .materialCount = 0,
        .mesh = NULL,
        .triangleMesh = NULL
    };

    // Build a tiny uniform grid for direct intersection
    UniformGrid grid = {0};
    UniformGridBuild(&grid,
                     sceneSettings.sceneObjects,
                     sceneSettings.objectCount,
                     NULL,
                     4.0);
    ctx.uniformGrid = &grid;

    // Build a crude mesh for segments if needed by intersection code
    SurfaceMesh mesh = {0};
    SurfaceMeshInit(&mesh);
    SurfaceBuildMeshes(&mesh, NULL,
                       sceneSettings.sceneObjects,
                       sceneSettings.objectCount,
                       8.0);
    ctx.mesh = &mesh;

    // Ray from camera center through screen center
    // Aim a ray from camera toward the first object's center to guarantee a hit
    const SceneObject* target = &sceneSettings.sceneObjects[0];
    double tx = target->x;
    double ty = target->y;
    double dx = tx - sceneSettings.camera.x;
    double dy = ty - sceneSettings.camera.y;
    double len = sqrt(dx*dx + dy*dy);
    if (len < 1e-6) { dx = 0.0; dy = 1.0; len = 1.0; }
    dx /= len; dy /= len;
    Ray2D ray = { sceneSettings.camera.x, sceneSettings.camera.y, dx, dy };
    HitInfo2D hit = {0};
    hit.objectIndex = -1; hit.triangleIndex = -1; hit.baryW = 1.0;
    bool ok = UniformGridTraceRay(ctx.uniformGrid, &ray, PATH_EPSILON, DBL_MAX, &hit);
    assert_true("debug_trace_hit", ok);
    if (ok) {
        // Orient normal for incoming
        double inx = -dx, iny = -dy;
        double lenIn = sqrt(inx*inx + iny*iny);
        if (lenIn > 1e-9) { inx /= lenIn; iny /= lenIn; }
        double ndot = inx * hit.nx + iny * hit.ny;
        if (ndot < 0.0) { hit.nx = -hit.nx; hit.ny = -hit.ny; ndot = -ndot; }

        // Orient normal and check it faces incoming
        assert_true("debug_normal_facing", ndot >= 0.0);

        // BSDF pdf/value checks
        MaterialBSDF m = {0};
        MaterialBSDFInitFromSceneObject(&sceneSettings.sceneObjects[hit.objectIndex], &m);
        BSDFSample s;
        FastRNG rng; FastRNGSeed(&rng, 111, 222);
        bool sampled = MaterialBSDFSample(&m, hit.nx, hit.ny, inx, iny, 0.0, &rng, &s);
        assert_true("debug_bsdf_sample_ok", sampled);
        if (sampled) {
            double pdf = MaterialBSDFAngularPdf(&m, hit.nx, hit.ny, inx, iny, s.dirX, s.dirY);
            assert_true("debug_pdf_positive", pdf > 0.0);
            double val = MaterialBSDFEvaluateCos(&m, hit.nx, hit.ny, inx, iny, s.dirX, s.dirY);
            assert_true("debug_eval_positive", val >= 0.0);
        }

        // Light PDF check at hit
        double lx = light.x - hit.px;
        double ly = light.y - hit.py;
        double lDist = sqrt(lx*lx + ly*ly);
        if (lDist > 1e-6) { lx /= lDist; ly /= lDist; }
        double pdfL = CircleLightPdfSolidAngle(&light, hit.px, hit.py, 0.0);
        assert_true("debug_light_pdf_positive", pdfL > 0.0);
    }

    SurfaceMeshFree(&mesh);
    UniformGridFree(&grid);
    CleanupRayTracing();
    return 0;
}

static int run_bridge_apply_file_mode(const char *runtime_scene_path) {
    RuntimeSceneBridgePreflight preflight;
    RuntimeSceneBridgePreflight summary;
    bool ok = false;
    if (!runtime_scene_path || !runtime_scene_path[0]) {
        fprintf(stderr, "runtime_scene_bridge_apply_file: missing path\n");
        return EXIT_FAILURE;
    }
    ok = runtime_scene_bridge_preflight_file(runtime_scene_path, &preflight);
    if (!ok) {
        fprintf(stderr, "runtime_scene_bridge_preflight_file failed: %s\n", preflight.diagnostics);
        return EXIT_FAILURE;
    }
    ok = runtime_scene_bridge_apply_file(runtime_scene_path, &summary);
    if (!ok) {
        fprintf(stderr, "runtime_scene_bridge_apply_file failed: %s\n", summary.diagnostics);
        return EXIT_FAILURE;
    }
    printf("runtime_scene_bridge_apply_file: PASS scene_id=%s objects=%d materials=%d lights=%d cameras=%d\n",
           summary.scene_id,
           summary.object_count,
           summary.material_count,
           summary.light_count,
           summary.camera_count);
    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    if (argc == 3 && strcmp(argv[1], "--bridge-apply-file") == 0) {
        return run_bridge_apply_file_mode(argv[2]);
    }
    if (argc != 1) {
        fprintf(stderr, "usage: %s [--bridge-apply-file <scene_runtime.json>]\n", argv[0]);
        return EXIT_FAILURE;
    }

    test_diffuse_evaluate();
    test_diffuse_pdf();
    test_sample_diffuse_consistency();
    test_scene_object_z_roundtrip();
    test_scene_object_z_missing_fallback();
    test_animation_scene_source_legacy_migration();
    test_animation_scene_source_roundtrip_runtime_lane();
    test_animation_scene_source_select_runtime_failure_rolls_back();
    test_animation_scene_source_select_runtime_persists_on_save();
    test_animation_apply_active_scene_source_invalid_fluid_falls_back_2d();
    test_animation_restore_active_scene_source_persists_fallback_correction();
    test_manifest_default_roots_expands_runtime_and_legacy_paths();
    test_scene_source_catalog_collect_admits_runtime_and_manifest_lanes();
    test_menu_state_manifest_option_visibility_matrix();
    test_depth_projection_scalars();
    test_runtime_scene_bridge_preflight_accepts_runtime_contract();
    test_runtime_scene_bridge_rejects_authoring_variant();
    test_runtime_scene_bridge_rejects_malformed_runtime_payload();
    test_runtime_scene_bridge_optional_lanes_default_deterministic();
    test_runtime_scene_bridge_rejects_noncanonical_unit_system();
    test_runtime_scene_bridge_apply_uses_world_scale_mapping();
    test_runtime_scene_bridge_apply_preserves_editor_mode_state();
    test_runtime_scene_bridge_apply_3d_primitives_scaffold();
    test_runtime_scene_bridge_apply_ps4d_fixture_retains_digest_truth();
    test_scene_compile_and_preflight_roundtrip();
    test_runtime_scene_bridge_apply_runtime_fixture();
    test_path_eval_3d_uses_linear_handle_units();
    test_runtime_scene_bridge_apply_compile_output();
    test_runtime_scene_bridge_writeback_overlay_preserves_non_ray_state();
    test_runtime_scene_bridge_writeback_rejects_foreign_extension_namespace();
    test_runtime_scene_bridge_writeback_rejects_forbidden_top_level_overlay_key();
    test_runtime_scene_bridge_writeback_rejects_invalid_space_mode_value();
    test_runtime_scene_bridge_writeback_rejects_non_object_ray_extension_payload();
    test_runtime_scene_bridge_writeback_rejects_missing_overlay_meta();
    test_runtime_scene_bridge_writeback_rejects_stale_logical_clock();
    test_runtime_scene_bridge_writeback_rejects_wrong_overlay_producer();
    test_runtime_scene_bridge_writeback_rejects_negative_logical_clock();
    test_runtime_scene_bridge_writeback_rejects_runtime_core_unit_system_overlay();
    test_runtime_scene_bridge_writeback_rejects_runtime_core_world_scale_overlay();
    test_runtime_scene_bridge_writeback_space_mode_tiebreak_rejects_lexically_larger_producer();
    test_runtime_scene_bridge_writeback_space_mode_tiebreak_accepts_lexically_smaller_producer();
    test_runtime_scene_bridge_trio_fixture_compile_writeback_apply();
    test_runtime_scene_bridge_apply_hydrates_ray_authoring_paths();
    test_scene_editor_runtime_scene_persistence_roundtrip();
    test_mode_backend_route_2d_defaults();
    test_mode_backend_route_3d_controlled_lane();
    test_mode_backend_scene_digest_status_2d_canonical_empty();
    test_mode_backend_scene_digest_status_ps4d_fixture();
    test_mode_backend_view_carrier_2d_defaults();
    test_mode_backend_view_carrier_3d_compat_fallback();
    test_mode_backend_primitive_prep_plan_2d_defaults();
    test_mode_backend_primitive_prep_plan_3d_compat_placeholder();
    test_mode_backend_primitive_prep_plan_native3d_placeholder_contract();
    test_editor_mode_router_capabilities_2d();
    test_editor_mode_router_capabilities_3d_scaffold();
    test_editor_mode_router_cycle_policy();
    test_scene_editor_control_surface_source_path_parity();
    test_scene_editor_control_surface_native_lane_parity();
    test_scene_editor_control_surface_controlled_3d_interaction_parity();
    test_scene_editor_control_surface_controlled_3d_bezier_mode_enablement();
    test_scene_editor_control_surface_controlled_3d_object_mode_canvas_enablement();
    test_scene_editor_control_surface_selected_object_status();
    test_deterministic_modes();
    test_hit_normal_and_pdfs();
    failures += run_fluid_pack_import_tests();
    failures += run_kit_viz_fluid_overlay_adapter_tests();
    failures += run_render_metrics_dataset_tests();

    if (failures > 0) {
        printf("TEST RESULT: %d failure(s)\n", failures);
        return EXIT_FAILURE;
    }
    printf("TEST RESULT: PASS\n");
    return EXIT_SUCCESS;
}
