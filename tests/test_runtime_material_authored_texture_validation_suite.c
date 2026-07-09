#include <png.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <json-c/json.h>

#include "app/animation.h"
#include "render/runtime_material_authored_texture_3d.h"
#include "test_runtime_lighting_materials_internal.h"
#include "test_support.h"

static bool authored_texture_validation_write_png_rgba(const char* path) {
    static const unsigned char rgba[] = {128u, 128u, 128u, 255u};
    FILE* png_file = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    bool ok = false;
    if (!path) return false;
    png_file = fopen(path, "wb");
    if (!png_file) return false;
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
                 1u,
                 1u,
                 8,
                 PNG_COLOR_TYPE_RGBA,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);
    png_write_row(png_ptr, rgba);
    png_write_end(png_ptr, info_ptr);
    ok = true;
cleanup:
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(png_file);
    return ok;
}

static json_object* authored_texture_validation_new_surface(const char* face_role,
                                                            const char* file_name) {
    json_object* surface = json_object_new_object();
    if (!surface || !face_role || !file_name) {
        if (surface) json_object_put(surface);
        return NULL;
    }
    json_object_object_add(surface, "face_role", json_object_new_string(face_role));
    json_object_object_add(surface, "file_name", json_object_new_string(file_name));
    return surface;
}

static bool authored_texture_validation_add_semantic_net_fields(
    json_object* surface,
    const char* net_layout_kind,
    const char* net_slot,
    const char* orientation,
    const int* corner_ids,
    const int* edge_ids,
    const char* const* adjacent_face_roles) {
    json_object* corner_array = json_object_new_array();
    json_object* edge_array = json_object_new_array();
    json_object* adjacent_array = json_object_new_array();
    int i = 0;
    if (!surface || !net_layout_kind || !net_slot || !orientation || !corner_ids || !edge_ids ||
        !adjacent_face_roles || !corner_array || !edge_array || !adjacent_array) {
        if (corner_array) json_object_put(corner_array);
        if (edge_array) json_object_put(edge_array);
        if (adjacent_array) json_object_put(adjacent_array);
        return false;
    }
    json_object_object_add(surface, "net_layout_kind", json_object_new_string(net_layout_kind));
    json_object_object_add(surface, "net_slot", json_object_new_string(net_slot));
    json_object_object_add(surface, "orientation", json_object_new_string(orientation));
    for (i = 0; i < 4; ++i) {
        json_object_array_add(corner_array, json_object_new_int(corner_ids[i]));
        json_object_array_add(edge_array, json_object_new_int(edge_ids[i]));
        json_object_array_add(adjacent_array, json_object_new_string(adjacent_face_roles[i]));
    }
    json_object_object_add(surface, "corner_ids", corner_array);
    json_object_object_add(surface, "edge_ids", edge_array);
    json_object_object_add(surface, "adjacent_face_roles", adjacent_array);
    return true;
}

static bool authored_texture_validation_add_default_plane_semantic_fields(json_object* surface) {
    static const int corner_ids[4] = {255, 255, 255, 255};
    static const int edge_ids[4] = {255, 255, 255, 255};
    static const char* adjacent_face_roles[4] = {"NONE", "NONE", "NONE", "NONE"};
    return authored_texture_validation_add_semantic_net_fields(
        surface, "PLANE", "FRONT", "R0", corner_ids, edge_ids, adjacent_face_roles);
}

static bool authored_texture_validation_write_manifest(json_object* root,
                                                       const char* manifest_path) {
    int write_ok = 0;
    if (!root || !manifest_path) return false;
    write_ok = json_object_to_file_ext(manifest_path, root, JSON_C_TO_STRING_PRETTY);
    json_object_put(root);
    return write_ok == 0;
}

static bool authored_texture_validation_binding_is_clear(int scene_object_index) {
    char manifest_path[RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY];
    char binding_mode[RUNTIME_MATERIAL_AUTHORED_TEXTURE_MODE_CAPACITY];
    int face_count = 0;
    return !RuntimeMaterialAuthoredTextureGetBinding(scene_object_index,
                                                     manifest_path,
                                                     sizeof(manifest_path),
                                                     binding_mode,
                                                     sizeof(binding_mode),
                                                     &face_count);
}

