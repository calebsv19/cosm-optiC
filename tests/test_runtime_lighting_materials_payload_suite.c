#include <math.h>
#include <png.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app/animation.h"
#include "config/config_manager.h"
#include "editor/scene_editor_material_face_placement.h"
#include "editor/scene_editor_material_stack.h"
#include "import/runtime_scene_bridge.h"
#include "material/material_manager.h"
#include "render/material_bsdf.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_disney_3d.h"
#include "render/runtime_diffuse_bounce_3d.h"
#include "render/runtime_direct_light_3d.h"
#include "render/runtime_emission_transparency_3d.h"
#include "render/runtime_material_authored_texture_3d.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_principled_bsdf_3d.h"
#include "render/runtime_material_response_3d.h"
#include "render/runtime_material_texture_3d.h"
#include "render/runtime_material_texture_stack_3d.h"
#include "render/runtime_water_material_3d.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_scene_3d_builder.h"
#include "test_runtime_lighting_materials.h"
#include "test_runtime_lighting_materials_internal.h"
#include "test_support.h"

#include <json-c/json.h>

static RuntimeMaterialTextureLayer runtime_material_test_make_strong_overlay(
    RuntimeMaterialTextureLayerKind kind,
    double scale);

static void runtime_material_payload_test_add_material_intent_summaries(
    json_object* surface,
    const char* const* layer_material_intent_stable_ids,
    size_t layer_material_intent_count,
    const char* explicit_base_summary,
    const char* explicit_overlay_summary) {
    const char* base_summary = explicit_base_summary;
    const char* overlay_summary = explicit_overlay_summary;
    if (!surface) {
        return;
    }
    if (!base_summary || !overlay_summary) {
        for (size_t i = 0u; i < layer_material_intent_count; ++i) {
            RuntimeMaterialTextureLayerKind kind = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE;
            const char* stable_id = NULL;
            if (!layer_material_intent_stable_ids || !layer_material_intent_stable_ids[i]) {
                continue;
            }
            stable_id = layer_material_intent_stable_ids[i];
            kind = RuntimeMaterialTextureLayerKindFromStableId(stable_id);
            if (!explicit_base_summary && RuntimeMaterialTextureLayerKindIsBase(kind)) {
                base_summary = stable_id;
            } else if (!explicit_overlay_summary &&
                       RuntimeMaterialTextureLayerKindIsOverlay(kind)) {
                overlay_summary = stable_id;
            }
        }
    }
    if (base_summary && base_summary[0] != '\0') {
        json_object_object_add(surface,
                               "base_material_intent_kind",
                               json_object_new_string(base_summary));
    }
    if (overlay_summary && overlay_summary[0] != '\0') {
        json_object_object_add(surface,
                               "overlay_material_intent_kind",
                               json_object_new_string(overlay_summary));
    }
}

