#include <math.h>
#include <png.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app/animation.h"
#include "config/config_manager.h"
#include "config/scene/config_scene_material_persistence.h"
#include "editor/scene_editor_material_graph.h"
#include "editor/scene_editor_runtime_scene_persistence.h"
#include "editor/material_preview_surface_eval.h"
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
#include "render/runtime_material_graph_3d.h"
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

static bool runtime_material_test_write_text_file(const char* path, const char* text) {
    FILE* file = NULL;
    if (!path || !text) return false;
    file = fopen(path, "wb");
    if (!file) return false;
    if (fwrite(text, 1u, strlen(text), file) != strlen(text)) {
        fclose(file);
        return false;
    }
    return fclose(file) == 0;
}

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

static bool runtime_material_payload_test_write_authored_texture_manifest_with_channels(
    const char* manifest_path,
    const char* object_id,
    const char* primitive_kind,
    const char* face_role,
    const char* color_file_name,
    const char* roughness_file_name) {
    json_object* root = NULL;
    json_object* base_surfaces = NULL;
    json_object* base_surface = NULL;
    json_object* channels = NULL;
    json_object* color_channel = NULL;
    json_object* roughness_channel = NULL;
    int write_ok = 0;
    if (!manifest_path || !object_id || !primitive_kind || !face_role || !color_file_name ||
        !roughness_file_name) {
        return false;
    }
    root = json_object_new_object();
    base_surfaces = json_object_new_array();
    base_surface = json_object_new_object();
    channels = json_object_new_array();
    color_channel = json_object_new_object();
    roughness_channel = json_object_new_object();
    if (!root || !base_surfaces || !base_surface || !channels || !color_channel ||
        !roughness_channel) {
        if (root) json_object_put(root);
        if (base_surfaces) json_object_put(base_surfaces);
        if (base_surface) json_object_put(base_surface);
        if (channels) json_object_put(channels);
        if (color_channel) json_object_put(color_channel);
        if (roughness_channel) json_object_put(roughness_channel);
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
    json_object_object_add(base_surface, "file_name", json_object_new_string(color_file_name));
    if (!runtime_material_payload_test_add_surface_semantic_fields(base_surface,
                                                                   primitive_kind,
                                                                   face_role,
                                                                   NULL,
                                                                   NULL,
                                                                   NULL)) {
        json_object_put(root);
        return false;
    }
    json_object_object_add(color_channel,
                           "channel",
                           json_object_new_string("base_color.rgb"));
    json_object_object_add(color_channel, "source", json_object_new_string("rgba"));
    json_object_object_add(color_channel, "file_name", json_object_new_string(color_file_name));
    json_object_array_add(channels, color_channel);
    json_object_object_add(roughness_channel,
                           "channel",
                           json_object_new_string("roughness.scalar"));
    json_object_object_add(roughness_channel, "source", json_object_new_string("luminance"));
    json_object_object_add(roughness_channel,
                           "file_name",
                           json_object_new_string(roughness_file_name));
    json_object_array_add(channels, roughness_channel);
    json_object_object_add(base_surface, "material_channels", channels);
    json_object_array_add(base_surfaces, base_surface);
    json_object_object_add(root, "base_surfaces", base_surfaces);
    write_ok = json_object_to_file_ext(manifest_path, root, JSON_C_TO_STRING_PRETTY);
    json_object_put(root);
    return write_ok == 0;
}

static bool runtime_material_payload_test_write_authored_texture_manifest_with_single_channel(
    const char* manifest_path,
    const char* object_id,
    const char* primitive_kind,
    const char* face_role,
    const char* file_name,
    const char* channel,
    const char* source) {
    json_object* root = NULL;
    json_object* base_surfaces = NULL;
    json_object* base_surface = NULL;
    json_object* channels = NULL;
    json_object* channel_ref = NULL;
    int write_ok = 0;
    if (!manifest_path || !object_id || !primitive_kind || !face_role || !file_name ||
        !channel || !source) {
        return false;
    }
    root = json_object_new_object();
    base_surfaces = json_object_new_array();
    base_surface = json_object_new_object();
    channels = json_object_new_array();
    channel_ref = json_object_new_object();
    if (!root || !base_surfaces || !base_surface || !channels || !channel_ref) {
        if (root) json_object_put(root);
        if (base_surfaces) json_object_put(base_surfaces);
        if (base_surface) json_object_put(base_surface);
        if (channels) json_object_put(channels);
        if (channel_ref) json_object_put(channel_ref);
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
    json_object_object_add(channel_ref, "channel", json_object_new_string(channel));
    json_object_object_add(channel_ref, "source", json_object_new_string(source));
    json_object_object_add(channel_ref, "file_name", json_object_new_string(file_name));
    json_object_array_add(channels, channel_ref);
    json_object_object_add(base_surface, "material_channels", channels);
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

static int test_runtime_material_compatibility_bridges_are_explicit(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialPayload3D payload = {0};
    RuntimeMaterialSurfaceEval preview_eval = {0};
    RuntimePrincipledBSDF3D principled = RuntimePrincipledBSDF3D_Default();
    const Material* transparent = NULL;
    const Material* glossy = NULL;
    double expected_transparency = 0.0;
    double expected_spec = 0.0;
    double expected_diffuse = 0.0;
    double expected_total = 0.0;

    MaterialManagerResetDefaults();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 2;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 10.0, 0.0, NULL, 0);
    InitObject(&sceneSettings.sceneObjects[1], OBJECT_CIRCLE, 5.0, -2.0, 6.0, 0.0, NULL, 0);

    transparent = MaterialManagerGet(MATERIAL_PRESET_TRANSPARENT);
    assert_true("runtime_material_bridge_transparent_preset_present", transparent != NULL);
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[0].alpha = 0.25;
    expected_transparency = transparent->transparency * 0.25;
    assert_true("runtime_material_bridge_payload_transparency_ok",
                RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(0, &payload));
    assert_true("runtime_material_bridge_preview_transparency_ok",
                MaterialPreviewSurfaceEvaluateObject(&sceneSettings.sceneObjects[0],
                                                     0,
                                                     NULL,
                                                     0.35,
                                                     0.45,
                                                     &preview_eval));
    assert_close("runtime_material_bridge_payload_legacy_alpha_transparency",
                 payload.transparency,
                 expected_transparency,
                 1e-9);
    assert_close("runtime_material_bridge_preview_legacy_alpha_transparency",
                 preview_eval.transparency,
                 expected_transparency,
                 1e-9);

    glossy = MaterialManagerGet(MATERIAL_PRESET_GLOSSY);
    assert_true("runtime_material_bridge_glossy_preset_present", glossy != NULL);
    sceneSettings.sceneObjects[1].material_id = MATERIAL_PRESET_GLOSSY;
    sceneSettings.sceneObjects[1].color = 0x3366CC;
    sceneSettings.sceneObjects[1].reflectivity = 0.52;
    sceneSettings.sceneObjects[1].roughness = 0.19;
    expected_spec = glossy->specular + sceneSettings.sceneObjects[1].reflectivity;
    expected_diffuse = glossy->diffuse;
    expected_total = expected_spec + expected_diffuse;
    if (expected_total > 1.0) {
        expected_spec /= expected_total;
        expected_diffuse /= expected_total;
    }
    assert_true("runtime_material_bridge_payload_object_preset_ok",
                RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(1, &payload));
    assert_close("runtime_material_bridge_object_reflectivity_authoritative",
                 payload.bsdf.reflectivity,
                 sceneSettings.sceneObjects[1].reflectivity,
                 1e-9);
    assert_close("runtime_material_bridge_preset_ior_fallback",
                 payload.opticalIor,
                 glossy->ior,
                 1e-9);
    assert_close("runtime_material_bridge_specular_fallback",
                 payload.bsdf.specWeight,
                 expected_spec,
                 1e-9);
    assert_close("runtime_material_bridge_diffuse_fallback",
                 payload.bsdf.diffuseWeight,
                 expected_diffuse,
                 1e-9);

    principled.ior = 1.33;
    principled.reflectivity = 0.18;
    principled.specularWeight = 0.25;
    principled.dielectricF0 = 0.0;
    principled = RuntimePrincipledBSDF3D_Normalize(principled);
    assert_close("runtime_material_bridge_reflectivity_f0_floor",
                 principled.dielectricF0,
                 0.18,
                 1e-12);
    assert_close("runtime_material_bridge_reflectivity_floor_scales_specular",
                 principled.specularF0R,
                 0.18 * 0.25,
                 1e-12);

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

static int test_runtime_material_payload_authored_texture_channel_references(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeMaterialAuthoredTextureChannelRef channels[4];
    RuntimeMaterialAuthoredTextureFaceMetadata metadata;
    char channel_summary[160];
    const char* dir = "/tmp/ray_tracing_authored_texture_channel_refs";
    const char* color_png_path = "/tmp/ray_tracing_authored_texture_channel_refs/plane_color.png";
    const char* roughness_png_path =
        "/tmp/ray_tracing_authored_texture_channel_refs/plane_roughness.png";
    const char* manifest_path =
        "/tmp/ray_tracing_authored_texture_channel_refs/plane_manifest.json";
    const char* manifest_rel = "plane_manifest.json";
    const unsigned char color_rgba[4] = {180u, 90u, 40u, 255u};
    const unsigned char roughness_rgba[4] = {64u, 64u, 64u, 255u};
    int channel_count = 0;

    memset(channels, 0, sizeof(channels));
    memset(&metadata, 0, sizeof(metadata));
    memset(channel_summary, 0, sizeof(channel_summary));
    (void)mkdir(dir, 0775);
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    RuntimeMaterialAuthoredTextureResetAll();
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 8.0, 0.0, NULL, 0);
    snprintf(animSettings.runtimeScenePath,
             sizeof(animSettings.runtimeScenePath),
             "%s/scene_runtime.json",
             dir);
    assert_true("runtime_material_payload_channel_refs_color_png_write",
                runtime_material_payload_test_write_png_rgba(color_png_path,
                                                             color_rgba,
                                                             1u,
                                                             1u));
    assert_true("runtime_material_payload_channel_refs_roughness_png_write",
                runtime_material_payload_test_write_png_rgba(roughness_png_path,
                                                             roughness_rgba,
                                                             1u,
                                                             1u));
    assert_true("runtime_material_payload_channel_refs_manifest_write",
                runtime_material_payload_test_write_authored_texture_manifest_with_channels(
                    manifest_path,
                    "channel_plane",
                    "PLANE",
                    "FRONT",
                    "plane_color.png",
                    "plane_roughness.png"));
    assert_true("runtime_material_payload_channel_refs_bind_ok",
                RuntimeMaterialAuthoredTextureBindManifestForObject(0,
                                                                   "channel_plane",
                                                                   manifest_rel,
                                                                   "override"));
    assert_true("runtime_material_payload_channel_refs_metadata_ok",
                RuntimeMaterialAuthoredTextureGetFaceMetadata(0, 0, &metadata));
    assert_true("runtime_material_payload_channel_refs_metadata_count",
                metadata.channelRefCount == 2);
    assert_true("runtime_material_payload_channel_refs_get_ok",
                RuntimeMaterialAuthoredTextureGetFaceChannels(0,
                                                              0,
                                                              channels,
                                                              4u,
                                                              &channel_count));
    assert_true("runtime_material_payload_channel_refs_count", channel_count == 2);
    assert_true("runtime_material_payload_channel_refs_visual_supported",
                RuntimeMaterialAuthoredTextureChannelNameSupported(channels[0].channel) &&
                    RuntimeMaterialAuthoredTextureChannelIsVisual(channels[0].channel) &&
                    strcmp(channels[0].channel, "base_color.rgb") == 0 &&
                    strcmp(channels[0].source, "rgba") == 0 &&
                    strcmp(channels[0].fileName, "plane_color.png") == 0);
    assert_true("runtime_material_payload_channel_refs_physical_supported",
                RuntimeMaterialAuthoredTextureChannelNameSupported(channels[1].channel) &&
                    RuntimeMaterialAuthoredTextureChannelIsPhysicalScalar(channels[1].channel) &&
                    strcmp(channels[1].channel, "roughness.scalar") == 0 &&
                    strcmp(channels[1].source, "luminance") == 0 &&
                    strcmp(channels[1].fileName, "plane_roughness.png") == 0);
    assert_true("runtime_material_payload_channel_refs_summary_ok",
                RuntimeMaterialAuthoredTextureGetChannelSummary(0,
                                                                channel_summary,
                                                                sizeof(channel_summary)));
    assert_true("runtime_material_payload_channel_refs_summary_visual",
                strstr(channel_summary, "base_color.rgb") != NULL);
    assert_true("runtime_material_payload_channel_refs_summary_physical",
                strstr(channel_summary, "roughness.scalar") != NULL);

    RuntimeMaterialAuthoredTextureResetAll();
    unlink(color_png_path);
    unlink(roughness_png_path);
    unlink(manifest_path);
    rmdir(dir);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_material_payload_authored_texture_no_displacement_guard(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char* dir = "/tmp/ray_tracing_authored_texture_displacement_guard";
    const char* png_path = "/tmp/ray_tracing_authored_texture_displacement_guard/height.png";
    const char* manifest_path =
        "/tmp/ray_tracing_authored_texture_displacement_guard/plane_manifest.json";
    const char* manifest_rel = "plane_manifest.json";
    const unsigned char rgba[4] = {128u, 128u, 128u, 255u};

    (void)mkdir(dir, 0775);
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    RuntimeMaterialAuthoredTextureResetAll();
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0.0, 0.0, 8.0, 0.0, NULL, 0);
    snprintf(animSettings.runtimeScenePath,
             sizeof(animSettings.runtimeScenePath),
             "%s/scene_runtime.json",
             dir);

    assert_true("runtime_material_no_displacement_normal_supported",
                RuntimeMaterialAuthoredTextureChannelNameSupported("normal.tangent"));
    assert_true("runtime_material_no_displacement_normal_shading_only",
                RuntimeMaterialAuthoredTextureChannelIsShadingNormal("normal.tangent") &&
                    !RuntimeMaterialAuthoredTextureChannelIsVisual("normal.tangent") &&
                    !RuntimeMaterialAuthoredTextureChannelIsPhysicalScalar("normal.tangent") &&
                    !RuntimeMaterialAuthoredTextureChannelIsDisplacement("normal.tangent"));
    assert_true("runtime_material_no_displacement_bump_supported",
                RuntimeMaterialAuthoredTextureChannelNameSupported("bump.height"));
    assert_true("runtime_material_no_displacement_bump_shading_only",
                RuntimeMaterialAuthoredTextureChannelIsShadingNormal("bump.height") &&
                    !RuntimeMaterialAuthoredTextureChannelIsVisual("bump.height") &&
                    !RuntimeMaterialAuthoredTextureChannelIsPhysicalScalar("bump.height") &&
                    !RuntimeMaterialAuthoredTextureChannelIsDisplacement("bump.height"));
    assert_true("runtime_material_no_displacement_channel_deferred",
                !RuntimeMaterialAuthoredTextureChannelNameSupported("displacement.height") &&
                    RuntimeMaterialAuthoredTextureChannelIsDisplacement("displacement.height") &&
                    !RuntimeMaterialAuthoredTextureChannelIsShadingNormal("displacement.height"));

    assert_true("runtime_material_no_displacement_png_write",
                runtime_material_payload_test_write_png_rgba(png_path, rgba, 1u, 1u));
    assert_true("runtime_material_no_displacement_manifest_write",
                runtime_material_payload_test_write_authored_texture_manifest_with_single_channel(
                    manifest_path,
                    "displacement_guard_plane",
                    "PLANE",
                    "FRONT",
                    "height.png",
                    "displacement.height",
                    "luminance"));
    assert_true("runtime_material_no_displacement_manifest_rejected",
                !RuntimeMaterialAuthoredTextureBindManifestForObject(0,
                                                                     "displacement_guard_plane",
                                                                     manifest_rel,
                                                                     "override"));
    assert_true("runtime_material_no_displacement_invalid_recorded",
                RuntimeMaterialAuthoredTextureGetInvalidBinding(0, NULL, 0u, NULL, 0u, NULL, 0u));

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

static int test_runtime_principled_bsdf_conversion_adapter_m2_contract(void) {
    RuntimeMaterialPayload3D payload = {0};
    RuntimeMaterialSurfaceEval surface =
        RuntimeMaterialSurfaceEvalMakeBase(0.90, 0.10, 0.05, 0.34, 0.015, 0.25, 0.70, 0.40);
    RuntimePrincipledBSDF3D payload_bsdf = {0};
    RuntimePrincipledBSDF3D surface_bsdf = {0};
    RuntimePrincipledBSDF3D floor_bsdf = {0};
    double payload_f0 = RuntimePrincipledBSDF3D_DielectricF0FromIor(1.33);
    double surface_f0 = RuntimePrincipledBSDF3D_DielectricF0FromIor(1.45);

    payload.valid = true;
    payload.baseColorR = 0.80;
    payload.baseColorG = 0.20;
    payload.baseColorB = 0.10;
    payload.transparency = 0.40;
    payload.opticalIor = 1.33;
    payload.emissive = 0.0;
    payload.bsdf.baseColorR = 0.10;
    payload.bsdf.baseColorG = 0.10;
    payload.bsdf.baseColorB = 0.10;
    payload.bsdf.roughness = 0.34;
    payload.bsdf.reflectivity = 0.015;
    payload.bsdf.ior = 1.50;
    payload.bsdf.opacity = 0.25;
    payload.bsdf.specWeight = 0.25;
    payload.bsdf.diffuseWeight = 0.70;

    payload_bsdf = RuntimePrincipledBSDF3D_FromMaterialPayload(&payload);

    assert_close("runtime_principled_bsdf_adapter_payload_ior",
                 payload_bsdf.ior,
                 1.33,
                 1e-12);
    assert_close("runtime_principled_bsdf_adapter_payload_ior_f0",
                 payload_bsdf.dielectricF0,
                 payload_f0,
                 1e-12);
    assert_close("runtime_principled_bsdf_adapter_payload_opacity_bridge",
                 payload_bsdf.opacity,
                 0.60,
                 1e-12);
    assert_close("runtime_principled_bsdf_adapter_payload_transmission_bridge",
                 payload_bsdf.transmissionWeight,
                 0.40,
                 1e-12);
    assert_close("runtime_principled_bsdf_adapter_payload_no_metallic",
                 payload_bsdf.metallic,
                 0.0,
                 1e-12);
    assert_close("runtime_principled_bsdf_adapter_payload_specular_scales_f0_r",
                 payload_bsdf.specularF0R,
                 payload_f0 * 0.25,
                 1e-12);
    assert_close("runtime_principled_bsdf_adapter_payload_specular_scales_f0_g",
                 payload_bsdf.specularF0G,
                 payload_f0 * 0.25,
                 1e-12);
    assert_close("runtime_principled_bsdf_adapter_payload_diffuse_transmission_scaled",
                 payload_bsdf.diffuseWeight,
                 0.70 * 0.60,
                 1e-12);

    surface_bsdf = RuntimePrincipledBSDF3D_FromSurfaceEval(&surface, 1.45, 0.0);
    assert_close("runtime_principled_bsdf_adapter_surface_ior_f0",
                 surface_bsdf.dielectricF0,
                 surface_f0,
                 1e-12);
    assert_close("runtime_principled_bsdf_adapter_surface_opacity_bridge",
                 surface_bsdf.opacity,
                 0.60,
                 1e-12);
    assert_close("runtime_principled_bsdf_adapter_surface_no_metallic",
                 surface_bsdf.metallic,
                 0.0,
                 1e-12);
    assert_close("runtime_principled_bsdf_adapter_surface_specular_neutral_r",
                 surface_bsdf.specularF0R,
                 surface_f0 * 0.25,
                 1e-12);
    assert_close("runtime_principled_bsdf_adapter_surface_specular_neutral_b",
                 surface_bsdf.specularF0B,
                 surface_f0 * 0.25,
                 1e-12);

    payload.bsdf.reflectivity = 0.18;
    floor_bsdf = RuntimePrincipledBSDF3D_FromMaterialPayload(&payload);
    assert_close("runtime_principled_bsdf_adapter_reflectivity_floor",
                 floor_bsdf.dielectricF0,
                 0.18,
                 1e-12);
    assert_close("runtime_principled_bsdf_adapter_floor_specular_scales_f0",
                 floor_bsdf.specularF0R,
                 0.18 * 0.25,
                 1e-12);

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
    stack.layers[2] =
        RuntimeMaterialTextureLayerMakeOverlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL);
    snprintf(stack.layers[2].layerId, sizeof(stack.layers[2].layerId), "%s", "oil");
    stack.layers[3] =
        RuntimeMaterialTextureLayerMakeOverlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME);
    snprintf(stack.layers[3].layerId, sizeof(stack.layers[3].layerId), "%s", "grime_custom");
    stack.layers[4] =
        RuntimeMaterialTextureLayerMakeOverlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME);
    snprintf(stack.layers[4].layerId, sizeof(stack.layers[4].layerId), "%s", "grime_custom");

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
    assert_true("runtime_material_texture_stack_normalize_duplicate_kind_id",
                strcmp(stack.layers[2].layerId, "oil_2") == 0);
    assert_true("runtime_material_texture_stack_normalize_duplicate_custom_id",
                strcmp(stack.layers[4].layerId, "grime_custom_4") == 0);
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

static void runtime_material_test_assert_surface_matches_payload(
    const char* label,
    const RuntimeMaterialSurfaceEval* eval,
    const RuntimeMaterialPayload3D* payload) {
    char name[160];
    if (!label || !eval || !payload) {
        assert_true("runtime_material_surface_payload_compare_args", false);
        return;
    }

    snprintf(name, sizeof(name), "%s_color_r", label);
    assert_close(name, eval->colorR, payload->baseColorR, 1e-9);
    snprintf(name, sizeof(name), "%s_color_g", label);
    assert_close(name, eval->colorG, payload->baseColorG, 1e-9);
    snprintf(name, sizeof(name), "%s_color_b", label);
    assert_close(name, eval->colorB, payload->baseColorB, 1e-9);
    snprintf(name, sizeof(name), "%s_roughness", label);
    assert_close(name, eval->roughness, payload->bsdf.roughness, 1e-9);
    snprintf(name, sizeof(name), "%s_reflectivity", label);
    assert_close(name, eval->reflectivity, payload->bsdf.reflectivity, 1e-9);
    snprintf(name, sizeof(name), "%s_spec_weight", label);
    assert_close(name, eval->specWeight, payload->bsdf.specWeight, 1e-9);
    snprintf(name, sizeof(name), "%s_diffuse_weight", label);
    assert_close(name, eval->diffuseWeight, payload->bsdf.diffuseWeight, 1e-9);
    snprintf(name, sizeof(name), "%s_transparency", label);
    assert_close(name, eval->transparency, payload->transparency, 1e-9);
    snprintf(name, sizeof(name), "%s_texture_mask", label);
    assert_close(name, eval->textureMask, payload->textureMask, 1e-9);
    snprintf(name, sizeof(name), "%s_texture_u", label);
    assert_close(name, eval->textureU, payload->textureU, 1e-9);
    snprintf(name, sizeof(name), "%s_texture_v", label);
    assert_close(name, eval->textureV, payload->textureV, 1e-9);
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

static int test_runtime_material_graph_compiles_stack_and_channels(void) {
    RuntimeMaterialGraphDocument graph = RuntimeMaterialGraphDocumentMake("m7_s1_fixture");
    RuntimeMaterialGraphDocument invalid_graph =
        RuntimeMaterialGraphDocumentMake("m7_s1_invalid_fixture");
    RuntimeMaterialTextureLayer base =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD);
    RuntimeMaterialTextureLayer oil =
        runtime_material_test_make_strong_overlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL, 2.25);
    RuntimeMaterialGraphCompileResult compiled = {0};
    RuntimeMaterialGraphCompileResult compiled_again = {0};
    RuntimeMaterialGraphCompileResult invalid_result = {0};

    snprintf(base.layerId, sizeof(base.layerId), "%s", "graph_base_wood");
    snprintf(oil.layerId, sizeof(oil.layerId), "%s", "graph_oil_overlay");
    oil.params.coverage = 0.58;
    oil.params.seed = 41;

    assert_true("runtime_material_graph_add_base",
                RuntimeMaterialGraphDocumentAddNode(
                    &graph,
                    RuntimeMaterialGraphNodeMakeLayer("base_node", base)));
    assert_true("runtime_material_graph_add_overlay",
                RuntimeMaterialGraphDocumentAddNode(
                    &graph,
                    RuntimeMaterialGraphNodeMakeLayer("oil_node", oil)));
    assert_true("runtime_material_graph_add_roughness_channel",
                RuntimeMaterialGraphDocumentAddNode(
                    &graph,
                    RuntimeMaterialGraphNodeMakeChannelOutput("roughness_output",
                                                              "roughness.scalar",
                                                              "luminance",
                                                              "roughness.png")));
    assert_true("runtime_material_graph_rejects_duplicate_node",
                !RuntimeMaterialGraphDocumentAddNode(
                    &graph,
                    RuntimeMaterialGraphNodeMakeChannelOutput("roughness_output",
                                                              "specular.weight",
                                                              "luminance",
                                                              "specular.png")));

    assert_true("runtime_material_graph_compile_ok",
                RuntimeMaterialGraphCompileToStack(&graph, &compiled));
    assert_true("runtime_material_graph_compile_again_ok",
                RuntimeMaterialGraphCompileToStack(&graph, &compiled_again));
    assert_true("runtime_material_graph_compile_active", compiled.active);
    assert_true("runtime_material_graph_stack_layer_count",
                compiled.stack.layerCount == 2);
    assert_true("runtime_material_graph_stack_order_base",
                compiled.stack.layers[0].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD &&
                    strcmp(compiled.stack.layers[0].layerId, "graph_base_wood") == 0);
    assert_true("runtime_material_graph_stack_order_overlay",
                compiled.stack.layers[1].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL &&
                    strcmp(compiled.stack.layers[1].layerId, "graph_oil_overlay") == 0);
    assert_close("runtime_material_graph_overlay_coverage",
                 compiled.stack.layers[1].params.coverage,
                 0.58,
                 1e-9);
    assert_true("runtime_material_graph_channel_count", compiled.channelRefCount == 1);
    assert_true("runtime_material_graph_channel_is_physical_scalar",
                strcmp(compiled.channelRefs[0].channel, "roughness.scalar") == 0 &&
                    RuntimeMaterialAuthoredTextureChannelIsPhysicalScalar(
                        compiled.channelRefs[0].channel) &&
                    !RuntimeMaterialAuthoredTextureChannelIsVisual(
                        compiled.channelRefs[0].channel));
    assert_true("runtime_material_graph_channel_source_stable",
                strcmp(compiled.channelRefs[0].source, "luminance") == 0 &&
                    strcmp(compiled.channelRefs[0].fileName, "roughness.png") == 0);
    assert_true("runtime_material_graph_compile_deterministic_layers",
                compiled_again.stack.layerCount == compiled.stack.layerCount &&
                    strcmp(compiled_again.stack.layers[0].layerId,
                           compiled.stack.layers[0].layerId) == 0 &&
                    strcmp(compiled_again.stack.layers[1].layerId,
                           compiled.stack.layers[1].layerId) == 0 &&
                    compiled_again.stack.layers[1].params.seed ==
                        compiled.stack.layers[1].params.seed);
    assert_true("runtime_material_graph_compile_deterministic_channels",
                compiled_again.channelRefCount == compiled.channelRefCount &&
                    strcmp(compiled_again.channelRefs[0].channel,
                           compiled.channelRefs[0].channel) == 0);

    assert_true("runtime_material_graph_add_deferred_displacement",
                RuntimeMaterialGraphDocumentAddNode(
                    &invalid_graph,
                    RuntimeMaterialGraphNodeMakeChannelOutput("displacement_output",
                                                              "displacement.height",
                                                              "height",
                                                              "height.png")));
    assert_true("runtime_material_graph_rejects_deferred_displacement",
                !RuntimeMaterialGraphCompileToStack(&invalid_graph, &invalid_result));
    assert_true("runtime_material_graph_deferred_displacement_inactive",
                !invalid_result.active &&
                    RuntimeMaterialAuthoredTextureChannelIsDisplacement(
                        "displacement.height"));

    return 0;
}