static int test_authored_texture_validation_rejects_unsupported_schema(void) {
    AnimationConfig saved_anim = animSettings;
    const char* dir = "/tmp/rt_authored_texture_validation_schema";
    const char* png_path = "/tmp/rt_authored_texture_validation_schema/front.png";
    const char* manifest_path = "/tmp/rt_authored_texture_validation_schema/manifest.json";
    const char* manifest_rel = "manifest.json";
    json_object* root = NULL;
    json_object* surfaces = NULL;
    bool ok = false;

    memset(&animSettings, 0, sizeof(animSettings));
    RuntimeMaterialAuthoredTextureResetAll();
    (void)mkdir(dir, 0775);
    assert_true("authored_texture_validation_schema_png_write",
                authored_texture_validation_write_png_rgba(png_path));

    root = json_object_new_object();
    surfaces = json_object_new_array();
    assert_true("authored_texture_validation_schema_root_ok", root && surfaces);
    json_object_object_add(root, "schema_version", json_object_new_int(99));
    json_object_object_add(root, "export_binding_kind", json_object_new_string("SEPARATE_FACES"));
    json_object_object_add(root, "primitive_kind", json_object_new_string("PLANE"));
    json_object_object_add(root, "source_object_id", json_object_new_string("plane_a"));
    json_object_array_add(surfaces, authored_texture_validation_new_surface("FRONT", "front.png"));
    json_object_object_add(root, "surfaces", surfaces);
    assert_true("authored_texture_validation_schema_manifest_write",
                authored_texture_validation_write_manifest(root, manifest_path));

    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath), "%s/scene.json", dir);
    ok = RuntimeMaterialAuthoredTextureBindManifestForObject(0, "plane_a", manifest_rel, "override");
    assert_true("authored_texture_validation_schema_bind_rejected", !ok);
    assert_true("authored_texture_validation_schema_binding_clear",
                authored_texture_validation_binding_is_clear(0));

    RuntimeMaterialAuthoredTextureResetAll();
    unlink(manifest_path);
    unlink(png_path);
    rmdir(dir);
    animSettings = saved_anim;
    return 0;
}

static int test_authored_texture_validation_rejects_missing_emitted_output_kind(void) {
    AnimationConfig saved_anim = animSettings;
    const char* dir = "/tmp/rt_authored_texture_validation_missing_output";
    const char* png_path = "/tmp/rt_authored_texture_validation_missing_output/front.png";
    const char* manifest_path = "/tmp/rt_authored_texture_validation_missing_output/manifest.json";
    const char* manifest_rel = "manifest.json";
    json_object* root = NULL;
    json_object* base_surfaces = NULL;
    bool ok = false;

    memset(&animSettings, 0, sizeof(animSettings));
    RuntimeMaterialAuthoredTextureResetAll();
    (void)mkdir(dir, 0775);
    assert_true("authored_texture_validation_missing_output_png_write",
                authored_texture_validation_write_png_rgba(png_path));

    root = json_object_new_object();
    base_surfaces = json_object_new_array();
    assert_true("authored_texture_validation_missing_output_root_ok", root && base_surfaces);
    json_object_object_add(root, "schema_version", json_object_new_int(5));
    json_object_object_add(root, "export_binding_kind", json_object_new_string("SEPARATE_FACES"));
    json_object_object_add(root, "primitive_kind", json_object_new_string("PLANE"));
    json_object_object_add(root, "source_object_id", json_object_new_string("plane_a"));
    json_object_array_add(base_surfaces,
                          authored_texture_validation_new_surface("FRONT", "front.png"));
    json_object_object_add(root, "base_surfaces", base_surfaces);
    assert_true("authored_texture_validation_missing_output_manifest_write",
                authored_texture_validation_write_manifest(root, manifest_path));

    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath), "%s/scene.json", dir);
    ok = RuntimeMaterialAuthoredTextureBindManifestForObject(0, "plane_a", manifest_rel, "override");
    assert_true("authored_texture_validation_missing_output_bind_rejected", !ok);
    assert_true("authored_texture_validation_missing_output_binding_clear",
                authored_texture_validation_binding_is_clear(0));

    RuntimeMaterialAuthoredTextureResetAll();
    unlink(manifest_path);
    unlink(png_path);
    rmdir(dir);
    animSettings = saved_anim;
    return 0;
}

