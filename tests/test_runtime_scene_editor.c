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
#include "editor/material_editor_internal.h"
#include "editor/material_preview_surface_eval.h"
#include "editor/object_editor.h"
#include "editor/object_editor_object_ops.h"
#include "editor/object_editor_selection_tracker.h"
#include "editor/editor_mode_router.h"
#include "editor/scene_editor_control_surface.h"
#include "editor/scene_editor_digest_overlay.h"
#include "editor/scene_editor_material_face_metrics.h"
#include "editor/scene_editor_material_face_placement.h"
#include "editor/scene_editor_material_graph.h"
#include "editor/scene_editor_material_preview.h"
#include "editor/scene_editor_material_stack.h"
#include "editor/scene_editor_runtime_scene_persistence.h"
#include "editor/scene_editor_scene_view_packet.h"
#include "editor/scene_editor_tool_state.h"
#include "editor/scene_editor_viewport_nav.h"
#include "editor/scene_editor_viewport_render.h"
#include "import/runtime_mesh_asset_loader.h"
#include "import/runtime_scene_bridge.h"
#include "material/material_manager.h"
#include "render/runtime_material_authored_texture_3d.h"
#include "render/runtime_material_graph_3d.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_material_texture_3d.h"
#include "render/runtime_scene_3d_builder.h"
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

static bool test_scene_editor_write_text_file(const char* path, const char* text) {
    FILE* file = NULL;
    if (!path || !text) return false;
    file = fopen(path, "w");
    if (!file) return false;
    if (fputs(text, file) < 0) {
        fclose(file);
        return false;
    }
    return fclose(file) == 0;
}

