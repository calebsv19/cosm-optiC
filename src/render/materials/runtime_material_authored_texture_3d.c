#include "render/runtime_material_authored_texture_3d.h"

#include <math.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <json-c/json.h>

#include "config/config_manager.h"
#include "core_scene.h"

typedef enum RuntimeMaterialAuthoredPrimitiveKind {
    RUNTIME_MATERIAL_AUTHORED_PRIMITIVE_KIND_NONE = 0,
    RUNTIME_MATERIAL_AUTHORED_PRIMITIVE_KIND_PLANE = 1,
    RUNTIME_MATERIAL_AUTHORED_PRIMITIVE_KIND_RECT_PRISM = 2
} RuntimeMaterialAuthoredPrimitiveKind;

typedef struct RuntimeMaterialAuthoredTextureFace {
    bool active;
    int faceGroupIndex;
    int width;
    int height;
    unsigned char* rgba;
    RuntimeMaterialAuthoredTextureFaceMetadata metadata;
} RuntimeMaterialAuthoredTextureFace;

typedef struct RuntimeMaterialAuthoredTextureBinding {
    bool active;
    int sceneObjectIndex;
    RuntimeMaterialAuthoredPrimitiveKind primitiveKind;
    int faceCount;
    char objectId[64];
    char manifestPath[RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY];
    char resolvedManifestPath[RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY];
    char bindingMode[RUNTIME_MATERIAL_AUTHORED_TEXTURE_MODE_CAPACITY];
    RuntimeMaterialAuthoredTextureFace faces[RUNTIME_MATERIAL_AUTHORED_TEXTURE_MAX_FACES];
} RuntimeMaterialAuthoredTextureBinding;

static RuntimeMaterialAuthoredTextureBinding s_authored_texture_bindings[MAX_OBJECTS];

static void runtime_material_authored_texture_copy_text(char* dst,
                                                        size_t dst_size,
                                                        const char* src) {
    if (!dst || dst_size == 0u) return;
    if (!src) src = "";
    (void)snprintf(dst, dst_size, "%s", src);
}

static bool runtime_material_authored_texture_is_absolute_path(const char* path) {
    return path && path[0] == '/';
}

static double runtime_material_authored_texture_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static void runtime_material_authored_texture_face_metadata_reset(
    RuntimeMaterialAuthoredTextureFaceMetadata* metadata) {
    int i = 0;
    if (!metadata) return;
    memset(metadata, 0, sizeof(*metadata));
    for (i = 0; i < RUNTIME_MATERIAL_AUTHORED_TEXTURE_FACE_EDGE_COUNT; ++i) {
        metadata->adjacentFaceGroupIndices[i] = -1;
    }
}

static RuntimeMaterialAuthoredTextureBinding* runtime_material_authored_texture_binding_at(
    int scene_object_index) {
    if (scene_object_index < 0 || scene_object_index >= MAX_OBJECTS) {
        return NULL;
    }
    return &s_authored_texture_bindings[scene_object_index];
}

static void runtime_material_authored_texture_face_reset(RuntimeMaterialAuthoredTextureFace* face) {
    if (!face) return;
    free(face->rgba);
    runtime_material_authored_texture_face_metadata_reset(&face->metadata);
    memset(face, 0, sizeof(*face));
    face->faceGroupIndex = -1;
    face->metadata.faceGroupIndex = -1;
    for (int i = 0; i < RUNTIME_MATERIAL_AUTHORED_TEXTURE_FACE_EDGE_COUNT; ++i) {
        face->metadata.adjacentFaceGroupIndices[i] = -1;
    }
}

static void runtime_material_authored_texture_binding_reset(
    RuntimeMaterialAuthoredTextureBinding* binding) {
    int i = 0;
    if (!binding) return;
    for (i = 0; i < RUNTIME_MATERIAL_AUTHORED_TEXTURE_MAX_FACES; ++i) {
        runtime_material_authored_texture_face_reset(&binding->faces[i]);
    }
    memset(binding, 0, sizeof(*binding));
    binding->sceneObjectIndex = -1;
}

