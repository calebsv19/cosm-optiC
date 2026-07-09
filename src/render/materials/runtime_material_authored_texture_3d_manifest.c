#include "render/runtime_material_authored_texture_3d_internal.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config/config_manager.h"
#include "core_authored_texture.h"
#include "core_scene.h"
#include "render/runtime_material_texture_stack_3d.h"

RuntimeMaterialAuthoredTextureBinding s_authored_texture_bindings[MAX_OBJECTS];

static bool runtime_material_authored_texture_manifest_array_exists(json_object* root,
                                                                    const char* key) {
    json_object* field = NULL;
    return root && key && json_object_object_get_ex(root, key, &field) &&
           json_object_is_type(field, json_type_array);
}

static bool runtime_material_authored_texture_manifest_parse_schema_version(
    json_object* manifest_root,
    int* out_schema_version) {
    json_object* field = NULL;
    int schema_version = 0;
    if (!manifest_root || !out_schema_version ||
        !json_object_object_get_ex(manifest_root, "schema_version", &field) ||
        !json_object_is_type(field, json_type_int)) {
        return false;
    }
    schema_version = json_object_get_int(field);
    if (!core_authored_texture_schema_version_supported(schema_version)) {
        return false;
    }
    *out_schema_version = schema_version;
    return true;
}

static bool runtime_material_authored_texture_manifest_parse_contract(
    json_object* manifest_root,
    CoreAuthoredTextureManifestContract* out_contract) {
    json_object* field = NULL;
    const char* text = NULL;
    if (!manifest_root || !out_contract) {
        return false;
    }
    memset(out_contract, 0, sizeof(*out_contract));
    if (!runtime_material_authored_texture_manifest_parse_schema_version(manifest_root,
                                                                         &out_contract->schema_version)) {
        return false;
    }
    if (!json_object_object_get_ex(manifest_root, "export_binding_kind", &field) ||
        !json_object_is_type(field, json_type_string) ||
        !core_authored_texture_binding_kind_parse(json_object_get_string(field),
                                                  &out_contract->binding_kind)) {
        return false;
    }
    if (!json_object_object_get_ex(manifest_root, "primitive_kind", &field) ||
        !json_object_is_type(field, json_type_string) ||
        !core_authored_texture_primitive_kind_parse(json_object_get_string(field),
                                                    &out_contract->primitive_kind)) {
        return false;
    }
    out_contract->has_legacy_surfaces =
        runtime_material_authored_texture_manifest_array_exists(manifest_root, "surfaces");
    out_contract->has_base_surfaces =
        runtime_material_authored_texture_manifest_array_exists(manifest_root, "base_surfaces");
    out_contract->has_overlay_surfaces =
        runtime_material_authored_texture_manifest_array_exists(manifest_root, "overlay_surfaces");
    if (out_contract->schema_version == CORE_AUTHORED_TEXTURE_SCHEMA_V1 ||
        out_contract->schema_version == CORE_AUTHORED_TEXTURE_SCHEMA_V2) {
        out_contract->output_kind = CORE_AUTHORED_TEXTURE_OUTPUT_KIND_LEGACY_FLATTENED;
        return core_authored_texture_manifest_contract_validate(out_contract);
    }
    if (!json_object_object_get_ex(manifest_root, "emitted_output_kind", &field) ||
        !json_object_is_type(field, json_type_string)) {
        return false;
    }
    text = json_object_get_string(field);
    if (!core_authored_texture_output_kind_parse(text, &out_contract->output_kind)) {
        return false;
    }
    return core_authored_texture_manifest_contract_validate(out_contract);
}

static bool runtime_material_authored_texture_faces_complete(
    CoreAuthoredTexturePrimitiveKind primitive_kind,
    const RuntimeMaterialAuthoredTextureFace* faces,
    int face_count) {
    CoreAuthoredTextureFaceRole face_roles[RUNTIME_MATERIAL_AUTHORED_TEXTURE_MAX_FACES];
    size_t expected = core_authored_texture_expected_face_count(primitive_kind);
    size_t i = 0u;
    if (!faces || expected == 0u || face_count != (int)expected) {
        return false;
    }
    for (i = 0u; i < expected; ++i) {
        if (!faces[i].active) {
            return false;
        }
        face_roles[i] = (CoreAuthoredTextureFaceRole)(i + 1u);
    }
    return core_authored_texture_face_roles_complete(primitive_kind, face_roles, expected);
}

static bool runtime_material_authored_texture_is_absolute_path(const char* path) {
    return path && path[0] == '/';
}