static RuntimeMaterialGraphDocument runtime_material_test_make_graph_document(
    const char* graph_id) {
    RuntimeMaterialGraphDocument graph = RuntimeMaterialGraphDocumentMake(graph_id);
    RuntimeMaterialTextureLayer base =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD);
    RuntimeMaterialTextureLayer oil =
        runtime_material_test_make_strong_overlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL, 2.25);
    snprintf(base.layerId, sizeof(base.layerId), "%s", "graph_base_wood");
    snprintf(oil.layerId, sizeof(oil.layerId), "%s", "graph_oil_overlay");
    oil.params.coverage = 0.58;
    oil.params.seed = 41;
    (void)RuntimeMaterialGraphDocumentAddNode(
        &graph,
        RuntimeMaterialGraphNodeMakeLayer("base_node", base));
    (void)RuntimeMaterialGraphDocumentAddNode(
        &graph,
        RuntimeMaterialGraphNodeMakeLayer("oil_node", oil));
    (void)RuntimeMaterialGraphDocumentAddNode(
        &graph,
        RuntimeMaterialGraphNodeMakeChannelOutput("roughness_output",
                                                  "roughness.scalar",
                                                  "luminance",
                                                  "roughness.png"));
    return graph;
}

static int test_runtime_material_graph_scene_config_round_trip_stack_fallback(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialGraphDocument graph =
        runtime_material_test_make_graph_document("m7_s2_config_graph");
    RuntimeMaterialGraphDocument loaded_graph = RuntimeMaterialGraphDocumentEmpty();
    RuntimeMaterialGraphCompileResult compile_result = {0};
    RuntimeMaterialTextureStack saved_stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialTextureStack loaded_stack = RuntimeMaterialTextureStackEmpty();
    json_object* root = json_object_new_object();
    json_object* objects = json_object_new_array();
    json_object* obj = json_object_new_object();
    json_object* material_graph = NULL;
    json_object* material_stack = NULL;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[0],
               OBJECT_POLYGON,
               0.0,
               0.0,
               1.0,
               0.0,
               NULL,
               0);
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialGraphResetAll();
    assert_true("runtime_material_graph_config_set_graph",
                SceneEditorMaterialGraphSetObjectGraph(0, &graph, &compile_result));
    assert_true("runtime_material_graph_config_stack_fallback_set",
                SceneEditorMaterialStackGetObjectStack(0, &saved_stack));

    material_graph = ConfigSaveMaterialGraphForObject(0);
    material_stack = ConfigSaveMaterialTextureStackForObject(0);
    assert_true("runtime_material_graph_config_saved_graph", material_graph != NULL);
    assert_true("runtime_material_graph_config_saved_stack_fallback", material_stack != NULL);

    json_object_object_add(obj, "type", json_object_new_string("rectangle"));
    json_object_object_add(obj, "x", json_object_new_double(0.0));
    json_object_object_add(obj, "y", json_object_new_double(0.0));
    json_object_object_add(obj, "scale", json_object_new_double(1.0));
    json_object_object_add(obj, "color", json_object_new_int(0x303030));
    json_object_object_add(obj, "materialId", json_object_new_int(MaterialManagerDefaultId()));
    json_object_object_add(obj, "materialGraph", material_graph);
    json_object_object_add(obj, "materialTextureStack", material_stack);
    json_object_array_add(objects, obj);
    json_object_object_add(root, "objects", objects);

    LoadSceneObjects(root);
    assert_true("runtime_material_graph_config_loaded_graph",
                SceneEditorMaterialGraphGetObjectGraph(0, &loaded_graph));
    assert_true("runtime_material_graph_config_loaded_stack_fallback",
                SceneEditorMaterialStackGetObjectStack(0, &loaded_stack));
    assert_true("runtime_material_graph_config_graph_id",
                strcmp(loaded_graph.graphId, "m7_s2_config_graph") == 0);
    assert_true("runtime_material_graph_config_layer_count",
                loaded_stack.layerCount == saved_stack.layerCount &&
                    loaded_stack.layerCount == 2);
    assert_true("runtime_material_graph_config_layer_ids",
                strcmp(loaded_stack.layers[0].layerId, "graph_base_wood") == 0 &&
                    strcmp(loaded_stack.layers[1].layerId, "graph_oil_overlay") == 0);
    assert_close("runtime_material_graph_config_overlay_coverage",
                 loaded_stack.layers[1].params.coverage,
                 0.58,
                 1e-9);

    json_object_put(root);
    SceneEditorMaterialGraphResetAll();
    SceneEditorMaterialStackResetAll();
    sceneSettings = saved_scene;
    return 0;
}

