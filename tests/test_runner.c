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
#include "app/animation_output.h"
#include "app/render_export_batch.h"
#include "app/preview_mode_route.h"
#include "app/preview_playback.h"
#include "app/preview_camera_sample.h"
#include "app/preview_camera_projector.h"
#include "app/preview_retained_scene_renderer.h"
#include "editor/editor_mode_router.h"
#include "editor/scene_editor_control_surface.h"
#include "editor/scene_editor_runtime_scene_persistence.h"
#include "editor/scene_editor_tool_state.h"
#include "render/ray_tracing2.h"
#include "render/ray_tracing_mode_backend.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_direct_light_3d.h"
#include "render/runtime_diffuse_bounce_3d.h"
#include "render/runtime_emission_transparency_3d.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_material_response_3d.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_native_3d_tile_occupancy.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_scene_3d_builder.h"
#include "render/runtime_visibility_3d.h"
#include "render/integrator_common.h"
#include "render/ray_tracing2_preview.h"
#include "render/integrators/direct_light_integrator.h"
#include "render/integrators/forward_light_integrator.h"
#include "render/integrators/hybrid/camera_path_integrator.h"
#include "render/uniform_grid.h"
#include "render/light_pdf.h"
#include "render/ray_types.h"
#include "render/render_helper.h"
#include "render/ray_tracing_integrator_catalog.h"
#include "import/runtime_scene_bridge.h"
#include "path/path_system.h"
#include "config/config_scene_path_io.h"
#include "core_scene_compile.h"
#include "ui/menu_batch_panel.h"
#include "ui/menu_layout.h"
#include "ui/menu_panel_chrome.h"
#include "ui/sdl_menu_render.h"
#include "ui/scene_source_catalog.h"
#include "ui/sdl_menu_state.h"
#include "fluid_pack_import_test.h"
#include "kit_viz_fluid_overlay_adapter_test.h"
#include "render_metrics_dataset_test.h"
#include "cJSON.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int failures = 0;
static const char* kRuntimeSceneConfigPath = "data/runtime/scene_config.json";
static const char* kRuntimeAnimationConfigPath = "data/runtime/animation_config.json";

static int test_rect_right(const SDL_Rect *rect) {
    return rect ? rect->x + rect->w : 0;
}

static int test_rect_bottom(const SDL_Rect *rect) {
    return rect ? rect->y + rect->h : 0;
}

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

static bool path_exists(const char *path) {
    struct stat st;
    if (!path || !path[0]) return false;
    return stat(path, &st) == 0;
}

static void restore_env_or_unset(const char* name, const char* value) {
    if (!name) return;
    if (value) {
        setenv(name, value, 1);
    } else {
        unsetenv(name);
    }
}