static bool runtime_material_authored_texture_resolve_manifest_path(const char* manifest_path,
                                                                    char* out_path,
                                                                    size_t out_path_size) {
    char base_dir[RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY];
    CoreResult dir_result;
    if (!manifest_path || !manifest_path[0] || !out_path || out_path_size == 0u) {
        return false;
    }
    if (runtime_material_authored_texture_is_absolute_path(manifest_path)) {
        runtime_material_authored_texture_copy_text(out_path, out_path_size, manifest_path);
        return true;
    }
    if (!animSettings.runtimeScenePath[0]) {
        return false;
    }
    dir_result = core_scene_dirname(animSettings.runtimeScenePath, base_dir, sizeof(base_dir));
    if (dir_result.code != CORE_OK || !base_dir[0]) {
        return false;
    }
    (void)snprintf(out_path, out_path_size, "%s/%s", base_dir, manifest_path);
    return true;
}

static bool runtime_material_authored_texture_resolve_relative_to_file(const char* owner_path,
                                                                       const char* relative_path,
                                                                       char* out_path,
                                                                       size_t out_path_size) {
    char base_dir[RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY];
    CoreResult dir_result;
    if (!owner_path || !owner_path[0] || !relative_path || !relative_path[0] || !out_path ||
        out_path_size == 0u) {
        return false;
    }
    if (runtime_material_authored_texture_is_absolute_path(relative_path)) {
        runtime_material_authored_texture_copy_text(out_path, out_path_size, relative_path);
        return true;
    }
    dir_result = core_scene_dirname(owner_path, base_dir, sizeof(base_dir));
    if (dir_result.code != CORE_OK || !base_dir[0]) {
        return false;
    }
    (void)snprintf(out_path, out_path_size, "%s/%s", base_dir, relative_path);
    return true;
}

static int runtime_material_authored_texture_face_group_from_role(
    CoreAuthoredTexturePrimitiveKind primitive_kind,
    const char* face_role) {
    CoreAuthoredTextureFaceRole role = CORE_AUTHORED_TEXTURE_FACE_ROLE_NONE;
    if (!core_authored_texture_face_role_parse(face_role, &role)) {
        return -1;
    }
    if (primitive_kind == CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_PLANE) {
        return role == CORE_AUTHORED_TEXTURE_FACE_ROLE_FRONT ? 0 : -1;
    }
    if (primitive_kind == CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_RECT_PRISM) {
        switch (role) {
            case CORE_AUTHORED_TEXTURE_FACE_ROLE_FRONT: return 0;
            case CORE_AUTHORED_TEXTURE_FACE_ROLE_BACK: return 1;
            case CORE_AUTHORED_TEXTURE_FACE_ROLE_LEFT: return 2;
            case CORE_AUTHORED_TEXTURE_FACE_ROLE_RIGHT: return 3;
            case CORE_AUTHORED_TEXTURE_FACE_ROLE_TOP: return 4;
            case CORE_AUTHORED_TEXTURE_FACE_ROLE_BOTTOM: return 5;
            case CORE_AUTHORED_TEXTURE_FACE_ROLE_NONE:
            default: return -1;
        }
    }
    return -1;
}

static CoreAuthoredTextureFaceRole runtime_material_authored_texture_face_role_from_group_index(
    CoreAuthoredTexturePrimitiveKind primitive_kind,
    int face_group_index) {
    if (primitive_kind == CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_PLANE) {
        return face_group_index == 0 ? CORE_AUTHORED_TEXTURE_FACE_ROLE_FRONT
                                     : CORE_AUTHORED_TEXTURE_FACE_ROLE_NONE;
    }
    if (primitive_kind == CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_RECT_PRISM) {
        switch (face_group_index) {
            case 0: return CORE_AUTHORED_TEXTURE_FACE_ROLE_FRONT;
            case 1: return CORE_AUTHORED_TEXTURE_FACE_ROLE_BACK;
            case 2: return CORE_AUTHORED_TEXTURE_FACE_ROLE_LEFT;
            case 3: return CORE_AUTHORED_TEXTURE_FACE_ROLE_RIGHT;
            case 4: return CORE_AUTHORED_TEXTURE_FACE_ROLE_TOP;
            case 5: return CORE_AUTHORED_TEXTURE_FACE_ROLE_BOTTOM;
            default: return CORE_AUTHORED_TEXTURE_FACE_ROLE_NONE;
        }
    }
    return CORE_AUTHORED_TEXTURE_FACE_ROLE_NONE;
}