static int test_authored_texture_validation_rejects_invalid_binding_kind(void) {
    AnimationConfig saved_anim = animSettings;
    const char* dir = "/tmp/rt_authored_texture_validation_binding_kind";
    const char* png_path = "/tmp/rt_authored_texture_validation_binding_kind/front.png";
    const char* manifest_path = "/tmp/rt_authored_texture_validation_binding_kind/manifest.json";
    const char* manifest_rel = "manifest.json";
    json_object* root = NULL;
    json_object* base_surfaces = NULL;
    bool ok = false;

    memset(&animSettings, 0, sizeof(animSettings));
    RuntimeMaterialAuthoredTextureResetAll();
    (void)mkdir(dir, 0775);
    assert_true("authored_texture_validation_binding_kind_png_write",
                authored_texture_validation_write_png_rgba(png_path));

    root = json_object_new_object();
    base_surfaces = json_object_new_array();
    assert_true("authored_texture_validation_binding_kind_root_ok", root && base_surfaces);
    json_object_object_add(root, "schema_version", json_object_new_int(5));
    json_object_object_add(root, "export_binding_kind", json_object_new_string("ATLAS"));
    json_object_object_add(root, "emitted_output_kind", json_object_new_string("FLATTENED_ONLY"));
    json_object_object_add(root, "primitive_kind", json_object_new_string("PLANE"));
    json_object_object_add(root, "source_object_id", json_object_new_string("plane_a"));
    json_object_array_add(base_surfaces,
                          authored_texture_validation_new_surface("FRONT", "front.png"));
    json_object_object_add(root, "base_surfaces", base_surfaces);
    assert_true("authored_texture_validation_binding_kind_manifest_write",
                authored_texture_validation_write_manifest(root, manifest_path));

    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath), "%s/scene.json", dir);
    ok = RuntimeMaterialAuthoredTextureBindManifestForObject(0, "plane_a", manifest_rel, "override");
    assert_true("authored_texture_validation_binding_kind_bind_rejected", !ok);
    assert_true("authored_texture_validation_binding_kind_binding_clear",
                authored_texture_validation_binding_is_clear(0));

    RuntimeMaterialAuthoredTextureResetAll();
    unlink(manifest_path);
    unlink(png_path);
    rmdir(dir);
    animSettings = saved_anim;
    return 0;
}