static bool runtime_material_payload_test_add_surface_semantic_fields(
    json_object* surface,
    const char* primitive_kind,
    const char* face_role,
    const char* net_layout_kind,
    const char* net_slot,
    const char* orientation) {
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
    const char* resolved_layout = net_layout_kind;
    const char* resolved_slot = net_slot;
    const char* resolved_orientation = orientation;
    int corner_values[4] = {0, 1, 2, 3};
    int edge_values[4] = {0, 1, 2, 3};
    int i = 0;
    if (!surface || !primitive_kind || !face_role) {
        return false;
    }
    if (strcmp(primitive_kind, "PLANE") == 0) {
        resolved_layout = resolved_layout ? resolved_layout : "PLANE";
        resolved_slot = resolved_slot ? resolved_slot : "FRONT";
        resolved_orientation = resolved_orientation ? resolved_orientation : "R0";
        for (i = 0; i < 4; ++i) {
            corner_values[i] = 255;
            edge_values[i] = 255;
        }
        adjacent_roles = kPlaneAdjacentRoles;
    } else {
        resolved_layout = resolved_layout ? resolved_layout : "PRISM_CROSS";
        resolved_slot = resolved_slot ? resolved_slot : face_role;
        resolved_orientation = resolved_orientation ? resolved_orientation : "R0";
        if (strcmp(face_role, "FRONT") == 0) adjacent_roles = kFrontAdjacentRoles;
        else if (strcmp(face_role, "BACK") == 0) adjacent_roles = kBackAdjacentRoles;
        else if (strcmp(face_role, "LEFT") == 0) adjacent_roles = kLeftAdjacentRoles;
        else if (strcmp(face_role, "RIGHT") == 0) adjacent_roles = kRightAdjacentRoles;
        else if (strcmp(face_role, "TOP") == 0) adjacent_roles = kTopAdjacentRoles;
        else if (strcmp(face_role, "BOTTOM") == 0) adjacent_roles = kBottomAdjacentRoles;
        else return false;
    }
    if (!resolved_layout || !resolved_slot || !resolved_orientation || !adjacent_roles) {
        return false;
    }
    corner_ids = json_object_new_array();
    edge_ids = json_object_new_array();
    adjacent_face_roles = json_object_new_array();
    if (!corner_ids || !edge_ids || !adjacent_face_roles) {
        if (corner_ids) json_object_put(corner_ids);
        if (edge_ids) json_object_put(edge_ids);
        if (adjacent_face_roles) json_object_put(adjacent_face_roles);
        return false;
    }
    json_object_object_add(surface, "net_layout_kind", json_object_new_string(resolved_layout));
    json_object_object_add(surface, "net_slot", json_object_new_string(resolved_slot));
    json_object_object_add(surface, "orientation", json_object_new_string(resolved_orientation));
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

static bool runtime_material_payload_test_write_png_rgba(const char* path,
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

static bool runtime_material_payload_test_write_authored_texture_manifest(
    const char* manifest_path,
    const char* object_id,
    const char* primitive_kind,
    const char* face_role,
    const char* file_name) {
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
    json_object_object_add(root, "source_scene_id", json_object_new_string("scene_payload_test"));
    json_object_object_add(root, "source_object_id", json_object_new_string(object_id));
    json_object_object_add(root, "base_surface_count", json_object_new_int(1));
    json_object_object_add(base_surface, "surface_id", json_object_new_int(1));
    json_object_object_add(base_surface, "face_role", json_object_new_string(face_role));
    json_object_object_add(base_surface, "file_name", json_object_new_string(file_name));
    if (!runtime_material_payload_test_add_surface_semantic_fields(base_surface,
                                                                   primitive_kind,
                                                                   face_role,
                                                                   NULL,
                                                                   NULL,
                                                                   NULL)) {
        json_object_put(root);
        return false;
    }
    json_object_array_add(base_surfaces, base_surface);
    json_object_object_add(root, "base_surfaces", base_surfaces);
    write_ok = json_object_to_file_ext(manifest_path, root, JSON_C_TO_STRING_PRETTY);
    json_object_put(root);
    return write_ok == 0;
}

static bool runtime_material_payload_test_write_authored_texture_manifest_with_metadata(
    const char* manifest_path,
    const char* object_id,
    const char* primitive_kind,
    const char* face_role,
    const char* file_name,
    const char* net_layout_kind,
    const char* net_slot,
    const char* orientation,
    double layout_offset_x,
    double layout_offset_y,
    const char* const* layer_material_intent_stable_ids,
    size_t layer_material_intent_count) {
    json_object* root = NULL;
    json_object* base_surfaces = NULL;
    json_object* base_surface = NULL;
    int write_ok = 0;
    if (!manifest_path || !object_id || !primitive_kind || !face_role || !file_name ||
        !net_layout_kind || !net_slot || !orientation) {
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
    json_object_object_add(root, "source_scene_id", json_object_new_string("scene_payload_test"));
    json_object_object_add(root, "source_object_id", json_object_new_string(object_id));
    json_object_object_add(root, "base_surface_count", json_object_new_int(1));
    json_object_object_add(base_surface, "surface_id", json_object_new_int(1));
    json_object_object_add(base_surface, "face_role", json_object_new_string(face_role));
    json_object_object_add(base_surface, "file_name", json_object_new_string(file_name));
    if (!runtime_material_payload_test_add_surface_semantic_fields(base_surface,
                                                                   primitive_kind,
                                                                   face_role,
                                                                   net_layout_kind,
                                                                   net_slot,
                                                                   orientation)) {
        json_object_put(root);
        return false;
    }
    {
        json_object* base_layer_material_intents = json_object_new_array();
        if (!base_layer_material_intents) {
            json_object_put(root);
            return false;
        }
        for (size_t i = 0u; i < layer_material_intent_count; ++i) {
            if (layer_material_intent_stable_ids && layer_material_intent_stable_ids[i]) {
                json_object_array_add(base_layer_material_intents,
                                      json_object_new_string(layer_material_intent_stable_ids[i]));
            }
        }
        json_object_object_add(base_surface,
                               "layer_material_intent_stable_ids",
                               base_layer_material_intents);
    }
    json_object_object_add(base_surface, "layout_offset_x", json_object_new_double(layout_offset_x));
    json_object_object_add(base_surface, "layout_offset_y", json_object_new_double(layout_offset_y));
    runtime_material_payload_test_add_material_intent_summaries(base_surface,
                                                                layer_material_intent_stable_ids,
                                                                layer_material_intent_count,
                                                                NULL,
                                                                NULL);
    json_object_array_add(base_surfaces, base_surface);
    json_object_object_add(root, "base_surfaces", base_surfaces);
    write_ok = json_object_to_file_ext(manifest_path, root, JSON_C_TO_STRING_PRETTY);
    json_object_put(root);
    return write_ok == 0;
}

static bool runtime_material_payload_test_write_dual_lane_manifest_with_intents(
    const char* manifest_path,
    const char* object_id,
    const char* primitive_kind,
    const char* face_role,
    const char* base_file_name,
    const char* overlay_file_name,
    const char* const* base_layer_material_intent_stable_ids,
    size_t base_layer_material_intent_count,
    const char* const* overlay_layer_material_intent_stable_ids,
    size_t overlay_layer_material_intent_count,
    const char* overlay_material_intent_kind) {
    json_object* root = NULL;
    json_object* base_surfaces = NULL;
    json_object* overlay_surfaces = NULL;
    json_object* base_surface = NULL;
    json_object* overlay_surface = NULL;
    json_object* base_intents = NULL;
    json_object* overlay_intents = NULL;
    int write_ok = 0;
    if (!manifest_path || !object_id || !primitive_kind || !face_role || !base_file_name ||
        !overlay_file_name) {
        return false;
    }
    root = json_object_new_object();
    base_surfaces = json_object_new_array();
    overlay_surfaces = json_object_new_array();
    base_surface = json_object_new_object();
    overlay_surface = json_object_new_object();
    base_intents = json_object_new_array();
    overlay_intents = json_object_new_array();
    if (!root || !base_surfaces || !overlay_surfaces || !base_surface || !overlay_surface ||
        !base_intents || !overlay_intents) {
        if (root) json_object_put(root);
        if (base_surfaces) json_object_put(base_surfaces);
        if (overlay_surfaces) json_object_put(overlay_surfaces);
        if (base_surface) json_object_put(base_surface);
        if (overlay_surface) json_object_put(overlay_surface);
        if (base_intents) json_object_put(base_intents);
        if (overlay_intents) json_object_put(overlay_intents);
        return false;
    }
    json_object_object_add(root, "schema_version", json_object_new_int(5));
    json_object_object_add(root,
                           "export_binding_kind",
                           json_object_new_string("SEPARATE_FACES"));
    json_object_object_add(root,
                           "emitted_output_kind",
                           json_object_new_string("BASE_PLUS_OVERLAY"));
    json_object_object_add(root, "primitive_kind", json_object_new_string(primitive_kind));
    json_object_object_add(root, "source_scene_id", json_object_new_string("scene_payload_test"));
    json_object_object_add(root, "source_object_id", json_object_new_string(object_id));
    json_object_object_add(root, "surface_count", json_object_new_int(1));
    json_object_object_add(root, "base_surface_count", json_object_new_int(1));
    json_object_object_add(root, "overlay_surface_count", json_object_new_int(1));
    if (overlay_material_intent_kind) {
        json_object_object_add(root,
                               "overlay_material_intent_kind",
                               json_object_new_string(overlay_material_intent_kind));
    }
    json_object_object_add(base_surface, "surface_id", json_object_new_int(1));
    json_object_object_add(base_surface, "face_role", json_object_new_string(face_role));
    json_object_object_add(base_surface, "file_name", json_object_new_string(base_file_name));
    json_object_object_add(overlay_surface, "surface_id", json_object_new_int(1));
    json_object_object_add(overlay_surface, "face_role", json_object_new_string(face_role));
    json_object_object_add(overlay_surface, "file_name", json_object_new_string(overlay_file_name));
    if (!runtime_material_payload_test_add_surface_semantic_fields(base_surface,
                                                                   primitive_kind,
                                                                   face_role,
                                                                   NULL,
                                                                   NULL,
                                                                   NULL) ||
        !runtime_material_payload_test_add_surface_semantic_fields(overlay_surface,
                                                                   primitive_kind,
                                                                   face_role,
                                                                   NULL,
                                                                   NULL,
                                                                   NULL)) {
        json_object_put(root);
        return false;
    }
    for (size_t i = 0u; i < base_layer_material_intent_count; ++i) {
        if (base_layer_material_intent_stable_ids &&
            base_layer_material_intent_stable_ids[i]) {
            json_object_array_add(base_intents,
                                  json_object_new_string(
                                      base_layer_material_intent_stable_ids[i]));
        }
    }
    for (size_t i = 0u; i < overlay_layer_material_intent_count; ++i) {
        if (overlay_layer_material_intent_stable_ids &&
            overlay_layer_material_intent_stable_ids[i]) {
            json_object_array_add(overlay_intents,
                                  json_object_new_string(
                                      overlay_layer_material_intent_stable_ids[i]));
        }
    }
    json_object_object_add(base_surface, "layer_material_intent_stable_ids", base_intents);
    json_object_object_add(overlay_surface, "layer_material_intent_stable_ids", overlay_intents);
    runtime_material_payload_test_add_material_intent_summaries(base_surface,
                                                                base_layer_material_intent_stable_ids,
                                                                base_layer_material_intent_count,
                                                                NULL,
                                                                NULL);
    runtime_material_payload_test_add_material_intent_summaries(overlay_surface,
                                                                overlay_layer_material_intent_stable_ids,
                                                                overlay_layer_material_intent_count,
                                                                NULL,
                                                                NULL);
    json_object_array_add(base_surfaces, base_surface);
    json_object_array_add(overlay_surfaces, overlay_surface);
    json_object_object_add(root, "base_surfaces", base_surfaces);
    json_object_object_add(root, "overlay_surfaces", overlay_surfaces);
    write_ok = json_object_to_file_ext(manifest_path, root, JSON_C_TO_STRING_PRETTY);
    json_object_put(root);
    return write_ok == 0;
}

static bool runtime_material_payload_test_write_authored_texture_manifest_with_explicit_summaries(
    const char* manifest_path,
    const char* object_id,
    const char* primitive_kind,
    const char* face_role,
    const char* file_name,
    const char* const* layer_material_intent_stable_ids,
    size_t layer_material_intent_count,
    const char* explicit_base_summary,
    const char* explicit_overlay_summary) {
    json_object* root = NULL;
    json_object* base_surfaces = NULL;
    json_object* base_surface = NULL;
    json_object* layer_intents = NULL;
    int write_ok = 0;
    if (!manifest_path || !object_id || !primitive_kind || !face_role || !file_name) {
        return false;
    }
    root = json_object_new_object();
    base_surfaces = json_object_new_array();
    base_surface = json_object_new_object();
    layer_intents = json_object_new_array();
    if (!root || !base_surfaces || !base_surface || !layer_intents) {
        if (root) json_object_put(root);
        if (base_surfaces) json_object_put(base_surfaces);
        if (base_surface) json_object_put(base_surface);
        if (layer_intents) json_object_put(layer_intents);
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
    json_object_object_add(root, "source_scene_id", json_object_new_string("scene_payload_test"));
    json_object_object_add(root, "source_object_id", json_object_new_string(object_id));
    json_object_object_add(root, "base_surface_count", json_object_new_int(1));
    json_object_object_add(base_surface, "surface_id", json_object_new_int(1));
    json_object_object_add(base_surface, "face_role", json_object_new_string(face_role));
    json_object_object_add(base_surface, "file_name", json_object_new_string(file_name));
    if (!runtime_material_payload_test_add_surface_semantic_fields(base_surface,
                                                                   primitive_kind,
                                                                   face_role,
                                                                   NULL,
                                                                   NULL,
                                                                   NULL)) {
        json_object_put(root);
        return false;
    }
    for (size_t i = 0u; i < layer_material_intent_count; ++i) {
        if (layer_material_intent_stable_ids && layer_material_intent_stable_ids[i]) {
            json_object_array_add(layer_intents,
                                  json_object_new_string(layer_material_intent_stable_ids[i]));
        }
    }
    json_object_object_add(base_surface, "layer_material_intent_stable_ids", layer_intents);
    runtime_material_payload_test_add_material_intent_summaries(base_surface,
                                                                layer_material_intent_stable_ids,
                                                                layer_material_intent_count,
                                                                explicit_base_summary,
                                                                explicit_overlay_summary);
    json_object_array_add(base_surfaces, base_surface);
    json_object_object_add(root, "base_surfaces", base_surfaces);
    write_ok = json_object_to_file_ext(manifest_path, root, JSON_C_TO_STRING_PRETTY);
    json_object_put(root);
    return write_ok == 0;
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
    sceneSettings.sceneObjects[1].alpha = 0.5;
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
    assert_close("runtime_material_payload_3d_scene_object_base_r_match",
                 payload.baseColorR,
                 expected.baseColorR,
                 1e-9);
    assert_close("runtime_material_payload_3d_scene_object_base_g_match",
                 payload.baseColorG,
                 expected.baseColorG,
                 1e-9);
    assert_close("runtime_material_payload_3d_scene_object_base_b_match",
                 payload.baseColorB,
                 expected.baseColorB,
                 1e-9);
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

static int test_runtime_material_payload_3d_authoring_object_values_override_preset(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialPayload3D payload = {0};
    bool ok = false;

    MaterialManagerResetDefaults();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 6.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_GLOSSY;
    sceneSettings.sceneObjects[0].color = 0x3366CC;
    sceneSettings.sceneObjects[0].reflectivity = 0.52;
    sceneSettings.sceneObjects[0].roughness = 0.19;

    ok = RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(0, &payload);
    assert_true("runtime_material_payload_authoring_object_override_ok", ok);
    assert_close("runtime_material_payload_authoring_object_override_base_r",
                 payload.baseColorR,
                 0x33 / 255.0,
                 1e-9);
    assert_close("runtime_material_payload_authoring_object_override_base_g",
                 payload.baseColorG,
                 0x66 / 255.0,
                 1e-9);
    assert_close("runtime_material_payload_authoring_object_override_base_b",
                 payload.baseColorB,
                 0xCC / 255.0,
                 1e-9);
    assert_close("runtime_material_payload_authoring_object_override_reflectivity",
                 payload.bsdf.reflectivity,
                 0.52,
                 1e-9);
    assert_close("runtime_material_payload_authoring_object_override_roughness",
                 payload.bsdf.roughness,
                 0.19,
                 1e-9);

    sceneSettings = saved_scene;
    return 0;
}

static int test_runtime_material_payload_3d_object_multipliers_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialPayload3D payload = {0};
    MaterialBSDF bsdf = {0};
    bool ok = false;

    MaterialManagerResetDefaults();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 2;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 10.0, 0.0, NULL, 0);
    InitObject(&sceneSettings.sceneObjects[1], OBJECT_CIRCLE, 5.0, -2.0, 6.0, 0.0, NULL, 0);

    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[0].alpha = 0.25;
    ok = RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(0, &payload);
    assert_true("runtime_material_payload_multiplier_transparency_ok", ok);
    assert_close("runtime_material_payload_multiplier_transparency_scaled",
                 payload.transparency,
                 0.25 * 0.75,
                 1e-9);

    sceneSettings.sceneObjects[1].material_id = MATERIAL_PRESET_EMISSIVE;
    sceneSettings.sceneObjects[1].emissiveStrength = 0.4;
    MaterialBSDFInitFromSceneObject(&sceneSettings.sceneObjects[1], &bsdf);
    assert_close("runtime_material_payload_multiplier_emissive_scaled",
                 bsdf.emissive,
                 0.4 * 0.5,
                 1e-9);

    sceneSettings = saved_scene;
    return 0;
}

static int test_runtime_material_payload_3d_water_override_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialPayload3D payload = {0};
    RuntimeWaterMaterial3DOverride override = {0};
    double expected_tint_r = 0.0;
    double expected_tint_g = 0.0;
    double expected_tint_b = 0.0;
    bool ok = false;

    MaterialManagerResetDefaults();
    RuntimeWaterMaterial3D_ClearAll();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 10.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[0].alpha = 1.0;
    sceneSettings.sceneObjects[0].opacity = 1.0;

    override.valid = true;
    override.ior = 1.333;
    override.absorptionDistance = 4.0;
    override.absorptionR = 0.10;
    override.absorptionG = 0.035;
    override.absorptionB = 0.015;
    override.transparency = 0.92;
    override.reflectivity = 0.12;
    override.roughness = 0.02;
    ok = RuntimeWaterMaterial3D_Set(0, &override);
    assert_true("runtime_material_payload_water_override_set", ok);
    RuntimeWaterMaterial3D_ComputeTransmittanceTint(override.absorptionDistance,
                                                    override.absorptionR,
                                                    override.absorptionG,
                                                    override.absorptionB,
                                                    &expected_tint_r,
                                                    &expected_tint_g,
                                                    &expected_tint_b);

    ok = RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(0, &payload);
    assert_true("runtime_material_payload_water_override_resolve", ok);
    assert_true("runtime_material_payload_water_override_valid", payload.valid);
    assert_close("runtime_material_payload_water_override_ior",
                 payload.opticalIor,
                 1.333,
                 1e-9);
    assert_close("runtime_material_payload_water_override_bsdf_ior",
                 payload.bsdf.ior,
                 1.333,
                 1e-9);
    assert_close("runtime_material_payload_water_override_absorption_distance",
                 payload.absorptionDistance,
                 4.0,
                 1e-9);
    assert_close("runtime_material_payload_water_override_transparency",
                 payload.transparency,
                 0.92,
                 1e-9);
    assert_close("runtime_material_payload_water_override_tint_r",
                 payload.baseColorR,
                 expected_tint_r,
                 1e-9);
    assert_close("runtime_material_payload_water_override_tint_g",
                 payload.baseColorG,
                 expected_tint_g,
                 1e-9);
    assert_close("runtime_material_payload_water_override_tint_b",
                 payload.baseColorB,
                 expected_tint_b,
                 1e-9);
    assert_close("runtime_material_payload_water_override_reflectivity",
                 payload.bsdf.reflectivity,
                 0.12,
                 1e-9);
    assert_close("runtime_material_payload_water_override_roughness",
                 payload.bsdf.roughness,
                 0.02,
                 1e-9);
    assert_true("runtime_material_payload_water_override_ggx",
                payload.bsdf.model == MATERIAL_BSDF_GGX);
    assert_true("runtime_material_payload_water_override_specular_weight",
                payload.bsdf.specWeight >= 0.12);
    assert_true("runtime_material_payload_water_override_solid", !payload.thinWalled);

    RuntimeWaterMaterial3D_ClearAll();
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
    assert_true("material_manager_transparent_preset_has_dielectric_ior",
                transparent->ior > 1.0f);
    assert_true("material_manager_transparent_preset_has_absorption_distance",
                transparent->absorption_distance > 0.0f);
    assert_true("material_manager_transparent_preset_low_roughness",
                transparent->roughness <= 0.05f);
    assert_true("material_manager_transparent_preset_is_thin_walled_by_default",
                transparent->thin_walled);
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
                         "\"ior\":1.31,"
                         "\"absorption_distance\":3.5,"
                         "\"thin_walled\":true,"
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
    assert_close("material_manager_load_dir_transparent_parses_ior",
                 transparent->ior,
                 1.31,
                 1e-6);
    assert_close("material_manager_load_dir_transparent_parses_absorption_distance",
                 transparent->absorption_distance,
                 3.5,
                 1e-6);
    assert_true("material_manager_load_dir_transparent_parses_thin_walled",
                transparent->thin_walled);

    remove(mirror_path);
    remove(emissive_path);
    remove(transparent_path);
    rmdir(dir_template);
    MaterialManagerResetDefaults();
    return 0;
}

static int test_material_manager_transparent_override_inherits_shipped_defaults(void) {
    char dir_template[] = "/tmp/rt_material_partial_ids_XXXXXX";
    char transparent_path[512];
    const Material* transparent = NULL;
    bool ok = false;

    MaterialManagerResetDefaults();
    if (!mkdtemp(dir_template)) {
        return 0;
    }

    snprintf(transparent_path, sizeof(transparent_path), "%s/transparent.json", dir_template);
    ok = write_text_file(transparent_path,
                         "{"
                         "\"diffuse\":0.05,"
                         "\"specular\":0.0,"
                         "\"reflectivity\":0.0,"
                         "\"roughness\":0.04,"
                         "\"transparency\":0.70,"
                         "\"base_color\":[1.0,1.0,1.0],"
                         "\"emissive\":[0.0,0.0,0.0]"
                         "}");
    assert_true("material_manager_partial_transparent_file_ok", ok);

    MaterialManagerLoadDir(dir_template);
    transparent = MaterialManagerGet(MATERIAL_PRESET_TRANSPARENT);

    assert_true("material_manager_partial_transparent_exists", transparent != NULL);
    assert_close("material_manager_partial_transparent_keeps_override_transparency",
                 transparent->transparency,
                 0.70,
                 1e-6);
    assert_close("material_manager_partial_transparent_inherits_ior",
                 transparent->ior,
                 1.45,
                 1e-6);
    assert_close("material_manager_partial_transparent_inherits_absorption_distance",
                 transparent->absorption_distance,
                 2.0,
                 1e-6);
    assert_true("material_manager_partial_transparent_inherits_thin_walled",
                transparent->thin_walled);

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
    assert_close("runtime_material_payload_3d_hit_base_r_match",
                 payload.baseColorR,
                 expected.baseColorR,
                 1e-9);
    assert_close("runtime_material_payload_3d_hit_base_g_match",
                 payload.baseColorG,
                 expected.baseColorG,
                 1e-9);
    assert_close("runtime_material_payload_3d_hit_base_b_match",
                 payload.baseColorB,
                 expected.baseColorB,
                 1e-9);
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
    assert_close("runtime_material_payload_3d_hit_optical_ior_match",
                 payload.opticalIor,
                 MaterialManagerGet(MATERIAL_PRESET_GLOSSY)->ior,
                 1e-9);
    assert_close("runtime_material_payload_3d_hit_absorption_distance_match",
                 payload.absorptionDistance,
                 MaterialManagerGet(MATERIAL_PRESET_GLOSSY)->absorption_distance,
                 1e-9);
    assert_true("runtime_material_payload_3d_hit_thin_walled_match",
                payload.thinWalled == MaterialManagerGet(MATERIAL_PRESET_GLOSSY)->thin_walled);
    assert_true("runtime_material_payload_3d_hit_invalid_index_rejected",
                !RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(4, &payload));
    assert_true("runtime_material_payload_3d_hit_invalid_hit_rejected",
                !RuntimeMaterialPayload3D_ResolveFromHit(NULL, &payload));

    sceneSettings = saved_scene;
    return 0;
}

static int test_runtime_material_payload_3d_rust_texture_is_hit_anchored(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialPayload3D baseline = {0};
    RuntimeMaterialPayload3D textured = {0};
    RuntimeMaterialPayload3D repeated = {0};
    RuntimeMaterialPayload3D panned = {0};
    HitInfo3D hit = {0};
    bool ok = false;

    MaterialManagerResetDefaults();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 8.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].color = 0xB0B0B0;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_ROUGH_METAL;
    sceneSettings.sceneObjects[0].textureId = RUNTIME_MATERIAL_TEXTURE_3D_RUST;
    sceneSettings.sceneObjects[0].textureStrength = 0.0;

    HitInfo3D_Reset(&hit);
    hit.sceneObjectIndex = 0;
    hit.triangleIndex = 7;
    hit.primitiveIndex = 1;
    hit.baryU = 0.34;
    hit.baryV = 0.52;
    hit.baryW = 0.14;

    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &baseline);
    assert_true("runtime_material_payload_rust_baseline_ok", ok);

    sceneSettings.sceneObjects[0].textureStrength = 1.0;
    sceneSettings.sceneObjects[0].textureScale = 1.0;
    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &textured);
    assert_true("runtime_material_payload_rust_textured_ok", ok);

    if (textured.textureMask <= 1e-9) {
        hit.baryU = 0.15;
        hit.baryV = 0.18;
        hit.baryW = 0.67;
        ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &textured);
        assert_true("runtime_material_payload_rust_textured_retry_ok", ok);
    }

    assert_true("runtime_material_payload_rust_mask_active", textured.textureMask > 1e-9);
    assert_true("runtime_material_payload_rust_reflectivity_reduced",
                textured.bsdf.reflectivity < baseline.bsdf.reflectivity);
    assert_true("runtime_material_payload_rust_roughness_increased",
                textured.bsdf.roughness > baseline.bsdf.roughness);
    assert_true("runtime_material_payload_rust_color_shifted_red",
                textured.baseColorR > baseline.baseColorR &&
                textured.baseColorG < baseline.baseColorG);

    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &repeated);
    assert_true("runtime_material_payload_rust_repeat_ok", ok);
    assert_close("runtime_material_payload_rust_repeat_mask_stable",
                 repeated.textureMask,
                 textured.textureMask,
                 1e-12);
    assert_close("runtime_material_payload_rust_repeat_u_stable",
                 repeated.textureU,
                 textured.textureU,
                 1e-12);

    sceneSettings.sceneObjects[0].textureOffsetU = 0.25;
    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &panned);
    assert_true("runtime_material_payload_rust_pan_ok", ok);
    assert_true("runtime_material_payload_rust_pan_moves_u",
                fabs(panned.textureU - textured.textureU) > 1e-6);

    sceneSettings = saved_scene;
    return 0;
}