void RuntimeMaterialAuthoredTextureResetAll(void) {
    int i = 0;
    for (i = 0; i < MAX_OBJECTS; ++i) {
        runtime_material_authored_texture_binding_reset(&s_authored_texture_bindings[i]);
    }
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

static RuntimeMaterialAuthoredPrimitiveKind runtime_material_authored_texture_primitive_kind_from_manifest(
    const char* primitive_kind) {
    if (!primitive_kind) return RUNTIME_MATERIAL_AUTHORED_PRIMITIVE_KIND_NONE;
    if (strcmp(primitive_kind, "PLANE") == 0) {
        return RUNTIME_MATERIAL_AUTHORED_PRIMITIVE_KIND_PLANE;
    }
    if (strcmp(primitive_kind, "RECT_PRISM") == 0) {
        return RUNTIME_MATERIAL_AUTHORED_PRIMITIVE_KIND_RECT_PRISM;
    }
    return RUNTIME_MATERIAL_AUTHORED_PRIMITIVE_KIND_NONE;
}

static int runtime_material_authored_texture_face_group_from_role(
    RuntimeMaterialAuthoredPrimitiveKind primitive_kind,
    const char* face_role) {
    if (!face_role) return -1;
    if (primitive_kind == RUNTIME_MATERIAL_AUTHORED_PRIMITIVE_KIND_PLANE) {
        return strcmp(face_role, "FRONT") == 0 ? 0 : -1;
    }
    if (primitive_kind == RUNTIME_MATERIAL_AUTHORED_PRIMITIVE_KIND_RECT_PRISM) {
        if (strcmp(face_role, "FRONT") == 0) return 0;
        if (strcmp(face_role, "BACK") == 0) return 1;
        if (strcmp(face_role, "LEFT") == 0) return 2;
        if (strcmp(face_role, "RIGHT") == 0) return 3;
        if (strcmp(face_role, "TOP") == 0) return 4;
        if (strcmp(face_role, "BOTTOM") == 0) return 5;
    }
    return -1;
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

static void runtime_material_authored_texture_parse_adjacent_face_groups(
    RuntimeMaterialAuthoredPrimitiveKind primitive_kind,
    json_object* object,
    const char* key,
    int* out_face_group_indices,
    size_t expected_count) {
    json_object* array = NULL;
    size_t i = 0u;
    if (!out_face_group_indices || expected_count == 0u) return;
    for (i = 0u; i < expected_count; ++i) {
        out_face_group_indices[i] = -1;
    }
    if (!object || !key || !json_object_object_get_ex(object, key, &array) ||
        !json_object_is_type(array, json_type_array)) {
        return;
    }
    for (i = 0u; i < expected_count && i < json_object_array_length(array); ++i) {
        json_object* value = json_object_array_get_idx(array, (int)i);
        const char* role = NULL;
        if (!value || !json_object_is_type(value, json_type_string)) {
            continue;
        }
        role = json_object_get_string(value);
        out_face_group_indices[i] =
            runtime_material_authored_texture_face_group_from_role(primitive_kind, role);
    }
}

static void runtime_material_authored_texture_parse_face_metadata(
    RuntimeMaterialAuthoredTextureBinding* binding,
    RuntimeMaterialAuthoredTextureFace* face,
    json_object* surface) {
    json_object* field = NULL;
    if (!binding || !face || !surface) return;
    face->metadata.active = true;
    face->metadata.sceneObjectIndex = binding->sceneObjectIndex;
    face->metadata.faceGroupIndex = face->faceGroupIndex;
    if (json_object_object_get_ex(surface, "net_layout_kind", &field) &&
        json_object_is_type(field, json_type_string)) {
        runtime_material_authored_texture_copy_text(face->metadata.netLayoutKind,
                                                    sizeof(face->metadata.netLayoutKind),
                                                    json_object_get_string(field));
    }
    if (json_object_object_get_ex(surface, "net_slot", &field) &&
        json_object_is_type(field, json_type_string)) {
        runtime_material_authored_texture_copy_text(face->metadata.netSlot,
                                                    sizeof(face->metadata.netSlot),
                                                    json_object_get_string(field));
    }
    if (json_object_object_get_ex(surface, "orientation", &field) &&
        json_object_is_type(field, json_type_string)) {
        runtime_material_authored_texture_copy_text(face->metadata.orientation,
                                                    sizeof(face->metadata.orientation),
                                                    json_object_get_string(field));
    }
    (void)runtime_material_authored_texture_parse_u8_array(surface,
                                                           "corner_ids",
                                                           face->metadata.cornerIds,
                                                           RUNTIME_MATERIAL_AUTHORED_TEXTURE_FACE_CORNER_COUNT);
    (void)runtime_material_authored_texture_parse_u8_array(surface,
                                                           "edge_ids",
                                                           face->metadata.edgeIds,
                                                           RUNTIME_MATERIAL_AUTHORED_TEXTURE_FACE_EDGE_COUNT);
    runtime_material_authored_texture_parse_adjacent_face_groups(binding->primitiveKind,
                                                                 surface,
                                                                 "adjacent_face_roles",
                                                                 face->metadata.adjacentFaceGroupIndices,
                                                                 RUNTIME_MATERIAL_AUTHORED_TEXTURE_FACE_EDGE_COUNT);
    if (json_object_object_get_ex(surface, "layout_offset_x", &field) &&
        (json_object_is_type(field, json_type_double) || json_object_is_type(field, json_type_int))) {
        face->metadata.layoutOffsetX = json_object_get_double(field);
    }
    if (json_object_object_get_ex(surface, "layout_offset_y", &field) &&
        (json_object_is_type(field, json_type_double) || json_object_is_type(field, json_type_int))) {
        face->metadata.layoutOffsetY = json_object_get_double(field);
    }
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

static bool runtime_material_authored_texture_load_faces(RuntimeMaterialAuthoredTextureBinding* binding,
                                                         json_object* manifest_root,
                                                         const char* manifest_resolved_path) {
    json_object* surfaces = NULL;
    size_t i = 0u;
    if (!binding || !manifest_root || !manifest_resolved_path) {
        return false;
    }
    if (!json_object_object_get_ex(manifest_root, "surfaces", &surfaces) ||
        !json_object_is_type(surfaces, json_type_array)) {
        return false;
    }
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
            continue;
        }
        if (!json_object_object_get_ex(surface, "face_role", &face_role_obj) ||
            !json_object_is_type(face_role_obj, json_type_string) ||
            !json_object_object_get_ex(surface, "file_name", &file_name_obj) ||
            !json_object_is_type(file_name_obj, json_type_string)) {
            continue;
        }
        face_role = json_object_get_string(face_role_obj);
        file_name = json_object_get_string(file_name_obj);
        face_group_index = runtime_material_authored_texture_face_group_from_role(
            binding->primitiveKind, face_role);
        if (face_group_index < 0 ||
            face_group_index >= RUNTIME_MATERIAL_AUTHORED_TEXTURE_MAX_FACES ||
            !runtime_material_authored_texture_resolve_relative_to_file(manifest_resolved_path,
                                                                        file_name,
                                                                        png_path,
                                                                        sizeof(png_path)) ||
            !runtime_material_authored_texture_read_png_rgba(png_path, &rgba, &width, &height)) {
            continue;
        }
        face = &binding->faces[face_group_index];
        runtime_material_authored_texture_face_reset(face);
        face->active = true;
        face->faceGroupIndex = face_group_index;
        face->width = width;
        face->height = height;
        face->rgba = rgba;
        runtime_material_authored_texture_parse_face_metadata(binding, face, surface);
        binding->faceCount += 1;
    }
    return binding->faceCount > 0;
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
    const char* primitive_kind = NULL;
    bool ok = false;
    if (!binding || !object_id || !object_id[0] || !manifest_path || !manifest_path[0]) {
        return false;
    }
    runtime_material_authored_texture_binding_reset(binding);
    if (!runtime_material_authored_texture_resolve_manifest_path(manifest_path,
                                                                 resolved_manifest_path,
                                                                 sizeof(resolved_manifest_path))) {
        return false;
    }
    manifest_root = json_object_from_file(resolved_manifest_path);
    if (!manifest_root || !json_object_is_type(manifest_root, json_type_object)) {
        goto cleanup;
    }
    if (!json_object_object_get_ex(manifest_root, "source_object_id", &field) ||
        !json_object_is_type(field, json_type_string)) {
        goto cleanup;
    }
    manifest_object_id = json_object_get_string(field);
    if (!manifest_object_id || strcmp(manifest_object_id, object_id) != 0) {
        goto cleanup;
    }
    if (!json_object_object_get_ex(manifest_root, "primitive_kind", &field) ||
        !json_object_is_type(field, json_type_string)) {
        goto cleanup;
    }
    primitive_kind = json_object_get_string(field);
    binding->primitiveKind =
        runtime_material_authored_texture_primitive_kind_from_manifest(primitive_kind);
    if (binding->primitiveKind == RUNTIME_MATERIAL_AUTHORED_PRIMITIVE_KIND_NONE) {
        goto cleanup;
    }
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
    if (!runtime_material_authored_texture_load_faces(binding,
                                                      manifest_root,
                                                      resolved_manifest_path)) {
        goto cleanup;
    }
    binding->active = true;
    ok = true;
cleanup:
    if (manifest_root) {
        json_object_put(manifest_root);
    }
    if (!ok) {
        runtime_material_authored_texture_binding_reset(binding);
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
        *out_face_count = binding->faceCount;
    }
    return true;
}

bool RuntimeMaterialAuthoredTextureGetFaceMetadata(
    int scene_object_index,
    int face_group_index,
    RuntimeMaterialAuthoredTextureFaceMetadata* out_metadata) {
    RuntimeMaterialAuthoredTextureBinding* binding =
        runtime_material_authored_texture_binding_at(scene_object_index);
    RuntimeMaterialAuthoredTextureFace* face = NULL;
    if (out_metadata) {
        runtime_material_authored_texture_face_metadata_reset(out_metadata);
        out_metadata->sceneObjectIndex = scene_object_index;
        out_metadata->faceGroupIndex = face_group_index;
    }
    if (!binding || !binding->active || face_group_index < 0 ||
        face_group_index >= RUNTIME_MATERIAL_AUTHORED_TEXTURE_MAX_FACES || !out_metadata) {
        return false;
    }
    face = &binding->faces[face_group_index];
    if (!face->active) {
        return false;
    }
    *out_metadata = face->metadata;
    return true;
}

static void runtime_material_authored_texture_sample_channel(
    const RuntimeMaterialAuthoredTextureFace* face,
    double x,
    double y,
    double* out_r,
    double* out_g,
    double* out_b,
    double* out_a) {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    double tx = 0.0;
    double ty = 0.0;
    double accum_r = 0.0;
    double accum_g = 0.0;
    double accum_b = 0.0;
    double accum_a = 0.0;
    int sx[2];
    int sy[2];
    double wx[2];
    double wy[2];
    if (!face || !face->rgba || face->width <= 0 || face->height <= 0) {
        return;
    }
    x = runtime_material_authored_texture_clamp01(x) * (double)(face->width - 1);
    y = runtime_material_authored_texture_clamp01(y) * (double)(face->height - 1);
    x0 = (int)floor(x);
    y0 = (int)floor(y);
    x1 = x0 < face->width - 1 ? x0 + 1 : x0;
    y1 = y0 < face->height - 1 ? y0 + 1 : y0;
    tx = x - (double)x0;
    ty = y - (double)y0;
    sx[0] = x0;
    sx[1] = x1;
    sy[0] = y0;
    sy[1] = y1;
    wx[0] = 1.0 - tx;
    wx[1] = tx;
    wy[0] = 1.0 - ty;
    wy[1] = ty;
    for (int iy = 0; iy < 2; ++iy) {
        for (int ix = 0; ix < 2; ++ix) {
            size_t pixel_index = ((size_t)sy[iy] * (size_t)face->width + (size_t)sx[ix]) * 4u;
            double weight = wx[ix] * wy[iy];
            accum_r += ((double)face->rgba[pixel_index + 0u] / 255.0) * weight;
            accum_g += ((double)face->rgba[pixel_index + 1u] / 255.0) * weight;
            accum_b += ((double)face->rgba[pixel_index + 2u] / 255.0) * weight;
            accum_a += ((double)face->rgba[pixel_index + 3u] / 255.0) * weight;
        }
    }
    if (out_r) *out_r = accum_r;
    if (out_g) *out_g = accum_g;
    if (out_b) *out_b = accum_b;
    if (out_a) *out_a = accum_a;
}

bool RuntimeMaterialAuthoredTextureSampleFace(int scene_object_index,
                                              int face_group_index,
                                              double u,
                                              double v,
                                              RuntimeMaterialAuthoredTextureSample* out_sample) {
    RuntimeMaterialAuthoredTextureBinding* binding =
        runtime_material_authored_texture_binding_at(scene_object_index);
    RuntimeMaterialAuthoredTextureFace* face = NULL;
    if (out_sample) {
        memset(out_sample, 0, sizeof(*out_sample));
        out_sample->sceneObjectIndex = scene_object_index;
        out_sample->faceGroupIndex = face_group_index;
        out_sample->u = u;
        out_sample->v = v;
    }
    if (!binding || !binding->active || face_group_index < 0 ||
        face_group_index >= RUNTIME_MATERIAL_AUTHORED_TEXTURE_MAX_FACES) {
        return false;
    }
    face = &binding->faces[face_group_index];
    if (!face->active || !face->rgba) {
        return false;
    }
    if (out_sample) {
        out_sample->active = true;
        runtime_material_authored_texture_sample_channel(face,
                                                         u,
                                                         v,
                                                         &out_sample->colorR,
                                                         &out_sample->colorG,
                                                         &out_sample->colorB,
                                                         &out_sample->alpha);
    }
    return true;
}