static bool runtime_material_authored_texture_parse_u8_array(json_object* object,
                                                             const char* key,
                                                             unsigned char* out_values,
                                                             size_t expected_count) {
    json_object* array = NULL;
    size_t i = 0u;
    if (!out_values || expected_count == 0u) return false;
    if (!object || !key || !json_object_object_get_ex(object, key, &array) ||
        !json_object_is_type(array, json_type_array) ||
        json_object_array_length(array) != expected_count) {
        return false;
    }
    for (i = 0u; i < expected_count; ++i) {
        json_object* value = json_object_array_get_idx(array, (int)i);
        int parsed = 0;
        if (!value || !json_object_is_type(value, json_type_int)) {
            return false;
        }
        parsed = json_object_get_int(value);
        if (parsed < 0 || parsed > 255) {
            return false;
        }
        out_values[i] = (unsigned char)parsed;
    }
    return true;
}

static bool runtime_material_authored_texture_parse_adjacent_face_groups(
    CoreAuthoredTexturePrimitiveKind primitive_kind,
    json_object* object,
    const char* key,
    CoreAuthoredTextureFaceRole* out_adjacent_face_roles,
    int* out_face_group_indices,
    size_t expected_count) {
    json_object* array = NULL;
    size_t i = 0u;
    if (!out_adjacent_face_roles || !out_face_group_indices || expected_count == 0u) return false;
    for (i = 0u; i < expected_count; ++i) {
        out_adjacent_face_roles[i] = CORE_AUTHORED_TEXTURE_FACE_ROLE_NONE;
        out_face_group_indices[i] = -1;
    }
    if (!object || !key || !json_object_object_get_ex(object, key, &array) ||
        !json_object_is_type(array, json_type_array) ||
        json_object_array_length(array) != expected_count) {
        return false;
    }
    for (i = 0u; i < expected_count; ++i) {
        json_object* value = json_object_array_get_idx(array, (int)i);
        const char* role = NULL;
        CoreAuthoredTextureFaceRole parsed_role = CORE_AUTHORED_TEXTURE_FACE_ROLE_NONE;
        if (!value || !json_object_is_type(value, json_type_string)) {
            return false;
        }
        role = json_object_get_string(value);
        if (!core_authored_texture_face_role_parse(role, &parsed_role)) {
            return false;
        }
        out_adjacent_face_roles[i] = parsed_role;
        if (parsed_role != CORE_AUTHORED_TEXTURE_FACE_ROLE_NONE) {
            out_face_group_indices[i] =
                runtime_material_authored_texture_face_group_from_role(primitive_kind, role);
            if (out_face_group_indices[i] < 0) {
                return false;
            }
        }
    }
    return true;
}

static bool runtime_material_authored_texture_parse_face_material_intent_summary_field(
    RuntimeMaterialAuthoredTextureFace* face,
    json_object* surface,
    const char* key,
    bool require_overlay_family,
    bool* out_present) {
    json_object* field = NULL;
    const char* stable_id = NULL;
    RuntimeMaterialTextureLayerKind kind = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE;
    if (out_present) {
        *out_present = false;
    }
    if (!face || !surface || !key) {
        return false;
    }
    if (!json_object_object_get_ex(surface, key, &field)) {
        return true;
    }
    if (!field || !json_object_is_type(field, json_type_string)) {
        return false;
    }
    stable_id = json_object_get_string(field);
    if (!stable_id || stable_id[0] == '\0' || strcmp(stable_id, "none") == 0) {
        if (out_present) {
            *out_present = true;
        }
        return true;
    }
    kind = RuntimeMaterialTextureLayerKindFromStableId(stable_id);
    if (kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE) {
        return false;
    }
    if (require_overlay_family) {
        if (!RuntimeMaterialTextureLayerKindIsOverlay(kind)) {
            return false;
        }
        runtime_material_authored_texture_copy_text(face->metadata.overlayMaterialIntentKind,
                                                    sizeof(face->metadata.overlayMaterialIntentKind),
                                                    stable_id);
    } else {
        if (!RuntimeMaterialTextureLayerKindIsBase(kind)) {
            return false;
        }
        runtime_material_authored_texture_copy_text(face->metadata.baseMaterialIntentKind,
                                                    sizeof(face->metadata.baseMaterialIntentKind),
                                                    stable_id);
    }
    if (out_present) {
        *out_present = true;
    }
    return true;
}

static void runtime_material_authored_texture_parse_face_material_intents(
    RuntimeMaterialAuthoredTextureFace* face,
    json_object* surface,
    bool allow_base_fallback,
    bool allow_overlay_fallback) {
    json_object* array = NULL;
    size_t i = 0u;
    if (!face || !surface ||
        !json_object_object_get_ex(surface, "layer_material_intent_stable_ids", &array) ||
        !json_object_is_type(array, json_type_array)) {
        return;
    }
    for (i = 0u; i < json_object_array_length(array); ++i) {
        json_object* value = json_object_array_get_idx(array, (int)i);
        const char* stable_id = NULL;
        RuntimeMaterialTextureLayerKind kind = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE;
        if (!value || !json_object_is_type(value, json_type_string)) {
            continue;
        }
        stable_id = json_object_get_string(value);
        kind = RuntimeMaterialTextureLayerKindFromStableId(stable_id);
        if (allow_base_fallback && RuntimeMaterialTextureLayerKindIsBase(kind)) {
            runtime_material_authored_texture_copy_text(face->metadata.baseMaterialIntentKind,
                                                        sizeof(face->metadata.baseMaterialIntentKind),
                                                        stable_id);
        } else if (allow_overlay_fallback && RuntimeMaterialTextureLayerKindIsOverlay(kind)) {
            runtime_material_authored_texture_copy_text(face->metadata.overlayMaterialIntentKind,
                                                        sizeof(face->metadata.overlayMaterialIntentKind),
                                                        stable_id);
        }
    }
}