static int test_runtime_material_payload_3d_face_texture_override_affects_hit(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialPayload3D baseline = {0};
    RuntimeMaterialPayload3D textured = {0};
    HitInfo3D hit = {0};
    SceneEditorMaterialFacePlacement placement = {0};
    bool ok = false;
    bool found_mask = false;

    MaterialManagerResetDefaults();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    SceneEditorMaterialFacePlacementResetAll();
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 8.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].color = 0xB0B0B0;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_MIRROR;
    sceneSettings.sceneObjects[0].textureId = RUNTIME_MATERIAL_TEXTURE_3D_NONE;
    sceneSettings.sceneObjects[0].textureStrength = 0.0;

    HitInfo3D_Reset(&hit);
    hit.sceneObjectIndex = 0;
    hit.triangleIndex = 18;
    hit.localTriangleIndex = 4;
    hit.primitiveIndex = 1;
    hit.baryU = 0.34;
    hit.baryV = 0.52;
    hit.baryW = 0.14;

    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &baseline);
    assert_true("runtime_material_payload_face_texture_baseline_ok", ok);
    assert_close("runtime_material_payload_face_texture_baseline_mask",
                 baseline.textureMask,
                 0.0,
                 1e-12);

    placement.hasOverride = true;
    placement.sceneObjectIndex = 0;
    placement.faceGroupIndex = 2;
    placement.textureId = RUNTIME_MATERIAL_TEXTURE_3D_RUST;
    placement.scale = 1.0;
    placement.strength = 1.0;
    assert_true("runtime_material_payload_face_texture_override_set",
                SceneEditorMaterialFacePlacementSetOverride(&placement));

    for (int u = 1; u < 9 && !found_mask; ++u) {
        for (int v = 1; v < 9 && !found_mask; ++v) {
            double bary_v = (double)u / 10.0;
            double bary_w = (double)v / 10.0;
            if (bary_v + bary_w >= 0.95) continue;
            hit.baryV = bary_v;
            hit.baryW = bary_w;
            hit.baryU = 1.0 - bary_v - bary_w;
            ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &textured);
            assert_true("runtime_material_payload_face_texture_override_retry_ok", ok);
            found_mask = textured.textureMask > 1e-9;
        }
    }

    assert_true("runtime_material_payload_face_texture_mask_active", found_mask);
    assert_true("runtime_material_payload_face_texture_roughness_increased",
                textured.bsdf.roughness > baseline.bsdf.roughness);
    assert_true("runtime_material_payload_face_texture_reflectivity_reduced",
                textured.bsdf.reflectivity < baseline.bsdf.reflectivity);
    assert_true("runtime_material_payload_face_texture_object_default_unchanged",
                sceneSettings.sceneObjects[0].textureId == RUNTIME_MATERIAL_TEXTURE_3D_NONE);

    SceneEditorMaterialFacePlacementResetAll();
    sceneSettings = saved_scene;
    return 0;
}