static bool test_scene_editor_write_large_placeholder_file(const char* path, size_t bytes) {
    FILE* file = NULL;
    char chunk[4096];
    size_t remaining = bytes;
    if (!path || bytes == 0u) return false;
    memset(chunk, 'x', sizeof(chunk));
    file = fopen(path, "w");
    if (!file) return false;
    while (remaining > 0u) {
        size_t write_count = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
        if (fwrite(chunk, 1u, write_count, file) != write_count) {
            fclose(file);
            return false;
        }
        remaining -= write_count;
    }
    return fclose(file) == 0;
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

static bool test_scene_editor_write_channel_authored_texture_manifest(
    const char* manifest_path,
    const char* object_id,
    const char* file_name) {
    json_object* root = NULL;
    json_object* base_surfaces = NULL;
    json_object* base_surface = NULL;
    json_object* channels = NULL;
    const char* channel_names[] = {
        "base_color.rgb",
        "roughness.scalar",
        "normal.tangent"
    };
    const char* channel_sources[] = {
        "rgba",
        "luminance",
        "rgb"
    };
    int write_ok = 0;
    if (!manifest_path || !object_id || !file_name) return false;
    root = json_object_new_object();
    base_surfaces = json_object_new_array();
    base_surface = json_object_new_object();
    channels = json_object_new_array();
    if (!root || !base_surfaces || !base_surface || !channels) {
        if (root) json_object_put(root);
        if (base_surfaces) json_object_put(base_surfaces);
        if (base_surface) json_object_put(base_surface);
        if (channels) json_object_put(channels);
        return false;
    }
    json_object_object_add(root, "schema_version", json_object_new_int(5));
    json_object_object_add(root,
                           "export_binding_kind",
                           json_object_new_string("SEPARATE_FACES"));
    json_object_object_add(root,
                           "emitted_output_kind",
                           json_object_new_string("FLATTENED_ONLY"));
    json_object_object_add(root, "primitive_kind", json_object_new_string("PLANE"));
    json_object_object_add(root, "source_scene_id", json_object_new_string("scene_channel_test"));
    json_object_object_add(root, "source_object_id", json_object_new_string(object_id));
    json_object_object_add(root, "base_surface_count", json_object_new_int(1));
    json_object_object_add(base_surface, "surface_id", json_object_new_int(1));
    json_object_object_add(base_surface, "face_role", json_object_new_string("FRONT"));
    json_object_object_add(base_surface, "file_name", json_object_new_string(file_name));
    if (!test_scene_editor_add_surface_semantic_fields(base_surface, "PLANE", "FRONT")) {
        json_object_put(root);
        return false;
    }
    for (int i = 0; i < 3; ++i) {
        json_object* channel = json_object_new_object();
        if (!channel) {
            json_object_put(root);
            return false;
        }
        json_object_object_add(channel, "channel", json_object_new_string(channel_names[i]));
        json_object_object_add(channel, "source", json_object_new_string(channel_sources[i]));
        json_object_object_add(channel, "file_name", json_object_new_string(file_name));
        json_object_array_add(channels, channel);
    }
    json_object_object_add(base_surface, "material_channels", channels);
    json_object_array_add(base_surfaces, base_surface);
    json_object_object_add(root, "base_surfaces", base_surfaces);
    write_ok = json_object_to_file_ext(manifest_path, root, JSON_C_TO_STRING_PRETTY);
    json_object_put(root);
    return write_ok == 0;
}

static bool test_scene_editor_write_glass_channel_authored_texture_manifest(
    const char* manifest_path,
    const char* object_id,
    const char* file_name) {
    json_object* root = NULL;
    json_object* base_surfaces = NULL;
    json_object* base_surface = NULL;
    json_object* channels = NULL;
    const char* channel_names[] = {
        "base_color.rgb",
        "roughness.scalar",
        "opacity.coverage",
        "transmission.weight",
        "bump.height"
    };
    const char* channel_sources[] = {
        "rgba",
        "luminance",
        "alpha",
        "red",
        "green"
    };
    int write_ok = 0;
    if (!manifest_path || !object_id || !file_name) return false;
    root = json_object_new_object();
    base_surfaces = json_object_new_array();
    base_surface = json_object_new_object();
    channels = json_object_new_array();
    if (!root || !base_surfaces || !base_surface || !channels) {
        if (root) json_object_put(root);
        if (base_surfaces) json_object_put(base_surfaces);
        if (base_surface) json_object_put(base_surface);
        if (channels) json_object_put(channels);
        return false;
    }
    json_object_object_add(root, "schema_version", json_object_new_int(5));
    json_object_object_add(root,
                           "export_binding_kind",
                           json_object_new_string("SEPARATE_FACES"));
    json_object_object_add(root,
                           "emitted_output_kind",
                           json_object_new_string("FLATTENED_ONLY"));
    json_object_object_add(root, "primitive_kind", json_object_new_string("PLANE"));
    json_object_object_add(root, "source_scene_id", json_object_new_string("scene_glass_channel_test"));
    json_object_object_add(root, "source_object_id", json_object_new_string(object_id));
    json_object_object_add(root, "base_surface_count", json_object_new_int(1));
    json_object_object_add(base_surface, "surface_id", json_object_new_int(1));
    json_object_object_add(base_surface, "face_role", json_object_new_string("FRONT"));
    json_object_object_add(base_surface, "file_name", json_object_new_string(file_name));
    if (!test_scene_editor_add_surface_semantic_fields(base_surface, "PLANE", "FRONT")) {
        json_object_put(root);
        return false;
    }
    for (int i = 0; i < 5; ++i) {
        json_object* channel = json_object_new_object();
        if (!channel) {
            json_object_put(root);
            return false;
        }
        json_object_object_add(channel, "channel", json_object_new_string(channel_names[i]));
        json_object_object_add(channel, "source", json_object_new_string(channel_sources[i]));
        json_object_object_add(channel, "file_name", json_object_new_string(file_name));
        json_object_array_add(channels, channel);
    }
    json_object_object_add(base_surface, "material_channels", channels);
    json_object_array_add(base_surfaces, base_surface);
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

static int test_scene_editor_runtime_scene_persist_keeps_preview_limited_mesh_reload(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char* dir = "/tmp/ray_tracing_runtime_scene_persist_preview_limited";
    const char* scene_path = "/tmp/ray_tracing_runtime_scene_persist_preview_limited/scene_runtime.json";
    const char* huge_path = "/tmp/ray_tracing_runtime_scene_persist_preview_limited/huge_skipped.runtime.json";
    const char* small_path =
        "tests/fixtures/mesh_asset_runtime_spheres/assets/mesh_assets/asset_sphere_8x4.runtime.json";
    char small_abs[1024];
    char scene_json[4096];
    char diagnostics[256];
    RuntimeSceneBridgePreflight summary = {0};
    const RayTracingRuntimeMeshAssetSet* mesh_assets = NULL;
    bool ok = false;

    assert_true("runtime_scene_persist_preview_limited_mkdir",
                mkdir(dir, 0777) == 0 || access(dir, F_OK) == 0);
    assert_true("runtime_scene_persist_preview_limited_small_realpath",
                realpath(small_path, small_abs) != NULL);
    assert_true("runtime_scene_persist_preview_limited_huge_write",
                test_scene_editor_write_large_placeholder_file(huge_path, 1024u * 1024u + 16u));
    if (!small_abs[0]) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    snprintf(scene_json,
             sizeof(scene_json),
             "{"
             "\"schema_family\":\"codework_scene\","
             "\"schema_variant\":\"scene_runtime_v1\","
             "\"schema_version\":1,"
             "\"scene_id\":\"persist_preview_limited_mesh\","
             "\"space_mode_default\":\"3d\","
             "\"unit_system\":\"meters\","
             "\"world_scale\":1.0,"
             "\"objects\":["
             "{"
             "\"object_id\":\"obj_huge\","
             "\"object_type\":\"mesh_asset_instance\","
             "\"transform\":{\"position\":{\"x\":0,\"y\":0,\"z\":0},"
             "\"rotation\":{\"x\":0,\"y\":0,\"z\":0},"
             "\"scale\":{\"x\":1,\"y\":1,\"z\":1}},"
             "\"geometry_ref\":{\"kind\":\"mesh_asset\",\"id\":\"asset_huge\"},"
             "\"extensions\":{\"line_drawing\":{\"runtime_mesh_path\":\"%s\"}}"
             "},"
             "{"
             "\"object_id\":\"obj_plane\","
             "\"object_type\":\"plane_primitive\","
             "\"transform\":{\"position\":{\"x\":0,\"y\":0,\"z\":0},"
             "\"rotation\":{\"x\":0,\"y\":0,\"z\":0},"
             "\"scale\":{\"x\":1,\"y\":1,\"z\":1}},"
             "\"primitive\":{\"kind\":\"plane_primitive\",\"width\":4,\"height\":4}"
             "},"
             "{"
             "\"object_id\":\"obj_platform\","
             "\"object_type\":\"mesh_asset_instance\","
             "\"transform\":{\"position\":{\"x\":0,\"y\":0,\"z\":1.25},"
             "\"rotation\":{\"x\":0,\"y\":0,\"z\":0},"
             "\"scale\":{\"x\":2,\"y\":2,\"z\":2}},"
             "\"geometry_ref\":{\"kind\":\"mesh_asset\",\"id\":\"asset_sphere_8x4\"},"
             "\"extensions\":{\"line_drawing\":{\"runtime_mesh_path\":\"%s\"}}"
             "}],"
             "\"materials\":[],"
             "\"lights\":[{\"position\":{\"x\":0,\"y\":0,\"z\":4}}],"
             "\"cameras\":[{\"position\":{\"x\":0,\"y\":-6,\"z\":4}}],"
             "\"constraints\":[],"
             "\"extensions\":{}"
             "}",
             huge_path,
             small_abs);

    assert_true("runtime_scene_persist_preview_limited_scene_write",
                test_scene_editor_write_text_file(scene_path, scene_json));

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    ok = runtime_scene_bridge_apply_file_defer_mesh_assets(scene_path, &summary);
    assert_true("runtime_scene_persist_preview_limited_initial_apply", ok);
    mesh_assets = ray_tracing_runtime_mesh_assets_last();
    assert_true("runtime_scene_persist_preview_limited_initial_mesh_state",
                mesh_assets &&
                mesh_assets->instance_count == 1 &&
                mesh_assets->skipped_instance_count == 1);

    sceneSettings.bezierPath.numPoints = 1;
    sceneSettings.bezierPath.points[0].x = 1.0;
    sceneSettings.bezierPath.points[0].y = 2.0;
    ok = SceneEditorRuntimeScenePersistAuthoring(diagnostics, sizeof(diagnostics));
    assert_true("runtime_scene_persist_preview_limited_persist_ok", ok);
    mesh_assets = ray_tracing_runtime_mesh_assets_last();
    assert_true("runtime_scene_persist_preview_limited_after_persist_mesh_state",
                mesh_assets &&
                mesh_assets->instance_count == 1 &&
                mesh_assets->skipped_instance_count == 1 &&
                mesh_assets->skipped_instances[0].scene_object_index == 0);

    unlink(scene_path);
    unlink(huge_path);
    rmdir(dir);
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
    face_placement.layerIndex = 2;
    snprintf(face_placement.layerId, sizeof(face_placement.layerId), "%s", "rust_detail");
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
        assert_true("runtime_scene_authoring_material_persist_has_face_layer_index",
                    strstr(persisted_json, "\"layer_index\":2") != NULL);
        assert_true("runtime_scene_authoring_material_persist_has_face_layer_id",
                    strstr(persisted_json, "\"layer_id\":\"rust_detail\"") != NULL);
        assert_true("runtime_scene_authoring_material_persist_has_face_texture_id",
                    strstr(persisted_json, "\"texture_id\":2") != NULL);
        assert_true("runtime_scene_authoring_material_persist_has_authored_texture",
                    strstr(persisted_json, "\"authored_texture\"") != NULL);
        assert_true("runtime_scene_authoring_material_persist_has_manifest_path",
                    strstr(persisted_json, texture_manifest_rel) != NULL);
        assert_true("runtime_scene_authoring_material_persist_has_scene_relative_scope",
                    strstr(persisted_json, "\"path_scope\":\"scene_relative\"") != NULL);
        assert_true("runtime_scene_authoring_material_persist_no_local_manifest_path",
                    strstr(persisted_json, "\"local_manifest_path\"") == NULL);
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
    assert_true("runtime_scene_authoring_material_persist_hydrated_face_layer_override",
                SceneEditorMaterialFacePlacementHasOverrideForLayer(0, 4, "rust_detail"));
    face_placement = SceneEditorMaterialFacePlacementGetEffectiveForLayer(&sceneSettings.sceneObjects[0],
                                                                          0,
                                                                          4,
                                                                          "rust_detail");
    assert_true("runtime_scene_authoring_material_persist_hydrated_face_layer_id",
                strcmp(face_placement.layerId, "rust_detail") == 0);
    assert_true("runtime_scene_authoring_material_persist_hydrated_face_layer_index",
                face_placement.layerIndex == 2);
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
        assert_true("material_editor_authored_replace_clear_persisted_scene_relative_scope",
                    strstr(persisted_json, "\"path_scope\":\"scene_relative\"") != NULL);
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
        assert_true("material_editor_authored_invalid_reopen_persisted_scene_relative_scope",
                    strstr(persisted_json, "\"path_scope\":\"scene_relative\"") != NULL);
        assert_true("material_editor_authored_invalid_reopen_persisted_reason",
                    strstr(persisted_json,
                           "\"invalid_reason\":\"schema or output contract invalid\"") != NULL);
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

static int test_material_editor_authored_texture_local_absolute_reference_policy(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char* runtime_dir = "/tmp/ray_tracing_authored_texture_local_policy_scene";
    const char* runtime_path = "/tmp/ray_tracing_authored_texture_local_policy_scene/scene_runtime.json";
    const char* external_dir = "/tmp/ray_tracing_authored_texture_local_policy_external";
    const char* texture_png = "/tmp/ray_tracing_authored_texture_local_policy_external/local_plane.png";
    const char* texture_manifest =
        "/tmp/ray_tracing_authored_texture_local_policy_external/local_plane_manifest.json";
    const char* runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"authored_texture_local_policy\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[{"
          "\"object_id\":\"local_plane\","
          "\"object_type\":\"plane_primitive\","
          "\"transform\":{"
            "\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
            "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}"
          "},"
          "\"primitive\":{\"kind\":\"plane_primitive\",\"width\":1.0,\"height\":1.0},"
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
    unsigned char texture_rgba[] = {180u, 90u, 40u, 255u};
    char diagnostics[256];
    char* persisted_json = NULL;
    char authored_manifest_path[RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY];
    char authored_binding_mode[RUNTIME_MATERIAL_AUTHORED_TEXTURE_MODE_CAPACITY];
    char authored_reason[RUNTIME_MATERIAL_AUTHORED_TEXTURE_REASON_CAPACITY];
    bool ok = false;

    assert_true("material_editor_authored_local_policy_mkdir_runtime",
                mkdir(runtime_dir, 0775) == 0 || access(runtime_dir, F_OK) == 0);
    assert_true("material_editor_authored_local_policy_mkdir_external",
                mkdir(external_dir, 0775) == 0 || access(external_dir, F_OK) == 0);
    assert_true("material_editor_authored_local_policy_scene_write",
                test_scene_editor_write_text_file(runtime_path, runtime_json));
    assert_true("material_editor_authored_local_policy_png_write",
                test_scene_editor_write_png_rgba(texture_png, texture_rgba, 1u, 1u));
    assert_true("material_editor_authored_local_policy_manifest_write",
                test_scene_editor_write_invalid_authored_texture_manifest_missing_output_kind(
                    texture_manifest,
                    "local_plane",
                    "PLANE",
                    "FRONT",
                    "local_plane.png"));

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialFacePlacementResetAll();
    SceneEditorMaterialStackResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    ok = runtime_scene_bridge_apply_file(runtime_path, &summary);
    assert_true("material_editor_authored_local_policy_apply_ok", ok);
    if (!ok) {
        unlink(texture_png);
        unlink(texture_manifest);
        rmdir(external_dir);
        unlink(runtime_path);
        rmdir(runtime_dir);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath), "%s", runtime_path);
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();
    assert_true("material_editor_authored_local_policy_bind_rejected",
                !MaterialEditorBindAuthoredTextureManifestForFocused(texture_manifest));
    assert_true("material_editor_authored_local_policy_persist_ok",
                SceneEditorRuntimeScenePersistAuthoring(diagnostics, sizeof(diagnostics)));

    persisted_json = read_text_file_alloc(runtime_path, NULL);
    assert_true("material_editor_authored_local_policy_readback_ok", persisted_json != NULL);
    if (persisted_json) {
        assert_true("material_editor_authored_local_policy_persisted_local_scope",
                    strstr(persisted_json, "\"path_scope\":\"local_absolute\"") != NULL);
        assert_true("material_editor_authored_local_policy_persisted_local_path",
                    strstr(persisted_json, "\"local_manifest_path\":\"/tmp/ray_tracing_authored_texture_local_policy_external/local_plane_manifest.json\"") != NULL);
        assert_true("material_editor_authored_local_policy_no_portable_absolute_manifest",
                    strstr(persisted_json, "\"manifest_path\":\"/tmp/") == NULL);
        assert_true("material_editor_authored_local_policy_persisted_reason",
                    strstr(persisted_json,
                           "\"invalid_reason\":\"schema or output contract invalid\"") != NULL);
        free(persisted_json);
        persisted_json = NULL;
    }

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialFacePlacementResetAll();
    SceneEditorMaterialStackResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    assert_true("material_editor_authored_local_policy_reapply_ok",
                runtime_scene_bridge_apply_file(runtime_path, &reapply_summary));
    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath), "%s", runtime_path);
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();
    assert_true("material_editor_authored_local_policy_invalid_summary",
                MaterialEditorGetAuthoredTextureInvalidSummary(authored_manifest_path,
                                                              sizeof(authored_manifest_path),
                                                              authored_binding_mode,
                                                              sizeof(authored_binding_mode),
                                                              authored_reason,
                                                              sizeof(authored_reason)));
    assert_true("material_editor_authored_local_policy_invalid_summary_path",
                strcmp(authored_manifest_path, texture_manifest) == 0);
    assert_true("material_editor_authored_local_policy_invalid_summary_mode",
                strcmp(authored_binding_mode, "override") == 0);
    assert_true("material_editor_authored_local_policy_invalid_summary_reason",
                strcmp(authored_reason, "schema or output contract invalid") == 0);

    unlink(texture_png);
    unlink(texture_manifest);
    rmdir(external_dir);
    unlink(runtime_path);
    rmdir(runtime_dir);
    RuntimeMaterialAuthoredTextureResetAll();
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
    sceneSettings.sceneObjects[0].alpha = 0.42;
    sceneSettings.sceneObjects[0].reflectivity = 0.35;
    sceneSettings.sceneObjects[0].roughness = 0.65;

    ObjectEditorObjectAssignMaterial(&sceneSettings.sceneObjects[0], MATERIAL_PRESET_TRANSPARENT);

    assert_true("object_editor_assign_material_updates_material_id",
                sceneSettings.sceneObjects[0].material_id == MATERIAL_PRESET_TRANSPARENT);
    assert_true("object_editor_assign_material_preserves_color",
                sceneSettings.sceneObjects[0].color == 0x00FF00);
    assert_close("object_editor_assign_material_preserves_alpha",
                 sceneSettings.sceneObjects[0].alpha,
                 0.42,
                 1e-9);
    assert_close("object_editor_assign_material_uses_preset_reflectivity",
                 sceneSettings.sceneObjects[0].reflectivity,
                 0.0,
                 1e-9);
    assert_close("object_editor_assign_material_uses_preset_roughness",
                 sceneSettings.sceneObjects[0].roughness,
                 0.04,
                 1e-9);

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
    MaterialEditorProofReadback proof_readback;
    MaterialEditorCompactLayoutRects compact_rects;
    SDL_Rect compact_bounds = {10, 20, 300, 360};
    char proof_status[MATERIAL_EDITOR_PROOF_TEXT_CAPACITY];

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
    assert_true("material_editor_subpane_default_stack",
                MaterialEditorGetActiveSubPane() == MATERIAL_EDITOR_SUBPANE_STACK &&
                    strcmp(MaterialEditorSubPaneLabel(MATERIAL_EDITOR_SUBPANE_STACK),
                           "Layer Stack") == 0 &&
                    strcmp(MaterialEditorSubPaneCompactLabel(MATERIAL_EDITOR_SUBPANE_STACK),
                           "Stack") == 0);
    MaterialEditorSetActiveSubPane(MATERIAL_EDITOR_SUBPANE_TEXTURES);
    assert_true("material_editor_subpane_switches_textures",
                MaterialEditorGetActiveSubPane() == MATERIAL_EDITOR_SUBPANE_TEXTURES &&
                    strcmp(MaterialEditorSubPaneLabel(MATERIAL_EDITOR_SUBPANE_TEXTURES),
                           "Textures & Channels") == 0 &&
                    strcmp(MaterialEditorSubPaneCompactLabel(MATERIAL_EDITOR_SUBPANE_TEXTURES),
                           "Tex") == 0);
    MaterialEditorSetActiveSubPane((MaterialEditorSubPane)999);
    assert_true("material_editor_subpane_clamps_invalid",
                MaterialEditorGetActiveSubPane() == MATERIAL_EDITOR_SUBPANE_STACK);
    assert_true("material_editor_identity_popover_starts_closed",
                !MaterialEditorIdentityPopoverOpen());
    assert_true("material_editor_identity_popover_toggles_open",
                MaterialEditorToggleIdentityPopover());
    MaterialEditorSetIdentityPopoverOpen(false);
    assert_true("material_editor_identity_popover_set_closed",
                !MaterialEditorIdentityPopoverOpen());
    compact_rects = MaterialEditorCompactLayoutBuild(compact_bounds, true);
    assert_true("material_editor_compact_layout_header",
                compact_rects.identity_header.x == 16 &&
                    compact_rects.identity_header.y == 24 &&
                    compact_rects.identity_header.h == MATERIAL_EDITOR_COMPACT_HEADER_HEIGHT &&
                    compact_rects.identity_header.w == 288);
    assert_true("material_editor_compact_layout_tabs",
                compact_rects.tab_rects[MATERIAL_EDITOR_SUBPANE_STACK].h ==
                        MATERIAL_EDITOR_COMPACT_TAB_HEIGHT &&
                    compact_rects.tab_rects[MATERIAL_EDITOR_SUBPANE_TEXTURES].x >
                        compact_rects.tab_rects[MATERIAL_EDITOR_SUBPANE_STACK].x &&
                    compact_rects.tab_rects[MATERIAL_EDITOR_SUBPANE_PROOF].w > 0);
    assert_true("material_editor_compact_layout_popover_overlays_content",
                compact_rects.identity_popover_visible &&
                    compact_rects.identity_popover.y <
                        compact_rects.content.y &&
                    compact_rects.content.y >
                        compact_rects.identity_header.y + compact_rects.identity_header.h);
    s_material_editor_compact_layout_rects =
        MaterialEditorCompactLayoutBuild(compact_bounds, false);
    {
        SDL_Event click;
        memset(&click, 0, sizeof(click));
        click.type = SDL_MOUSEBUTTONDOWN;
        click.button.button = SDL_BUTTON_LEFT;
        click.button.x =
            s_material_editor_compact_layout_rects.tab_rects[MATERIAL_EDITOR_SUBPANE_GRAPH].x + 2;
        click.button.y =
            s_material_editor_compact_layout_rects.tab_rects[MATERIAL_EDITOR_SUBPANE_GRAPH].y + 2;
        HandleMaterialEditorEvents(&click);
    }
    assert_true("material_editor_compact_tab_input_switches_graph",
                MaterialEditorGetActiveSubPane() == MATERIAL_EDITOR_SUBPANE_GRAPH);
    {
        SDL_Event click;
        memset(&click, 0, sizeof(click));
        click.type = SDL_MOUSEBUTTONDOWN;
        click.button.button = SDL_BUTTON_LEFT;
        click.button.x = compact_rects.identity_disclosure.x + 2;
        click.button.y = compact_rects.identity_disclosure.y + 2;
        HandleMaterialEditorEvents(&click);
    }
    assert_true("material_editor_compact_identity_input_toggles",
                MaterialEditorIdentityPopoverOpen());
    MaterialEditorSetIdentityPopoverOpen(false);
    MaterialEditorSetActiveSubPane(MATERIAL_EDITOR_SUBPANE_STACK);
    assert_true("material_editor_destination_stack_label",
                MaterialEditorMutationDestinationForFocusedTextureControls() ==
                    MATERIAL_EDITOR_MUTATION_DESTINATION_MATERIAL_STACK &&
                    strcmp(MaterialEditorMutationDestinationLabel(
                               MATERIAL_EDITOR_MUTATION_DESTINATION_MATERIAL_STACK),
                           "material_stack") == 0);
    assert_true("material_editor_stack_panel_group_label",
                MaterialEditorPanelGroupForMutationDestination(
                    MATERIAL_EDITOR_MUTATION_DESTINATION_MATERIAL_STACK) ==
                    MATERIAL_EDITOR_PANEL_GROUP_BASE_LAYER &&
                    strcmp(MaterialEditorPanelGroupLabel(MATERIAL_EDITOR_PANEL_GROUP_BASE_LAYER),
                           "Base Layer") == 0);
    assert_true("material_editor_texture_binding_panel_group_label",
                MaterialEditorPanelGroupForMutationDestination(
                    MATERIAL_EDITOR_MUTATION_DESTINATION_AUTHORED_TEXTURE_BINDING) ==
                    MATERIAL_EDITOR_PANEL_GROUP_TEXTURE_BINDING &&
                    strcmp(MaterialEditorPanelGroupLabel(
                               MATERIAL_EDITOR_PANEL_GROUP_TEXTURE_BINDING),
                           "Texture Binding") == 0);
    assert_true("material_editor_physical_response_panel_group_label",
                strcmp(MaterialEditorPanelGroupLabel(
                           MATERIAL_EDITOR_PANEL_GROUP_PHYSICAL_RESPONSE),
                       "Physical Response") == 0);
    assert_true("material_editor_preview_panel_group_label",
                strcmp(MaterialEditorPanelGroupLabel(
                           MATERIAL_EDITOR_PANEL_GROUP_PREVIEW_READBACK),
                       "Preview & Readback") == 0);
    assert_true("material_editor_proof_readback_builds",
                MaterialEditorBuildFocusedProofReadback(&proof_readback));
    assert_true("material_editor_proof_readback_schema",
                strcmp(proof_readback.request_schema,
                       "ray_tracing_material_proof_package_request_v1") == 0 &&
                    strcmp(proof_readback.summary_schema,
                           "ray_tracing_material_proof_summary_v1") == 0);
    assert_true("material_editor_proof_readback_route",
                strcmp(proof_readback.route_primary, "headless_material_preview") == 0 &&
                    strcmp(proof_readback.route_status,
                           "request_shape_only_not_launched") == 0 &&
                    proof_readback.m4_request_compatible &&
                    proof_readback.launch_deferred);
    assert_true("material_editor_proof_readback_artifacts",
                strcmp(proof_readback.request_path, "request.json") == 0 &&
                    strcmp(proof_readback.summary_path, "summary.json") == 0 &&
                    strcmp(proof_readback.index_path, "index.md") == 0 &&
                    strcmp(proof_readback.image_path, "preview.bmp") == 0 &&
                    strcmp(proof_readback.image_status, "not_generated_by_editor") == 0);
    assert_true("material_editor_proof_readback_destination_group",
                strcmp(proof_readback.destination_label, "material_stack") == 0 &&
                    strcmp(proof_readback.panel_group_label, "Base Layer") == 0);
    MaterialEditorFormatProofReadbackStatus(&proof_readback,
                                            proof_status,
                                            sizeof(proof_status));
    assert_true("material_editor_proof_readback_status_text",
                strstr(proof_status, "headless_material_preview") != NULL &&
                    strstr(proof_status, "material_stack") != NULL &&
                    strstr(proof_status, "request_shape_only_not_launched") != NULL);
    assert_true("material_editor_proof_readback_prime",
                MaterialEditorPrimeProofReadbackForFocused() &&
                    s_material_editor_proof_readback_valid &&
                    strstr(s_material_editor_proof_readback_status,
                           "headless_material_preview") != NULL);
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

static int test_material_editor_graph_readback_reports_mvp_integration(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeMaterialGraphDocument graph =
        RuntimeMaterialGraphDocumentMake("m7_s4_editor_graph");
    RuntimeMaterialTextureLayer base =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD);
    RuntimeMaterialTextureLayer oil =
        RuntimeMaterialTextureLayerMakeOverlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL);
    RuntimeMaterialGraphCompileResult compile_result = {0};
    MaterialEditorGraphReadback readback = {0};

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialGraphResetAll();

    sceneSettings.objectCount = 2;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, -3.0, 0.0, 5.0, 0.0, NULL, 0);
    InitObject(&sceneSettings.sceneObjects[1], OBJECT_CIRCLE, 3.0, 0.0, 5.0, 0.0, NULL, 0);
    ObjectEditorSelectionTrackerSetCurrent(1, sceneSettings.objectCount);
    InitializeMaterialEditor();

    snprintf(base.layerId, sizeof(base.layerId), "%s", "graph_base_wood");
    snprintf(oil.layerId, sizeof(oil.layerId), "%s", "graph_oil_overlay");
    oil.params.coverage = 0.62;
    oil.placement.strength = 1.0;
    assert_true("material_editor_graph_readback_add_base",
                RuntimeMaterialGraphDocumentAddNode(
                    &graph,
                    RuntimeMaterialGraphNodeMakeLayer("base_node", base)));
    assert_true("material_editor_graph_readback_add_overlay",
                RuntimeMaterialGraphDocumentAddNode(
                    &graph,
                    RuntimeMaterialGraphNodeMakeLayer("oil_node", oil)));
    assert_true("material_editor_graph_readback_add_channel",
                RuntimeMaterialGraphDocumentAddNode(
                    &graph,
                    RuntimeMaterialGraphNodeMakeChannelOutput("roughness_output",
                                                              "roughness.scalar",
                                                              "luminance",
                                                              "roughness.png")));
    assert_true("material_editor_graph_readback_set_graph",
                SceneEditorMaterialGraphSetObjectGraph(1, &graph, &compile_result));
    assert_true("material_editor_graph_readback_builds",
                MaterialEditorBuildFocusedGraphReadback(&readback));
    assert_true("material_editor_graph_readback_identity",
                readback.has_graph &&
                    strcmp(readback.phase, "M8-S6") == 0 &&
                    strcmp(readback.graph_id, "m7_s4_editor_graph") == 0 &&
                    readback.scene_object_index == 1);
    assert_true("material_editor_graph_readback_counts",
                readback.graph_node_count == 3 &&
                    readback.compiled_stack_layer_count == 2 &&
                    readback.channel_ref_count == 1);
    assert_true("material_editor_graph_readback_route",
                readback.has_compiled_stack_fallback &&
                    strcmp(readback.authoring_state, "graph_document") == 0 &&
                    strcmp(readback.evaluator_route, "compiled_stack_fallback") == 0);
    assert_true("material_editor_graph_readback_mvp_status",
                readback.visual_graph_mvp_available &&
                    !readback.visual_node_ui_deferred &&
                    strcmp(readback.integration_status,
                           "visual_graph_mvp_compiled_stack") == 0);

    SceneEditorMaterialGraphResetAll();
    SceneEditorMaterialStackResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_graph_actions_create_layer_channel_and_clear(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeMaterialGraphDocument graph = RuntimeMaterialGraphDocumentEmpty();
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    MaterialEditorGraphReadback readback = {0};

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialGraphResetAll();

    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 5.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].textureId = RUNTIME_MATERIAL_TEXTURE_3D_RUST;
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();
    MaterialEditorSetActiveSubPane(MATERIAL_EDITOR_SUBPANE_GRAPH);

    assert_true("material_editor_graph_actions_initial_mvp",
                MaterialEditorBuildFocusedGraphReadback(&readback) &&
                    !readback.has_graph &&
                    readback.visual_graph_mvp_available &&
                    strcmp(readback.integration_status,
                           "graph_mvp_create_layer_channel_clear") == 0);
    assert_true("material_editor_graph_actions_create",
                MaterialEditorEnsureGraphForFocused());
    assert_true("material_editor_graph_actions_created_graph",
                SceneEditorMaterialGraphGetObjectGraph(0, &graph) &&
                    graph.nodeCount >= 1 &&
                    SceneEditorMaterialStackGetObjectStack(0, &stack) &&
                    stack.layerCount >= 1);
    assert_true("material_editor_graph_actions_add_layer",
                MaterialEditorAddGraphLayerNodeForFocused());
    assert_true("material_editor_graph_actions_layer_readback",
                MaterialEditorBuildFocusedGraphReadback(&readback) &&
                    readback.has_graph &&
                    readback.graph_node_count >= 2 &&
                    readback.compiled_stack_layer_count >= 2);
    assert_true("material_editor_graph_actions_add_channel",
                MaterialEditorAddGraphChannelNodeForFocused());
    assert_true("material_editor_graph_actions_channel_readback",
                MaterialEditorBuildFocusedGraphReadback(&readback) &&
                    readback.channel_ref_count == 1 &&
                    SceneEditorMaterialGraphGetObjectGraph(0, &graph) &&
                    graph.nodeCount >= 3);
    assert_true("material_editor_graph_actions_clear",
                MaterialEditorClearGraphForFocused());
    assert_true("material_editor_graph_actions_cleared_readback",
                MaterialEditorBuildFocusedGraphReadback(&readback) &&
                    !readback.has_graph &&
                    readback.has_compiled_stack_fallback);

    SceneEditorMaterialGraphResetAll();
    SceneEditorMaterialStackResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_material_readback_reports_preset_custom_and_graph(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialGraphDocument graph =
        RuntimeMaterialGraphDocumentMake("m8_s3_material_graph");
    RuntimeMaterialTextureLayer base =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD);
    RuntimeMaterialTextureLayer oil =
        RuntimeMaterialTextureLayerMakeOverlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL);
    RuntimeMaterialGraphCompileResult compile_result = {0};
    MaterialEditorMaterialReadback material = {0};
    MaterialEditorActiveLayerReadback layer = {0};

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialGraphResetAll();
    RuntimeMaterialAuthoredTextureResetAll();

    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 5.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_GLOSSY;
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();

    assert_true("material_editor_material_readback_preset",
                MaterialEditorBuildMaterialReadback(&material) &&
                    material.preset_valid &&
                    !material.custom_stack &&
                    !material.graph_backed &&
                    strcmp(material.state_label, "Preset material") == 0 &&
                    strstr(material.preset_label, "Glossy") != NULL);
    assert_true("material_editor_material_readback_save_deferred",
                material.save_request_deferred &&
                    strcmp(material.save_request_label,
                           "save_preset_request_deferred") == 0);

    stack.layerCount = 2;
    stack.layers[0] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD);
    stack.layers[1] =
        RuntimeMaterialTextureLayerMakeOverlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST);
    snprintf(stack.layers[1].layerId, sizeof(stack.layers[1].layerId), "%s", "rust_detail");
    snprintf(stack.layers[1].displayName, sizeof(stack.layers[1].displayName), "%s", "Rust Detail");
    stack.layers[1].placement.strength = 0.37;
    stack.layers[1].opacity = 0.41;
    stack.layers[1].roughnessInfluence = 0.62;
    stack.layers[1].reflectivityInfluence = -0.20;
    stack.layers[1].specularInfluence = 0.36;
    stack.layers[1].diffuseInfluence = 0.18;
    stack.layers[1].transparencyInfluence = -0.15;
    assert_true("material_editor_material_readback_set_stack",
                SceneEditorMaterialStackSetObjectStack(0, &stack));
    assert_true("material_editor_material_readback_select_layer",
                MaterialEditorSetActiveLayerIndex(1));
    assert_true("material_editor_material_readback_custom",
                MaterialEditorBuildMaterialReadback(&material) &&
                    material.custom_stack &&
                    !material.graph_backed &&
                    material.stack_layer_count == 2 &&
                    strcmp(material.state_label, "Customized material") == 0);
    assert_true("material_editor_active_layer_readback",
                MaterialEditorBuildActiveLayerReadback(&layer) &&
                    layer.has_layer &&
                    layer.active_index == 1 &&
                    layer.layer_count == 2 &&
                    !layer.base_layer &&
                    layer.persisted_stack &&
                    strcmp(layer.role_label, "Overlay") == 0 &&
                    strcmp(layer.kind_label, "Rust") == 0 &&
                    strcmp(layer.source_label, "object stack") == 0 &&
                    strcmp(layer.edit_owner_label, "material_stack") == 0 &&
                    strstr(layer.title, "Overlay 2/2") != NULL &&
                    strstr(layer.detail, "Rust Detail") != NULL &&
                    strstr(layer.detail, "rust_detail") != NULL &&
                    strstr(layer.detail, "object stack -> material_stack") != NULL &&
                    strstr(layer.state_label, "enabled") != NULL &&
                    strstr(layer.state_label, "opacity 0.41") != NULL &&
                    strstr(layer.state_label, "strength 0.37") != NULL &&
                    strstr(layer.response_summary, "R +0.62") != NULL &&
                    strstr(layer.response_summary, "Refl -0.20") != NULL &&
                    strstr(layer.response_summary, "Spec +0.36") != NULL &&
                    strstr(layer.response_summary, "Diff +0.18") != NULL &&
                    strstr(layer.response_summary, "Trans -0.15") != NULL);
    assert_true("material_editor_layer_influence_rough_signed",
                MaterialEditorApplyLayerInfluenceValueToFocused(
                    MATERIAL_EDITOR_LAYER_INFLUENCE_ROUGHNESS,
                    -0.30));
    assert_true("material_editor_layer_influence_reflect_signed",
                MaterialEditorApplyLayerInfluenceValueToFocused(
                    MATERIAL_EDITOR_LAYER_INFLUENCE_REFLECTIVITY,
                    0.45));
    assert_true("material_editor_layer_influence_spec_step",
                MaterialEditorApplyLayerInfluenceStepToFocused(
                    MATERIAL_EDITOR_LAYER_INFLUENCE_SPECULAR,
                    -0.41));
    assert_true("material_editor_layer_influence_diffuse_signed",
                MaterialEditorApplyLayerInfluenceValueToFocused(
                    MATERIAL_EDITOR_LAYER_INFLUENCE_DIFFUSE,
                    -1.25));
    assert_true("material_editor_layer_influence_trans_signed",
                MaterialEditorApplyLayerInfluenceValueToFocused(
                    MATERIAL_EDITOR_LAYER_INFLUENCE_TRANSPARENCY,
                    1.20));
    assert_true("material_editor_layer_influence_invalid_rejected",
                !MaterialEditorApplyLayerInfluenceValueToFocused(
                    MATERIAL_EDITOR_LAYER_INFLUENCE_NONE,
                    0.25));
    assert_true("material_editor_layer_influence_stack_written",
                SceneEditorMaterialStackGetObjectStack(0, &stack));
    assert_close("material_editor_layer_influence_rough_value",
                 stack.layers[1].roughnessInfluence,
                 -0.30,
                 1e-9);
    assert_close("material_editor_layer_influence_reflect_value",
                 stack.layers[1].reflectivityInfluence,
                 0.45,
                 1e-9);
    assert_close("material_editor_layer_influence_spec_step_value",
                 stack.layers[1].specularInfluence,
                 -0.05,
                 1e-9);
    assert_close("material_editor_layer_influence_diffuse_clamped",
                 stack.layers[1].diffuseInfluence,
                 -1.0,
                 1e-9);
    assert_close("material_editor_layer_influence_trans_clamped",
                 stack.layers[1].transparencyInfluence,
                 1.0,
                 1e-9);
    assert_true("material_editor_layer_influence_dirty",
                sceneSettings.sceneObjects[0].dirty);
    assert_true("material_editor_layer_influence_readback_updated",
                MaterialEditorBuildActiveLayerReadback(&layer) &&
                    strstr(layer.response_summary, "R -0.30") != NULL &&
                    strstr(layer.response_summary, "Refl +0.45") != NULL &&
                    strstr(layer.response_summary, "Spec -0.05") != NULL &&
                    strstr(layer.response_summary, "Diff -1.00") != NULL &&
                    strstr(layer.response_summary, "Trans +1.00") != NULL);

    snprintf(base.layerId, sizeof(base.layerId), "%s", "graph_base_wood");
    snprintf(oil.layerId, sizeof(oil.layerId), "%s", "graph_oil_overlay");
    assert_true("material_editor_material_readback_graph_base",
                RuntimeMaterialGraphDocumentAddNode(
                    &graph,
                    RuntimeMaterialGraphNodeMakeLayer("base_node", base)));
    assert_true("material_editor_material_readback_graph_oil",
                RuntimeMaterialGraphDocumentAddNode(
                    &graph,
                    RuntimeMaterialGraphNodeMakeLayer("oil_node", oil)));
    assert_true("material_editor_material_readback_set_graph",
                SceneEditorMaterialGraphSetObjectGraph(0, &graph, &compile_result));
    assert_true("material_editor_material_readback_graph",
                MaterialEditorBuildMaterialReadback(&material) &&
                    material.graph_backed &&
                    material.custom_stack &&
                    strcmp(material.state_label, "Graph-backed custom") == 0 &&
                    strcmp(material.source_label, "graph_document + compiled_stack") == 0);

    RuntimeMaterialAuthoredTextureResetAll();
    SceneEditorMaterialGraphResetAll();
    SceneEditorMaterialStackResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_recipe_readback_and_cycles_family_surface_finish(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    MaterialEditorRecipeReadback recipe = {0};
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialGraphResetAll();

    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 5.0, 0.0, NULL, 0);
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();

    assert_true("material_editor_recipe_initial",
                MaterialEditorBuildRecipeReadback(&recipe) &&
                    strcmp(recipe.family_label, "Diffuse") == 0 &&
                    strcmp(recipe.surface_label, "Solid Tint") == 0 &&
                    strcmp(recipe.finish_label, "Clear") == 0 &&
                    strstr(recipe.header_label, "Material Diffuse") != NULL &&
                    strstr(recipe.header_label, "Surface Solid Tint") != NULL);
    assert_true("material_editor_recipe_family_cycle",
                MaterialEditorCycleRecipeFamilyForFocused() &&
                    sceneSettings.sceneObjects[0].material_id == MATERIAL_PRESET_TRANSPARENT &&
                    MaterialEditorBuildRecipeReadback(&recipe) &&
                    strcmp(recipe.family_label, "Glass") == 0 &&
                    strstr(recipe.header_label, "Material Glass") != NULL);
    assert_true("material_editor_recipe_surface_cycle",
                MaterialEditorCycleRecipeSurfaceForFocused() &&
                    SceneEditorMaterialStackGetObjectStack(0, &stack) &&
                    stack.layerCount >= 1 &&
                    stack.layers[0].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD &&
                    MaterialEditorBuildRecipeReadback(&recipe) &&
                    strcmp(recipe.surface_label, "Wood") == 0);
    assert_true("material_editor_recipe_finish_cycle_rust",
                MaterialEditorCycleRecipeFinishForFocused() &&
                    SceneEditorMaterialStackGetObjectStack(0, &stack) &&
                    stack.layerCount >= 2 &&
                    stack.layers[1].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST &&
                    MaterialEditorBuildRecipeReadback(&recipe) &&
                    strcmp(recipe.finish_label, "Rust") == 0);
    assert_true("material_editor_recipe_finish_cycle_grime",
                MaterialEditorCycleRecipeFinishForFocused() &&
                    SceneEditorMaterialStackGetObjectStack(0, &stack) &&
                    stack.layerCount >= 2 &&
                    stack.layers[1].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME &&
                    MaterialEditorBuildRecipeReadback(&recipe) &&
                    strcmp(recipe.finish_label, "Grime") == 0);

    SceneEditorMaterialGraphResetAll();
    SceneEditorMaterialStackResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_recipe_dropdown_options_apply_exact_choices(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    MaterialEditorRecipeOption options[MATERIAL_EDITOR_RECIPE_MENU_MAX_ITEMS];
    MaterialEditorRecipeReadback recipe = {0};
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    int option_count = 0;
    int glass_index = -1;
    int metal_index = -1;
    int glass_oil_index = -1;
    int brushed_index = -1;
    int rust_index = -1;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialGraphResetAll();

    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 5.0, 0.0, NULL, 0);
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();

    option_count = MaterialEditorBuildRecipeOptions(MATERIAL_EDITOR_RECIPE_AXIS_FAMILY,
                                                    options,
                                                    MATERIAL_EDITOR_RECIPE_MENU_MAX_ITEMS);
    for (int i = 0; i < option_count; ++i) {
        if (options[i].material_id == MATERIAL_PRESET_TRANSPARENT) glass_index = i;
        if (options[i].material_id == MATERIAL_PRESET_ROUGH_METAL) metal_index = i;
    }
    assert_true("material_editor_recipe_dropdown_has_family_choices",
                glass_index >= 0 && metal_index >= 0);
    assert_true("material_editor_recipe_dropdown_apply_glass",
                MaterialEditorApplyRecipeOptionForFocused(MATERIAL_EDITOR_RECIPE_AXIS_FAMILY,
                                                          glass_index) &&
                    MaterialEditorBuildRecipeReadback(&recipe) &&
                    strcmp(recipe.family_label, "Glass") == 0);
    option_count = MaterialEditorBuildRecipeOptions(MATERIAL_EDITOR_RECIPE_AXIS_SURFACE,
                                                    options,
                                                    MATERIAL_EDITOR_RECIPE_MENU_MAX_ITEMS);
    assert_true("material_editor_recipe_glass_surface_is_compact",
                option_count == 1 &&
                    options[0].layer_kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID);
    option_count = MaterialEditorBuildRecipeOptions(MATERIAL_EDITOR_RECIPE_AXIS_FINISH,
                                                    options,
                                                    MATERIAL_EDITOR_RECIPE_MENU_MAX_ITEMS);
    for (int i = 0; i < option_count; ++i) {
        if (options[i].layer_kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL) {
            glass_oil_index = i;
        }
    }
    assert_true("material_editor_recipe_glass_finish_has_oil", glass_oil_index >= 0);
    assert_true("material_editor_recipe_dropdown_apply_metal",
                MaterialEditorApplyRecipeOptionForFocused(MATERIAL_EDITOR_RECIPE_AXIS_FAMILY,
                                                          metal_index) &&
                    MaterialEditorBuildRecipeReadback(&recipe) &&
                    strcmp(recipe.family_label, "Metal") == 0);
    option_count = MaterialEditorBuildRecipeOptions(MATERIAL_EDITOR_RECIPE_AXIS_SURFACE,
                                                    options,
                                                    MATERIAL_EDITOR_RECIPE_MENU_MAX_ITEMS);
    for (int i = 0; i < option_count; ++i) {
        if (options[i].layer_kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRUSHED_METAL) {
            brushed_index = i;
        }
    }
    assert_true("material_editor_recipe_metal_surface_has_brushed", brushed_index >= 0);
    assert_true("material_editor_recipe_dropdown_apply_brushed",
                MaterialEditorApplyRecipeOptionForFocused(MATERIAL_EDITOR_RECIPE_AXIS_SURFACE,
                                                          brushed_index) &&
                    SceneEditorMaterialStackGetObjectStack(0, &stack) &&
                    stack.layers[0].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRUSHED_METAL);
    option_count = MaterialEditorBuildRecipeOptions(MATERIAL_EDITOR_RECIPE_AXIS_FINISH,
                                                    options,
                                                    MATERIAL_EDITOR_RECIPE_MENU_MAX_ITEMS);
    for (int i = 0; i < option_count; ++i) {
        if (options[i].layer_kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST) {
            rust_index = i;
        }
    }
    assert_true("material_editor_recipe_metal_finish_has_rust", rust_index >= 0);
    assert_true("material_editor_recipe_dropdown_apply_rust",
                MaterialEditorApplyRecipeOptionForFocused(MATERIAL_EDITOR_RECIPE_AXIS_FINISH,
                                                          rust_index) &&
                    SceneEditorMaterialStackGetObjectStack(0, &stack) &&
                    stack.layerCount >= 2 &&
                    stack.layers[1].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST &&
                    MaterialEditorBuildRecipeReadback(&recipe) &&
                    strcmp(recipe.finish_label, "Rust") == 0);

    SceneEditorMaterialGraphResetAll();
    SceneEditorMaterialStackResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static const MaterialEditorResponseRow* test_material_editor_response_row(
    const MaterialEditorResponseReadback* readback,
    const char* label) {
    if (!readback || !label) return NULL;
    for (int i = 0; i < readback->row_count; ++i) {
        if (strcmp(readback->rows[i].label, label) == 0) {
            return &readback->rows[i];
        }
    }
    return NULL;
}