static bool runtime_material_authored_texture_channel_source_supported(const char* source) {
    if (!source || !source[0]) return false;
    return strcmp(source, "rgba") == 0 ||
           strcmp(source, "rgb") == 0 ||
           strcmp(source, "alpha") == 0 ||
           strcmp(source, "red") == 0 ||
           strcmp(source, "green") == 0 ||
           strcmp(source, "blue") == 0 ||
           strcmp(source, "luminance") == 0;
}

static bool runtime_material_authored_texture_parse_material_channels(
    RuntimeMaterialAuthoredTextureFace* face,
    json_object* surface,
    const char* fallback_file_name) {
    json_object* channels = NULL;
    size_t channel_count = 0u;
    if (!face || !surface) return false;
    if (!json_object_object_get_ex(surface, "material_channels", &channels)) {
        return true;
    }
    if (!channels || !json_object_is_type(channels, json_type_array)) {
        return false;
    }
    channel_count = json_object_array_length(channels);
    if (channel_count > RUNTIME_MATERIAL_AUTHORED_TEXTURE_MAX_CHANNEL_REFS) {
        return false;
    }
    face->metadata.channelRefCount = 0;
    for (size_t i = 0u; i < channel_count; ++i) {
        json_object* entry = json_object_array_get_idx(channels, (int)i);
        json_object* field = NULL;
        const char* channel = NULL;
        const char* source = "rgba";
        const char* file_name = fallback_file_name;
        RuntimeMaterialAuthoredTextureChannelRef* ref = NULL;
        if (!entry || !json_object_is_type(entry, json_type_object)) {
            return false;
        }
        if (!json_object_object_get_ex(entry, "channel", &field) ||
            !json_object_is_type(field, json_type_string)) {
            return false;
        }
        channel = json_object_get_string(field);
        if (!RuntimeMaterialAuthoredTextureChannelNameSupported(channel)) {
            return false;
        }
        if (json_object_object_get_ex(entry, "source", &field)) {
            if (!field || !json_object_is_type(field, json_type_string)) {
                return false;
            }
            source = json_object_get_string(field);
        }
        if (!runtime_material_authored_texture_channel_source_supported(source)) {
            return false;
        }
        if (json_object_object_get_ex(entry, "file_name", &field)) {
            if (!field || !json_object_is_type(field, json_type_string)) {
                return false;
            }
            file_name = json_object_get_string(field);
        }
        if (!file_name || !file_name[0]) {
            return false;
        }
        ref = &face->metadata.channelRefs[face->metadata.channelRefCount++];
        ref->active = true;
        runtime_material_authored_texture_copy_text(ref->channel,
                                                    sizeof(ref->channel),
                                                    channel);
        runtime_material_authored_texture_copy_text(ref->source,
                                                    sizeof(ref->source),
                                                    source);
        runtime_material_authored_texture_copy_text(ref->fileName,
                                                    sizeof(ref->fileName),
                                                    file_name);
    }
    return true;
}