static int test_runtime_material_payload_3d_authored_texture_override_affects_hit(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeMaterialPayload3D baseline = {0};
    RuntimeMaterialPayload3D authored = {0};
    HitInfo3D hit = {0};
    const char* dir = "/tmp/rt_authored_texture_payload_suite";
    const char* png_path = "/tmp/rt_authored_texture_payload_suite/glass_plane_front.png";
    const char* manifest_path =
        "/tmp/rt_authored_texture_payload_suite/glass_plane_texture_manifest.json";
    const char* manifest_rel = "glass_plane_texture_manifest.json";
    const unsigned char rgba[] = {255u, 40u, 20u, 128u};
    bool ok = false;

    MaterialManagerResetDefaults();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    RuntimeMaterialAuthoredTextureResetAll();
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 8.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].color = 0xC0E0FF;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[0].alpha = 0.8;
    sceneSettings.sceneObjects[0].textureId = RUNTIME_MATERIAL_TEXTURE_3D_RUST;
    sceneSettings.sceneObjects[0].textureStrength = 1.0;

    HitInfo3D_Reset(&hit);
    hit.sceneObjectIndex = 0;
    hit.triangleIndex = 2;
    hit.localTriangleIndex = 0;
    hit.primitiveIndex = 0;
    hit.baryU = 0.4;
    hit.baryV = 0.2;
    hit.baryW = 0.4;

    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &baseline);
    assert_true("runtime_material_payload_authored_baseline_ok", ok);

    (void)mkdir(dir, 0775);
    assert_true("runtime_material_payload_authored_png_write_ok",
                runtime_material_payload_test_write_png_rgba(png_path, rgba, 1u, 1u));
    assert_true("runtime_material_payload_authored_manifest_write_ok",
                runtime_material_payload_test_write_authored_texture_manifest(manifest_path,
                                                                              "glass_plane",
                                                                              "PLANE",
                                                                              "FRONT",
                                                                              "glass_plane_front.png"));
    snprintf(animSettings.runtimeScenePath,
             sizeof(animSettings.runtimeScenePath),
             "%s/scene_runtime.json",
             dir);
    assert_true("runtime_material_payload_authored_bind_ok",
                RuntimeMaterialAuthoredTextureBindManifestForObject(0,
                                                                   "glass_plane",
                                                                   manifest_rel,
                                                                   "override"));

    sceneSettings.sceneObjects[0].textureStrength = 0.0;
    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &authored);
    assert_true("runtime_material_payload_authored_hit_ok", ok);
    assert_true("runtime_material_payload_authored_mask_active", authored.textureMask > 0.45);
    assert_true("runtime_material_payload_authored_red_shift",
                authored.baseColorR > baseline.baseColorR);
    assert_true("runtime_material_payload_authored_green_reduced",
                authored.baseColorG < baseline.baseColorG);
    assert_true("runtime_material_payload_authored_transparency_reduced",
                authored.transparency < baseline.transparency);

    RuntimeMaterialAuthoredTextureResetAll();
    unlink(png_path);
    unlink(manifest_path);
    rmdir(dir);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_material_payload_authored_texture_metadata_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeMaterialAuthoredTextureFaceMetadata metadata;
    const char* dir = "/tmp/ray_tracing_authored_texture_metadata_contract";
    const char* png_path = "/tmp/ray_tracing_authored_texture_metadata_contract/plane_front.png";
    const char* manifest_path =
        "/tmp/ray_tracing_authored_texture_metadata_contract/plane_manifest.json";
    const char* manifest_rel = "plane_manifest.json";
    const unsigned char rgba[4] = {255u, 255u, 255u, 255u};
    const char* intents[] = {"concrete", "oil"};

    (void)mkdir(dir, 0775);
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 8.0, 0.0, NULL, 0);
    snprintf(animSettings.runtimeScenePath,
             sizeof(animSettings.runtimeScenePath),
             "%s/scene_runtime.json",
             dir);
    assert_true("runtime_material_payload_authored_metadata_png_write_ok",
                runtime_material_payload_test_write_png_rgba(png_path, rgba, 1u, 1u));
    assert_true("runtime_material_payload_authored_metadata_manifest_write_ok",
                runtime_material_payload_test_write_authored_texture_manifest_with_metadata(
                    manifest_path,
                    "glass_plane",
                    "PLANE",
                    "FRONT",
                    "plane_front.png",
                    "PLANE",
                    "FRONT",
                    "R0",
                    11.5,
                    -4.25,
                    intents,
                    2u));
    assert_true("runtime_material_payload_authored_metadata_bind_ok",
                RuntimeMaterialAuthoredTextureBindManifestForObject(0,
                                                                   "glass_plane",
                                                                   manifest_rel,
                                                                   "override"));
    assert_true("runtime_material_payload_authored_metadata_get_ok",
                RuntimeMaterialAuthoredTextureGetFaceMetadata(0, 0, &metadata));
    assert_true("runtime_material_payload_authored_metadata_active", metadata.active);
    assert_true("runtime_material_payload_authored_metadata_slot",
                strcmp(metadata.netSlot, "FRONT") == 0);
    assert_true("runtime_material_payload_authored_metadata_layout_kind",
                strcmp(metadata.netLayoutKind, "PLANE") == 0);
    assert_true("runtime_material_payload_authored_metadata_orientation",
                strcmp(metadata.orientation, "R0") == 0);
    assert_true("runtime_material_payload_authored_metadata_base_material_intent",
                strcmp(metadata.baseMaterialIntentKind, "concrete") == 0);
    assert_true("runtime_material_payload_authored_metadata_overlay_material_intent",
                strcmp(metadata.overlayMaterialIntentKind, "oil") == 0);
    assert_true("runtime_material_payload_authored_metadata_corner_ids",
                metadata.cornerIds[0] == 255u && metadata.cornerIds[1] == 255u &&
                    metadata.cornerIds[2] == 255u && metadata.cornerIds[3] == 255u);
    assert_true("runtime_material_payload_authored_metadata_edge_ids",
                metadata.edgeIds[0] == 255u && metadata.edgeIds[1] == 255u &&
                    metadata.edgeIds[2] == 255u && metadata.edgeIds[3] == 255u);
    assert_close("runtime_material_payload_authored_metadata_offset_x",
                 metadata.layoutOffsetX,
                 11.5,
                 1e-9);
    assert_close("runtime_material_payload_authored_metadata_offset_y",
                 metadata.layoutOffsetY,
                 -4.25,
                 1e-9);

    RuntimeMaterialAuthoredTextureResetAll();
    unlink(png_path);
    unlink(manifest_path);
    rmdir(dir);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_material_payload_authored_texture_explicit_summary_overrides_layer_list(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeMaterialAuthoredTextureFaceMetadata metadata;
    const char* dir = "/tmp/ray_tracing_authored_texture_summary_precedence";
    const char* png_path = "/tmp/ray_tracing_authored_texture_summary_precedence/plane_front.png";
    const char* manifest_path =
        "/tmp/ray_tracing_authored_texture_summary_precedence/plane_manifest.json";
    const char* manifest_rel = "plane_manifest.json";
    const unsigned char rgba[4] = {255u, 255u, 255u, 255u};
    const char* intents[] = {"solid", "oil"};

    (void)mkdir(dir, 0775);
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 8.0, 0.0, NULL, 0);
    snprintf(animSettings.runtimeScenePath,
             sizeof(animSettings.runtimeScenePath),
             "%s/scene_runtime.json",
             dir);
    assert_true("runtime_material_payload_explicit_summary_png_write_ok",
                runtime_material_payload_test_write_png_rgba(png_path, rgba, 1u, 1u));
    assert_true("runtime_material_payload_explicit_summary_manifest_write_ok",
                runtime_material_payload_test_write_authored_texture_manifest_with_explicit_summaries(
                    manifest_path,
                    "glass_plane",
                    "PLANE",
                    "FRONT",
                    "plane_front.png",
                    intents,
                    2u,
                    "concrete",
                    "grime"));
    assert_true("runtime_material_payload_explicit_summary_bind_ok",
                RuntimeMaterialAuthoredTextureBindManifestForObject(0,
                                                                   "glass_plane",
                                                                   manifest_rel,
                                                                   "override"));
    assert_true("runtime_material_payload_explicit_summary_get_ok",
                RuntimeMaterialAuthoredTextureGetFaceMetadata(0, 0, &metadata));
    assert_true("runtime_material_payload_explicit_summary_base_kind",
                strcmp(metadata.baseMaterialIntentKind, "concrete") == 0);
    assert_true("runtime_material_payload_explicit_summary_overlay_kind",
                strcmp(metadata.overlayMaterialIntentKind, "grime") == 0);

    RuntimeMaterialAuthoredTextureResetAll();
    unlink(png_path);
    unlink(manifest_path);
    rmdir(dir);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_material_payload_authored_texture_face_base_intent_modulates_bsdf(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeMaterialPayload3D baseline = {0};
    RuntimeMaterialPayload3D authored = {0};
    HitInfo3D hit = {0};
    const char* dir = "/tmp/rt_authored_texture_base_intent_suite";
    const char* png_path = "/tmp/rt_authored_texture_base_intent_suite/metal_plane_front.png";
    const char* manifest_path =
        "/tmp/rt_authored_texture_base_intent_suite/metal_plane_texture_manifest.json";
    const char* manifest_rel = "metal_plane_texture_manifest.json";
    const unsigned char rgba[] = {210u, 210u, 210u, 255u};
    const char* intents[] = {"metal"};
    bool ok = false;

    MaterialManagerResetDefaults();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    RuntimeMaterialAuthoredTextureResetAll();
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 8.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].color = 0xA0A0A0;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_ROUGH_METAL;
    sceneSettings.sceneObjects[0].alpha = 1.0;
    sceneSettings.sceneObjects[0].textureStrength = 0.0;

    HitInfo3D_Reset(&hit);
    hit.sceneObjectIndex = 0;
    hit.triangleIndex = 2;
    hit.localTriangleIndex = 0;
    hit.primitiveIndex = 0;
    hit.baryU = 0.25;
    hit.baryV = 0.25;
    hit.baryW = 0.50;

    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &baseline);
    assert_true("runtime_material_payload_authored_base_intent_baseline_ok", ok);

    (void)mkdir(dir, 0775);
    assert_true("runtime_material_payload_authored_base_intent_png_write_ok",
                runtime_material_payload_test_write_png_rgba(png_path, rgba, 1u, 1u));
    assert_true("runtime_material_payload_authored_base_intent_manifest_write_ok",
                runtime_material_payload_test_write_authored_texture_manifest_with_metadata(
                    manifest_path,
                    "metal_plane",
                    "PLANE",
                    "FRONT",
                    "metal_plane_front.png",
                    "PLANE",
                    "FRONT",
                    "R0",
                    0.0,
                    0.0,
                    intents,
                    1u));
    snprintf(animSettings.runtimeScenePath,
             sizeof(animSettings.runtimeScenePath),
             "%s/scene_runtime.json",
             dir);
    assert_true("runtime_material_payload_authored_base_intent_bind_ok",
                RuntimeMaterialAuthoredTextureBindManifestForObject(0,
                                                                   "metal_plane",
                                                                   manifest_rel,
                                                                   "override"));
    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &authored);
    assert_true("runtime_material_payload_authored_base_intent_hit_ok", ok);
    assert_true("runtime_material_payload_authored_base_intent_reflectivity_up",
                authored.bsdf.reflectivity > baseline.bsdf.reflectivity + 1e-6);
    assert_true("runtime_material_payload_authored_base_intent_roughness_down",
                authored.bsdf.roughness < baseline.bsdf.roughness - 1e-6);

    RuntimeMaterialAuthoredTextureResetAll();
    unlink(png_path);
    unlink(manifest_path);
    rmdir(dir);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_material_payload_authored_texture_face_overlay_intent_overrides_binding_default(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeMaterialPayload3D baseline = {0};
    RuntimeMaterialPayload3D authored = {0};
    RuntimeMaterialAuthoredTextureFaceMetadata metadata = {0};
    HitInfo3D hit = {0};
    const char* dir = "/tmp/rt_authored_texture_overlay_intent_suite";
    const char* base_png_path = "/tmp/rt_authored_texture_overlay_intent_suite/intent_plane_front_base.png";
    const char* overlay_png_path = "/tmp/rt_authored_texture_overlay_intent_suite/intent_plane_front_overlay.png";
    const char* manifest_path =
        "/tmp/rt_authored_texture_overlay_intent_suite/intent_plane_texture_manifest.json";
    const char* manifest_rel = "intent_plane_texture_manifest.json";
    const unsigned char base_rgba[] = {120u, 120u, 120u, 255u};
    const unsigned char overlay_rgba[] = {240u, 220u, 40u, 255u};
    const char* base_intents[] = {"solid"};
    const char* overlay_intents[] = {"oil"};
    bool ok = false;

    MaterialManagerResetDefaults();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    RuntimeMaterialAuthoredTextureResetAll();
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 8.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].color = 0xA8A8A8;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[0].alpha = 1.0;
    sceneSettings.sceneObjects[0].textureStrength = 0.0;

    HitInfo3D_Reset(&hit);
    hit.sceneObjectIndex = 0;
    hit.triangleIndex = 2;
    hit.localTriangleIndex = 0;
    hit.primitiveIndex = 0;
    hit.baryU = 0.2;
    hit.baryV = 0.3;
    hit.baryW = 0.5;

    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &baseline);
    assert_true("runtime_material_payload_authored_overlay_intent_baseline_ok", ok);

    (void)mkdir(dir, 0775);
    assert_true("runtime_material_payload_authored_overlay_intent_base_png_write_ok",
                runtime_material_payload_test_write_png_rgba(base_png_path, base_rgba, 1u, 1u));
    assert_true("runtime_material_payload_authored_overlay_intent_overlay_png_write_ok",
                runtime_material_payload_test_write_png_rgba(overlay_png_path, overlay_rgba, 1u, 1u));
    assert_true("runtime_material_payload_authored_overlay_intent_manifest_write_ok",
                runtime_material_payload_test_write_dual_lane_manifest_with_intents(
                    manifest_path,
                    "intent_plane",
                    "PLANE",
                    "FRONT",
                    "intent_plane_front_base.png",
                    "intent_plane_front_overlay.png",
                    base_intents,
                    1u,
                    overlay_intents,
                    1u,
                    NULL));
    snprintf(animSettings.runtimeScenePath,
             sizeof(animSettings.runtimeScenePath),
             "%s/scene_runtime.json",
             dir);
    assert_true("runtime_material_payload_authored_overlay_intent_bind_ok",
                RuntimeMaterialAuthoredTextureBindManifestForObject(0,
                                                                   "intent_plane",
                                                                   manifest_rel,
                                                                   "override"));
    assert_true("runtime_material_payload_authored_overlay_intent_metadata_ok",
                RuntimeMaterialAuthoredTextureGetFaceMetadata(0, 0, &metadata));
    assert_true("runtime_material_payload_authored_overlay_intent_metadata_overlay_kind",
                strcmp(metadata.overlayMaterialIntentKind, "oil") == 0);
    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &authored);
    assert_true("runtime_material_payload_authored_overlay_intent_hit_ok", ok);
    assert_true("runtime_material_payload_authored_overlay_intent_reflectivity_up",
                authored.bsdf.reflectivity > baseline.bsdf.reflectivity + 1e-6);

    RuntimeMaterialAuthoredTextureResetAll();
    unlink(base_png_path);
    unlink(overlay_png_path);
    unlink(manifest_path);
    rmdir(dir);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_material_payload_authored_texture_becomes_base_but_keeps_overlay_stack(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeMaterialPayload3D authored_only = {0};
    RuntimeMaterialPayload3D authored_with_base_stack = {0};
    RuntimeMaterialPayload3D authored_with_overlay_stack = {0};
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    HitInfo3D hit = {0};
    const char* dir = "/tmp/rt_authored_texture_overlay_stack_suite";
    const char* png_path = "/tmp/rt_authored_texture_overlay_stack_suite/painted_plane_front.png";
    const char* manifest_path =
        "/tmp/rt_authored_texture_overlay_stack_suite/painted_plane_texture_manifest.json";
    const char* manifest_rel = "painted_plane_texture_manifest.json";
    const unsigned char rgba[] = {240u, 24u, 16u, 255u};
    bool ok = false;

    MaterialManagerResetDefaults();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 8.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].color = 0x90A0B8;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_ROUGH_METAL;
    sceneSettings.sceneObjects[0].alpha = 1.0;
    sceneSettings.sceneObjects[0].textureId = RUNTIME_MATERIAL_TEXTURE_3D_NONE;
    sceneSettings.sceneObjects[0].textureStrength = 0.0;

    HitInfo3D_Reset(&hit);
    hit.sceneObjectIndex = 0;
    hit.triangleIndex = 2;
    hit.localTriangleIndex = 0;
    hit.primitiveIndex = 0;
    hit.baryU = 0.31;
    hit.baryV = 0.26;
    hit.baryW = 0.43;

    (void)mkdir(dir, 0775);
    assert_true("runtime_material_payload_authored_stack_png_write_ok",
                runtime_material_payload_test_write_png_rgba(png_path, rgba, 1u, 1u));
    assert_true("runtime_material_payload_authored_stack_manifest_write_ok",
                runtime_material_payload_test_write_authored_texture_manifest(manifest_path,
                                                                              "painted_plane",
                                                                              "PLANE",
                                                                              "FRONT",
                                                                              "painted_plane_front.png"));
    snprintf(animSettings.runtimeScenePath,
             sizeof(animSettings.runtimeScenePath),
             "%s/scene_runtime.json",
             dir);
    assert_true("runtime_material_payload_authored_stack_bind_ok",
                RuntimeMaterialAuthoredTextureBindManifestForObject(0,
                                                                   "painted_plane",
                                                                   manifest_rel,
                                                                   "override"));

    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &authored_only);
    assert_true("runtime_material_payload_authored_stack_authored_only_ok", ok);
    assert_true("runtime_material_payload_authored_stack_authored_only_mask",
                authored_only.textureMask > 0.99);
    assert_true("runtime_material_payload_authored_stack_authored_only_red_dominant",
                authored_only.baseColorR > authored_only.baseColorG &&
                    authored_only.baseColorR > authored_only.baseColorB);

    stack.layerCount = 1;
    stack.layers[0] = RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD);
    stack.layers[0].placement.scale = 2.4;
    assert_true("runtime_material_payload_authored_stack_base_only_set",
                SceneEditorMaterialStackSetObjectStack(0, &stack));
    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &authored_with_base_stack);
    assert_true("runtime_material_payload_authored_stack_base_only_ok", ok);
    assert_close("runtime_material_payload_authored_stack_base_only_color_r_matches",
                 authored_with_base_stack.baseColorR,
                 authored_only.baseColorR,
                 1e-9);
    assert_close("runtime_material_payload_authored_stack_base_only_color_g_matches",
                 authored_with_base_stack.baseColorG,
                 authored_only.baseColorG,
                 1e-9);
    assert_close("runtime_material_payload_authored_stack_base_only_color_b_matches",
                 authored_with_base_stack.baseColorB,
                 authored_only.baseColorB,
                 1e-9);
    assert_close("runtime_material_payload_authored_stack_base_only_roughness_matches",
                 authored_with_base_stack.bsdf.roughness,
                 authored_only.bsdf.roughness,
                 1e-9);

    stack.layerCount = 2;
    stack.layers[0] = RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD);
    stack.layers[0].placement.scale = 2.4;
    stack.layers[1] =
        runtime_material_test_make_strong_overlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME, 3.8);
    assert_true("runtime_material_payload_authored_stack_overlay_set",
                SceneEditorMaterialStackSetObjectStack(0, &stack));
    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &authored_with_overlay_stack);
    assert_true("runtime_material_payload_authored_stack_overlay_ok", ok);
    assert_true("runtime_material_payload_authored_stack_overlay_darkens",
                authored_with_overlay_stack.baseColorR <
                    authored_only.baseColorR - 1e-6 ||
                authored_with_overlay_stack.baseColorG <
                    authored_only.baseColorG - 1e-6 ||
                authored_with_overlay_stack.baseColorB <
                    authored_only.baseColorB - 1e-6);
    assert_true("runtime_material_payload_authored_stack_overlay_roughens",
                authored_with_overlay_stack.bsdf.roughness >
                    authored_only.bsdf.roughness + 1e-6);

    SceneEditorMaterialStackResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    unlink(png_path);
    unlink(manifest_path);
    rmdir(dir);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_material_payload_3d_fog_texture_roughens_transparency(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialPayload3D baseline = {0};
    RuntimeMaterialPayload3D textured = {0};
    HitInfo3D hit = {0};
    bool ok = false;

    MaterialManagerResetDefaults();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 8.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].color = 0xB8D8FF;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[0].alpha = 0.8;
    sceneSettings.sceneObjects[0].textureId = RUNTIME_MATERIAL_TEXTURE_3D_FOG;
    sceneSettings.sceneObjects[0].textureStrength = 0.0;

    HitInfo3D_Reset(&hit);
    hit.sceneObjectIndex = 0;
    hit.triangleIndex = 4;
    hit.primitiveIndex = 1;
    hit.baryU = 0.20;
    hit.baryV = 0.30;
    hit.baryW = 0.50;

    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &baseline);
    assert_true("runtime_material_payload_fog_baseline_ok", ok);

    sceneSettings.sceneObjects[0].textureStrength = 1.0;
    sceneSettings.sceneObjects[0].textureScale = 2.0;
    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &textured);
    assert_true("runtime_material_payload_fog_textured_ok", ok);
    assert_true("runtime_material_payload_fog_mask_active", textured.textureMask > 1e-9);
    assert_true("runtime_material_payload_fog_roughness_preserved_or_increased",
                textured.bsdf.roughness >= baseline.bsdf.roughness);
    assert_true("runtime_material_payload_fog_transparency_reduced",
                textured.transparency < baseline.transparency);
    assert_true("runtime_material_payload_fog_keeps_some_transparency",
                textured.transparency > 0.0);

    sceneSettings = saved_scene;
    return 0;
}