static int test_authored_texture_validation_rejects_incomplete_rect_prism_base_lane(void) {
    AnimationConfig saved_anim = animSettings;
    const char* dir = "/tmp/rt_authored_texture_validation_rect_prism_base";
    const char* manifest_path = "/tmp/rt_authored_texture_validation_rect_prism_base/manifest.json";
    const char* manifest_rel = "manifest.json";
    const char* roles[5] = {"FRONT", "BACK", "LEFT", "RIGHT", "TOP"};
    json_object* root = NULL;
    json_object* base_surfaces = NULL;
    bool ok = false;

    memset(&animSettings, 0, sizeof(animSettings));
    RuntimeMaterialAuthoredTextureResetAll();
    (void)mkdir(dir, 0775);
    for (int i = 0; i < 5; ++i) {
        char png_path[256];
        snprintf(png_path, sizeof(png_path), "%s/%s.png", dir, roles[i]);
        assert_true("authored_texture_validation_rect_prism_png_write",
                    authored_texture_validation_write_png_rgba(png_path));
    }

    root = json_object_new_object();
    base_surfaces = json_object_new_array();
    assert_true("authored_texture_validation_rect_prism_root_ok", root && base_surfaces);
    json_object_object_add(root, "schema_version", json_object_new_int(5));
    json_object_object_add(root, "export_binding_kind", json_object_new_string("SEPARATE_FACES"));
    json_object_object_add(root, "emitted_output_kind", json_object_new_string("FLATTENED_ONLY"));
    json_object_object_add(root, "primitive_kind", json_object_new_string("RECT_PRISM"));
    json_object_object_add(root, "source_object_id", json_object_new_string("box_a"));
    for (int i = 0; i < 5; ++i) {
        char file_name[64];
        snprintf(file_name, sizeof(file_name), "%s.png", roles[i]);
        json_object_array_add(base_surfaces,
                              authored_texture_validation_new_surface(roles[i], file_name));
    }
    json_object_object_add(root, "base_surfaces", base_surfaces);
    assert_true("authored_texture_validation_rect_prism_manifest_write",
                authored_texture_validation_write_manifest(root, manifest_path));

    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath), "%s/scene.json", dir);
    ok = RuntimeMaterialAuthoredTextureBindManifestForObject(0, "box_a", manifest_rel, "override");
    assert_true("authored_texture_validation_rect_prism_bind_rejected", !ok);
    assert_true("authored_texture_validation_rect_prism_binding_clear",
                authored_texture_validation_binding_is_clear(0));

    RuntimeMaterialAuthoredTextureResetAll();
    unlink(manifest_path);
    for (int i = 0; i < 5; ++i) {
        char png_path[256];
        snprintf(png_path, sizeof(png_path), "%s/%s.png", dir, roles[i]);
        unlink(png_path);
    }
    rmdir(dir);
    animSettings = saved_anim;
    return 0;
}

static int test_authored_texture_validation_rejects_incomplete_overlay_lane(void) {
    AnimationConfig saved_anim = animSettings;
    const char* dir = "/tmp/rt_authored_texture_validation_overlay";
    const char* base_png_path = "/tmp/rt_authored_texture_validation_overlay/front_base.png";
    const char* overlay_png_path = "/tmp/rt_authored_texture_validation_overlay/front_overlay.png";
    const char* manifest_path = "/tmp/rt_authored_texture_validation_overlay/manifest.json";
    const char* manifest_rel = "manifest.json";
    json_object* root = NULL;
    json_object* base_surfaces = NULL;
    json_object* overlay_surfaces = NULL;
    bool ok = false;

    memset(&animSettings, 0, sizeof(animSettings));
    RuntimeMaterialAuthoredTextureResetAll();
    (void)mkdir(dir, 0775);
    assert_true("authored_texture_validation_overlay_base_png_write",
                authored_texture_validation_write_png_rgba(base_png_path));
    assert_true("authored_texture_validation_overlay_overlay_png_write",
                authored_texture_validation_write_png_rgba(overlay_png_path));

    root = json_object_new_object();
    base_surfaces = json_object_new_array();
    overlay_surfaces = json_object_new_array();
    assert_true("authored_texture_validation_overlay_root_ok",
                root && base_surfaces && overlay_surfaces);
    json_object_object_add(root, "schema_version", json_object_new_int(5));
    json_object_object_add(root, "export_binding_kind", json_object_new_string("SEPARATE_FACES"));
    json_object_object_add(root, "emitted_output_kind", json_object_new_string("BASE_PLUS_OVERLAY"));
    json_object_object_add(root, "primitive_kind", json_object_new_string("PLANE"));
    json_object_object_add(root, "source_object_id", json_object_new_string("plane_a"));
    json_object_array_add(base_surfaces,
                          authored_texture_validation_new_surface("FRONT", "front_base.png"));
    /* overlay_surfaces intentionally left empty */
    json_object_object_add(root, "base_surfaces", base_surfaces);
    json_object_object_add(root, "overlay_surfaces", overlay_surfaces);
    assert_true("authored_texture_validation_overlay_manifest_write",
                authored_texture_validation_write_manifest(root, manifest_path));

    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath), "%s/scene.json", dir);
    ok = RuntimeMaterialAuthoredTextureBindManifestForObject(0, "plane_a", manifest_rel, "override");
    assert_true("authored_texture_validation_overlay_bind_rejected", !ok);
    assert_true("authored_texture_validation_overlay_binding_clear",
                authored_texture_validation_binding_is_clear(0));

    RuntimeMaterialAuthoredTextureResetAll();
    unlink(manifest_path);
    unlink(base_png_path);
    unlink(overlay_png_path);
    rmdir(dir);
    animSettings = saved_anim;
    return 0;
}