static bool runtime_material_authored_texture_parse_face_metadata(
    RuntimeMaterialAuthoredTextureBinding* binding,
    RuntimeMaterialAuthoredTextureFace* face,
    json_object* surface,
    const char* file_name) {
    json_object* field = NULL;
    bool has_base_summary = false;
    bool has_overlay_summary = false;
    CoreAuthoredTextureNetLayoutKind layout_kind = CORE_AUTHORED_TEXTURE_NET_LAYOUT_KIND_NONE;
    CoreAuthoredTextureNetSlot net_slot = CORE_AUTHORED_TEXTURE_NET_SLOT_NONE;
    CoreAuthoredTextureNetOrientation orientation = CORE_AUTHORED_TEXTURE_NET_ORIENTATION_NONE;
    CoreAuthoredTextureFaceRole adjacent_face_roles[RUNTIME_MATERIAL_AUTHORED_TEXTURE_FACE_EDGE_COUNT];
    CoreAuthoredTextureFaceRole face_role = CORE_AUTHORED_TEXTURE_FACE_ROLE_NONE;
    if (!binding || !face || !surface) return false;
    face->metadata.active = true;
    face->metadata.sceneObjectIndex = binding->sceneObjectIndex;
    face->metadata.faceGroupIndex = face->faceGroupIndex;
    if (!json_object_object_get_ex(surface, "net_layout_kind", &field) ||
        !json_object_is_type(field, json_type_string) ||
        !core_authored_texture_net_layout_kind_parse(json_object_get_string(field), &layout_kind)) {
        return false;
    }
    if (!json_object_object_get_ex(surface, "net_slot", &field) ||
        !json_object_is_type(field, json_type_string) ||
        !core_authored_texture_net_slot_parse(json_object_get_string(field), &net_slot)) {
        return false;
    }
    if (!json_object_object_get_ex(surface, "orientation", &field) ||
        !json_object_is_type(field, json_type_string) ||
        !core_authored_texture_net_orientation_parse(json_object_get_string(field), &orientation)) {
        return false;
    }
    if (!runtime_material_authored_texture_parse_u8_array(
            surface,
            "corner_ids",
            face->metadata.cornerIds,
            RUNTIME_MATERIAL_AUTHORED_TEXTURE_FACE_CORNER_COUNT) ||
        !runtime_material_authored_texture_parse_u8_array(
            surface,
            "edge_ids",
            face->metadata.edgeIds,
            RUNTIME_MATERIAL_AUTHORED_TEXTURE_FACE_EDGE_COUNT) ||
        !runtime_material_authored_texture_parse_adjacent_face_groups(
            binding->primitiveKind,
            surface,
            "adjacent_face_roles",
            adjacent_face_roles,
            face->metadata.adjacentFaceGroupIndices,
            RUNTIME_MATERIAL_AUTHORED_TEXTURE_FACE_EDGE_COUNT)) {
        return false;
    }
    face_role = runtime_material_authored_texture_face_role_from_group_index(binding->primitiveKind,
                                                                             face->faceGroupIndex);
    if (!core_authored_texture_semantic_net_validate(binding->primitiveKind,
                                                     face_role,
                                                     layout_kind,
                                                     net_slot,
                                                     orientation,
                                                     face->metadata.cornerIds,
                                                     face->metadata.edgeIds,
                                                     adjacent_face_roles)) {
        return false;
    }
    runtime_material_authored_texture_copy_text(face->metadata.netLayoutKind,
                                                sizeof(face->metadata.netLayoutKind),
                                                core_authored_texture_net_layout_kind_name(
                                                    layout_kind));
    runtime_material_authored_texture_copy_text(face->metadata.netSlot,
                                                sizeof(face->metadata.netSlot),
                                                core_authored_texture_net_slot_name(net_slot));
    runtime_material_authored_texture_copy_text(face->metadata.orientation,
                                                sizeof(face->metadata.orientation),
                                                core_authored_texture_net_orientation_name(
                                                    orientation));
    if (!runtime_material_authored_texture_parse_face_material_intent_summary_field(
            face, surface, "base_material_intent_kind", false, &has_base_summary) ||
        !runtime_material_authored_texture_parse_face_material_intent_summary_field(
            face, surface, "overlay_material_intent_kind", true, &has_overlay_summary)) {
        return false;
    }
    runtime_material_authored_texture_parse_face_material_intents(
        face, surface, !has_base_summary, !has_overlay_summary);
    if (!runtime_material_authored_texture_parse_material_channels(face, surface, file_name)) {
        return false;
    }
    if (json_object_object_get_ex(surface, "layout_offset_x", &field) &&
        (json_object_is_type(field, json_type_double) || json_object_is_type(field, json_type_int))) {
        face->metadata.layoutOffsetX = json_object_get_double(field);
    }
    if (json_object_object_get_ex(surface, "layout_offset_y", &field) &&
        (json_object_is_type(field, json_type_double) || json_object_is_type(field, json_type_int))) {
        face->metadata.layoutOffsetY = json_object_get_double(field);
    }
    return true;
}