static int test_runtime_material_texture_3d_uv_sampler_matches_hit_sampler(void) {
    SceneConfig saved_scene = sceneSettings;
    HitInfo3D hit = {0};
    RuntimeMaterialTexture3DSample hit_sample = {0};
    RuntimeMaterialTexture3DSample uv_sample = {0};
    bool hit_ok = false;
    bool uv_ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 8.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].textureId = RUNTIME_MATERIAL_TEXTURE_3D_RUST;
    sceneSettings.sceneObjects[0].textureStrength = 1.0;
    sceneSettings.sceneObjects[0].textureScale = 3.0;
    sceneSettings.sceneObjects[0].textureOffsetU = 0.125;
    sceneSettings.sceneObjects[0].textureOffsetV = 0.375;

    HitInfo3D_Reset(&hit);
    hit.sceneObjectIndex = 0;
    hit.triangleIndex = 6;
    hit.baryU = 0.18;
    hit.baryV = 0.37;
    hit.baryW = 0.45;

    hit_ok = RuntimeMaterialTexture3D_Sample(&sceneSettings.sceneObjects[0],
                                             &hit,
                                             &hit_sample);
    uv_ok = RuntimeMaterialTexture3D_SampleUV(&sceneSettings.sceneObjects[0],
                                              hit.triangleIndex,
                                              hit.baryV,
                                              hit.baryW,
                                              &uv_sample);

    assert_true("runtime_material_texture_uv_parity_active", hit_ok == uv_ok);
    assert_true("runtime_material_texture_uv_parity_kind", hit_sample.kind == uv_sample.kind);
    assert_close("runtime_material_texture_uv_parity_u", hit_sample.u, uv_sample.u, 1e-12);
    assert_close("runtime_material_texture_uv_parity_v", hit_sample.v, uv_sample.v, 1e-12);
    assert_close("runtime_material_texture_uv_parity_mask", hit_sample.mask, uv_sample.mask, 1e-12);

    sceneSettings = saved_scene;
    return 0;
}

static int test_runtime_material_texture_3d_rust_parameter_modes_change_masks(void) {
    SceneObject object;
    RuntimeMaterialTexture3DPlacement placement = {0};
    RuntimeMaterialTexture3DSample samples[4];
    double diff_sum = 0.0;

    memset(&object, 0, sizeof(object));
    memset(samples, 0, sizeof(samples));
    object.textureId = RUNTIME_MATERIAL_TEXTURE_3D_RUST;
    object.textureStrength = 1.0;
    object.textureScale = 1.0;
    placement.textureId = RUNTIME_MATERIAL_TEXTURE_3D_RUST;
    placement.strength = 1.0;
    placement.scale = 1.0;
    placement.params = RuntimeMaterialTexture3DDefaultParams();
    placement.params.coverage = 0.78;
    placement.params.edgeSoftness = 0.75;
    placement.params.contrast = 0.25;
    placement.params.grain = 0.35;
    placement.params.flow = 0.75;
    placement.params.colorDepth = 0.8;
    placement.params.surfaceDamage = 0.8;

    for (int mode = 0; mode < 4; ++mode) {
        placement.params.patternMode = mode;
        assert_true("runtime_material_texture_param_mode_sample_ok",
                    RuntimeMaterialTexture3D_SamplePlacedUV(&object,
                                                            0.37,
                                                            0.61,
                                                            123,
                                                            &placement,
                                                            &samples[mode]) ||
                        samples[mode].mask >= 0.0);
    }

    for (int mode = 1; mode < 4; ++mode) {
        diff_sum += fabs(samples[mode].mask - samples[0].mask);
    }
    assert_true("runtime_material_texture_param_modes_differ", diff_sum > 1e-5);
    assert_close("runtime_material_texture_param_color_depth",
                 samples[2].colorDepth,
                 0.8,
                 1e-12);
    assert_close("runtime_material_texture_param_surface_damage",
                 samples[2].surfaceDamage,
                 0.8,
                 1e-12);
    return 0;
}

static int test_runtime_material_payload_3d_surface_damage_controls_roughness(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialPayload3D low_damage = {0};
    RuntimeMaterialPayload3D high_damage = {0};
    HitInfo3D hit = {0};

    MaterialManagerResetDefaults();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 8.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].color = 0xB0B0B0;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_MIRROR;
    sceneSettings.sceneObjects[0].textureId = RUNTIME_MATERIAL_TEXTURE_3D_RUST;
    sceneSettings.sceneObjects[0].textureStrength = 1.0;
    sceneSettings.sceneObjects[0].textureScale = 1.0;
    sceneSettings.sceneObjects[0].texturePatternMode = RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_PATCH;
    sceneSettings.sceneObjects[0].textureCoverage = 1.0;
    sceneSettings.sceneObjects[0].textureEdgeSoftness = 1.0;
    sceneSettings.sceneObjects[0].textureContrast = 0.2;
    sceneSettings.sceneObjects[0].textureGrain = 0.4;
    sceneSettings.sceneObjects[0].textureColorDepth = 0.6;

    HitInfo3D_Reset(&hit);
    hit.sceneObjectIndex = 0;
    hit.triangleIndex = 9;
    hit.primitiveIndex = 1;
    hit.baryU = 0.22;
    hit.baryV = 0.33;
    hit.baryW = 0.45;

    sceneSettings.sceneObjects[0].textureSurfaceDamage = 0.05;
    assert_true("runtime_material_payload_low_damage_ok",
                RuntimeMaterialPayload3D_ResolveFromHit(&hit, &low_damage));
    sceneSettings.sceneObjects[0].textureSurfaceDamage = 1.0;
    assert_true("runtime_material_payload_high_damage_ok",
                RuntimeMaterialPayload3D_ResolveFromHit(&hit, &high_damage));

    assert_true("runtime_material_payload_damage_mask_active",
                high_damage.textureMask > 1e-9 && low_damage.textureMask > 1e-9);
    assert_true("runtime_material_payload_damage_roughness_increases",
                high_damage.bsdf.roughness > low_damage.bsdf.roughness);
    assert_true("runtime_material_payload_damage_reflectivity_reduces",
                high_damage.bsdf.reflectivity < low_damage.bsdf.reflectivity);

    sceneSettings = saved_scene;
    return 0;
}

static int test_runtime_principled_bsdf_normalizes_core_parameters(void) {
    RuntimePrincipledBSDF3D bsdf = RuntimePrincipledBSDF3D_Default();
    RuntimePrincipledBSDF3D normalized = {0};

    bsdf.baseColorR = 1.4;
    bsdf.baseColorG = -0.5;
    bsdf.baseColorB = 0.25;
    bsdf.metallic = 1.5;
    bsdf.roughness = 0.0;
    bsdf.specularWeight = 0.8;
    bsdf.diffuseWeight = 0.8;
    bsdf.reflectivity = 0.2;
    bsdf.ior = 1.5;
    bsdf.opacity = 1.2;
    bsdf.transmissionWeight = -1.0;

    normalized = RuntimePrincipledBSDF3D_Normalize(bsdf);

    assert_true("runtime_principled_bsdf_normalize_valid", normalized.valid);
    assert_close("runtime_principled_bsdf_normalize_color_r",
                 normalized.baseColorR,
                 1.0,
                 1e-12);
    assert_close("runtime_principled_bsdf_normalize_color_g",
                 normalized.baseColorG,
                 0.0,
                 1e-12);
    assert_close("runtime_principled_bsdf_normalize_roughness_floor",
                 normalized.roughness,
                 0.02,
                 1e-12);
    assert_close("runtime_principled_bsdf_normalize_metal_diffuse",
                 normalized.diffuseWeight,
                 0.0,
                 1e-12);
    assert_true("runtime_principled_bsdf_normalize_specular_f0_colored",
                normalized.specularF0R > normalized.specularF0G &&
                normalized.specularF0R > normalized.specularF0B);
    assert_close("runtime_principled_bsdf_dielectric_f0_ior15",
                 RuntimePrincipledBSDF3D_DielectricF0FromIor(1.5),
                 0.04,
                 1e-12);
    assert_close("runtime_principled_bsdf_fresnel_normal",
                 RuntimePrincipledBSDF3D_FresnelSchlick(1.0, 0.04),
                 0.04,
                 1e-12);
    assert_close("runtime_principled_bsdf_fresnel_grazing",
                 RuntimePrincipledBSDF3D_FresnelSchlick(0.0, 0.04),
                 1.0,
                 1e-12);

    return 0;
}

static int test_runtime_principled_bsdf_payload_adapter_preserves_material_signal(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialPayload3D payload = {0};
    RuntimePrincipledBSDF3D bsdf = {0};
    MaterialBSDF legacy = {0};

    MaterialManagerResetDefaults();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 8.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].color = 0x336699;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_GLOSSY;
    sceneSettings.sceneObjects[0].alpha = 0.70;
    sceneSettings.sceneObjects[0].opacity = 1.0;
    sceneSettings.sceneObjects[0].reflectivity = 0.35;
    sceneSettings.sceneObjects[0].roughness = 0.22;
    sceneSettings.sceneObjects[0].emissiveStrength = 0.4;

    assert_true("runtime_principled_bsdf_payload_resolve",
                RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(0, &payload));
    bsdf = RuntimePrincipledBSDF3D_FromMaterialPayload(&payload);
    legacy = RuntimePrincipledBSDF3D_ToMaterialBSDF(&bsdf);

    assert_true("runtime_principled_bsdf_payload_valid", bsdf.valid);
    assert_close("runtime_principled_bsdf_payload_color_r",
                 bsdf.baseColorR,
                 payload.baseColorR,
                 1e-12);
    assert_close("runtime_principled_bsdf_payload_roughness",
                 bsdf.roughness,
                 payload.bsdf.roughness,
                 1e-12);
    assert_close("runtime_principled_bsdf_payload_transmission",
                 bsdf.transmissionWeight,
                 payload.transparency,
                 1e-12);
    assert_true("runtime_principled_bsdf_payload_f0_reflectivity",
                bsdf.dielectricF0 >= payload.bsdf.reflectivity - 1e-12);
    assert_close("runtime_principled_bsdf_to_legacy_albedo",
                 legacy.albedo,
                 payload.bsdf.albedo,
                 1e-12);
    assert_close("runtime_principled_bsdf_to_legacy_roughness",
                 legacy.roughness,
                 payload.bsdf.roughness,
                 1e-12);

    sceneSettings = saved_scene;
    return 0;
}

