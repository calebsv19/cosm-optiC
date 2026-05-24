#include <math.h>
#include <png.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app/animation.h"
#include "editor/material_editor.h"
#include "editor/material_editor_face_preview.h"
#include "editor/material_preview_surface_eval.h"
#include "editor/object_editor.h"
#include "editor/object_editor_object_ops.h"
#include "editor/object_editor_selection_tracker.h"
#include "editor/editor_mode_router.h"
#include "editor/scene_editor_control_surface.h"
#include "editor/scene_editor_digest_overlay.h"
#include "editor/scene_editor_material_face_metrics.h"
#include "editor/scene_editor_material_face_placement.h"
#include "editor/scene_editor_material_preview.h"
#include "editor/scene_editor_material_stack.h"
#include "editor/scene_editor_runtime_scene_persistence.h"
#include "editor/scene_editor_tool_state.h"
#include "editor/scene_editor_viewport_nav.h"
#include "editor/scene_editor_viewport_render.h"
#include "import/runtime_scene_bridge.h"
#include "render/runtime_material_authored_texture_3d.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_material_texture_3d.h"
#include "test_runtime_scene_editor.h"
#include "test_support.h"

#include <json-c/json.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static bool test_scene_editor_write_png_rgba(const char* path,
                                             const unsigned char* rgba,
                                             unsigned width,
                                             unsigned height) {
    FILE* png_file = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    bool ok = false;
    if (!path || !rgba || width == 0u || height == 0u) {
        return false;
    }
    png_file = fopen(path, "wb");
    if (!png_file) {
        return false;
    }
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fclose(png_file);
        return false;
    }
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(png_file);
        return false;
    }
    if (setjmp(png_jmpbuf(png_ptr))) {
        ok = false;
        goto cleanup;
    }
    png_init_io(png_ptr, png_file);
    png_set_IHDR(png_ptr,
                 info_ptr,
                 width,
                 height,
                 8,
                 PNG_COLOR_TYPE_RGBA,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);
    for (unsigned y = 0u; y < height; ++y) {
        png_write_row(png_ptr, rgba + ((size_t)y * (size_t)width * 4u));
    }
    png_write_end(png_ptr, info_ptr);
    ok = true;
cleanup:
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(png_file);
    return ok;
}

static bool test_scene_editor_add_surface_semantic_fields(json_object* surface,
                                                          const char* primitive_kind,
                                                          const char* face_role) {
    static const char* kPlaneAdjacentRoles[4] = {"NONE", "NONE", "NONE", "NONE"};
    static const char* kFrontAdjacentRoles[4] = {"TOP", "RIGHT", "BOTTOM", "LEFT"};
    static const char* kBackAdjacentRoles[4] = {"TOP", "LEFT", "BOTTOM", "RIGHT"};
    static const char* kLeftAdjacentRoles[4] = {"TOP", "FRONT", "BOTTOM", "BACK"};
    static const char* kRightAdjacentRoles[4] = {"TOP", "BACK", "BOTTOM", "FRONT"};
    static const char* kTopAdjacentRoles[4] = {"BACK", "RIGHT", "FRONT", "LEFT"};
    static const char* kBottomAdjacentRoles[4] = {"FRONT", "RIGHT", "BACK", "LEFT"};
    json_object* corner_ids = NULL;
    json_object* edge_ids = NULL;
    json_object* adjacent_face_roles = NULL;
    const char* const* adjacent_roles = NULL;
    const char* layout_kind = NULL;
    const char* net_slot = NULL;
    int corner_values[4] = {0, 1, 2, 3};
    int edge_values[4] = {0, 1, 2, 3};
    int i = 0;
    if (!surface || !primitive_kind || !face_role) {
        return false;
    }
    if (strcmp(primitive_kind, "PLANE") == 0) {
        layout_kind = "PLANE";
        net_slot = "FRONT";
        for (i = 0; i < 4; ++i) {
            corner_values[i] = 255;
            edge_values[i] = 255;
        }
        adjacent_roles = kPlaneAdjacentRoles;
    } else if (strcmp(primitive_kind, "RECT_PRISM") == 0) {
        layout_kind = "PRISM_CROSS";
        net_slot = face_role;
        if (strcmp(face_role, "FRONT") == 0) adjacent_roles = kFrontAdjacentRoles;
        else if (strcmp(face_role, "BACK") == 0) adjacent_roles = kBackAdjacentRoles;
        else if (strcmp(face_role, "LEFT") == 0) adjacent_roles = kLeftAdjacentRoles;
        else if (strcmp(face_role, "RIGHT") == 0) adjacent_roles = kRightAdjacentRoles;
        else if (strcmp(face_role, "TOP") == 0) adjacent_roles = kTopAdjacentRoles;
        else if (strcmp(face_role, "BOTTOM") == 0) adjacent_roles = kBottomAdjacentRoles;
        else return false;
    } else {
        return false;
    }
    corner_ids = json_object_new_array();
    edge_ids = json_object_new_array();
    adjacent_face_roles = json_object_new_array();
    if (!corner_ids || !edge_ids || !adjacent_face_roles || !layout_kind || !net_slot ||
        !adjacent_roles) {
        if (corner_ids) json_object_put(corner_ids);
        if (edge_ids) json_object_put(edge_ids);
        if (adjacent_face_roles) json_object_put(adjacent_face_roles);
        return false;
    }
    json_object_object_add(surface, "net_layout_kind", json_object_new_string(layout_kind));
    json_object_object_add(surface, "net_slot", json_object_new_string(net_slot));
    json_object_object_add(surface, "orientation", json_object_new_string("R0"));
    for (i = 0; i < 4; ++i) {
        json_object_array_add(corner_ids, json_object_new_int(corner_values[i]));
        json_object_array_add(edge_ids, json_object_new_int(edge_values[i]));
        json_object_array_add(adjacent_face_roles,
                              json_object_new_string(adjacent_roles[i]));
    }
    json_object_object_add(surface, "corner_ids", corner_ids);
    json_object_object_add(surface, "edge_ids", edge_ids);
    json_object_object_add(surface, "adjacent_face_roles", adjacent_face_roles);
    return true;
}

static bool test_scene_editor_write_authored_texture_manifest(const char* manifest_path,
                                                              const char* object_id,
                                                              const char* primitive_kind,
                                                              const char* face_role,
                                                              const char* file_name) {
    static const char* kRectPrismRoles[] = {"FRONT", "BACK", "LEFT", "RIGHT", "TOP", "BOTTOM"};
    json_object* root = NULL;
    json_object* base_surfaces = NULL;
    json_object* base_surface = NULL;
    int write_ok = 0;
    if (!manifest_path || !object_id || !primitive_kind || !face_role || !file_name) {
        return false;
    }
    root = json_object_new_object();
    base_surfaces = json_object_new_array();
    base_surface = json_object_new_object();
    if (!root || !base_surfaces || !base_surface) {
        if (root) json_object_put(root);
        if (base_surfaces) json_object_put(base_surfaces);
        if (base_surface) json_object_put(base_surface);
        return false;
    }
    json_object_object_add(root, "schema_version", json_object_new_int(5));
    json_object_object_add(root,
                           "export_binding_kind",
                           json_object_new_string("SEPARATE_FACES"));
    json_object_object_add(root,
                           "emitted_output_kind",
                           json_object_new_string("FLATTENED_ONLY"));
    json_object_object_add(root, "primitive_kind", json_object_new_string(primitive_kind));
    json_object_object_add(root, "source_scene_id", json_object_new_string("scene_editor_test"));
    json_object_object_add(root, "source_object_id", json_object_new_string(object_id));
    if (strcmp(primitive_kind, "RECT_PRISM") == 0) {
        json_object_put(base_surface);
        base_surface = NULL;
        json_object_object_add(root, "base_surface_count", json_object_new_int(6));
        for (int i = 0; i < 6; ++i) {
            json_object* prism_base_surface = json_object_new_object();
            if (!prism_base_surface) {
                if (prism_base_surface) json_object_put(prism_base_surface);
                json_object_put(root);
                return false;
            }
            json_object_object_add(prism_base_surface, "surface_id", json_object_new_int(i + 1));
            json_object_object_add(prism_base_surface, "face_role", json_object_new_string(kRectPrismRoles[i]));
            json_object_object_add(prism_base_surface, "file_name", json_object_new_string(file_name));
            if (!test_scene_editor_add_surface_semantic_fields(prism_base_surface,
                                                               primitive_kind,
                                                               kRectPrismRoles[i])) {
                json_object_put(prism_base_surface);
                json_object_put(root);
                return false;
            }
            json_object_array_add(base_surfaces, prism_base_surface);
        }
    } else {
        json_object_object_add(root, "base_surface_count", json_object_new_int(1));
        json_object_object_add(base_surface, "surface_id", json_object_new_int(1));
        json_object_object_add(base_surface, "face_role", json_object_new_string(face_role));
        json_object_object_add(base_surface, "file_name", json_object_new_string(file_name));
        if (!test_scene_editor_add_surface_semantic_fields(base_surface,
                                                           primitive_kind,
                                                           face_role)) {
            json_object_put(root);
            return false;
        }
        json_object_array_add(base_surfaces, base_surface);
    }
    json_object_object_add(root, "base_surfaces", base_surfaces);
    write_ok = json_object_to_file_ext(manifest_path, root, JSON_C_TO_STRING_PRETTY);
    json_object_put(root);
    return write_ok == 0;
}