static int test_runtime_material_graph_runtime_scene_round_trip_stack_fallback(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char* runtime_path = "/tmp/ray_tracing_material_graph_runtime_scene.json";
    const char* runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"material_graph_runtime\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[{"
          "\"object_id\":\"graph_box\","
          "\"object_type\":\"rect_prism_primitive\","
          "\"transform\":{"
            "\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
            "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}"
          "},"
          "\"primitive\":{\"kind\":\"rect_prism_primitive\",\"width\":1.0,\"height\":1.0,\"depth\":1.0},"
          "\"flags\":{\"visible\":true}"
        "}],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{\"ray_tracing\":{\"authoring\":{\"object_materials\":[{"
          "\"object_id\":\"graph_box\","
          "\"material_id\":0"
        "}]}}}"
        "}";
    RuntimeMaterialGraphDocument graph =
        runtime_material_test_make_graph_document("m7_s2_runtime_graph");
    RuntimeMaterialGraphDocument loaded_graph = RuntimeMaterialGraphDocumentEmpty();
    RuntimeMaterialGraphCompileResult compile_result = {0};
    RuntimeMaterialTextureStack loaded_stack = RuntimeMaterialTextureStackEmpty();
    RuntimeSceneBridgePreflight summary = {0};
    char diagnostics[256];

    assert_true("runtime_material_graph_runtime_write",
                runtime_material_test_write_text_file(runtime_path, runtime_json));
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialGraphResetAll();
    assert_true("runtime_material_graph_runtime_apply",
                runtime_scene_bridge_apply_file(runtime_path, &summary));
    assert_true("runtime_material_graph_runtime_set_graph",
                SceneEditorMaterialGraphSetObjectGraph(0, &graph, &compile_result));
    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    snprintf(animSettings.runtimeScenePath,
             sizeof(animSettings.runtimeScenePath),
             "%s",
             runtime_path);
    assert_true("runtime_material_graph_runtime_persist",
                SceneEditorRuntimeScenePersistAuthoring(diagnostics, sizeof(diagnostics)));

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialGraphResetAll();
    assert_true("runtime_material_graph_runtime_reload",
                runtime_scene_bridge_apply_file(runtime_path, &summary));
    assert_true("runtime_material_graph_runtime_loaded_graph",
                SceneEditorMaterialGraphGetObjectGraph(0, &loaded_graph));
    assert_true("runtime_material_graph_runtime_loaded_stack_fallback",
                SceneEditorMaterialStackGetObjectStack(0, &loaded_stack));
    assert_true("runtime_material_graph_runtime_graph_id",
                strcmp(loaded_graph.graphId, "m7_s2_runtime_graph") == 0);
    assert_true("runtime_material_graph_runtime_stack_layers",
                loaded_stack.layerCount == 2 &&
                    strcmp(loaded_stack.layers[0].layerId, "graph_base_wood") == 0 &&
                    strcmp(loaded_stack.layers[1].layerId, "graph_oil_overlay") == 0);

    unlink(runtime_path);
    SceneEditorMaterialGraphResetAll();
    SceneEditorMaterialStackResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_material_graph_evaluator_parity_matches_authored_stack(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialGraphDocument graph =
        runtime_material_test_make_graph_document("m7_s3_evaluator_graph");
    RuntimeMaterialGraphCompileResult compile_result = {0};
    RuntimeMaterialTextureStack authored_stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialSurfaceEval stack_preview = {0};
    RuntimeMaterialSurfaceEval graph_preview = {0};
    RuntimeMaterialPayload3D stack_payload = {0};
    RuntimeMaterialPayload3D graph_payload = {0};
    HitInfo3D hit = {0};
    SceneObject* object = NULL;
    const int scene_object_index = 0;
    const double sample_u = 0.18;
    const double sample_v = 0.31;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialGraphResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    RuntimeWaterMaterial3D_ClearAll();
    MaterialManagerResetDefaults();

    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[scene_object_index],
               OBJECT_CIRCLE,
               0.0,
               0.0,
               8.0,
               0.0,
               NULL,
               0);
    object = &sceneSettings.sceneObjects[scene_object_index];
    object->color = 0x6c5a42;
    object->alpha = 1.0;
    object->roughness = 0.46;
    object->reflectivity = 0.12;
    object->textureId = RUNTIME_MATERIAL_TEXTURE_3D_NONE;
    object->textureStrength = 0.0;
    object->textureSeed = 0;

    assert_true("runtime_material_graph_parity_compile",
                RuntimeMaterialGraphCompileToStack(&graph, &compile_result));
    authored_stack = compile_result.stack;
    assert_true("runtime_material_graph_parity_stack_set",
                SceneEditorMaterialStackSetObjectStack(scene_object_index, &authored_stack));
    assert_true("runtime_material_graph_parity_stack_preview",
                MaterialPreviewSurfaceEvaluateObject(object,
                                                     scene_object_index,
                                                     NULL,
                                                     sample_u,
                                                     sample_v,
                                                     &stack_preview));
    memset(&hit, 0, sizeof(hit));
    hit.sceneObjectIndex = scene_object_index;
    hit.hasObjectTextureCoord = true;
    hit.objectTextureCoord.x = sample_u;
    hit.objectTextureCoord.y = sample_v;
    hit.objectTextureCoord.z = 0.0;
    hit.normal.x = 0.0;
    hit.normal.y = 0.0;
    hit.normal.z = 1.0;
    assert_true("runtime_material_graph_parity_stack_payload",
                RuntimeMaterialPayload3D_ResolveFromHit(&hit, &stack_payload));

    SceneEditorMaterialStackResetAll();
    assert_true("runtime_material_graph_parity_graph_set",
                SceneEditorMaterialGraphSetObjectGraph(scene_object_index,
                                                       &graph,
                                                       &compile_result));
    assert_true("runtime_material_graph_parity_graph_preview",
                MaterialPreviewSurfaceEvaluateObject(object,
                                                     scene_object_index,
                                                     NULL,
                                                     sample_u,
                                                     sample_v,
                                                     &graph_preview));
    assert_true("runtime_material_graph_parity_graph_payload",
                RuntimeMaterialPayload3D_ResolveFromHit(&hit, &graph_payload));

    assert_true("runtime_material_graph_parity_active_graph",
                SceneEditorMaterialGraphHasObjectGraph(scene_object_index));
    assert_close("runtime_material_graph_parity_preview_color_r",
                 graph_preview.colorR,
                 stack_preview.colorR,
                 1e-9);
    assert_close("runtime_material_graph_parity_preview_color_g",
                 graph_preview.colorG,
                 stack_preview.colorG,
                 1e-9);
    assert_close("runtime_material_graph_parity_preview_color_b",
                 graph_preview.colorB,
                 stack_preview.colorB,
                 1e-9);
    assert_close("runtime_material_graph_parity_preview_roughness",
                 graph_preview.roughness,
                 stack_preview.roughness,
                 1e-9);
    assert_close("runtime_material_graph_parity_preview_reflectivity",
                 graph_preview.reflectivity,
                 stack_preview.reflectivity,
                 1e-9);
    assert_close("runtime_material_graph_parity_preview_spec",
                 graph_preview.specWeight,
                 stack_preview.specWeight,
                 1e-9);
    assert_close("runtime_material_graph_parity_payload_color_r",
                 graph_payload.baseColorR,
                 stack_payload.baseColorR,
                 1e-9);
    assert_close("runtime_material_graph_parity_payload_color_g",
                 graph_payload.baseColorG,
                 stack_payload.baseColorG,
                 1e-9);
    assert_close("runtime_material_graph_parity_payload_color_b",
                 graph_payload.baseColorB,
                 stack_payload.baseColorB,
                 1e-9);
    assert_close("runtime_material_graph_parity_payload_roughness",
                 graph_payload.bsdf.roughness,
                 stack_payload.bsdf.roughness,
                 1e-9);
    assert_close("runtime_material_graph_parity_payload_reflectivity",
                 graph_payload.bsdf.reflectivity,
                 stack_payload.bsdf.reflectivity,
                 1e-9);
    assert_close("runtime_material_graph_parity_payload_spec",
                 graph_payload.bsdf.specWeight,
                 stack_payload.bsdf.specWeight,
                 1e-9);

    SceneEditorMaterialGraphResetAll();
    SceneEditorMaterialStackResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    sceneSettings = saved_scene;
    return 0;
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

static int test_runtime_material_texture_stack_procedural_physical_channels(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialSurfaceEval base_eval = {0};
    RuntimeMaterialSurfaceEval channel_eval = {0};
    RuntimeMaterialSurfaceEval preview_eval = {0};
    RuntimeMaterialProceduralPhysicalChannels channels = {0};
    RuntimeMaterialPayload3D payload = {0};
    MaterialBSDF bsdf = {0};
    HitInfo3D hit = {0};
    SceneObject* object = NULL;
    bool found_channels = false;
    double found_u = 0.0;
    double found_v = 0.0;
    const int scene_object_index = 0;
    const int seed_key = (scene_object_index + 1) * 19349663;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    SceneEditorMaterialFacePlacementResetAll();
    SceneEditorMaterialStackResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    RuntimeWaterMaterial3D_ClearAll();
    MaterialManagerResetDefaults();

    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[scene_object_index],
               OBJECT_CIRCLE,
               0.0,
               0.0,
               8.0,
               0.0,
               NULL,
               0);
    object = &sceneSettings.sceneObjects[scene_object_index];
    object->color = 0x6c5a42;
    object->alpha = 1.0;
    object->roughness = 0.46;
    object->reflectivity = 0.12;
    object->textureId = RUNTIME_MATERIAL_TEXTURE_3D_NONE;
    object->textureStrength = 0.0;
    object->textureSeed = seed_key;

    stack.layerCount = 2;
    stack.layers[0] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID);
    stack.layers[1] =
        runtime_material_test_make_strong_overlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL, 2.65);
    stack.layers[1].params.coverage = 0.64;
    stack.layers[1].params.seed = 83;
    assert_true("runtime_material_procedural_channels_stack_set",
                SceneEditorMaterialStackSetObjectStack(scene_object_index, &stack));

    MaterialBSDFInitFromSceneObject(object, &bsdf);
    base_eval = RuntimeMaterialSurfaceEvalMakeBase(bsdf.baseColorR,
                                                   bsdf.baseColorG,
                                                   bsdf.baseColorB,
                                                   bsdf.roughness,
                                                   bsdf.reflectivity,
                                                   bsdf.specWeight,
                                                   bsdf.diffuseWeight,
                                                   0.0);

    for (int i = 0; i < 32 && !found_channels; ++i) {
        double u = 0.08 + ((double)i * 0.031);
        double v = 0.21 + ((double)i * 0.027);
        memset(&channel_eval, 0, sizeof(channel_eval));
        memset(&channels, 0, sizeof(channels));
        found_channels =
            RuntimeMaterialTextureStackEvaluatePhysicalChannelsPlacedUV(&stack,
                                                                        object,
                                                                        u,
                                                                        v,
                                                                        seed_key,
                                                                        &base_eval,
                                                                        &channel_eval,
                                                                        &channels);
        if (found_channels) {
            found_u = u;
            found_v = v;
        }
    }

    assert_true("runtime_material_procedural_channels_found", found_channels);
    assert_true("runtime_material_procedural_channels_active", channels.active);
    assert_true("runtime_material_procedural_channels_mask", channels.layerMask > 1e-9);
    assert_true("runtime_material_procedural_channels_roughness_active",
                channels.roughnessActive);
    assert_true("runtime_material_procedural_channels_reflectivity_active",
                channels.reflectivityActive);
    assert_true("runtime_material_procedural_channels_specular_active",
                channels.specularActive);
    assert_close("runtime_material_procedural_channels_roughness_value",
                 channels.roughnessScalar,
                 channel_eval.roughness,
                 1e-12);
    assert_close("runtime_material_procedural_channels_reflectivity_value",
                 channels.reflectivityCompat,
                 channel_eval.reflectivity,
                 1e-12);
    assert_close("runtime_material_procedural_channels_specular_value",
                 channels.specularWeight,
                 channel_eval.specWeight,
                 1e-12);
    assert_true("runtime_material_procedural_channels_no_opacity_promotion",
                !channels.opacityCoverageActive);
    assert_close("runtime_material_procedural_channels_coverage_default",
                 channels.opacityCoverage,
                 1.0,
                 1e-12);
    assert_true("runtime_material_procedural_channels_no_transmission_promotion",
                !channels.transmissionWeightActive);
    assert_close("runtime_material_procedural_channels_transmission_default",
                 channels.transmissionWeight,
                 base_eval.transparency,
                 1e-12);

    assert_true("runtime_material_procedural_channels_preview_ok",
                MaterialPreviewSurfaceEvaluateObject(object,
                                                     scene_object_index,
                                                     NULL,
                                                     found_u,
                                                     found_v,
                                                     &preview_eval));
    assert_close("runtime_material_procedural_channels_preview_roughness",
                 channels.roughnessScalar,
                 preview_eval.roughness,
                 1e-12);
    assert_close("runtime_material_procedural_channels_preview_reflectivity",
                 channels.reflectivityCompat,
                 preview_eval.reflectivity,
                 1e-12);
    assert_close("runtime_material_procedural_channels_preview_specular",
                 channels.specularWeight,
                 preview_eval.specWeight,
                 1e-12);

    HitInfo3D_Reset(&hit);
    hit.sceneObjectIndex = scene_object_index;
    hit.triangleIndex = 0;
    hit.localTriangleIndex = -1;
    hit.primitiveIndex = 0;
    hit.hasObjectTextureCoord = true;
    hit.normal.z = 1.0;
    hit.objectTextureCoord.x = found_u;
    hit.objectTextureCoord.y = found_v;
    assert_true("runtime_material_procedural_channels_payload_ok",
                RuntimeMaterialPayload3D_ResolveFromHit(&hit, &payload));
    assert_close("runtime_material_procedural_channels_payload_roughness",
                 channels.roughnessScalar,
                 payload.bsdf.roughness,
                 1e-12);
    assert_close("runtime_material_procedural_channels_payload_reflectivity",
                 channels.reflectivityCompat,
                 payload.bsdf.reflectivity,
                 1e-12);
    assert_close("runtime_material_procedural_channels_payload_specular",
                 channels.specularWeight,
                 payload.bsdf.specWeight,
                 1e-12);
    assert_close("runtime_material_procedural_channels_payload_transparency",
                 payload.transparency,
                 base_eval.transparency,
                 1e-12);

    RuntimeWaterMaterial3D_ClearAll();
    RuntimeMaterialAuthoredTextureResetAll();
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialFacePlacementResetAll();
    sceneSettings = saved_scene;
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
    RuntimeMaterialTextureStack reordered_stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialTextureLayer tmp_layer;
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

    stack.layerCount = 4;
    stack.layers[0] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID);
    stack.layers[1] =
        runtime_material_test_make_strong_overlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME, 3.0);
    stack.layers[1].placement.strength = 0.0;
    stack.layers[2] =
        runtime_material_test_make_strong_overlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SCRATCHES, 3.0);
    stack.layers[2].placement.strength = 0.0;
    stack.layers[3] =
        runtime_material_test_make_strong_overlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL, 3.0);
    stack.layers[3].placement.strength = 0.0;
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
    snprintf(placement.layerId, sizeof(placement.layerId), "%s", stack.layers[3].layerId);
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
    reordered_stack = stack;
    tmp_layer = reordered_stack.layers[1];
    reordered_stack.layers[1] = reordered_stack.layers[2];
    reordered_stack.layers[2] = tmp_layer;
    assert_true("runtime_material_payload_face_layer_id_reordered_stack_set",
                SceneEditorMaterialStackSetObjectStack(0, &reordered_stack));

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