static int test_runtime_principled_bsdf_surface_eval_adapter_and_lobes(void) {
    RuntimeMaterialSurfaceEval surface =
        RuntimeMaterialSurfaceEvalMakeBase(0.25, 0.50, 0.75, 0.30, 0.18, 0.45, 0.55, 0.20);
    RuntimePrincipledBSDF3D bsdf =
        RuntimePrincipledBSDF3D_FromSurfaceEval(&surface, 1.45, 0.2);
    double diffuse = RuntimePrincipledBSDF3D_EvaluateDiffuseCos(&bsdf, 0.8);
    double spec_smooth = RuntimePrincipledBSDF3D_EvaluateGGXSpecularCos(&bsdf, 0.8, 0.8, 0.9);
    double pdf = RuntimePrincipledBSDF3D_GGXHalfVectorPdf(&bsdf, 0.9, 0.8);

    assert_true("runtime_principled_bsdf_surface_valid", bsdf.valid);
    assert_close("runtime_principled_bsdf_surface_color_b",
                 bsdf.baseColorB,
                 0.75,
                 1e-12);
    assert_close("runtime_principled_bsdf_surface_transmission",
                 bsdf.transmissionWeight,
                 0.20,
                 1e-12);
    assert_true("runtime_principled_bsdf_surface_probabilities",
                RuntimePrincipledBSDF3D_DiffuseProbability(&bsdf) > 0.0 &&
                RuntimePrincipledBSDF3D_SpecularProbability(&bsdf) > 0.0);
    assert_true("runtime_principled_bsdf_surface_diffuse_positive", diffuse > 0.0);
    assert_true("runtime_principled_bsdf_surface_specular_positive", spec_smooth > 0.0);
    assert_true("runtime_principled_bsdf_surface_pdf_positive", pdf > 0.0);

    bsdf.roughness = 0.85;
    bsdf = RuntimePrincipledBSDF3D_Normalize(bsdf);
    assert_true("runtime_principled_bsdf_surface_rough_spec_changes",
                fabs(RuntimePrincipledBSDF3D_EvaluateGGXSpecularCos(&bsdf, 0.8, 0.8, 0.9) -
                     spec_smooth) > 1e-5);

    return 0;
}

static int test_runtime_material_texture_stack_legacy_object_adapter_contract(void) {
    SceneObject object;
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    bool ok = false;

    memset(&object, 0, sizeof(object));
    object.textureId = RUNTIME_MATERIAL_TEXTURE_3D_NONE;
    object.textureScale = 1.0;

    ok = RuntimeMaterialTextureStackBuildLegacyFromObject(&object, &stack);
    assert_true("runtime_material_texture_stack_solid_legacy_ok", ok);
    assert_true("runtime_material_texture_stack_solid_legacy_layer_count",
                stack.layerCount == 1);
    assert_true("runtime_material_texture_stack_solid_legacy_active_count",
                RuntimeMaterialTextureStackActiveLayerCount(&stack) == 1);
    assert_true("runtime_material_texture_stack_solid_legacy_base_kind",
                stack.layers[0].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID);
    assert_true("runtime_material_texture_stack_solid_legacy_base_role",
                stack.layers[0].role == RUNTIME_MATERIAL_TEXTURE_LAYER_ROLE_BASE);
    assert_true("runtime_material_texture_stack_solid_legacy_base_id",
                strcmp(stack.layers[0].layerId, "solid") == 0);

    object.textureId = RUNTIME_MATERIAL_TEXTURE_3D_RUST;
    object.textureOffsetU = 0.25;
    object.textureOffsetV = 0.75;
    object.textureScale = 3.5;
    object.textureStrength = 0.65;
    object.texturePatternMode = RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_FLOW;
    object.textureCoverage = 0.8;
    object.textureGrain = 0.2;
    object.textureEdgeSoftness = 0.4;
    object.textureContrast = 0.6;
    object.textureFlow = 0.9;
    object.textureColorDepth = 0.7;
    object.textureSurfaceDamage = 0.55;
    object.textureSeed = 42;

    ok = RuntimeMaterialTextureStackBuildLegacyFromObject(&object, &stack);
    assert_true("runtime_material_texture_stack_rust_legacy_ok", ok);
    assert_true("runtime_material_texture_stack_rust_legacy_layer_count",
                stack.layerCount == 2);
    assert_true("runtime_material_texture_stack_rust_legacy_overlay_kind",
                stack.layers[1].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST);
    assert_true("runtime_material_texture_stack_rust_legacy_overlay_role",
                stack.layers[1].role == RUNTIME_MATERIAL_TEXTURE_LAYER_ROLE_OVERLAY);
    assert_true("runtime_material_texture_stack_rust_legacy_overlay_blend",
                stack.layers[1].blendMode ==
                    RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_OVERLAY_DAMAGE);
    assert_true("runtime_material_texture_stack_rust_legacy_overlay_id",
                strcmp(stack.layers[1].layerId, "rust") == 0);
    assert_close("runtime_material_texture_stack_rust_legacy_offset_u",
                 stack.layers[1].placement.offsetU,
                 0.25,
                 1e-9);
    assert_close("runtime_material_texture_stack_rust_legacy_offset_v",
                 stack.layers[1].placement.offsetV,
                 0.75,
                 1e-9);
    assert_close("runtime_material_texture_stack_rust_legacy_scale",
                 stack.layers[1].placement.scale,
                 3.5,
                 1e-9);
    assert_close("runtime_material_texture_stack_rust_legacy_strength",
                 stack.layers[1].placement.strength,
                 0.65,
                 1e-9);
    assert_true("runtime_material_texture_stack_rust_legacy_texture_id",
                stack.layers[1].placement.textureId == RUNTIME_MATERIAL_TEXTURE_3D_RUST);
    assert_true("runtime_material_texture_stack_rust_legacy_pattern",
                stack.layers[1].params.patternMode == RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_FLOW);
    assert_true("runtime_material_texture_stack_rust_legacy_seed",
                stack.layers[1].params.seed == 42);

    return 0;
}

static int test_runtime_material_texture_stack_normalizes_bounds_contract(void) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();

    stack.layerCount = RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS + 4;
    stack.layers[0] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD);
    stack.layers[1] =
        RuntimeMaterialTextureLayerMakeOverlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL);
    stack.layers[1].opacity = 2.0;
    stack.layers[1].placement.scale = 0.0;
    stack.layers[1].placement.strength = -0.5;
    stack.layers[1].roughnessInfluence = -2.0;
    stack.layers[1].specularInfluence = 2.0;
    stack.layers[1].layerId[0] = '\0';
    stack.layers[1].displayName[0] = '\0';

    stack = RuntimeMaterialTextureStackNormalize(stack);
    assert_true("runtime_material_texture_stack_normalize_count_clamped",
                stack.layerCount == RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS);
    assert_true("runtime_material_texture_stack_normalize_base_kind",
                stack.layers[0].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD);
    assert_true("runtime_material_texture_stack_normalize_base_replace",
                stack.layers[0].blendMode == RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_REPLACE);
    assert_true("runtime_material_texture_stack_normalize_overlay_kind",
                stack.layers[1].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL);
    assert_true("runtime_material_texture_stack_normalize_overlay_id",
                strcmp(stack.layers[1].layerId, "oil") == 0);
    assert_close("runtime_material_texture_stack_normalize_opacity",
                 stack.layers[1].opacity,
                 1.0,
                 1e-9);
    assert_close("runtime_material_texture_stack_normalize_scale",
                 stack.layers[1].placement.scale,
                 1.0,
                 1e-9);
    assert_close("runtime_material_texture_stack_normalize_strength",
                 stack.layers[1].placement.strength,
                 0.0,
                 1e-9);
    assert_close("runtime_material_texture_stack_normalize_roughness_influence",
                 stack.layers[1].roughnessInfluence,
                 -1.0,
                 1e-9);
    assert_close("runtime_material_texture_stack_normalize_specular_influence",
                 stack.layers[1].specularInfluence,
                 1.0,
                 1e-9);

    return 0;
}

static int test_runtime_material_texture_stack_surface_eval_applies_rust_response(void) {
    SceneObject object;
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialSurfaceEval base_eval =
        RuntimeMaterialSurfaceEvalMakeBase(0.25, 0.25, 0.25, 0.10, 0.80, 0.80, 0.20, 0.0);
    RuntimeMaterialSurfaceEval surface_eval = {0};
    bool found_active = false;

    memset(&object, 0, sizeof(object));
    object.textureId = RUNTIME_MATERIAL_TEXTURE_3D_RUST;
    object.textureScale = 4.0;
    object.textureStrength = 1.0;
    object.textureCoverage = 1.0;
    object.textureGrain = 0.5;
    object.textureEdgeSoftness = 1.0;
    object.textureContrast = 0.5;
    object.textureColorDepth = 1.0;
    object.textureSurfaceDamage = 1.0;
    object.texturePatternMode = RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_PATCH;

    assert_true("runtime_material_texture_stack_surface_eval_builds",
                RuntimeMaterialTextureStackBuildLegacyFromObject(&object, &stack));

    for (int i = 0; i < 16 && !found_active; ++i) {
        double u = 0.07 + (double)i * 0.053;
        double v = 0.19 + (double)i * 0.037;
        memset(&surface_eval, 0, sizeof(surface_eval));
        found_active = RuntimeMaterialTextureStackEvaluatePlacedUV(&stack,
                                                                   &object,
                                                                   u,
                                                                   v,
                                                                   19,
                                                                   &base_eval,
                                                                   &surface_eval);
    }

    assert_true("runtime_material_texture_stack_surface_eval_active", found_active);
    assert_true("runtime_material_texture_stack_surface_eval_mask",
                surface_eval.textureMask > 1e-9);
    assert_true("runtime_material_texture_stack_surface_eval_layer_mask",
                surface_eval.layerMasks[1] > 1e-9);
    assert_true("runtime_material_texture_stack_surface_eval_rust_color",
                surface_eval.colorR > base_eval.colorR &&
                surface_eval.colorB < base_eval.colorB);
    assert_true("runtime_material_texture_stack_surface_eval_roughness",
                surface_eval.roughness > base_eval.roughness);
    assert_true("runtime_material_texture_stack_surface_eval_reflectivity",
                surface_eval.reflectivity < base_eval.reflectivity);
    assert_true("runtime_material_texture_stack_surface_eval_spec",
                surface_eval.specWeight < base_eval.specWeight);

    return 0;
}

static double runtime_material_test_surface_color_delta(RuntimeMaterialSurfaceEval a,
                                                        RuntimeMaterialSurfaceEval b) {
    return fabs(a.colorR - b.colorR) + fabs(a.colorG - b.colorG) + fabs(a.colorB - b.colorB);
}

static int test_runtime_material_texture_stack_base_patterns_change_surface_response(void) {
    SceneObject object;
    RuntimeMaterialTextureLayerKind kinds[] = {
        RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRUSHED_METAL,
        RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD,
        RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRICK,
        RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_CONCRETE,
        RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_STONE
    };
    const char* labels[] = {
        "runtime_material_texture_stack_base_brushed_metal_active",
        "runtime_material_texture_stack_base_wood_active",
        "runtime_material_texture_stack_base_brick_active",
        "runtime_material_texture_stack_base_concrete_active",
        "runtime_material_texture_stack_base_stone_active"
    };
    RuntimeMaterialSurfaceEval base_eval =
        RuntimeMaterialSurfaceEvalMakeBase(0.18, 0.24, 0.31, 0.24, 0.18, 0.28, 0.72, 0.0);

    memset(&object, 0, sizeof(object));

    for (int i = 0; i < 5; ++i) {
        RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
        RuntimeMaterialSurfaceEval surface_eval = {0};
        bool active = false;
        double delta = 0.0;
        stack.layerCount = 1;
        stack.layers[0] = RuntimeMaterialTextureLayerMakeBase(kinds[i]);
        stack.layers[0].placement.scale = 3.25;
        stack.layers[0].placement.strength = 1.0;
        stack.layers[0].params.grain = 0.68;
        stack.layers[0].params.colorDepth = 0.90;
        stack.layers[0].params.flow = 0.35;

        active = RuntimeMaterialTextureStackEvaluatePlacedUV(&stack,
                                                             &object,
                                                             0.37,
                                                             0.42,
                                                             71,
                                                             &base_eval,
                                                             &surface_eval);
        delta = runtime_material_test_surface_color_delta(surface_eval, base_eval);
        assert_true(labels[i], active);
        assert_true("runtime_material_texture_stack_base_layer_mask",
                    surface_eval.layerMasks[0] > 0.999);
        assert_true("runtime_material_texture_stack_base_color_changes", delta > 0.04);
        assert_true("runtime_material_texture_stack_base_response_changes",
                    fabs(surface_eval.roughness - base_eval.roughness) > 0.02 ||
                    fabs(surface_eval.reflectivity - base_eval.reflectivity) > 0.02 ||
                    fabs(surface_eval.specWeight - base_eval.specWeight) > 0.02);
    }

    return 0;
}