static int test_material_editor_response_readback_uses_family_matrix_for_glass(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    MaterialEditorResponseReadback readback = {0};
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    const MaterialEditorResponseRow* trans = NULL;
    const MaterialEditorResponseRow* rough = NULL;
    const MaterialEditorResponseRow* ior = NULL;
    const MaterialEditorResponseRow* absorb = NULL;
    const MaterialEditorResponseRow* thin = NULL;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialGraphResetAll();

    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 5.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[0].alpha = 0.8;
    sceneSettings.sceneObjects[0].color = SceneObjectPackRGBBytes(180u, 220u, 255u);
    stack.layerCount = 1;
    stack.layers[0] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID);
    stack.layers[0].roughnessInfluence = 0.32;
    stack.layers[0].reflectivityInfluence = 0.12;
    stack.layers[0].specularInfluence = 0.44;
    assert_true("material_editor_response_glass_set_stack",
                SceneEditorMaterialStackSetObjectStack(0, &stack));
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();
    assert_true("material_editor_response_glass_select_base",
                MaterialEditorSetActiveLayerIndex(0));

    assert_true("material_editor_response_glass_build",
                MaterialEditorBuildResponseReadback(&readback));
    trans = test_material_editor_response_row(&readback, "Trans");
    rough = test_material_editor_response_row(&readback, "Rough");
    ior = test_material_editor_response_row(&readback, "IOR");
    absorb = test_material_editor_response_row(&readback, "Absorb");
    thin = test_material_editor_response_row(&readback, "Thin");
    assert_true("material_editor_response_glass_family",
                readback.family == MATERIAL_EDITOR_RESPONSE_FAMILY_GLASS &&
                    readback.family_specific &&
                    !readback.has_guarded_fields &&
                    strcmp(readback.title, "Glass Response") == 0 &&
                    strstr(readback.route_label, "glass family matrix") != NULL);
    assert_true("material_editor_response_glass_rows_present",
                trans && rough && ior && absorb && thin && readback.row_count == 8);
    assert_true("material_editor_response_glass_trans_editable",
                strcmp(trans->value, "0.75") == 0 &&
                    trans->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE &&
                    strstr(trans->note, "preset transport") != NULL);
    assert_true("material_editor_response_glass_layer_roughness",
                strcmp(rough->value, "0.32") == 0 &&
                    rough->field == MATERIAL_EDITOR_RESPONSE_FIELD_ROUGHNESS &&
                    rough->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE);
    assert_true("material_editor_response_glass_ior_absorb_thin_editable",
                strcmp(ior->value, "1.45") == 0 &&
                    strcmp(absorb->value, "2.00") == 0 &&
                    strcmp(thin->value, "on") == 0 &&
                    ior->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE &&
                    absorb->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE &&
                    thin->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE);

    SceneEditorMaterialGraphResetAll();
    SceneEditorMaterialStackResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_glass_response_mutation_routes_to_composite_helpers(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    MaterialEditorResponseReadback readback = {0};
    RuntimeMaterialPayload3D payload = {0};
    const Material* glass_preset = NULL;
    const MaterialEditorResponseRow* trans = NULL;
    const MaterialEditorResponseRow* ior = NULL;
    const MaterialEditorResponseRow* absorb = NULL;
    const MaterialEditorResponseRow* thin = NULL;
    const MaterialEditorResponseRow* reflect = NULL;
    const MaterialEditorResponseRow* spec = NULL;
    const MaterialEditorResponseRow* tint = NULL;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialGraphResetAll();

    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 5.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[0].color = SceneObjectPackRGBBytes(180u, 220u, 255u);
    stack.layerCount = 1;
    stack.layers[0] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID);
    assert_true("material_editor_glass_response_mutation_seed_stack",
                SceneEditorMaterialStackSetObjectStack(0, &stack));
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();
    assert_true("material_editor_glass_response_mutation_select_base",
                MaterialEditorSetActiveLayerIndex(0));

    assert_true("material_editor_glass_response_roughness_route",
                MaterialEditorApplyResponseValueToFocused(
                    MATERIAL_EDITOR_RESPONSE_FIELD_ROUGHNESS,
                    0.61));
    assert_true("material_editor_glass_response_reflect_route",
                MaterialEditorApplyResponseValueToFocused(
                    MATERIAL_EDITOR_RESPONSE_FIELD_REFLECTIVITY,
                    0.27));
    assert_true("material_editor_glass_response_spec_route",
                MaterialEditorApplyResponseValueToFocused(
                    MATERIAL_EDITOR_RESPONSE_FIELD_SPECULAR,
                    0.73));
    assert_true("material_editor_glass_response_tint_route",
                MaterialEditorApplyResponseTintToFocused(
                    SceneObjectPackRGBBytes(90u, 150u, 220u)));
    assert_true("material_editor_glass_response_trans_route",
                MaterialEditorApplyResponseValueToFocused(
                    MATERIAL_EDITOR_RESPONSE_FIELD_TRANSMISSION,
                    0.58));
    assert_true("material_editor_glass_response_ior_route",
                MaterialEditorApplyResponseValueToFocused(
                    MATERIAL_EDITOR_RESPONSE_FIELD_IOR,
                    1.62));
    assert_true("material_editor_glass_response_absorb_route",
                MaterialEditorApplyResponseValueToFocused(
                    MATERIAL_EDITOR_RESPONSE_FIELD_ABSORPTION,
                    3.25));
    assert_true("material_editor_glass_response_thin_route",
                MaterialEditorApplyResponseValueToFocused(
                    MATERIAL_EDITOR_RESPONSE_FIELD_THIN_WALLED,
                    0.0));
    assert_true("material_editor_glass_response_stack_written",
                SceneEditorMaterialStackGetObjectStack(0, &stack));
    assert_close("material_editor_glass_response_roughness_value",
                 stack.layers[0].roughnessInfluence,
                 0.61,
                 1e-9);
    assert_close("material_editor_glass_response_reflect_value",
                 stack.layers[0].reflectivityInfluence,
                 0.27,
                 1e-9);
    assert_close("material_editor_glass_response_spec_value",
                 stack.layers[0].specularInfluence,
                 0.73,
                 1e-9);
    assert_true("material_editor_glass_response_tint_object_color",
                SceneObjectColorR(&sceneSettings.sceneObjects[0]) == 90u &&
                    SceneObjectColorG(&sceneSettings.sceneObjects[0]) == 150u &&
                    SceneObjectColorB(&sceneSettings.sceneObjects[0]) == 220u);
    assert_true("material_editor_glass_response_transport_override_written",
                sceneSettings.sceneObjects[0].hasGlassTransportOverride &&
                    sceneSettings.sceneObjects[0].glassTransmission == 0.58 &&
                    sceneSettings.sceneObjects[0].glassIor == 1.62 &&
                    sceneSettings.sceneObjects[0].glassAbsorptionDistance == 3.25 &&
                    !sceneSettings.sceneObjects[0].glassThinWalled);
    glass_preset = MaterialManagerGet(MATERIAL_PRESET_TRANSPARENT);
    assert_true("material_editor_glass_response_preset_preserved",
                glass_preset &&
                    glass_preset->transparency == 0.75f &&
                    glass_preset->ior == 1.45f &&
                    glass_preset->absorption_distance == 2.0f &&
                    glass_preset->thin_walled);
    assert_true("material_editor_glass_response_payload_uses_override",
                RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(0, &payload));
    assert_close("material_editor_glass_response_payload_trans",
                 payload.transparency,
                 0.58,
                 1e-9);
    assert_close("material_editor_glass_response_payload_ior",
                 payload.opticalIor,
                 1.62,
                 1e-9);
    assert_close("material_editor_glass_response_payload_absorb",
                 payload.absorptionDistance,
                 3.25,
                 1e-9);
    assert_true("material_editor_glass_response_payload_thin",
                !payload.thinWalled);
    assert_true("material_editor_glass_response_step_routes",
                MaterialEditorApplyResponseStepToFocused(
                    MATERIAL_EDITOR_RESPONSE_FIELD_ROUGHNESS,
                    0.05));
    assert_true("material_editor_glass_response_transport_step_routes",
                MaterialEditorApplyResponseStepToFocused(
                    MATERIAL_EDITOR_RESPONSE_FIELD_IOR,
                    0.05));
    assert_true("material_editor_glass_response_thin_step_toggles",
                MaterialEditorApplyResponseStepToFocused(
                    MATERIAL_EDITOR_RESPONSE_FIELD_THIN_WALLED,
                    0.05) &&
                    sceneSettings.sceneObjects[0].glassThinWalled);
    assert_true("material_editor_glass_response_step_stack_written",
                SceneEditorMaterialStackGetObjectStack(0, &stack));
    assert_close("material_editor_glass_response_step_value",
                 stack.layers[0].roughnessInfluence,
                 0.66,
                 1e-9);
    assert_true("material_editor_glass_response_marks_dirty",
                sceneSettings.sceneObjects[0].dirty);

    assert_true("material_editor_glass_response_readback_after_mutation",
                MaterialEditorBuildResponseReadback(&readback));
    trans = test_material_editor_response_row(&readback, "Trans");
    ior = test_material_editor_response_row(&readback, "IOR");
    absorb = test_material_editor_response_row(&readback, "Absorb");
    thin = test_material_editor_response_row(&readback, "Thin");
    reflect = test_material_editor_response_row(&readback, "Reflect");
    spec = test_material_editor_response_row(&readback, "Spec");
    tint = test_material_editor_response_row(&readback, "Tint");
    assert_true("material_editor_glass_response_readback_editable_fields",
                trans && ior && absorb && thin && reflect && spec && tint &&
                    trans->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE &&
                    ior->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE &&
                    absorb->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE &&
                    thin->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE &&
                    strcmp(trans->value, "0.58") == 0 &&
                    strcmp(ior->value, "1.67") == 0 &&
                    strcmp(absorb->value, "3.25") == 0 &&
                    strcmp(thin->value, "on") == 0 &&
                    reflect->field == MATERIAL_EDITOR_RESPONSE_FIELD_REFLECTIVITY &&
                    spec->field == MATERIAL_EDITOR_RESPONSE_FIELD_SPECULAR &&
                    tint->field == MATERIAL_EDITOR_RESPONSE_FIELD_TINT &&
                    reflect->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE &&
                    spec->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE &&
                    tint->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE);

    SceneEditorMaterialGraphResetAll();
    SceneEditorMaterialStackResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_glass_transport_override_is_object_local_and_loaded(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeMaterialPayload3D payload_a = {0};
    RuntimeMaterialPayload3D payload_b = {0};
    SceneObject loaded;
    struct json_object* obj = json_object_new_object();

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    memset(&loaded, 0, sizeof(loaded));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialGraphResetAll();

    sceneSettings.objectCount = 2;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 5.0, 0.0, NULL, 0);
    InitObject(&sceneSettings.sceneObjects[1], OBJECT_CIRCLE, 8.0, 0.0, 5.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[1].material_id = MATERIAL_PRESET_TRANSPARENT;
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();

    assert_true("material_editor_glass_transport_object_a_override",
                MaterialEditorApplyResponseValueToFocused(
                    MATERIAL_EDITOR_RESPONSE_FIELD_TRANSMISSION,
                    0.42) &&
                    MaterialEditorApplyResponseValueToFocused(
                        MATERIAL_EDITOR_RESPONSE_FIELD_IOR,
                        1.75) &&
                    MaterialEditorApplyResponseValueToFocused(
                        MATERIAL_EDITOR_RESPONSE_FIELD_ABSORPTION,
                        4.5) &&
                    MaterialEditorApplyResponseValueToFocused(
                        MATERIAL_EDITOR_RESPONSE_FIELD_THIN_WALLED,
                        0.0));
    assert_true("material_editor_glass_transport_payload_a",
                RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(0, &payload_a));
    assert_true("material_editor_glass_transport_payload_b",
                RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(1, &payload_b));
    assert_close("material_editor_glass_transport_a_trans",
                 payload_a.transparency,
                 0.42,
                 1e-9);
    assert_close("material_editor_glass_transport_a_ior",
                 payload_a.opticalIor,
                 1.75,
                 1e-9);
    assert_close("material_editor_glass_transport_a_absorb",
                 payload_a.absorptionDistance,
                 4.5,
                 1e-9);
    assert_true("material_editor_glass_transport_a_thin", !payload_a.thinWalled);
    assert_close("material_editor_glass_transport_b_trans_preset",
                 payload_b.transparency,
                 0.75,
                 1e-9);
    assert_close("material_editor_glass_transport_b_ior_preset",
                 payload_b.opticalIor,
                 1.45,
                 1e-6);
    assert_close("material_editor_glass_transport_b_absorb_preset",
                 payload_b.absorptionDistance,
                 2.0,
                 1e-9);
    assert_true("material_editor_glass_transport_b_thin_preset", payload_b.thinWalled);

    ObjectEditorObjectAssignMaterial(&sceneSettings.sceneObjects[0], MATERIAL_PRESET_ROUGH_METAL);
    assert_true("material_editor_glass_transport_override_reset_on_material_change",
                !sceneSettings.sceneObjects[0].hasGlassTransportOverride);

    json_object_object_add(obj, "texture", json_object_new_string(""));
    json_object_object_add(obj, "color", json_object_new_int(0xFFFFFF));
    json_object_object_add(obj, "materialId", json_object_new_int(MATERIAL_PRESET_TRANSPARENT));
    json_object_object_add(obj, "glassTransportOverride", json_object_new_boolean(true));
    json_object_object_add(obj, "glassTransmission", json_object_new_double(0.36));
    json_object_object_add(obj, "glassIor", json_object_new_double(1.58));
    json_object_object_add(obj, "glassAbsorptionDistance", json_object_new_double(2.75));
    json_object_object_add(obj, "glassThinWalled", json_object_new_boolean(false));
    LoadObjectProperties(obj, &loaded);
    assert_true("material_editor_glass_transport_load_override",
                loaded.hasGlassTransportOverride &&
                    loaded.material_id == MATERIAL_PRESET_TRANSPARENT &&
                    loaded.glassTransmission == 0.36 &&
                    loaded.glassIor == 1.58 &&
                    loaded.glassAbsorptionDistance == 2.75 &&
                    !loaded.glassThinWalled);
    json_object_put(obj);

    SceneEditorMaterialGraphResetAll();
    SceneEditorMaterialStackResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_mirror_response_readback_gets_family_matrix(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    MaterialEditorResponseReadback readback = {0};
    const MaterialEditorResponseRow* reflect = NULL;
    const MaterialEditorResponseRow* rough = NULL;
    const MaterialEditorResponseRow* spec = NULL;
    const MaterialEditorResponseRow* tint = NULL;
    const MaterialEditorResponseRow* dominance = NULL;
    const MaterialEditorResponseRow* base = NULL;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialGraphResetAll();

    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 5.0, 0.0, NULL, 0);
    ObjectEditorObjectAssignMaterial(&sceneSettings.sceneObjects[0], MATERIAL_PRESET_MIRROR);
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();

    assert_true("material_editor_mirror_response_build",
                MaterialEditorBuildResponseReadback(&readback));
    reflect = test_material_editor_response_row(&readback, "Reflect");
    rough = test_material_editor_response_row(&readback, "Rough");
    spec = test_material_editor_response_row(&readback, "Spec");
    tint = test_material_editor_response_row(&readback, "Tint");
    dominance = test_material_editor_response_row(&readback, "Domin");
    base = test_material_editor_response_row(&readback, "Base");
    assert_true("material_editor_mirror_response_family",
                readback.family == MATERIAL_EDITOR_RESPONSE_FAMILY_MIRROR &&
                    readback.family_specific &&
                    !readback.has_guarded_fields &&
                    strcmp(readback.title, "Mirror Response") == 0 &&
                    strstr(readback.route_label, "mirror family matrix") != NULL);
    assert_true("material_editor_mirror_response_rows",
                reflect && rough && spec && tint && dominance && base &&
                    readback.row_count == 6);
    assert_true("material_editor_mirror_response_editable_core",
                strcmp(reflect->value, "0.95") == 0 &&
                    strcmp(rough->value, "0.00") == 0 &&
                    strcmp(spec->value, "1.00") == 0 &&
                    strcmp(tint->value, "1.00 1.00 1.00") == 0 &&
                    reflect->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE &&
                    rough->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE &&
                    spec->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE &&
                    tint->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE);
    assert_true("material_editor_mirror_response_dominance_readback",
                strcmp(dominance->value, "0.95") == 0 &&
                    strcmp(base->value, "0.05") == 0 &&
                    dominance->field == MATERIAL_EDITOR_RESPONSE_FIELD_MIRROR_DOMINANCE &&
                    base->field == MATERIAL_EDITOR_RESPONSE_FIELD_MIRROR_BASE &&
                    dominance->state == MATERIAL_EDITOR_RESPONSE_FIELD_READBACK &&
                    base->state == MATERIAL_EDITOR_RESPONSE_FIELD_READBACK);

    SceneEditorMaterialGraphResetAll();
    SceneEditorMaterialStackResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_mirror_response_override_is_object_local_and_loaded(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeMaterialPayload3D payload_a = {0};
    RuntimeMaterialPayload3D payload_b = {0};
    MaterialEditorResponseReadback readback = {0};
    const MaterialEditorResponseRow* reflect = NULL;
    const MaterialEditorResponseRow* rough = NULL;
    const MaterialEditorResponseRow* spec = NULL;
    const MaterialEditorResponseRow* tint = NULL;
    const Material* mirror_preset = NULL;
    SceneObject loaded;
    struct json_object* obj = json_object_new_object();

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    memset(&loaded, 0, sizeof(loaded));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialGraphResetAll();

    sceneSettings.objectCount = 2;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 5.0, 0.0, NULL, 0);
    InitObject(&sceneSettings.sceneObjects[1], OBJECT_CIRCLE, 8.0, 0.0, 5.0, 0.0, NULL, 0);
    ObjectEditorObjectAssignMaterial(&sceneSettings.sceneObjects[0], MATERIAL_PRESET_MIRROR);
    ObjectEditorObjectAssignMaterial(&sceneSettings.sceneObjects[1], MATERIAL_PRESET_MIRROR);
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();

    assert_true("material_editor_mirror_response_reflect_route",
                MaterialEditorApplyResponseValueToFocused(
                    MATERIAL_EDITOR_RESPONSE_FIELD_REFLECTIVITY,
                    0.82));
    assert_true("material_editor_mirror_response_rough_route",
                MaterialEditorApplyResponseValueToFocused(
                    MATERIAL_EDITOR_RESPONSE_FIELD_ROUGHNESS,
                    0.12));
    assert_true("material_editor_mirror_response_spec_route",
                MaterialEditorApplyResponseValueToFocused(
                    MATERIAL_EDITOR_RESPONSE_FIELD_SPECULAR,
                    0.64));
    assert_true("material_editor_mirror_response_tint_route",
                MaterialEditorApplyResponseTintToFocused(
                    SceneObjectPackRGBBytes(190u, 210u, 230u)));
    assert_true("material_editor_mirror_response_override_written",
                sceneSettings.sceneObjects[0].hasMirrorResponseOverride &&
                    sceneSettings.sceneObjects[0].mirrorReflectivity == 0.82 &&
                    sceneSettings.sceneObjects[0].mirrorRoughness == 0.12 &&
                    sceneSettings.sceneObjects[0].mirrorSpecular == 0.64 &&
                    sceneSettings.sceneObjects[0].mirrorTint ==
                        SceneObjectPackRGBBytes(190u, 210u, 230u) &&
                    sceneSettings.sceneObjects[0].reflectivity == 0.82 &&
                    sceneSettings.sceneObjects[0].roughness == 0.12);
    mirror_preset = MaterialManagerGet(MATERIAL_PRESET_MIRROR);
    assert_true("material_editor_mirror_response_preset_preserved",
                mirror_preset &&
                    mirror_preset->reflectivity == 0.95f &&
                    mirror_preset->roughness == 0.0f &&
                    mirror_preset->specular == 0.1f);
    assert_true("material_editor_mirror_response_payload_a",
                RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(0, &payload_a));
    assert_close("material_editor_mirror_response_payload_reflect",
                 payload_a.bsdf.reflectivity,
                 0.82,
                 1e-9);
    assert_close("material_editor_mirror_response_payload_rough",
                 payload_a.bsdf.roughness,
                 0.12,
                 1e-9);
    assert_close("material_editor_mirror_response_payload_spec",
                 payload_a.bsdf.specWeight,
                 0.64,
                 1e-9);
    assert_close("material_editor_mirror_response_payload_tint_r",
                 payload_a.baseColorR,
                 190.0 / 255.0,
                 1e-9);
    assert_true("material_editor_mirror_response_payload_b",
                RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(1, &payload_b));
    assert_close("material_editor_mirror_response_payload_b_reflect_preset",
                 payload_b.bsdf.reflectivity,
                 0.95,
                 1e-6);
    assert_close("material_editor_mirror_response_payload_b_rough_floor",
                 payload_b.bsdf.roughness,
                 0.02,
                 1e-9);

    assert_true("material_editor_mirror_response_step_routes",
                MaterialEditorApplyResponseStepToFocused(
                    MATERIAL_EDITOR_RESPONSE_FIELD_ROUGHNESS,
                    0.05));
    assert_close("material_editor_mirror_response_step_value",
                 sceneSettings.sceneObjects[0].mirrorRoughness,
                 0.17,
                 1e-9);
    assert_true("material_editor_mirror_response_readback_after_mutation",
                MaterialEditorBuildResponseReadback(&readback));
    reflect = test_material_editor_response_row(&readback, "Reflect");
    rough = test_material_editor_response_row(&readback, "Rough");
    spec = test_material_editor_response_row(&readback, "Spec");
    tint = test_material_editor_response_row(&readback, "Tint");
    assert_true("material_editor_mirror_response_readback_override",
                reflect && rough && spec && tint &&
                    strcmp(reflect->value, "0.82") == 0 &&
                    strcmp(rough->value, "0.17") == 0 &&
                    strcmp(spec->value, "0.64") == 0 &&
                    strcmp(tint->value, "0.75 0.82 0.90") == 0 &&
                    reflect->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE &&
                    rough->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE &&
                    spec->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE &&
                    tint->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE);

    ObjectEditorObjectAssignMaterial(&sceneSettings.sceneObjects[0], MATERIAL_PRESET_ROUGH_METAL);
    assert_true("material_editor_mirror_response_override_reset_on_material_change",
                !sceneSettings.sceneObjects[0].hasMirrorResponseOverride);

    json_object_object_add(obj, "texture", json_object_new_string(""));
    json_object_object_add(obj, "color", json_object_new_int(0xFFFFFF));
    json_object_object_add(obj, "materialId", json_object_new_int(MATERIAL_PRESET_MIRROR));
    json_object_object_add(obj, "mirrorResponseOverride", json_object_new_boolean(true));
    json_object_object_add(obj, "mirrorReflectivity", json_object_new_double(0.71));
    json_object_object_add(obj, "mirrorRoughness", json_object_new_double(0.08));
    json_object_object_add(obj, "mirrorSpecular", json_object_new_double(0.92));
    json_object_object_add(obj, "mirrorTint", json_object_new_int(0xA0B0C0));
    LoadObjectProperties(obj, &loaded);
    assert_true("material_editor_mirror_response_load_override",
                loaded.hasMirrorResponseOverride &&
                    loaded.material_id == MATERIAL_PRESET_MIRROR &&
                    loaded.mirrorReflectivity == 0.71 &&
                    loaded.mirrorRoughness == 0.08 &&
                    loaded.mirrorSpecular == 0.92 &&
                    loaded.mirrorTint == 0xA0B0C0);
    json_object_put(obj);

    SceneEditorMaterialGraphResetAll();
    SceneEditorMaterialStackResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_metal_response_routes_stack_preview_payload_and_proof(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    MaterialEditorResponseReadback readback = {0};
    MaterialEditorProofReadback proof = {0};
    RuntimeMaterialSurfaceEval preview_eval = {0};
    RuntimeMaterialPayload3D payload = {0};
    HitInfo3D hit = {0};
    const MaterialEditorResponseRow* rough = NULL;
    const MaterialEditorResponseRow* reflect = NULL;
    const MaterialEditorResponseRow* spec = NULL;
    const MaterialEditorResponseRow* tint = NULL;
    const MaterialEditorResponseRow* metal = NULL;
    const MaterialEditorResponseRow* base = NULL;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialGraphResetAll();

    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 5.0, 0.0, NULL, 0);
    ObjectEditorObjectAssignMaterial(&sceneSettings.sceneObjects[0], MATERIAL_PRESET_ROUGH_METAL);
    stack.layerCount = 1;
    stack.layers[0] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID);
    assert_true("material_editor_metal_response_seed_stack",
                SceneEditorMaterialStackSetObjectStack(0, &stack));
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();
    assert_true("material_editor_metal_response_select_base",
                MaterialEditorSetActiveLayerIndex(0));

    assert_true("material_editor_metal_response_default_build",
                MaterialEditorBuildResponseReadback(&readback));
    rough = test_material_editor_response_row(&readback, "Rough");
    reflect = test_material_editor_response_row(&readback, "Reflect");
    spec = test_material_editor_response_row(&readback, "Spec");
    tint = test_material_editor_response_row(&readback, "Tint");
    metal = test_material_editor_response_row(&readback, "Metal");
    base = test_material_editor_response_row(&readback, "Base");
    assert_true("material_editor_metal_response_family",
                readback.family == MATERIAL_EDITOR_RESPONSE_FAMILY_METAL &&
                    readback.family_specific &&
                    readback.has_guarded_fields &&
                    strcmp(readback.title, "Metal Response") == 0 &&
                    strstr(readback.subtitle, "Rough conductor") != NULL &&
                    strstr(readback.route_label, "metal family matrix") != NULL);
    assert_true("material_editor_metal_response_rows",
                rough && reflect && spec && tint && metal && base &&
                    rough->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE &&
                    reflect->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE &&
                    spec->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE &&
                    tint->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE &&
                    metal->field == MATERIAL_EDITOR_RESPONSE_FIELD_METALLIC &&
                    metal->state == MATERIAL_EDITOR_RESPONSE_FIELD_GUARDED &&
                    strcmp(metal->value, "1.00") == 0 &&
                    base->field == MATERIAL_EDITOR_RESPONSE_FIELD_DIFFUSE_BASE &&
                    base->state == MATERIAL_EDITOR_RESPONSE_FIELD_READBACK);

    assert_true("material_editor_metal_proof_default_build",
                MaterialEditorBuildFocusedProofReadback(&proof));
    assert_true("material_editor_metal_proof_default",
                proof.metal_proof_readback &&
                    !proof.glass_proof_readback &&
                    !proof.mirror_proof_readback &&
                    strcmp(proof.metal_proof_case, "default rough metal") == 0 &&
                    strcmp(proof.metal_proof_package,
                           "m11_s5_material_family_preview_grid") == 0 &&
                    strstr(proof.metal_proof_coverage, "rough Metal contrast") != NULL);

    assert_true("material_editor_metal_response_rough_route",
                MaterialEditorApplyResponseValueToFocused(
                    MATERIAL_EDITOR_RESPONSE_FIELD_ROUGHNESS,
                    0.41));
    assert_true("material_editor_metal_response_reflect_route",
                MaterialEditorApplyResponseValueToFocused(
                    MATERIAL_EDITOR_RESPONSE_FIELD_REFLECTIVITY,
                    0.63));
    assert_true("material_editor_metal_response_spec_route",
                MaterialEditorApplyResponseValueToFocused(
                    MATERIAL_EDITOR_RESPONSE_FIELD_SPECULAR,
                    0.78));
    assert_true("material_editor_metal_response_tint_route",
                MaterialEditorApplyResponseTintToFocused(
                    SceneObjectPackRGBBytes(170u, 187u, 204u)));
    assert_true("material_editor_metal_response_stack_written",
                SceneEditorMaterialStackGetObjectStack(0, &stack));
    assert_close("material_editor_metal_response_stack_rough",
                 stack.layers[0].roughnessInfluence,
                 0.41,
                 1e-9);
    assert_close("material_editor_metal_response_stack_reflect",
                 stack.layers[0].reflectivityInfluence,
                 0.63,
                 1e-9);
    assert_close("material_editor_metal_response_stack_spec",
                 stack.layers[0].specularInfluence,
                 0.78,
                 1e-9);
    assert_true("material_editor_metal_response_tint_object_color",
                SceneObjectColorR(&sceneSettings.sceneObjects[0]) == 170u &&
                    SceneObjectColorG(&sceneSettings.sceneObjects[0]) == 187u &&
                    SceneObjectColorB(&sceneSettings.sceneObjects[0]) == 204u &&
                    !sceneSettings.sceneObjects[0].hasMirrorResponseOverride &&
                    !sceneSettings.sceneObjects[0].hasGlassTransportOverride);
    assert_true("material_editor_metal_response_step_routes",
                MaterialEditorApplyResponseStepToFocused(
                    MATERIAL_EDITOR_RESPONSE_FIELD_ROUGHNESS,
                    0.05));
    assert_true("material_editor_metal_response_step_stack_written",
                SceneEditorMaterialStackGetObjectStack(0, &stack));
    assert_close("material_editor_metal_response_step_rough",
                 stack.layers[0].roughnessInfluence,
                 0.46,
                 1e-9);

    assert_true("material_editor_metal_response_readback_after_mutation",
                MaterialEditorBuildResponseReadback(&readback));
    rough = test_material_editor_response_row(&readback, "Rough");
    reflect = test_material_editor_response_row(&readback, "Reflect");
    spec = test_material_editor_response_row(&readback, "Spec");
    tint = test_material_editor_response_row(&readback, "Tint");
    metal = test_material_editor_response_row(&readback, "Metal");
    assert_true("material_editor_metal_response_readback_values",
                rough && reflect && spec && tint && metal &&
                    strcmp(rough->value, "0.46") == 0 &&
                    strcmp(reflect->value, "0.63") == 0 &&
                    strcmp(spec->value, "0.78") == 0 &&
                    strcmp(tint->value, "0.67 0.73 0.80") == 0 &&
                    strcmp(metal->value, "1.00") == 0 &&
                    metal->state == MATERIAL_EDITOR_RESPONSE_FIELD_GUARDED);

    assert_true("material_editor_metal_preview_eval",
                MaterialPreviewSurfaceEvaluateObject(&sceneSettings.sceneObjects[0],
                                                     0,
                                                     NULL,
                                                     0.25,
                                                     0.5,
                                                     &preview_eval));
    assert_close("material_editor_metal_preview_rough",
                 preview_eval.roughness,
                 0.46,
                 1e-9);
    assert_close("material_editor_metal_preview_reflect",
                 preview_eval.reflectivity,
                 0.63,
                 1e-9);
    assert_close("material_editor_metal_preview_spec",
                 preview_eval.specWeight,
                 0.78,
                 1e-9);
    HitInfo3D_Reset(&hit);
    hit.sceneObjectIndex = 0;
    hit.triangleIndex = 0;
    hit.localTriangleIndex = -1;
    hit.primitiveIndex = 0;
    hit.hasObjectTextureCoord = true;
    hit.objectTextureCoord.x = 0.25;
    hit.objectTextureCoord.y = 0.5;
    hit.normal.z = 1.0;
    assert_true("material_editor_metal_payload_eval",
                RuntimeMaterialPayload3D_ResolveFromHit(&hit, &payload));
    assert_close("material_editor_metal_payload_rough",
                 payload.bsdf.roughness,
                 preview_eval.roughness,
                 1e-9);
    assert_close("material_editor_metal_payload_reflect",
                 payload.bsdf.reflectivity,
                 preview_eval.reflectivity,
                 1e-9);
    assert_close("material_editor_metal_payload_spec",
                 payload.bsdf.specWeight,
                 preview_eval.specWeight,
                 1e-9);

    assert_true("material_editor_metal_proof_tinted_build",
                MaterialEditorBuildFocusedProofReadback(&proof));
    assert_true("material_editor_metal_proof_tinted",
                strcmp(proof.metal_proof_case, "tinted rough metal") == 0 &&
                    strcmp(proof.metal_proof_package,
                           "m11_s5_material_family_preview_grid") == 0 &&
                    strstr(proof.metal_missing_proof, "metallic remains guarded") != NULL);

    stack.layerCount = 2;
    stack.layers[1] =
        RuntimeMaterialTextureLayerMakeOverlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST);
    stack.layers[1].roughnessInfluence = 0.82;
    stack.layers[1].reflectivityInfluence = 0.28;
    stack.layers[1].specularInfluence = 0.36;
    assert_true("material_editor_metal_response_rust_stack_set",
                SceneEditorMaterialStackSetObjectStack(0, &stack));
    assert_true("material_editor_metal_response_select_rust",
                MaterialEditorSetActiveLayerIndex(1));
    assert_true("material_editor_metal_proof_rust_build",
                MaterialEditorBuildFocusedProofReadback(&proof));
    assert_true("material_editor_metal_proof_rust_overlay",
                strcmp(proof.metal_proof_case, "Rust metal overlay") == 0 &&
                    strcmp(proof.metal_proof_package,
                           "m11_s5_material_family_preview_grid") == 0 &&
                    strstr(proof.metal_proof_coverage, "damaged Metal overlays") != NULL &&
                    strstr(proof.metal_missing_proof, "metallic still guarded") != NULL);

    SceneEditorMaterialGraphResetAll();
    SceneEditorMaterialStackResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_response_mutation_rejects_non_glass_family(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialGraphResetAll();

    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 5.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_GLOSSY;
    stack.layerCount = 1;
    stack.layers[0] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID);
    assert_true("material_editor_non_glass_response_mutation_seed_stack",
                SceneEditorMaterialStackSetObjectStack(0, &stack));
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();

    assert_true("material_editor_non_glass_response_mutation_rejected",
                !MaterialEditorApplyResponseValueToFocused(
                    MATERIAL_EDITOR_RESPONSE_FIELD_ROUGHNESS,
                    0.91));
    assert_true("material_editor_non_glass_response_tint_rejected",
                !MaterialEditorApplyResponseTintToFocused(
                    SceneObjectPackRGBBytes(120u, 120u, 120u)));
    assert_true("material_editor_non_glass_response_stack_unchanged",
                SceneEditorMaterialStackGetObjectStack(0, &stack) &&
                    stack.layers[0].roughnessInfluence == 0.0);

    SceneEditorMaterialGraphResetAll();
    SceneEditorMaterialStackResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_glass_overlay_affordances_add_select_and_readback(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    MaterialEditorResponseReadback readback = {0};
    const MaterialEditorResponseRow* reflect = NULL;
    const MaterialEditorResponseRow* spec = NULL;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialGraphResetAll();

    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 5.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();

    assert_true("material_editor_glass_overlay_add_fog",
                MaterialEditorApplyGlassOverlayForFocused(
                    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG));
    assert_true("material_editor_glass_overlay_fog_stack",
                SceneEditorMaterialStackGetObjectStack(0, &stack) &&
                    stack.layerCount == 2 &&
                    stack.layers[1].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG &&
                    MaterialEditorGetActiveLayerIndex() == 1);
    assert_true("material_editor_glass_overlay_fog_readback",
                MaterialEditorBuildResponseReadback(&readback) &&
                    (reflect = test_material_editor_response_row(&readback, "Reflect")) != NULL &&
                    strcmp(reflect->value, "0.00") == 0 &&
                    strstr(readback.route_label, "selected layer") != NULL);

    assert_true("material_editor_glass_overlay_add_oil",
                MaterialEditorApplyGlassOverlayForFocused(
                    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL));
    assert_true("material_editor_glass_overlay_oil_stack",
                SceneEditorMaterialStackGetObjectStack(0, &stack) &&
                    stack.layerCount == 3 &&
                    stack.layers[2].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL &&
                    MaterialEditorGetActiveLayerIndex() == 2);
    assert_true("material_editor_glass_overlay_oil_readback",
                MaterialEditorBuildResponseReadback(&readback));
    reflect = test_material_editor_response_row(&readback, "Reflect");
    spec = test_material_editor_response_row(&readback, "Spec");
    assert_true("material_editor_glass_overlay_oil_response_values",
                reflect && spec &&
                    strcmp(reflect->value, "0.35") == 0 &&
                    strcmp(spec->value, "0.55") == 0 &&
                    reflect->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE &&
                    spec->state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE);

    assert_true("material_editor_glass_overlay_reselect_fog",
                MaterialEditorApplyGlassOverlayForFocused(
                    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG));
    assert_true("material_editor_glass_overlay_reselect_no_duplicate",
                SceneEditorMaterialStackGetObjectStack(0, &stack) &&
                    stack.layerCount == 3 &&
                    MaterialEditorGetActiveLayerIndex() == 1);
    assert_true("material_editor_glass_overlay_clear_selects_base",
                MaterialEditorApplyGlassOverlayForFocused(
                    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE) &&
                    MaterialEditorGetActiveLayerIndex() == 0 &&
                    SceneEditorMaterialStackGetObjectStack(0, &stack) &&
                    stack.layerCount == 3);
    assert_true("material_editor_glass_overlay_marks_dirty",
                sceneSettings.sceneObjects[0].dirty);

    SceneEditorMaterialGraphResetAll();
    SceneEditorMaterialStackResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_glass_overlay_affordances_reject_non_glass(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialGraphResetAll();

    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 5.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_ROUGH_METAL;
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();

    assert_true("material_editor_non_glass_overlay_shortcut_rejected",
                !MaterialEditorApplyGlassOverlayForFocused(
                    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG));
    assert_true("material_editor_non_glass_overlay_no_stack",
                !SceneEditorMaterialStackGetObjectStack(0, &stack));

    SceneEditorMaterialGraphResetAll();
    SceneEditorMaterialStackResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_response_readback_keeps_non_glass_on_generic_matrix(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    MaterialEditorResponseReadback readback = {0};

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialGraphResetAll();

    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 5.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_GLOSSY;
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();

    assert_true("material_editor_response_glossy_build",
                MaterialEditorBuildResponseReadback(&readback));
    assert_true("material_editor_response_glossy_generic_matrix",
                readback.family == MATERIAL_EDITOR_RESPONSE_FAMILY_GENERIC &&
                    !readback.family_specific &&
                    strcmp(readback.title, "Material Response") == 0 &&
                    strstr(readback.subtitle, "Generic selected-layer response") != NULL &&
                    test_material_editor_response_row(&readback, "Reflect") != NULL);

    SceneEditorMaterialGraphResetAll();
    SceneEditorMaterialStackResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_texture_channel_readback_groups_ownership(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char* texture_dir = "/tmp/ray_tracing_material_editor_s4_channels";
    const char* texture_png = "/tmp/ray_tracing_material_editor_s4_channels/channel_plane.png";
    const char* texture_manifest =
        "/tmp/ray_tracing_material_editor_s4_channels/channel_manifest.json";
    unsigned char texture_rgba[] = {120u, 180u, 210u, 255u};
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    MaterialEditorTextureChannelReadback readback = {0};

    (void)mkdir(texture_dir, 0775);
    assert_true("material_editor_channel_readback_png_write",
                test_scene_editor_write_png_rgba(texture_png, texture_rgba, 1u, 1u));
    assert_true("material_editor_channel_readback_manifest_write",
                test_scene_editor_write_channel_authored_texture_manifest(texture_manifest,
                                                                          "channel_plane",
                                                                          "channel_plane.png"));

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialGraphResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_POLYGON, 0.0, 0.0, 1.0, 0.0, NULL, 0);
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();

    stack.layerCount = 2;
    stack.layers[0] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID);
    stack.layers[1] =
        RuntimeMaterialTextureLayerMakeOverlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST);
    snprintf(stack.layers[1].layerId, sizeof(stack.layers[1].layerId), "%s", "rust_channel");
    snprintf(stack.layers[1].displayName, sizeof(stack.layers[1].displayName), "%s", "Rust Channel");
    assert_true("material_editor_channel_readback_stack_set",
                SceneEditorMaterialStackSetObjectStack(0, &stack));
    assert_true("material_editor_channel_readback_layer_select",
                MaterialEditorSetActiveLayerIndex(1));
    assert_true("material_editor_channel_readback_bind_manifest",
                RuntimeMaterialAuthoredTextureBindManifestForObject(0,
                                                                    "channel_plane",
                                                                    texture_manifest,
                                                                    "override"));
    assert_true("material_editor_channel_readback_build",
                MaterialEditorBuildTextureChannelReadback(0, &readback));
    assert_true("material_editor_channel_readback_visual",
                readback.has_authored_channels &&
                    readback.visual_count == 1 &&
                    strstr(readback.visual_channels, "base_color.rgb") != NULL);
    assert_true("material_editor_channel_readback_physical",
                readback.physical_count == 1 &&
                    strstr(readback.physical_channels, "roughness.scalar") != NULL);
    assert_true("material_editor_channel_readback_future",
                readback.future_count == 1 &&
                    strstr(readback.future_channels, "normal.tangent") != NULL);
    assert_true("material_editor_channel_readback_deferred",
                strstr(readback.deferred_channels, "displacement.height") != NULL);
    assert_true("material_editor_channel_readback_procedural_source",
                readback.has_procedural_source &&
                    strstr(readback.procedural_source, "Rust Channel") != NULL &&
                    strstr(readback.procedural_source, "placement") != NULL);
    assert_true("material_editor_channel_readback_non_glass_has_no_mapping",
                !readback.has_glass_mapping);

    RuntimeMaterialAuthoredTextureResetAll();
    SceneEditorMaterialStackResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_glass_channel_mapping_labels_authored_and_overlay_intent(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char* texture_dir = "/tmp/ray_tracing_material_editor_s4_glass_channels";
    const char* texture_png = "/tmp/ray_tracing_material_editor_s4_glass_channels/glass_channel.png";
    const char* texture_manifest =
        "/tmp/ray_tracing_material_editor_s4_glass_channels/glass_channel_manifest.json";
    unsigned char texture_rgba[] = {90u, 160u, 220u, 180u};
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    MaterialEditorTextureChannelReadback readback = {0};

    (void)mkdir(texture_dir, 0775);
    assert_true("material_editor_glass_channel_png_write",
                test_scene_editor_write_png_rgba(texture_png, texture_rgba, 1u, 1u));
    assert_true("material_editor_glass_channel_manifest_write",
                test_scene_editor_write_glass_channel_authored_texture_manifest(texture_manifest,
                                                                                "glass_channel",
                                                                                "glass_channel.png"));

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialGraphResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_POLYGON, 0.0, 0.0, 1.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();

    stack.layerCount = 2;
    stack.layers[0] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID);
    stack.layers[1] =
        RuntimeMaterialTextureLayerMakeOverlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SCRATCHES);
    snprintf(stack.layers[1].displayName, sizeof(stack.layers[1].displayName), "%s", "Scratch Channel");
    assert_true("material_editor_glass_channel_stack_set",
                SceneEditorMaterialStackSetObjectStack(0, &stack));
    assert_true("material_editor_glass_channel_layer_select",
                MaterialEditorSetActiveLayerIndex(1));
    assert_true("material_editor_glass_channel_bind_manifest",
                RuntimeMaterialAuthoredTextureBindManifestForObject(0,
                                                                    "glass_channel",
                                                                    texture_manifest,
                                                                    "override"));
    assert_true("material_editor_glass_channel_readback_build",
                MaterialEditorBuildTextureChannelReadback(0, &readback));
    assert_true("material_editor_glass_channel_mapping_present",
                readback.has_glass_mapping &&
                    strstr(readback.glass_authored_mapping, "base_color.rgb -> glass tint") != NULL &&
                    strstr(readback.glass_authored_mapping, "roughness.scalar -> clarity/frost") != NULL &&
                    strstr(readback.glass_authored_mapping, "alpha/coverage -> physical coverage only") != NULL &&
                    strstr(readback.glass_authored_mapping, "transmission.weight -> guarded transmission intent") != NULL &&
                    strstr(readback.glass_authored_mapping, "normal/bump -> future scratch/frost detail") != NULL);
    assert_true("material_editor_glass_channel_overlay_intent",
                strstr(readback.glass_procedural_mapping, "Scratches") != NULL &&
                    strstr(readback.glass_procedural_mapping, "roughness/spec") != NULL);
    assert_true("material_editor_glass_channel_deferred_boundary",
                strstr(readback.glass_deferred_mapping, "transmission.weight") != NULL &&
                    strstr(readback.glass_deferred_mapping, "readback intent") != NULL);

    RuntimeMaterialAuthoredTextureResetAll();
    SceneEditorMaterialStackResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_glass_proof_readback_maps_current_state_to_m4_coverage(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    MaterialEditorProofReadback proof = {0};

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialGraphResetAll();

    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 5.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[0].color = SceneObjectPackRGBBytes(255u, 255u, 255u);
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();

    stack.layerCount = 1;
    stack.layers[0] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID);
    assert_true("material_editor_glass_proof_clear_stack_set",
                SceneEditorMaterialStackSetObjectStack(0, &stack));
    assert_true("material_editor_glass_proof_clear_build",
                MaterialEditorBuildFocusedProofReadback(&proof));
    assert_true("material_editor_glass_proof_clear_m4_s1",
                proof.glass_proof_readback &&
                    strcmp(proof.glass_proof_case, "clear glass") == 0 &&
                    strcmp(proof.glass_proof_package,
                           "m4_s1_glass_roughness_transmission_matrix") == 0 &&
                    strstr(proof.glass_proof_coverage, "clear/smooth") != NULL &&
                    strstr(proof.glass_missing_proof, "caustics") != NULL &&
                    proof.launch_deferred);

    stack.layers[0].roughnessInfluence = 0.62;
    assert_true("material_editor_glass_proof_frosted_stack_set",
                SceneEditorMaterialStackSetObjectStack(0, &stack));
    assert_true("material_editor_glass_proof_frosted_build",
                MaterialEditorBuildFocusedProofReadback(&proof));
    assert_true("material_editor_glass_proof_frosted_m4_s1",
                strcmp(proof.glass_proof_case, "frosted glass") == 0 &&
                    strcmp(proof.glass_proof_package,
                           "m4_s1_glass_roughness_transmission_matrix") == 0 &&
                    strstr(proof.glass_proof_coverage, "smooth-to-frosted") != NULL);

    stack.layers[0].roughnessInfluence = 0.08;
    sceneSettings.sceneObjects[0].color = SceneObjectPackRGBBytes(120u, 170u, 230u);
    assert_true("material_editor_glass_proof_tinted_stack_set",
                SceneEditorMaterialStackSetObjectStack(0, &stack));
    assert_true("material_editor_glass_proof_tinted_build",
                MaterialEditorBuildFocusedProofReadback(&proof));
    assert_true("material_editor_glass_proof_tinted_partial",
                strcmp(proof.glass_proof_case, "tinted glass") == 0 &&
                    strcmp(proof.glass_proof_package,
                           "m4_s1_glass_roughness_transmission_matrix") == 0 &&
                    strstr(proof.glass_proof_coverage, "partial") != NULL &&
                    strstr(proof.glass_missing_proof, "tinted glass") != NULL);

    stack.layerCount = 2;
    stack.layers[1] =
        RuntimeMaterialTextureLayerMakeOverlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME);
    assert_true("material_editor_glass_proof_dirty_stack_set",
                SceneEditorMaterialStackSetObjectStack(0, &stack));
    assert_true("material_editor_glass_proof_dirty_select_overlay",
                MaterialEditorSetActiveLayerIndex(1));
    assert_true("material_editor_glass_proof_dirty_build",
                MaterialEditorBuildFocusedProofReadback(&proof));
    assert_true("material_editor_glass_proof_dirty_m4_s3",
                strcmp(proof.glass_proof_case, "Grime glass overlay") == 0 &&
                    strcmp(proof.glass_proof_package,
                           "m4_s3_overlay_stack_response_matrix") == 0 &&
                    strstr(proof.glass_proof_coverage, "fog/scratches/oil/grime") != NULL &&
                    strstr(proof.glass_missing_proof, "combined tinted dirty glass") != NULL);

    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_ROUGH_METAL;
    assert_true("material_editor_non_glass_proof_has_no_glass_mapping",
                MaterialEditorBuildFocusedProofReadback(&proof) &&
                    !proof.glass_proof_readback);

    SceneEditorMaterialGraphResetAll();
    SceneEditorMaterialStackResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_material_editor_mirror_proof_readback_maps_current_state_to_coverage(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    MaterialEditorProofReadback proof = {0};

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialGraphResetAll();

    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 5.0, 0.0, NULL, 0);
    ObjectEditorObjectAssignMaterial(&sceneSettings.sceneObjects[0], MATERIAL_PRESET_MIRROR);
    ObjectEditorSelectionTrackerSetCurrent(0, sceneSettings.objectCount);
    InitializeMaterialEditor();

    assert_true("material_editor_mirror_proof_default_build",
                MaterialEditorBuildFocusedProofReadback(&proof));
    assert_true("material_editor_mirror_proof_default_su4",
                proof.mirror_proof_readback &&
                    !proof.glass_proof_readback &&
                    strcmp(proof.mirror_proof_case, "default mirror") == 0 &&
                    strcmp(proof.mirror_proof_package,
                           "su4_mirror_surface_unification_matrix") == 0 &&
                    strstr(proof.mirror_proof_coverage, "default mirror dominance") != NULL &&
                    strstr(proof.mirror_missing_proof, "readback-only") != NULL);

    sceneSettings.sceneObjects[0].color = SceneObjectPackRGBBytes(180u, 210u, 240u);
    assert_true("material_editor_mirror_proof_tinted_build",
                MaterialEditorBuildFocusedProofReadback(&proof));
    assert_true("material_editor_mirror_proof_tinted_su4",
                strcmp(proof.mirror_proof_case, "tinted mirror") == 0 &&
                    strcmp(proof.mirror_proof_package,
                           "su4_mirror_surface_unification_matrix") == 0 &&
                    strstr(proof.mirror_proof_coverage, "tint readback") != NULL &&
                    strstr(proof.mirror_missing_proof, "tinted mirror") != NULL);

    assert_true("material_editor_mirror_proof_rough_route",
                MaterialEditorApplyResponseValueToFocused(
                    MATERIAL_EDITOR_RESPONSE_FIELD_ROUGHNESS,
                    0.32));
    assert_true("material_editor_mirror_proof_rough_build",
                MaterialEditorBuildFocusedProofReadback(&proof));
    assert_true("material_editor_mirror_proof_rough_glossy",
                strcmp(proof.mirror_proof_case, "rough mirror") == 0 &&
                    strcmp(proof.mirror_proof_package,
                           "disney_v2_mirror_glossy_preservation_matrix") == 0 &&
                    strstr(proof.mirror_proof_coverage, "rough reflection") != NULL &&
                    strstr(proof.mirror_missing_proof, "rough mirror visual") != NULL);

    ObjectEditorObjectAssignMaterial(&sceneSettings.sceneObjects[0], MATERIAL_PRESET_MIRROR);
    sceneSettings.sceneObjects[0].color = SceneObjectPackRGBBytes(255u, 255u, 255u);
    animSettings.lightIntensity = 80.0;
    assert_true("material_editor_mirror_proof_illuminated_build",
                MaterialEditorBuildFocusedProofReadback(&proof));
    assert_true("material_editor_mirror_proof_illuminated_m10_s4",
                strcmp(proof.mirror_proof_case, "illuminated mirror dominance") == 0 &&
                    strcmp(proof.mirror_proof_package,
                           "m10_s4_illuminated_mirror_dominance_regression") == 0 &&
                    strstr(proof.mirror_proof_coverage, "bright direct light") != NULL &&
                    strstr(proof.mirror_missing_proof, "packaged visual matrix") != NULL);

    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_ROUGH_METAL;
    assert_true("material_editor_non_mirror_proof_has_no_mirror_mapping",
                MaterialEditorBuildFocusedProofReadback(&proof) &&
                    !proof.mirror_proof_readback);

    SceneEditorMaterialGraphResetAll();
    SceneEditorMaterialStackResetAll();
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