static int test_authored_texture_validation_rejects_mixed_v5_legacy_and_base_lanes(void) {
    AnimationConfig saved_anim = animSettings;
    const char* dir = "/tmp/rt_authored_texture_validation_mixed_v5";
    const char* png_path = "/tmp/rt_authored_texture_validation_mixed_v5/front.png";
    const char* manifest_path = "/tmp/rt_authored_texture_validation_mixed_v5/manifest.json";
    const char* manifest_rel = "manifest.json";
    json_object* root = NULL;
    json_object* surfaces = NULL;
    json_object* base_surfaces = NULL;
    bool ok = false;

    memset(&animSettings, 0, sizeof(animSettings));
    RuntimeMaterialAuthoredTextureResetAll();
    (void)mkdir(dir, 0775);
    assert_true("authored_texture_validation_mixed_v5_png_write",
                authored_texture_validation_write_png_rgba(png_path));

    root = json_object_new_object();
    surfaces = json_object_new_array();
    base_surfaces = json_object_new_array();
    assert_true("authored_texture_validation_mixed_v5_root_ok",
                root && surfaces && base_surfaces);
    json_object_object_add(root, "schema_version", json_object_new_int(5));
    json_object_object_add(root, "export_binding_kind", json_object_new_string("SEPARATE_FACES"));
    json_object_object_add(root, "emitted_output_kind", json_object_new_string("FLATTENED_ONLY"));
    json_object_object_add(root, "primitive_kind", json_object_new_string("PLANE"));
    json_object_object_add(root, "source_object_id", json_object_new_string("plane_a"));
    json_object_array_add(surfaces,
                          authored_texture_validation_new_surface("FRONT", "front.png"));
    json_object_array_add(base_surfaces,
                          authored_texture_validation_new_surface("FRONT", "front.png"));
    json_object_object_add(root, "surfaces", surfaces);
    json_object_object_add(root, "base_surfaces", base_surfaces);
    assert_true("authored_texture_validation_mixed_v5_manifest_write",
                authored_texture_validation_write_manifest(root, manifest_path));

    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath), "%s/scene.json", dir);
    ok = RuntimeMaterialAuthoredTextureBindManifestForObject(0, "plane_a", manifest_rel, "override");
    assert_true("authored_texture_validation_mixed_v5_bind_rejected", !ok);
    assert_true("authored_texture_validation_mixed_v5_binding_clear",
                authored_texture_validation_binding_is_clear(0));

    RuntimeMaterialAuthoredTextureResetAll();
    unlink(manifest_path);
    unlink(png_path);
    rmdir(dir);
    animSettings = saved_anim;
    return 0;
}