static cJSON* find_named_dataset_entry(cJSON* items, const char* name) {
    if (!cJSON_IsArray(items) || !name) return NULL;
    cJSON* entry = NULL;
    cJSON_ArrayForEach(entry, items) {
        cJSON* entry_name = cJSON_GetObjectItem(entry, "name");
        if (cJSON_IsString(entry_name) && strcmp(entry_name->valuestring, name) == 0) {
            return entry;
        }
    }
    return NULL;
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

static const RuntimeSceneBridgePrimitiveSeed *find_primitive_seed_by_object_id(
    const RuntimeSceneBridge3DPrimitiveSeedState *seed_state,
    const char *object_id) {
    if (!seed_state || !object_id || !object_id[0]) return NULL;
    for (int i = 0; i < seed_state->primitive_count; ++i) {
        if (strcmp(seed_state->primitives[i].object_id, object_id) == 0) {
            return &seed_state->primitives[i];
        }
    }
    return NULL;
}

static int find_runtime_primitive_index_by_object_id(const RuntimeScene3D *scene,
                                                     const char *object_id) {
    if (!scene || !object_id || !object_id[0]) return -1;
    for (int i = 0; i < scene->primitiveCount; ++i) {
        if (strcmp(scene->primitives[i].source.objectId, object_id) == 0) {
            return i;
        }
    }
    return -1;
}

static int count_runtime_triangles_for_primitive(const RuntimeScene3D *scene,
                                                 int primitive_index) {
    int total = 0;
    if (!scene || primitive_index < 0) return 0;
    for (int i = 0; i < scene->triangleMesh.triangleCount; ++i) {
        if (scene->triangleMesh.triangles[i].primitiveIndex == primitive_index) total += 1;
    }
    return total;
}

static double expected_camera_pitch_for_t(const Path *camera_path,
                                          const CameraPath3D *camera_path3d,
                                          double normalized_t) {
    int segment = 0;
    int next = 0;
    double local_t = 0.0;
    double pitch0 = 0.0;
    double pitch1 = 0.0;
    if (!camera_path || !camera_path3d || camera_path->numPoints <= 0) {
        return 0.0;
    }
    if (camera_path->numPoints == 1) {
        return camera_path3d->point_pitch[0];
    }
    PathMapNormalizedT(camera_path, normalized_t, &segment, &local_t);
    if (segment < 0) segment = 0;
    if (segment >= camera_path->numPoints) segment = camera_path->numPoints - 1;
    next = (segment + 1 < camera_path->numPoints) ? (segment + 1) : segment;
    pitch0 = camera_path3d->point_pitch[segment];
    pitch1 = camera_path3d->point_pitch[next];
    return pitch0 + (pitch1 - pitch0) * local_t;
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

static void test_scene_editor_tool_state_contract(void) {
    SceneEditorToolStateReset();
    assert_true("tool_state_default_select",
                SceneEditorToolStateGetActive() == SCENE_EDITOR_TOOL_SELECT);
    assert_true("tool_state_default_label",
                strcmp(SceneEditorToolStateToolLabel(SceneEditorToolStateGetActive()), "Select") == 0);
    assert_true("tool_state_select_active",
                SceneEditorToolStateToolIsActive(SCENE_EDITOR_TOOL_SELECT));
    assert_true("tool_state_shift_select_to_add",
                SceneEditorToolStateGetEffective(KMOD_SHIFT) == SCENE_EDITOR_TOOL_ADD);

    SceneEditorToolStateSetActive(SCENE_EDITOR_TOOL_ADD);
    assert_true("tool_state_add_active",
                SceneEditorToolStateGetActive() == SCENE_EDITOR_TOOL_ADD);
    assert_true("tool_state_shift_add_stays_add",
                SceneEditorToolStateGetEffective(KMOD_SHIFT) == SCENE_EDITOR_TOOL_ADD);

    SceneEditorToolStateSetActive(SCENE_EDITOR_TOOL_DELETE);
    assert_true("tool_state_delete_active",
                SceneEditorToolStateGetActive() == SCENE_EDITOR_TOOL_DELETE);
    assert_true("tool_state_shift_delete_stays_delete",
                SceneEditorToolStateGetEffective(KMOD_SHIFT) == SCENE_EDITOR_TOOL_DELETE);

    SceneEditorToolStateToggleOrReset(SCENE_EDITOR_TOOL_DELETE);
    assert_true("tool_state_toggle_delete_to_select",
                SceneEditorToolStateGetActive() == SCENE_EDITOR_TOOL_SELECT);
    SceneEditorToolStateToggleOrReset(SCENE_EDITOR_TOOL_ADD);
    assert_true("tool_state_toggle_select_to_add",
                SceneEditorToolStateGetActive() == SCENE_EDITOR_TOOL_ADD);

    SceneEditorToolStateSetActive((SceneEditorTool)99);
    assert_true("tool_state_invalid_clamps_select",
                SceneEditorToolStateGetActive() == SCENE_EDITOR_TOOL_SELECT);
    assert_true("tool_state_resolve_invalid_clamps_select",
                SceneEditorToolStateResolveEffective((SceneEditorTool)99, KMOD_NONE) ==
                    SCENE_EDITOR_TOOL_SELECT);
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
    strncpy(animSettings.runtimeScenePath, "third_party/codework_shared/assets/scenes/trio_contract/scene_runtime_min.json",
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
                       "third_party/codework_shared/assets/scenes/trio_contract/scene_runtime_min.json") == 0);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_integrator_split_roundtrip_and_default_3d(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char* json_missing_3d_integrator =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"integratorMode\": 1\n"
        "}\n";

    assert_true("integrator_split_write_missing_3d",
                write_text_file(kRuntimeAnimationConfigPath, json_missing_3d_integrator));
    LoadAnimationConfig();
    assert_true("integrator_split_missing_3d_defaulted",
                animSettings.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT);
    assert_true("integrator_split_2d_preserved_on_load",
                animSettings.integratorMode == RAY_TRACING_2D_INTEGRATOR_HYBRID);

    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_HYBRID;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DISNEY;
    SaveAnimationConfig();
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_FORWARD_LIGHT;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT;
    LoadAnimationConfig();

    assert_true("integrator_split_roundtrip_2d_persisted",
                animSettings.integratorMode == RAY_TRACING_2D_INTEGRATOR_HYBRID);
    assert_true("integrator_split_roundtrip_3d_persisted",
                animSettings.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_DISNEY);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_scene_source_select_runtime_failure_rolls_back(void) {
    static const char *kFixtureRuntimePath = "third_party/codework_shared/assets/scenes/trio_contract/scene_runtime_min.json";
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
    static const char *kFixtureRuntimePath = "third_party/codework_shared/assets/scenes/trio_contract/scene_runtime_min.json";
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
    static const char *kRuntimeFixturePath = "third_party/codework_shared/assets/scenes/trio_contract/scene_runtime_min.json";
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

static int test_animation_video_output_root_migrates_from_output_root(void) {
    char tmp_template[] = "/tmp/ray_tracing_video_root_XXXXXX";
    char *tmp_root = mkdtemp(tmp_template);
    size_t backup_size = 0;
    char *backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    char json[1024];
    char expected_video_root[PATH_MAX];

    assert_true("video_output_root_tmpdir_created", tmp_root != NULL);
    if (!tmp_root) {
        restore_runtime_animation_config(backup, backup_size);
        return 0;
    }

    snprintf(json,
             sizeof(json),
             "{\n"
             "  \"inputRoot\": \"config\",\n"
             "  \"outputRoot\": \"%s\",\n"
             "  \"frameDir\": \"%s/frames/default\",\n"
             "  \"fps\": 24,\n"
             "  \"spaceMode\": 0\n"
             "}\n",
             tmp_root,
             tmp_root);
    assert_true("video_output_root_write_runtime",
                write_text_file(kRuntimeAnimationConfigPath, json));
    LoadAnimationConfig();

    assert_true("video_output_root_migrated_compose",
                ray_tracing_compose_path(tmp_root,
                                         "videos",
                                         expected_video_root,
                                         sizeof(expected_video_root)));
    assert_true("video_output_root_migrated_matches",
                strcmp(animSettings.videoOutputRoot, expected_video_root) == 0);
    assert_true("video_output_root_migrated_exists",
                path_exists(animSettings.videoOutputRoot));

    rmdir(expected_video_root);
    rmdir(tmp_root);
    setenv("RAY_TRACING_VIDEO_OUTPUT_ROOT", ray_tracing_default_video_output_root(), 1);
    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_data_paths_resolve_video_output_path_uses_configured_root(void) {
    char output_path[PATH_MAX];
    const char *video_env = getenv("RAY_TRACING_VIDEO_OUTPUT_ROOT");
    char video_env_backup[PATH_MAX] = {0};
    bool had_video_env = false;
    if (video_env && video_env[0]) {
        strncpy(video_env_backup, video_env, sizeof(video_env_backup) - 1);
        video_env_backup[sizeof(video_env_backup) - 1] = '\0';
        had_video_env = true;
    }

    assert_true("video_output_path_resolve_configured",
                ray_tracing_resolve_video_output_path("/tmp/ray_tracing_video_root",
                                                      output_path,
                                                      sizeof(output_path)));
    assert_true("video_output_path_resolve_configured_value",
                strcmp(output_path,
                       "/tmp/ray_tracing_video_root/output.mp4") == 0);

    setenv("RAY_TRACING_VIDEO_OUTPUT_ROOT", ray_tracing_default_video_output_root(), 1);
    assert_true("video_output_path_resolve_default",
                ray_tracing_resolve_video_output_path("",
                                                      output_path,
                                                      sizeof(output_path)));
    assert_true("video_output_path_resolve_default_value",
                strcmp(output_path,
                       ray_tracing_default_video_output_path()) == 0);
    if (had_video_env) {
        setenv("RAY_TRACING_VIDEO_OUTPUT_ROOT", video_env_backup, 1);
    } else {
        unsetenv("RAY_TRACING_VIDEO_OUTPUT_ROOT");
    }
    return 0;
}

static int test_render_export_batch_counts_and_clears_frames(void) {
    char tmp_template[] = "/tmp/ray_tracing_export_frames_XXXXXX";
    char *tmp_root = mkdtemp(tmp_template);
    char frame0[PATH_MAX];
    char frame1[PATH_MAX];
    char note[PATH_MAX];
    char expected_video_output[PATH_MAX];
    char original_frame_dir[sizeof(animSettings.frameDir)];
    char original_video_root[sizeof(animSettings.videoOutputRoot)];
    RayTracingRenderExportStatus status = {0};

    assert_true("export_batch_tmpdir_created", tmp_root != NULL);
    if (!tmp_root) return 0;

    snprintf(frame0, sizeof(frame0), "%s/frame_0000.bmp", tmp_root);
    snprintf(frame1, sizeof(frame1), "%s/frame_0001.bmp", tmp_root);
    snprintf(note, sizeof(note), "%s/keep.txt", tmp_root);
    assert_true("export_batch_write_frame0", write_text_file(frame0, "a"));
    assert_true("export_batch_write_frame1", write_text_file(frame1, "b"));
    assert_true("export_batch_write_note", write_text_file(note, "keep"));

    strncpy(original_frame_dir, animSettings.frameDir, sizeof(original_frame_dir) - 1);
    original_frame_dir[sizeof(original_frame_dir) - 1] = '\0';
    strncpy(original_video_root, animSettings.videoOutputRoot, sizeof(original_video_root) - 1);
    original_video_root[sizeof(original_video_root) - 1] = '\0';

    strncpy(animSettings.frameDir, tmp_root, sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    strncpy(animSettings.videoOutputRoot, tmp_root, sizeof(animSettings.videoOutputRoot) - 1);
    animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';
    assert_true("export_batch_expected_video_path",
                ray_tracing_resolve_video_output_path(animSettings.videoOutputRoot,
                                                      expected_video_output,
                                                      sizeof(expected_video_output)));

    assert_true("export_batch_count_ok", ray_tracing_render_export_count_active_frames(&status));
    assert_true("export_batch_count_two", status.frame_count == 2u);
    assert_true("export_batch_video_path",
                strcmp(status.video_output_path, expected_video_output) == 0);

    assert_true("export_batch_clear_ok", ray_tracing_render_export_clear_active_frames(&status));
    assert_true("export_batch_clear_removed_two", status.files_cleared == 2u);
    assert_true("export_batch_frame0_removed", !path_exists(frame0));
    assert_true("export_batch_frame1_removed", !path_exists(frame1));
    assert_true("export_batch_note_retained", path_exists(note));

    strncpy(animSettings.frameDir, original_frame_dir, sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    strncpy(animSettings.videoOutputRoot, original_video_root, sizeof(animSettings.videoOutputRoot) - 1);
    animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';

    unlink(note);
    rmdir(tmp_root);
    return 0;
}

static int test_render_export_batch_make_video_rejects_empty_frame_dir(void) {
    char tmp_template[] = "/tmp/ray_tracing_export_video_empty_XXXXXX";
    char *tmp_root = mkdtemp(tmp_template);
    char original_frame_dir[sizeof(animSettings.frameDir)];
    char original_video_root[sizeof(animSettings.videoOutputRoot)];
    RayTracingRenderExportStatus status = {0};

    assert_true("export_video_empty_tmpdir_created", tmp_root != NULL);
    if (!tmp_root) return 0;

    strncpy(original_frame_dir, animSettings.frameDir, sizeof(original_frame_dir) - 1);
    original_frame_dir[sizeof(original_frame_dir) - 1] = '\0';
    strncpy(original_video_root, animSettings.videoOutputRoot, sizeof(original_video_root) - 1);
    original_video_root[sizeof(original_video_root) - 1] = '\0';

    strncpy(animSettings.frameDir, tmp_root, sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    strncpy(animSettings.videoOutputRoot, tmp_root, sizeof(animSettings.videoOutputRoot) - 1);
    animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';

    assert_true("export_video_empty_rejected",
                !ray_tracing_render_export_make_video(&status));
    assert_true("export_video_empty_code",
                status.code == RAY_TRACING_RENDER_EXPORT_NO_FRAMES);

    strncpy(animSettings.frameDir, original_frame_dir, sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    strncpy(animSettings.videoOutputRoot, original_video_root, sizeof(animSettings.videoOutputRoot) - 1);
    animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';

    rmdir(tmp_root);
    return 0;
}

static int test_menu_batch_panel_click_starts_frame_dir_edit(void) {
    MenuRuntimeState state;
    MenuBatchPanelLayout layout;
    SDL_Event event;
    memset(&state, 0, sizeof(state));
    memset(&layout, 0, sizeof(layout));
    memset(&event, 0, sizeof(event));

    strncpy(animSettings.frameDir, "/tmp/ray_tracing_menu_frames", sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    layout.frameDirValueRect = (SDL_Rect){100, 120, 220, 34};

    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.x = 110;
    event.button.y = 130;
    event.button.clicks = 1;

    assert_true("menu_batch_frame_edit_click_consumed",
                menu_batch_panel_handle_click(&event, NULL, NULL, &state, &layout));
    assert_true("menu_batch_frame_edit_active", state.editingFrameDir);
    assert_true("menu_batch_frame_edit_buffer",
                strcmp(state.pathInputBuffer, animSettings.frameDir) == 0);
    return 0;
}

static int test_menu_batch_panel_clear_button_updates_frame_count(void) {
    char tmp_template[] = "/tmp/ray_tracing_menu_batch_clear_XXXXXX";
    char *tmp_root = mkdtemp(tmp_template);
    char frame0[PATH_MAX];
    char frame1[PATH_MAX];
    char original_frame_dir[sizeof(animSettings.frameDir)];
    char original_video_root[sizeof(animSettings.videoOutputRoot)];
    MenuRuntimeState state;
    MenuBatchPanelLayout layout;
    SDL_Event event;

    assert_true("menu_batch_clear_tmpdir_created", tmp_root != NULL);
    if (!tmp_root) return 0;

    snprintf(frame0, sizeof(frame0), "%s/frame_0000.bmp", tmp_root);
    snprintf(frame1, sizeof(frame1), "%s/frame_0001.bmp", tmp_root);
    assert_true("menu_batch_clear_write_frame0", write_text_file(frame0, "a"));
    assert_true("menu_batch_clear_write_frame1", write_text_file(frame1, "b"));

    strncpy(original_frame_dir, animSettings.frameDir, sizeof(original_frame_dir) - 1);
    original_frame_dir[sizeof(original_frame_dir) - 1] = '\0';
    strncpy(original_video_root, animSettings.videoOutputRoot, sizeof(original_video_root) - 1);
    original_video_root[sizeof(original_video_root) - 1] = '\0';

    memset(&state, 0, sizeof(state));
    memset(&layout, 0, sizeof(layout));
    memset(&event, 0, sizeof(event));

    strncpy(animSettings.frameDir, tmp_root, sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    strncpy(animSettings.videoOutputRoot, tmp_root, sizeof(animSettings.videoOutputRoot) - 1);
    animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';
    menu_batch_panel_refresh(&state);

    layout.clearFramesRect = (SDL_Rect){200, 200, 118, 34};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.x = 210;
    event.button.y = 210;
    event.button.clicks = 1;

    assert_true("menu_batch_clear_click_consumed",
                menu_batch_panel_handle_click(&event, NULL, NULL, &state, &layout));
    assert_true("menu_batch_clear_removed_frame0", !path_exists(frame0));
    assert_true("menu_batch_clear_removed_frame1", !path_exists(frame1));
    assert_true("menu_batch_clear_count_zero", state.exportBatchStatus.frame_count == 0u);
    assert_true("menu_batch_clear_status_label",
                strcmp(state.statusLabel, "Cleared 2 frames") == 0);

    strncpy(animSettings.frameDir, original_frame_dir, sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    strncpy(animSettings.videoOutputRoot, original_video_root, sizeof(animSettings.videoOutputRoot) - 1);
    animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';
    rmdir(tmp_root);
    return 0;
}

static int test_menu_layout_builds_non_overlapping_primary_zones(void) {
    MenuRuntimeState state;
    MenuScreenLayout screen;
    AnimationConfig saved_anim = animSettings;

    memset(&state, 0, sizeof(state));
    memset(&screen, 0, sizeof(screen));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.integratorMode = 2;
    animSettings.spaceMode = SPACE_MODE_3D;

    menu_layout_build_base(NULL, &state, &screen);

    assert_true("menu_layout_left_panel_has_width", screen.leftPanelRect.w >= 350);
    assert_true("menu_layout_slider_panel_has_width", screen.sliderPanelRect.w >= 300);
    assert_true("menu_layout_center_panel_has_width", screen.centerBatchRect.w >= 300);
    assert_true("menu_layout_left_before_center",
                screen.leftPanelRect.x + screen.leftPanelRect.w < screen.centerBatchRect.x);
    assert_true("menu_layout_center_before_slider",
                screen.centerBatchRect.x + screen.centerBatchRect.w < screen.sliderPanelRect.x);
    assert_true("menu_layout_center_above_footer",
                screen.centerBatchRect.y + screen.centerBatchRect.h <= screen.bottomActionRowRect.y);
    animSettings = saved_anim;
    return 0;
}

static int test_menu_layout_keeps_manifest_dropdown_inside_left_panel(void) {
    MenuRuntimeState state;
    MenuButtonLayout buttons;
    MenuScreenLayout screen;
    AnimationConfig saved_anim = animSettings;

    memset(&state, 0, sizeof(state));
    memset(&buttons, 0, sizeof(buttons));
    memset(&screen, 0, sizeof(screen));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.integratorMode = 2;
    animSettings.spaceMode = SPACE_MODE_3D;

    state.manifestDropdownOpen = true;
    menu_layout_build_base(NULL, &state, &screen);
    menu_render_build_button_layout(NULL, &state, &screen, &buttons);

    menu_layout_finalize_with_buttons(&screen, &buttons, &state);

    assert_true("menu_layout_manifest_overlay_visible", screen.manifestReserveRect.w > 0 && screen.manifestReserveRect.h > 0);
    assert_true("menu_layout_manifest_overlay_below_input_row",
                screen.manifestReserveRect.y >= buttons.inputRootValueRect.y + buttons.inputRootValueRect.h);
    assert_true("menu_layout_manifest_overlay_inside_left_panel_left",
                screen.manifestReserveRect.x >= screen.leftPanelRect.x);
    assert_true("menu_layout_manifest_overlay_inside_left_panel_right",
                test_rect_right(&screen.manifestReserveRect) <= test_rect_right(&screen.leftPanelRect));
    assert_true("menu_layout_manifest_overlay_inside_left_panel_bottom",
                test_rect_bottom(&screen.manifestReserveRect) <= test_rect_bottom(&screen.leftPanelRect));
    assert_true("menu_layout_batch_y_unchanged_when_manifest_opens",
                screen.centerBatchRect.y == screen.centerControlsRect.y + screen.centerControlsRect.h + 18);
    assert_true("menu_layout_batch_above_footer",
                screen.centerBatchRect.y + screen.centerBatchRect.h <= screen.bottomActionRowRect.y);
    animSettings = saved_anim;
    return 0;
}

static int test_menu_button_layout_respects_owned_screen_zones(void) {
    MenuRuntimeState state;
    MenuScreenLayout screen;
    MenuButtonLayout buttons;
    AnimationConfig saved_anim = animSettings;

    memset(&state, 0, sizeof(state));
    memset(&screen, 0, sizeof(screen));
    memset(&buttons, 0, sizeof(buttons));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.integratorMode = 2;
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.editorMode = 0;

    menu_layout_build_base(NULL, &state, &screen);
    menu_render_build_button_layout(NULL, &state, &screen, &buttons);
    menu_layout_finalize_with_buttons(&screen, &buttons, &state);

    assert_true("menu_buttons_left_controls_inside_left_panel",
                buttons.loadSceneRect.x >= screen.leftPanelRect.x &&
                test_rect_right(&buttons.inputRootApplyRect) <= test_rect_right(&screen.leftPanelRect));
    assert_true("menu_buttons_center_controls_inside_center_zone",
                buttons.falloffRect.x >= screen.centerControlsRect.x &&
                test_rect_right(&buttons.tilePreviewRect) <= test_rect_right(&screen.centerControlsRect));
    assert_true("menu_buttons_route_stack_inside_route_zone",
                buttons.spaceModeRect.x >= screen.routeStackRect.x &&
                test_rect_right(&buttons.previewRect) <= test_rect_right(&screen.routeStackRect) &&
                buttons.spaceModeRect.y >= screen.routeStackRect.y &&
                test_rect_bottom(&buttons.previewRect) <= test_rect_bottom(&screen.routeStackRect));
    assert_true("menu_buttons_footer_inside_bottom_row",
                buttons.exitRect.y >= screen.bottomActionRowRect.y &&
                test_rect_bottom(&buttons.exitRect) <= test_rect_bottom(&screen.bottomActionRowRect) &&
                test_rect_right(&buttons.saveRect) <= test_rect_right(&screen.bottomActionRowRect));
    animSettings = saved_anim;
    return 0;
}

static int test_menu_batch_panel_layout_centers_inside_batch_zone(void) {
    MenuRuntimeState state;
    MenuScreenLayout screen;
    MenuButtonLayout buttons;
    MenuBatchPanelLayout batch;
    AnimationConfig saved_anim = animSettings;

    memset(&state, 0, sizeof(state));
    memset(&screen, 0, sizeof(screen));
    memset(&buttons, 0, sizeof(buttons));
    memset(&batch, 0, sizeof(batch));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.integratorMode = 2;
    animSettings.spaceMode = SPACE_MODE_3D;

    menu_layout_build_base(NULL, &state, &screen);
    menu_render_build_button_layout(NULL, &state, &screen, &buttons);
    menu_layout_finalize_with_buttons(&screen, &buttons, &state);
    menu_batch_panel_build_layout(NULL, &state, &screen, &batch);

    assert_true("menu_batch_panel_inside_center_zone_left",
                batch.panelRect.x >= screen.centerBatchRect.x);
    assert_true("menu_batch_panel_inside_center_zone_right",
                test_rect_right(&batch.panelRect) <= test_rect_right(&screen.centerBatchRect));
    assert_true("menu_batch_panel_inside_center_zone_vertical",
                batch.panelRect.y >= screen.centerBatchRect.y &&
                test_rect_bottom(&batch.panelRect) <= test_rect_bottom(&screen.centerBatchRect));
    assert_true("menu_batch_panel_centered_horizontally",
                abs((batch.panelRect.x - screen.centerBatchRect.x) -
                    (test_rect_right(&screen.centerBatchRect) - test_rect_right(&batch.panelRect))) <= 2);

    animSettings = saved_anim;
    return 0;
}

static int test_menu_batch_panel_header_does_not_overlap_route_rows(void) {
    MenuRuntimeState state;
    MenuScreenLayout screen;
    MenuButtonLayout buttons;
    MenuBatchPanelLayout batch;
    AnimationConfig saved_anim = animSettings;

    memset(&state, 0, sizeof(state));
    memset(&screen, 0, sizeof(screen));
    memset(&buttons, 0, sizeof(buttons));
    memset(&batch, 0, sizeof(batch));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.integratorMode = 2;
    animSettings.spaceMode = SPACE_MODE_3D;

    menu_layout_build_base(NULL, &state, &screen);
    menu_render_build_button_layout(NULL, &state, &screen, &buttons);
    menu_layout_finalize_with_buttons(&screen, &buttons, &state);
    menu_batch_panel_build_layout(NULL, &state, &screen, &batch);

    assert_true("menu_batch_header_label_inside_panel",
                batch.videoFileLabelRect.y >= batch.panelRect.y + MENU_PANEL_CHROME_TITLE_BAND);
    assert_true("menu_batch_header_divider_below_label",
                batch.headerDividerRect.y > batch.videoFileLabelRect.y);
    assert_true("menu_batch_frame_row_below_header_divider",
                batch.frameDirValueRect.y >= batch.headerDividerRect.y + batch.headerDividerRect.h + 6);
    assert_true("menu_batch_video_row_below_frame_row",
                batch.videoRootValueRect.y >= batch.frameDirValueRect.y + batch.frameDirValueRect.h + 8);

    animSettings = saved_anim;
    return 0;
}

static int test_integrator_catalog_menu_routes_by_space_mode(void) {
    AnimationConfig saved_anim = animSettings;
    RayTracingIntegratorMenuState menu_state;
    MenuButtonLayout buttons;
    MenuRuntimeState state;
    MenuScreenLayout screen;

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_HYBRID;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DISNEY;
    animSettings.spaceMode = SPACE_MODE_3D;

    menu_state = RayTracingIntegratorCatalog_BuildMenuState(&animSettings);
    assert_true("integrator_menu_3d_uses_3d_catalog", menu_state.uses3DCatalog);
    assert_true("integrator_menu_3d_label",
                strcmp(menu_state.buttonLabel, "Integrator: 3D Direct Light") == 0);
    assert_true("integrator_menu_3d_no_path_toggles", !menu_state.showPathToggles);
    assert_true("integrator_menu_3d_visible_count_four", menu_state.visibleCount == 4);

    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE;
    menu_state = RayTracingIntegratorCatalog_BuildMenuState(&animSettings);
    assert_true("integrator_menu_3d_diffuse_label",
                strcmp(menu_state.buttonLabel, "Integrator: 3D Diffuse Bounce") == 0);

    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_MATERIAL;
    menu_state = RayTracingIntegratorCatalog_BuildMenuState(&animSettings);
    assert_true("integrator_menu_3d_material_label",
                strcmp(menu_state.buttonLabel, "Integrator: 3D Material") == 0);

    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY;
    menu_state = RayTracingIntegratorCatalog_BuildMenuState(&animSettings);
    assert_true("integrator_menu_3d_emission_transparency_label",
                strcmp(menu_state.buttonLabel,
                       "Integrator: 3D Emission / Transparency") == 0);

    memset(&buttons, 0, sizeof(buttons));
    memset(&state, 0, sizeof(state));
    memset(&screen, 0, sizeof(screen));
    menu_layout_build_base(NULL, &state, &screen);
    menu_render_build_button_layout(NULL, &state, &screen, &buttons);
    assert_true("integrator_menu_3d_layout_no_path_toggles", !buttons.showPathToggles);

    animSettings.spaceMode = SPACE_MODE_2D;
    menu_state = RayTracingIntegratorCatalog_BuildMenuState(&animSettings);
    assert_true("integrator_menu_2d_uses_legacy_catalog", !menu_state.uses3DCatalog);
    assert_true("integrator_menu_2d_label",
                strcmp(menu_state.buttonLabel, "Integrator: Hybrid") == 0);
    assert_true("integrator_menu_2d_show_path_toggles", menu_state.showPathToggles);

    animSettings = saved_anim;
    return 0;
}

static int test_integrator_catalog_cycle_preserves_inactive_mode(void) {
    AnimationConfig saved_anim = animSettings;

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_HYBRID;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DISNEY;
    animSettings.spaceMode = SPACE_MODE_3D;

    RayTracingIntegratorCatalog_CycleActiveSelection(&animSettings);
    assert_true("integrator_cycle_3d_keeps_2d",
                animSettings.integratorMode == RAY_TRACING_2D_INTEGRATOR_HYBRID);
    assert_true("integrator_cycle_3d_advanced_to_diffuse_bounce",
                animSettings.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE);

    RayTracingIntegratorCatalog_CycleActiveSelection(&animSettings);
    assert_true("integrator_cycle_3d_advanced_to_material",
                animSettings.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_MATERIAL);

    RayTracingIntegratorCatalog_CycleActiveSelection(&animSettings);
    assert_true("integrator_cycle_3d_advanced_to_emission_transparency",
                animSettings.integratorMode3D ==
                    RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY);

    RayTracingIntegratorCatalog_CycleActiveSelection(&animSettings);
    assert_true("integrator_cycle_3d_wraps_to_direct_light",
                animSettings.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT);

    animSettings.spaceMode = SPACE_MODE_2D;
    RayTracingIntegratorCatalog_CycleActiveSelection(&animSettings);
    assert_true("integrator_cycle_2d_advanced_to_direct",
                animSettings.integratorMode == RAY_TRACING_2D_INTEGRATOR_DIRECT_LIGHT);
    assert_true("integrator_cycle_2d_keeps_3d",
                animSettings.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT);

    animSettings = saved_anim;
    return 0;
}

static int test_menu_fit_text_to_width_supports_in_place_buffer(void) {
    char text[128];

    snprintf(text, sizeof(text), "Render Frames Root: /tmp/example/very/long/path/value");
    menu_render_fit_text_to_width(NULL, text, 0, text, sizeof(text));

    assert_true("menu_fit_text_in_place_nonempty", text[0] != '\0');
    assert_true("menu_fit_text_in_place_prefix",
                strstr(text, "Render Frames Root") == text);
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
    bool ok = runtime_scene_bridge_preflight_file("third_party/codework_shared/assets/scenes/trio_contract/scene_runtime_min.json",
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
    bool ok = runtime_scene_bridge_preflight_file("third_party/codework_shared/assets/scenes/trio_contract/scene_authoring_min.json",
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
    RuntimeSceneBridge3DPrimitiveSeedState seeds;
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

    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);
    assert_true("runtime_scene_apply_3d_primitives_seeds_valid", seeds.valid);
    assert_true("runtime_scene_apply_3d_primitives_seeds_plane_count", seeds.plane_primitive_count == 1);
    assert_true("runtime_scene_apply_3d_primitives_seeds_rect_prism_count",
                seeds.rect_prism_primitive_count == 0);
    assert_true("runtime_scene_apply_3d_primitives_seeds_retained_count", seeds.primitive_count == 1);
    assert_true("runtime_scene_apply_3d_primitives_seeds_excluded_count",
                seeds.excluded_primitive_count == 2);
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

static int test_runtime_scene_bridge_apply_ps4d_fixture_retains_primitive_seed_truth(void) {
    RuntimeSceneBridgePreflight summary;
    RuntimeSceneBridge3DPrimitiveSeedState seeds;
    const RuntimeSceneBridgePrimitiveSeed *plane_seed = NULL;
    const RuntimeSceneBridgePrimitiveSeed *center_seed = NULL;
    const RuntimeSceneBridgePrimitiveSeed *offset_seed = NULL;
    bool ok = runtime_scene_bridge_apply_file("../physics_sim/config/samples/ps4d_runtime_scene_visual_test.json",
                                              &summary);
    assert_true("runtime_scene_ps4d_seed_apply_ok", ok);
    if (!ok) return 0;

    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);
    assert_true("runtime_scene_ps4d_seed_valid", seeds.valid);
    assert_true("runtime_scene_ps4d_seed_retained_count", seeds.primitive_count == 3);
    assert_true("runtime_scene_ps4d_seed_plane_count", seeds.plane_primitive_count == 1);
    assert_true("runtime_scene_ps4d_seed_rect_prism_count", seeds.rect_prism_primitive_count == 2);
    assert_true("runtime_scene_ps4d_seed_excluded_count", seeds.excluded_primitive_count == 0);

    plane_seed = find_primitive_seed_by_object_id(&seeds, "plane_floor");
    center_seed = find_primitive_seed_by_object_id(&seeds, "prism_center");
    offset_seed = find_primitive_seed_by_object_id(&seeds, "prism_offset");

    assert_true("runtime_scene_ps4d_seed_plane_present", plane_seed != NULL);
    assert_true("runtime_scene_ps4d_seed_center_present", center_seed != NULL);
    assert_true("runtime_scene_ps4d_seed_offset_present", offset_seed != NULL);
    if (!plane_seed || !center_seed || !offset_seed) return 0;

    assert_true("runtime_scene_ps4d_seed_plane_kind",
                plane_seed->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE);
    assert_true("runtime_scene_ps4d_seed_center_kind",
                center_seed->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM);
    assert_true("runtime_scene_ps4d_seed_offset_kind",
                offset_seed->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM);
    assert_true("runtime_scene_ps4d_seed_plane_dimensions", plane_seed->has_dimensions);
    assert_true("runtime_scene_ps4d_seed_center_dimensions", center_seed->has_dimensions);
    assert_true("runtime_scene_ps4d_seed_offset_dimensions", offset_seed->has_dimensions);

    assert_close("runtime_scene_ps4d_seed_plane_origin_z", plane_seed->origin_z, -1.0, 1e-9);
    assert_close("runtime_scene_ps4d_seed_plane_width", plane_seed->width, 8.0, 1e-9);
    assert_close("runtime_scene_ps4d_seed_plane_height", plane_seed->height, 6.0, 1e-9);
    assert_close("runtime_scene_ps4d_seed_plane_axis_u_x", plane_seed->axis_u_x, 1.0, 1e-9);
    assert_close("runtime_scene_ps4d_seed_plane_axis_v_y", plane_seed->axis_v_y, 1.0, 1e-9);
    assert_close("runtime_scene_ps4d_seed_plane_normal_z", plane_seed->normal_z, 1.0, 1e-9);

    assert_close("runtime_scene_ps4d_seed_center_origin_z", center_seed->origin_z, 0.25, 1e-9);
    assert_close("runtime_scene_ps4d_seed_center_width", center_seed->width, 2.0, 1e-9);
    assert_close("runtime_scene_ps4d_seed_center_height", center_seed->height, 1.5, 1e-9);
    assert_close("runtime_scene_ps4d_seed_center_depth", center_seed->depth, 2.5, 1e-9);

    assert_close("runtime_scene_ps4d_seed_offset_origin_x", offset_seed->origin_x, 2.5, 1e-9);
    assert_close("runtime_scene_ps4d_seed_offset_origin_y", offset_seed->origin_y, 1.5, 1e-9);
    assert_close("runtime_scene_ps4d_seed_offset_width", offset_seed->width, 1.25, 1e-9);
    assert_close("runtime_scene_ps4d_seed_offset_height", offset_seed->height, 1.25, 1e-9);
    assert_close("runtime_scene_ps4d_seed_offset_depth", offset_seed->depth, 1.5, 1e-9);
    return 0;
}

static int test_runtime_scene_3d_builder_uses_retained_seed_scope(void) {
    const char *runtime_json_primitives =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_3d_builder_scope\","
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
            "\"primitive\":{\"kind\":\"plane\",\"width\":6.0,\"height\":4.0,"
            "\"frame\":{\"origin\":{\"x\":5.0,\"y\":0.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"axis_v\":{\"x\":0.0,\"y\":1.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":0.0,\"z\":1.0}}},"
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
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary;
    RuntimeScene3D scene;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    ok = runtime_scene_bridge_apply_json(runtime_json_primitives, &summary);
    assert_true("runtime_scene_3d_builder_scope_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeeds(&scene);
    assert_true("runtime_scene_3d_builder_scope_build_ok", ok);
    assert_true("runtime_scene_3d_builder_scope_primitive_count", scene.primitiveCount == 1);
    assert_true("runtime_scene_3d_builder_scope_triangle_count", scene.triangleMesh.triangleCount == 2);
    assert_true("runtime_scene_3d_builder_scope_object_id",
                strcmp(scene.primitives[0].source.objectId, "obj_plane") == 0);
    assert_true("runtime_scene_3d_builder_scope_plane_kind",
                scene.primitives[0].kind == RUNTIME_PRIMITIVE_3D_KIND_PLANE);
    assert_close("runtime_scene_3d_builder_scope_plane_width",
                 scene.primitives[0].shape.plane.width,
                 6.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_scope_plane_height",
                 scene.primitives[0].shape.plane.height,
                 4.0,
                 1e-9);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_scene_3d_builder_builds_ps4d_triangle_scene(void) {
    RuntimeSceneBridgePreflight summary;
    RuntimeScene3D scene;
    int plane_index = -1;
    int center_index = -1;
    int offset_index = -1;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    ok = runtime_scene_bridge_apply_file("../physics_sim/config/samples/ps4d_runtime_scene_visual_test.json",
                                         &summary);
    assert_true("runtime_scene_3d_builder_ps4d_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeeds(&scene);
    assert_true("runtime_scene_3d_builder_ps4d_build_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    assert_true("runtime_scene_3d_builder_ps4d_primitive_count", scene.primitiveCount == 3);
    assert_true("runtime_scene_3d_builder_ps4d_triangle_count", scene.triangleMesh.triangleCount == 26);

    plane_index = find_runtime_primitive_index_by_object_id(&scene, "plane_floor");
    center_index = find_runtime_primitive_index_by_object_id(&scene, "prism_center");
    offset_index = find_runtime_primitive_index_by_object_id(&scene, "prism_offset");
    assert_true("runtime_scene_3d_builder_ps4d_plane_index", plane_index == 0);
    assert_true("runtime_scene_3d_builder_ps4d_center_index", center_index == 1);
    assert_true("runtime_scene_3d_builder_ps4d_offset_index", offset_index == 2);
    assert_true("runtime_scene_3d_builder_ps4d_plane_triangles",
                count_runtime_triangles_for_primitive(&scene, plane_index) == 2);
    assert_true("runtime_scene_3d_builder_ps4d_center_triangles",
                count_runtime_triangles_for_primitive(&scene, center_index) == 12);
    assert_true("runtime_scene_3d_builder_ps4d_offset_triangles",
                count_runtime_triangles_for_primitive(&scene, offset_index) == 12);

    assert_true("runtime_scene_3d_builder_ps4d_plane_kind",
                scene.primitives[plane_index].kind == RUNTIME_PRIMITIVE_3D_KIND_PLANE);
    assert_true("runtime_scene_3d_builder_ps4d_center_kind",
                scene.primitives[center_index].kind == RUNTIME_PRIMITIVE_3D_KIND_RECT_PRISM);
    assert_true("runtime_scene_3d_builder_ps4d_offset_kind",
                scene.primitives[offset_index].kind == RUNTIME_PRIMITIVE_3D_KIND_RECT_PRISM);
    assert_close("runtime_scene_3d_builder_ps4d_plane_width",
                 scene.primitives[plane_index].shape.plane.width,
                 8.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_ps4d_plane_height",
                 scene.primitives[plane_index].shape.plane.height,
                 6.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_ps4d_center_depth",
                 scene.primitives[center_index].shape.rectPrism.depth,
                 2.5,
                 1e-9);
    assert_close("runtime_scene_3d_builder_ps4d_offset_origin_x",
                 scene.primitives[offset_index].shape.rectPrism.origin.x,
                 2.5,
                 1e-9);

    for (int i = 0; i < scene.triangleMesh.triangleCount; ++i) {
        const RuntimeTriangle3D *triangle = &scene.triangleMesh.triangles[i];
        assert_true("runtime_scene_3d_builder_ps4d_triangle_normal_unitish",
                    fabs(vec3_length(triangle->normal) - 1.0) < 1e-9);
    }
    for (int i = 0; i < 2; ++i) {
        const RuntimeTriangle3D *triangle = &scene.triangleMesh.triangles[i];
        assert_true("runtime_scene_3d_builder_ps4d_plane_triangle_primitive_index",
                    triangle->primitiveIndex == plane_index);
        assert_true("runtime_scene_3d_builder_ps4d_plane_triangle_scene_object_index",
                    triangle->sceneObjectIndex == scene.primitives[plane_index].source.sceneObjectIndex);
        assert_close("runtime_scene_3d_builder_ps4d_plane_triangle_normal_z",
                     triangle->normal.z,
                     1.0,
                     1e-9);
    }

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_scene_3d_builder_promotes_authored_light_camera_samples(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_3d_builder_samples\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":1.0,\"y\":2.0,\"z\":3.0}}],"
        "\"cameras\":[{\"position\":{\"x\":4.0,\"y\":5.0,\"z\":6.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"ray_tracing\":{"
            "\"authoring\":{"
              "\"light_path\":{"
                "\"mode\":\"BEZIER_CUBIC\","
                "\"points\":["
                  "{"
                    "\"x\":0.0,\"y\":0.0,\"rotation\":0.0,\"handleLink\":false,"
                    "\"velocity1\":{\"vx\":2.0,\"vy\":0.0}"
                  "},"
                  "{"
                    "\"x\":10.0,\"y\":0.0,\"rotation\":0.0,\"handleLink\":false,"
                    "\"velocity2\":{\"vx\":-2.0,\"vy\":0.0}"
                  "}"
                "]"
              "},"
              "\"light_path_depth\":{"
                "\"points\":["
                  "{\"z\":1.0,\"velocity1\":{\"vz\":1.5}},"
                  "{\"z\":5.0,\"velocity2\":{\"vz\":-1.5}}"
                "]"
              "},"
              "\"camera_path\":{"
                "\"mode\":\"BEZIER_CUBIC\","
                "\"points\":["
                  "{"
                    "\"x\":2.0,\"y\":1.0,\"rotation\":0.25,\"handleLink\":false,"
                    "\"velocity1\":{\"vx\":1.0,\"vy\":1.5}"
                  "},"
                  "{"
                    "\"x\":8.0,\"y\":5.0,\"rotation\":1.25,\"handleLink\":false,"
                    "\"velocity2\":{\"vx\":-1.0,\"vy\":-1.5}"
                  "}"
                "]"
              "},"
              "\"camera_path_depth\":{"
                "\"points\":["
                  "{\"z\":2.0,\"lookPitch\":0.10,\"velocity1\":{\"vz\":0.75}},"
                  "{\"z\":6.0,\"lookPitch\":0.40,\"velocity2\":{\"vz\":-0.75}}"
                "]"
              "}"
            "}"
          "}"
        "}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeScene3D scene;
    Point expected_light_xy = {0.0, 0.0};
    Point expected_camera_xy = {0.0, 0.0};
    double expected_light_z = 0.0;
    double expected_camera_z = 0.0;
    double expected_camera_yaw = 0.0;
    double expected_camera_pitch = 0.0;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_scene_3d_builder_samples_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.lightIntensity = 42.0;
    animSettings.forwardDecay = 17.5;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;

    expected_light_xy = GetPositionAlongPathNormalized(&sceneSettings.bezierPath, 0.5);
    expected_light_z =
        CameraPath3D_GetPositionZNormalized(&sceneSettings.bezierPath, &sceneSettings.bezierPath3D, 0.5);
    expected_camera_xy = GetPositionAlongPathNormalized(&sceneSettings.cameraPath, 0.5);
    expected_camera_yaw = GetRotationAlongPathNormalized(&sceneSettings.cameraPath, 0.5);
    expected_camera_z =
        CameraPath3D_GetPositionZNormalized(&sceneSettings.cameraPath, &sceneSettings.cameraPath3D, 0.5);
    expected_camera_pitch =
        expected_camera_pitch_for_t(&sceneSettings.cameraPath, &sceneSettings.cameraPath3D, 0.5);

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, 0.5);
    assert_true("runtime_scene_3d_builder_samples_build_ok", ok);
    assert_true("runtime_scene_3d_builder_samples_no_primitives", scene.primitiveCount == 0);
    assert_true("runtime_scene_3d_builder_samples_no_triangles", scene.triangleMesh.triangleCount == 0);
    assert_true("runtime_scene_3d_builder_samples_has_light", scene.hasLight);
    assert_true("runtime_scene_3d_builder_samples_has_camera", scene.hasCamera);
    assert_close("runtime_scene_3d_builder_samples_light_x",
                 scene.light.position.x,
                 expected_light_xy.x,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_light_y",
                 scene.light.position.y,
                 expected_light_xy.y,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_light_z",
                 scene.light.position.z,
                 expected_light_z,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_light_radius",
                 scene.light.radius,
                 10.0,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_light_intensity",
                 scene.light.intensity,
                 42.0,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_light_falloff",
                 scene.light.falloffDistance,
                 17.5,
                 1e-6);
    assert_true("runtime_scene_3d_builder_samples_light_falloff_mode",
                scene.light.falloffMode == FORWARD_FALLOFF_MODE_LINEAR);
    assert_close("runtime_scene_3d_builder_samples_camera_x",
                 scene.camera.position.x,
                 expected_camera_xy.x,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_camera_y",
                 scene.camera.position.y,
                 expected_camera_xy.y,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_camera_z",
                 scene.camera.position.z,
                 expected_camera_z,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_camera_yaw",
                 scene.camera.rotation,
                 expected_camera_yaw,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_camera_pitch",
                 scene.camera.lookPitch,
                 expected_camera_pitch,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_camera_zoom",
                 scene.camera.zoom,
                 1.0,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_camera_near_plane",
                 scene.camera.nearPlane,
                 0.1,
                 1e-6);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_scene_3d_builder_falls_back_to_seeded_camera_state(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_3d_builder_camera_fallback\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":-2.0,\"y\":3.0,\"z\":4.0}}],"
        "\"cameras\":[{\"position\":{\"x\":7.0,\"y\":8.0,\"z\":9.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeScene3D scene;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_scene_3d_builder_camera_fallback_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    sceneSettings.camera.rotation = 0.75;
    sceneSettings.camera.zoom = 1.5;
    animSettings.lightIntensity = 11.0;
    animSettings.forwardDecay = 9.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_NONE;

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, 0.65);
    assert_true("runtime_scene_3d_builder_camera_fallback_build_ok", ok);
    assert_true("runtime_scene_3d_builder_camera_fallback_has_light", scene.hasLight);
    assert_true("runtime_scene_3d_builder_camera_fallback_has_camera", scene.hasCamera);
    assert_close("runtime_scene_3d_builder_camera_fallback_light_x",
                 scene.light.position.x,
                 -2.0,
                 1e-6);
    assert_close("runtime_scene_3d_builder_camera_fallback_light_y",
                 scene.light.position.y,
                 3.0,
                 1e-6);
    assert_close("runtime_scene_3d_builder_camera_fallback_light_z",
                 scene.light.position.z,
                 4.0,
                 1e-6);
    assert_true("runtime_scene_3d_builder_camera_fallback_light_mode",
                scene.light.falloffMode == FORWARD_FALLOFF_MODE_NONE);
    assert_close("runtime_scene_3d_builder_camera_fallback_camera_x",
                 scene.camera.position.x,
                 7.0,
                 1e-6);
    assert_close("runtime_scene_3d_builder_camera_fallback_camera_y",
                 scene.camera.position.y,
                 8.0,
                 1e-6);
    assert_close("runtime_scene_3d_builder_camera_fallback_camera_z",
                 scene.camera.position.z,
                 9.0,
                 1e-6);
    assert_close("runtime_scene_3d_builder_camera_fallback_camera_yaw",
                 scene.camera.rotation,
                 0.75,
                 1e-6);
    assert_close("runtime_scene_3d_builder_camera_fallback_camera_pitch",
                 scene.camera.lookPitch,
                 0.0,
                 1e-6);
    assert_close("runtime_scene_3d_builder_camera_fallback_camera_zoom",
                 scene.camera.zoom,
                 1.5,
                 1e-6);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_ray_3d_triangle_intersection_contract(void) {
    RuntimeTriangle3D triangle = {0};
    Ray3D ray = {0};
    HitInfo3D hit = {0};
    bool ok = false;

    triangle.p0 = vec3(0.0, 0.0, 0.0);
    triangle.p1 = vec3(1.0, 0.0, 0.0);
    triangle.p2 = vec3(0.0, 1.0, 0.0);
    triangle.normal = vec3(0.0, 0.0, 1.0);
    triangle.primitiveIndex = 3;
    triangle.sceneObjectIndex = 7;
    ray = RuntimeRay3D_Make(vec3(0.25, 0.25, 3.0), vec3(0.0, 0.0, -2.0));

    ok = RuntimeRay3D_IntersectTriangle(&ray, &triangle, 11, 0.001, 10.0, &hit);
    assert_true("runtime_ray_3d_triangle_hit_ok", ok);
    assert_close("runtime_ray_3d_triangle_hit_t", hit.t, 3.0, 1e-6);
    assert_close("runtime_ray_3d_triangle_hit_px", hit.position.x, 0.25, 1e-6);
    assert_close("runtime_ray_3d_triangle_hit_py", hit.position.y, 0.25, 1e-6);
    assert_close("runtime_ray_3d_triangle_hit_pz", hit.position.z, 0.0, 1e-6);
    assert_close("runtime_ray_3d_triangle_hit_nz", hit.normal.z, 1.0, 1e-6);
    assert_true("runtime_ray_3d_triangle_hit_triangle_index", hit.triangleIndex == 11);
    assert_true("runtime_ray_3d_triangle_hit_primitive_index", hit.primitiveIndex == 3);
    assert_true("runtime_ray_3d_triangle_hit_scene_object_index", hit.sceneObjectIndex == 7);
    assert_close("runtime_ray_3d_triangle_hit_bary_sum",
                 hit.baryU + hit.baryV + hit.baryW,
                 1.0,
                 1e-6);
    assert_true("runtime_ray_3d_triangle_hit_bary_inside",
                hit.baryU >= 0.0 && hit.baryV >= 0.0 && hit.baryW >= 0.0);
    return 0;
}

static int test_runtime_ray_3d_scene_first_hit_contract(void) {
    RuntimeScene3D scene;
    Ray3D ray = {0};
    HitInfo3D hit = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    scene.primitiveCapacity = 2;
    scene.triangleMesh.triangleCapacity = 2;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_ray_3d_scene_alloc_primitives", scene.primitives != NULL);
    assert_true("runtime_ray_3d_scene_alloc_triangles", scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    scene.primitiveCount = 2;
    scene.triangleMesh.triangleCount = 2;

    snprintf(scene.primitives[0].source.objectId,
             sizeof(scene.primitives[0].source.objectId),
             "%s",
             "rear_plane");
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 41;
    snprintf(scene.primitives[1].source.objectId,
             sizeof(scene.primitives[1].source.objectId),
             "%s",
             "front_plane");
    scene.primitives[1].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[1].source.sceneObjectIndex = 42;

    scene.triangleMesh.triangles[0].p0 = vec3(-1.0, -1.0, 0.0);
    scene.triangleMesh.triangles[0].p1 = vec3(1.0, -1.0, 0.0);
    scene.triangleMesh.triangles[0].p2 = vec3(-1.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 0.0, 1.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 41;

    scene.triangleMesh.triangles[1].p0 = vec3(-1.0, -1.0, 1.0);
    scene.triangleMesh.triangles[1].p1 = vec3(1.0, -1.0, 1.0);
    scene.triangleMesh.triangles[1].p2 = vec3(-1.0, 1.0, 1.0);
    scene.triangleMesh.triangles[1].normal = vec3(0.0, 0.0, 1.0);
    scene.triangleMesh.triangles[1].primitiveIndex = 1;
    scene.triangleMesh.triangles[1].sceneObjectIndex = 42;

    ray = RuntimeRay3D_Make(vec3(0.0, 0.0, 3.0), vec3(0.0, 0.0, -1.0));
    ok = RuntimeRay3D_TraceSceneFirstHit(&scene, &ray, 0.001, 10.0, &hit);
    assert_true("runtime_ray_3d_scene_first_hit_ok", ok);
    assert_close("runtime_ray_3d_scene_first_hit_t", hit.t, 2.0, 1e-6);
    assert_close("runtime_ray_3d_scene_first_hit_pz", hit.position.z, 1.0, 1e-6);
    assert_true("runtime_ray_3d_scene_first_hit_triangle_index", hit.triangleIndex == 1);
    assert_true("runtime_ray_3d_scene_first_hit_primitive_index", hit.primitiveIndex == 1);
    assert_true("runtime_ray_3d_scene_first_hit_scene_object_index", hit.sceneObjectIndex == 42);
    assert_true("runtime_ray_3d_scene_first_hit_object_id",
                strcmp(hit.source.objectId, "front_plane") == 0);
    assert_true("runtime_ray_3d_scene_first_hit_kind",
                hit.source.kind == RUNTIME_PRIMITIVE_3D_KIND_PLANE);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_ray_3d_offset_contract(void) {
    Ray3D forward =
        RuntimeRay3D_MakeOffset(vec3(1.0, 2.0, 3.0), vec3(0.0, 0.0, 1.0), vec3(0.0, 0.0, 4.0), 0.05);
    Ray3D backward =
        RuntimeRay3D_MakeOffset(vec3(1.0, 2.0, 3.0), vec3(0.0, 0.0, 1.0), vec3(0.0, 0.0, -4.0), 0.05);

    assert_close("runtime_ray_3d_offset_forward_origin_z", forward.origin.z, 3.05, 1e-6);
    assert_close("runtime_ray_3d_offset_backward_origin_z", backward.origin.z, 2.95, 1e-6);
    assert_close("runtime_ray_3d_offset_forward_dir_len", vec3_length(forward.direction), 1.0, 1e-6);
    assert_close("runtime_ray_3d_offset_backward_dir_len", vec3_length(backward.direction), 1.0, 1e-6);
    return 0;
}

static int test_runtime_visibility_3d_visible_contract(void) {
    RuntimeScene3D scene;
    HitInfo3D surface_hit = {0};
    RuntimeLight3D light = {0};
    HitInfo3D blocker_hit = {0};
    double light_distance = 0.0;
    bool blocked = false;
    bool visible = false;

    RuntimeScene3D_Init(&scene);
    surface_hit.position = vec3(0.0, 0.0, 0.0);
    surface_hit.normal = vec3(0.0, 0.0, 1.0);
    light.position = vec3(0.0, 0.0, 3.0);

    blocked = RuntimeVisibility3D_TraceToLight(&scene,
                                               surface_hit.position,
                                               surface_hit.normal,
                                               light.position,
                                               &blocker_hit,
                                               &light_distance);
    visible = RuntimeVisibility3D_HasLineOfSightFromHit(&scene, &surface_hit, &light);

    assert_true("runtime_visibility_3d_visible_not_blocked", !blocked);
    assert_true("runtime_visibility_3d_visible_los", visible);
    assert_close("runtime_visibility_3d_visible_distance", light_distance, 3.0, 1e-6);
    assert_true("runtime_visibility_3d_visible_reset_triangle", blocker_hit.triangleIndex == -1);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_visibility_3d_blocked_contract(void) {
    RuntimeScene3D scene;
    HitInfo3D surface_hit = {0};
    RuntimeLight3D light = {0};
    HitInfo3D blocker_hit = {0};
    double light_distance = 0.0;
    bool blocked = false;
    bool visible = false;

    RuntimeScene3D_Init(&scene);
    scene.primitiveCapacity = 1;
    scene.triangleMesh.triangleCapacity = 1;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_visibility_3d_blocked_alloc_primitives", scene.primitives != NULL);
    assert_true("runtime_visibility_3d_blocked_alloc_triangles", scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    scene.primitiveCount = 1;
    scene.triangleMesh.triangleCount = 1;
    snprintf(scene.primitives[0].source.objectId,
             sizeof(scene.primitives[0].source.objectId),
             "%s",
             "blocker");
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 88;

    scene.triangleMesh.triangles[0].p0 = vec3(-1.0, -1.0, 1.5);
    scene.triangleMesh.triangles[0].p1 = vec3(1.0, -1.0, 1.5);
    scene.triangleMesh.triangles[0].p2 = vec3(-1.0, 1.0, 1.5);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 0.0, -1.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 88;

    surface_hit.position = vec3(0.0, 0.0, 0.0);
    surface_hit.normal = vec3(0.0, 0.0, 1.0);
    light.position = vec3(0.0, 0.0, 3.0);

    blocked = RuntimeVisibility3D_TraceToLight(&scene,
                                               surface_hit.position,
                                               surface_hit.normal,
                                               light.position,
                                               &blocker_hit,
                                               &light_distance);
    visible = RuntimeVisibility3D_HasLineOfSightFromHit(&scene, &surface_hit, &light);

    assert_true("runtime_visibility_3d_blocked_blocked", blocked);
    assert_true("runtime_visibility_3d_blocked_not_visible", !visible);
    assert_close("runtime_visibility_3d_blocked_distance", light_distance, 3.0, 1e-6);
    assert_true("runtime_visibility_3d_blocked_triangle_index", blocker_hit.triangleIndex == 0);
    assert_true("runtime_visibility_3d_blocked_primitive_index", blocker_hit.primitiveIndex == 0);
    assert_true("runtime_visibility_3d_blocked_scene_object_index", blocker_hit.sceneObjectIndex == 88);
    assert_true("runtime_visibility_3d_blocked_object_id",
                strcmp(blocker_hit.source.objectId, "blocker") == 0);
    assert_close("runtime_visibility_3d_blocked_hit_z", blocker_hit.position.z, 1.5, 1e-6);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_camera_projector_3d_center_ray_contract(void) {
    RuntimeCamera3D camera = {0};
    RuntimeCameraProjector3D projector = {0};
    Ray3D ray = {0};
    bool ok = false;

    camera.position = vec3(1.0, 2.0, 3.0);
    camera.rotation = 0.0;
    camera.lookPitch = 0.0;
    camera.zoom = 1.0;
    camera.nearPlane = 0.1;

    ok = RuntimeCameraProjector3D_Build(&camera, 201, 101, &projector);
    assert_true("runtime_camera_projector_3d_build_ok", ok);
    if (!ok) return 0;

    ray = RuntimeCameraProjector3D_MakePrimaryRay(&projector, 100.0, 50.0);
    assert_close("runtime_camera_projector_3d_center_origin_x", ray.origin.x, 1.0, 1e-6);
    assert_close("runtime_camera_projector_3d_center_origin_y", ray.origin.y, 2.0, 1e-6);
    assert_close("runtime_camera_projector_3d_center_origin_z", ray.origin.z, 3.0, 1e-6);
    assert_close("runtime_camera_projector_3d_center_dir_x", ray.direction.x, 0.0, 1e-6);
    assert_close("runtime_camera_projector_3d_center_dir_y", ray.direction.y, -1.0, 1e-6);
    assert_close("runtime_camera_projector_3d_center_dir_z", ray.direction.z, 0.0, 1e-6);

    ray = RuntimeCameraProjector3D_MakePrimaryRay(&projector, 200.0, 50.0);
    assert_true("runtime_camera_projector_3d_right_ray_x_positive", ray.direction.x > 0.0);
    assert_true("runtime_camera_projector_3d_right_ray_y_negative", ray.direction.y < 0.0);
    return 0;
}

static int test_runtime_camera_projector_3d_pitch_contract(void) {
    RuntimeCamera3D camera = {0};
    RuntimeCameraProjector3D projector = {0};
    Ray3D ray = {0};
    bool ok = false;

    camera.position = vec3(0.0, 0.0, 0.0);
    camera.rotation = 0.0;
    camera.lookPitch = M_PI / 4.0;
    camera.zoom = 1.0;
    camera.nearPlane = 0.1;

    ok = RuntimeCameraProjector3D_Build(&camera, 101, 101, &projector);
    assert_true("runtime_camera_projector_3d_pitch_build_ok", ok);
    if (!ok) return 0;

    ray = RuntimeCameraProjector3D_MakePrimaryRay(&projector, 50.0, 50.0);
    assert_true("runtime_camera_projector_3d_pitch_z_positive", ray.direction.z > 0.0);
    assert_true("runtime_camera_projector_3d_pitch_y_negative", ray.direction.y < 0.0);
    assert_close("runtime_camera_projector_3d_pitch_dir_len",
                 vec3_length(ray.direction),
                 1.0,
                 1e-6);
    return 0;
}

static int test_runtime_camera_projector_3d_zoom_contract(void) {
    RuntimeCamera3D base_camera = {0};
    RuntimeCameraProjector3D wide_projector = {0};
    RuntimeCameraProjector3D zoomed_projector = {0};
    Ray3D wide_ray = {0};
    Ray3D zoomed_ray = {0};
    bool ok_wide = false;
    bool ok_zoomed = false;

    base_camera.position = vec3(0.0, 0.0, 0.0);
    base_camera.rotation = 0.0;
    base_camera.lookPitch = 0.0;
    base_camera.nearPlane = 0.1;

    base_camera.zoom = 1.0;
    ok_wide = RuntimeCameraProjector3D_Build(&base_camera, 201, 101, &wide_projector);
    base_camera.zoom = 2.0;
    ok_zoomed = RuntimeCameraProjector3D_Build(&base_camera, 201, 101, &zoomed_projector);
    assert_true("runtime_camera_projector_3d_zoom_build_wide_ok", ok_wide);
    assert_true("runtime_camera_projector_3d_zoom_build_zoomed_ok", ok_zoomed);
    if (!ok_wide || !ok_zoomed) return 0;

    wide_ray = RuntimeCameraProjector3D_MakePrimaryRay(&wide_projector, 200.0, 50.0);
    zoomed_ray = RuntimeCameraProjector3D_MakePrimaryRay(&zoomed_projector, 200.0, 50.0);
    assert_true("runtime_camera_projector_3d_zoom_narrows_horizontal_spread",
                fabs(zoomed_ray.direction.x) < fabs(wide_ray.direction.x));
    return 0;
}

static int test_runtime_camera_projector_3d_preview_projection_parity(void) {
    RuntimeCamera3D runtime_camera = {0};
    RuntimeCameraProjector3D runtime_projector = {0};
    PreviewCameraSample preview_sample = {0};
    PreviewCameraProjector preview_projector = {0};
    SDL_Rect viewport = {0, 0, 1000, 500};
    Vec3 world_point = vec3(0.0, -10.0, 0.0);
    double runtime_sx = 0.0;
    double runtime_sy = 0.0;
    double runtime_depth = 0.0;
    bool runtime_inside = false;
    double preview_sx = 0.0;
    double preview_sy = 0.0;
    double preview_depth = 0.0;
    bool preview_inside = false;
    bool ok = false;

    runtime_camera.position = vec3(0.0, 0.0, 0.0);
    runtime_camera.rotation = 0.0;
    runtime_camera.lookPitch = 0.0;
    runtime_camera.zoom = 1.0;
    runtime_camera.nearPlane = 0.1;

    preview_sample.valid = true;
    preview_sample.position_x = 0.0;
    preview_sample.position_y = 0.0;
    preview_sample.position_z = 0.0;
    preview_sample.yaw_radians = 0.0;
    preview_sample.pitch_radians = 0.0;
    preview_sample.fov_y_degrees = 55.0;
    preview_sample.aspect_ratio = 2.0;

    ok = RuntimeCameraProjector3D_Build(&runtime_camera, viewport.w, viewport.h, &runtime_projector);
    assert_true("runtime_camera_projector_3d_preview_parity_build_runtime", ok);
    assert_true("runtime_camera_projector_3d_preview_parity_build_preview",
                PreviewCameraProjectorBuild(&preview_sample, viewport, &preview_projector));
    if (!ok || !PreviewCameraProjectorBuild(&preview_sample, viewport, &preview_projector)) {
        return 0;
    }

    assert_true("runtime_camera_projector_3d_preview_parity_center",
                RuntimeCameraProjector3D_ProjectPoint(&runtime_projector,
                                                     world_point,
                                                     &runtime_sx,
                                                     &runtime_sy,
                                                     &runtime_depth,
                                                     &runtime_inside));
    assert_true("runtime_camera_projector_3d_preview_parity_center_preview",
                PreviewCameraProjectorProjectPoint(&preview_projector,
                                                   world_point.x,
                                                   world_point.y,
                                                   world_point.z,
                                                   &preview_sx,
                                                   &preview_sy,
                                                   &preview_depth,
                                                   &preview_inside));
    assert_close("runtime_camera_projector_3d_preview_parity_center_x",
                 runtime_sx,
                 preview_sx,
                 1e-6);
    assert_close("runtime_camera_projector_3d_preview_parity_center_y",
                 runtime_sy,
                 preview_sy,
                 1e-6);
    assert_close("runtime_camera_projector_3d_preview_parity_center_depth",
                 runtime_depth,
                 preview_depth,
                 1e-6);
    assert_true("runtime_camera_projector_3d_preview_parity_center_inside",
                runtime_inside == preview_inside);

    world_point = vec3(10.0, -10.0, 0.0);
    assert_true("runtime_camera_projector_3d_preview_parity_right",
                RuntimeCameraProjector3D_ProjectPoint(&runtime_projector,
                                                     world_point,
                                                     &runtime_sx,
                                                     &runtime_sy,
                                                     &runtime_depth,
                                                     &runtime_inside));
    assert_true("runtime_camera_projector_3d_preview_parity_right_preview",
                PreviewCameraProjectorProjectPoint(&preview_projector,
                                                   world_point.x,
                                                   world_point.y,
                                                   world_point.z,
                                                   &preview_sx,
                                                   &preview_sy,
                                                   &preview_depth,
                                                   &preview_inside));
    assert_close("runtime_camera_projector_3d_preview_parity_right_x",
                 runtime_sx,
                 preview_sx,
                 1e-6);
    assert_close("runtime_camera_projector_3d_preview_parity_right_y",
                 runtime_sy,
                 preview_sy,
                 1e-6);

    world_point = vec3(0.0, -10.0, 5.0);
    assert_true("runtime_camera_projector_3d_preview_parity_top",
                RuntimeCameraProjector3D_ProjectPoint(&runtime_projector,
                                                     world_point,
                                                     &runtime_sx,
                                                     &runtime_sy,
                                                     &runtime_depth,
                                                     &runtime_inside));
    assert_true("runtime_camera_projector_3d_preview_parity_top_preview",
                PreviewCameraProjectorProjectPoint(&preview_projector,
                                                   world_point.x,
                                                   world_point.y,
                                                   world_point.z,
                                                   &preview_sx,
                                                   &preview_sy,
                                                   &preview_depth,
                                                   &preview_inside));
    assert_close("runtime_camera_projector_3d_preview_parity_top_x",
                 runtime_sx,
                 preview_sx,
                 1e-6);
    assert_close("runtime_camera_projector_3d_preview_parity_top_y",
                 runtime_sy,
                 preview_sy,
                 1e-6);
    return 0;
}

static int test_runtime_material_payload_3d_scene_object_resolution_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialPayload3D payload = {0};
    MaterialBSDF expected = {0};
    SceneObject expected_object;
    int default_material_id = 0;
    bool ok = false;

    MaterialManagerResetDefaults();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 2;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 10.0, 0.0, NULL, 0);
    InitObject(&sceneSettings.sceneObjects[1], OBJECT_CIRCLE, 5.0, -2.0, 6.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[1].color = 0x804020;
    sceneSettings.sceneObjects[1].opacity = 0.75;
    sceneSettings.sceneObjects[1].reflectivity = 0.35;
    sceneSettings.sceneObjects[1].roughness = 0.15;
    sceneSettings.sceneObjects[1].material_id = 999;

    default_material_id = MaterialManagerDefaultId();
    expected_object = sceneSettings.sceneObjects[1];
    expected_object.material_id = default_material_id;
    MaterialBSDFInitFromSceneObject(&expected_object, &expected);

    ok = RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(1, &payload);
    assert_true("runtime_material_payload_3d_scene_object_ok", ok);
    assert_true("runtime_material_payload_3d_scene_object_valid", payload.valid);
    assert_true("runtime_material_payload_3d_scene_object_index_match",
                payload.sceneObjectIndex == 1);
    assert_true("runtime_material_payload_3d_scene_object_material_clamped",
                payload.materialId == default_material_id);
    assert_close("runtime_material_payload_3d_scene_object_albedo_match",
                 payload.bsdf.albedo,
                 expected.albedo,
                 1e-9);
    assert_close("runtime_material_payload_3d_scene_object_opacity_match",
                 payload.bsdf.opacity,
                 expected.opacity,
                 1e-9);
    assert_close("runtime_material_payload_3d_scene_object_reflectivity_match",
                 payload.bsdf.reflectivity,
                 expected.reflectivity,
                 1e-9);
    assert_close("runtime_material_payload_3d_scene_object_roughness_match",
                 payload.bsdf.roughness,
                 expected.roughness,
                 1e-9);
    assert_close("runtime_material_payload_3d_scene_object_diffuse_weight_match",
                 payload.bsdf.diffuseWeight,
                 expected.diffuseWeight,
                 1e-9);
    assert_close("runtime_material_payload_3d_scene_object_spec_weight_match",
                 payload.bsdf.specWeight,
                 expected.specWeight,
                 1e-9);

    sceneSettings = saved_scene;
    return 0;
}

static int test_material_manager_default_presets_include_i4_entries(void) {
    const Material* emissive = NULL;
    const Material* transparent = NULL;

    MaterialManagerResetDefaults();
    assert_true("material_manager_default_preset_count_i4",
                MaterialManagerCount() >= (MATERIAL_PRESET_TRANSPARENT + 1));

    emissive = MaterialManagerGet(MATERIAL_PRESET_EMISSIVE);
    transparent = MaterialManagerGet(MATERIAL_PRESET_TRANSPARENT);

    assert_true("material_manager_emissive_preset_exists", emissive != NULL);
    assert_true("material_manager_transparent_preset_exists", transparent != NULL);
    assert_true("material_manager_emissive_preset_has_emission",
                emissive->emissive.x > 0.0f ||
                emissive->emissive.y > 0.0f ||
                emissive->emissive.z > 0.0f);
    assert_close("material_manager_emissive_preset_transparency_zero",
                 emissive->transparency,
                 0.0,
                 1e-9);
    assert_true("material_manager_transparent_preset_has_transparency",
                transparent->transparency > 0.0f);
    assert_close("material_manager_transparent_preset_emissive_zero",
                 transparent->emissive.x + transparent->emissive.y + transparent->emissive.z,
                 0.0,
                 1e-9);
    return 0;
}

static int test_material_manager_load_dir_preserves_shipped_preset_ids(void) {
    char dir_template[] = "/tmp/rt_material_ids_XXXXXX";
    char mirror_path[512];
    char emissive_path[512];
    char transparent_path[512];
    const Material* mirror = NULL;
    const Material* emissive = NULL;
    const Material* transparent = NULL;
    bool ok = false;

    MaterialManagerResetDefaults();
    if (!mkdtemp(dir_template)) {
        return 0;
    }

    snprintf(mirror_path, sizeof(mirror_path), "%s/mirror.json", dir_template);
    snprintf(emissive_path, sizeof(emissive_path), "%s/emissive.json", dir_template);
    snprintf(transparent_path, sizeof(transparent_path), "%s/transparent.json", dir_template);

    ok = write_text_file(mirror_path,
                         "{"
                         "\"diffuse\":0.0,"
                         "\"specular\":0.1,"
                         "\"reflectivity\":0.77,"
                         "\"roughness\":0.0,"
                         "\"transparency\":0.0,"
                         "\"base_color\":[1.0,1.0,1.0],"
                         "\"emissive\":[0.0,0.0,0.0]"
                         "}");
    assert_true("material_manager_load_dir_mirror_file_ok", ok);
    ok = write_text_file(emissive_path,
                         "{"
                         "\"diffuse\":0.0,"
                         "\"specular\":0.0,"
                         "\"reflectivity\":0.0,"
                         "\"roughness\":1.0,"
                         "\"transparency\":0.0,"
                         "\"base_color\":[1.0,1.0,1.0],"
                         "\"emissive\":[0.25,0.25,0.25]"
                         "}");
    assert_true("material_manager_load_dir_emissive_file_ok", ok);
    ok = write_text_file(transparent_path,
                         "{"
                         "\"diffuse\":0.05,"
                         "\"specular\":0.0,"
                         "\"reflectivity\":0.0,"
                         "\"roughness\":1.0,"
                         "\"transparency\":0.66,"
                         "\"base_color\":[1.0,1.0,1.0],"
                         "\"emissive\":[0.0,0.0,0.0]"
                         "}");
    assert_true("material_manager_load_dir_transparent_file_ok", ok);

    MaterialManagerLoadDir(dir_template);

    mirror = MaterialManagerGet(MATERIAL_PRESET_MIRROR);
    emissive = MaterialManagerGet(MATERIAL_PRESET_EMISSIVE);
    transparent = MaterialManagerGet(MATERIAL_PRESET_TRANSPARENT);

    assert_true("material_manager_load_dir_mirror_preset_exists", mirror != NULL);
    assert_true("material_manager_load_dir_emissive_preset_exists", emissive != NULL);
    assert_true("material_manager_load_dir_transparent_preset_exists", transparent != NULL);
    assert_close("material_manager_load_dir_mirror_keeps_canonical_id",
                 mirror->reflectivity,
                 0.77,
                 1e-6);
    assert_close("material_manager_load_dir_emissive_keeps_canonical_id",
                 emissive->emissive.x,
                 0.25,
                 1e-6);
    assert_close("material_manager_load_dir_transparent_keeps_canonical_id",
                 transparent->transparency,
                 0.66,
                 1e-6);

    remove(mirror_path);
    remove(emissive_path);
    remove(transparent_path);
    rmdir(dir_template);
    MaterialManagerResetDefaults();
    return 0;
}

static int test_runtime_material_payload_3d_hit_resolution_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialPayload3D payload = {0};
    MaterialBSDF expected = {0};
    HitInfo3D hit = {0};
    bool ok = false;

    MaterialManagerResetDefaults();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 8.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].color = 0xC0C0FF;
    sceneSettings.sceneObjects[0].opacity = 1.0;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_GLOSSY;
    sceneSettings.sceneObjects[0].reflectivity = 0.5;
    sceneSettings.sceneObjects[0].roughness = 0.05;
    MaterialBSDFInitFromSceneObject(&sceneSettings.sceneObjects[0], &expected);

    HitInfo3D_Reset(&hit);
    hit.sceneObjectIndex = 0;
    hit.triangleIndex = 3;
    hit.primitiveIndex = 1;

    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &payload);
    assert_true("runtime_material_payload_3d_hit_ok", ok);
    assert_true("runtime_material_payload_3d_hit_valid", payload.valid);
    assert_true("runtime_material_payload_3d_hit_index_match",
                payload.sceneObjectIndex == 0);
    assert_true("runtime_material_payload_3d_hit_material_match",
                payload.materialId == MATERIAL_PRESET_GLOSSY);
    assert_close("runtime_material_payload_3d_hit_albedo_match",
                 payload.bsdf.albedo,
                 expected.albedo,
                 1e-9);
    assert_close("runtime_material_payload_3d_hit_reflectivity_match",
                 payload.bsdf.reflectivity,
                 expected.reflectivity,
                 1e-9);
    assert_close("runtime_material_payload_3d_hit_roughness_match",
                 payload.bsdf.roughness,
                 expected.roughness,
                 1e-9);
    assert_close("runtime_material_payload_3d_hit_emissive_match",
                 payload.emissive,
                 expected.emissive,
                 1e-9);
    assert_close("runtime_material_payload_3d_hit_transparency_match",
                 payload.transparency,
                 0.0,
                 1e-9);
    assert_true("runtime_material_payload_3d_hit_invalid_index_rejected",
                !RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(4, &payload));
    assert_true("runtime_material_payload_3d_hit_invalid_hit_rejected",
                !RuntimeMaterialPayload3D_ResolveFromHit(NULL, &payload));

    sceneSettings = saved_scene;
    return 0;
}

static int test_runtime_direct_light_3d_shade_pixel_visible_contract(void) {
    RuntimeScene3D scene;
    RuntimeCameraProjector3D projector = {0};
    RuntimeDirectLight3DResult result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    scene.hasLight = true;
    scene.light.position = vec3(0.0, -2.0, 0.0);
    scene.light.intensity = 10.0;
    scene.light.falloffDistance = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene.hasCamera = true;
    scene.camera.position = vec3(0.0, 0.0, 0.0);
    scene.camera.rotation = 0.0;
    scene.camera.lookPitch = 0.0;
    scene.camera.zoom = 1.0;
    scene.camera.nearPlane = 0.1;

    scene.primitiveCapacity = 1;
    scene.triangleMesh.triangleCapacity = 1;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_direct_light_3d_visible_alloc_primitives", scene.primitives != NULL);
    assert_true("runtime_direct_light_3d_visible_alloc_triangles", scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    scene.primitiveCount = 1;
    scene.triangleMesh.triangleCount = 1;
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 5;
    snprintf(scene.primitives[0].source.objectId,
             sizeof(scene.primitives[0].source.objectId),
             "%s",
             "lit_wall");
    scene.triangleMesh.triangles[0].p0 = vec3(-3.0, -5.0, -3.0);
    scene.triangleMesh.triangles[0].p1 = vec3(-3.0, -5.0, 3.0);
    scene.triangleMesh.triangles[0].p2 = vec3(3.0, -5.0, -3.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 5;

    ok = RuntimeCameraProjector3D_Build(&scene.camera, 101, 101, &projector);
    assert_true("runtime_direct_light_3d_visible_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    ok = RuntimeDirectLight3D_ShadePixel(&scene, &projector, 50.0, 50.0, &result);
    assert_true("runtime_direct_light_3d_visible_shade_ok", ok);
    assert_true("runtime_direct_light_3d_visible_hit", result.hit);
    assert_true("runtime_direct_light_3d_visible_los", result.visible);
    assert_close("runtime_direct_light_3d_visible_hit_y", result.hitInfo.position.y, -5.0, 1e-6);
    assert_true("runtime_direct_light_3d_visible_ndotl_positive", result.ndotl > 0.99);
    assert_true("runtime_direct_light_3d_visible_radiance_positive", result.radiance > 0.0);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_direct_light_3d_shade_pixel_shadowed_contract(void) {
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimeDirectLight3DResult result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    scene.hasLight = true;
    scene.light.position = vec3(2.0, -2.0, 0.0);
    scene.light.intensity = 10.0;
    scene.light.falloffDistance = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene.primitiveCapacity = 2;
    scene.triangleMesh.triangleCapacity = 2;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_direct_light_3d_shadowed_alloc_primitives", scene.primitives != NULL);
    assert_true("runtime_direct_light_3d_shadowed_alloc_triangles", scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    scene.primitiveCount = 2;
    scene.triangleMesh.triangleCount = 2;
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 5;
    snprintf(scene.primitives[0].source.objectId,
             sizeof(scene.primitives[0].source.objectId),
             "%s",
             "lit_wall");
    scene.primitives[1].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[1].source.sceneObjectIndex = 6;
    snprintf(scene.primitives[1].source.objectId,
             sizeof(scene.primitives[1].source.objectId),
             "%s",
             "blocker");
    scene.triangleMesh.triangles[0].p0 = vec3(-3.0, -5.0, -3.0);
    scene.triangleMesh.triangles[0].p1 = vec3(-3.0, -5.0, 3.0);
    scene.triangleMesh.triangles[0].p2 = vec3(3.0, -5.0, -3.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 5;
    scene.triangleMesh.triangles[1].p0 = vec3(1.0, -4.5, -2.0);
    scene.triangleMesh.triangles[1].p1 = vec3(1.0, -2.5, 0.0);
    scene.triangleMesh.triangles[1].p2 = vec3(1.0, -4.5, 2.0);
    scene.triangleMesh.triangles[1].normal = vec3(1.0, 0.0, 0.0);
    scene.triangleMesh.triangles[1].primitiveIndex = 1;
    scene.triangleMesh.triangles[1].sceneObjectIndex = 6;

    hit.t = 5.0;
    hit.position = vec3(0.0, -5.0, 0.0);
    hit.normal = vec3(0.0, 1.0, 0.0);
    hit.triangleIndex = 0;
    hit.primitiveIndex = 0;
    hit.sceneObjectIndex = 5;
    hit.source = scene.primitives[0].source;
    hit.baryU = 0.333333333333;
    hit.baryV = 0.333333333333;
    hit.baryW = 0.333333333334;

    ok = RuntimeDirectLight3D_ShadeHit(&scene, &hit, &result);
    assert_true("runtime_direct_light_3d_shadowed_shade_ok", ok);
    assert_true("runtime_direct_light_3d_shadowed_hit", result.hit);
    assert_true("runtime_direct_light_3d_shadowed_not_visible", !result.visible);
    assert_close("runtime_direct_light_3d_shadowed_radiance_zero", result.radiance, 0.0, 1e-9);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_direct_light_3d_authored_light_motion_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_direct_light_motion\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"lit_wall\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":6.0,\"height\":6.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":-2.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"ray_tracing\":{"
            "\"authoring\":{"
              "\"light_path\":{"
                "\"mode\":\"BEZIER_CUBIC\","
                "\"points\":["
                  "{\"x\":0.0,\"y\":-2.0,\"rotation\":0.0,\"handleLink\":false},"
                  "{\"x\":3.0,\"y\":-2.0,\"rotation\":0.0,\"handleLink\":false}"
                "]"
              "},"
              "\"light_path_depth\":{"
                "\"points\":["
                  "{\"z\":0.0},"
                  "{\"z\":3.0}"
                "]"
              "}"
            "}"
          "}"
        "}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeScene3D scene_start;
    RuntimeScene3D scene_end;
    RuntimeCameraProjector3D projector = {0};
    RuntimeDirectLight3DResult start_result = {0};
    RuntimeDirectLight3DResult end_result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene_start);
    RuntimeScene3D_Init(&scene_end);
    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_direct_light_3d_motion_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene_start);
        RuntimeScene3D_Free(&scene_end);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene_start, 0.0);
    assert_true("runtime_direct_light_3d_motion_build_start_ok", ok);
    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene_end, 1.0);
    assert_true("runtime_direct_light_3d_motion_build_end_ok", ok);
    ok = RuntimeCameraProjector3D_Build(&scene_start.camera, 101, 101, &projector);
    assert_true("runtime_direct_light_3d_motion_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene_start);
        RuntimeScene3D_Free(&scene_end);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    ok = RuntimeDirectLight3D_ShadePixel(&scene_start, &projector, 50.0, 50.0, &start_result);
    assert_true("runtime_direct_light_3d_motion_shade_start_ok", ok);
    ok = RuntimeDirectLight3D_ShadePixel(&scene_end, &projector, 50.0, 50.0, &end_result);
    assert_true("runtime_direct_light_3d_motion_shade_end_ok", ok);
    assert_true("runtime_direct_light_3d_motion_start_visible", start_result.visible);
    assert_true("runtime_direct_light_3d_motion_end_visible", end_result.visible);
    assert_true("runtime_direct_light_3d_motion_radiance_changes",
                fabs(start_result.radiance - end_result.radiance) > 1e-6);
    assert_true("runtime_direct_light_3d_motion_distance_changes",
                fabs(start_result.lightDistance - end_result.lightDistance) > 1e-6);

    RuntimeScene3D_Free(&scene_start);
    RuntimeScene3D_Free(&scene_end);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_diffuse_bounce_3d_shadowed_hit_lift_contract(void) {
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimeDirectLight3DResult direct_result = {0};
    RuntimeDiffuseBounce3DResult diffuse_result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    scene.hasLight = true;
    scene.light.position = vec3(2.0, -2.0, 0.0);
    scene.light.intensity = 10.0;
    scene.light.falloffDistance = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene.primitiveCapacity = 2;
    scene.triangleMesh.triangleCapacity = 2;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_diffuse_bounce_shadowed_alloc_primitives", scene.primitives != NULL);
    assert_true("runtime_diffuse_bounce_shadowed_alloc_triangles", scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    scene.primitiveCount = 2;
    scene.triangleMesh.triangleCount = 2;
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 7;
    snprintf(scene.primitives[0].source.objectId,
             sizeof(scene.primitives[0].source.objectId),
             "%s",
             "floor");
    scene.primitives[1].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[1].source.sceneObjectIndex = 8;
    snprintf(scene.primitives[1].source.objectId,
             sizeof(scene.primitives[1].source.objectId),
             "%s",
             "bounce_card");

    scene.triangleMesh.triangles[0].p0 = vec3(-3.0, -5.0, -3.0);
    scene.triangleMesh.triangles[0].p1 = vec3(-3.0, -5.0, 3.0);
    scene.triangleMesh.triangles[0].p2 = vec3(3.0, -5.0, -3.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 7;

    scene.triangleMesh.triangles[1].p0 = vec3(0.75, -4.8, -1.0);
    scene.triangleMesh.triangles[1].p1 = vec3(0.75, -3.1, 0.0);
    scene.triangleMesh.triangles[1].p2 = vec3(0.75, -4.8, 1.0);
    scene.triangleMesh.triangles[1].normal = vec3(1.0, 0.0, 0.0);
    scene.triangleMesh.triangles[1].primitiveIndex = 1;
    scene.triangleMesh.triangles[1].sceneObjectIndex = 8;

    hit.t = 5.0;
    hit.position = vec3(0.0, -5.0, 0.0);
    hit.normal = vec3(0.0, 1.0, 0.0);
    hit.triangleIndex = 0;
    hit.primitiveIndex = 0;
    hit.sceneObjectIndex = 7;
    hit.source = scene.primitives[0].source;
    hit.baryU = 0.333333333333;
    hit.baryV = 0.333333333333;
    hit.baryW = 0.333333333334;

    ok = RuntimeDirectLight3D_ShadeHit(&scene, &hit, &direct_result);
    assert_true("runtime_diffuse_bounce_shadowed_direct_ok", ok);
    assert_true("runtime_diffuse_bounce_shadowed_direct_not_visible", !direct_result.visible);
    assert_close("runtime_diffuse_bounce_shadowed_direct_zero",
                 direct_result.radiance,
                 0.0,
                 1e-9);

    ok = RuntimeDiffuseBounce3D_ShadeHit(&scene, &hit, &diffuse_result);
    assert_true("runtime_diffuse_bounce_shadowed_diffuse_ok", ok);
    assert_true("runtime_diffuse_bounce_shadowed_diffuse_hit", diffuse_result.hit);
    assert_close("runtime_diffuse_bounce_shadowed_direct_preserved",
                 diffuse_result.directRadiance,
                 0.0,
                 1e-9);
    assert_true("runtime_diffuse_bounce_shadowed_secondary_rays_24",
                diffuse_result.secondaryRayCount == 24);
    assert_true("runtime_diffuse_bounce_shadowed_secondary_hits_positive",
                diffuse_result.secondaryHitCount > 0);
    assert_true("runtime_diffuse_bounce_shadowed_secondary_lit_hits_positive",
                diffuse_result.secondaryContributingHitCount > 0);
    assert_true("runtime_diffuse_bounce_shadowed_bounce_positive",
                diffuse_result.bounceRadiance > 0.0);
    assert_true("runtime_diffuse_bounce_shadowed_total_positive",
                diffuse_result.radiance > 0.0);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_diffuse_bounce_3d_seed_branch_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_diffuse_bounce_seed\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"lit_wall\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":8.0,\"height\":8.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":-2.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeScene3D scene;
    RuntimeCameraProjector3D projector = {0};
    RuntimePrimaryHit3DResult primary_hit = {0};
    RuntimeDirectLight3DResult direct_result = {0};
    RuntimeDiffuseBounce3DResult diffuse_result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_diffuse_bounce_seed_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, 0.0);
    assert_true("runtime_diffuse_bounce_seed_build_ok", ok);
    ok = RuntimeCameraProjector3D_Build(&scene.camera, 101, 101, &projector);
    assert_true("runtime_diffuse_bounce_seed_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    ok = RuntimeDirectLight3D_TracePrimaryHit(&scene, &projector, 50.0, 50.0, &primary_hit);
    assert_true("runtime_diffuse_bounce_seed_primary_hit_ok", ok);
    assert_true("runtime_diffuse_bounce_seed_primary_hit_found", primary_hit.hit);

    ok = RuntimeDirectLight3D_ShadePixel(&scene, &projector, 50.0, 50.0, &direct_result);
    assert_true("runtime_diffuse_bounce_seed_direct_ok", ok);
    ok = RuntimeDiffuseBounce3D_ShadePixel(&scene, &projector, 50.0, 50.0, &diffuse_result);
    assert_true("runtime_diffuse_bounce_seed_diffuse_ok", ok);
    assert_true("runtime_diffuse_bounce_seed_diffuse_hit", diffuse_result.hit);
    assert_true("runtime_diffuse_bounce_seed_same_triangle",
                diffuse_result.hitInfo.triangleIndex == primary_hit.hitInfo.triangleIndex);
    assert_true("runtime_diffuse_bounce_seed_secondary_rays_24",
                diffuse_result.secondaryRayCount == 24);
    assert_true("runtime_diffuse_bounce_seed_secondary_hits_zero",
                diffuse_result.secondaryHitCount == 0);
    assert_close("runtime_diffuse_bounce_seed_direct_match",
                 diffuse_result.directRadiance,
                 direct_result.radiance,
                 1e-9);
    assert_close("runtime_diffuse_bounce_seed_total_match",
                 diffuse_result.radiance,
                 direct_result.radiance,
                 1e-9);
    assert_close("runtime_diffuse_bounce_seed_bounce_zero",
                 diffuse_result.bounceRadiance,
                 0.0,
                 1e-9);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_material_response_3d_seed_branch_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_material_response_seed\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"lit_wall\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":8.0,\"height\":8.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"material_ref\":{\"id\":\"mat_glossy\"}"
          "}"
        "],"
        "\"materials\":["
          "{"
            "\"material_id\":\"mat_glossy\","
            "\"albedo\":[0.8, 0.8, 0.8]"
          "}"
        "],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":-2.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"ray_tracing\":{"
            "\"authoring\":{"
              "\"object_materials\":[{\"scene_object_index\":0,\"material_id\":3}]"
            "}"
          "}"
        "}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeScene3D scene;
    RuntimeCameraProjector3D projector = {0};
    RuntimeDiffuseBounce3DResult diffuse_result = {0};
    RuntimeMaterialResponse3DResult matte_result = {0};
    RuntimeMaterialResponse3DResult mirror_result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_material_response_seed_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, 0.0);
    assert_true("runtime_material_response_seed_build_ok", ok);
    ok = RuntimeCameraProjector3D_Build(&scene.camera, 101, 101, &projector);
    assert_true("runtime_material_response_seed_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    ok = RuntimeDiffuseBounce3D_ShadePixel(&scene, &projector, 50.0, 50.0, &diffuse_result);
    assert_true("runtime_material_response_seed_diffuse_ok", ok);

    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_DEFAULT;
    ok = RuntimeMaterialResponse3D_ShadePixel(&scene, &projector, 50.0, 50.0, &matte_result);
    assert_true("runtime_material_response_seed_matte_ok", ok);
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_MIRROR;
    ok = RuntimeMaterialResponse3D_ShadePixel(&scene, &projector, 50.0, 50.0, &mirror_result);
    assert_true("runtime_material_response_seed_mirror_ok", ok);
    assert_true("runtime_material_response_seed_matte_hit", matte_result.hit);
    assert_true("runtime_material_response_seed_mirror_hit", mirror_result.hit);
    assert_true("runtime_material_response_seed_matte_payload_resolved",
                matte_result.materialResolved);
    assert_true("runtime_material_response_seed_mirror_payload_resolved",
                mirror_result.materialResolved);
    assert_true("runtime_material_response_seed_matte_id_match",
                matte_result.payload.materialId == MATERIAL_PRESET_DEFAULT);
    assert_true("runtime_material_response_seed_mirror_id_match",
                mirror_result.payload.materialId == MATERIAL_PRESET_MIRROR);
    assert_true("runtime_material_response_seed_matte_secondary_rays_match",
                matte_result.secondaryRayCount == diffuse_result.secondaryRayCount);
    assert_true("runtime_material_response_seed_mirror_secondary_rays_match",
                mirror_result.secondaryRayCount == diffuse_result.secondaryRayCount);
    assert_true("runtime_material_response_seed_matte_differs_from_diffuse",
                fabs(matte_result.radiance - diffuse_result.radiance) > 1e-6);
    assert_true("runtime_material_response_seed_mirror_differs_from_diffuse",
                fabs(mirror_result.radiance - diffuse_result.radiance) > 1e-6);
    assert_true("runtime_material_response_seed_mirror_direct_vs_matte",
                fabs(mirror_result.directRadiance - matte_result.directRadiance) > 1e-6);
    assert_true("runtime_material_response_seed_bounce_zero_preserved",
                matte_result.bounceRadiance == 0.0 &&
                mirror_result.bounceRadiance == 0.0);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_emission_transparency_3d_seed_branch_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_emission_transparency_seed\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"lit_wall\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":8.0,\"height\":8.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"material_ref\":{\"id\":\"mat_emissive\"}"
          "}"
        "],"
        "\"materials\":["
          "{"
            "\"material_id\":\"mat_emissive\","
            "\"emissive\":[1.0, 1.0, 1.0]"
          "}"
        "],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":-2.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"ray_tracing\":{"
            "\"authoring\":{"
              "\"object_materials\":[{\"scene_object_index\":0,\"material_id\":4}]"
            "}"
          "}"
        "}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeScene3D scene;
    RuntimeCameraProjector3D projector = {0};
    RuntimeMaterialResponse3DResult material_result = {0};
    RuntimeEmissionTransparency3DResult emission_result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_emission_transparency_seed_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, 0.0);
    assert_true("runtime_emission_transparency_seed_build_ok", ok);
    ok = RuntimeCameraProjector3D_Build(&scene.camera, 101, 101, &projector);
    assert_true("runtime_emission_transparency_seed_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_EMISSIVE;
    ok = RuntimeMaterialResponse3D_ShadePixel(&scene, &projector, 50.0, 50.0, &material_result);
    assert_true("runtime_emission_transparency_seed_material_ok", ok);
    ok = RuntimeEmissionTransparency3D_ShadePixel(&scene,
                                                  &projector,
                                                  50.0,
                                                  50.0,
                                                  &emission_result);
    assert_true("runtime_emission_transparency_seed_branch_ok", ok);
    assert_true("runtime_emission_transparency_seed_hit", emission_result.hit);
    assert_true("runtime_emission_transparency_seed_payload_resolved",
                emission_result.payloadResolved);
    assert_true("runtime_emission_transparency_seed_payload_valid",
                emission_result.payload.valid);
    assert_true("runtime_emission_transparency_seed_material_id_match",
                emission_result.payload.materialId == MATERIAL_PRESET_EMISSIVE);
    assert_true("runtime_emission_transparency_seed_emissive_positive",
                emission_result.payload.emissive > 0.0);
    assert_close("runtime_emission_transparency_seed_transparency_zero",
                 emission_result.payload.transparency,
                 0.0,
                 1e-9);
    assert_true("runtime_emission_transparency_seed_secondary_rays_match",
                emission_result.secondaryRayCount == 24);
    assert_true("runtime_emission_transparency_seed_direct_lifts_material",
                emission_result.directRadiance > material_result.directRadiance);
    assert_close("runtime_emission_transparency_seed_bounce_match",
                 emission_result.bounceRadiance,
                 material_result.bounceRadiance,
                 1e-9);
    assert_true("runtime_emission_transparency_seed_total_lifts_material",
                emission_result.radiance > material_result.radiance);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_emission_transparency_3d_transmission_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    char dir_template[] = "/tmp/ray_tracing_materialsXXXXXX";
    char transparent_path[PATH_MAX];
    char matte_path[PATH_MAX];
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_emission_transparency_transmission\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"front_wall\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":8.0,\"height\":8.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-4.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-4.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"back_wall\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":8.0,\"height\":8.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-7.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-7.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":-2.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeScene3D scene;
    RuntimeCameraProjector3D projector = {0};
    RuntimeMaterialResponse3DResult material_result = {0};
    RuntimeEmissionTransparency3DResult transparent_result = {0};
    const Material* material = NULL;
    bool ok = false;
    int transparent_id = -1;
    int matte_id = -1;

    RuntimeScene3D_Init(&scene);
    MaterialManagerResetDefaults();
    if (!mkdtemp(dir_template)) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    snprintf(transparent_path, sizeof(transparent_path), "%s/00_transparent.json", dir_template);
    snprintf(matte_path, sizeof(matte_path), "%s/01_matte.json", dir_template);
    ok = write_text_file(transparent_path,
                         "{"
                         "\"diffuse\":0.15,"
                         "\"specular\":0.0,"
                         "\"reflectivity\":0.0,"
                         "\"roughness\":1.0,"
                         "\"transparency\":0.75,"
                         "\"base_color\":[1.0,1.0,1.0],"
                         "\"emissive\":[0.0,0.0,0.0]"
                         "}");
    assert_true("runtime_emission_transparency_transparency_file_ok", ok);
    ok = write_text_file(matte_path,
                         "{"
                         "\"diffuse\":0.85,"
                         "\"specular\":0.05,"
                         "\"reflectivity\":0.1,"
                         "\"roughness\":0.6,"
                         "\"transparency\":0.0,"
                         "\"base_color\":[1.0,1.0,1.0],"
                         "\"emissive\":[0.0,0.0,0.0]"
                         "}");
    assert_true("runtime_emission_transparency_matte_file_ok", ok);
    MaterialManagerLoadDir(dir_template);

    for (int i = 0; i < MaterialManagerCount(); ++i) {
        material = MaterialManagerGet(i);
        if (material && material->transparency > 0.5f) {
            transparent_id = i;
        } else if (material) {
            matte_id = i;
        }
    }
    assert_true("runtime_emission_transparency_transparent_id_found", transparent_id >= 0);
    assert_true("runtime_emission_transparency_matte_id_found", matte_id >= 0);

    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_emission_transparency_transmission_apply_ok", ok);
    if (!ok) {
        remove(transparent_path);
        remove(matte_path);
        rmdir(dir_template);
        MaterialManagerResetDefaults();
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    sceneSettings.sceneObjects[0].material_id = transparent_id;
    sceneSettings.sceneObjects[1].material_id = matte_id;
    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, 0.0);
    assert_true("runtime_emission_transparency_transmission_build_ok", ok);
    ok = RuntimeCameraProjector3D_Build(&scene.camera, 101, 101, &projector);
    assert_true("runtime_emission_transparency_transmission_projector_ok", ok);
    if (!ok) {
        remove(transparent_path);
        remove(matte_path);
        rmdir(dir_template);
        MaterialManagerResetDefaults();
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    ok = RuntimeMaterialResponse3D_ShadePixel(&scene, &projector, 50.0, 50.0, &material_result);
    assert_true("runtime_emission_transparency_transmission_material_ok", ok);
    ok = RuntimeEmissionTransparency3D_ShadePixel(&scene,
                                                  &projector,
                                                  50.0,
                                                  50.0,
                                                  &transparent_result);
    assert_true("runtime_emission_transparency_transmission_branch_ok", ok);
    assert_true("runtime_emission_transparency_transmission_hit", transparent_result.hit);
    assert_true("runtime_emission_transparency_transmission_payload_resolved",
                transparent_result.payloadResolved);
    assert_true("runtime_emission_transparency_transmission_transparency_positive",
                transparent_result.payload.transparency > 0.5);
    assert_true("runtime_emission_transparency_transmission_radiance_differs",
                fabs(transparent_result.radiance - material_result.radiance) > 1e-6);
    assert_true("runtime_emission_transparency_transmission_direct_differs",
                fabs(transparent_result.directRadiance - material_result.directRadiance) > 1e-6);

    remove(transparent_path);
    remove(matte_path);
    rmdir(dir_template);
    MaterialManagerResetDefaults();
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_emission_transparency_3d_transparent_prism_reaches_behind_surface(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_emission_transparency_prism_through\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"front_prism\","
            "\"object_type\":\"rect_prism_primitive\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"primitive\":{\"kind\":\"rect_prism_primitive\","
              "\"width\":2.0,\"height\":2.0,\"depth\":2.0}"
          "},"
          "{"
            "\"object_id\":\"back_wall\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":6.0,\"height\":6.0,"
              "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-8.0,\"z\":0.0},"
              "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
              "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
              "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-8.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":1.5,\"y\":-3.0,\"z\":2.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeScene3D scene;
    RuntimeCameraProjector3D projector = {0};
    RuntimeMaterialResponse3DResult material_result = {0};
    RuntimeEmissionTransparency3DResult transparent_result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    MaterialManagerResetDefaults();

    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_emission_transparency_prism_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[1].material_id = MATERIAL_PRESET_EMISSIVE;
    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, 0.0);
    assert_true("runtime_emission_transparency_prism_build_ok", ok);
    ok = RuntimeCameraProjector3D_Build(&scene.camera, 101, 101, &projector);
    assert_true("runtime_emission_transparency_prism_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    ok = RuntimeMaterialResponse3D_ShadePixel(&scene, &projector, 50.0, 50.0, &material_result);
    assert_true("runtime_emission_transparency_prism_material_ok", ok);
    ok = RuntimeEmissionTransparency3D_ShadePixel(&scene,
                                                  &projector,
                                                  50.0,
                                                  50.0,
                                                  &transparent_result);
    assert_true("runtime_emission_transparency_prism_branch_ok", ok);
    assert_true("runtime_emission_transparency_prism_hit", transparent_result.hit);
    assert_true("runtime_emission_transparency_prism_payload_resolved",
                transparent_result.payloadResolved);
    assert_true("runtime_emission_transparency_prism_transparency_positive",
                transparent_result.payload.transparency > 0.5);
    assert_true("runtime_emission_transparency_prism_reaches_emissive_surface",
                transparent_result.directRadiance > material_result.directRadiance + 0.1);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_native_3d_render_live_buffer_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_native_runtime_render\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"lit_wall\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":8.0,\"height\":8.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":-2.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeNative3DRenderStats centered_stats = {0};
    RuntimeNative3DRenderStats diffuse_stats = {0};
    RuntimeNative3DRenderStats material_stats = {0};
    RuntimeNative3DRenderStats emission_stats = {0};
    RuntimeNative3DRenderStats offset_stats = {0};
    RayTracingRuntimeRoute route;
    uint8_t centered_pixels[51 * 51];
    uint8_t diffuse_pixels[51 * 51];
    uint8_t material_pixels[51 * 51];
    uint8_t emission_pixels[51 * 51];
    uint8_t offset_pixels[51 * 51];
    bool ok = false;
    bool material_differs = false;
    int i = 0;

    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_native_3d_render_apply_ok", ok);
    if (!ok) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.interactiveMode = false;
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DISNEY;
    sceneSettings.camera.x = 0.0;
    sceneSettings.camera.y = 0.0;
    sceneSettings.cameraZ = 0.0;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    route = RayTracingModeBackend_ResolveRoute();
    assert_true("runtime_native_3d_render_route_native", RayTracingModeBackend_IsNative3D(&route));
    assert_true("runtime_native_3d_render_route_3d_direct_light",
                route.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT);

    ok = RuntimeNative3DRenderToPixelBuffer(centered_pixels,
                                            route.integratorMode3D,
                                            51,
                                            51,
                                            0.0,
                                            0.0,
                                            -2.0,
                                            &centered_stats);
    assert_true("runtime_native_3d_render_centered_ok", ok);
    assert_true("runtime_native_3d_render_centered_hits_positive",
                centered_stats.hitPixelCount > 0);
    assert_true("runtime_native_3d_render_centered_visible_positive",
                centered_stats.visiblePixelCount > 0);
    assert_true("runtime_native_3d_render_centered_radiance_positive",
                centered_stats.maxRadiance > 0.0);
    assert_true("runtime_native_3d_render_centered_pixel_positive",
                centered_pixels[(25 * 51) + 25] > 0);

    ok = RuntimeNative3DRenderToPixelBuffer(diffuse_pixels,
                                            RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE,
                                            51,
                                            51,
                                            0.0,
                                            0.0,
                                            -2.0,
                                            &diffuse_stats);
    assert_true("runtime_native_3d_render_diffuse_seed_ok", ok);
    assert_true("runtime_native_3d_render_diffuse_seed_hits_positive",
                diffuse_stats.hitPixelCount > 0);
    assert_true("runtime_native_3d_render_diffuse_seed_visible_positive",
                diffuse_stats.visiblePixelCount > 0);
    assert_true("runtime_native_3d_render_diffuse_seed_secondary_rays_positive",
                diffuse_stats.secondaryRayCount > 0);
    assert_true("runtime_native_3d_render_diffuse_seed_secondary_hits_zero",
                diffuse_stats.secondaryHitCount == 0);
    assert_true("runtime_native_3d_render_diffuse_seed_bounce_pixels_zero",
                diffuse_stats.bouncePixelCount == 0);
    assert_close("runtime_native_3d_render_diffuse_seed_radiance_match",
                 diffuse_stats.maxRadiance,
                 centered_stats.maxRadiance,
                 1e-9);
    assert_true("runtime_native_3d_render_diffuse_seed_pixel_match",
                diffuse_pixels[(25 * 51) + 25] == centered_pixels[(25 * 51) + 25]);

    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_GLOSSY;
    ok = RuntimeNative3DRenderToPixelBuffer(material_pixels,
                                            RAY_TRACING_3D_INTEGRATOR_MATERIAL,
                                            51,
                                            51,
                                            0.0,
                                            0.0,
                                            -2.0,
                                            &material_stats);
    assert_true("runtime_native_3d_render_material_seed_ok", ok);
    assert_true("runtime_native_3d_render_material_seed_hits_positive",
                material_stats.hitPixelCount > 0);
    assert_true("runtime_native_3d_render_material_seed_visible_positive",
                material_stats.visiblePixelCount > 0);
    assert_true("runtime_native_3d_render_material_seed_secondary_rays_positive",
                material_stats.secondaryRayCount > 0);
    assert_true("runtime_native_3d_render_material_seed_radiance_differs",
                fabs(material_stats.maxRadiance - diffuse_stats.maxRadiance) > 1e-6);
    for (i = 0; i < (51 * 51); ++i) {
        if (material_pixels[i] != diffuse_pixels[i]) {
            material_differs = true;
            break;
        }
    }
    assert_true("runtime_native_3d_render_material_seed_pixels_differ",
                material_differs);

    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_EMISSIVE;
    ok = RuntimeNative3DRenderToPixelBuffer(emission_pixels,
                                            RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY,
                                            51,
                                            51,
                                            0.0,
                                            0.0,
                                            -2.0,
                                            &emission_stats);
    assert_true("runtime_native_3d_render_emission_seed_ok", ok);
    assert_true("runtime_native_3d_render_emission_seed_hits_positive",
                emission_stats.hitPixelCount > 0);
    assert_true("runtime_native_3d_render_emission_seed_visible_positive",
                emission_stats.visiblePixelCount > 0);
    assert_true("runtime_native_3d_render_emission_seed_secondary_rays_positive",
                emission_stats.secondaryRayCount > 0);
    assert_true("runtime_native_3d_render_emission_seed_radiance_positive",
                emission_stats.maxRadiance > 0.0);
    ok = RuntimeNative3DRenderToPixelBuffer(material_pixels,
                                            RAY_TRACING_3D_INTEGRATOR_MATERIAL,
                                            51,
                                            51,
                                            0.0,
                                            0.0,
                                            -2.0,
                                            &material_stats);
    assert_true("runtime_native_3d_render_emission_material_reference_ok", ok);
    assert_true("runtime_native_3d_render_emission_seed_radiance_lifts_material",
                emission_stats.maxRadiance > material_stats.maxRadiance);
    assert_true("runtime_native_3d_render_emission_seed_center_pixel_lifts_material",
                emission_pixels[(25 * 51) + 25] > material_pixels[(25 * 51) + 25]);

    ok = RuntimeNative3DRenderToPixelBuffer(offset_pixels,
                                            route.integratorMode3D,
                                            51,
                                            51,
                                            0.0,
                                            3.0,
                                            -2.0,
                                            &offset_stats);
    assert_true("runtime_native_3d_render_offset_ok", ok);
    assert_true("runtime_native_3d_render_offset_pixel_changes",
                centered_pixels[(25 * 51) + 25] != offset_pixels[(25 * 51) + 25]);

    memset(offset_pixels, 255, sizeof(offset_pixels));
    memset(&offset_stats, 0, sizeof(offset_stats));
    ok = RuntimeNative3DRenderToPixelBuffer(offset_pixels,
                                            RAY_TRACING_3D_INTEGRATOR_DISNEY,
                                            51,
                                            51,
                                            0.0,
                                            3.0,
                                            -2.0,
                                            &offset_stats);
    assert_true("runtime_native_3d_render_disney_rejected", !ok);
    assert_true("runtime_native_3d_render_disney_hits_zero",
                offset_stats.hitPixelCount == 0);
    assert_true("runtime_native_3d_render_disney_pixel_cleared",
                offset_pixels[(25 * 51) + 25] == 0);

    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_native_3d_render_prepared_region_parity(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_native_3d_tiled_parity\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"floor\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":8.0,\"height\":8.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":-2.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeNative3DRenderStats full_stats = {0};
    RuntimeNative3DRenderStats tiled_stats = {0};
    RuntimeNative3DPreparedFrame frame = {0};
    TileGrid grid = {0};
    uint8_t full_pixels[51 * 51];
    uint8_t tiled_pixels[51 * 51];
    bool ok = false;

    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_native_3d_tile_parity_apply_ok", ok);
    if (!ok) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.interactiveMode = false;
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE;
    sceneSettings.camera.x = 0.0;
    sceneSettings.camera.y = 0.0;
    sceneSettings.cameraZ = 0.0;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = RuntimeNative3DRenderToPixelBuffer(full_pixels,
                                            RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE,
                                            51,
                                            51,
                                            0.0,
                                            0.0,
                                            -2.0,
                                            &full_stats);
    assert_true("runtime_native_3d_tile_parity_full_ok", ok);

    memset(tiled_pixels, 0, sizeof(tiled_pixels));
    ok = RuntimeNative3DPrepareFrame(&frame, 51, 51, 0.0, 0.0, -2.0);
    assert_true("runtime_native_3d_tile_parity_prepare_ok", ok);
    if (ok) {
        TileGridEnsure(&grid, 51, 51, 16);
        ok = RuntimeNative3DPrepareFrameTileOccupancy(&frame, grid.tileSize);
        assert_true("runtime_native_3d_tile_parity_occupancy_prepare_ok", ok);
        for (size_t ti = 0; ti < grid.count; ++ti) {
            const IntegratorTile* tile = &grid.tiles[ti];
            RuntimeNative3DRenderStats tile_stats = {0};
            if (!RuntimeNative3DPreparedRegionMayContainGeometry(&frame,
                                                                 tile->originX,
                                                                 tile->originY,
                                                                 tile->originX + tile->width,
                                                                 tile->originY + tile->height)) {
                continue;
            }
            ok = RuntimeNative3DRenderPreparedRegion(tiled_pixels,
                                                     RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE,
                                                     &frame,
                                                     tile->originX,
                                                     tile->originY,
                                                     tile->originX + tile->width,
                                                     tile->originY + tile->height,
                                                     &tile_stats);
            assert_true("runtime_native_3d_tile_parity_region_ok", ok);
            if (!ok) break;
            RuntimeNative3DRenderStats_Accumulate(&tiled_stats, &tile_stats);
        }
    }

    assert_true("runtime_native_3d_tile_parity_pixels_match",
                memcmp(full_pixels, tiled_pixels, sizeof(full_pixels)) == 0);
    assert_true("runtime_native_3d_tile_parity_hit_count_match",
                full_stats.hitPixelCount == tiled_stats.hitPixelCount);
    assert_true("runtime_native_3d_tile_parity_visible_count_match",
                full_stats.visiblePixelCount == tiled_stats.visiblePixelCount);
    assert_true("runtime_native_3d_tile_parity_bounce_pixels_match",
                full_stats.bouncePixelCount == tiled_stats.bouncePixelCount);
    assert_true("runtime_native_3d_tile_parity_secondary_rays_match",
                full_stats.secondaryRayCount == tiled_stats.secondaryRayCount);
    assert_true("runtime_native_3d_tile_parity_secondary_hits_match",
                full_stats.secondaryHitCount == tiled_stats.secondaryHitCount);
    assert_true("runtime_native_3d_tile_parity_secondary_lit_hits_match",
                full_stats.secondaryContributingHitCount ==
                    tiled_stats.secondaryContributingHitCount);
    assert_close("runtime_native_3d_tile_parity_max_radiance_match",
                 full_stats.maxRadiance,
                 tiled_stats.maxRadiance,
                 1e-9);
    assert_close("runtime_native_3d_tile_parity_max_bounce_match",
                 full_stats.maxBounceRadiance,
                 tiled_stats.maxBounceRadiance,
                 1e-9);
    assert_close("runtime_native_3d_tile_parity_total_bounce_match",
                 full_stats.totalBounceRadiance,
                 tiled_stats.totalBounceRadiance,
                 1e-9);

    TileGridFree(&grid);
    RuntimeNative3DPreparedFrame_Free(&frame);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_native_3d_tile_occupancy_contract(void) {
    RuntimeScene3D scene = {0};
    RuntimeCamera3D camera = {0};
    RuntimeCameraProjector3D projector = {0};
    RuntimeNative3DTileOccupancy occupancy = {0};
    RuntimeTriangle3D* triangle = NULL;
    int occupied_tiles = 0;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    RuntimeNative3DTileOccupancy_Init(&occupancy);

    triangle = (RuntimeTriangle3D*)calloc(1, sizeof(*triangle));
    assert_true("runtime_native_3d_tile_occupancy_triangle_alloc", triangle != NULL);
    if (!triangle) {
        RuntimeNative3DTileOccupancy_Free(&occupancy);
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    triangle->p0 = vec3(-1.0, -5.0, -1.0);
    triangle->p1 = vec3(1.0, -5.0, -1.0);
    triangle->p2 = vec3(0.0, -5.0, 1.0);
    triangle->normal = vec3(0.0, -1.0, 0.0);
    scene.triangleMesh.triangles = triangle;
    scene.triangleMesh.triangleCount = 1;
    scene.triangleMesh.triangleCapacity = 1;

    camera.position = vec3(0.0, 0.0, 0.0);
    camera.rotation = 0.0;
    camera.lookPitch = 0.0;
    camera.zoom = 1.0;
    camera.nearPlane = 0.1;

    ok = RuntimeCameraProjector3D_Build(&camera, 64, 64, &projector);
    assert_true("runtime_native_3d_tile_occupancy_projector_ok", ok);
    ok = RuntimeNative3DTileOccupancy_Build(&occupancy, &scene, &projector, 16);
    assert_true("runtime_native_3d_tile_occupancy_build_ok", ok);

    for (int ty = 0; ty < 4; ++ty) {
        for (int tx = 0; tx < 4; ++tx) {
            if (RuntimeNative3DTileOccupancy_RegionMayContainGeometry(&occupancy,
                                                                      tx * 16,
                                                                      ty * 16,
                                                                      (tx + 1) * 16,
                                                                      (ty + 1) * 16)) {
                occupied_tiles += 1;
            }
        }
    }

    assert_true("runtime_native_3d_tile_occupancy_positive_tiles",
                occupied_tiles > 0);
    assert_true("runtime_native_3d_tile_occupancy_culls_some_tiles",
                occupied_tiles < 16);
    assert_true("runtime_native_3d_tile_occupancy_center_tile_hit",
                RuntimeNative3DTileOccupancy_RegionMayContainGeometry(&occupancy,
                                                                      16,
                                                                      16,
                                                                      32,
                                                                      32));
    assert_true("runtime_native_3d_tile_occupancy_corner_tile_empty",
                !RuntimeNative3DTileOccupancy_RegionMayContainGeometry(&occupancy,
                                                                       0,
                                                                       0,
                                                                       16,
                                                                       16));

    RuntimeNative3DTileOccupancy_Free(&occupancy);
    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_native_3d_dirty_rect_preview_base_parity(void) {
    enum { kWidth = 8, kHeight = 6 };
    Uint8 luminance[kWidth * kHeight];
    uint32_t full_abgr[kWidth * kHeight];
    uint32_t dirty_abgr[kWidth * kHeight];
    SDL_Rect rect_a = {.x = 0, .y = 0, .w = 4, .h = 3};
    SDL_Rect rect_b = {.x = 4, .y = 0, .w = 4, .h = 3};
    SDL_Rect rect_c = {.x = 0, .y = 3, .w = 8, .h = 3};

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            luminance[y * kWidth + x] = (Uint8)((x * 17) + (y * 29));
        }
    }

    memset(full_abgr, 0, sizeof(full_abgr));
    memset(dirty_abgr, 0, sizeof(dirty_abgr));

    RayTracingPreview_CopyLuminanceRectToABGR(full_abgr,
                                              kWidth,
                                              kHeight,
                                              luminance,
                                              NULL);
    RayTracingPreview_CopyLuminanceRectToABGR(dirty_abgr,
                                              kWidth,
                                              kHeight,
                                              luminance,
                                              &rect_a);
    RayTracingPreview_CopyLuminanceRectToABGR(dirty_abgr,
                                              kWidth,
                                              kHeight,
                                              luminance,
                                              &rect_b);
    RayTracingPreview_CopyLuminanceRectToABGR(dirty_abgr,
                                              kWidth,
                                              kHeight,
                                              luminance,
                                              &rect_c);

    assert_true("runtime_native_3d_dirty_rect_preview_base_parity_match",
                memcmp(full_abgr, dirty_abgr, sizeof(full_abgr)) == 0);
    return 0;
}

static int test_animation_output_render_metrics_route_truth_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    int saved_frame_counter = frameCounter;
    int saved_loop_count = loopCount;
    double saved_current_time = currentTime;
    const char* export_env = getenv("RAY_TRACING_EXPORT_RENDER_METRICS_DATASET");
    const char* path_env = getenv("RAY_TRACING_RENDER_METRICS_DATASET_PATH");
    char* export_env_backup = export_env ? strdup(export_env) : NULL;
    char* path_env_backup = path_env ? strdup(path_env) : NULL;
    char tmp_template[] = "/tmp/ray_tracing_metrics_route_truthXXXXXX";
    int fd = mkstemp(tmp_template);
    char json_path[1024];
    char* json_text = NULL;
    const char *runtime_json_native =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_metrics_native_route\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"floor\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":6.0,\"height\":6.0,"
              "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_v\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
              "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"block\","
            "\"object_type\":\"rect_prism_primitive\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-3.0,\"z\":0.5},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"primitive\":{\"kind\":\"rect_prism_primitive\","
              "\"width\":2.0,\"height\":2.0,\"depth\":1.0}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":1.0,\"y\":-1.5,\"z\":2.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":2.0,\"z\":8.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *runtime_json_compat =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_metrics_compat_route\","
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
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":20.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    cJSON* root = NULL;
    cJSON* metadata = NULL;
    cJSON* items = NULL;
    cJSON* table = NULL;
    cJSON* row0 = NULL;

    if (fd < 0) {
        assert_true("animation_output_metrics_route_truth_mkstemp", false);
        free(export_env_backup);
        free(path_env_backup);
        return 0;
    }
    close(fd);

    if (snprintf(json_path, sizeof(json_path), "%s.json", tmp_template) >= (int)sizeof(json_path)) {
        assert_true("animation_output_metrics_route_truth_path_overflow", false);
        unlink(tmp_template);
        free(export_env_backup);
        free(path_env_backup);
        return 0;
    }
    if (rename(tmp_template, json_path) != 0) {
        assert_true("animation_output_metrics_route_truth_path_rename", false);
        unlink(tmp_template);
        free(export_env_backup);
        free(path_env_backup);
        return 0;
    }

    setenv("RAY_TRACING_EXPORT_RENDER_METRICS_DATASET", "1", 1);
    setenv("RAY_TRACING_RENDER_METRICS_DATASET_PATH", json_path, 1);
    frameCounter = 7;
    loopCount = 3;
    currentTime = 1.5;

    memset(&animSettings, 0, sizeof(animSettings));
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    animSettings.spaceMode = SPACE_MODE_2D;
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_HYBRID;
    animSettings.fps = 30;
    animSettings.frameDuration = 1.0 / 30.0;
    AnimationExportRenderMetricsDatasetIfEnabled();
    json_text = read_text_file_alloc(json_path, NULL);
    assert_true("animation_output_metrics_route_truth_2d_json", json_text != NULL);
    if (json_text) {
        root = cJSON_Parse(json_text);
        metadata = root ? cJSON_GetObjectItem(root, "metadata") : NULL;
        items = root ? cJSON_GetObjectItem(root, "items") : NULL;
        table = find_named_dataset_entry(items, "render_metrics_table_v2");
        row0 = table ? cJSON_GetObjectItem(table, "row0") : NULL;
        assert_true("animation_output_metrics_route_truth_2d_parse", root != NULL);
        assert_true("animation_output_metrics_route_truth_2d_row0", row0 != NULL);
        assert_true("animation_output_metrics_route_truth_2d_legacy_mode",
                    cJSON_GetObjectItem(row0, "integrator_mode") &&
                    cJSON_GetObjectItem(row0, "integrator_mode")->valueint == 1);
        assert_true("animation_output_metrics_route_truth_2d_route_family",
                    cJSON_GetObjectItem(row0, "route_family") &&
                    cJSON_GetObjectItem(row0, "route_family")->valueint == 0);
        assert_true("animation_output_metrics_route_truth_2d_uses_3d_false",
                    cJSON_GetObjectItem(row0, "integrator_uses_3d_catalog") &&
                    !cJSON_IsTrue(cJSON_GetObjectItem(row0, "integrator_uses_3d_catalog")));
        assert_true("animation_output_metrics_route_truth_2d_status_label",
                    metadata &&
                    cJSON_GetObjectItem(metadata, "integrator_status_label") &&
                    strcmp(cJSON_GetObjectItem(metadata, "integrator_status_label")->valuestring,
                           "integrator: Hybrid") == 0);
        cJSON_Delete(root);
        root = NULL;
        free(json_text);
        json_text = NULL;
    }

    memset(&animSettings, 0, sizeof(animSettings));
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_FORWARD_LIGHT;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT;
    assert_true("animation_output_metrics_route_truth_native_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json_native, &summary));
    AnimationExportRenderMetricsDatasetIfEnabled();
    json_text = read_text_file_alloc(json_path, NULL);
    assert_true("animation_output_metrics_route_truth_native_json", json_text != NULL);
    if (json_text) {
        root = cJSON_Parse(json_text);
        metadata = root ? cJSON_GetObjectItem(root, "metadata") : NULL;
        items = root ? cJSON_GetObjectItem(root, "items") : NULL;
        table = find_named_dataset_entry(items, "render_metrics_table_v2");
        row0 = table ? cJSON_GetObjectItem(table, "row0") : NULL;
        assert_true("animation_output_metrics_route_truth_native_parse", root != NULL);
        assert_true("animation_output_metrics_route_truth_native_row0", row0 != NULL);
        assert_true("animation_output_metrics_route_truth_native_legacy_field_preserved",
                    cJSON_GetObjectItem(row0, "integrator_mode") &&
                    cJSON_GetObjectItem(row0, "integrator_mode")->valueint == 0);
        assert_true("animation_output_metrics_route_truth_native_3d_mode",
                    cJSON_GetObjectItem(row0, "integrator_mode_3d") &&
                    cJSON_GetObjectItem(row0, "integrator_mode_3d")->valueint == 0);
        assert_true("animation_output_metrics_route_truth_native_route_family",
                    cJSON_GetObjectItem(row0, "route_family") &&
                    cJSON_GetObjectItem(row0, "route_family")->valueint == 2);
        assert_true("animation_output_metrics_route_truth_native_uses_3d_true",
                    cJSON_GetObjectItem(row0, "integrator_uses_3d_catalog") &&
                    cJSON_IsTrue(cJSON_GetObjectItem(row0, "integrator_uses_3d_catalog")));
        assert_true("animation_output_metrics_route_truth_native_status_label",
                    metadata &&
                    cJSON_GetObjectItem(metadata, "integrator_status_label") &&
                    strcmp(cJSON_GetObjectItem(metadata, "integrator_status_label")->valuestring,
                           "integrator: 3D Direct Light") == 0);
        cJSON_Delete(root);
        root = NULL;
        free(json_text);
        json_text = NULL;
    }

    memset(&animSettings, 0, sizeof(animSettings));
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_FORWARD_LIGHT;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE;
    assert_true("animation_output_metrics_route_truth_native_diffuse_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json_native, &summary));
    AnimationExportRenderMetricsDatasetIfEnabled();
    json_text = read_text_file_alloc(json_path, NULL);
    assert_true("animation_output_metrics_route_truth_native_diffuse_json", json_text != NULL);
    if (json_text) {
        root = cJSON_Parse(json_text);
        metadata = root ? cJSON_GetObjectItem(root, "metadata") : NULL;
        items = root ? cJSON_GetObjectItem(root, "items") : NULL;
        table = find_named_dataset_entry(items, "render_metrics_table_v2");
        row0 = table ? cJSON_GetObjectItem(table, "row0") : NULL;
        assert_true("animation_output_metrics_route_truth_native_diffuse_parse", root != NULL);
        assert_true("animation_output_metrics_route_truth_native_diffuse_row0", row0 != NULL);
        assert_true("animation_output_metrics_route_truth_native_diffuse_3d_mode",
                    cJSON_GetObjectItem(row0, "integrator_mode_3d") &&
                    cJSON_GetObjectItem(row0, "integrator_mode_3d")->valueint == 1);
        assert_true("animation_output_metrics_route_truth_native_diffuse_route_family",
                    cJSON_GetObjectItem(row0, "route_family") &&
                    cJSON_GetObjectItem(row0, "route_family")->valueint == 2);
        assert_true("animation_output_metrics_route_truth_native_diffuse_uses_3d_true",
                    cJSON_GetObjectItem(row0, "integrator_uses_3d_catalog") &&
                    cJSON_IsTrue(cJSON_GetObjectItem(row0, "integrator_uses_3d_catalog")));
        assert_true("animation_output_metrics_route_truth_native_diffuse_status_label",
                    metadata &&
                    cJSON_GetObjectItem(metadata, "integrator_status_label") &&
                    strcmp(cJSON_GetObjectItem(metadata, "integrator_status_label")->valuestring,
                           "integrator: 3D Diffuse Bounce") == 0);
        cJSON_Delete(root);
        root = NULL;
        free(json_text);
        json_text = NULL;
    }

    memset(&animSettings, 0, sizeof(animSettings));
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_FORWARD_LIGHT;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_MATERIAL;
    assert_true("animation_output_metrics_route_truth_native_material_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json_native, &summary));
    AnimationExportRenderMetricsDatasetIfEnabled();
    json_text = read_text_file_alloc(json_path, NULL);
    assert_true("animation_output_metrics_route_truth_native_material_json", json_text != NULL);
    if (json_text) {
        root = cJSON_Parse(json_text);
        metadata = root ? cJSON_GetObjectItem(root, "metadata") : NULL;
        items = root ? cJSON_GetObjectItem(root, "items") : NULL;
        table = find_named_dataset_entry(items, "render_metrics_table_v2");
        row0 = table ? cJSON_GetObjectItem(table, "row0") : NULL;
        assert_true("animation_output_metrics_route_truth_native_material_parse", root != NULL);
        assert_true("animation_output_metrics_route_truth_native_material_row0", row0 != NULL);
        assert_true("animation_output_metrics_route_truth_native_material_3d_mode",
                    cJSON_GetObjectItem(row0, "integrator_mode_3d") &&
                    cJSON_GetObjectItem(row0, "integrator_mode_3d")->valueint == 2);
        assert_true("animation_output_metrics_route_truth_native_material_route_family",
                    cJSON_GetObjectItem(row0, "route_family") &&
                    cJSON_GetObjectItem(row0, "route_family")->valueint == 2);
        assert_true("animation_output_metrics_route_truth_native_material_uses_3d_true",
                    cJSON_GetObjectItem(row0, "integrator_uses_3d_catalog") &&
                    cJSON_IsTrue(cJSON_GetObjectItem(row0, "integrator_uses_3d_catalog")));
        assert_true("animation_output_metrics_route_truth_native_material_status_label",
                    metadata &&
                    cJSON_GetObjectItem(metadata, "integrator_status_label") &&
                    strcmp(cJSON_GetObjectItem(metadata, "integrator_status_label")->valuestring,
                           "integrator: 3D Material") == 0);
        cJSON_Delete(root);
        root = NULL;
        free(json_text);
        json_text = NULL;
    }

    memset(&animSettings, 0, sizeof(animSettings));
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_FORWARD_LIGHT;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY;
    assert_true("animation_output_metrics_route_truth_native_emission_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json_native, &summary));
    AnimationExportRenderMetricsDatasetIfEnabled();
    json_text = read_text_file_alloc(json_path, NULL);
    assert_true("animation_output_metrics_route_truth_native_emission_json", json_text != NULL);
    if (json_text) {
        root = cJSON_Parse(json_text);
        metadata = root ? cJSON_GetObjectItem(root, "metadata") : NULL;
        items = root ? cJSON_GetObjectItem(root, "items") : NULL;
        table = find_named_dataset_entry(items, "render_metrics_table_v2");
        row0 = table ? cJSON_GetObjectItem(table, "row0") : NULL;
        assert_true("animation_output_metrics_route_truth_native_emission_parse", root != NULL);
        assert_true("animation_output_metrics_route_truth_native_emission_row0", row0 != NULL);
        assert_true("animation_output_metrics_route_truth_native_emission_3d_mode",
                    cJSON_GetObjectItem(row0, "integrator_mode_3d") &&
                    cJSON_GetObjectItem(row0, "integrator_mode_3d")->valueint == 3);
        assert_true("animation_output_metrics_route_truth_native_emission_route_family",
                    cJSON_GetObjectItem(row0, "route_family") &&
                    cJSON_GetObjectItem(row0, "route_family")->valueint == 2);
        assert_true("animation_output_metrics_route_truth_native_emission_uses_3d_true",
                    cJSON_GetObjectItem(row0, "integrator_uses_3d_catalog") &&
                    cJSON_IsTrue(cJSON_GetObjectItem(row0, "integrator_uses_3d_catalog")));
        assert_true("animation_output_metrics_route_truth_native_emission_status_label",
                    metadata &&
                    cJSON_GetObjectItem(metadata, "integrator_status_label") &&
                    strcmp(cJSON_GetObjectItem(metadata, "integrator_status_label")->valuestring,
                           "integrator: 3D Emission / Transparency") == 0);
        cJSON_Delete(root);
        root = NULL;
        free(json_text);
        json_text = NULL;
    }

    memset(&animSettings, 0, sizeof(animSettings));
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_FORWARD_LIGHT;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DISNEY;
    assert_true("animation_output_metrics_route_truth_compat_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json_compat, &summary));
    AnimationExportRenderMetricsDatasetIfEnabled();
    json_text = read_text_file_alloc(json_path, NULL);
    assert_true("animation_output_metrics_route_truth_compat_json", json_text != NULL);
    if (json_text) {
        root = cJSON_Parse(json_text);
        metadata = root ? cJSON_GetObjectItem(root, "metadata") : NULL;
        items = root ? cJSON_GetObjectItem(root, "items") : NULL;
        table = find_named_dataset_entry(items, "render_metrics_table_v2");
        row0 = table ? cJSON_GetObjectItem(table, "row0") : NULL;
        assert_true("animation_output_metrics_route_truth_compat_parse", root != NULL);
        assert_true("animation_output_metrics_route_truth_compat_row0", row0 != NULL);
        assert_true("animation_output_metrics_route_truth_compat_3d_mode",
                    cJSON_GetObjectItem(row0, "integrator_mode_3d") &&
                    cJSON_GetObjectItem(row0, "integrator_mode_3d")->valueint == 0);
        assert_true("animation_output_metrics_route_truth_compat_route_family",
                    cJSON_GetObjectItem(row0, "route_family") &&
                    cJSON_GetObjectItem(row0, "route_family")->valueint == 1);
        assert_true("animation_output_metrics_route_truth_compat_uses_3d_true",
                    cJSON_GetObjectItem(row0, "integrator_uses_3d_catalog") &&
                    cJSON_IsTrue(cJSON_GetObjectItem(row0, "integrator_uses_3d_catalog")));
        assert_true("animation_output_metrics_route_truth_compat_status_label",
                    metadata &&
                    cJSON_GetObjectItem(metadata, "integrator_status_label") &&
                    strcmp(cJSON_GetObjectItem(metadata, "integrator_status_label")->valuestring,
                           "integrator fallback: compat 3D Direct Light") == 0);
        cJSON_Delete(root);
        root = NULL;
        free(json_text);
        json_text = NULL;
    }

    unlink(json_path);
    restore_env_or_unset("RAY_TRACING_EXPORT_RENDER_METRICS_DATASET", export_env_backup);
    restore_env_or_unset("RAY_TRACING_RENDER_METRICS_DATASET_PATH", path_env_backup);
    free(export_env_backup);
    free(path_env_backup);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    frameCounter = saved_frame_counter;
    loopCount = saved_loop_count;
    currentTime = saved_current_time;
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
    bool ok = runtime_scene_bridge_apply_file("third_party/codework_shared/assets/scenes/trio_contract/scene_runtime_min.json",
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
                       "third_party/codework_shared/assets/scenes/trio_contract/scene_runtime_min.json") == 0);

    // Regression: menu apply path passes animSettings.runtimeScenePath as input.
    ok = runtime_scene_bridge_apply_file(animSettings.runtimeScenePath, &alias_summary);
    assert_true("runtime_scene_apply_alias_buffer_ok", ok);
    if (ok) {
        assert_true("runtime_scene_apply_alias_buffer_valid_contract", alias_summary.valid_contract);
        assert_true("runtime_scene_apply_alias_buffer_path_retained",
                    strcmp(animSettings.runtimeScenePath,
                           "third_party/codework_shared/assets/scenes/trio_contract/scene_runtime_min.json") == 0);
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

static int test_path_normalized_spacing_preserves_tail_motion(void) {
    Path path = {0};
    int original_space_mode = animSettings.spaceMode;
    const double t_values[] = {0.80, 0.85, 0.90, 0.95, 1.00};
    double min_step = 1e9;
    double max_step = 0.0;

    path.mode = BEZIER_CUBIC;
    path.numPoints = 3;
    path.points[0] = (Point){0.0, 0.0};
    path.points[1] = (Point){12.0, 0.0};
    path.points[2] = (Point){20.0, 0.0};
    path.handles[0][0] = (Velocity){4.0, 0.0};
    path.handles[0][1] = (Velocity){-4.0, 0.0};
    path.handles[1][0] = (Velocity){0.0, 40.0};
    path.handles[1][1] = (Velocity){0.0, -40.0};

    animSettings.spaceMode = SPACE_MODE_3D;
    for (int i = 0; i < 4; ++i) {
        double global_a = PathResolveNormalizedGlobalT(&path, t_values[i]);
        double global_b = PathResolveNormalizedGlobalT(&path, t_values[i + 1]);
        Point previous = GetPositionAlongPath(&path, global_a);
        double step = 0.0;
        const int integration_steps = 256;
        for (int sample = 1; sample <= integration_steps; ++sample) {
            double alpha = (double)sample / (double)integration_steps;
            double raw_t = global_a + (global_b - global_a) * alpha;
            Point current = GetPositionAlongPath(&path, raw_t);
            double dx = current.x - previous.x;
            double dy = current.y - previous.y;
            step += sqrt(dx * dx + dy * dy);
            previous = current;
        }
        if (step < min_step) min_step = step;
        if (step > max_step) max_step = step;
    }

    assert_true("path_norm_tail_steps_nonzero", min_step > 0.1);
    assert_true("path_norm_tail_uniformity", max_step / min_step < 1.2);

    animSettings.spaceMode = original_space_mode;
    return 0;
}

static int test_path_traversal_endpoints_follow_sampled_contract(void) {
    Path path = {0};
    CameraPath3D path3d = {0};
    PathTraversalEndpoints endpoints = {0};
    bool ok = false;
    int original_space_mode = animSettings.spaceMode;

    path.mode = BEZIER_CUBIC;
    path.numPoints = 3;
    path.points[0] = (Point){2.0, 3.0};
    path.points[1] = (Point){8.0, 11.0};
    path.points[2] = (Point){17.0, 19.0};
    path.handles[0][0] = (Velocity){1.5, 2.0};
    path.handles[0][1] = (Velocity){-1.0, -1.5};
    path.handles[1][0] = (Velocity){2.5, -0.5};
    path.handles[1][1] = (Velocity){-2.0, 1.0};
    path3d.point_z[0] = 4.0;
    path3d.point_z[1] = 9.0;
    path3d.point_z[2] = 13.0;
    path3d.handles_vz[0][0] = 1.0;
    path3d.handles_vz[0][1] = -1.5;
    path3d.handles_vz[1][0] = 0.5;
    path3d.handles_vz[1][1] = -0.75;

    animSettings.spaceMode = SPACE_MODE_3D;
    ok = PathResolveTraversalEndpoints(&path, &path3d, &endpoints);

    assert_true("path_traversal_endpoints_resolved", ok);
    assert_true("path_traversal_start_index", endpoints.start_point_index == 0);
    assert_true("path_traversal_end_index", endpoints.end_point_index == 2);
    assert_true("path_traversal_has_z", endpoints.has_z);
    assert_close("path_traversal_start_x", endpoints.start_xy.x, path.points[0].x, 1e-9);
    assert_close("path_traversal_start_y", endpoints.start_xy.y, path.points[0].y, 1e-9);
    assert_close("path_traversal_end_x", endpoints.end_xy.x, path.points[2].x, 1e-9);
    assert_close("path_traversal_end_y", endpoints.end_xy.y, path.points[2].y, 1e-9);
    assert_close("path_traversal_start_z", endpoints.start_z, path3d.point_z[0], 1e-9);
    assert_close("path_traversal_end_z", endpoints.end_z, path3d.point_z[2], 1e-9);

    animSettings.spaceMode = original_space_mode;
    return 0;
}

static int test_camera_path_default_preserves_empty_authored_state(void) {
    SceneConfig scene = {0};
    struct json_object* root = json_object_new_object();
    struct json_object* camera_path = json_object_new_object();
    struct json_object* points = json_object_new_array();
    bool loaded = false;

    scene.camera.x = 12.0;
    scene.camera.y = -4.0;
    scene.cameraZ = 7.5;
    config_scene_ensure_camera_path_default(&scene);
    assert_true("camera_path_default_empty_points", scene.cameraPath.numPoints == 0);

    json_object_object_add(camera_path, "mode", json_object_new_string("BEZIER_CUBIC"));
    json_object_object_add(camera_path, "points", points);
    json_object_object_add(root, "cameraPath", camera_path);
    loaded = config_scene_load_camera_path_from_json(root, "cameraPath", &scene.cameraPath);
    assert_true("camera_path_load_empty_allowed", loaded);
    assert_true("camera_path_load_empty_points", scene.cameraPath.numPoints == 0);

    json_object_put(root);
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
    char *authoring_json = read_text_file_alloc("third_party/codework_shared/assets/scenes/trio_contract/scene_authoring_interop_min.json",
                                                &authoring_size);
    char *overlay_json = read_text_file_alloc("third_party/codework_shared/assets/scenes/trio_contract/ray_overlay_min.json",
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

static int test_scene_editor_runtime_scene_persistence_roundtrip_object_materials(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_path = "/tmp/ray_tracing_runtime_scene_authoring_material_roundtrip.json";
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_authoring_material_persist_1\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[{"
          "\"object_id\":\"box_a\","
          "\"object_type\":\"rect_prism_primitive\","
          "\"transform\":{"
            "\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
            "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}"
          "},"
          "\"primitive\":{"
            "\"kind\":\"rect_prism_primitive\","
            "\"width\":1.0,"
            "\"height\":1.0,"
            "\"depth\":1.0"
          "},"
          "\"flags\":{\"visible\":true}"
        "}],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    char diagnostics[256];
    char *persisted_json = NULL;
    FILE *file = fopen(runtime_path, "wb");
    RuntimeSceneBridgePreflight summary = {0};
    bool ok = false;

    assert_true("runtime_scene_authoring_material_persist_open_tmp", file != NULL);
    if (!file) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    fwrite(runtime_json, 1, strlen(runtime_json), file);
    fclose(file);

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    ok = runtime_scene_bridge_apply_file(runtime_path, &summary);
    assert_true("runtime_scene_authoring_material_persist_apply_ok", ok);
    if (!ok) {
        unlink(runtime_path);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath), "%s", runtime_path);
    sceneSettings.sceneObjects[0].material_id = 3;

    ok = SceneEditorRuntimeScenePersistAuthoring(diagnostics, sizeof(diagnostics));
    assert_true("runtime_scene_authoring_material_persist_writeback_ok", ok);
    if (!ok) {
        unlink(runtime_path);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    persisted_json = read_text_file_alloc(runtime_path, NULL);
    assert_true("runtime_scene_authoring_material_persist_readback_ok", persisted_json != NULL);
    if (persisted_json) {
        assert_true("runtime_scene_authoring_material_persist_has_object_materials",
                    strstr(persisted_json, "\"object_materials\"") != NULL);
        assert_true("runtime_scene_authoring_material_persist_has_object_id",
                    strstr(persisted_json, "\"box_a\"") != NULL);
        assert_true("runtime_scene_authoring_material_persist_has_material_id",
                    strstr(persisted_json, "\"material_id\":3") != NULL);
    }

    assert_true("runtime_scene_authoring_material_persist_hydrated_material_id",
                sceneSettings.sceneObjects[0].material_id == 3);

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
    assert_true("route2d_not_3d_catalog", !route.integratorUses3DCatalog);
    assert_true("route2d_status_label_hybrid",
                strstr(RayTracingModeBackend_IntegratorStatusLabel(&route), "Hybrid") != NULL);
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
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_HYBRID;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DISNEY;
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
    assert_true("route3d_compat_forces_direct_light_legacy_mode",
                route.integratorMode == RAY_TRACING_2D_INTEGRATOR_DIRECT_LIGHT);
    assert_true("route3d_compat_forces_direct_light_3d_mode",
                route.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT);
    assert_true("route3d_compat_uses_3d_catalog", route.integratorUses3DCatalog);
    assert_true("route3d_compat_cache_off", !route.buildIrradianceCache);
    assert_true("route3d_tile_preview_off", !route.tilePreviewEnabled);
    assert_true("route3d_compat_status_label",
                strstr(RayTracingModeBackend_IntegratorStatusLabel(&route), "compat 3D Direct Light") != NULL);
    return 0;
}

static int test_mode_backend_route_3d_native_lane(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json_route_3d_native =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_route_3d_native\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"floor\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":6.0,\"height\":6.0,"
              "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_v\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
              "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"block\","
            "\"object_type\":\"rect_prism_primitive\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-3.0,\"z\":0.5},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"primitive\":{\"kind\":\"rect_prism_primitive\","
              "\"width\":2.0,\"height\":2.0,\"depth\":1.0}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":1.0,\"y\":-1.5,\"z\":2.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":2.0,\"z\":8.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RayTracingRuntimeRoute route;

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_FORWARD_LIGHT;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DISNEY;
    animSettings.useTiledRenderer = true;
    animSettings.tileSize = 16;
    animSettings.tilePreviewEnabled = true;
    assert_true("route3d_native_seed_runtime_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json_route_3d_native, &summary));

    route = RayTracingModeBackend_ResolveRoute();

    assert_true("route3d_native_family_native",
                route.routeFamily == RAY_TRACING_ROUTE_NATIVE_3D);
    assert_true("route3d_native_not_canonical_helper",
                !RayTracingModeBackend_IsCanonical2D(&route));
    assert_true("route3d_native_not_compat_helper",
                !RayTracingModeBackend_IsCompat3DFallback(&route));
    assert_true("route3d_native_helper",
                RayTracingModeBackend_IsNative3D(&route));
    assert_true("route3d_native_controlled_helper",
                RayTracingModeBackend_IsControlled3D(&route));
    assert_true("route3d_native_lane_controlled",
                route.backendLane == RAY_TRACING_BACKEND_CONTROLLED_3D);
    assert_true("route3d_native_no_fallback_projection",
                !route.fallbackTo2DProjection);
    assert_true("route3d_native_projection_mode_3d",
                route.projectionMode == SPACE_MODE_3D);
    assert_true("route3d_native_runtime_scaffold_enabled",
                route.usesRuntime3DScaffold);
    assert_close("route3d_native_runtime_camera_z", route.runtimeCameraZ, 8.0, 1e-9);
    assert_close("route3d_native_ray_origin_y_offset_zero", route.rayOriginYOffset, 0.0, 1e-9);
    assert_true("route3d_native_scaffold_primitive_count", route.scaffoldPrimitiveCount == 2);
    assert_true("route3d_native_legacy_mode_direct_light",
                route.integratorMode == RAY_TRACING_2D_INTEGRATOR_DIRECT_LIGHT);
    assert_true("route3d_native_3d_mode_direct_light",
                route.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT);
    assert_true("route3d_native_uses_3d_catalog", route.integratorUses3DCatalog);
    assert_true("route3d_native_cache_off", !route.buildIrradianceCache);
    assert_true("route3d_native_tiles_enabled", route.useTiles);
    assert_true("route3d_native_tile_preview_on", route.tilePreviewEnabled);
    assert_true("route3d_native_status_label_reserved_clamp",
                strstr(RayTracingModeBackend_IntegratorStatusLabel(&route), "reserved 3D") != NULL);

    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE;
    route = RayTracingModeBackend_ResolveRoute();
    assert_true("route3d_native_3d_mode_diffuse_bounce",
                route.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE);
    assert_true("route3d_native_status_label_diffuse_bounce",
                strstr(RayTracingModeBackend_IntegratorStatusLabel(&route),
                       "3D Diffuse Bounce") != NULL);

    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY;
    route = RayTracingModeBackend_ResolveRoute();
    assert_true("route3d_native_3d_mode_emission_transparency",
                route.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY);
    assert_true("route3d_native_status_label_emission_transparency",
                strstr(RayTracingModeBackend_IntegratorStatusLabel(&route),
                       "3D Emission / Transparency") != NULL);

    sceneSettings = saved_scene;
    animSettings = saved_anim;
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

static int test_mode_backend_scene_digest_status_3d_native_fixture(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json_route_3d_native =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_route_3d_native_digest\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"floor\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":6.0,\"height\":6.0,"
              "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_v\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
              "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"block\","
            "\"object_type\":\"rect_prism_primitive\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-3.0,\"z\":0.5},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"primitive\":{\"kind\":\"rect_prism_primitive\","
              "\"width\":2.0,\"height\":2.0,\"depth\":1.0}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":1.0,\"y\":-1.5,\"z\":2.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":2.0,\"z\":8.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RayTracingRuntimeRoute route;
    RayTracingSceneDigestStatus status;

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    assert_true("digestnative_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json_route_3d_native, &summary));

    route = RayTracingModeBackend_ResolveRoute();
    status = RayTracingModeBackend_BuildSceneDigestStatus(&route);

    assert_true("digestnative_route_native",
                route.routeFamily == RAY_TRACING_ROUTE_NATIVE_3D);
    assert_true("digestnative_status_valid", status.valid);
    assert_true("digestnative_scaffold_count_two", status.scaffoldPrimitiveCount == 2);
    assert_true("digestnative_primitive_count_two", status.digestPrimitiveCount == 2);
    assert_true("digestnative_plane_count_one", status.planePrimitiveCount == 1);
    assert_true("digestnative_prism_count_one", status.rectPrismPrimitiveCount == 1);

    sceneSettings = saved_scene;
    animSettings = saved_anim;
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

static int test_runtime_scene_3d_r0_scope_contract_defaults(void) {
    RuntimeScene3D scene;

    RuntimeScene3D_Init(&scene);

    assert_true("runtime_scene_3d_scope_plane_enabled", scene.scope.planeEnabled);
    assert_true("runtime_scene_3d_scope_rect_prism_enabled", scene.scope.rectPrismEnabled);
    assert_true("runtime_scene_3d_scope_triangle_mesh_disabled", !scene.scope.triangleMeshEnabled);
    assert_true("runtime_scene_3d_kind_plane_supported",
                RuntimePrimitive3DKindSupportedByR0(RUNTIME_PRIMITIVE_3D_KIND_PLANE));
    assert_true("runtime_scene_3d_kind_rect_prism_supported",
                RuntimePrimitive3DKindSupportedByR0(RUNTIME_PRIMITIVE_3D_KIND_RECT_PRISM));
    assert_true("runtime_scene_3d_kind_triangle_mesh_not_supported",
                !RuntimePrimitive3DKindSupportedByR0(RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH));
    assert_true("runtime_scene_3d_kind_label_plane",
                strcmp(RuntimePrimitive3DKindLabel(RUNTIME_PRIMITIVE_3D_KIND_PLANE), "plane") == 0);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_scene_3d_r0_ownership_contract_defaults(void) {
    RuntimeScene3D scene;

    RuntimeScene3D_Init(&scene);

    assert_true("runtime_scene_3d_ownership_renderer_truth",
                scene.ownership.rendererOwnsGeometryTruth);
    assert_true("runtime_scene_3d_ownership_scene_objects_compat_only",
                scene.ownership.sceneObjectsRemainCompatOnly);
    assert_true("runtime_scene_3d_ownership_preview_digest_non_authoritative",
                scene.ownership.previewDigestIsNonAuthoritative);
    assert_true("runtime_scene_3d_camera_default_zoom",
                fabs(scene.camera.zoom - 1.0) < 1e-9);
    assert_true("runtime_scene_3d_camera_default_near_plane_positive",
                scene.camera.nearPlane > 0.0);

    RuntimeScene3D_Free(&scene);
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
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json_route_3d_compat =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_router_3d_compat\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"obj_box\","
            "\"object_type\":\"box\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":20.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = 1;
    assert_true("router3d_compat_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json_route_3d_compat, &summary));

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

    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_editor_mode_router_capabilities_3d_native(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json_route_3d_native =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_router_3d_native\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"floor\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":6.0,\"height\":6.0,"
              "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_v\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
              "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":1.0,\"y\":-1.5,\"z\":2.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":2.0,\"z\":8.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    EditorModeCapabilities caps;

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = 1;
    assert_true("router3d_native_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json_route_3d_native, &summary));

    caps = EditorModeRouter_GetCapabilities();
    assert_true("router3d_native_controlled", caps.isControlled3D);
    assert_true("router3d_native_no_projection_fallback", !caps.uses2DProjectionFallback);
    assert_true("router3d_native_label_native",
                strstr(EditorModeRouter_SpaceButtonLabel(), "Native") != NULL);
    assert_true("router3d_native_hint_native",
                strstr(EditorModeRouter_RuntimeHintLabel(), "native route active") != NULL);

    sceneSettings = saved_scene;
    animSettings = saved_anim;
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
    input.digestStatus.valid = true;
    input.digestStatus.digestPrimitiveCount = 3;
    input.digestStatus.planePrimitiveCount = 1;
    input.digestStatus.rectPrismPrimitiveCount = 2;
    input.digestStatus.hasSceneBounds = true;
    input.digestStatus.boundsEnabled = true;
    snprintf(input.digestStatus.constructionPlaneAxis,
             sizeof(input.digestStatus.constructionPlaneAxis),
             "z");
    input.digestStatus.constructionPlaneOffset = 0.0;
    input.digestStatus.scaffoldPrimitiveCount = 3;

    SceneEditorControlSurfaceBuild(&input, &contract);

    assert_true("surface_native_lane_enum",
                contract.lane == SCENE_EDITOR_CONTROL_SURFACE_LANE_CONTROLLED_3D);
    assert_true("surface_native_digest_available",
                strstr(contract.statusDigest, "Digest: prim=3") != NULL);
    assert_true("surface_native_runtime_retained",
                strstr(contract.statusRuntime, "retained digest viewport controls") != NULL);
    assert_true("surface_native_preview_enabled", contract.previewEnabled);
    assert_true("surface_native_frame_enabled", contract.laneKeyFrameEnabled);
    assert_true("surface_native_orbit_enabled", contract.laneGestureOrbitEnabled);
    assert_true("surface_native_wheel_enabled", contract.laneWheelZoomEnabled);
    assert_true("surface_native_bezier_canvas_disabled", !contract.laneBezierCanvasEditEnabled);
    assert_true("surface_native_object_canvas_disabled", !contract.laneObjectCanvasEditEnabled);
    assert_true("surface_native_camera_canvas_enabled", contract.laneCameraCanvasEditEnabled);
    assert_true("surface_native_viewport_bezier_disabled", !contract.laneViewportBezierPlacementEnabled);
    assert_true("surface_native_viewport_pick_disabled", !contract.laneViewportObjectPickEnabled);
    assert_true("surface_native_canvas_enabled", contract.laneCanvasEditEnabled);
    assert_true("surface_native_controls_active",
                strstr(contract.statusControls, "Alt+drag orbit") != NULL);
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
    assert_true("surface_controlled3d_camera_canvas_enabled",
                contract.laneCameraCanvasEditEnabled);
    assert_true("surface_controlled3d_viewport_bezier_disabled_in_camera_mode",
                !contract.laneViewportBezierPlacementEnabled);
    assert_true("surface_controlled3d_viewport_pick_disabled_in_camera_mode",
                !contract.laneViewportObjectPickEnabled);
    assert_true("surface_controlled3d_viewport_camera_enabled",
                contract.laneViewportCameraPlacementEnabled);
    assert_true("surface_controlled3d_canvas_enabled",
                contract.laneCanvasEditEnabled);
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

static void test_preview_camera_sample_evaluate_contract(void) {
    Camera base_camera = {.x = 9.0, .y = -4.0, .zoom = 1.0, .rotation = 0.25};
    PreviewCameraSample sample = {0};
    Path camera_path = {0};
    CameraPath3D camera_path3d = {0};

    assert_true("preview_camera_sample_base",
                PreviewCameraSampleEvaluate(&base_camera,
                                            3.5,
                                            NULL,
                                            NULL,
                                            0.25,
                                            1600,
                                            900,
                                            &sample));
    assert_true("preview_camera_sample_base_valid", sample.valid);
    assert_true("preview_camera_sample_base_no_path", !sample.uses_authored_path);
    assert_close("preview_camera_sample_base_x", sample.position_x, 9.0, 1e-6);
    assert_close("preview_camera_sample_base_y", sample.position_y, -4.0, 1e-6);
    assert_close("preview_camera_sample_base_z", sample.position_z, 3.5, 1e-6);
    assert_close("preview_camera_sample_base_yaw", sample.yaw_radians, 0.25, 1e-6);
    assert_close("preview_camera_sample_base_pitch", sample.pitch_radians, 0.0, 1e-6);
    assert_close("preview_camera_sample_base_fov", sample.fov_y_degrees,
                 PREVIEW_CAMERA_SAMPLE_DEFAULT_FOV_Y_DEGREES, 1e-6);
    assert_close("preview_camera_sample_base_aspect", sample.aspect_ratio, 1600.0 / 900.0, 1e-6);

    camera_path.numPoints = 2;
    camera_path.mode = BEZIER_CUBIC;
    camera_path.points[0] = (Point){0.0, 0.0};
    camera_path.points[1] = (Point){10.0, 0.0};
    camera_path.rotations[0] = 0.0;
    camera_path.rotations[1] = M_PI / 2.0;
    camera_path3d.point_z[0] = 0.0;
    camera_path3d.point_z[1] = 10.0;
    camera_path3d.point_pitch[0] = 0.0;
    camera_path3d.point_pitch[1] = M_PI / 4.0;

    assert_true("preview_camera_sample_path",
                PreviewCameraSampleEvaluate(&base_camera,
                                            3.5,
                                            &camera_path,
                                            &camera_path3d,
                                            0.5,
                                            1200,
                                            800,
                                            &sample));
    assert_true("preview_camera_sample_path_authored", sample.uses_authored_path);
    {
        Point expected_point = GetPositionAlongPathNormalized(&camera_path, 0.5);
        double expected_yaw = GetRotationAlongPathNormalized(&camera_path, 0.5);
        double expected_z =
            CameraPath3D_GetPositionZNormalized(&camera_path, &camera_path3d, 0.5);
        assert_close("preview_camera_sample_path_x", sample.position_x, expected_point.x, 1e-6);
        assert_close("preview_camera_sample_path_y", sample.position_y, expected_point.y, 1e-6);
        assert_close("preview_camera_sample_path_z", sample.position_z, expected_z, 1e-6);
        assert_close("preview_camera_sample_path_yaw", sample.yaw_radians, expected_yaw, 1e-6);
    }
    assert_true("preview_camera_sample_path_pitch_bounds",
                sample.pitch_radians > 0.0 && sample.pitch_radians < (M_PI / 4.0));
    assert_close("preview_camera_sample_path_aspect", sample.aspect_ratio, 1.5, 1e-6);
}

static void test_preview_camera_projector_projection_contract(void) {
    PreviewCameraSample sample = {0};
    PreviewCameraProjector projector = {0};
    SDL_Rect viewport = {0, 0, 1000, 500};
    double sx = 0.0;
    double sy = 0.0;
    double depth = 0.0;
    double cx = 0.0;
    double cy = 0.0;
    double cz = 0.0;
    bool inside = false;

    sample.valid = true;
    sample.position_x = 0.0;
    sample.position_y = 0.0;
    sample.position_z = 0.0;
    sample.yaw_radians = 0.0;
    sample.pitch_radians = 0.0;
    sample.fov_y_degrees = 60.0;
    sample.aspect_ratio = 2.0;

    assert_true("preview_camera_projector_build",
                PreviewCameraProjectorBuild(&sample, viewport, &projector));
    assert_close("preview_camera_projector_forward_x", projector.forward_x, 0.0, 1e-6);
    assert_close("preview_camera_projector_forward_y", projector.forward_y, -1.0, 1e-6);
    assert_close("preview_camera_projector_forward_z", projector.forward_z, 0.0, 1e-6);
    assert_close("preview_camera_projector_right_x", projector.right_x, 1.0, 1e-6);
    assert_close("preview_camera_projector_right_y", projector.right_y, 0.0, 1e-6);
    assert_close("preview_camera_projector_right_z", projector.right_z, 0.0, 1e-6);
    assert_close("preview_camera_projector_up_x", projector.up_x, 0.0, 1e-6);
    assert_close("preview_camera_projector_up_y", projector.up_y, 0.0, 1e-6);
    assert_close("preview_camera_projector_up_z", projector.up_z, 1.0, 1e-6);

    PreviewCameraProjectorWorldToCamera(&projector, 0.0, -10.0, 0.0, &cx, &cy, &cz);
    assert_close("preview_camera_projector_cam_forward_x", cx, 0.0, 1e-6);
    assert_close("preview_camera_projector_cam_forward_y", cy, 0.0, 1e-6);
    assert_close("preview_camera_projector_cam_forward_z", cz, 10.0, 1e-6);

    assert_true("preview_camera_projector_project_center",
                PreviewCameraProjectorProjectPoint(&projector,
                                                   0.0,
                                                   -10.0,
                                                   0.0,
                                                   &sx,
                                                   &sy,
                                                   &depth,
                                                   &inside));
    assert_close("preview_camera_projector_center_x", sx, 500.0, 1e-6);
    assert_close("preview_camera_projector_center_y", sy, 250.0, 1e-6);
    assert_close("preview_camera_projector_center_depth", depth, 10.0, 1e-6);
    assert_true("preview_camera_projector_center_inside", inside);

    assert_true("preview_camera_projector_project_screen_right",
                PreviewCameraProjectorProjectPoint(&projector,
                                                   10.0,
                                                   -10.0,
                                                   0.0,
                                                   &sx,
                                                   &sy,
                                                   &depth,
                                                   &inside));
    assert_true("preview_camera_projector_screen_right_x", sx > 500.0);
    assert_close("preview_camera_projector_screen_right_y", sy, 250.0, 1e-6);
    assert_true("preview_camera_projector_screen_right_inside", inside);

    assert_true("preview_camera_projector_project_screen_top",
                PreviewCameraProjectorProjectPoint(&projector,
                                                   0.0,
                                                   -10.0,
                                                   5.0,
                                                   &sx,
                                                   &sy,
                                                   &depth,
                                                   &inside));
    assert_true("preview_camera_projector_screen_top_y", sy < 250.0);
    assert_true("preview_camera_projector_screen_top_inside", inside);

    inside = true;
    assert_true("preview_camera_projector_reject_behind",
                !PreviewCameraProjectorProjectPoint(&projector,
                                                    0.0,
                                                    10.0,
                                                    0.0,
                                                    &sx,
                                                    &sy,
                                                    &depth,
                                                    &inside));
    assert_true("preview_camera_projector_reject_behind_inside", !inside);
}

static void test_preview_retained_scene_line_segments_contract(void) {
    RuntimeSceneBridge3DDigestState digest = {0};
    PreviewRetainedSceneLineSegment segments[PREVIEW_RETAINED_SCENE_MAX_LINE_SEGMENTS];
    int count = 0;

    digest.valid = true;
    digest.has_scene_bounds = true;
    digest.bounds_enabled = true;
    digest.bounds_min_x = -6.0;
    digest.bounds_min_y = -5.0;
    digest.bounds_min_z = -1.0;
    digest.bounds_max_x = 6.0;
    digest.bounds_max_y = 5.0;
    digest.bounds_max_z = 5.5;
    digest.has_construction_plane = true;
    digest.construction_plane_offset = -1.0;
    digest.primitive_count = 2;
    digest.primitives[0].kind = RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE;
    digest.primitives[0].origin_x = 0.0;
    digest.primitives[0].origin_y = 0.0;
    digest.primitives[0].origin_z = -1.0;
    digest.primitives[0].has_dimensions = true;
    digest.primitives[0].width = 8.0;
    digest.primitives[0].height = 6.0;
    digest.primitives[1].kind = RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM;
    digest.primitives[1].origin_x = 1.0;
    digest.primitives[1].origin_y = 2.0;
    digest.primitives[1].origin_z = 1.5;
    digest.primitives[1].has_dimensions = true;
    digest.primitives[1].width = 2.0;
    digest.primitives[1].height = 3.0;
    digest.primitives[1].depth = 4.0;

    count = PreviewRetainedSceneBuildLineSegments(&digest,
                                                  segments,
                                                  PREVIEW_RETAINED_SCENE_MAX_LINE_SEGMENTS);
    assert_close("preview_retained_scene_line_count", (double)count, 32.0, 1e-6);
    assert_close("preview_retained_scene_bounds_first_ax", segments[0].ax, -6.0, 1e-6);
    assert_close("preview_retained_scene_bounds_first_ay", segments[0].ay, -5.0, 1e-6);
    assert_close("preview_retained_scene_bounds_first_az", segments[0].az, -1.0, 1e-6);
    assert_close("preview_retained_scene_plane_start", segments[12].az, -1.0, 1e-6);
    assert_close("preview_retained_scene_primitive_plane_z", segments[16].az, -1.0, 1e-6);
    assert_close("preview_retained_scene_prism_last_bz", segments[31].bz, 3.5, 1e-6);
}

static void test_preview_mode_route_select_contract(void) {
    RayTracingRuntimeRoute route = {0};
    RayTracingSceneDigestStatus digest_status = {0};
    PreviewModeRouteDecision decision = {0};

    route.requestedMode = SPACE_MODE_2D;
    route.routeFamily = RAY_TRACING_ROUTE_CANONICAL_2D;
    assert_true("preview_mode_route_2d",
                PreviewModeRouteSelect(&route, &digest_status, false, &decision));
    assert_close("preview_mode_route_2d_branch",
                 (double)decision.branch,
                 (double)PREVIEW_RENDER_BRANCH_LEGACY_2D,
                 1e-6);
    assert_true("preview_mode_route_2d_label",
                strstr(decision.branchLabel, "2D") != NULL);

    route.requestedMode = SPACE_MODE_3D;
    route.routeFamily = RAY_TRACING_ROUTE_COMPAT_3D_FALLBACK;
    digest_status.valid = false;
    assert_true("preview_mode_route_fallback",
                PreviewModeRouteSelect(&route, &digest_status, false, &decision));
    assert_close("preview_mode_route_fallback_branch",
                 (double)decision.branch,
                 (double)PREVIEW_RENDER_BRANCH_FALLBACK_2D,
                 1e-6);
    assert_true("preview_mode_route_fallback_status",
                strstr(decision.statusLine, "fallback") != NULL);

    digest_status.valid = true;
    digest_status.digestPrimitiveCount = 3;
    assert_true("preview_mode_route_retained",
                PreviewModeRouteSelect(&route, &digest_status, true, &decision));
    assert_close("preview_mode_route_retained_branch",
                 (double)decision.branch,
                 (double)PREVIEW_RENDER_BRANCH_RETAINED_3D,
                 1e-6);
    assert_true("preview_mode_route_retained_status",
                strstr(decision.statusLine, "primitives=3") != NULL);
}

static void test_preview_playback_evaluate_contract(void) {
    PreviewPlaybackSample sample = {0};

    assert_true("preview_playback_bounce_start",
                PreviewPlaybackEvaluate(0.0, 4.0, true, "stop", &sample));
    assert_close("preview_playback_bounce_start_t", sample.normalized_t, 0.0, 1e-6);
    assert_true("preview_playback_bounce_start_forward", !sample.reverse_direction);

    assert_true("preview_playback_bounce_mid",
                PreviewPlaybackEvaluate(2.0, 4.0, true, "stop", &sample));
    assert_close("preview_playback_bounce_mid_t", sample.normalized_t, 0.5, 1e-6);
    assert_true("preview_playback_bounce_mid_forward", !sample.reverse_direction);

    assert_true("preview_playback_bounce_reverse",
                PreviewPlaybackEvaluate(6.0, 4.0, true, "stop", &sample));
    assert_close("preview_playback_bounce_reverse_t", sample.normalized_t, 0.5, 1e-6);
    assert_true("preview_playback_bounce_reverse_dir", sample.reverse_direction);

    assert_true("preview_playback_loop_wrap",
                PreviewPlaybackEvaluate(5.0, 4.0, false, "loop", &sample));
    assert_close("preview_playback_loop_wrap_t", sample.normalized_t, 0.25, 1e-6);
    assert_true("preview_playback_loop_mode",
                sample.mode == PREVIEW_PLAYBACK_MODE_LOOP);

    assert_true("preview_playback_stop_clamp",
                PreviewPlaybackEvaluate(9.0, 4.0, false, "stop", &sample));
    assert_close("preview_playback_stop_clamp_t", sample.normalized_t, 1.0, 1e-6);
    assert_true("preview_playback_stop_clamped", sample.clamped);
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
    test_animation_integrator_split_roundtrip_and_default_3d();
    test_animation_scene_source_select_runtime_failure_rolls_back();
    test_animation_scene_source_select_runtime_persists_on_save();
    test_animation_apply_active_scene_source_invalid_fluid_falls_back_2d();
    test_animation_restore_active_scene_source_persists_fallback_correction();
    test_animation_video_output_root_migrates_from_output_root();
    test_data_paths_resolve_video_output_path_uses_configured_root();
    test_render_export_batch_counts_and_clears_frames();
    test_render_export_batch_make_video_rejects_empty_frame_dir();
    test_menu_batch_panel_click_starts_frame_dir_edit();
    test_menu_batch_panel_clear_button_updates_frame_count();
    test_menu_layout_builds_non_overlapping_primary_zones();
    test_menu_layout_keeps_manifest_dropdown_inside_left_panel();
    test_menu_button_layout_respects_owned_screen_zones();
    test_menu_batch_panel_layout_centers_inside_batch_zone();
    test_menu_batch_panel_header_does_not_overlap_route_rows();
    test_integrator_catalog_menu_routes_by_space_mode();
    test_integrator_catalog_cycle_preserves_inactive_mode();
    test_menu_fit_text_to_width_supports_in_place_buffer();
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
    test_runtime_scene_bridge_apply_ps4d_fixture_retains_primitive_seed_truth();
    test_runtime_scene_3d_builder_uses_retained_seed_scope();
    test_runtime_scene_3d_builder_builds_ps4d_triangle_scene();
    test_runtime_scene_3d_builder_promotes_authored_light_camera_samples();
    test_runtime_scene_3d_builder_falls_back_to_seeded_camera_state();
    test_runtime_ray_3d_triangle_intersection_contract();
    test_runtime_ray_3d_scene_first_hit_contract();
    test_runtime_ray_3d_offset_contract();
    test_runtime_visibility_3d_visible_contract();
    test_runtime_visibility_3d_blocked_contract();
    test_runtime_camera_projector_3d_center_ray_contract();
    test_runtime_camera_projector_3d_pitch_contract();
    test_runtime_camera_projector_3d_zoom_contract();
    test_runtime_camera_projector_3d_preview_projection_parity();
    test_material_manager_default_presets_include_i4_entries();
    test_material_manager_load_dir_preserves_shipped_preset_ids();
    test_runtime_material_payload_3d_scene_object_resolution_contract();
    test_runtime_material_payload_3d_hit_resolution_contract();
    test_runtime_direct_light_3d_shade_pixel_visible_contract();
    test_runtime_direct_light_3d_shade_pixel_shadowed_contract();
    test_runtime_direct_light_3d_authored_light_motion_contract();
    test_runtime_diffuse_bounce_3d_shadowed_hit_lift_contract();
    test_runtime_diffuse_bounce_3d_seed_branch_contract();
    test_runtime_material_response_3d_seed_branch_contract();
    test_runtime_emission_transparency_3d_seed_branch_contract();
    test_runtime_emission_transparency_3d_transmission_contract();
    test_runtime_emission_transparency_3d_transparent_prism_reaches_behind_surface();
    test_runtime_native_3d_render_live_buffer_contract();
    test_runtime_native_3d_render_prepared_region_parity();
    test_runtime_native_3d_tile_occupancy_contract();
    test_runtime_native_3d_dirty_rect_preview_base_parity();
    test_animation_output_render_metrics_route_truth_contract();
    test_scene_compile_and_preflight_roundtrip();
    test_runtime_scene_bridge_apply_runtime_fixture();
    test_path_eval_3d_uses_linear_handle_units();
    test_path_normalized_spacing_preserves_tail_motion();
    test_path_traversal_endpoints_follow_sampled_contract();
    test_camera_path_default_preserves_empty_authored_state();
    test_preview_camera_sample_evaluate_contract();
    test_preview_camera_projector_projection_contract();
    test_scene_editor_tool_state_contract();
    test_preview_retained_scene_line_segments_contract();
    test_preview_mode_route_select_contract();
    test_preview_playback_evaluate_contract();
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
    test_scene_editor_runtime_scene_persistence_roundtrip_object_materials();
    test_mode_backend_route_2d_defaults();
    test_mode_backend_route_3d_controlled_lane();
    test_mode_backend_route_3d_native_lane();
    test_mode_backend_scene_digest_status_2d_canonical_empty();
    test_mode_backend_scene_digest_status_ps4d_fixture();
    test_mode_backend_scene_digest_status_3d_native_fixture();
    test_mode_backend_view_carrier_2d_defaults();
    test_mode_backend_view_carrier_3d_compat_fallback();
    test_mode_backend_primitive_prep_plan_2d_defaults();
    test_mode_backend_primitive_prep_plan_3d_compat_placeholder();
    test_mode_backend_primitive_prep_plan_native3d_placeholder_contract();
    test_runtime_scene_3d_r0_scope_contract_defaults();
    test_runtime_scene_3d_r0_ownership_contract_defaults();
    test_editor_mode_router_capabilities_2d();
    test_editor_mode_router_capabilities_3d_scaffold();
    test_editor_mode_router_capabilities_3d_native();
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
