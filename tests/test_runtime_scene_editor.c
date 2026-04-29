#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app/animation.h"
#include "editor/object_editor.h"
#include "editor/object_editor_object_ops.h"
#include "editor/editor_mode_router.h"
#include "editor/scene_editor_control_surface.h"
#include "editor/scene_editor_runtime_scene_persistence.h"
#include "editor/scene_editor_tool_state.h"
#include "import/runtime_scene_bridge.h"
#include "test_runtime_scene_editor.h"
#include "test_support.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
    sceneSettings.sceneObjects[0].color = 0x00FF00;
    sceneSettings.sceneObjects[0].alpha = 0.35;
    sceneSettings.sceneObjects[0].emissiveStrength = 0.65;

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

    free(persisted_json);
    unlink(runtime_path);
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


int run_test_runtime_scene_editor_tests(void) {
    test_scene_editor_tool_state_contract();
    test_scene_editor_runtime_scene_persistence_roundtrip();
    test_scene_editor_runtime_scene_persistence_roundtrip_object_materials();
    test_object_editor_material_assignment_preserves_object_color();
    test_object_editor_slider_assignments_update_object_fields();
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
    return 0;
}