static int test_runtime_material_preview_payload_surface_eval_parity_stack_face_path(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialSurfaceEval preview_object = {0};
    RuntimeMaterialSurfaceEval preview_face = {0};
    RuntimeMaterialPayload3D payload_object = {0};
    RuntimeMaterialPayload3D payload_face = {0};
    SceneEditorMaterialFacePlacement placement = {0};
    HitInfo3D hit = {0};
    bool ok = false;
    bool found_face_sample = false;
    const int scene_object_index = 0;
    const int face_group_index = 1;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    SceneEditorMaterialFacePlacementResetAll();
    SceneEditorMaterialStackResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    RuntimeWaterMaterial3D_ClearAll();
    MaterialManagerResetDefaults();

    sceneSettings.objectCount = 1;
    InitObject(&sceneSettings.sceneObjects[scene_object_index],
               OBJECT_POLYGON,
               0.0,
               0.0,
               1.0,
               0.0,
               NULL,
               0);
    sceneSettings.sceneObjects[scene_object_index].color = 0x4a5968;
    sceneSettings.sceneObjects[scene_object_index].alpha = 1.0;
    sceneSettings.sceneObjects[scene_object_index].textureId = RUNTIME_MATERIAL_TEXTURE_3D_NONE;
    sceneSettings.sceneObjects[scene_object_index].textureStrength = 0.0;
    sceneSettings.sceneObjects[scene_object_index].textureSeed =
        (scene_object_index + 1) * 19349663;
    sceneSettings.sceneObjects[scene_object_index].reflectivity = 0.18;
    sceneSettings.sceneObjects[scene_object_index].roughness = 0.52;

    stack.layerCount = 3;
    stack.layers[0] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD);
    stack.layers[0].placement.scale = 2.75;
    stack.layers[0].placement.strength = 1.0;
    stack.layers[0].params.grain = 0.42;
    stack.layers[0].params.colorDepth = 0.76;
    stack.layers[0].params.seed = 17;
    stack.layers[1] =
        runtime_material_test_make_strong_overlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME, 3.25);
    stack.layers[1].params.coverage = 0.35;
    stack.layers[1].params.seed = 23;
    stack.layers[2] =
        runtime_material_test_make_strong_overlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL, 2.85);
    stack.layers[2].params.coverage = 0.42;
    stack.layers[2].params.seed = 29;

    assert_true("runtime_material_preview_payload_parity_stack_set",
                SceneEditorMaterialStackSetObjectStack(scene_object_index, &stack));

    HitInfo3D_Reset(&hit);
    hit.sceneObjectIndex = scene_object_index;
    hit.triangleIndex = 0;
    hit.localTriangleIndex = -1;
    hit.primitiveIndex = 0;
    hit.hasObjectTextureCoord = true;
    hit.normal.z = 1.0;
    hit.objectTextureCoord.x = 0.37;
    hit.objectTextureCoord.y = 0.46;

    assert_true("runtime_material_preview_payload_parity_object_preview_ok",
                MaterialPreviewSurfaceEvaluateObject(&sceneSettings.sceneObjects[scene_object_index],
                                                     scene_object_index,
                                                     NULL,
                                                     hit.objectTextureCoord.x,
                                                     hit.objectTextureCoord.y,
                                                     &preview_object));
    assert_true("runtime_material_preview_payload_parity_object_payload_ok",
                RuntimeMaterialPayload3D_ResolveFromHit(&hit, &payload_object));
    assert_true("runtime_material_preview_payload_parity_object_stack_active",
                preview_object.active && payload_object.textureMask > 0.0);
    runtime_material_test_assert_surface_matches_payload(
        "runtime_material_preview_payload_parity_object",
        &preview_object,
        &payload_object);

    placement.hasOverride = true;
    placement.sceneObjectIndex = scene_object_index;
    placement.faceGroupIndex = face_group_index;
    placement.layerIndex = 1;
    snprintf(placement.layerId, sizeof(placement.layerId), "%s", stack.layers[2].layerId);
    placement.textureId = RUNTIME_MATERIAL_TEXTURE_3D_RUST;
    placement.offsetU = 0.13;
    placement.offsetV = 0.21;
    placement.scale = 3.75;
    placement.strength = 1.0;
    placement.params.coverage = 0.95;
    placement.params.grain = 0.62;
    placement.params.edgeSoftness = 1.0;
    placement.params.contrast = 0.40;
    placement.params.flow = 0.70;
    placement.params.colorDepth = 1.0;
    placement.params.surfaceDamage = 0.86;
    placement.params.seed = 31;
    assert_true("runtime_material_preview_payload_parity_face_override_set",
                SceneEditorMaterialFacePlacementSetOverride(&placement));
    assert_true("runtime_material_preview_payload_parity_face_layer_id",
                SceneEditorMaterialFacePlacementHasOverrideForLayer(scene_object_index,
                                                                    face_group_index,
                                                                    "oil"));

    for (int u = 1; u < 9 && !found_face_sample; ++u) {
        for (int v = 1; v < 9 && !found_face_sample; ++v) {
            double island_u = 0.0;
            double island_v = 0.0;
            double bary_v = (double)u / 10.0;
            double bary_w = (double)v / 10.0;
            if (bary_v + bary_w >= 0.95) continue;

            HitInfo3D_Reset(&hit);
            hit.sceneObjectIndex = scene_object_index;
            hit.triangleIndex = 2;
            hit.localTriangleIndex = 2;
            hit.primitiveIndex = 0;
            hit.baryV = bary_v;
            hit.baryW = bary_w;
            hit.baryU = 1.0 - bary_v - bary_w;
            SceneEditorMaterialFacePlacementResolveIslandUV(hit.localTriangleIndex,
                                                            hit.baryU,
                                                            hit.baryV,
                                                            hit.baryW,
                                                            &island_u,
                                                            &island_v);

            ok = MaterialPreviewSurfaceEvaluateFace(&sceneSettings.sceneObjects[scene_object_index],
                                                    scene_object_index,
                                                    face_group_index,
                                                    island_u,
                                                    island_v,
                                                    &preview_face);
            assert_true("runtime_material_preview_payload_parity_face_preview_ok", ok);
            ok = RuntimeMaterialPayload3D_ResolveFromHit(&hit, &payload_face);
            assert_true("runtime_material_preview_payload_parity_face_payload_ok", ok);

            if (preview_face.layerMasks[2] > 1e-9 && payload_face.textureMask > 1e-9) {
                found_face_sample = true;
                runtime_material_test_assert_surface_matches_payload(
                    "runtime_material_preview_payload_parity_face",
                    &preview_face,
                    &payload_face);
            }
        }
    }

    assert_true("runtime_material_preview_payload_parity_face_sample_found", found_face_sample);

    RuntimeWaterMaterial3D_ClearAll();
    RuntimeMaterialAuthoredTextureResetAll();
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialFacePlacementResetAll();
    sceneSettings = saved_scene;
    return 0;
}