static int test_authored_texture_validation_rejects_invalid_semantic_orientation(void) {
    AnimationConfig saved_anim = animSettings;
    const char* dir = "/tmp/rt_authored_texture_validation_bad_orientation";
    const char* png_path = "/tmp/rt_authored_texture_validation_bad_orientation/front.png";
    const char* manifest_path = "/tmp/rt_authored_texture_validation_bad_orientation/manifest.json";
    const char* manifest_rel = "manifest.json";
    json_object* root = NULL;
    json_object* base_surfaces = NULL;
    json_object* surface = NULL;
    bool ok = false;

    memset(&animSettings, 0, sizeof(animSettings));
    RuntimeMaterialAuthoredTextureResetAll();
    (void)mkdir(dir, 0775);
    assert_true("authored_texture_validation_bad_orientation_png_write",
                authored_texture_validation_write_png_rgba(png_path));

    root = json_object_new_object();
    base_surfaces = json_object_new_array();
    surface = authored_texture_validation_new_surface("FRONT", "front.png");
    assert_true("authored_texture_validation_bad_orientation_root_ok",
                root && base_surfaces && surface);
    assert_true("authored_texture_validation_bad_orientation_semantics",
                authored_texture_validation_add_default_plane_semantic_fields(surface));
    json_object_object_del(surface, "orientation");
    json_object_object_add(surface, "orientation", json_object_new_string("R45"));
    json_object_object_add(root, "schema_version", json_object_new_int(5));
    json_object_object_add(root, "export_binding_kind", json_object_new_string("SEPARATE_FACES"));
    json_object_object_add(root, "emitted_output_kind", json_object_new_string("FLATTENED_ONLY"));
    json_object_object_add(root, "primitive_kind", json_object_new_string("PLANE"));
    json_object_object_add(root, "source_object_id", json_object_new_string("plane_a"));
    json_object_array_add(base_surfaces, surface);
    json_object_object_add(root, "base_surfaces", base_surfaces);
    assert_true("authored_texture_validation_bad_orientation_manifest_write",
                authored_texture_validation_write_manifest(root, manifest_path));

    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath), "%s/scene.json", dir);
    ok = RuntimeMaterialAuthoredTextureBindManifestForObject(0, "plane_a", manifest_rel, "override");
    assert_true("authored_texture_validation_bad_orientation_bind_rejected", !ok);
    assert_true("authored_texture_validation_bad_orientation_binding_clear",
                authored_texture_validation_binding_is_clear(0));

    RuntimeMaterialAuthoredTextureResetAll();
    unlink(manifest_path);
    unlink(png_path);
    rmdir(dir);
    animSettings = saved_anim;
    return 0;
}

static int test_authored_texture_validation_rejects_invalid_plane_corner_ids(void) {
    AnimationConfig saved_anim = animSettings;
    const char* dir = "/tmp/rt_authored_texture_validation_bad_corners";
    const char* png_path = "/tmp/rt_authored_texture_validation_bad_corners/front.png";
    const char* manifest_path = "/tmp/rt_authored_texture_validation_bad_corners/manifest.json";
    const char* manifest_rel = "manifest.json";
    json_object* root = NULL;
    json_object* base_surfaces = NULL;
    json_object* surface = NULL;
    static const int bad_corner_ids[4] = {0, 1, 2, 3};
    static const int edge_ids[4] = {255, 255, 255, 255};
    static const char* adjacent_face_roles[4] = {"NONE", "NONE", "NONE", "NONE"};
    bool ok = false;

    memset(&animSettings, 0, sizeof(animSettings));
    RuntimeMaterialAuthoredTextureResetAll();
    (void)mkdir(dir, 0775);
    assert_true("authored_texture_validation_bad_corners_png_write",
                authored_texture_validation_write_png_rgba(png_path));

    root = json_object_new_object();
    base_surfaces = json_object_new_array();
    surface = authored_texture_validation_new_surface("FRONT", "front.png");
    assert_true("authored_texture_validation_bad_corners_root_ok",
                root && base_surfaces && surface);
    assert_true("authored_texture_validation_bad_corners_semantics",
                authored_texture_validation_add_semantic_net_fields(
                    surface,
                    "PLANE",
                    "FRONT",
                    "R0",
                    bad_corner_ids,
                    edge_ids,
                    adjacent_face_roles));
    json_object_object_add(root, "schema_version", json_object_new_int(5));
    json_object_object_add(root, "export_binding_kind", json_object_new_string("SEPARATE_FACES"));
    json_object_object_add(root, "emitted_output_kind", json_object_new_string("FLATTENED_ONLY"));
    json_object_object_add(root, "primitive_kind", json_object_new_string("PLANE"));
    json_object_object_add(root, "source_object_id", json_object_new_string("plane_a"));
    json_object_array_add(base_surfaces, surface);
    json_object_object_add(root, "base_surfaces", base_surfaces);
    assert_true("authored_texture_validation_bad_corners_manifest_write",
                authored_texture_validation_write_manifest(root, manifest_path));

    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath), "%s/scene.json", dir);
    ok = RuntimeMaterialAuthoredTextureBindManifestForObject(0, "plane_a", manifest_rel, "override");
    assert_true("authored_texture_validation_bad_corners_bind_rejected", !ok);
    assert_true("authored_texture_validation_bad_corners_binding_clear",
                authored_texture_validation_binding_is_clear(0));

    RuntimeMaterialAuthoredTextureResetAll();
    unlink(manifest_path);
    unlink(png_path);
    rmdir(dir);
    animSettings = saved_anim;
    return 0;
}