static bool runtime_material_authored_texture_read_png_rgba(const char* path,
                                                            unsigned char** out_rgba,
                                                            int* out_width,
                                                            int* out_height) {
    FILE* png_file = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep* rows = NULL;
    unsigned char* rgba = NULL;
    png_uint_32 width = 0u;
    png_uint_32 height = 0u;
    int bit_depth = 0;
    int color_type = 0;
    bool ok = false;
    if (!path || !out_rgba || !out_width || !out_height) {
        return false;
    }
    *out_rgba = NULL;
    *out_width = 0;
    *out_height = 0;
    png_file = fopen(path, "rb");
    if (!png_file) {
        return false;
    }
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fclose(png_file);
        return false;
    }
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(png_file);
        return false;
    }
    if (setjmp(png_jmpbuf(png_ptr))) {
        ok = false;
        goto cleanup;
    }
    png_init_io(png_ptr, png_file);
    png_read_info(png_ptr, info_ptr);
    width = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);
    if (bit_depth == 16) png_set_strip_16(png_ptr);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    }
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png_ptr);
    }
    png_read_update_info(png_ptr, info_ptr);
    if (width == 0u || height == 0u || width > 8192u || height > 8192u) {
        goto cleanup;
    }
    rgba = (unsigned char*)malloc((size_t)width * (size_t)height * 4u);
    rows = (png_bytep*)malloc(sizeof(png_bytep) * (size_t)height);
    if (!rgba || !rows) {
        goto cleanup;
    }
    for (png_uint_32 y = 0u; y < height; ++y) {
        rows[y] = rgba + ((size_t)y * (size_t)width * 4u);
    }
    png_read_image(png_ptr, rows);
    png_read_end(png_ptr, info_ptr);
    *out_rgba = rgba;
    *out_width = (int)width;
    *out_height = (int)height;
    rgba = NULL;
    ok = true;
cleanup:
    free(rows);
    free(rgba);
    if (png_ptr || info_ptr) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    }
    if (png_file) {
        fclose(png_file);
    }
    return ok;
}

static bool runtime_material_authored_texture_load_face_array(RuntimeMaterialAuthoredTextureBinding* binding,
                                                              json_object* manifest_root,
                                                              const char* manifest_resolved_path,
                                                              const char* array_key,
                                                              RuntimeMaterialAuthoredTextureFace* faces,
                                                              int* out_face_count) {
    json_object* surfaces = NULL;
    bool seen_face_groups[RUNTIME_MATERIAL_AUTHORED_TEXTURE_MAX_FACES] = {false};
    size_t i = 0u;
    if (!binding || !manifest_root || !manifest_resolved_path || !array_key || !faces ||
        !out_face_count) {
        return false;
    }
    if (!json_object_object_get_ex(manifest_root, array_key, &surfaces) ||
        !json_object_is_type(surfaces, json_type_array)) {
        return false;
    }
    *out_face_count = 0;
    for (i = 0u; i < json_object_array_length(surfaces); ++i) {
        json_object* surface = json_object_array_get_idx(surfaces, (int)i);
        json_object* face_role_obj = NULL;
        json_object* file_name_obj = NULL;
        const char* face_role = NULL;
        const char* file_name = NULL;
        char png_path[RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY];
        unsigned char* rgba = NULL;
        int width = 0;
        int height = 0;
        int face_group_index = -1;
        RuntimeMaterialAuthoredTextureFace* face = NULL;
        if (!surface || !json_object_is_type(surface, json_type_object)) {
            goto fail;
        }
        if (!json_object_object_get_ex(surface, "face_role", &face_role_obj) ||
            !json_object_is_type(face_role_obj, json_type_string) ||
            !json_object_object_get_ex(surface, "file_name", &file_name_obj) ||
            !json_object_is_type(file_name_obj, json_type_string)) {
            goto fail;
        }
        face_role = json_object_get_string(face_role_obj);
        file_name = json_object_get_string(file_name_obj);
        face_group_index = runtime_material_authored_texture_face_group_from_role(
            binding->primitiveKind, face_role);
        if (face_group_index < 0 ||
            face_group_index >= RUNTIME_MATERIAL_AUTHORED_TEXTURE_MAX_FACES) {
            goto fail;
        }
        if (seen_face_groups[face_group_index]) {
            goto fail;
        }
        if (!runtime_material_authored_texture_resolve_relative_to_file(manifest_resolved_path,
                                                                        file_name,
                                                                        png_path,
                                                                        sizeof(png_path)) ||
            !runtime_material_authored_texture_read_png_rgba(png_path, &rgba, &width, &height)) {
            goto fail;
        }
        face = &faces[face_group_index];
        runtime_material_authored_texture_face_reset(face);
        face->active = true;
        face->faceGroupIndex = face_group_index;
        face->width = width;
        face->height = height;
        face->rgba = rgba;
        if (!runtime_material_authored_texture_parse_face_metadata(binding,
                                                                   face,
                                                                   surface,
                                                                   file_name)) {
            goto fail;
        }
        seen_face_groups[face_group_index] = true;
        *out_face_count += 1;
    }
    return *out_face_count > 0;
fail:
    for (i = 0u; i < (size_t)RUNTIME_MATERIAL_AUTHORED_TEXTURE_MAX_FACES; ++i) {
        runtime_material_authored_texture_face_reset(&faces[i]);
    }
    *out_face_count = 0;
    return false;
}