static int test_runtime_material_preview_payload_surface_eval_parity_grounded_primitive_face(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char* runtime_path = "/tmp/ray_tracing_material_m3s1_grounded_uv_parity.json";
    const char* runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"material_m3s1_grounded_uv_parity\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[{"
          "\"object_id\":\"grounded_prism\","
          "\"object_type\":\"rect_prism_primitive\","
          "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
            "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
          "\"primitive\":{\"kind\":\"rect_prism_primitive\","
            "\"width\":4.0,\"height\":3.0,\"depth\":2.0},"
          "\"flags\":{\"visible\":true}"
        "}],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialSurfaceEval preview_face = {0};
    RuntimeMaterialPayload3D payload_face = {0};
    SceneEditorMaterialFacePlacement placement = {0};
    HitInfo3D hit = {0};
    bool found_face_sample = false;
    const int scene_object_index = 0;
    const int primitive_index = 0;
    const int face_group_index = 2;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialFacePlacementResetAll();
    SceneEditorMaterialStackResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    RuntimeWaterMaterial3D_ClearAll();
    MaterialManagerResetDefaults();

    assert_true("runtime_material_preview_payload_grounded_write",
                runtime_material_test_write_text_file(runtime_path, runtime_json));
    assert_true("runtime_material_preview_payload_grounded_apply",
                runtime_scene_bridge_apply_file(runtime_path, &summary));
    assert_true("runtime_material_preview_payload_grounded_object_count",
                sceneSettings.objectCount == 1);

    sceneSettings.sceneObjects[scene_object_index].color = 0x526170;
    sceneSettings.sceneObjects[scene_object_index].alpha = 1.0;
    sceneSettings.sceneObjects[scene_object_index].textureId = RUNTIME_MATERIAL_TEXTURE_3D_NONE;
    sceneSettings.sceneObjects[scene_object_index].textureStrength = 0.0;
    sceneSettings.sceneObjects[scene_object_index].reflectivity = 0.14;
    sceneSettings.sceneObjects[scene_object_index].roughness = 0.56;

    stack.layerCount = 3;
    stack.layers[0] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD);
    stack.layers[0].placement.scale = 2.15;
    stack.layers[0].placement.strength = 1.0;
    stack.layers[0].params.grain = 0.58;
    stack.layers[0].params.colorDepth = 0.72;
    stack.layers[0].params.seed = 41;
    stack.layers[1] =
        runtime_material_test_make_strong_overlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME, 2.8);
    stack.layers[1].params.coverage = 0.30;
    stack.layers[1].params.seed = 43;
    stack.layers[2] =
        runtime_material_test_make_strong_overlay(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL, 3.1);
    stack.layers[2].params.coverage = 0.50;
    stack.layers[2].params.seed = 47;
    assert_true("runtime_material_preview_payload_grounded_stack_set",
                SceneEditorMaterialStackSetObjectStack(scene_object_index, &stack));

    placement.hasOverride = true;
    placement.sceneObjectIndex = scene_object_index;
    placement.faceGroupIndex = face_group_index;
    placement.layerIndex = 1;
    snprintf(placement.layerId, sizeof(placement.layerId), "%s", stack.layers[2].layerId);
    placement.textureId = RUNTIME_MATERIAL_TEXTURE_3D_RUST;
    placement.offsetU = 0.19;
    placement.offsetV = 0.27;
    placement.scale = 4.2;
    placement.strength = 1.0;
    placement.params.coverage = 0.92;
    placement.params.grain = 0.64;
    placement.params.edgeSoftness = 1.0;
    placement.params.contrast = 0.38;
    placement.params.flow = 0.44;
    placement.params.colorDepth = 1.0;
    placement.params.surfaceDamage = 0.90;
    placement.params.seed = 53;
    assert_true("runtime_material_preview_payload_grounded_face_override_set",
                SceneEditorMaterialFacePlacementSetOverride(&placement));

    for (int u = 1; u < 9 && !found_face_sample; ++u) {
        for (int v = 1; v < 9 && !found_face_sample; ++v) {
            double island_u = 0.0;
            double island_v = 0.0;
            double bary_v = (double)u / 10.0;
            double bary_w = (double)v / 10.0;
            if (bary_v + bary_w >= 0.95) continue;

            HitInfo3D_Reset(&hit);
            hit.sceneObjectIndex = scene_object_index;
            hit.triangleIndex = 4;
            hit.localTriangleIndex = 4;
            hit.primitiveIndex = primitive_index;
            hit.baryV = bary_v;
            hit.baryW = bary_w;
            hit.baryU = 1.0 - bary_v - bary_w;
            SceneEditorMaterialFacePlacementResolveIslandUV(hit.localTriangleIndex,
                                                            hit.baryU,
                                                            hit.baryV,
                                                            hit.baryW,
                                                            &island_u,
                                                            &island_v);

            assert_true("runtime_material_preview_payload_grounded_preview_ok",
                        MaterialPreviewSurfaceEvaluateFacePrimitive(
                            &sceneSettings.sceneObjects[scene_object_index],
                            scene_object_index,
                            primitive_index,
                            face_group_index,
                            island_u,
                            island_v,
                            &preview_face));
            assert_true("runtime_material_preview_payload_grounded_payload_ok",
                        RuntimeMaterialPayload3D_ResolveFromHit(&hit, &payload_face));

            if (preview_face.layerMasks[2] > 1e-9 && payload_face.textureMask > 1e-9) {
                found_face_sample = true;
                runtime_material_test_assert_surface_matches_payload(
                    "runtime_material_preview_payload_grounded_face",
                    &preview_face,
                    &payload_face);
            }
        }
    }

    assert_true("runtime_material_preview_payload_grounded_face_sample_found",
                found_face_sample);

    unlink(runtime_path);
    RuntimeWaterMaterial3D_ClearAll();
    RuntimeMaterialAuthoredTextureResetAll();
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialFacePlacementResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
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
        json_object_object_add(base, "id", json_object_new_string("base_wood_custom"));
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
        json_object_object_add(overlay, "id", json_object_new_string("grime_detail"));
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
        json_object_object_add(face_entry, "layerId", json_object_new_string("grime_detail"));
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
    assert_true("runtime_material_config_v2_stack_base_id",
                strcmp(stack.layers[0].layerId, "base_wood_custom") == 0);
    assert_true("runtime_material_config_v2_stack_overlay_kind",
                stack.layers[1].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME);
    assert_true("runtime_material_config_v2_stack_overlay_id",
                strcmp(stack.layers[1].layerId, "grime_detail") == 0);
    assert_close("runtime_material_config_v2_stack_overlay_scale",
                 stack.layers[1].placement.scale,
                 3.0,
                 1e-9);
    assert_true("runtime_material_config_v2_face_layer_loaded",
                SceneEditorMaterialFacePlacementHasOverrideForLayer(0, 3, "grime_detail"));
    {
        SceneEditorMaterialFacePlacement placement =
            SceneEditorMaterialFacePlacementGetEffectiveForLayer(&sceneSettings.sceneObjects[0],
                                                                 0,
                                                                 3,
                                                                 "grime_detail");
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

static int test_runtime_material_hydration_parity_scene_config_and_runtime_scene(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char* runtime_path = "/tmp/ray_tracing_material_hydration_parity_runtime.json";
    const char* runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"material_hydration_parity\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[{"
          "\"object_id\":\"parity_box\","
          "\"object_type\":\"rect_prism_primitive\","
          "\"transform\":{"
            "\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
            "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}"
          "},"
          "\"primitive\":{\"kind\":\"rect_prism_primitive\",\"width\":1.0,\"height\":1.0,\"depth\":1.0},"
          "\"flags\":{\"visible\":true}"
        "}],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{\"ray_tracing\":{\"authoring\":{\"object_materials\":[{"
          "\"object_id\":\"parity_box\","
          "\"material_id\":0,"
          "\"materialTextureStack\":{\"version\":1,\"layers\":["
            "{"
              "\"id\":\"base_parity\","
              "\"kind\":\"wood\","
              "\"role\":\"base\","
              "\"placement\":{\"offsetU\":0.11,\"offsetV\":0.22,\"scale\":2.0,\"strength\":0.8},"
              "\"parameters\":{\"patternMode\":2,\"coverage\":0.73,\"colorDepth\":0.61,\"surfaceDamage\":0.27}"
            "},"
            "{"
              "\"id\":\"grime_parity\","
              "\"kind\":\"grime\","
              "\"role\":\"overlay\","
              "\"opacity\":0.82,"
              "\"placement\":{\"offsetU\":0.31,\"offsetV\":0.41,\"scale\":3.5,\"strength\":0.92},"
              "\"parameters\":{\"patternMode\":3,\"coverage\":0.66,\"grain\":0.44,\"surfaceDamage\":0.58},"
              "\"roughnessInfluence\":0.45,"
              "\"specularInfluence\":-0.25"
            "}"
          "]},"
          "\"procedural_texture\":{\"face_placements\":[{"
            "\"face_group_index\":3,"
            "\"layer_index\":1,"
            "\"layer_id\":\"grime_parity\","
            "\"texture_id\":1,"
            "\"offset_u\":0.19,"
            "\"offset_v\":0.29,"
            "\"scale\":4.25,"
            "\"strength\":0.64,"
            "\"rotation\":0.33,"
            "\"parameters\":{\"coverage\":0.57,\"surface_damage\":0.83,\"seed\":77}"
          "}]}"
        "}]}}}"
        "}";
    json_object* root = json_object_new_object();
    json_object* objects = json_object_new_array();
    json_object* obj = json_object_new_object();
    json_object* stack_obj = json_object_new_object();
    json_object* layers = json_object_new_array();
    json_object* face_placements = json_object_new_array();
    RuntimeMaterialTextureStack config_stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialTextureStack runtime_stack = RuntimeMaterialTextureStackEmpty();
    SceneEditorMaterialFacePlacement config_face;
    SceneEditorMaterialFacePlacement runtime_face;
    SceneEditorMaterialFacePlacement stale_face = {0};
    RuntimeMaterialTextureStack stale_stack = RuntimeMaterialTextureStackEmpty();
    RuntimeSceneBridgePreflight summary = {0};

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialFacePlacementResetAll();
    RuntimeMaterialAuthoredTextureResetAll();

    json_object_object_add(obj, "type", json_object_new_string("rectangle"));
    json_object_object_add(obj, "x", json_object_new_double(0.0));
    json_object_object_add(obj, "y", json_object_new_double(0.0));
    json_object_object_add(obj, "scale", json_object_new_double(1.0));
    json_object_object_add(obj, "color", json_object_new_int(0x303030));
    json_object_object_add(obj, "materialId", json_object_new_int(MaterialManagerDefaultId()));
    {
        json_object* base = json_object_new_object();
        json_object* placement = json_object_new_object();
        json_object* parameters = json_object_new_object();
        json_object_object_add(placement, "offset_u", json_object_new_double(0.11));
        json_object_object_add(placement, "offset_v", json_object_new_double(0.22));
        json_object_object_add(placement, "scale", json_object_new_double(2.0));
        json_object_object_add(placement, "strength", json_object_new_double(0.8));
        json_object_object_add(parameters, "pattern_mode", json_object_new_int(2));
        json_object_object_add(parameters, "coverage", json_object_new_double(0.73));
        json_object_object_add(parameters, "color_depth", json_object_new_double(0.61));
        json_object_object_add(parameters, "surface_damage", json_object_new_double(0.27));
        json_object_object_add(base, "id", json_object_new_string("base_parity"));
        json_object_object_add(base, "kind", json_object_new_string("wood"));
        json_object_object_add(base, "role", json_object_new_string("base"));
        json_object_object_add(base, "placement", placement);
        json_object_object_add(base, "parameters", parameters);
        json_object_array_add(layers, base);
    }
    {
        json_object* overlay = json_object_new_object();
        json_object* placement = json_object_new_object();
        json_object* parameters = json_object_new_object();
        json_object_object_add(placement, "offset_u", json_object_new_double(0.31));
        json_object_object_add(placement, "offset_v", json_object_new_double(0.41));
        json_object_object_add(placement, "scale", json_object_new_double(3.5));
        json_object_object_add(placement, "strength", json_object_new_double(0.92));
        json_object_object_add(parameters, "pattern_mode", json_object_new_int(3));
        json_object_object_add(parameters, "coverage", json_object_new_double(0.66));
        json_object_object_add(parameters, "grain", json_object_new_double(0.44));
        json_object_object_add(parameters, "surface_damage", json_object_new_double(0.58));
        json_object_object_add(overlay, "id", json_object_new_string("grime_parity"));
        json_object_object_add(overlay, "kind", json_object_new_string("grime"));
        json_object_object_add(overlay, "role", json_object_new_string("overlay"));
        json_object_object_add(overlay, "opacity", json_object_new_double(0.82));
        json_object_object_add(overlay, "placement", placement);
        json_object_object_add(overlay, "parameters", parameters);
        json_object_object_add(overlay, "roughness_influence", json_object_new_double(0.45));
        json_object_object_add(overlay, "specular_influence", json_object_new_double(-0.25));
        json_object_array_add(layers, overlay);
    }
    json_object_object_add(stack_obj, "version", json_object_new_int(1));
    json_object_object_add(stack_obj, "layers", layers);
    json_object_object_add(obj, "material_texture_stack", stack_obj);
    {
        json_object* face_entry = json_object_new_object();
        json_object* parameters = json_object_new_object();
        json_object_object_add(parameters, "coverage", json_object_new_double(0.57));
        json_object_object_add(parameters, "surface_damage", json_object_new_double(0.83));
        json_object_object_add(parameters, "seed", json_object_new_int(77));
        json_object_object_add(face_entry, "face_group_index", json_object_new_int(3));
        json_object_object_add(face_entry, "layer_index", json_object_new_int(1));
        json_object_object_add(face_entry, "layer_id", json_object_new_string("grime_parity"));
        json_object_object_add(face_entry, "texture_id", json_object_new_int(RUNTIME_MATERIAL_TEXTURE_3D_RUST));
        json_object_object_add(face_entry, "offset_u", json_object_new_double(0.19));
        json_object_object_add(face_entry, "offset_v", json_object_new_double(0.29));
        json_object_object_add(face_entry, "scale", json_object_new_double(4.25));
        json_object_object_add(face_entry, "strength", json_object_new_double(0.64));
        json_object_object_add(face_entry, "rotation", json_object_new_double(0.33));
        json_object_object_add(face_entry, "parameters", parameters);
        json_object_array_add(face_placements, face_entry);
    }
    json_object_object_add(obj, "material_face_placements", face_placements);
    json_object_array_add(objects, obj);
    json_object_object_add(root, "objects", objects);
    LoadSceneObjects(root);
    assert_true("runtime_material_hydration_parity_config_stack",
                SceneEditorMaterialStackGetObjectStack(0, &config_stack));
    config_face = SceneEditorMaterialFacePlacementGetEffectiveForLayer(&sceneSettings.sceneObjects[0],
                                                                       0,
                                                                       3,
                                                                       "grime_parity");

    assert_true("runtime_material_hydration_parity_runtime_write",
                runtime_material_test_write_text_file(runtime_path, runtime_json));
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialFacePlacementResetAll();
    assert_true("runtime_material_hydration_parity_runtime_apply",
                runtime_scene_bridge_apply_file(runtime_path, &summary));
    assert_true("runtime_material_hydration_parity_runtime_stack",
                SceneEditorMaterialStackGetObjectStack(0, &runtime_stack));
    runtime_face = SceneEditorMaterialFacePlacementGetEffectiveForLayer(&sceneSettings.sceneObjects[0],
                                                                        0,
                                                                        3,
                                                                        "grime_parity");

    assert_true("runtime_material_hydration_parity_layer_count",
                config_stack.layerCount == runtime_stack.layerCount && runtime_stack.layerCount == 2);
    assert_true("runtime_material_hydration_parity_base_id",
                strcmp(config_stack.layers[0].layerId, runtime_stack.layers[0].layerId) == 0);
    assert_true("runtime_material_hydration_parity_overlay_id",
                strcmp(config_stack.layers[1].layerId, runtime_stack.layers[1].layerId) == 0);
    assert_true("runtime_material_hydration_parity_overlay_kind",
                config_stack.layers[1].kind == runtime_stack.layers[1].kind);
    assert_close("runtime_material_hydration_parity_overlay_scale",
                 runtime_stack.layers[1].placement.scale,
                 config_stack.layers[1].placement.scale,
                 1e-9);
    assert_close("runtime_material_hydration_parity_overlay_roughness",
                 runtime_stack.layers[1].roughnessInfluence,
                 config_stack.layers[1].roughnessInfluence,
                 1e-9);
    assert_true("runtime_material_hydration_parity_face_layer_id",
                strcmp(config_face.layerId, runtime_face.layerId) == 0);
    assert_true("runtime_material_hydration_parity_face_layer_index",
                config_face.layerIndex == runtime_face.layerIndex);
    assert_close("runtime_material_hydration_parity_face_strength",
                 runtime_face.strength,
                 config_face.strength,
                 1e-9);
    assert_close("runtime_material_hydration_parity_face_damage",
                 runtime_face.params.surfaceDamage,
                 config_face.params.surfaceDamage,
                 1e-9);

    stale_stack.layerCount = 1;
    stale_stack.layers[0] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD);
    assert_true("runtime_material_hydration_parity_stale_stack_seed_config",
                SceneEditorMaterialStackSetObjectStack(0, &stale_stack));
    stale_face.hasOverride = true;
    stale_face.sceneObjectIndex = 0;
    stale_face.faceGroupIndex = 3;
    snprintf(stale_face.layerId, sizeof(stale_face.layerId), "%s", "stale_layer");
    stale_face.scale = 1.0;
    assert_true("runtime_material_hydration_parity_stale_face_seed_config",
                SceneEditorMaterialFacePlacementSetOverride(&stale_face));
    assert_true("runtime_material_hydration_parity_stale_authored_seed_config",
                !RuntimeMaterialAuthoredTextureBindManifestForObject(
                    0,
                    "stale_config",
                    "/tmp/ray_tracing_missing_stale_config_authored_texture_manifest.json",
                    "override"));
    assert_true("runtime_material_hydration_parity_stale_authored_invalid_config",
                RuntimeMaterialAuthoredTextureGetInvalidBinding(0, NULL, 0u, NULL, 0u, NULL, 0u));
    {
        json_object* reset_root = json_object_new_object();
        json_object* reset_objects = json_object_new_array();
        json_object* reset_obj = json_object_new_object();
        json_object_object_add(reset_obj, "type", json_object_new_string("rectangle"));
        json_object_object_add(reset_obj, "x", json_object_new_double(0.0));
        json_object_object_add(reset_obj, "y", json_object_new_double(0.0));
        json_object_object_add(reset_obj, "scale", json_object_new_double(1.0));
        json_object_array_add(reset_objects, reset_obj);
        json_object_object_add(reset_root, "objects", reset_objects);
        LoadSceneObjects(reset_root);
        json_object_put(reset_root);
    }
    assert_true("runtime_material_hydration_parity_config_clears_stack",
                !SceneEditorMaterialStackHasObjectStack(0));
    assert_true("runtime_material_hydration_parity_config_clears_face",
                !SceneEditorMaterialFacePlacementHasOverride(0, 3));
    assert_true("runtime_material_hydration_parity_config_clears_authored",
                !RuntimeMaterialAuthoredTextureGetInvalidBinding(0, NULL, 0u, NULL, 0u, NULL, 0u));

    assert_true("runtime_material_hydration_parity_stale_stack_seed_runtime",
                SceneEditorMaterialStackSetObjectStack(0, &stale_stack));
    assert_true("runtime_material_hydration_parity_stale_face_seed_runtime",
                SceneEditorMaterialFacePlacementSetOverride(&stale_face));
    assert_true("runtime_material_hydration_parity_stale_authored_seed_runtime",
                !RuntimeMaterialAuthoredTextureBindManifestForObject(
                    0,
                    "stale_runtime",
                    "/tmp/ray_tracing_missing_stale_runtime_authored_texture_manifest.json",
                    "override"));
    assert_true("runtime_material_hydration_parity_stale_authored_invalid_runtime",
                RuntimeMaterialAuthoredTextureGetInvalidBinding(0, NULL, 0u, NULL, 0u, NULL, 0u));
    assert_true("runtime_material_hydration_parity_runtime_reset_apply",
                runtime_scene_bridge_apply_file(runtime_path, &summary));
    assert_true("runtime_material_hydration_parity_runtime_reload_stack",
                SceneEditorMaterialStackHasObjectStack(0));
    assert_true("runtime_material_hydration_parity_runtime_no_stale_face",
                !SceneEditorMaterialFacePlacementHasOverrideForLayer(0, 3, "stale_layer"));
    assert_true("runtime_material_hydration_parity_runtime_no_stale_authored",
                !RuntimeMaterialAuthoredTextureGetInvalidBinding(0, NULL, 0u, NULL, 0u, NULL, 0u));

    unlink(runtime_path);
    json_object_put(root);
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialFacePlacementResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

int run_test_runtime_lighting_materials_payload_suite(void) {
    test_runtime_material_payload_3d_scene_object_resolution_contract();
    test_runtime_material_payload_3d_authoring_object_values_override_preset();
    test_runtime_material_payload_3d_object_multipliers_contract();
    test_runtime_material_compatibility_bridges_are_explicit();
    test_runtime_material_payload_3d_water_override_contract();
    test_material_manager_default_presets_include_i4_entries();
    test_material_manager_load_dir_preserves_shipped_preset_ids();
    test_material_manager_transparent_override_inherits_shipped_defaults();
    test_runtime_material_payload_3d_hit_resolution_contract();
    test_runtime_material_payload_3d_rust_texture_is_hit_anchored();
    test_runtime_material_payload_3d_face_texture_override_affects_hit();
    test_runtime_material_payload_3d_authored_texture_override_affects_hit();
    test_runtime_material_payload_authored_texture_becomes_base_but_keeps_overlay_stack();
    test_runtime_material_payload_authored_texture_channel_references();
    test_runtime_material_payload_authored_texture_no_displacement_guard();
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
    test_runtime_principled_bsdf_conversion_adapter_m2_contract();
    test_runtime_material_texture_stack_legacy_object_adapter_contract();
    test_runtime_material_texture_stack_normalizes_bounds_contract();
    test_runtime_material_texture_stack_surface_eval_applies_rust_response();
    test_runtime_material_texture_stack_base_patterns_change_surface_response();
    test_runtime_material_texture_stack_base_patterns_are_repeat_stable();
    test_runtime_material_texture_stack_base_then_overlay_orders_response();
    test_runtime_material_texture_stack_grime_and_oil_overlay_responses();
    test_runtime_material_texture_stack_procedural_physical_channels();
    test_runtime_material_graph_compiles_stack_and_channels();
    test_runtime_material_graph_scene_config_round_trip_stack_fallback();
    test_runtime_material_graph_runtime_scene_round_trip_stack_fallback();
    test_runtime_material_graph_evaluator_parity_matches_authored_stack();
    test_runtime_material_texture_stack_grime_oil_order_changes_result();
    test_runtime_material_texture_stack_overlays_do_not_reopen_transparency();
    test_runtime_material_payload_uses_object_v2_stack_override();
    test_runtime_material_payload_face_override_targets_v2_layer_id_after_reorder();
    test_runtime_material_preview_payload_surface_eval_parity_stack_face_path();
    test_runtime_material_preview_payload_surface_eval_parity_grounded_primitive_face();
    test_runtime_material_config_loads_v2_stack_schema();
    test_runtime_material_hydration_parity_scene_config_and_runtime_scene();
    return 0;
}