static bool test_scene_editor_write_invalid_authored_texture_manifest_missing_output_kind(
    const char* manifest_path,
    const char* object_id,
    const char* primitive_kind,
    const char* face_role,
    const char* file_name) {
    json_object* root = NULL;
    json_object* base_surfaces = NULL;
    json_object* surface = NULL;
    int write_ok = 0;
    if (!manifest_path || !object_id || !primitive_kind || !face_role || !file_name) {
        return false;
    }
    root = json_object_new_object();
    base_surfaces = json_object_new_array();
    surface = json_object_new_object();
    if (!root || !base_surfaces || !surface) {
        if (root) json_object_put(root);
        if (base_surfaces) json_object_put(base_surfaces);
        if (surface) json_object_put(surface);
        return false;
    }
    json_object_object_add(root, "schema_version", json_object_new_int(5));
    json_object_object_add(root,
                           "export_binding_kind",
                           json_object_new_string("SEPARATE_FACES"));
    json_object_object_add(root, "primitive_kind", json_object_new_string(primitive_kind));
    json_object_object_add(root, "source_object_id", json_object_new_string(object_id));
    json_object_object_add(surface, "face_role", json_object_new_string(face_role));
    json_object_object_add(surface, "file_name", json_object_new_string(file_name));
    json_object_array_add(base_surfaces, surface);
    json_object_object_add(root, "base_surfaces", base_surfaces);
    write_ok = json_object_to_file_ext(manifest_path, root, JSON_C_TO_STRING_PRETTY);
    json_object_put(root);
    return write_ok == 0;
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
    animSettings.lightIntensity = 6.75;
    animSettings.lightRadius = 2.25;

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
        assert_true("runtime_scene_authoring_persist_has_light_settings",
                    strstr(persisted_json, "\"light_settings\"") != NULL);
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
    assert_close("runtime_scene_authoring_persist_light_intensity",
                 animSettings.lightIntensity,
                 6.75,
                 1e-6);
    assert_close("runtime_scene_authoring_persist_light_radius",
                 animSettings.lightRadius,
                 2.25,
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
    const char *texture_dir = "/tmp/ray_tracing_runtime_scene_authoring_texture_set";
    const char *texture_png =
        "/tmp/ray_tracing_runtime_scene_authoring_texture_set/box_a_front.png";
    const char *texture_manifest =
        "/tmp/ray_tracing_runtime_scene_authoring_texture_set/box_a_texture_manifest.json";
    const char *texture_manifest_rel =
        "ray_tracing_runtime_scene_authoring_texture_set/box_a_texture_manifest.json";
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
    RuntimeSceneBridgePreflight reapply_summary = {0};
    RuntimeMaterialPayload3D payload = {0};
    SceneEditorMaterialFacePlacement face_placement;
    unsigned char texture_rgba[] = {255u, 48u, 24u, 255u};
    char authored_manifest_path[RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY];
    char authored_binding_mode[RUNTIME_MATERIAL_AUTHORED_TEXTURE_MODE_CAPACITY];
    int authored_face_count = 0;
    HitInfo3D hit = {0};
    bool ok = false;

    assert_true("runtime_scene_authoring_material_persist_open_tmp", file != NULL);
    if (!file) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    fwrite(runtime_json, 1, strlen(runtime_json), file);
    fclose(file);
    (void)mkdir(texture_dir, 0775);
    assert_true("runtime_scene_authoring_material_texture_png_write_ok",
                test_scene_editor_write_png_rgba(texture_png, texture_rgba, 1u, 1u));
    assert_true("runtime_scene_authoring_material_texture_manifest_write_ok",
                test_scene_editor_write_authored_texture_manifest(texture_manifest,
                                                                  "box_a",
                                                                  "RECT_PRISM",
                                                                  "TOP",
                                                                  "box_a_front.png"));

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialFacePlacementResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
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
    sceneSettings.sceneObjects[0].color = 0x00FF00;
    sceneSettings.sceneObjects[0].alpha = 0.35;
    sceneSettings.sceneObjects[0].emissiveStrength = 0.65;
    sceneSettings.sceneObjects[0].textureId = RUNTIME_MATERIAL_TEXTURE_3D_RUST;
    sceneSettings.sceneObjects[0].textureOffsetU = 0.12;
    sceneSettings.sceneObjects[0].textureOffsetV = 0.23;
    sceneSettings.sceneObjects[0].textureScale = 2.5;
    sceneSettings.sceneObjects[0].textureStrength = 0.8;
    sceneSettings.sceneObjects[0].texturePatternMode = RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_FLOW;
    sceneSettings.sceneObjects[0].textureCoverage = 0.72;
    sceneSettings.sceneObjects[0].textureGrain = 0.41;
    sceneSettings.sceneObjects[0].textureEdgeSoftness = 0.63;
    sceneSettings.sceneObjects[0].textureContrast = 0.29;
    sceneSettings.sceneObjects[0].textureFlow = 0.86;
    sceneSettings.sceneObjects[0].textureColorDepth = 0.74;
    sceneSettings.sceneObjects[0].textureSurfaceDamage = 0.66;
    sceneSettings.sceneObjects[0].textureSeed = 17;
    memset(&face_placement, 0, sizeof(face_placement));
    face_placement.hasOverride = true;
    face_placement.sceneObjectIndex = 0;
    face_placement.faceGroupIndex = 4;
    face_placement.textureId = RUNTIME_MATERIAL_TEXTURE_3D_FOG;
    face_placement.offsetU = 0.31;
    face_placement.offsetV = 0.42;
    face_placement.scale = 3.5;
    face_placement.strength = 0.7;
    face_placement.rotation = 0.25;
    face_placement.params = RuntimeMaterialTexture3DParamsFromObject(&sceneSettings.sceneObjects[0]);
    face_placement.params.patternMode = RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_SPECKLE;
    face_placement.params.coverage = 0.52;
    face_placement.params.surfaceDamage = 0.91;
    assert_true("runtime_scene_authoring_material_face_override_seeded",
                SceneEditorMaterialFacePlacementSetOverride(&face_placement));
    assert_true("runtime_scene_authoring_material_authored_texture_bind_ok",
                RuntimeMaterialAuthoredTextureBindManifestForObject(0,
                                                                   "box_a",
                                                                   texture_manifest_rel,
                                                                   "override"));

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
        assert_true("runtime_scene_authoring_material_persist_has_object_color",
                    strstr(persisted_json, "\"object_color\":65280") != NULL);
        assert_true("runtime_scene_authoring_material_persist_has_alpha",
                    strstr(persisted_json, "\"alpha\":") != NULL);
        assert_true("runtime_scene_authoring_material_persist_has_emissive_strength",
                    strstr(persisted_json, "\"emissive_strength\":") != NULL);
        assert_true("runtime_scene_authoring_material_persist_has_procedural_texture",
                    strstr(persisted_json, "\"procedural_texture\"") != NULL);
        assert_true("runtime_scene_authoring_material_persist_has_texture_id",
                    strstr(persisted_json, "\"texture_id\":1") != NULL);
        assert_true("runtime_scene_authoring_material_persist_has_parameters",
                    strstr(persisted_json, "\"parameters\"") != NULL);
        assert_true("runtime_scene_authoring_material_persist_has_flow_pattern",
                    strstr(persisted_json, "\"pattern_mode\":3") != NULL);
        assert_true("runtime_scene_authoring_material_persist_has_face_placements",
                    strstr(persisted_json, "\"face_placements\"") != NULL);
        assert_true("runtime_scene_authoring_material_persist_has_face_group",
                    strstr(persisted_json, "\"face_group_index\":4") != NULL);
        assert_true("runtime_scene_authoring_material_persist_has_face_texture_id",
                    strstr(persisted_json, "\"texture_id\":2") != NULL);
        assert_true("runtime_scene_authoring_material_persist_has_authored_texture",
                    strstr(persisted_json, "\"authored_texture\"") != NULL);
        assert_true("runtime_scene_authoring_material_persist_has_manifest_path",
                    strstr(persisted_json, texture_manifest_rel) != NULL);
    }

    assert_true("runtime_scene_authoring_material_persist_hydrated_material_id",
                sceneSettings.sceneObjects[0].material_id == 3);
    assert_true("runtime_scene_authoring_material_persist_hydrated_object_color",
                sceneSettings.sceneObjects[0].color == 0x00FF00);
    assert_close("runtime_scene_authoring_material_persist_hydrated_alpha",
                 sceneSettings.sceneObjects[0].alpha,
                 0.35,
                 1e-9);
    assert_close("runtime_scene_authoring_material_persist_hydrated_emissive_strength",
                 sceneSettings.sceneObjects[0].emissiveStrength,
                 0.65,
                 1e-9);
    assert_true("runtime_scene_authoring_material_persist_hydrated_texture_id",
                sceneSettings.sceneObjects[0].textureId == RUNTIME_MATERIAL_TEXTURE_3D_RUST);
    assert_close("runtime_scene_authoring_material_persist_hydrated_texture_offset_u",
                 sceneSettings.sceneObjects[0].textureOffsetU,
                 0.12,
                 1e-9);
    assert_close("runtime_scene_authoring_material_persist_hydrated_texture_offset_v",
                 sceneSettings.sceneObjects[0].textureOffsetV,
                 0.23,
                 1e-9);
    assert_close("runtime_scene_authoring_material_persist_hydrated_texture_scale",
                 sceneSettings.sceneObjects[0].textureScale,
                 2.5,
                 1e-9);
    assert_close("runtime_scene_authoring_material_persist_hydrated_texture_strength",
                 sceneSettings.sceneObjects[0].textureStrength,
                 0.8,
                 1e-9);
    assert_true("runtime_scene_authoring_material_persist_hydrated_pattern",
                sceneSettings.sceneObjects[0].texturePatternMode ==
                    RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_FLOW);
    assert_close("runtime_scene_authoring_material_persist_hydrated_coverage",
                 sceneSettings.sceneObjects[0].textureCoverage,
                 0.72,
                 1e-9);
    assert_close("runtime_scene_authoring_material_persist_hydrated_surface_damage",
                 sceneSettings.sceneObjects[0].textureSurfaceDamage,
                 0.66,
                 1e-9);
    assert_true("runtime_scene_authoring_material_persist_hydrated_seed",
                sceneSettings.sceneObjects[0].textureSeed == 17);
    assert_true("runtime_scene_authoring_material_persist_hydrated_face_override",
                SceneEditorMaterialFacePlacementHasOverride(0, 4));
    face_placement = SceneEditorMaterialFacePlacementGetEffective(&sceneSettings.sceneObjects[0], 0, 4);
    assert_true("runtime_scene_authoring_material_persist_hydrated_face_texture_id",
                face_placement.textureId == RUNTIME_MATERIAL_TEXTURE_3D_FOG);
    assert_close("runtime_scene_authoring_material_persist_hydrated_face_offset_u",
                 face_placement.offsetU,
                 0.31,
                 1e-9);
    assert_close("runtime_scene_authoring_material_persist_hydrated_face_offset_v",
                 face_placement.offsetV,
                 0.42,
                 1e-9);
    assert_close("runtime_scene_authoring_material_persist_hydrated_face_scale",
                 face_placement.scale,
                 3.5,
                 1e-9);
    assert_close("runtime_scene_authoring_material_persist_hydrated_face_strength",
                 face_placement.strength,
                 0.7,
                 1e-9);
    assert_close("runtime_scene_authoring_material_persist_hydrated_face_rotation",
                 face_placement.rotation,
                 0.25,
                 1e-9);
    assert_true("runtime_scene_authoring_material_persist_hydrated_face_pattern",
                face_placement.params.patternMode ==
                    RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_SPECKLE);
    assert_close("runtime_scene_authoring_material_persist_hydrated_face_damage",
                 face_placement.params.surfaceDamage,
                 0.91,
                 1e-9);
    assert_true("runtime_scene_authoring_material_persist_reapply_ok",
                runtime_scene_bridge_apply_file(runtime_path, &reapply_summary));
    assert_true("runtime_scene_authoring_material_persist_reapply_binding_present",
                RuntimeMaterialAuthoredTextureGetBinding(0,
                                                        authored_manifest_path,
                                                        sizeof(authored_manifest_path),
                                                        authored_binding_mode,
                                                        sizeof(authored_binding_mode),
                                                        &authored_face_count));
    assert_true("runtime_scene_authoring_material_persist_reapply_binding_path",
                strcmp(authored_manifest_path, texture_manifest_rel) == 0);
    assert_true("runtime_scene_authoring_material_persist_reapply_binding_mode",
                strcmp(authored_binding_mode, "override") == 0);
    assert_true("runtime_scene_authoring_material_persist_reapply_face_count",
                authored_face_count == 6);
    HitInfo3D_Reset(&hit);
    hit.sceneObjectIndex = 0;
    hit.triangleIndex = 12;
    hit.localTriangleIndex = 8;
    hit.primitiveIndex = 0;
    hit.baryU = 0.2;
    hit.baryV = 0.3;
    hit.baryW = 0.5;
    assert_true("runtime_scene_authoring_material_persist_reapply_payload_ok",
                RuntimeMaterialPayload3D_ResolveFromHit(&hit, &payload));
    assert_true("runtime_scene_authoring_material_persist_reapply_payload_mask",
                payload.textureMask > 0.99);
    assert_true("runtime_scene_authoring_material_persist_reapply_payload_red",
                payload.baseColorR > payload.baseColorG);

    free(persisted_json);
    unlink(texture_png);
    unlink(texture_manifest);
    rmdir(texture_dir);
    unlink(runtime_path);
    RuntimeMaterialAuthoredTextureResetAll();
    SceneEditorMaterialFacePlacementResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_authored_texture_binding_routes_to_runtime_binding(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *texture_dir = "/tmp/ray_tracing_material_editor_authored_texture_set";
    const char *texture_png =
        "/tmp/ray_tracing_material_editor_authored_texture_set/box_bind_top.png";
    const char *texture_manifest =
        "/tmp/ray_tracing_material_editor_authored_texture_set/box_bind_texture_manifest.json";
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_material_editor_authored_bind\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[{"
          "\"object_id\":\"box_bind\","
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
    RuntimeSceneBridgePreflight summary = {0};
    unsigned char texture_rgba[] = {96u, 140u, 220u, 255u};
    char authored_manifest_path[RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY];
    char authored_binding_mode[RUNTIME_MATERIAL_AUTHORED_TEXTURE_MODE_CAPACITY];
    int authored_face_count = 0;
    bool ok = false;

    (void)mkdir(texture_dir, 0775);
    assert_true("material_editor_authored_texture_png_write_ok",
                test_scene_editor_write_png_rgba(texture_png, texture_rgba, 1u, 1u));
    assert_true("material_editor_authored_texture_manifest_write_ok",
                test_scene_editor_write_authored_texture_manifest(texture_manifest,
                                                                  "box_bind",
                                                                  "RECT_PRISM",
                                                                  "TOP",
                                                                  "box_bind_top.png"));

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialFacePlacementResetAll();
    SceneEditorMaterialStackResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("material_editor_authored_texture_runtime_apply_ok", ok);
    if (!ok) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();
    assert_true("material_editor_authored_texture_bind_ok",
                MaterialEditorBindAuthoredTextureManifestForFocused(texture_manifest));
    assert_true("material_editor_authored_texture_summary_ok",
                MaterialEditorGetAuthoredTextureBindingSummary(authored_manifest_path,
                                                              sizeof(authored_manifest_path),
                                                              authored_binding_mode,
                                                              sizeof(authored_binding_mode),
                                                              &authored_face_count));
    assert_true("material_editor_authored_texture_summary_path_matches",
                strstr(authored_manifest_path, "box_bind_texture_manifest.json") != NULL);
    assert_true("material_editor_authored_texture_summary_mode_override",
                strcmp(authored_binding_mode, "override") == 0);
    assert_true("material_editor_authored_texture_summary_face_count",
                authored_face_count == 6);

    assert_true("material_editor_authored_texture_clear_ok",
                MaterialEditorClearAuthoredTextureBindingForFocused());
    assert_true("material_editor_authored_texture_summary_cleared",
                !MaterialEditorGetAuthoredTextureBindingSummary(authored_manifest_path,
                                                               sizeof(authored_manifest_path),
                                                               authored_binding_mode,
                                                               sizeof(authored_binding_mode),
                                                               &authored_face_count));

    RuntimeMaterialAuthoredTextureResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_authored_texture_binding_persists_and_reopens(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_path = "/tmp/ray_tracing_material_editor_authored_reopen.json";
    const char *texture_dir = "/tmp/ray_tracing_material_editor_authored_reopen_textures";
    const char *texture_png =
        "/tmp/ray_tracing_material_editor_authored_reopen_textures/box_bind_top.png";
    const char *texture_manifest =
        "/tmp/ray_tracing_material_editor_authored_reopen_textures/box_bind_texture_manifest.json";
    const char *texture_manifest_rel =
        "ray_tracing_material_editor_authored_reopen_textures/box_bind_texture_manifest.json";
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_material_editor_authored_reopen\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[{"
          "\"object_id\":\"box_bind\","
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
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeSceneBridgePreflight reapply_summary = {0};
    unsigned char texture_rgba[] = {96u, 140u, 220u, 255u};
    char diagnostics[256];
    char *persisted_json = NULL;
    char authored_manifest_path[RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY];
    char authored_binding_mode[RUNTIME_MATERIAL_AUTHORED_TEXTURE_MODE_CAPACITY];
    int authored_face_count = 0;
    FILE *file = fopen(runtime_path, "wb");
    bool ok = false;

    assert_true("material_editor_authored_reopen_open_tmp", file != NULL);
    if (!file) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    fwrite(runtime_json, 1, strlen(runtime_json), file);
    fclose(file);

    (void)mkdir(texture_dir, 0775);
    assert_true("material_editor_authored_reopen_png_write_ok",
                test_scene_editor_write_png_rgba(texture_png, texture_rgba, 1u, 1u));
    assert_true("material_editor_authored_reopen_manifest_write_ok",
                test_scene_editor_write_authored_texture_manifest(texture_manifest,
                                                                  "box_bind",
                                                                  "RECT_PRISM",
                                                                  "TOP",
                                                                  "box_bind_top.png"));

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialFacePlacementResetAll();
    SceneEditorMaterialStackResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    ok = runtime_scene_bridge_apply_file(runtime_path, &summary);
    assert_true("material_editor_authored_reopen_apply_ok", ok);
    if (!ok) {
        unlink(texture_png);
        unlink(texture_manifest);
        rmdir(texture_dir);
        unlink(runtime_path);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath), "%s", runtime_path);
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();
    assert_true("material_editor_authored_reopen_bind_ok",
                MaterialEditorBindAuthoredTextureManifestForFocused(texture_manifest));
    assert_true("material_editor_authored_reopen_persist_ok",
                SceneEditorRuntimeScenePersistAuthoring(diagnostics, sizeof(diagnostics)));

    persisted_json = read_text_file_alloc(runtime_path, NULL);
    assert_true("material_editor_authored_reopen_readback_ok", persisted_json != NULL);
    if (persisted_json) {
        assert_true("material_editor_authored_reopen_persisted_relative_manifest",
                    strstr(persisted_json, texture_manifest_rel) != NULL);
        assert_true("material_editor_authored_reopen_persisted_not_absolute_manifest",
                    strstr(persisted_json, texture_manifest) == NULL);
    }

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialFacePlacementResetAll();
    SceneEditorMaterialStackResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    assert_true("material_editor_authored_reopen_reapply_ok",
                runtime_scene_bridge_apply_file(runtime_path, &reapply_summary));

    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();
    assert_true("material_editor_authored_reopen_summary_ok",
                MaterialEditorGetAuthoredTextureBindingSummary(authored_manifest_path,
                                                              sizeof(authored_manifest_path),
                                                              authored_binding_mode,
                                                              sizeof(authored_binding_mode),
                                                              &authored_face_count));
    assert_true("material_editor_authored_reopen_summary_relative_path",
                strcmp(authored_manifest_path, texture_manifest_rel) == 0);
    assert_true("material_editor_authored_reopen_summary_mode",
                strcmp(authored_binding_mode, "override") == 0);
    assert_true("material_editor_authored_reopen_summary_face_count",
                authored_face_count == 6);

    free(persisted_json);
    unlink(texture_png);
    unlink(texture_manifest);
    rmdir(texture_dir);
    unlink(runtime_path);
    RuntimeMaterialAuthoredTextureResetAll();
    SceneEditorMaterialFacePlacementResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_authored_texture_binding_replace_clear_roundtrip(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_path = "/tmp/ray_tracing_material_editor_authored_replace_clear.json";
    const char *texture_dir = "/tmp/ray_tracing_material_editor_authored_replace_clear_textures";
    const char *texture_png_a =
        "/tmp/ray_tracing_material_editor_authored_replace_clear_textures/box_bind_top_a.png";
    const char *texture_manifest_a =
        "/tmp/ray_tracing_material_editor_authored_replace_clear_textures/box_bind_texture_manifest_a.json";
    const char *texture_manifest_a_rel =
        "ray_tracing_material_editor_authored_replace_clear_textures/box_bind_texture_manifest_a.json";
    const char *texture_png_b =
        "/tmp/ray_tracing_material_editor_authored_replace_clear_textures/box_bind_top_b.png";
    const char *texture_manifest_b =
        "/tmp/ray_tracing_material_editor_authored_replace_clear_textures/box_bind_texture_manifest_b.json";
    const char *texture_manifest_b_rel =
        "ray_tracing_material_editor_authored_replace_clear_textures/box_bind_texture_manifest_b.json";
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_material_editor_authored_replace_clear\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[{"
          "\"object_id\":\"box_bind\","
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
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeSceneBridgePreflight reapply_summary = {0};
    RuntimeSceneBridgePreflight reapply_cleared_summary = {0};
    unsigned char texture_rgba_a[] = {96u, 140u, 220u, 255u};
    unsigned char texture_rgba_b[] = {212u, 96u, 120u, 255u};
    char diagnostics[256];
    char *persisted_json = NULL;
    char authored_manifest_path[RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY];
    char authored_binding_mode[RUNTIME_MATERIAL_AUTHORED_TEXTURE_MODE_CAPACITY];
    int authored_face_count = 0;
    FILE *file = fopen(runtime_path, "wb");
    bool ok = false;

    assert_true("material_editor_authored_replace_clear_open_tmp", file != NULL);
    if (!file) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    fwrite(runtime_json, 1, strlen(runtime_json), file);
    fclose(file);

    (void)mkdir(texture_dir, 0775);
    assert_true("material_editor_authored_replace_clear_png_a_write_ok",
                test_scene_editor_write_png_rgba(texture_png_a, texture_rgba_a, 1u, 1u));
    assert_true("material_editor_authored_replace_clear_manifest_a_write_ok",
                test_scene_editor_write_authored_texture_manifest(texture_manifest_a,
                                                                  "box_bind",
                                                                  "RECT_PRISM",
                                                                  "TOP",
                                                                  "box_bind_top_a.png"));
    assert_true("material_editor_authored_replace_clear_png_b_write_ok",
                test_scene_editor_write_png_rgba(texture_png_b, texture_rgba_b, 1u, 1u));
    assert_true("material_editor_authored_replace_clear_manifest_b_write_ok",
                test_scene_editor_write_authored_texture_manifest(texture_manifest_b,
                                                                  "box_bind",
                                                                  "RECT_PRISM",
                                                                  "TOP",
                                                                  "box_bind_top_b.png"));

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialFacePlacementResetAll();
    SceneEditorMaterialStackResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    ok = runtime_scene_bridge_apply_file(runtime_path, &summary);
    assert_true("material_editor_authored_replace_clear_apply_ok", ok);
    if (!ok) {
        unlink(texture_png_a);
        unlink(texture_manifest_a);
        unlink(texture_png_b);
        unlink(texture_manifest_b);
        rmdir(texture_dir);
        unlink(runtime_path);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath), "%s", runtime_path);
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();

    assert_true("material_editor_authored_replace_clear_bind_a_ok",
                MaterialEditorBindAuthoredTextureManifestForFocused(texture_manifest_a));
    assert_true("material_editor_authored_replace_clear_summary_a_ok",
                MaterialEditorGetAuthoredTextureBindingSummary(authored_manifest_path,
                                                              sizeof(authored_manifest_path),
                                                              authored_binding_mode,
                                                              sizeof(authored_binding_mode),
                                                              &authored_face_count));
    assert_true("material_editor_authored_replace_clear_summary_a_path",
                strcmp(authored_manifest_path, texture_manifest_a) == 0);

    assert_true("material_editor_authored_replace_clear_bind_b_ok",
                MaterialEditorBindAuthoredTextureManifestForFocused(texture_manifest_b));
    assert_true("material_editor_authored_replace_clear_summary_b_ok",
                MaterialEditorGetAuthoredTextureBindingSummary(authored_manifest_path,
                                                              sizeof(authored_manifest_path),
                                                              authored_binding_mode,
                                                              sizeof(authored_binding_mode),
                                                              &authored_face_count));
    assert_true("material_editor_authored_replace_clear_summary_b_path",
                strcmp(authored_manifest_path, texture_manifest_b) == 0);
    assert_true("material_editor_authored_replace_clear_summary_b_mode",
                strcmp(authored_binding_mode, "override") == 0);
    assert_true("material_editor_authored_replace_clear_summary_b_face_count",
                authored_face_count == 6);

    assert_true("material_editor_authored_replace_clear_persist_bound_ok",
                SceneEditorRuntimeScenePersistAuthoring(diagnostics, sizeof(diagnostics)));
    persisted_json = read_text_file_alloc(runtime_path, NULL);
    assert_true("material_editor_authored_replace_clear_readback_bound_ok", persisted_json != NULL);
    if (persisted_json) {
        assert_true("material_editor_authored_replace_clear_persisted_uses_b_relative",
                    strstr(persisted_json, texture_manifest_b_rel) != NULL);
        assert_true("material_editor_authored_replace_clear_persisted_not_a_relative",
                    strstr(persisted_json, texture_manifest_a_rel) == NULL);
        assert_true("material_editor_authored_replace_clear_persisted_not_b_absolute",
                    strstr(persisted_json, texture_manifest_b) == NULL);
        free(persisted_json);
        persisted_json = NULL;
    }

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialFacePlacementResetAll();
    SceneEditorMaterialStackResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    assert_true("material_editor_authored_replace_clear_reapply_bound_ok",
                runtime_scene_bridge_apply_file(runtime_path, &reapply_summary));
    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath), "%s", runtime_path);
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();
    assert_true("material_editor_authored_replace_clear_reapply_summary_ok",
                MaterialEditorGetAuthoredTextureBindingSummary(authored_manifest_path,
                                                              sizeof(authored_manifest_path),
                                                              authored_binding_mode,
                                                              sizeof(authored_binding_mode),
                                                              &authored_face_count));
    assert_true("material_editor_authored_replace_clear_reapply_summary_b_relative_path",
                strcmp(authored_manifest_path, texture_manifest_b_rel) == 0);

    assert_true("material_editor_authored_replace_clear_clear_ok",
                MaterialEditorClearAuthoredTextureBindingForFocused());
    assert_true("material_editor_authored_replace_clear_summary_cleared",
                !MaterialEditorGetAuthoredTextureBindingSummary(authored_manifest_path,
                                                               sizeof(authored_manifest_path),
                                                               authored_binding_mode,
                                                               sizeof(authored_binding_mode),
                                                               &authored_face_count));
    assert_true("material_editor_authored_replace_clear_persist_cleared_ok",
                SceneEditorRuntimeScenePersistAuthoring(diagnostics, sizeof(diagnostics)));
    persisted_json = read_text_file_alloc(runtime_path, NULL);
    assert_true("material_editor_authored_replace_clear_readback_cleared_ok", persisted_json != NULL);
    if (persisted_json) {
        assert_true("material_editor_authored_replace_clear_persisted_cleared_authored_texture_null",
                    strstr(persisted_json, "\"authored_texture\":null") != NULL ||
                    strstr(persisted_json, "\"authored_texture\": null") != NULL);
        assert_true("material_editor_authored_replace_clear_persisted_cleared_no_manifest_b",
                    strstr(persisted_json, texture_manifest_b_rel) == NULL);
        free(persisted_json);
        persisted_json = NULL;
    }

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialFacePlacementResetAll();
    SceneEditorMaterialStackResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    assert_true("material_editor_authored_replace_clear_reapply_cleared_ok",
                runtime_scene_bridge_apply_file(runtime_path, &reapply_cleared_summary));
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();
    assert_true("material_editor_authored_replace_clear_reapply_cleared_summary_absent",
                !MaterialEditorGetAuthoredTextureBindingSummary(authored_manifest_path,
                                                               sizeof(authored_manifest_path),
                                                               authored_binding_mode,
                                                               sizeof(authored_binding_mode),
                                                               &authored_face_count));

    unlink(texture_png_a);
    unlink(texture_manifest_a);
    unlink(texture_png_b);
    unlink(texture_manifest_b);
    rmdir(texture_dir);
    unlink(runtime_path);
    RuntimeMaterialAuthoredTextureResetAll();
    SceneEditorMaterialFacePlacementResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_authored_texture_invalid_binding_surfaces_reason_and_clears(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char* texture_dir = "/tmp/ray_tracing_material_editor_authored_invalid_set";
    const char* texture_png =
        "/tmp/ray_tracing_material_editor_authored_invalid_set/plane_front.png";
    const char* texture_manifest =
        "/tmp/ray_tracing_material_editor_authored_invalid_set/plane_texture_manifest.json";
    const char* runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_material_editor_authored_invalid_bind\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[{"
          "\"object_id\":\"plane_bind\","
          "\"object_type\":\"plane_primitive\","
          "\"transform\":{"
            "\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
            "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}"
          "},"
          "\"primitive\":{"
            "\"kind\":\"plane_primitive\","
            "\"width\":1.0,"
            "\"height\":1.0"
          "},"
          "\"flags\":{\"visible\":true}"
        "}],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    unsigned char texture_rgba[] = {96u, 140u, 220u, 255u};
    char authored_manifest_path[RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY];
    char authored_binding_mode[RUNTIME_MATERIAL_AUTHORED_TEXTURE_MODE_CAPACITY];
    char authored_reason[RUNTIME_MATERIAL_AUTHORED_TEXTURE_REASON_CAPACITY];
    bool ok = false;

    (void)mkdir(texture_dir, 0775);
    assert_true("material_editor_authored_invalid_png_write_ok",
                test_scene_editor_write_png_rgba(texture_png, texture_rgba, 1u, 1u));
    assert_true("material_editor_authored_invalid_manifest_write_ok",
                test_scene_editor_write_invalid_authored_texture_manifest_missing_output_kind(
                    texture_manifest,
                    "plane_bind",
                    "PLANE",
                    "FRONT",
                    "plane_front.png"));

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialFacePlacementResetAll();
    SceneEditorMaterialStackResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("material_editor_authored_invalid_runtime_apply_ok", ok);
    if (!ok) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();
    assert_true("material_editor_authored_invalid_bind_rejected",
                !MaterialEditorBindAuthoredTextureManifestForFocused(texture_manifest));
    assert_true("material_editor_authored_invalid_summary_absent",
                !MaterialEditorGetAuthoredTextureBindingSummary(authored_manifest_path,
                                                               sizeof(authored_manifest_path),
                                                               authored_binding_mode,
                                                               sizeof(authored_binding_mode),
                                                               NULL));
    assert_true("material_editor_authored_invalid_summary_present",
                MaterialEditorGetAuthoredTextureInvalidSummary(authored_manifest_path,
                                                              sizeof(authored_manifest_path),
                                                              authored_binding_mode,
                                                              sizeof(authored_binding_mode),
                                                              authored_reason,
                                                              sizeof(authored_reason)));
    assert_true("material_editor_authored_invalid_summary_path_matches",
                strcmp(authored_manifest_path, texture_manifest) == 0);
    assert_true("material_editor_authored_invalid_summary_mode_override",
                strcmp(authored_binding_mode, "override") == 0);
    assert_true("material_editor_authored_invalid_summary_reason",
                strcmp(authored_reason, "schema or output contract invalid") == 0);

    assert_true("material_editor_authored_invalid_clear_ok",
                MaterialEditorClearAuthoredTextureBindingForFocused());
    assert_true("material_editor_authored_invalid_summary_cleared",
                !MaterialEditorGetAuthoredTextureInvalidSummary(authored_manifest_path,
                                                               sizeof(authored_manifest_path),
                                                               authored_binding_mode,
                                                               sizeof(authored_binding_mode),
                                                               authored_reason,
                                                               sizeof(authored_reason)));

    unlink(texture_png);
    unlink(texture_manifest);
    rmdir(texture_dir);
    RuntimeMaterialAuthoredTextureResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_authored_texture_invalid_binding_persists_and_reopens(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char* runtime_path = "/tmp/ray_tracing_material_editor_authored_invalid_reopen.json";
    const char* texture_dir = "/tmp/ray_tracing_material_editor_authored_invalid_reopen_set";
    const char* texture_png =
        "/tmp/ray_tracing_material_editor_authored_invalid_reopen_set/plane_front.png";
    const char* texture_manifest =
        "/tmp/ray_tracing_material_editor_authored_invalid_reopen_set/plane_texture_manifest.json";
    const char* texture_manifest_rel =
        "ray_tracing_material_editor_authored_invalid_reopen_set/plane_texture_manifest.json";
    const char* runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_material_editor_authored_invalid_reopen\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[{"
          "\"object_id\":\"plane_bind\","
          "\"object_type\":\"plane_primitive\","
          "\"transform\":{"
            "\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
            "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}"
          "},"
          "\"primitive\":{"
            "\"kind\":\"plane_primitive\","
            "\"width\":1.0,"
            "\"height\":1.0"
          "},"
          "\"flags\":{\"visible\":true}"
        "}],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeSceneBridgePreflight reapply_summary = {0};
    unsigned char texture_rgba[] = {96u, 140u, 220u, 255u};
    char diagnostics[256];
    char* persisted_json = NULL;
    char authored_manifest_path[RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY];
    char authored_binding_mode[RUNTIME_MATERIAL_AUTHORED_TEXTURE_MODE_CAPACITY];
    char authored_reason[RUNTIME_MATERIAL_AUTHORED_TEXTURE_REASON_CAPACITY];
    FILE* file = fopen(runtime_path, "wb");
    bool ok = false;

    assert_true("material_editor_authored_invalid_reopen_open_tmp", file != NULL);
    if (!file) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    fwrite(runtime_json, 1, strlen(runtime_json), file);
    fclose(file);

    (void)mkdir(texture_dir, 0775);
    assert_true("material_editor_authored_invalid_reopen_png_write_ok",
                test_scene_editor_write_png_rgba(texture_png, texture_rgba, 1u, 1u));
    assert_true("material_editor_authored_invalid_reopen_manifest_write_ok",
                test_scene_editor_write_invalid_authored_texture_manifest_missing_output_kind(
                    texture_manifest,
                    "plane_bind",
                    "PLANE",
                    "FRONT",
                    "plane_front.png"));

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialFacePlacementResetAll();
    SceneEditorMaterialStackResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    ok = runtime_scene_bridge_apply_file(runtime_path, &summary);
    assert_true("material_editor_authored_invalid_reopen_apply_ok", ok);
    if (!ok) {
        unlink(texture_png);
        unlink(texture_manifest);
        rmdir(texture_dir);
        unlink(runtime_path);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath), "%s", runtime_path);
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();
    assert_true("material_editor_authored_invalid_reopen_bind_rejected",
                !MaterialEditorBindAuthoredTextureManifestForFocused(texture_manifest));
    assert_true("material_editor_authored_invalid_reopen_persist_ok",
                SceneEditorRuntimeScenePersistAuthoring(diagnostics, sizeof(diagnostics)));

    persisted_json = read_text_file_alloc(runtime_path, NULL);
    assert_true("material_editor_authored_invalid_reopen_readback_ok", persisted_json != NULL);
    if (persisted_json) {
        assert_true("material_editor_authored_invalid_reopen_persisted_relative_manifest",
                    strstr(persisted_json, texture_manifest_rel) != NULL);
        assert_true("material_editor_authored_invalid_reopen_persisted_not_null",
                    strstr(persisted_json, "\"authored_texture\":null") == NULL &&
                    strstr(persisted_json, "\"authored_texture\": null") == NULL);
        free(persisted_json);
        persisted_json = NULL;
    }

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialFacePlacementResetAll();
    SceneEditorMaterialStackResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    assert_true("material_editor_authored_invalid_reopen_reapply_ok",
                runtime_scene_bridge_apply_file(runtime_path, &reapply_summary));
    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath), "%s", runtime_path);
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();
    assert_true("material_editor_authored_invalid_reopen_summary_present",
                MaterialEditorGetAuthoredTextureInvalidSummary(authored_manifest_path,
                                                              sizeof(authored_manifest_path),
                                                              authored_binding_mode,
                                                              sizeof(authored_binding_mode),
                                                              authored_reason,
                                                              sizeof(authored_reason)));
    assert_true("material_editor_authored_invalid_reopen_summary_relative_path",
                strcmp(authored_manifest_path, texture_manifest_rel) == 0);
    assert_true("material_editor_authored_invalid_reopen_summary_mode_override",
                strcmp(authored_binding_mode, "override") == 0);
    assert_true("material_editor_authored_invalid_reopen_summary_reason",
                strcmp(authored_reason, "schema or output contract invalid") == 0);

    unlink(texture_png);
    unlink(texture_manifest);
    rmdir(texture_dir);
    unlink(runtime_path);
    RuntimeMaterialAuthoredTextureResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_scene_editor_runtime_scene_material_stack_roundtrip_payload(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_path = "/tmp/ray_tracing_runtime_scene_material_stack_roundtrip.json";
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_authoring_material_stack_1\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[{"
          "\"object_id\":\"box_stack\","
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
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialTextureStack hydrated = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialPayload3D baseline = {0};
    RuntimeMaterialPayload3D textured = {0};
    HitInfo3D hit = {0};
    bool ok = false;

    assert_true("runtime_scene_stack_roundtrip_open_tmp", file != NULL);
    if (!file) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    fwrite(runtime_json, 1, strlen(runtime_json), file);
    fclose(file);

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialFacePlacementResetAll();
    SceneEditorMaterialStackResetAll();
    RuntimeMaterialAuthoredTextureResetAll();

    ok = runtime_scene_bridge_apply_file(runtime_path, &summary);
    assert_true("runtime_scene_stack_roundtrip_apply_ok", ok);
    if (!ok) {
        unlink(runtime_path);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    HitInfo3D_Reset(&hit);
    hit.sceneObjectIndex = 0;
    hit.triangleIndex = 0;
    hit.localTriangleIndex = 0;
    hit.primitiveIndex = 0;
    hit.baryU = 0.2;
    hit.baryV = 0.3;
    hit.baryW = 0.5;
    assert_true("runtime_scene_stack_roundtrip_baseline_payload_ok",
                RuntimeMaterialPayload3D_ResolveFromHit(&hit, &baseline));

    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath), "%s", runtime_path);
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_ROUGH_METAL;
    sceneSettings.sceneObjects[0].color = 0x6A6A66;

    stack.layerCount = 3;
    stack.layers[0] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD);
    stack.layers[0].placement.strength = 0.85;
    stack.layers[0].placement.scale = 2.0;
    stack.layers[1] =
        RuntimeMaterialTextureLayerMakeOverlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME);
    stack.layers[1].opacity = 1.0;
    stack.layers[1].placement.strength = 1.0;
    stack.layers[1].placement.scale = 1.5;
    stack.layers[1].params.coverage = 1.0;
    stack.layers[1].params.grain = 0.85;
    stack.layers[1].roughnessInfluence = 0.75;
    stack.layers[2] =
        RuntimeMaterialTextureLayerMakeOverlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL);
    stack.layers[2].opacity = 0.65;
    stack.layers[2].placement.strength = 0.9;
    stack.layers[2].params.coverage = 0.85;
    stack.layers[2].specularInfluence = 0.7;
    assert_true("runtime_scene_stack_roundtrip_stack_seeded",
                SceneEditorMaterialStackSetObjectStack(0, &stack));

    ok = SceneEditorRuntimeScenePersistAuthoring(diagnostics, sizeof(diagnostics));
    assert_true("runtime_scene_stack_roundtrip_persist_ok", ok);
    if (!ok) {
        unlink(runtime_path);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    persisted_json = read_text_file_alloc(runtime_path, NULL);
    assert_true("runtime_scene_stack_roundtrip_readback_ok", persisted_json != NULL);
    if (persisted_json) {
        assert_true("runtime_scene_stack_roundtrip_has_stack_json",
                    strstr(persisted_json, "\"material_texture_stack\"") != NULL);
        assert_true("runtime_scene_stack_roundtrip_has_wood",
                    strstr(persisted_json, "\"kind\":\"wood\"") != NULL);
        assert_true("runtime_scene_stack_roundtrip_has_grime",
                    strstr(persisted_json, "\"kind\":\"grime\"") != NULL);
        assert_true("runtime_scene_stack_roundtrip_has_oil",
                    strstr(persisted_json, "\"kind\":\"oil\"") != NULL);
    }

    assert_true("runtime_scene_stack_roundtrip_hydrated_stack",
                SceneEditorMaterialStackGetObjectStack(0, &hydrated));
    assert_true("runtime_scene_stack_roundtrip_hydrated_layer_count",
                hydrated.layerCount == 3);
    assert_true("runtime_scene_stack_roundtrip_hydrated_base_kind",
                hydrated.layers[0].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD);
    assert_true("runtime_scene_stack_roundtrip_hydrated_grime_kind",
                hydrated.layers[1].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME);
    assert_close("runtime_scene_stack_roundtrip_hydrated_grime_roughness",
                 hydrated.layers[1].roughnessInfluence,
                 0.75,
                 1e-9);
    assert_true("runtime_scene_stack_roundtrip_hydrated_oil_kind",
                hydrated.layers[2].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL);

    assert_true("runtime_scene_stack_roundtrip_textured_payload_ok",
                RuntimeMaterialPayload3D_ResolveFromHit(&hit, &textured));
    assert_true("runtime_scene_stack_roundtrip_textured_payload_mask",
                textured.textureMask > 0.0);
    assert_true("runtime_scene_stack_roundtrip_payload_color_or_roughness_changed",
                fabs(textured.baseColorR - baseline.baseColorR) > 1e-6 ||
                fabs(textured.baseColorG - baseline.baseColorG) > 1e-6 ||
                fabs(textured.baseColorB - baseline.baseColorB) > 1e-6 ||
                fabs(textured.bsdf.roughness - baseline.bsdf.roughness) > 1e-6);

    free(persisted_json);
    unlink(runtime_path);
    SceneEditorMaterialStackResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    SceneEditorMaterialFacePlacementResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_scene_editor_runtime_scene_authored_texture_overlay_roundtrip_payload(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char* runtime_path =
        "/tmp/ray_tracing_runtime_scene_authored_texture_overlay_roundtrip.json";
    const char* texture_dir = "/tmp/ray_tracing_runtime_scene_authored_texture_overlay_roundtrip_set";
    const char* texture_png =
        "/tmp/ray_tracing_runtime_scene_authored_texture_overlay_roundtrip_set/box_stack_front.png";
    const char* texture_manifest =
        "/tmp/ray_tracing_runtime_scene_authored_texture_overlay_roundtrip_set/box_stack_texture_manifest.json";
    const char* texture_manifest_rel =
        "ray_tracing_runtime_scene_authored_texture_overlay_roundtrip_set/box_stack_texture_manifest.json";
    const char* runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_authoring_authored_overlay_1\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[{"
          "\"object_id\":\"box_stack_authored\","
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
    char* persisted_json = NULL;
    FILE* file = fopen(runtime_path, "wb");
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeSceneBridgePreflight reopen_summary = {0};
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialTextureStack hydrated = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialPayload3D authored_only = {0};
    RuntimeMaterialPayload3D authored_overlay = {0};
    HitInfo3D hit = {0};
    unsigned char texture_rgba[] = {248u, 28u, 18u, 255u};
    char authored_manifest_path[RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY];
    char authored_binding_mode[RUNTIME_MATERIAL_AUTHORED_TEXTURE_MODE_CAPACITY];
    int authored_face_count = 0;
    bool ok = false;

    assert_true("runtime_scene_authored_overlay_roundtrip_open_tmp", file != NULL);
    if (!file) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    fwrite(runtime_json, 1, strlen(runtime_json), file);
    fclose(file);
    (void)mkdir(texture_dir, 0775);
    assert_true("runtime_scene_authored_overlay_roundtrip_png_write_ok",
                test_scene_editor_write_png_rgba(texture_png, texture_rgba, 1u, 1u));
    assert_true("runtime_scene_authored_overlay_roundtrip_manifest_write_ok",
                test_scene_editor_write_authored_texture_manifest(texture_manifest,
                                                                  "box_stack_authored",
                                                                  "RECT_PRISM",
                                                                  "FRONT",
                                                                  "box_stack_front.png"));

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialFacePlacementResetAll();
    SceneEditorMaterialStackResetAll();
    RuntimeMaterialAuthoredTextureResetAll();

    ok = runtime_scene_bridge_apply_file(runtime_path, &summary);
    assert_true("runtime_scene_authored_overlay_roundtrip_apply_ok", ok);
    if (!ok) {
        unlink(texture_png);
        unlink(texture_manifest);
        rmdir(texture_dir);
        unlink(runtime_path);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath), "%s", runtime_path);
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_ROUGH_METAL;
    sceneSettings.sceneObjects[0].color = 0x707882;
    sceneSettings.sceneObjects[0].textureId = RUNTIME_MATERIAL_TEXTURE_3D_NONE;
    sceneSettings.sceneObjects[0].textureStrength = 0.0;

    stack.layerCount = 2;
    stack.layers[0] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD);
    stack.layers[0].placement.scale = 2.0;
    stack.layers[1] =
        RuntimeMaterialTextureLayerMakeOverlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME);
    stack.layers[1].opacity = 1.0;
    stack.layers[1].placement.strength = 1.0;
    stack.layers[1].placement.scale = 1.5;
    stack.layers[1].params.coverage = 1.0;
    stack.layers[1].params.grain = 0.85;
    stack.layers[1].roughnessInfluence = 0.75;
    assert_true("runtime_scene_authored_overlay_roundtrip_stack_seeded",
                SceneEditorMaterialStackSetObjectStack(0, &stack));
    assert_true("runtime_scene_authored_overlay_roundtrip_bind_ok",
                RuntimeMaterialAuthoredTextureBindManifestForObject(0,
                                                                   "box_stack_authored",
                                                                   texture_manifest_rel,
                                                                   "override"));

    ok = SceneEditorRuntimeScenePersistAuthoring(diagnostics, sizeof(diagnostics));
    assert_true("runtime_scene_authored_overlay_roundtrip_persist_ok", ok);
    if (!ok) {
        unlink(texture_png);
        unlink(texture_manifest);
        rmdir(texture_dir);
        unlink(runtime_path);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    persisted_json = read_text_file_alloc(runtime_path, NULL);
    assert_true("runtime_scene_authored_overlay_roundtrip_readback_ok", persisted_json != NULL);
    if (persisted_json) {
        assert_true("runtime_scene_authored_overlay_roundtrip_has_stack_json",
                    strstr(persisted_json, "\"material_texture_stack\"") != NULL);
        assert_true("runtime_scene_authored_overlay_roundtrip_has_authored_texture",
                    strstr(persisted_json, "\"authored_texture\"") != NULL);
        assert_true("runtime_scene_authored_overlay_roundtrip_has_manifest_path",
                    strstr(persisted_json, texture_manifest_rel) != NULL);
    }
    free(persisted_json);
    persisted_json = NULL;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialFacePlacementResetAll();
    SceneEditorMaterialStackResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    ok = runtime_scene_bridge_apply_file(runtime_path, &reopen_summary);
    assert_true("runtime_scene_authored_overlay_roundtrip_reopen_ok", ok);
    if (!ok) {
        unlink(texture_png);
        unlink(texture_manifest);
        rmdir(texture_dir);
        unlink(runtime_path);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    assert_true("runtime_scene_authored_overlay_roundtrip_binding_summary_ok",
                RuntimeMaterialAuthoredTextureGetBinding(0,
                                                        authored_manifest_path,
                                                        sizeof(authored_manifest_path),
                                                        authored_binding_mode,
                                                        sizeof(authored_binding_mode),
                                                        &authored_face_count));
    assert_true("runtime_scene_authored_overlay_roundtrip_binding_face_count",
                authored_face_count == 6);
    assert_true("runtime_scene_authored_overlay_roundtrip_binding_path",
                strcmp(authored_manifest_path, texture_manifest_rel) == 0);
    assert_true("runtime_scene_authored_overlay_roundtrip_hydrated_stack",
                SceneEditorMaterialStackGetObjectStack(0, &hydrated));
    assert_true("runtime_scene_authored_overlay_roundtrip_hydrated_layer_count",
                hydrated.layerCount == 2);
    assert_true("runtime_scene_authored_overlay_roundtrip_hydrated_overlay_kind",
                hydrated.layers[1].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME);

    HitInfo3D_Reset(&hit);
    hit.sceneObjectIndex = 0;
    hit.triangleIndex = 0;
    hit.localTriangleIndex = 0;
    hit.primitiveIndex = 0;
    hit.baryU = 0.2;
    hit.baryV = 0.3;
    hit.baryW = 0.5;

    assert_true("runtime_scene_authored_overlay_roundtrip_overlay_payload_ok",
                RuntimeMaterialPayload3D_ResolveFromHit(&hit, &authored_overlay));
    assert_true("runtime_scene_authored_overlay_roundtrip_overlay_payload_mask",
                authored_overlay.textureMask > 0.0);
    assert_true("runtime_scene_authored_overlay_roundtrip_overlay_red_dominant",
                authored_overlay.baseColorR > authored_overlay.baseColorG &&
                    authored_overlay.baseColorR > authored_overlay.baseColorB);

    assert_true("runtime_scene_authored_overlay_roundtrip_clear_stack_ok",
                SceneEditorMaterialStackClearObjectStack(0));
    assert_true("runtime_scene_authored_overlay_roundtrip_authored_only_payload_ok",
                RuntimeMaterialPayload3D_ResolveFromHit(&hit, &authored_only));
    assert_true("runtime_scene_authored_overlay_roundtrip_authored_only_red_dominant",
                authored_only.baseColorR > authored_only.baseColorG &&
                    authored_only.baseColorR > authored_only.baseColorB);
    assert_true("runtime_scene_authored_overlay_roundtrip_overlay_changes_response",
                authored_overlay.baseColorR < authored_only.baseColorR - 1e-6 ||
                    authored_overlay.baseColorG < authored_only.baseColorG - 1e-6 ||
                    authored_overlay.baseColorB < authored_only.baseColorB - 1e-6 ||
                    authored_overlay.bsdf.roughness > authored_only.bsdf.roughness + 1e-6);

    unlink(texture_png);
    unlink(texture_manifest);
    rmdir(texture_dir);
    unlink(runtime_path);
    SceneEditorMaterialStackResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    SceneEditorMaterialFacePlacementResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_object_editor_material_assignment_preserves_object_color(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    sceneSettings.objectCount = 1;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_DEFAULT;
    sceneSettings.sceneObjects[0].color = 0x00FF00;

    ObjectEditorObjectAssignMaterial(&sceneSettings.sceneObjects[0], MATERIAL_PRESET_GLOSSY);

    assert_true("object_editor_assign_material_updates_material_id",
                sceneSettings.sceneObjects[0].material_id == MATERIAL_PRESET_GLOSSY);
    assert_true("object_editor_assign_material_preserves_color",
                sceneSettings.sceneObjects[0].color == 0x00FF00);

    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_object_editor_slider_assignments_update_object_fields(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    sceneSettings.objectCount = 1;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[0].alpha = 1.0;
    sceneSettings.sceneObjects[0].emissiveStrength = 1.0;

    ObjectEditorObjectAssignAlpha(&sceneSettings.sceneObjects[0], 0.25);
    ObjectEditorObjectAssignEmissiveStrength(&sceneSettings.sceneObjects[0], 0.75);

    assert_close("object_editor_assign_alpha_updates_object",
                 sceneSettings.sceneObjects[0].alpha,
                 0.25,
                 1e-9);
    assert_close("object_editor_assign_emissive_strength_updates_object",
                 sceneSettings.sceneObjects[0].emissiveStrength,
                 0.75,
                 1e-9);

    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_focuses_last_selected_and_updates_texture_fields(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    sceneSettings.objectCount = 2;
    sceneSettings.sceneObjects[0].textureScale = 1.0;
    sceneSettings.sceneObjects[1].textureScale = 1.0;
    SceneEditorMaterialStackResetAll();
    MaterialEditorSetSolidFacesEnabled(true);

    ObjectEditorSelectionTrackerSetCurrent(1, sceneSettings.objectCount);
    ObjectEditorSelectionTrackerSetCurrent(-1, sceneSettings.objectCount);
    InitializeMaterialEditor();

    assert_true("material_editor_solid_faces_default_on",
                MaterialEditorGetSolidFacesEnabled());
    assert_true("material_editor_solid_faces_toggle_off",
                !MaterialEditorToggleSolidFaces());
    assert_true("material_editor_solid_faces_set_on",
                (MaterialEditorSetSolidFacesEnabled(true), MaterialEditorGetSolidFacesEnabled()));
    assert_true("material_editor_focuses_last_selected",
                MaterialEditorResolveFocusedObjectIndex() == 1);
    assert_true("material_editor_applies_rust_kind",
                MaterialEditorApplyTextureKindToFocused(1));
    assert_true("material_editor_texture_kind_promotes_stack",
                SceneEditorMaterialStackGetObjectStack(1, &stack));
    assert_true("material_editor_texture_kind_stack_rust",
                stack.layerCount == 2 &&
                    stack.layers[1].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST);
    assert_true("material_editor_strength_slider_updates",
                MaterialEditorApplySliderValueToFocused(MATERIAL_EDITOR_SLIDER_STRENGTH, 0.75));
    assert_close("material_editor_strength_value",
                 (SceneEditorMaterialStackGetObjectStack(1, &stack) ? stack.layers[1].placement.strength : -1.0),
                 0.75,
                 1e-9);
    assert_true("material_editor_scale_slider_updates",
                MaterialEditorApplySliderValueToFocused(MATERIAL_EDITOR_SLIDER_SCALE, 1.0));
    assert_close("material_editor_scale_value",
                 (SceneEditorMaterialStackGetObjectStack(1, &stack) ? stack.layers[1].placement.scale : -1.0),
                 8.0,
                 1e-9);
    assert_true("material_editor_offset_u_updates",
                MaterialEditorApplySliderValueToFocused(MATERIAL_EDITOR_SLIDER_OFFSET_U, 0.20));
    assert_true("material_editor_offset_v_updates",
                MaterialEditorApplySliderValueToFocused(MATERIAL_EDITOR_SLIDER_OFFSET_V, 0.65));
    assert_close("material_editor_offset_u_value",
                 (SceneEditorMaterialStackGetObjectStack(1, &stack) ? stack.layers[1].placement.offsetU : -1.0),
                 0.20,
                 1e-9);
    assert_close("material_editor_offset_v_value",
                 (SceneEditorMaterialStackGetObjectStack(1, &stack) ? stack.layers[1].placement.offsetV : -1.0),
                 0.65,
                 1e-9);
    assert_true("material_editor_param_pattern_updates",
                MaterialEditorApplyTexturePatternToFocused(
                    RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_FLOW));
    assert_true("material_editor_param_coverage_updates",
                MaterialEditorApplyTextureParamValueToFocused(
                    MATERIAL_EDITOR_TEXTURE_PARAM_COVERAGE,
                    0.42));
    assert_true("material_editor_param_damage_updates",
                MaterialEditorApplyTextureParamValueToFocused(
                    MATERIAL_EDITOR_TEXTURE_PARAM_SURFACE_DAMAGE,
                    0.83));
    assert_true("material_editor_param_pattern_value",
                SceneEditorMaterialStackGetObjectStack(1, &stack) &&
                    stack.layers[1].params.patternMode ==
                    RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_FLOW);
    assert_close("material_editor_param_coverage_value",
                 (SceneEditorMaterialStackGetObjectStack(1, &stack) ? stack.layers[1].params.coverage : -1.0),
                 0.42,
                 1e-9);
    assert_close("material_editor_param_damage_value",
                 (SceneEditorMaterialStackGetObjectStack(1, &stack) ? stack.layers[1].params.surfaceDamage : -1.0),
                 0.83,
                 1e-9);
    assert_true("material_editor_rejects_empty_param_kind",
                !MaterialEditorApplyTextureParamValueToFocused(
                    MATERIAL_EDITOR_TEXTURE_PARAM_NONE,
                    0.99));
    assert_close("material_editor_empty_param_kind_keeps_coverage",
                 (SceneEditorMaterialStackGetObjectStack(1, &stack) ? stack.layers[1].params.coverage : -1.0),
                 0.42,
                 1e-9);
    assert_true("material_editor_marks_object_dirty",
                sceneSettings.sceneObjects[1].dirty);

    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_object_scope_clears_face_overrides_and_applies_all_faces(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    SceneEditorMaterialPreviewTriangleAddress address = {0};
    SceneEditorMaterialFacePlacement placement;
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    SDL_Color base = {128, 128, 132, 255};
    SDL_Color sampled = {0, 0, 0, 0};
    double mask = 0.0;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialFacePlacementResetAll();
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_POLYGON, 0.0, 0.0, 1.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].textureId = RUNTIME_MATERIAL_TEXTURE_3D_NONE;
    sceneSettings.sceneObjects[0].textureStrength = 1.0;
    sceneSettings.sceneObjects[0].textureScale = 2.0;

    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();

    address.sceneObjectIndex = 0;
    address.primitiveIndex = 0;
    address.triangleIndex = 4;
    address.localTriangleIndex = 4;
    address.faceGroupIndex = 2;
    assert_true("material_object_scope_face_select",
                MaterialEditorSetTriangleSelection(&address));
    assert_true("material_object_scope_face_rust_override",
                MaterialEditorApplyTextureKindToFocused(RUNTIME_MATERIAL_TEXTURE_3D_RUST));
    assert_true("material_object_scope_face_override_exists",
                SceneEditorMaterialFacePlacementHasOverride(0, 2));
    placement = SceneEditorMaterialFacePlacementGetEffective(&sceneSettings.sceneObjects[0], 0, 2);
    assert_true("material_object_scope_face_override_rust",
                placement.textureId == RUNTIME_MATERIAL_TEXTURE_3D_RUST);

    MaterialEditorClearTriangleSelection();
    assert_true("material_object_scope_none_applies_to_all",
                MaterialEditorApplyTextureKindToFocused(RUNTIME_MATERIAL_TEXTURE_3D_NONE));
    assert_true("material_object_scope_face_override_cleared",
                !SceneEditorMaterialFacePlacementHasOverride(0, 2));
    assert_true("material_object_scope_stack_is_solid_only",
                SceneEditorMaterialStackGetObjectStack(0, &stack) &&
                    stack.layerCount == 1 &&
                    stack.layers[0].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID);
    assert_true("material_object_scope_preview_has_no_texture",
                !SceneEditorMaterialPreviewEvaluateTextureColorForFace(
                    &sceneSettings.sceneObjects[0],
                    0,
                    0,
                    2,
                    0,
                    4,
                    0.50,
                    0.25,
                    0.25,
                    base,
                    &sampled,
                    &mask));
    assert_close("material_object_scope_preview_mask_zero", mask, 0.0, 1e-12);

    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialFacePlacementResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_layer_list_routes_object_stack_controls(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_POLYGON, 0.0, 0.0, 1.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].textureId = RUNTIME_MATERIAL_TEXTURE_3D_RUST;
    sceneSettings.sceneObjects[0].textureStrength = 1.0;
    sceneSettings.sceneObjects[0].textureScale = 2.0;
    sceneSettings.sceneObjects[0].textureCoverage = 0.50;

    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();

    assert_true("material_editor_layer_count_from_legacy",
                MaterialEditorFocusedLayerCount() == 2);
    assert_true("material_editor_layer_select_overlay",
                MaterialEditorSetActiveLayerIndex(1));
    assert_true("material_editor_layer_select_promotes_v2",
                SceneEditorMaterialStackHasObjectStack(0));
    assert_true("material_editor_layer_strength_routes_to_stack",
                MaterialEditorApplySliderValueToFocused(MATERIAL_EDITOR_SLIDER_STRENGTH, 0.35));
    assert_true("material_editor_layer_param_routes_to_stack",
                MaterialEditorApplyTextureParamValueToFocused(
                    MATERIAL_EDITOR_TEXTURE_PARAM_COVERAGE,
                    0.80));
    assert_true("material_editor_layer_kind_routes_to_stack",
                MaterialEditorApplyLayerKindToFocused(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME));
    assert_close("material_editor_layer_keeps_legacy_strength",
                 sceneSettings.sceneObjects[0].textureStrength,
                 1.0,
                 1e-12);
    assert_true("material_editor_layer_stack_readback",
                SceneEditorMaterialStackGetObjectStack(0, &stack));
    assert_close("material_editor_layer_strength_stack_value",
                 stack.layers[1].placement.strength,
                 0.35,
                 1e-12);
    assert_close("material_editor_layer_kind_default_coverage",
                 stack.layers[1].params.coverage,
                 0.58,
                 1e-12);
    assert_true("material_editor_layer_kind_default_pattern",
                stack.layers[1].params.patternMode == RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_FLOW);
    assert_true("material_editor_layer_kind_stack_value",
                stack.layers[1].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME);

    assert_true("material_editor_layer_add_overlay",
                MaterialEditorAddOverlayLayerToFocused());
    assert_true("material_editor_layer_add_count",
                MaterialEditorFocusedLayerCount() == 3);
    assert_true("material_editor_layer_add_active_last",
                MaterialEditorGetActiveLayerIndex() == 2);
    assert_true("material_editor_layer_move_up",
                MaterialEditorMoveActiveLayer(-1));
    assert_true("material_editor_layer_move_active_index",
                MaterialEditorGetActiveLayerIndex() == 1);
    assert_true("material_editor_layer_toggle_enabled",
                MaterialEditorToggleActiveLayerEnabled());
    assert_true("material_editor_layer_toggle_readback",
                SceneEditorMaterialStackGetObjectStack(0, &stack) &&
                    !stack.layers[1].enabled);
    assert_true("material_editor_layer_delete",
                MaterialEditorDeleteActiveLayer());
    assert_true("material_editor_layer_delete_count",
                MaterialEditorFocusedLayerCount() == 2);

    SceneEditorMaterialStackResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_object_projector_centers_focused_object(void) {
    RuntimeSceneBridge3DDigestState digest;
    SceneEditorDigestOverlayNavState nav_state;
    SceneEditorDigestOverlayProjector projector;
    SceneEditorDigestOverlayProjector scene_projector;
    SDL_Rect viewport = {0, 0, 800, 600};

    memset(&digest, 0, sizeof(digest));
    memset(&nav_state, 0, sizeof(nav_state));
    memset(&projector, 0, sizeof(projector));
    memset(&scene_projector, 0, sizeof(scene_projector));

    digest.valid = true;
    digest.primitive_count = 2;
    digest.primitives[0].kind = RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM;
    digest.primitives[0].scene_object_index = 7;
    digest.primitives[0].origin_x = -100.0;
    digest.primitives[0].origin_y = 0.0;
    digest.primitives[0].origin_z = 0.0;
    digest.primitives[0].has_dimensions = true;
    digest.primitives[0].width = 20.0;
    digest.primitives[0].height = 20.0;
    digest.primitives[0].depth = 20.0;
    digest.primitives[1].kind = RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM;
    digest.primitives[1].scene_object_index = 8;
    digest.primitives[1].origin_x = 50.0;
    digest.primitives[1].origin_y = 4.0;
    digest.primitives[1].origin_z = -2.0;
    digest.primitives[1].has_dimensions = true;
    digest.primitives[1].width = 10.0;
    digest.primitives[1].height = 6.0;
    digest.primitives[1].depth = 4.0;
    nav_state.overlay_zoom = 0.05;

    MaterialEditorSetViewMode(MATERIAL_EDITOR_VIEW_SCENE_PLACEMENT);
    assert_true("material_editor_view_mode_scene_placement",
                MaterialEditorGetViewMode() == MATERIAL_EDITOR_VIEW_SCENE_PLACEMENT);
    assert_true("material_editor_scene_projector_path_available",
                SceneEditorDigestOverlayBuildObjectProjector(&digest,
                                                             &viewport,
                                                             &nav_state,
                                                             8,
                                                             false,
                                                             &scene_projector));

    MaterialEditorSetViewMode(MATERIAL_EDITOR_VIEW_FOCUSED_ORIGIN);
    assert_true("material_editor_view_mode_focused_origin",
                MaterialEditorGetViewMode() == MATERIAL_EDITOR_VIEW_FOCUSED_ORIGIN);
    assert_true("material_editor_focused_object_projector_builds",
                SceneEditorDigestOverlayBuildObjectProjector(&digest,
                                                             &viewport,
                                                             &nav_state,
                                                             8,
                                                             true,
                                                             &projector));
    assert_close("material_editor_projector_center_x", projector.center_x, 50.0, 1e-9);
    assert_close("material_editor_projector_center_y", projector.center_y, 4.0, 1e-9);
    assert_close("material_editor_projector_center_z", projector.center_z, -2.0, 1e-9);
    assert_close("material_editor_projector_span", projector.span_max, 10.0, 1e-9);

    return 0;
}

static int test_material_editor_focused_zoom_accumulates_around_object_fit(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_material_zoom_focus\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"far_block\","
            "\"object_type\":\"rect_prism_primitive\","
            "\"transform\":{\"position\":{\"x\":-120.0,\"y\":0.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"primitive\":{\"kind\":\"rect_prism_primitive\","
              "\"width\":12.0,\"height\":12.0,\"depth\":12.0}"
          "},"
          "{"
            "\"object_id\":\"focus_block\","
            "\"object_type\":\"rect_prism_primitive\","
            "\"transform\":{\"position\":{\"x\":40.0,\"y\":3.0,\"z\":-1.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"primitive\":{\"kind\":\"rect_prism_primitive\","
              "\"width\":4.0,\"height\":3.0,\"depth\":2.0}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":4.0,\"z\":20.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    SceneEditorDigestOverlayNavState nav_state;
    SDL_Rect viewport = {0, 0, 800, 600};
    double fit_zoom = 0.0;
    double zoom_once = 0.0;
    double zoom_twice = 0.0;
    double zoom_far = 0.0;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = 1;
    MaterialEditorSetViewMode(MATERIAL_EDITOR_VIEW_FOCUSED_ORIGIN);
    assert_true("material_zoom_runtime_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json, &summary));

    memset(&nav_state, 0, sizeof(nav_state));
    assert_true("material_zoom_fit_focused_object",
                SceneEditorViewportNavFitDigestOverlayForTarget(&nav_state,
                                                                &viewport,
                                                                true,
                                                                EDITOR_MODE_MATERIAL,
                                                                1));
    fit_zoom = nav_state.overlay_zoom;
    assert_true("material_zoom_fit_positive", fit_zoom > 0.0);

    assert_true("material_zoom_wheel_once",
                SceneEditorViewportNavApplyDigestWheelZoom(&nav_state,
                                                           &viewport,
                                                           1,
                                                           EDITOR_MODE_MATERIAL,
                                                           1));
    zoom_once = nav_state.overlay_zoom;
    assert_true("material_zoom_first_step_increases", zoom_once > fit_zoom * 1.20);

    assert_true("material_zoom_wheel_twice",
                SceneEditorViewportNavApplyDigestWheelZoom(&nav_state,
                                                           &viewport,
                                                           1,
                                                           EDITOR_MODE_MATERIAL,
                                                           1));
    zoom_twice = nav_state.overlay_zoom;
    assert_true("material_zoom_accumulates_without_refit_reset",
                zoom_twice > zoom_once * 1.20);

    assert_true("material_zoom_wheel_far_out",
                SceneEditorViewportNavApplyDigestWheelZoom(&nav_state,
                                                           &viewport,
                                                           -40,
                                                           EDITOR_MODE_MATERIAL,
                                                           1));
    zoom_far = nav_state.overlay_zoom;
    assert_true("material_zoom_far_out_below_fit", zoom_far < fit_zoom);
    assert_true("material_zoom_far_out_keeps_positive", zoom_far > 0.0);

    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_preview_resolves_focused_triangle_substrate(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_material_triangle_preview\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"preview_plane\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":5.0,\"height\":3.0,"
              "\"frame\":{\"origin\":{\"x\":-6.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_v\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
              "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":-6.0,\"y\":0.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"preview_prism\","
            "\"object_type\":\"rect_prism_primitive\","
            "\"transform\":{\"position\":{\"x\":5.0,\"y\":0.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"primitive\":{\"kind\":\"rect_prism_primitive\","
              "\"width\":4.0,\"height\":3.0,\"depth\":2.0}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":4.0,\"z\":18.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeSceneBridge3DDigestState digest = {0};
    SceneEditorDigestOverlayNavState nav_state = {0};
    SceneEditorDigestOverlayProjector projector = {0};
    SDL_Rect viewport = {0, 0, 900, 650};
    SceneEditorMaterialPreviewTriangleAddress addresses
        [SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES];
    SceneEditorMaterialPreviewStats stats = {0};

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = 1;
    MaterialEditorSetViewMode(MATERIAL_EDITOR_VIEW_FOCUSED_ORIGIN);

    assert_true("material_preview_runtime_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json, &summary));
    runtime_scene_bridge_get_last_3d_digest_state(&digest);
    nav_state.overlay_zoom = 0.08;

    assert_true("material_preview_plane_projector",
                SceneEditorDigestOverlayBuildObjectProjector(&digest,
                                                             &viewport,
                                                             &nav_state,
                                                             0,
                                                             true,
                                                             &projector));
    memset(addresses, 0, sizeof(addresses));
    memset(&stats, 0, sizeof(stats));
    assert_true("material_preview_plane_triangles_resolve",
                SceneEditorMaterialPreviewResolveFocusedTriangles(0,
                                                                  &projector,
                                                                  addresses,
                                                                  SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES,
                                                                  &stats));
    assert_true("material_preview_plane_triangle_count", stats.triangleCount == 2);
    assert_true("material_preview_plane_face_group_count", stats.faceGroupCount == 1);
    assert_true("material_preview_plane_projected", stats.projected);
    assert_true("material_preview_plane_address_object", addresses[0].sceneObjectIndex == 0);
    assert_true("material_preview_plane_address_ordinal", addresses[1].localTriangleIndex == 1);

    assert_true("material_preview_prism_projector",
                SceneEditorDigestOverlayBuildObjectProjector(&digest,
                                                             &viewport,
                                                             &nav_state,
                                                             1,
                                                             true,
                                                             &projector));
    memset(addresses, 0, sizeof(addresses));
    memset(&stats, 0, sizeof(stats));
    assert_true("material_preview_prism_triangles_resolve",
                SceneEditorMaterialPreviewResolveFocusedTriangles(1,
                                                                  &projector,
                                                                  addresses,
                                                                  SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES,
                                                                  &stats));
    assert_true("material_preview_prism_triangle_count", stats.triangleCount == 12);
    assert_true("material_preview_prism_face_group_count", stats.faceGroupCount == 6);
    assert_true("material_preview_prism_projected", stats.projected);
    assert_true("material_preview_prism_address_object", addresses[0].sceneObjectIndex == 1);
    assert_true("material_preview_prism_last_face_group", addresses[11].faceGroupIndex == 5);

    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_face_metrics_ground_uv_scales_with_dimensions(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_material_face_metrics\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"plane_small\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":2.0,\"height\":1.0,"
              "\"frame\":{\"origin\":{\"x\":-8.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_v\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
              "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":-8.0,\"y\":0.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"plane_wide\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":5.0,\"height\":1.0,"
              "\"frame\":{\"origin\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_v\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
              "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"prism_grounded\","
            "\"object_type\":\"rect_prism_primitive\","
            "\"transform\":{\"position\":{\"x\":8.0,\"y\":0.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"primitive\":{\"kind\":\"rect_prism_primitive\","
              "\"width\":4.0,\"height\":3.0,\"depth\":2.0}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":4.0,\"z\":18.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    SceneEditorMaterialFaceMetrics metrics = {0};
    double grounded_small_u = 0.0;
    double grounded_small_v = 0.0;
    double grounded_wide_u = 0.0;
    double grounded_wide_v = 0.0;
    double grounded_prism_side_u = 0.0;
    double grounded_prism_side_v = 0.0;
    double grounded_prism_end_u = 0.0;
    double grounded_prism_end_v = 0.0;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = 1;

    assert_true("material_face_metrics_runtime_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json, &summary));

    assert_true("material_face_metrics_plane_small_resolve",
                SceneEditorMaterialFaceMetricsResolve(0, 0, 0, &metrics));
    assert_close("material_face_metrics_plane_small_width", metrics.width, 2.0, 1e-12);
    assert_close("material_face_metrics_plane_small_height", metrics.height, 1.0, 1e-12);
    assert_true("material_face_metrics_plane_small_ground",
                SceneEditorMaterialFaceMetricsResolveGroundedUV(0,
                                                                0,
                                                                0,
                                                                0.75,
                                                                0.50,
                                                                &grounded_small_u,
                                                                &grounded_small_v));

    assert_true("material_face_metrics_plane_wide_ground",
                SceneEditorMaterialFaceMetricsResolveGroundedUV(1,
                                                                1,
                                                                0,
                                                                0.75,
                                                                0.50,
                                                                &grounded_wide_u,
                                                                &grounded_wide_v));
    assert_true("material_face_metrics_plane_width_scales_grounded_u",
                grounded_wide_u > grounded_small_u);
    assert_close("material_face_metrics_plane_height_keeps_grounded_v",
                 grounded_wide_v,
                 grounded_small_v,
                 1e-12);

    assert_true("material_face_metrics_prism_side_resolve",
                SceneEditorMaterialFaceMetricsResolve(2, 2, 2, &metrics));
    assert_close("material_face_metrics_prism_side_width", metrics.width, 4.0, 1e-12);
    assert_close("material_face_metrics_prism_side_height", metrics.height, 2.0, 1e-12);
    assert_true("material_face_metrics_prism_end_resolve",
                SceneEditorMaterialFaceMetricsResolve(2, 2, 4, &metrics));
    assert_close("material_face_metrics_prism_end_width", metrics.width, 3.0, 1e-12);
    assert_close("material_face_metrics_prism_end_height", metrics.height, 2.0, 1e-12);
    assert_true("material_face_metrics_prism_side_ground",
                SceneEditorMaterialFaceMetricsResolveGroundedUV(2,
                                                                2,
                                                                2,
                                                                0.75,
                                                                0.75,
                                                                &grounded_prism_side_u,
                                                                &grounded_prism_side_v));
    assert_true("material_face_metrics_prism_end_ground",
                SceneEditorMaterialFaceMetricsResolveGroundedUV(2,
                                                                2,
                                                                4,
                                                                0.75,
                                                                0.75,
                                                                &grounded_prism_end_u,
                                                                &grounded_prism_end_v));
    assert_true("material_face_metrics_prism_width_changes_grounded_u",
                grounded_prism_side_u > grounded_prism_end_u);
    assert_close("material_face_metrics_prism_matching_depth_keeps_grounded_v",
                 grounded_prism_side_v,
                 grounded_prism_end_v,
                 1e-12);

    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_face_metrics_orients_vertical_faces_to_world_up(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_material_face_orientation\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"vertical_swapped\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":2.0,\"height\":6.0,"
              "\"frame\":{\"origin\":{\"x\":-4.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
              "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
              "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":-4.0,\"y\":0.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"horizontal_default\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":3.0,\"height\":5.0,"
              "\"frame\":{\"origin\":{\"x\":4.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_u\":{\"x\":0.0,\"y\":1.0,\"z\":0.0},"
              "\"axis_v\":{\"x\":-1.0,\"y\":0.0,\"z\":0.0},"
              "\"normal\":{\"x\":0.0,\"y\":0.0,\"z\":1.0}}},"
            "\"transform\":{\"position\":{\"x\":4.0,\"y\":0.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":4.0,\"z\":18.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    SceneEditorMaterialFaceMetrics metrics = {0};
    double grounded_u = 0.0;
    double grounded_v = 0.0;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = 1;

    assert_true("material_face_metrics_orientation_runtime_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json, &summary));

    assert_true("material_face_metrics_orientation_vertical_resolve",
                SceneEditorMaterialFaceMetricsResolve(0, 0, 0, &metrics));
    assert_true("material_face_metrics_orientation_vertical_swaps_axes", metrics.swapAxes);
    assert_true("material_face_metrics_orientation_vertical_keeps_up", !metrics.flipV);
    assert_close("material_face_metrics_orientation_vertical_width", metrics.width, 6.0, 1e-12);
    assert_close("material_face_metrics_orientation_vertical_height", metrics.height, 2.0, 1e-12);
    assert_true("material_face_metrics_orientation_vertical_ground",
                SceneEditorMaterialFaceMetricsResolveGroundedUV(0,
                                                                0,
                                                                0,
                                                                0.25,
                                                                0.75,
                                                                &grounded_u,
                                                                &grounded_v));
    assert_close("material_face_metrics_orientation_vertical_ground_u",
                 grounded_u,
                 2.0,
                 1e-12);
    assert_close("material_face_metrics_orientation_vertical_ground_v",
                 grounded_v,
                 0.0,
                 1e-12);

    assert_true("material_face_metrics_orientation_horizontal_resolve",
                SceneEditorMaterialFaceMetricsResolve(1, 1, 0, &metrics));
    assert_true("material_face_metrics_orientation_horizontal_swaps_axes", metrics.swapAxes);
    assert_true("material_face_metrics_orientation_horizontal_flips_u", metrics.flipU);
    assert_true("material_face_metrics_orientation_horizontal_keeps_v", !metrics.flipV);
    assert_close("material_face_metrics_orientation_horizontal_width", metrics.width, 5.0, 1e-12);
    assert_close("material_face_metrics_orientation_horizontal_height", metrics.height, 3.0, 1e-12);

    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_face_preview_display_size_respects_face_aspect(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_material_face_preview_display\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"wide_plane\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":5.0,\"height\":2.0,"
              "\"frame\":{\"origin\":{\"x\":-4.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_v\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
              "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":-4.0,\"y\":0.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"tall_plane\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":1.0,\"height\":5.0,"
              "\"frame\":{\"origin\":{\"x\":4.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_v\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
              "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":4.0,\"y\":0.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":4.0,\"z\":18.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    int wide_width = 0;
    int wide_height = 0;
    int tall_width = 0;
    int tall_height = 0;
    int neutral_width = 0;
    int neutral_height = 0;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = 1;
    sceneSettings.objectCount = 2;

    assert_true("material_face_preview_display_runtime_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json, &summary));
    assert_true("material_face_preview_display_wide",
                MaterialEditorFacePreviewResolveDisplaySize(&sceneSettings.sceneObjects[0],
                                                           0,
                                                           0,
                                                           240,
                                                           &wide_width,
                                                           &wide_height));
    assert_true("material_face_preview_display_tall",
                MaterialEditorFacePreviewResolveDisplaySize(&sceneSettings.sceneObjects[1],
                                                           1,
                                                           0,
                                                           240,
                                                           &tall_width,
                                                           &tall_height));
    assert_true("material_face_preview_display_neutral",
                MaterialEditorFacePreviewResolveDisplaySize(&sceneSettings.sceneObjects[0],
                                                           0,
                                                           -1,
                                                           240,
                                                           &neutral_width,
                                                           &neutral_height));
    assert_true("material_face_preview_display_wide_landscape", wide_width > wide_height);
    assert_true("material_face_preview_display_tall_portrait", tall_height > tall_width);
    assert_true("material_face_preview_display_neutral_square",
                neutral_width == neutral_height);
    assert_true("material_face_preview_display_clamped_max",
                wide_width <= 176 && wide_height <= 176 &&
                tall_width <= 176 && tall_height <= 176);

    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_preview_picks_and_selects_visible_triangle(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_material_triangle_pick\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"pick_prism\","
            "\"object_type\":\"rect_prism_primitive\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"primitive\":{\"kind\":\"rect_prism_primitive\","
              "\"width\":4.0,\"height\":3.0,\"depth\":2.0}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":4.0,\"z\":18.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeSceneBridge3DDigestState digest = {0};
    SceneEditorDigestOverlayNavState nav_state = {0};
    SceneEditorDigestOverlayProjector projector = {0};
    SDL_Rect viewport = {0, 0, 900, 650};
    SceneEditorMaterialPreviewTriangleAddress picked = {0};
    SceneEditorMaterialPreviewTriangleAddress readback = {0};
    SceneEditorMaterialPreviewTriangleAddress second = {0};
    int center_x = viewport.x + viewport.w / 2;
    int center_y = viewport.y + viewport.h / 2;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = 1;
    MaterialEditorSetViewMode(MATERIAL_EDITOR_VIEW_FOCUSED_ORIGIN);
    assert_true("material_pick_runtime_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json, &summary));
    runtime_scene_bridge_get_last_3d_digest_state(&digest);
    nav_state.overlay_zoom = 0.10;
    nav_state.orbit_yaw_deg = 28.0;
    nav_state.orbit_pitch_deg = -22.0;

    assert_true("material_pick_object_projector",
                SceneEditorDigestOverlayBuildObjectProjector(&digest,
                                                             &viewport,
                                                             &nav_state,
                                                             0,
                                                             true,
                                                             &projector));
    assert_true("material_pick_center_triangle",
                SceneEditorMaterialPreviewPickTriangle(0,
                                                       &projector,
                                                       center_x,
                                                       center_y,
                                                       &picked));
    assert_true("material_pick_address_object", picked.sceneObjectIndex == 0);
    assert_true("material_pick_address_local_range",
                picked.localTriangleIndex >= 0 && picked.localTriangleIndex < 12);
    assert_true("material_pick_address_face_group_range",
                picked.faceGroupIndex >= 0 && picked.faceGroupIndex < 6);
    MaterialEditorSetFocusedObjectIndex(0);
    assert_true("material_pick_focused_face_group_count",
                MaterialEditorFocusedFaceGroupCount() == 6);

    MaterialEditorClearTriangleSelection();
    assert_true("material_selection_initial_empty",
                MaterialEditorSelectedTriangleCount() == 0);
    assert_true("material_selection_set_picked",
                MaterialEditorSetTriangleSelection(&picked));
    assert_true("material_selection_count_one",
                MaterialEditorSelectedTriangleCount() == 1);
    assert_true("material_selection_face_count_one",
                MaterialEditorSelectedFaceGroupCount() == 1);
    assert_true("material_selection_active_face_group",
                MaterialEditorGetActiveFaceGroupIndex() == picked.faceGroupIndex);
    assert_true("material_selection_readback",
                MaterialEditorGetSelectedTriangle(0, &readback));
    assert_true("material_selection_readback_matches",
                readback.triangleIndex == picked.triangleIndex &&
                readback.localTriangleIndex == picked.localTriangleIndex);

    assert_true("material_selection_toggle_removes",
                MaterialEditorToggleTriangleSelection(&picked));
    assert_true("material_selection_count_zero",
                MaterialEditorSelectedTriangleCount() == 0);
    assert_true("material_selection_active_group_cleared",
                MaterialEditorGetActiveFaceGroupIndex() == -1);

    second = picked;
    second.triangleIndex += 1;
    second.localTriangleIndex += 1;
    assert_true("material_selection_toggle_first_adds",
                MaterialEditorToggleTriangleSelection(&picked));
    assert_true("material_selection_toggle_second_adds",
                MaterialEditorToggleTriangleSelection(&second));
    assert_true("material_selection_count_two",
                MaterialEditorSelectedTriangleCount() == 2);

    MaterialEditorClearTriangleSelection();
    assert_true("material_face_group_selection_sets_two_triangles",
                MaterialEditorSetFaceGroupSelection(&picked));
    assert_true("material_face_group_selection_triangle_count",
                MaterialEditorSelectedTriangleCount() == 2);
    assert_true("material_face_group_selection_face_count",
                MaterialEditorSelectedFaceGroupCount() == 1);
    assert_true("material_face_group_selection_active_group",
                MaterialEditorGetActiveFaceGroupIndex() == picked.faceGroupIndex);
    assert_true("material_face_group_set_active_valid",
                MaterialEditorSetActiveFaceGroupIndex(picked.faceGroupIndex));
    assert_true("material_face_group_rejects_unselected_active",
                !MaterialEditorSetActiveFaceGroupIndex(picked.faceGroupIndex + 100));
    assert_true("material_face_group_selection_by_index",
                MaterialEditorSetFaceGroupSelectionByIndex(picked.faceGroupIndex));
    assert_true("material_face_group_selection_by_index_active",
                MaterialEditorGetActiveFaceGroupIndex() == picked.faceGroupIndex);
    assert_true("material_face_group_toggle_removes_group",
                MaterialEditorToggleFaceGroupSelection(&picked));
    assert_true("material_face_group_toggle_count_zero",
                MaterialEditorSelectedTriangleCount() == 0);
    assert_true("material_face_group_toggle_active_cleared",
                MaterialEditorGetActiveFaceGroupIndex() == -1);

    MaterialEditorClearTriangleSelection();
    MaterialEditorSetFocusedObjectIndex(0);
    assert_true("material_canvas_pointer_selects",
                MaterialEditorHandleCanvasPointerDown(&projector, center_x, center_y, false));
    assert_true("material_canvas_pointer_selection_count",
                MaterialEditorSelectedTriangleCount() == 2);
    assert_true("material_canvas_pointer_face_count",
                MaterialEditorSelectedFaceGroupCount() == 1);
    assert_true("material_canvas_pointer_active_group",
                MaterialEditorGetActiveFaceGroupIndex() >= 0);
    assert_true("material_canvas_shift_pointer_toggles_face_off",
                MaterialEditorHandleCanvasPointerDown(&projector, center_x, center_y, true));
    assert_true("material_canvas_shift_pointer_selection_empty",
                MaterialEditorSelectedTriangleCount() == 0);
    assert_true("material_canvas_shift_pointer_active_empty",
                MaterialEditorGetActiveFaceGroupIndex() == -1);

    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static bool material_preview_color_differs(SDL_Color a, SDL_Color b) {
    return a.r != b.r || a.g != b.g || a.b != b.b || a.a != b.a;
}

static int test_material_editor_preview_texture_color_responds_to_controls(void) {
    SceneObject object;
    SDL_Color base = {128, 128, 132, 235};
    SDL_Color none = {0, 0, 0, 0};
    SDL_Color rust = {0, 0, 0, 0};
    SDL_Color rust_panned = {0, 0, 0, 0};
    SDL_Color fog = {0, 0, 0, 0};
    double mask = 0.0;
    double rust_mask = 0.0;
    double rust_panned_mask = 0.0;
    double fog_mask = 0.0;
    bool found_rust = false;
    bool found_pan_difference = false;
    bool found_fog = false;

    memset(&object, 0, sizeof(object));
    object.color = 0x808084;
    object.textureScale = 1.0;
    object.textureStrength = 0.0;
    object.textureId = RUNTIME_MATERIAL_TEXTURE_3D_RUST;
    assert_true("material_preview_texture_strength_zero_inactive",
                !SceneEditorMaterialPreviewEvaluateTextureColor(&object,
                                                                3,
                                                                0.25,
                                                                0.35,
                                                                base,
                                                                &none,
                                                                &mask));
    assert_true("material_preview_texture_strength_zero_base",
                !material_preview_color_differs(base, none));
    assert_close("material_preview_texture_strength_zero_mask", mask, 0.0, 1e-12);

    object.textureStrength = 1.0;
    object.textureScale = 2.5;
    object.textureOffsetU = 0.0;
    object.textureOffsetV = 0.0;
    for (int u = 1; u < 9 && !found_rust; ++u) {
        for (int v = 1; v < 9 && !found_rust; ++v) {
            double bary_v = (double)u / 10.0;
            double bary_w = (double)v / 10.0;
            if (bary_v + bary_w >= 0.95) continue;
            found_rust = SceneEditorMaterialPreviewEvaluateTextureColor(&object,
                                                                        3,
                                                                        bary_v,
                                                                        bary_w,
                                                                        base,
                                                                        &rust,
                                                                        &rust_mask);
        }
    }
    assert_true("material_preview_rust_finds_active_sample", found_rust);
    assert_true("material_preview_rust_changes_color", material_preview_color_differs(base, rust));
    assert_true("material_preview_rust_mask_positive", rust_mask > 0.0);

    for (int u = 1; u < 9 && !found_pan_difference; ++u) {
        for (int v = 1; v < 9 && !found_pan_difference; ++v) {
            double bary_v = (double)u / 10.0;
            double bary_w = (double)v / 10.0;
            SDL_Color unpanned = {0, 0, 0, 0};
            SDL_Color panned = {0, 0, 0, 0};
            double unpanned_mask = 0.0;
            double panned_mask = 0.0;
            if (bary_v + bary_w >= 0.95) continue;
            object.textureOffsetU = 0.0;
            object.textureOffsetV = 0.0;
            SceneEditorMaterialPreviewEvaluateTextureColor(&object,
                                                           4,
                                                           bary_v,
                                                           bary_w,
                                                           base,
                                                           &unpanned,
                                                           &unpanned_mask);
            object.textureOffsetU = 0.31;
            object.textureOffsetV = 0.17;
            SceneEditorMaterialPreviewEvaluateTextureColor(&object,
                                                           4,
                                                           bary_v,
                                                           bary_w,
                                                           base,
                                                           &panned,
                                                           &panned_mask);
            if (material_preview_color_differs(unpanned, panned) ||
                fabs(unpanned_mask - panned_mask) > 1e-9) {
                found_pan_difference = true;
                rust_panned = panned;
                rust_panned_mask = panned_mask;
            }
        }
    }
    assert_true("material_preview_pan_changes_sample", found_pan_difference);
    assert_true("material_preview_pan_mask_recorded", rust_panned_mask >= 0.0);
    assert_true("material_preview_pan_color_recorded",
                rust_panned.a == base.a || material_preview_color_differs(base, rust_panned));

    object.textureId = RUNTIME_MATERIAL_TEXTURE_3D_FOG;
    object.textureStrength = 1.0;
    object.textureScale = 2.0;
    object.textureOffsetU = 0.0;
    object.textureOffsetV = 0.0;
    for (int u = 1; u < 9 && !found_fog; ++u) {
        for (int v = 1; v < 9 && !found_fog; ++v) {
            double bary_v = (double)u / 10.0;
            double bary_w = (double)v / 10.0;
            if (bary_v + bary_w >= 0.95) continue;
            found_fog = SceneEditorMaterialPreviewEvaluateTextureColor(&object,
                                                                       5,
                                                                       bary_v,
                                                                       bary_w,
                                                                       base,
                                                                       &fog,
                                                                       &fog_mask);
        }
    }
    assert_true("material_preview_fog_finds_active_sample", found_fog);
    assert_true("material_preview_fog_changes_color", material_preview_color_differs(base, fog));
    assert_true("material_preview_fog_mask_positive", fog_mask > 0.0);
    return 0;
}

static int test_material_editor_preview_uses_v2_base_and_overlay_stack(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    SceneObject object;
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    SDL_Color base = {92, 94, 98, 255};
    SDL_Color legacy_color = {0, 0, 0, 0};
    SDL_Color stack_color = {0, 0, 0, 0};
    double legacy_mask = 0.0;
    double stack_mask = 0.0;
    bool found_stack_sample = false;

    memset(&object, 0, sizeof(object));
    object.color = 0x5c5e62;
    object.textureId = RUNTIME_MATERIAL_TEXTURE_3D_NONE;
    object.textureStrength = 0.0;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    sceneSettings.objectCount = 1;
    sceneSettings.sceneObjects[0] = object;

    stack.layerCount = 3;
    stack.layers[0] = RuntimeMaterialTextureLayerMakeBase(
        RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD);
    stack.layers[0].placement.scale = 2.0;
    stack.layers[0].placement.strength = 1.0;
    stack.layers[1] = RuntimeMaterialTextureLayerMakeOverlay(
        RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME);
    stack.layers[1].placement.scale = 3.0;
    stack.layers[1].placement.strength = 0.82;
    stack.layers[1].opacity = 0.90;
    stack.layers[2] = RuntimeMaterialTextureLayerMakeOverlay(
        RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL);
    stack.layers[2].placement.scale = 1.6;
    stack.layers[2].placement.strength = 0.65;
    stack.layers[2].opacity = 0.72;
    assert_true("material_preview_v2_stack_set",
                SceneEditorMaterialStackSetObjectStack(0, &stack));
    assert_true("material_preview_legacy_none_inactive",
                !SceneEditorMaterialPreviewEvaluateTextureColor(&object,
                                                                1,
                                                                0.25,
                                                                0.35,
                                                                base,
                                                                &legacy_color,
                                                                &legacy_mask));
    assert_true("material_preview_legacy_none_flat",
                !material_preview_color_differs(base, legacy_color));

    for (int u = 1; u < 9 && !found_stack_sample; ++u) {
        for (int v = 1; v < 9 && !found_stack_sample; ++v) {
            double bary_v = (double)u / 10.0;
            double bary_w = (double)v / 10.0;
            if (bary_v + bary_w >= 0.95) continue;
            found_stack_sample = SceneEditorMaterialPreviewEvaluateTextureColorForFace(
                &sceneSettings.sceneObjects[0],
                0,
                0,
                1,
                0,
                2,
                1.0 - bary_v - bary_w,
                bary_v,
                bary_w,
                base,
                &stack_color,
                &stack_mask);
        }
    }
    assert_true("material_preview_v2_stack_samples", found_stack_sample);
    assert_true("material_preview_v2_stack_changes_color",
                material_preview_color_differs(base, stack_color));
    assert_true("material_preview_v2_stack_mask_positive", stack_mask > 0.0);

    SceneEditorMaterialStackResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_preview_surface_eval_active_face_detail_preview_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    SceneObject object;
    RuntimeMaterialSurfaceEval object_eval = {0};
    RuntimeMaterialSurfaceEval face_eval = {0};
    RuntimeMaterialSurfaceEval second_face_eval = {0};
    Uint8 shade_r = 0u;
    Uint8 shade_g = 0u;
    Uint8 shade_b = 0u;
    bool found_face_detail = false;
    bool found_uv_variation = false;

    memset(&object, 0, sizeof(object));
    object.color = 0x6c7278;
    object.roughness = 0.18;
    object.reflectivity = 0.64;
    object.emissiveStrength = 0.12;
    object.alpha = 1.0;
    object.textureId = RUNTIME_MATERIAL_TEXTURE_3D_NONE;
    object.textureStrength = 0.0;
    object.textureScale = 1.0;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialFacePlacementResetAll();
    sceneSettings.objectCount = 1;
    sceneSettings.sceneObjects[0] = object;

    assert_true("material_preview_surface_eval_object_base",
                MaterialPreviewSurfaceEvaluateObject(&sceneSettings.sceneObjects[0],
                                                    0,
                                                    NULL,
                                                    0.25,
                                                    0.25,
                                                    &object_eval));
    assert_true("material_preview_surface_eval_face_override_kind",
                SceneEditorMaterialFacePlacementApplyTextureKind(&sceneSettings.sceneObjects[0],
                                                                 0,
                                                                 2,
                                                                 RUNTIME_MATERIAL_TEXTURE_3D_RUST));
    assert_true("material_preview_surface_eval_face_override_strength",
                SceneEditorMaterialFacePlacementApplyNormalizedValue(
                    &sceneSettings.sceneObjects[0],
                    0,
                    2,
                    SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_STRENGTH,
                    1.0));
    assert_true("material_preview_surface_eval_face_override_scale",
                SceneEditorMaterialFacePlacementApplyNormalizedValue(
                    &sceneSettings.sceneObjects[0],
                    0,
                    2,
                    SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_SCALE,
                    0.46));
    assert_true("material_preview_surface_eval_face_override_pattern",
                SceneEditorMaterialFacePlacementApplyTextureParamPatternMode(
                    &sceneSettings.sceneObjects[0],
                    0,
                    2,
                    RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_PATCH));
    assert_true("material_preview_surface_eval_face_override_damage",
                SceneEditorMaterialFacePlacementApplyTextureParamNormalizedValue(
                    &sceneSettings.sceneObjects[0],
                    0,
                    2,
                    SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_SURFACE_DAMAGE,
                    0.72));

    for (int u = 1; u < 9 && !found_face_detail; ++u) {
        for (int v = 1; v < 9 && !found_face_detail; ++v) {
            double sample_u = (double)u / 10.0;
            double sample_v = (double)v / 10.0;
            if (!MaterialPreviewSurfaceEvaluateFace(&sceneSettings.sceneObjects[0],
                                                    0,
                                                    2,
                                                    sample_u,
                                                    sample_v,
                                                    &face_eval)) {
                continue;
            }
            if (face_eval.textureMask > 0.0 &&
                (fabs(face_eval.colorR - object_eval.colorR) > 1e-6 ||
                 fabs(face_eval.colorG - object_eval.colorG) > 1e-6 ||
                 fabs(face_eval.colorB - object_eval.colorB) > 1e-6 ||
                 fabs(face_eval.roughness - object_eval.roughness) > 1e-6 ||
                 fabs(face_eval.reflectivity - object_eval.reflectivity) > 1e-6)) {
                found_face_detail = true;
            }
        }
    }
    assert_true("material_preview_surface_eval_face_detail_found", found_face_detail);

    assert_true("material_preview_surface_eval_face_first_sample",
                MaterialPreviewSurfaceEvaluateFace(&sceneSettings.sceneObjects[0],
                                                   0,
                                                   2,
                                                   0.15,
                                                   0.20,
                                                   &face_eval));
    for (int u = 2; u < 10 && !found_uv_variation; ++u) {
        for (int v = 2; v < 10 && !found_uv_variation; ++v) {
            double sample_u = (double)u / 10.0;
            double sample_v = (double)v / 10.0;
            if (!MaterialPreviewSurfaceEvaluateFace(&sceneSettings.sceneObjects[0],
                                                    0,
                                                    2,
                                                    sample_u,
                                                    sample_v,
                                                    &second_face_eval)) {
                continue;
            }
            if (fabs(face_eval.colorR - second_face_eval.colorR) > 1e-6 ||
                fabs(face_eval.colorG - second_face_eval.colorG) > 1e-6 ||
                fabs(face_eval.colorB - second_face_eval.colorB) > 1e-6 ||
                fabs(face_eval.textureMask - second_face_eval.textureMask) > 1e-6) {
                found_uv_variation = true;
            }
        }
    }
    assert_true("material_preview_surface_eval_face_uv_variation", found_uv_variation);

    MaterialPreviewSurfaceShadePixel(&face_eval,
                                     &sceneSettings.sceneObjects[0],
                                     0.15,
                                     0.20,
                                     232u,
                                     232u,
                                     232u,
                                     &shade_r,
                                     &shade_g,
                                     &shade_b);
    assert_true("material_preview_surface_eval_face_shade_visible",
                shade_r != 232u || shade_g != 232u || shade_b != 232u);

    SceneEditorMaterialFacePlacementResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_face_group_texture_island_and_override_controls(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    SceneObject object;
    SceneEditorMaterialPreviewTriangleAddress address = {0};
    SceneEditorMaterialPreviewTriangleAddress second_address = {0};
    SceneEditorMaterialFacePlacement placement;
    SDL_Color base = {128, 128, 132, 235};
    SDL_Color tri_a = {0, 0, 0, 0};
    SDL_Color tri_b = {0, 0, 0, 0};
    SDL_Color face_rust = {0, 0, 0, 0};
    double mask_a = 0.0;
    double mask_b = 0.0;
    double face_rust_mask = 0.0;
    bool found_face_rust = false;

    memset(&object, 0, sizeof(object));
    object.color = 0x808084;
    object.textureId = RUNTIME_MATERIAL_TEXTURE_3D_FOG;
    object.textureStrength = 1.0;
    object.textureScale = 2.0;
    object.textureOffsetU = 0.0;
    object.textureOffsetV = 0.0;
    SceneEditorMaterialFacePlacementResetAll();

    SceneEditorMaterialPreviewEvaluateTextureColorForFace(&object,
                                                          0,
                                                          0,
                                                          2,
                                                          0,
                                                          4,
                                                          0.75,
                                                          0.0,
                                                          0.25,
                                                          base,
                                                          &tri_a,
                                                          &mask_a);
    SceneEditorMaterialPreviewEvaluateTextureColorForFace(&object,
                                                          0,
                                                          0,
                                                          2,
                                                          1,
                                                          5,
                                                          0.75,
                                                          0.25,
                                                          0.0,
                                                          base,
                                                          &tri_b,
                                                          &mask_b);
    assert_true("material_face_island_color_continuous",
                !material_preview_color_differs(tri_a, tri_b));
    assert_close("material_face_island_mask_continuous", mask_a, mask_b, 1e-12);

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    sceneSettings.objectCount = 1;
    sceneSettings.sceneObjects[0] = object;
    MaterialEditorSetFocusedObjectIndex(0);
    address.sceneObjectIndex = 0;
    address.primitiveIndex = 0;
    address.triangleIndex = 4;
    address.localTriangleIndex = 4;
    address.faceGroupIndex = 2;
    assert_true("material_face_override_selects_active",
                MaterialEditorSetTriangleSelection(&address));
    assert_true("material_face_override_slider_applies",
                MaterialEditorApplySliderValueToFocused(MATERIAL_EDITOR_SLIDER_OFFSET_U, 0.31));
    assert_close("material_face_override_object_offset_unchanged",
                 sceneSettings.sceneObjects[0].textureOffsetU,
                 0.0,
                 1e-12);
    assert_true("material_face_override_exists",
                SceneEditorMaterialFacePlacementHasOverride(0, 2));
    placement = SceneEditorMaterialFacePlacementGetEffective(&sceneSettings.sceneObjects[0], 0, 2);
    assert_close("material_face_override_offset_value", placement.offsetU, 0.31, 1e-12);
    assert_true("material_face_override_inherits_texture_kind",
                placement.textureId == RUNTIME_MATERIAL_TEXTURE_3D_FOG);
    assert_true("material_face_texture_kind_applies_to_active",
                MaterialEditorApplyTextureKindToFocused(RUNTIME_MATERIAL_TEXTURE_3D_RUST));
    assert_true("material_face_texture_kind_object_unchanged",
                sceneSettings.sceneObjects[0].textureId == RUNTIME_MATERIAL_TEXTURE_3D_FOG);
    placement = SceneEditorMaterialFacePlacementGetEffective(&sceneSettings.sceneObjects[0], 0, 2);
    assert_true("material_face_texture_kind_override_value",
                placement.textureId == RUNTIME_MATERIAL_TEXTURE_3D_RUST);
    assert_true("material_face_param_pattern_applies",
                MaterialEditorApplyTexturePatternToFocused(
                    RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_PATCH));
    assert_true("material_face_param_damage_applies",
                MaterialEditorApplyTextureParamValueToFocused(
                    MATERIAL_EDITOR_TEXTURE_PARAM_SURFACE_DAMAGE,
                    0.88));
    placement = SceneEditorMaterialFacePlacementGetEffective(&sceneSettings.sceneObjects[0], 0, 2);
    assert_true("material_face_param_pattern_value",
                placement.params.patternMode == RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_PATCH);
    assert_close("material_face_param_damage_value",
                 placement.params.surfaceDamage,
                 0.88,
                 1e-12);
    sceneSettings.sceneObjects[0].textureId = RUNTIME_MATERIAL_TEXTURE_3D_NONE;
    assert_true("material_face_texture_override_active_without_object_texture",
                SceneEditorMaterialFacePlacementObjectHasActiveTextureOverride(0));
    for (int u = 1; u < 9 && !found_face_rust; ++u) {
        for (int v = 1; v < 9 && !found_face_rust; ++v) {
            double bary_v = (double)u / 10.0;
            double bary_w = (double)v / 10.0;
            if (bary_v + bary_w >= 0.95) continue;
            found_face_rust = SceneEditorMaterialPreviewEvaluateTextureColorForFace(
                &sceneSettings.sceneObjects[0],
                0,
                0,
                2,
                0,
                4,
                1.0 - bary_v - bary_w,
                bary_v,
                bary_w,
                base,
                &face_rust,
                &face_rust_mask);
        }
    }
    assert_true("material_face_texture_override_samples_without_object_texture",
                found_face_rust);
    assert_true("material_face_texture_override_changes_color",
                material_preview_color_differs(base, face_rust));
    second_address = address;
    second_address.triangleIndex = 6;
    second_address.localTriangleIndex = 6;
    second_address.faceGroupIndex = 3;
    assert_true("material_face_copy_adds_second_group",
                MaterialEditorToggleTriangleSelection(&second_address));
    assert_true("material_face_copy_restores_active_group",
                MaterialEditorSetActiveFaceGroupIndex(2));
    assert_true("material_face_copy_to_selected",
                MaterialEditorCopyActiveFacePlacementToSelected());
    assert_true("material_face_copy_target_override_exists",
                SceneEditorMaterialFacePlacementHasOverride(0, 3));
    placement = SceneEditorMaterialFacePlacementGetEffective(&sceneSettings.sceneObjects[0], 0, 3);
    assert_true("material_face_copy_target_texture_value",
                placement.textureId == RUNTIME_MATERIAL_TEXTURE_3D_RUST);
    assert_close("material_face_copy_target_offset_value", placement.offsetU, 0.31, 1e-12);
    assert_true("material_face_copy_target_pattern_value",
                placement.params.patternMode == RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_PATCH);
    assert_close("material_face_copy_target_damage_value",
                 placement.params.surfaceDamage,
                 0.88,
                 1e-12);
    assert_true("material_face_override_reset",
                MaterialEditorResetActiveFacePlacement());
    assert_true("material_face_override_reset_clears",
                !SceneEditorMaterialFacePlacementHasOverride(0, 2));

    SceneEditorMaterialFacePlacementResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_face_override_inherits_active_stack_layer(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    SceneEditorMaterialPreviewTriangleAddress address = {0};
    SceneEditorMaterialFacePlacement placement;
    SDL_Color base = {128, 128, 132, 235};
    SDL_Color sampled = {0, 0, 0, 0};
    double mask = 0.0;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialFacePlacementResetAll();
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_POLYGON, 0.0, 0.0, 1.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].color = 0x808084;

    stack.layers[0] = RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID);
    stack.layers[1] = RuntimeMaterialTextureLayerMakeOverlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST);
    stack.layers[1].placement.strength = 1.0;
    stack.layers[1].opacity = 1.0;
    stack.layerCount = 2;
    assert_true("material_face_stack_seed_set",
                SceneEditorMaterialStackSetObjectStack(0, &stack));

    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();
    assert_true("material_face_stack_active_overlay",
                MaterialEditorSetActiveLayerIndex(1));

    address.sceneObjectIndex = 0;
    address.primitiveIndex = 0;
    address.triangleIndex = 4;
    address.localTriangleIndex = 4;
    address.faceGroupIndex = 2;
    assert_true("material_face_stack_select_face",
                MaterialEditorSetTriangleSelection(&address));
    assert_true("material_face_stack_param_applies",
                MaterialEditorApplyTextureParamValueToFocused(
                    MATERIAL_EDITOR_TEXTURE_PARAM_COVERAGE,
                    0.81));
    assert_true("material_face_stack_selection_preserved",
                MaterialEditorSelectedTriangleCount() == 1);
    assert_true("material_face_stack_active_group_preserved",
                MaterialEditorGetActiveFaceGroupIndex() == 2);
    assert_true("material_face_stack_override_exists",
                SceneEditorMaterialFacePlacementHasOverride(0, 2));
    placement = SceneEditorMaterialFacePlacementGetEffective(&sceneSettings.sceneObjects[0], 0, 2);
    assert_true("material_face_stack_override_tracks_layer",
                placement.layerIndex == 1);
    assert_true("material_face_stack_override_keeps_rust_texture",
                placement.textureId == RUNTIME_MATERIAL_TEXTURE_3D_RUST);
    assert_close("material_face_stack_override_coverage_value",
                 placement.params.coverage,
                 0.81,
                 1e-12);
    assert_true("material_face_stack_preview_still_textured",
                SceneEditorMaterialPreviewEvaluateTextureColorForFace(
                    &sceneSettings.sceneObjects[0],
                    0,
                    0,
                    2,
                    0,
                    4,
                    0.50,
                    0.25,
                    0.25,
                    base,
                    &sampled,
                    &mask));
    assert_true("material_face_stack_preview_color_changed",
                material_preview_color_differs(base, sampled));
    assert_true("material_face_stack_preview_mask_positive",
                mask > 1e-6);

    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialFacePlacementResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
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
    assert_true("router_clamp_material_normal",
                EditorModeRouter_ClampEditorMode(EDITOR_MODE_MATERIAL, false) ==
                    EDITOR_MODE_MATERIAL);
    assert_true("router_clamp_lock_scene_to_path",
                EditorModeRouter_ClampEditorMode(1, true) == 0);
    assert_true("router_next_unlocked_forward",
                EditorModeRouter_NextEditorMode(0, false, false) == 1);
    assert_true("router_next_unlocked_reverse",
                EditorModeRouter_NextEditorMode(0, true, false) == EDITOR_MODE_MATERIAL);
    assert_true("router_next_unlocked_camera_to_material",
                EditorModeRouter_NextEditorMode(EDITOR_MODE_CAMERA, false, false) ==
                    EDITOR_MODE_MATERIAL);
    assert_true("router_next_unlocked_material_to_path",
                EditorModeRouter_NextEditorMode(EDITOR_MODE_MATERIAL, false, false) ==
                    EDITOR_MODE_PATH);
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

static int test_scene_editor_viewport_render_falls_back_without_digest(void) {
    assert_true("viewport_digest_route_uses_digest_when_available",
                SceneEditorViewportRenderShouldUseDigestLaneForState(true, true));
    assert_true("viewport_digest_route_falls_back_without_digest",
                !SceneEditorViewportRenderShouldUseDigestLaneForState(true, false));
    assert_true("viewport_2d_route_uses_planar_even_with_digest",
                !SceneEditorViewportRenderShouldUseDigestLaneForState(false, true));
    return 0;
}

static int test_scene_editor_control_surface_material_mode_contract(void) {
    SceneEditorControlSurfaceInput input;
    SceneEditorControlSurfaceContract contract;

    memset(&input, 0, sizeof(input));
    memset(&contract, 0, sizeof(contract));
    input.requestedMode = EDITOR_MODE_MATERIAL;
    input.lockObjectMode = false;
    input.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    input.sourceLabel = "Runtime Scene";
    input.sourcePath = "/tmp/runtime_scene_material_lane.json";
    input.objectCount = 3;
    input.hasSelectedObject = true;
    input.selectedObjectIndex = 1;
    input.route.routeFamily = RAY_TRACING_ROUTE_NATIVE_3D;

    SceneEditorControlSurfaceBuild(&input, &contract);

    assert_true("surface_material_mode_active",
                contract.activeMode == EDITOR_MODE_MATERIAL);
    assert_true("surface_material_mode_selectable",
                contract.modeSelectable[EDITOR_MODE_MATERIAL]);
    assert_true("surface_material_mode_canvas_enabled_with_selection",
                contract.laneCanvasEditEnabled);
    assert_true("surface_material_mode_controls_hint",
                strstr(contract.statusControls, "Material controls") != NULL);
    assert_true("surface_material_mode_label",
                strcmp(SceneEditorControlSurfaceModeLabel(EDITOR_MODE_MATERIAL), "Material") == 0);

    input.hasSelectedObject = false;
    input.selectedObjectIndex = -1;
    memset(&contract, 0, sizeof(contract));
    SceneEditorControlSurfaceBuild(&input, &contract);
    assert_true("surface_material_mode_canvas_disabled_without_selection",
                !contract.laneCanvasEditEnabled);
    return 0;
}


int run_test_runtime_scene_editor_tests(void) {
    int before = test_support_failures();

    test_scene_editor_tool_state_contract();
    test_scene_editor_runtime_scene_persistence_roundtrip();
    test_scene_editor_runtime_scene_persistence_roundtrip_object_materials();
    test_scene_editor_runtime_scene_material_stack_roundtrip_payload();
    test_object_editor_material_assignment_preserves_object_color();
    test_object_editor_slider_assignments_update_object_fields();
    test_material_editor_focuses_last_selected_and_updates_texture_fields();
    test_material_editor_object_scope_clears_face_overrides_and_applies_all_faces();
    test_material_editor_layer_list_routes_object_stack_controls();
    test_material_editor_object_projector_centers_focused_object();
    test_material_editor_focused_zoom_accumulates_around_object_fit();
    test_material_editor_preview_resolves_focused_triangle_substrate();
    test_material_editor_face_metrics_ground_uv_scales_with_dimensions();
    test_material_editor_face_metrics_orients_vertical_faces_to_world_up();
    test_material_editor_face_preview_display_size_respects_face_aspect();
    test_material_editor_preview_picks_and_selects_visible_triangle();
    test_material_editor_preview_texture_color_responds_to_controls();
    test_material_editor_preview_uses_v2_base_and_overlay_stack();
    test_material_preview_surface_eval_active_face_detail_preview_contract();
    test_material_editor_face_group_texture_island_and_override_controls();
    test_material_editor_face_override_inherits_active_stack_layer();
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
    test_scene_editor_viewport_render_falls_back_without_digest();
    test_scene_editor_control_surface_material_mode_contract();
    test_material_editor_authored_texture_binding_routes_to_runtime_binding();
    test_material_editor_authored_texture_binding_persists_and_reopens();
    test_material_editor_authored_texture_binding_replace_clear_roundtrip();
    test_material_editor_authored_texture_invalid_binding_surfaces_reason_and_clears();
    test_material_editor_authored_texture_invalid_binding_persists_and_reopens();
    test_scene_editor_runtime_scene_authored_texture_overlay_roundtrip_payload();
    return test_support_failures() - before;
}