static int test_scene_editor_scene_view_packet_exports_focused_object(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_view_packet_fixture\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"packet_prism\","
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
    SceneEditorSceneViewPacket packet = {0};
    SceneEditorSceneViewPacketReadback readback = {0};
    char packet_json[65536];

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = 1;

    assert_true("scene_view_packet_runtime_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json, &summary));
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[0].alpha = 0.40;
    sceneSettings.sceneObjects[0].opacity = 1.0;
    sceneSettings.sceneObjects[0].color = SceneObjectPackRGBBytes(32, 144, 220);
    runtime_scene_bridge_get_last_3d_digest_state(&digest);
    nav_state.overlay_zoom = 0.08;
    assert_true("scene_view_packet_projector",
                SceneEditorDigestOverlayBuildObjectProjector(&digest,
                                                             &viewport,
                                                             &nav_state,
                                                             0,
                                                             true,
                                                             &projector));
    assert_true("scene_view_packet_build",
                SceneEditorSceneViewPacketBuildFocusedObject(
                    0,
                    SCENE_EDITOR_SCENE_VIEW_PREVIEW_MATERIAL,
                    &projector,
                    &packet));
    assert_true("scene_view_packet_triangle_count", packet.triangleCount == 12);
    assert_true("scene_view_packet_face_group_count", packet.faceGroupCount == 6);
    assert_true("scene_view_packet_projected", packet.projected);
    assert_true("scene_view_packet_complete", packet.complete);
    assert_true("scene_view_packet_not_degraded",
                packet.degradedReason == SCENE_EDITOR_SCENE_VIEW_DEGRADED_NONE);
    assert_true("scene_view_packet_pick_object",
                packet.triangles[0].pickId.sceneObjectIndex == 0);
    assert_true("scene_view_packet_first_face_group",
                packet.triangles[0].pickId.faceGroupIndex == 0);
    assert_true("scene_view_packet_last_face_group",
                packet.triangles[11].pickId.faceGroupIndex == 5);
    assert_true("scene_view_packet_alpha_hint",
                packet.triangles[0].rgba[3] < 255);
    assert_true("scene_view_packet_transparent_flag",
                (packet.triangles[0].displayFlags &
                 SCENE_EDITOR_SCENE_VIEW_DISPLAY_TRANSPARENT) != 0u);
    assert_true("scene_view_packet_screen_projection",
                packet.triangles[0].screen0[0] != 0 || packet.triangles[0].screen0[1] != 0);
    memset(packet_json, 0, sizeof(packet_json));
    assert_true("scene_view_packet_json_export",
                SceneEditorSceneViewPacketToJsonString(&packet,
                                                       packet_json,
                                                       sizeof(packet_json)));
    assert_true("scene_view_packet_json_has_schema",
                strstr(packet_json, SCENE_EDITOR_SCENE_VIEW_PACKET_SCHEMA_VARIANT) != NULL);
    assert_true("scene_view_packet_json_has_material_quality",
                strstr(packet_json, "\"preview_quality\":\"material_preview\"") != NULL);
    assert_true("scene_view_packet_json_readback",
                SceneEditorSceneViewPacketReadbackFromJsonString(packet_json, &readback));
    assert_true("scene_view_packet_readback_valid", readback.valid);
    assert_true("scene_view_packet_readback_counts",
                readback.triangleCount == packet.triangleCount &&
                    readback.faceGroupCount == packet.faceGroupCount);
    assert_true("scene_view_packet_readback_pick_span",
                readback.firstPickId.faceGroupIndex == 0 &&
                    readback.lastPickId.faceGroupIndex == 5);
    assert_true("scene_view_packet_readback_alpha",
                readback.firstAlpha == packet.triangles[0].rgba[3]);
    assert_true("scene_view_packet_readback_flags",
                (readback.firstDisplayFlags &
                 SCENE_EDITOR_SCENE_VIEW_DISPLAY_TRANSPARENT) != 0u);

    memset(&packet, 0, sizeof(packet));
    assert_true("scene_view_packet_missing_object_rejected",
                !SceneEditorSceneViewPacketBuildFocusedObject(
                    9,
                    SCENE_EDITOR_SCENE_VIEW_PREVIEW_MATERIAL,
                    &projector,
                    &packet));
    assert_true("scene_view_packet_missing_object_degraded",
                packet.degradedReason == SCENE_EDITOR_SCENE_VIEW_DEGRADED_OBJECT_NOT_FOUND);

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

static int test_material_editor_face_indices_are_local_to_primitive(void) {
    RuntimeSceneBridge3DPrimitiveSeedState seeds = {0};
    RuntimeScene3D scene;

    memset(&scene, 0, sizeof(scene));
    seeds.valid = true;
    seeds.primitive_count = 2;
    seeds.plane_primitive_count = 2;
    for (int i = 0; i < 2; ++i) {
        RuntimeSceneBridgePrimitiveSeed* seed = &seeds.primitives[i];
        seed->kind = RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE;
        seed->scene_object_index = 7;
        seed->axis_u_x = 1.0;
        seed->axis_v_z = 1.0;
        seed->normal_y = 1.0;
        seed->width = 2.0 + (double)i;
        seed->height = 1.0;
    }

    assert_true("material_face_indices_build_scene",
                RuntimeScene3DBuilder_BuildFromPrimitiveSeedState(&scene, &seeds));
    assert_true("material_face_indices_triangle_count", scene.triangleMesh.triangleCount == 4);
    assert_true("material_face_indices_first_primitive_tri0",
                scene.triangleMesh.triangles[0].primitiveIndex == 0 &&
                scene.triangleMesh.triangles[0].localTriangleIndex == 0);
    assert_true("material_face_indices_first_primitive_tri1",
                scene.triangleMesh.triangles[1].primitiveIndex == 0 &&
                scene.triangleMesh.triangles[1].localTriangleIndex == 1);
    assert_true("material_face_indices_second_primitive_resets_tri0",
                scene.triangleMesh.triangles[2].primitiveIndex == 1 &&
                scene.triangleMesh.triangles[2].localTriangleIndex == 0);
    assert_true("material_face_indices_second_primitive_resets_tri1",
                scene.triangleMesh.triangles[3].primitiveIndex == 1 &&
                scene.triangleMesh.triangles[3].localTriangleIndex == 1);
    RuntimeScene3D_Free(&scene);
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
          "},"
          "{"
            "\"object_id\":\"skinny_prism\","
            "\"object_type\":\"rect_prism_primitive\","
            "\"transform\":{\"position\":{\"x\":9.0,\"y\":0.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"primitive\":{\"kind\":\"rect_prism_primitive\","
              "\"width\":8.0,\"height\":6.0,\"depth\":0.5,"
              "\"frame\":{\"origin\":{\"x\":9.0,\"y\":0.0,\"z\":0.0},"
                "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
                "\"axis_v\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
                "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}}"
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
    int skinny_width = 0;
    int skinny_height = 0;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = 1;
    sceneSettings.objectCount = 3;

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
    assert_true("material_face_preview_display_skinny_side",
                MaterialEditorFacePreviewResolveDisplaySize(&sceneSettings.sceneObjects[2],
                                                           2,
                                                           2,
                                                           240,
                                                           &skinny_width,
                                                           &skinny_height));
    assert_true("material_face_preview_display_wide_landscape", wide_width > wide_height);
    assert_true("material_face_preview_display_tall_portrait", tall_height > tall_width);
    assert_true("material_face_preview_display_neutral_square",
                neutral_width == neutral_height);
    assert_true("material_face_preview_display_skinny_keeps_extreme_aspect",
                skinny_width > skinny_height * 8);
    assert_true("material_face_preview_display_skinny_short_not_square_padded",
                skinny_height < 40);
    assert_true("material_face_preview_display_clamped_max",
                wide_width <= 220 && wide_height <= 220 &&
                tall_width <= 220 && tall_height <= 220 &&
                skinny_width <= 220 && skinny_height <= 220);
    MaterialEditorFacePreviewReset();
    assert_true("material_face_preview_alpha_default_off",
                !MaterialEditorFacePreviewGetUseTransparency());
    MaterialEditorFacePreviewSetUseTransparency(true);
    assert_true("material_face_preview_alpha_toggle_on",
                MaterialEditorFacePreviewGetUseTransparency());
    MaterialEditorFacePreviewSetUseTransparency(false);
    assert_true("material_face_preview_alpha_toggle_off",
                !MaterialEditorFacePreviewGetUseTransparency());

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
    SceneEditorMaterialPreviewTriangleAddress active_address = {0};
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
    assert_true("material_selection_active_address_readback",
                MaterialEditorGetActiveFaceAddress(&active_address));
    assert_true("material_selection_active_address_matches_primitive",
                active_address.primitiveIndex == picked.primitiveIndex &&
                    active_address.faceGroupIndex == picked.faceGroupIndex);

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
    MaterialEditorProofReadback proof_readback;
    MaterialEditorFaceRegionReadback region_readback;
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
    assert_true("material_face_override_destination_label",
                MaterialEditorMutationDestinationForFocusedTextureControls() ==
                    MATERIAL_EDITOR_MUTATION_DESTINATION_FACE_OVERRIDE &&
                    strcmp(MaterialEditorMutationDestinationLabel(
                               MATERIAL_EDITOR_MUTATION_DESTINATION_FACE_OVERRIDE),
                           "face_override") == 0);
    assert_true("material_face_override_panel_group_label",
                MaterialEditorPanelGroupForMutationDestination(
                    MATERIAL_EDITOR_MUTATION_DESTINATION_FACE_OVERRIDE) ==
                    MATERIAL_EDITOR_PANEL_GROUP_FACE_OVERRIDE &&
                    strcmp(MaterialEditorPanelGroupLabel(
                               MATERIAL_EDITOR_PANEL_GROUP_FACE_OVERRIDE),
                           "Face Override") == 0);
    assert_true("material_face_override_proof_readback",
                MaterialEditorBuildFocusedProofReadback(&proof_readback) &&
                    strcmp(proof_readback.destination_label, "face_override") == 0 &&
                    strcmp(proof_readback.panel_group_label, "Face Override") == 0 &&
                    proof_readback.m4_request_compatible &&
                    proof_readback.launch_deferred);
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
    assert_true("material_face_region_readback_builds",
                MaterialEditorBuildFaceRegionReadback(&region_readback));
    assert_true("material_face_region_readback_active",
                region_readback.active_face_group_index == 2 &&
                    strstr(region_readback.active_label, "Face #2") != NULL);
    assert_true("material_face_region_readback_selected_count",
                region_readback.selected_face_group_count == 2 &&
                    strstr(region_readback.selection_label, "Selected groups 2") != NULL);
    assert_true("material_face_region_readback_reset_copy",
                region_readback.can_reset && region_readback.can_copy_to_selected);
    assert_true("material_face_region_readback_legacy_override",
                region_readback.has_legacy_override &&
                    strstr(region_readback.override_label, "object-face") != NULL);
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
    assert_true("material_face_stack_default_controls_read_active_layer",
                material_editor_texture_kind_for_controls(&sceneSettings.sceneObjects[0]) ==
                    RUNTIME_MATERIAL_TEXTURE_3D_RUST);
    assert_close("material_face_stack_default_controls_read_strength",
                 material_editor_value_for_slider(&sceneSettings.sceneObjects[0],
                                                  MATERIAL_EDITOR_SLIDER_STRENGTH),
                 1.0,
                 1e-12);
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
    {
        MaterialEditorFaceRegionReadback region_readback = {0};
        assert_true("material_face_stack_region_readback_builds",
                    MaterialEditorBuildFaceRegionReadback(&region_readback));
        assert_true("material_face_stack_region_readback_layer",
                    region_readback.has_active_layer &&
                        strstr(region_readback.layer_label, "Rust") != NULL);
        assert_true("material_face_stack_region_readback_override",
                    region_readback.can_reset &&
                        region_readback.has_layer_specific_override &&
                        strstr(region_readback.override_label, "layer-specific") != NULL);
        assert_true("material_face_stack_region_readback_no_copy",
                    !region_readback.can_copy_to_selected);
    }
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
    test_scene_editor_runtime_scene_persist_keeps_preview_limited_mesh_reload();
    test_scene_editor_runtime_scene_persistence_roundtrip_object_materials();
    test_scene_editor_runtime_scene_material_stack_roundtrip_payload();
    test_object_editor_material_assignment_preserves_object_color();
    test_object_editor_slider_assignments_update_object_fields();
    test_material_editor_focuses_last_selected_and_updates_texture_fields();
    test_material_editor_graph_readback_reports_mvp_integration();
    test_material_editor_graph_actions_create_layer_channel_and_clear();
    test_material_editor_material_readback_reports_preset_custom_and_graph();
    test_material_editor_recipe_readback_and_cycles_family_surface_finish();
    test_material_editor_recipe_dropdown_options_apply_exact_choices();
    test_material_editor_response_readback_uses_family_matrix_for_glass();
    test_material_editor_glass_response_mutation_routes_to_composite_helpers();
    test_material_editor_glass_transport_override_is_object_local_and_loaded();
    test_material_editor_mirror_response_readback_gets_family_matrix();
    test_material_editor_mirror_response_override_is_object_local_and_loaded();
    test_material_editor_metal_response_routes_stack_preview_payload_and_proof();
    test_material_editor_response_mutation_rejects_non_glass_family();
    test_material_editor_glass_overlay_affordances_add_select_and_readback();
    test_material_editor_glass_overlay_affordances_reject_non_glass();
    test_material_editor_response_readback_keeps_non_glass_on_generic_matrix();
    test_material_editor_texture_channel_readback_groups_ownership();
    test_material_editor_glass_channel_mapping_labels_authored_and_overlay_intent();
    test_material_editor_glass_proof_readback_maps_current_state_to_m4_coverage();
    test_material_editor_mirror_proof_readback_maps_current_state_to_coverage();
    test_material_editor_object_scope_clears_face_overrides_and_applies_all_faces();
    test_material_editor_layer_list_routes_object_stack_controls();
    test_material_editor_object_projector_centers_focused_object();
    test_material_editor_focused_zoom_accumulates_around_object_fit();
    test_material_editor_preview_resolves_focused_triangle_substrate();
    test_scene_editor_scene_view_packet_exports_focused_object();
    test_material_editor_face_metrics_ground_uv_scales_with_dimensions();
    test_material_editor_face_indices_are_local_to_primitive();
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
    test_material_editor_authored_texture_local_absolute_reference_policy();
    test_scene_editor_runtime_scene_authored_texture_overlay_roundtrip_payload();
    return test_support_failures() - before;
}