bool RuntimeMaterialAuthoredTextureBindManifestForObject(int scene_object_index,
                                                         const char* object_id,
                                                         const char* manifest_path,
                                                         const char* binding_mode) {
    RuntimeMaterialAuthoredTextureBinding* binding =
        runtime_material_authored_texture_binding_at(scene_object_index);
    char resolved_manifest_path[RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY];
    json_object* manifest_root = NULL;
    json_object* field = NULL;
    const char* manifest_object_id = NULL;
    const char* overlay_material_intent_kind = NULL;
    CoreAuthoredTextureManifestContract manifest_contract;
    const char* failure_reason = "manifest validation failed";
    bool ok = false;
    if (!binding || !object_id || !object_id[0] || !manifest_path || !manifest_path[0]) {
        return false;
    }
    runtime_material_authored_texture_binding_reset(binding);
    if (!runtime_material_authored_texture_resolve_manifest_path(manifest_path,
                                                                 resolved_manifest_path,
                                                                 sizeof(resolved_manifest_path))) {
        runtime_material_authored_texture_binding_record_invalid(binding,
                                                                 scene_object_index,
                                                                 manifest_path,
                                                                 binding_mode,
                                                                 "manifest path unresolved");
        return false;
    }
    manifest_root = json_object_from_file(resolved_manifest_path);
    if (!manifest_root || !json_object_is_type(manifest_root, json_type_object)) {
        failure_reason = "manifest unreadable";
        goto cleanup;
    }
    if (!runtime_material_authored_texture_manifest_parse_contract(manifest_root,
                                                                   &manifest_contract)) {
        failure_reason = "schema or output contract invalid";
        goto cleanup;
    }
    if (!json_object_object_get_ex(manifest_root, "source_object_id", &field) ||
        !json_object_is_type(field, json_type_string)) {
        failure_reason = "source object id missing";
        goto cleanup;
    }
    manifest_object_id = json_object_get_string(field);
    if (!manifest_object_id || strcmp(manifest_object_id, object_id) != 0) {
        failure_reason = "source object mismatch";
        goto cleanup;
    }
    binding->primitiveKind = manifest_contract.primitive_kind;
    binding->sceneObjectIndex = scene_object_index;
    runtime_material_authored_texture_copy_text(binding->objectId,
                                                sizeof(binding->objectId),
                                                object_id);
    runtime_material_authored_texture_copy_text(binding->manifestPath,
                                                sizeof(binding->manifestPath),
                                                manifest_path);
    runtime_material_authored_texture_copy_text(binding->resolvedManifestPath,
                                                sizeof(binding->resolvedManifestPath),
                                                resolved_manifest_path);
    runtime_material_authored_texture_copy_text(binding->bindingMode,
                                                sizeof(binding->bindingMode),
                                                binding_mode && binding_mode[0] ? binding_mode
                                                                                : "override");
    runtime_material_authored_texture_copy_text(binding->overlayMaterialIntentKind,
                                                sizeof(binding->overlayMaterialIntentKind),
                                                "none");
    if (json_object_object_get_ex(manifest_root, "overlay_material_intent_kind", &field) &&
        json_object_is_type(field, json_type_string)) {
        overlay_material_intent_kind = json_object_get_string(field);
        runtime_material_authored_texture_copy_text(binding->overlayMaterialIntentKind,
                                                    sizeof(binding->overlayMaterialIntentKind),
                                                    overlay_material_intent_kind);
    }
    if (!runtime_material_authored_texture_load_face_array(binding,
                                                           manifest_root,
                                                           resolved_manifest_path,
                                                           manifest_contract.output_kind ==
                                                                   CORE_AUTHORED_TEXTURE_OUTPUT_KIND_LEGACY_FLATTENED
                                                               ? "surfaces"
                                                               : "base_surfaces",
                                                           binding->baseFaces,
                                                           &binding->baseFaceCount) ||
        !runtime_material_authored_texture_faces_complete(binding->primitiveKind,
                                                          binding->baseFaces,
                                                          binding->baseFaceCount)) {
        failure_reason = "base lane incomplete";
        goto cleanup;
    }
    if (manifest_contract.output_kind == CORE_AUTHORED_TEXTURE_OUTPUT_KIND_BASE_PLUS_OVERLAY) {
        if (!runtime_material_authored_texture_load_face_array(binding,
                                                               manifest_root,
                                                               resolved_manifest_path,
                                                               "overlay_surfaces",
                                                               binding->overlayFaces,
                                                               &binding->overlayFaceCount) ||
            !runtime_material_authored_texture_faces_complete(binding->primitiveKind,
                                                              binding->overlayFaces,
                                                              binding->overlayFaceCount)) {
            failure_reason = "overlay lane incomplete";
            goto cleanup;
        }
    }
    binding->active = true;
    ok = true;
cleanup:
    if (manifest_root) {
        json_object_put(manifest_root);
    }
    if (!ok) {
        runtime_material_authored_texture_binding_record_invalid(binding,
                                                                 scene_object_index,
                                                                 manifest_path,
                                                                 binding_mode,
                                                                 failure_reason);
    }
    return ok;
}