static int test_runtime_material_texture_stack_base_patterns_are_repeat_stable(void) {
    SceneObject object;
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialSurfaceEval base_eval =
        RuntimeMaterialSurfaceEvalMakeBase(0.20, 0.20, 0.20, 0.45, 0.10, 0.20, 0.80, 0.0);
    RuntimeMaterialSurfaceEval eval_a = {0};
    RuntimeMaterialSurfaceEval eval_b = {0};
    RuntimeMaterialSurfaceEval eval_c = {0};

    memset(&object, 0, sizeof(object));
    stack.layerCount = 1;
    stack.layers[0] = RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD);
    stack.layers[0].placement.scale = 4.0;
    stack.layers[0].params.seed = 77;
    stack.layers[0].params.grain = 0.58;
    stack.layers[0].params.flow = 0.22;

    assert_true("runtime_material_texture_stack_base_repeat_a",
                RuntimeMaterialTextureStackEvaluatePlacedUV(&stack,
                                                            &object,
                                                            0.13,
                                                            0.27,
                                                            5,
                                                            &base_eval,
                                                            &eval_a));
    assert_true("runtime_material_texture_stack_base_repeat_b",
                RuntimeMaterialTextureStackEvaluatePlacedUV(&stack,
                                                            &object,
                                                            0.13,
                                                            0.27,
                                                            5,
                                                            &base_eval,
                                                            &eval_b));
    assert_true("runtime_material_texture_stack_base_repeat_c",
                RuntimeMaterialTextureStackEvaluatePlacedUV(&stack,
                                                            &object,
                                                            0.71,
                                                            0.62,
                                                            5,
                                                            &base_eval,
                                                            &eval_c));

    assert_close("runtime_material_texture_stack_base_repeat_r",
                 eval_a.colorR,
                 eval_b.colorR,
                 1e-12);
    assert_close("runtime_material_texture_stack_base_repeat_g",
                 eval_a.colorG,
                 eval_b.colorG,
                 1e-12);
    assert_close("runtime_material_texture_stack_base_repeat_bv",
                 eval_a.colorB,
                 eval_b.colorB,
                 1e-12);
    assert_close("runtime_material_texture_stack_base_repeat_roughness",
                 eval_a.roughness,
                 eval_b.roughness,
                 1e-12);
    assert_true("runtime_material_texture_stack_base_repeat_uv_variance",
                runtime_material_test_surface_color_delta(eval_a, eval_c) > 1e-5);

    return 0;
}

static int test_runtime_material_texture_stack_base_then_overlay_orders_response(void) {
    SceneObject object;
    RuntimeMaterialTextureStack wood_stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialTextureStack stacked = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialSurfaceEval base_eval =
        RuntimeMaterialSurfaceEvalMakeBase(0.20, 0.20, 0.20, 0.30, 0.20, 0.30, 0.70, 0.0);
    RuntimeMaterialSurfaceEval wood_eval = {0};
    RuntimeMaterialSurfaceEval stacked_eval = {0};
    bool found_overlay = false;

    memset(&object, 0, sizeof(object));
    wood_stack.layerCount = 1;
    wood_stack.layers[0] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD);
    wood_stack.layers[0].placement.scale = 3.5;
    wood_stack.layers[0].params.grain = 0.52;
    wood_stack.layers[0].params.colorDepth = 0.85;

    stacked = wood_stack;
    stacked.layerCount = 2;
    stacked.layers[1] =
        RuntimeMaterialTextureLayerMakeOverlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST);
    stacked.layers[1].placement.scale = 5.0;
    stacked.layers[1].placement.strength = 1.0;
    stacked.layers[1].params.coverage = 1.0;
    stacked.layers[1].params.grain = 0.45;
    stacked.layers[1].params.edgeSoftness = 1.0;
    stacked.layers[1].params.contrast = 0.45;
    stacked.layers[1].params.colorDepth = 1.0;
    stacked.layers[1].params.surfaceDamage = 1.0;
    stacked.layers[1].params.patternMode = RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_PATCH;

    for (int i = 0; i < 16 && !found_overlay; ++i) {
        double u = 0.11 + ((double)i * 0.047);
        double v = 0.21 + ((double)i * 0.039);
        memset(&wood_eval, 0, sizeof(wood_eval));
        memset(&stacked_eval, 0, sizeof(stacked_eval));
        RuntimeMaterialTextureStackEvaluatePlacedUV(&wood_stack,
                                                    &object,
                                                    u,
                                                    v,
                                                    37,
                                                    &base_eval,
                                                    &wood_eval);
        RuntimeMaterialTextureStackEvaluatePlacedUV(&stacked,
                                                    &object,
                                                    u,
                                                    v,
                                                    37,
                                                    &base_eval,
                                                    &stacked_eval);
        found_overlay = stacked_eval.layerMasks[1] > 1e-9;
    }

    assert_true("runtime_material_texture_stack_base_overlay_layer_active", found_overlay);
    assert_true("runtime_material_texture_stack_base_overlay_roughens",
                stacked_eval.roughness > wood_eval.roughness);
    assert_true("runtime_material_texture_stack_base_overlay_reduces_reflectivity",
                stacked_eval.reflectivity < wood_eval.reflectivity);
    assert_true("runtime_material_texture_stack_base_overlay_changes_color",
                runtime_material_test_surface_color_delta(stacked_eval, wood_eval) > 0.02);

    return 0;
}

static RuntimeMaterialTextureLayer runtime_material_test_make_strong_overlay(
    RuntimeMaterialTextureLayerKind kind,
    double scale) {
    RuntimeMaterialTextureLayer layer = RuntimeMaterialTextureLayerMakeOverlay(kind);
    layer.placement.scale = scale;
    layer.placement.strength = 1.0;
    layer.params.coverage = 1.0;
    layer.params.grain = 0.55;
    layer.params.edgeSoftness = 1.0;
    layer.params.contrast = 0.45;
    layer.params.flow = 0.65;
    layer.params.colorDepth = 1.0;
    layer.params.surfaceDamage = 1.0;
    return layer;
}

static int test_runtime_material_texture_stack_grime_and_oil_overlay_responses(void) {
    SceneObject object;
    RuntimeMaterialTextureStack grime_stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialTextureStack oil_stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialSurfaceEval base_eval =
        RuntimeMaterialSurfaceEvalMakeBase(0.54, 0.48, 0.38, 0.38, 0.18, 0.16, 0.80, 0.0);
    RuntimeMaterialSurfaceEval grime_eval = {0};
    RuntimeMaterialSurfaceEval oil_eval = {0};
    bool found_grime = false;
    bool found_oil = false;

    memset(&object, 0, sizeof(object));
    grime_stack.layerCount = 2;
    grime_stack.layers[0] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID);
    grime_stack.layers[1] =
        runtime_material_test_make_strong_overlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME, 4.0);
    oil_stack.layerCount = 2;
    oil_stack.layers[0] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID);
    oil_stack.layers[1] =
        runtime_material_test_make_strong_overlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL, 3.0);

    for (int i = 0; i < 24 && (!found_grime || !found_oil); ++i) {
        double u = 0.09 + ((double)i * 0.041);
        double v = 0.17 + ((double)i * 0.035);
        if (!found_grime) {
            memset(&grime_eval, 0, sizeof(grime_eval));
            RuntimeMaterialTextureStackEvaluatePlacedUV(&grime_stack,
                                                        &object,
                                                        u,
                                                        v,
                                                        43,
                                                        &base_eval,
                                                        &grime_eval);
            found_grime = grime_eval.layerMasks[1] > 1e-9;
        }
        if (!found_oil) {
            memset(&oil_eval, 0, sizeof(oil_eval));
            RuntimeMaterialTextureStackEvaluatePlacedUV(&oil_stack,
                                                        &object,
                                                        u,
                                                        v,
                                                        43,
                                                        &base_eval,
                                                        &oil_eval);
            found_oil = oil_eval.layerMasks[1] > 1e-9;
        }
    }

    assert_true("runtime_material_texture_stack_grime_overlay_active", found_grime);
    assert_true("runtime_material_texture_stack_grime_darkens",
                grime_eval.colorR + grime_eval.colorG + grime_eval.colorB <
                    base_eval.colorR + base_eval.colorG + base_eval.colorB);
    assert_true("runtime_material_texture_stack_grime_roughens",
                grime_eval.roughness > base_eval.roughness);
    assert_true("runtime_material_texture_stack_grime_reduces_reflectivity",
                grime_eval.reflectivity < base_eval.reflectivity);
    assert_true("runtime_material_texture_stack_grime_reduces_spec",
                grime_eval.specWeight < base_eval.specWeight);

    assert_true("runtime_material_texture_stack_oil_overlay_active", found_oil);
    assert_true("runtime_material_texture_stack_oil_lowers_roughness",
                oil_eval.roughness < base_eval.roughness);
    assert_true("runtime_material_texture_stack_oil_raises_reflectivity",
                oil_eval.reflectivity > base_eval.reflectivity);
    assert_true("runtime_material_texture_stack_oil_raises_spec",
                oil_eval.specWeight > base_eval.specWeight);
    assert_close("runtime_material_texture_stack_oil_preserves_opaque_base",
                 oil_eval.transparency,
                 0.0,
                 1e-9);

    return 0;
}

static int test_runtime_material_texture_stack_grime_oil_order_changes_result(void) {
    SceneObject object;
    RuntimeMaterialTextureStack grime_then_oil = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialTextureStack oil_then_grime = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialSurfaceEval base_eval =
        RuntimeMaterialSurfaceEvalMakeBase(0.46, 0.42, 0.34, 0.42, 0.16, 0.18, 0.82, 0.0);
    RuntimeMaterialSurfaceEval eval_a = {0};
    RuntimeMaterialSurfaceEval eval_b = {0};
    bool found_both = false;

    memset(&object, 0, sizeof(object));
    grime_then_oil.layerCount = 3;
    grime_then_oil.layers[0] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD);
    grime_then_oil.layers[0].placement.scale = 2.0;
    grime_then_oil.layers[1] =
        runtime_material_test_make_strong_overlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME, 3.6);
    grime_then_oil.layers[2] =
        runtime_material_test_make_strong_overlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL, 3.6);

    oil_then_grime = grime_then_oil;
    oil_then_grime.layers[1] = grime_then_oil.layers[2];
    oil_then_grime.layers[2] = grime_then_oil.layers[1];

    for (int i = 0; i < 24 && !found_both; ++i) {
        double u = 0.12 + ((double)i * 0.037);
        double v = 0.23 + ((double)i * 0.029);
        memset(&eval_a, 0, sizeof(eval_a));
        memset(&eval_b, 0, sizeof(eval_b));
        RuntimeMaterialTextureStackEvaluatePlacedUV(&grime_then_oil,
                                                    &object,
                                                    u,
                                                    v,
                                                    59,
                                                    &base_eval,
                                                    &eval_a);
        RuntimeMaterialTextureStackEvaluatePlacedUV(&oil_then_grime,
                                                    &object,
                                                    u,
                                                    v,
                                                    59,
                                                    &base_eval,
                                                    &eval_b);
        found_both = eval_a.layerMasks[1] > 1e-9 &&
                     eval_a.layerMasks[2] > 1e-9 &&
                     eval_b.layerMasks[1] > 1e-9 &&
                     eval_b.layerMasks[2] > 1e-9;
    }

    assert_true("runtime_material_texture_stack_grime_oil_order_both_active", found_both);
    assert_true("runtime_material_texture_stack_grime_oil_order_changes_color",
                runtime_material_test_surface_color_delta(eval_a, eval_b) > 1e-5 ||
                    fabs(eval_a.roughness - eval_b.roughness) > 1e-5 ||
                    fabs(eval_a.specWeight - eval_b.specWeight) > 1e-5);

    return 0;
}

static int test_runtime_material_texture_stack_overlays_do_not_reopen_transparency(void) {
    SceneObject object;
    RuntimeMaterialTextureStack rust_then_oil = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialSurfaceEval transparent_base =
        RuntimeMaterialSurfaceEvalMakeBase(0.72, 0.76, 0.82, 0.16, 0.12, 0.24, 0.76, 0.75);
    RuntimeMaterialSurfaceEval eval = {0};
    bool found_both = false;

    memset(&object, 0, sizeof(object));
    rust_then_oil.layerCount = 3;
    rust_then_oil.layers[0] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID);
    rust_then_oil.layers[1] =
        runtime_material_test_make_strong_overlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST, 3.4);
    rust_then_oil.layers[2] =
        runtime_material_test_make_strong_overlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL, 3.4);

    for (int i = 0; i < 24 && !found_both; ++i) {
        double u = 0.08 + ((double)i * 0.039);
        double v = 0.19 + ((double)i * 0.031);
        memset(&eval, 0, sizeof(eval));
        RuntimeMaterialTextureStackEvaluatePlacedUV(&rust_then_oil,
                                                    &object,
                                                    u,
                                                    v,
                                                    71,
                                                    &transparent_base,
                                                    &eval);
        found_both = eval.layerMasks[1] > 1e-9 && eval.layerMasks[2] > 1e-9;
    }

    assert_true("runtime_material_texture_stack_rust_oil_both_active", found_both);
    assert_true("runtime_material_texture_stack_rust_oil_stays_within_base_transparency",
                eval.transparency <= transparent_base.transparency + 1e-9);
    assert_true("runtime_material_texture_stack_rust_oil_never_reopens_rust_damage",
                eval.transparency < transparent_base.transparency - 0.05);

    return 0;
}

