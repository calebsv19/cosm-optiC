#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "render/material_bsdf.h"
#include "render/fast_rng.h"
#include "config/config_manager.h"
#include "app/animation.h"
#include "editor/editor_mode_router.h"
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
#include "core_scene_compile.h"
#include "fluid_pack_import_test.h"
#include "kit_viz_fluid_overlay_adapter_test.h"
#include "render_metrics_dataset_test.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int failures = 0;
static const char* kRuntimeSceneConfigPath = "data/runtime/scene_config.json";

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
    assert_close("runtime_scene_apply_light_x", sceneSettings.bezierPath.points[0].x, 5.0, 1e-9);
    assert_close("runtime_scene_apply_light_y", sceneSettings.bezierPath.points[0].y, 8.0, 1e-9);
    assert_close("runtime_scene_apply_camera_x", sceneSettings.camera.x, 0.0, 1e-9);
    assert_close("runtime_scene_apply_camera_y", sceneSettings.camera.y, 0.0, 1e-9);
    assert_true("runtime_scene_apply_space_mode_2d", animSettings.spaceMode == SPACE_MODE_2D);
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

static int test_mode_backend_route_2d_defaults(void) {
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_2D;
    animSettings.integratorMode = 1;
    animSettings.useTiledRenderer = true;
    animSettings.tileSize = 12;
    animSettings.tilePreviewEnabled = true;

    RayTracingRuntimeRoute route = RayTracingModeBackend_ResolveRoute();

    assert_true("route2d_lane_canonical", route.backendLane == RAY_TRACING_BACKEND_CANONICAL_2D);
    assert_true("route2d_no_fallback", !route.fallbackTo2DProjection);
    assert_true("route2d_projection_2d", route.projectionMode == SPACE_MODE_2D);
    assert_true("route2d_tiles_enabled", route.useTiles);
    assert_true("route2d_tile_preview_enabled", route.tilePreviewEnabled);
    assert_true("route2d_cache_enabled", route.buildIrradianceCache);
    return 0;
}

static int test_mode_backend_route_3d_controlled_lane(void) {
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = 2;
    animSettings.useTiledRenderer = true;
    animSettings.tileSize = 16;
    animSettings.tilePreviewEnabled = true;

    RayTracingRuntimeRoute route = RayTracingModeBackend_ResolveRoute();

    assert_true("route3d_lane_controlled", route.backendLane == RAY_TRACING_BACKEND_CONTROLLED_3D);
    assert_true("route3d_fallback_projection", route.fallbackTo2DProjection);
    assert_true("route3d_projection_mode_2d", route.projectionMode == SPACE_MODE_2D);
    assert_true("route3d_integrator_preserved", route.integratorMode == 2);
    assert_true("route3d_tile_preview_off", !route.tilePreviewEnabled);
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
    assert_true("router3d_label_scaffold",
                strstr(EditorModeRouter_SpaceButtonLabel(), "Scaffold") != NULL);
    assert_true("router3d_hint_scaffold",
                strstr(EditorModeRouter_RuntimeHintLabel(), "3D scaffold") != NULL);
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
    test_depth_projection_scalars();
    test_runtime_scene_bridge_preflight_accepts_runtime_contract();
    test_runtime_scene_bridge_rejects_authoring_variant();
    test_scene_compile_and_preflight_roundtrip();
    test_runtime_scene_bridge_apply_runtime_fixture();
    test_runtime_scene_bridge_apply_compile_output();
    test_runtime_scene_bridge_writeback_overlay_preserves_non_ray_state();
    test_runtime_scene_bridge_writeback_rejects_foreign_extension_namespace();
    test_runtime_scene_bridge_writeback_rejects_forbidden_top_level_overlay_key();
    test_runtime_scene_bridge_writeback_rejects_invalid_space_mode_value();
    test_runtime_scene_bridge_writeback_rejects_non_object_ray_extension_payload();
    test_runtime_scene_bridge_writeback_rejects_missing_overlay_meta();
    test_runtime_scene_bridge_writeback_rejects_stale_logical_clock();
    test_runtime_scene_bridge_trio_fixture_compile_writeback_apply();
    test_mode_backend_route_2d_defaults();
    test_mode_backend_route_3d_controlled_lane();
    test_editor_mode_router_capabilities_2d();
    test_editor_mode_router_capabilities_3d_scaffold();
    test_editor_mode_router_cycle_policy();
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