static int test_authored_texture_validation_rejects_invalid_plane_adjacent_roles(void) {
    AnimationConfig saved_anim = animSettings;
    const char* dir = "/tmp/rt_authored_texture_validation_bad_adjacent";
    const char* png_path = "/tmp/rt_authored_texture_validation_bad_adjacent/front.png";
    const char* manifest_path = "/tmp/rt_authored_texture_validation_bad_adjacent/manifest.json";
    const char* manifest_rel = "manifest.json";
    json_object* root = NULL;
    json_object* base_surfaces = NULL;
    json_object* surface = NULL;
    static const int corner_ids[4] = {255, 255, 255, 255};
    static const int edge_ids[4] = {255, 255, 255, 255};
    static const char* bad_adjacent_face_roles[4] = {"LEFT", "NONE", "NONE", "NONE"};
    bool ok = false;

    memset(&animSettings, 0, sizeof(animSettings));
    RuntimeMaterialAuthoredTextureResetAll();
    (void)mkdir(dir, 0775);
    assert_true("authored_texture_validation_bad_adjacent_png_write",
                authored_texture_validation_write_png_rgba(png_path));

    root = json_object_new_object();
    base_surfaces = json_object_new_array();
    surface = authored_texture_validation_new_surface("FRONT", "front.png");
    assert_true("authored_texture_validation_bad_adjacent_root_ok",
                root && base_surfaces && surface);
    assert_true("authored_texture_validation_bad_adjacent_semantics",
                authored_texture_validation_add_semantic_net_fields(
                    surface,
                    "PLANE",
                    "FRONT",
                    "R0",
                    corner_ids,
                    edge_ids,
                    bad_adjacent_face_roles));
    json_object_object_add(root, "schema_version", json_object_new_int(5));
    json_object_object_add(root, "export_binding_kind", json_object_new_string("SEPARATE_FACES"));
    json_object_object_add(root, "emitted_output_kind", json_object_new_string("FLATTENED_ONLY"));
    json_object_object_add(root, "primitive_kind", json_object_new_string("PLANE"));
    json_object_object_add(root, "source_object_id", json_object_new_string("plane_a"));
    json_object_array_add(base_surfaces, surface);
    json_object_object_add(root, "base_surfaces", base_surfaces);
    assert_true("authored_texture_validation_bad_adjacent_manifest_write",
                authored_texture_validation_write_manifest(root, manifest_path));

    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath), "%s/scene.json", dir);
    ok = RuntimeMaterialAuthoredTextureBindManifestForObject(0, "plane_a", manifest_rel, "override");
    assert_true("authored_texture_validation_bad_adjacent_bind_rejected", !ok);
    assert_true("authored_texture_validation_bad_adjacent_binding_clear",
                authored_texture_validation_binding_is_clear(0));

    RuntimeMaterialAuthoredTextureResetAll();
    unlink(manifest_path);
    unlink(png_path);
    rmdir(dir);
    animSettings = saved_anim;
    return 0;
}

int run_test_runtime_material_authored_texture_validation_suite(void) {
    int before = test_support_failures();
    test_authored_texture_validation_rejects_unsupported_schema();
    test_authored_texture_validation_rejects_missing_emitted_output_kind();
    test_authored_texture_validation_rejects_invalid_binding_kind();
    test_authored_texture_validation_rejects_incomplete_rect_prism_base_lane();
    test_authored_texture_validation_rejects_incomplete_overlay_lane();
    test_authored_texture_validation_rejects_mixed_v5_legacy_and_base_lanes();
    test_authored_texture_validation_rejects_invalid_semantic_orientation();
    test_authored_texture_validation_rejects_invalid_plane_corner_ids();
    test_authored_texture_validation_rejects_invalid_plane_adjacent_roles();
    return test_support_failures() - before;
}