static int test_runtime_material_payload_uses_object_v2_stack_override(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialPayload3D payload = {0};
    HitInfo3D hit = {0};
    bool ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    SceneEditorMaterialFacePlacementResetAll();
    SceneEditorMaterialStackResetAll();
    MaterialManagerResetDefaults();
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_POLYGON, 0.0, 0.0, 1.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].color = 0x505050;
    sceneSettings.sceneObjects[0].textureId = RUNTIME_MATERIAL_TEXTURE_3D_NONE;
    sceneSettings.sceneObjects[0].textureStrength = 0.0;
    sceneSettings.sceneObjects[0].reflectivity = 0.10;
    sceneSettings.sceneObjects[0].roughness = 0.70;

    stack.layerCount = 2;
    stack.layers[0] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD);
    stack.layers[0].placement.scale = 2.0;
    stack.layers[1] =
        runtime_material_test_make_strong_overlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL, 3.0);
    assert_true("runtime_material_payload_v2_stack_set",
                SceneEditorMaterialStackSetObjectStack(0, &stack));

    hit.sceneObjectIndex = 0;
    hit.triangleIndex = 2;
    hit.localTriangleIndex = 2;
    hit.primitiveIndex = 0;
    hit.baryU = 0.24;
    hit.baryV = 0.31;
    hit.baryW = 0.45;
    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &payload);

    assert_true("runtime_material_payload_v2_stack_ok", ok);
    assert_true("runtime_material_payload_v2_stack_mask", payload.textureMask > 1e-9);
    assert_true("runtime_material_payload_v2_stack_oil_reflective",
                payload.bsdf.reflectivity > 0.10);
    assert_true("runtime_material_payload_v2_stack_oil_glossy",
                payload.bsdf.roughness < 0.70);

    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialFacePlacementResetAll();
    sceneSettings = saved_scene;
    return 0;
}

static int test_runtime_material_payload_face_override_targets_v2_layer_id_after_reorder(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialPayload3D baseline = {0};
    RuntimeMaterialPayload3D face_oil = {0};
    SceneEditorMaterialFacePlacement placement = {0};
    HitInfo3D hit = {0};
    bool ok = false;
    bool found_oil = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    SceneEditorMaterialFacePlacementResetAll();
    SceneEditorMaterialStackResetAll();
    MaterialManagerResetDefaults();
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_POLYGON, 0.0, 0.0, 1.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].color = 0x505050;
    sceneSettings.sceneObjects[0].textureId = RUNTIME_MATERIAL_TEXTURE_3D_NONE;
    sceneSettings.sceneObjects[0].textureStrength = 0.0;
    sceneSettings.sceneObjects[0].reflectivity = 0.10;
    sceneSettings.sceneObjects[0].roughness = 0.70;

    stack.layerCount = 3;
    stack.layers[0] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID);
    stack.layers[1] =
        runtime_material_test_make_strong_overlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME, 3.0);
    stack.layers[1].placement.strength = 0.0;
    stack.layers[2] =
        runtime_material_test_make_strong_overlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL, 3.0);
    stack.layers[2].placement.strength = 0.0;
    assert_true("runtime_material_payload_face_layer_id_stack_set",
                SceneEditorMaterialStackSetObjectStack(0, &stack));

    HitInfo3D_Reset(&hit);
    hit.sceneObjectIndex = 0;
    hit.triangleIndex = 2;
    hit.localTriangleIndex = 2;
    hit.primitiveIndex = 0;
    hit.baryU = 0.24;
    hit.baryV = 0.31;
    hit.baryW = 0.45;
    ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &baseline);
    assert_true("runtime_material_payload_face_layer_id_baseline_ok", ok);

    placement.hasOverride = true;
    placement.sceneObjectIndex = 0;
    placement.faceGroupIndex = 1;
    placement.layerIndex = 1;
    snprintf(placement.layerId, sizeof(placement.layerId), "%s", stack.layers[2].layerId);
    placement.textureId = RUNTIME_MATERIAL_TEXTURE_3D_RUST;
    placement.scale = 3.0;
    placement.strength = 1.0;
    placement.params.coverage = 1.0;
    placement.params.grain = 0.55;
    placement.params.edgeSoftness = 1.0;
    placement.params.contrast = 0.45;
    placement.params.flow = 0.65;
    placement.params.colorDepth = 1.0;
    placement.params.surfaceDamage = 1.0;
    assert_true("runtime_material_payload_face_layer_id_override_set",
                SceneEditorMaterialFacePlacementSetOverride(&placement));

    for (int u = 1; u < 9 && !found_oil; ++u) {
        for (int v = 1; v < 9 && !found_oil; ++v) {
            double bary_v = (double)u / 10.0;
            double bary_w = (double)v / 10.0;
            if (bary_v + bary_w >= 0.95) continue;
            hit.baryV = bary_v;
            hit.baryW = bary_w;
            hit.baryU = 1.0 - bary_v - bary_w;
            ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &face_oil);
            assert_true("runtime_material_payload_face_layer_id_retry_ok", ok);
            found_oil = face_oil.textureMask > baseline.textureMask + 1e-9 &&
                        face_oil.bsdf.reflectivity > baseline.bsdf.reflectivity + 1e-6 &&
                        face_oil.bsdf.roughness < baseline.bsdf.roughness - 1e-6;
        }
    }

    assert_true("runtime_material_payload_face_layer_id_oil_applied", found_oil);
    assert_true("runtime_material_payload_face_layer_id_durable",
                SceneEditorMaterialFacePlacementHasOverrideForLayer(0, 1, "oil"));
    assert_true("runtime_material_payload_face_layer_id_not_grime",
                !SceneEditorMaterialFacePlacementHasOverrideForLayer(0, 1, "grime"));

    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialFacePlacementResetAll();
    sceneSettings = saved_scene;
    return 0;
}

static int test_runtime_material_config_loads_v2_stack_schema(void) {
    SceneConfig saved_scene = sceneSettings;
    json_object* root = json_object_new_object();
    json_object* objects = json_object_new_array();
    json_object* obj = json_object_new_object();
    json_object* stack_obj = json_object_new_object();
    json_object* layers = json_object_new_array();
    json_object* face_placements = json_object_new_array();
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();

    SceneEditorMaterialStackResetAll();
    json_object_object_add(obj, "type", json_object_new_string("rectangle"));
    json_object_object_add(obj, "x", json_object_new_double(0.0));
    json_object_object_add(obj, "y", json_object_new_double(0.0));
    json_object_object_add(obj, "scale", json_object_new_double(1.0));
    json_object_object_add(obj, "color", json_object_new_int(0x303030));
    json_object_object_add(obj, "materialId", json_object_new_int(MaterialManagerDefaultId()));

    {
        json_object* base = json_object_new_object();
        json_object_object_add(base, "kind", json_object_new_string("wood"));
        json_object_object_add(base, "role", json_object_new_string("base"));
        json_object_array_add(layers, base);
    }
    {
        json_object* overlay = json_object_new_object();
        json_object* placement = json_object_new_object();
        json_object* parameters = json_object_new_object();
        json_object_object_add(placement, "scale", json_object_new_double(3.0));
        json_object_object_add(placement, "strength", json_object_new_double(1.0));
        json_object_object_add(parameters, "coverage", json_object_new_double(1.0));
        json_object_object_add(parameters, "colorDepth", json_object_new_double(1.0));
        json_object_object_add(parameters, "surfaceDamage", json_object_new_double(1.0));
        json_object_object_add(overlay, "kind", json_object_new_string("grime"));
        json_object_object_add(overlay, "role", json_object_new_string("overlay"));
        json_object_object_add(overlay, "placement", placement);
        json_object_object_add(overlay, "parameters", parameters);
        json_object_array_add(layers, overlay);
    }
    json_object_object_add(stack_obj, "version", json_object_new_int(1));
    json_object_object_add(stack_obj, "layers", layers);
    json_object_object_add(obj, "materialTextureStack", stack_obj);
    {
        json_object* face_entry = json_object_new_object();
        json_object_object_add(face_entry, "faceGroupIndex", json_object_new_int(3));
        json_object_object_add(face_entry, "layerIndex", json_object_new_int(1));
        json_object_object_add(face_entry, "layerId", json_object_new_string("grime"));
        json_object_object_add(face_entry, "textureId", json_object_new_int(RUNTIME_MATERIAL_TEXTURE_3D_RUST));
        json_object_object_add(face_entry, "scale", json_object_new_double(2.5));
        json_object_object_add(face_entry, "strength", json_object_new_double(0.65));
        json_object_array_add(face_placements, face_entry);
    }
    json_object_object_add(obj, "materialFacePlacements", face_placements);
    json_object_array_add(objects, obj);
    json_object_object_add(root, "objects", objects);

    LoadSceneObjects(root);

    assert_true("runtime_material_config_v2_stack_loaded",
                SceneEditorMaterialStackGetObjectStack(0, &stack));
    assert_true("runtime_material_config_v2_stack_layer_count", stack.layerCount == 2);
    assert_true("runtime_material_config_v2_stack_base_kind",
                stack.layers[0].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD);
    assert_true("runtime_material_config_v2_stack_overlay_kind",
                stack.layers[1].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME);
    assert_close("runtime_material_config_v2_stack_overlay_scale",
                 stack.layers[1].placement.scale,
                 3.0,
                 1e-9);
    assert_true("runtime_material_config_v2_face_layer_loaded",
                SceneEditorMaterialFacePlacementHasOverrideForLayer(0, 3, "grime"));
    {
        SceneEditorMaterialFacePlacement placement =
            SceneEditorMaterialFacePlacementGetEffectiveForLayer(&sceneSettings.sceneObjects[0],
                                                                 0,
                                                                 3,
                                                                 "grime");
        assert_true("runtime_material_config_v2_face_layer_index",
                    placement.layerIndex == 1);
        assert_close("runtime_material_config_v2_face_layer_strength",
                     placement.strength,
                     0.65,
                     1e-9);
    }

    json_object_put(root);
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialFacePlacementResetAll();
    sceneSettings = saved_scene;
    return 0;
}

int run_test_runtime_lighting_materials_payload_suite(void) {
    test_runtime_material_payload_3d_scene_object_resolution_contract();
    test_runtime_material_payload_3d_authoring_object_values_override_preset();
    test_runtime_material_payload_3d_object_multipliers_contract();
    test_runtime_material_payload_3d_water_override_contract();
    test_material_manager_default_presets_include_i4_entries();
    test_material_manager_load_dir_preserves_shipped_preset_ids();
    test_material_manager_transparent_override_inherits_shipped_defaults();
    test_runtime_material_payload_3d_hit_resolution_contract();
    test_runtime_material_payload_3d_rust_texture_is_hit_anchored();
    test_runtime_material_payload_3d_face_texture_override_affects_hit();
    test_runtime_material_payload_3d_authored_texture_override_affects_hit();
    test_runtime_material_payload_authored_texture_becomes_base_but_keeps_overlay_stack();
    test_runtime_material_payload_authored_texture_metadata_contract();
    test_runtime_material_payload_authored_texture_explicit_summary_overrides_layer_list();
    test_runtime_material_payload_authored_texture_face_base_intent_modulates_bsdf();
    test_runtime_material_payload_authored_texture_face_overlay_intent_overrides_binding_default();
    test_runtime_material_payload_3d_fog_texture_roughens_transparency();
    test_runtime_material_texture_3d_uv_sampler_matches_hit_sampler();
    test_runtime_material_texture_3d_rust_parameter_modes_change_masks();
    test_runtime_material_payload_3d_surface_damage_controls_roughness();
    test_runtime_principled_bsdf_normalizes_core_parameters();
    test_runtime_principled_bsdf_payload_adapter_preserves_material_signal();
    test_runtime_principled_bsdf_surface_eval_adapter_and_lobes();
    test_runtime_material_texture_stack_legacy_object_adapter_contract();
    test_runtime_material_texture_stack_normalizes_bounds_contract();
    test_runtime_material_texture_stack_surface_eval_applies_rust_response();
    test_runtime_material_texture_stack_base_patterns_change_surface_response();
    test_runtime_material_texture_stack_base_patterns_are_repeat_stable();
    test_runtime_material_texture_stack_base_then_overlay_orders_response();
    test_runtime_material_texture_stack_grime_and_oil_overlay_responses();
    test_runtime_material_texture_stack_grime_oil_order_changes_result();
    test_runtime_material_texture_stack_overlays_do_not_reopen_transparency();
    test_runtime_material_payload_uses_object_v2_stack_override();
    test_runtime_material_payload_face_override_targets_v2_layer_id_after_reorder();
    test_runtime_material_config_loads_v2_stack_schema();
    return 0;
}