bool RuntimeMaterialAuthoredTextureGetBinding(int scene_object_index,
                                              char* out_manifest_path,
                                              size_t out_manifest_path_size,
                                              char* out_binding_mode,
                                              size_t out_binding_mode_size,
                                              int* out_face_count) {
    RuntimeMaterialAuthoredTextureBinding* binding =
        runtime_material_authored_texture_binding_at(scene_object_index);
    if (!binding || !binding->active) {
        return false;
    }
    if (out_manifest_path && out_manifest_path_size > 0u) {
        runtime_material_authored_texture_copy_text(out_manifest_path,
                                                    out_manifest_path_size,
                                                    binding->manifestPath);
    }
    if (out_binding_mode && out_binding_mode_size > 0u) {
        runtime_material_authored_texture_copy_text(out_binding_mode,
                                                    out_binding_mode_size,
                                                    binding->bindingMode);
    }
    if (out_face_count) {
        *out_face_count = binding->baseFaceCount;
    }
    return true;
}

bool RuntimeMaterialAuthoredTextureGetInvalidBinding(int scene_object_index,
                                                     char* out_manifest_path,
                                                     size_t out_manifest_path_size,
                                                     char* out_binding_mode,
                                                     size_t out_binding_mode_size,
                                                     char* out_reason,
                                                     size_t out_reason_size) {
    RuntimeMaterialAuthoredTextureBinding* binding =
        runtime_material_authored_texture_binding_at(scene_object_index);
    if (!binding || binding->active || !binding->invalidActive) {
        return false;
    }
    if (out_manifest_path && out_manifest_path_size > 0u) {
        runtime_material_authored_texture_copy_text(out_manifest_path,
                                                    out_manifest_path_size,
                                                    binding->invalidManifestPath);
    }
    if (out_binding_mode && out_binding_mode_size > 0u) {
        runtime_material_authored_texture_copy_text(out_binding_mode,
                                                    out_binding_mode_size,
                                                    binding->invalidBindingMode);
    }
    if (out_reason && out_reason_size > 0u) {
        runtime_material_authored_texture_copy_text(out_reason,
                                                    out_reason_size,
                                                    binding->invalidReason);
    }
    return true;
}

bool RuntimeMaterialAuthoredTextureGetFaceMetadata(
    int scene_object_index,
    int face_group_index,
    RuntimeMaterialAuthoredTextureFaceMetadata* out_metadata) {
    RuntimeMaterialAuthoredTextureBinding* binding =
        runtime_material_authored_texture_binding_at(scene_object_index);
    RuntimeMaterialAuthoredTextureFace* base_face = NULL;
    RuntimeMaterialAuthoredTextureFace* overlay_face = NULL;
    if (out_metadata) {
        runtime_material_authored_texture_face_metadata_reset(out_metadata);
        out_metadata->sceneObjectIndex = scene_object_index;
        out_metadata->faceGroupIndex = face_group_index;
    }
    if (!binding || !binding->active || face_group_index < 0 ||
        face_group_index >= RUNTIME_MATERIAL_AUTHORED_TEXTURE_MAX_FACES || !out_metadata) {
        return false;
    }
    base_face = &binding->baseFaces[face_group_index];
    if (!base_face->active) {
        return false;
    }
    *out_metadata = base_face->metadata;
    overlay_face = &binding->overlayFaces[face_group_index];
    if (overlay_face->active && overlay_face->metadata.overlayMaterialIntentKind[0]) {
        runtime_material_authored_texture_copy_text(out_metadata->overlayMaterialIntentKind,
                                                    sizeof(out_metadata->overlayMaterialIntentKind),
                                                    overlay_face->metadata.overlayMaterialIntentKind);
    }
    return true;
}

bool RuntimeMaterialAuthoredTextureGetOverlayMaterialIntent(int scene_object_index,
                                                            char* out_overlay_material_intent,
                                                            size_t out_overlay_material_intent_size) {
    RuntimeMaterialAuthoredTextureBinding* binding =
        runtime_material_authored_texture_binding_at(scene_object_index);
    if (!binding || !binding->active || !out_overlay_material_intent ||
        out_overlay_material_intent_size == 0u) {
        return false;
    }
    runtime_material_authored_texture_copy_text(out_overlay_material_intent,
                                                out_overlay_material_intent_size,
                                                binding->overlayMaterialIntentKind);
    return true;
}
