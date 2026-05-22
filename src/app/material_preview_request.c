#include "app/material_preview_request.h"

#include <json-c/json.h>
#include <stdio.h>
#include <string.h>

static void material_preview_request_set_diag(char* out,
                                              size_t out_size,
                                              const char* message) {
    if (!out || out_size == 0u) return;
    if (!message) message = "material preview request error";
    snprintf(out, out_size, "%s", message);
}

static void material_preview_request_dirname(const char* path,
                                             char* out_dir,
                                             size_t out_dir_size) {
    const char* last_slash = NULL;
    size_t len = 0u;
    if (!out_dir || out_dir_size == 0u) return;
    out_dir[0] = '\0';
    if (!path || !path[0]) return;
    last_slash = strrchr(path, '/');
    if (!last_slash) {
        snprintf(out_dir, out_dir_size, ".");
        return;
    }
    len = (size_t)(last_slash - path);
    if (len == 0u) {
        snprintf(out_dir, out_dir_size, "/");
        return;
    }
    if (len >= out_dir_size) len = out_dir_size - 1u;
    memcpy(out_dir, path, len);
    out_dir[len] = '\0';
}

static void material_preview_request_resolve_path(const char* request_dir,
                                                  const char* raw_path,
                                                  char* out_path,
                                                  size_t out_path_size) {
    if (!out_path || out_path_size == 0u) return;
    out_path[0] = '\0';
    if (!raw_path || !raw_path[0]) return;
    if (raw_path[0] == '/') {
        snprintf(out_path, out_path_size, "%s", raw_path);
        return;
    }
    snprintf(out_path, out_path_size, "%s/%s", request_dir ? request_dir : ".", raw_path);
}

static bool material_preview_request_parse_double_any(json_object* obj,
                                                      const char* key_a,
                                                      const char* key_b,
                                                      double* out_value) {
    json_object* value = NULL;
    if (!obj || !out_value) return false;
    if (key_a &&
        json_object_object_get_ex(obj, key_a, &value) &&
        (json_object_is_type(value, json_type_double) || json_object_is_type(value, json_type_int))) {
        *out_value = json_object_get_double(value);
        return true;
    }
    if (key_b &&
        json_object_object_get_ex(obj, key_b, &value) &&
        (json_object_is_type(value, json_type_double) || json_object_is_type(value, json_type_int))) {
        *out_value = json_object_get_double(value);
        return true;
    }
    return false;
}

static bool material_preview_request_parse_int_any(json_object* obj,
                                                   const char* key_a,
                                                   const char* key_b,
                                                   int* out_value) {
    json_object* value = NULL;
    if (!obj || !out_value) return false;
    if (key_a &&
        json_object_object_get_ex(obj, key_a, &value) &&
        (json_object_is_type(value, json_type_int) || json_object_is_type(value, json_type_double))) {
        *out_value = json_object_get_int(value);
        return true;
    }
    if (key_b &&
        json_object_object_get_ex(obj, key_b, &value) &&
        (json_object_is_type(value, json_type_int) || json_object_is_type(value, json_type_double))) {
        *out_value = json_object_get_int(value);
        return true;
    }
    return false;
}

static void material_preview_request_parse_variant(json_object* variant_obj,
                                                   MaterialPreviewVariantOverrides* out_variant) {
    json_object* label_obj = NULL;
    json_object* preview_overlay_obj = NULL;
    json_object* overlay_kind_obj = NULL;
    if (!variant_obj || !out_variant || !json_object_is_type(variant_obj, json_type_object)) return;
    memset(out_variant, 0, sizeof(*out_variant));
    if (json_object_object_get_ex(variant_obj, "label", &label_obj) &&
        json_object_is_type(label_obj, json_type_string)) {
        snprintf(out_variant->label,
                 sizeof(out_variant->label),
                 "%s",
                 json_object_get_string(label_obj));
    }
    out_variant->has_alpha =
        material_preview_request_parse_double_any(variant_obj, "alpha", "transparency", &out_variant->alpha);
    out_variant->has_reflectivity =
        material_preview_request_parse_double_any(variant_obj, "reflectivity", NULL, &out_variant->reflectivity);
    out_variant->has_roughness =
        material_preview_request_parse_double_any(variant_obj, "roughness", NULL, &out_variant->roughness);
    out_variant->has_emissive_strength =
        material_preview_request_parse_double_any(variant_obj, "emissive_strength", "emissiveStrength", &out_variant->emissive_strength);
    out_variant->has_texture_id =
        material_preview_request_parse_int_any(variant_obj, "texture_id", "textureId", &out_variant->texture_id);
    out_variant->has_texture_strength =
        material_preview_request_parse_double_any(variant_obj, "texture_strength", "textureStrength", &out_variant->texture_strength);
    out_variant->has_texture_scale =
        material_preview_request_parse_double_any(variant_obj, "texture_scale", "textureScale", &out_variant->texture_scale);
    out_variant->has_texture_offset_u =
        material_preview_request_parse_double_any(variant_obj, "texture_offset_u", "textureOffsetU", &out_variant->texture_offset_u);
    out_variant->has_texture_offset_v =
        material_preview_request_parse_double_any(variant_obj, "texture_offset_v", "textureOffsetV", &out_variant->texture_offset_v);
    out_variant->has_texture_seed =
        material_preview_request_parse_int_any(variant_obj, "texture_seed", "textureSeed", &out_variant->texture_seed);
    out_variant->has_texture_pattern_mode =
        material_preview_request_parse_int_any(variant_obj, "texture_pattern_mode", "texturePatternMode", &out_variant->texture_pattern_mode);
    out_variant->has_texture_coverage =
        material_preview_request_parse_double_any(variant_obj, "texture_coverage", "textureCoverage", &out_variant->texture_coverage);
    out_variant->has_texture_grain =
        material_preview_request_parse_double_any(variant_obj, "texture_grain", "textureGrain", &out_variant->texture_grain);
    out_variant->has_texture_edge_softness =
        material_preview_request_parse_double_any(variant_obj, "texture_edge_softness", "textureEdgeSoftness", &out_variant->texture_edge_softness);
    out_variant->has_texture_contrast =
        material_preview_request_parse_double_any(variant_obj, "texture_contrast", "textureContrast", &out_variant->texture_contrast);
    out_variant->has_texture_flow =
        material_preview_request_parse_double_any(variant_obj, "texture_flow", "textureFlow", &out_variant->texture_flow);
    out_variant->has_texture_color_depth =
        material_preview_request_parse_double_any(variant_obj, "texture_color_depth", "textureColorDepth", &out_variant->texture_color_depth);
    out_variant->has_texture_surface_damage =
        material_preview_request_parse_double_any(variant_obj, "texture_surface_damage", "textureSurfaceDamage", &out_variant->texture_surface_damage);
    if (json_object_object_get_ex(variant_obj, "preview_overlay", &preview_overlay_obj) &&
        json_object_is_type(preview_overlay_obj, json_type_object) &&
        json_object_object_get_ex(preview_overlay_obj, "kind", &overlay_kind_obj) &&
        json_object_is_type(overlay_kind_obj, json_type_string)) {
        out_variant->has_preview_overlay = true;
        snprintf(out_variant->preview_overlay_kind,
                 sizeof(out_variant->preview_overlay_kind),
                 "%s",
                 json_object_get_string(overlay_kind_obj));
        out_variant->preview_overlay_opacity = 1.0;
        material_preview_request_parse_double_any(preview_overlay_obj, "opacity", NULL, &out_variant->preview_overlay_opacity);
        out_variant->has_preview_overlay_scale =
            material_preview_request_parse_double_any(preview_overlay_obj, "scale", NULL, &out_variant->preview_overlay_scale);
        out_variant->has_preview_overlay_strength =
            material_preview_request_parse_double_any(preview_overlay_obj, "strength", NULL, &out_variant->preview_overlay_strength);
        out_variant->has_preview_overlay_offset_u =
            material_preview_request_parse_double_any(preview_overlay_obj, "offset_u", "offsetU", &out_variant->preview_overlay_offset_u);
        out_variant->has_preview_overlay_offset_v =
            material_preview_request_parse_double_any(preview_overlay_obj, "offset_v", "offsetV", &out_variant->preview_overlay_offset_v);
        out_variant->has_preview_overlay_pattern_mode =
            material_preview_request_parse_int_any(preview_overlay_obj, "pattern_mode", "patternMode", &out_variant->preview_overlay_pattern_mode);
        out_variant->has_preview_overlay_coverage =
            material_preview_request_parse_double_any(preview_overlay_obj, "coverage", NULL, &out_variant->preview_overlay_coverage);
        out_variant->has_preview_overlay_grain =
            material_preview_request_parse_double_any(preview_overlay_obj, "grain", NULL, &out_variant->preview_overlay_grain);
        out_variant->has_preview_overlay_edge_softness =
            material_preview_request_parse_double_any(preview_overlay_obj, "edge_softness", "edgeSoftness", &out_variant->preview_overlay_edge_softness);
        out_variant->has_preview_overlay_contrast =
            material_preview_request_parse_double_any(preview_overlay_obj, "contrast", NULL, &out_variant->preview_overlay_contrast);
        out_variant->has_preview_overlay_flow =
            material_preview_request_parse_double_any(preview_overlay_obj, "flow", NULL, &out_variant->preview_overlay_flow);
        out_variant->has_preview_overlay_color_depth =
            material_preview_request_parse_double_any(preview_overlay_obj, "color_depth", "colorDepth", &out_variant->preview_overlay_color_depth);
        out_variant->has_preview_overlay_surface_damage =
            material_preview_request_parse_double_any(preview_overlay_obj, "surface_damage", "surfaceDamage", &out_variant->preview_overlay_surface_damage);
        out_variant->has_preview_overlay_seed =
            material_preview_request_parse_int_any(preview_overlay_obj, "seed", NULL, &out_variant->preview_overlay_seed);
    }
}

bool MaterialPreviewRequestLoadFromFile(const char* request_path,
                                        MaterialPreviewRequest* out_request,
                                        char* out_diagnostics,
                                        size_t out_diagnostics_size) {
    FILE* file = NULL;
    json_object* root = NULL;
    json_object* value = NULL;
    char request_dir[MATERIAL_PREVIEW_MAX_PATH];
    if (!request_path || !out_request) return false;
    memset(out_request, 0, sizeof(*out_request));
    out_request->cell_width = 256;
    out_request->cell_height = 256;
    out_request->columns = 5;
    out_request->background_color_rgb = 0u;
    snprintf(out_request->schema, sizeof(out_request->schema), "%s", MATERIAL_PREVIEW_REQUEST_SCHEMA);
    snprintf(out_request->request_path, sizeof(out_request->request_path), "%s", request_path);
    material_preview_request_dirname(request_path, request_dir, sizeof(request_dir));
    file = fopen(request_path, "rb");
    if (!file) {
        material_preview_request_set_diag(out_diagnostics, out_diagnostics_size, "failed to open request");
        return false;
    }
    root = json_object_from_file(request_path);
    fclose(file);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        material_preview_request_set_diag(out_diagnostics, out_diagnostics_size, "failed to parse request json");
        return false;
    }
    if (!json_object_object_get_ex(root, "schema", &value) ||
        !json_object_is_type(value, json_type_string) ||
        strcmp(json_object_get_string(value), MATERIAL_PREVIEW_REQUEST_SCHEMA) != 0) {
        json_object_put(root);
        material_preview_request_set_diag(out_diagnostics, out_diagnostics_size, "invalid schema");
        return false;
    }
    if (!json_object_object_get_ex(root, "runtime_scene_path", &value) ||
        !json_object_is_type(value, json_type_string)) {
        json_object_put(root);
        material_preview_request_set_diag(out_diagnostics, out_diagnostics_size, "missing runtime_scene_path");
        return false;
    }
    material_preview_request_resolve_path(request_dir,
                                          json_object_get_string(value),
                                          out_request->runtime_scene_path,
                                          sizeof(out_request->runtime_scene_path));
    if (json_object_object_get_ex(root, "object_id", &value) &&
        json_object_is_type(value, json_type_string)) {
        snprintf(out_request->object_id, sizeof(out_request->object_id), "%s", json_object_get_string(value));
    }
    if (material_preview_request_parse_int_any(root,
                                               "scene_object_index",
                                               "sceneObjectIndex",
                                               &out_request->scene_object_index)) {
        out_request->has_scene_object_index = true;
    }
    if (!out_request->object_id[0] && !out_request->has_scene_object_index) {
        json_object_put(root);
        material_preview_request_set_diag(out_diagnostics, out_diagnostics_size, "missing object_id or scene_object_index");
        return false;
    }
    if (!json_object_object_get_ex(root, "output_path", &value) ||
        !json_object_is_type(value, json_type_string)) {
        json_object_put(root);
        material_preview_request_set_diag(out_diagnostics, out_diagnostics_size, "missing output_path");
        return false;
    }
    material_preview_request_resolve_path(request_dir,
                                          json_object_get_string(value),
                                          out_request->output_path,
                                          sizeof(out_request->output_path));
    if (json_object_object_get_ex(root, "summary_path", &value) &&
        json_object_is_type(value, json_type_string)) {
        material_preview_request_resolve_path(request_dir,
                                              json_object_get_string(value),
                                              out_request->summary_path,
                                              sizeof(out_request->summary_path));
    }
    material_preview_request_parse_int_any(root, "width", "cell_width", &out_request->cell_width);
    material_preview_request_parse_int_any(root, "height", "cell_height", &out_request->cell_height);
    material_preview_request_parse_int_any(root, "columns", NULL, &out_request->columns);
    {
        int background_color = 0;
        if (material_preview_request_parse_int_any(root,
                                                   "background_color",
                                                   "backgroundColor",
                                                   &background_color)) {
            out_request->background_color_rgb = (unsigned int)background_color;
            out_request->has_background_color = true;
        }
    }
    if (out_request->cell_width < 32) out_request->cell_width = 32;
    if (out_request->cell_height < 32) out_request->cell_height = 32;
    if (out_request->columns < 1) out_request->columns = 1;
    if (json_object_object_get_ex(root, "variants", &value) &&
        json_object_is_type(value, json_type_array)) {
        size_t count = json_object_array_length(value);
        if (count > MATERIAL_PREVIEW_MAX_VARIANTS) count = MATERIAL_PREVIEW_MAX_VARIANTS;
        out_request->variant_count = (int)count;
        for (size_t i = 0; i < count; ++i) {
            material_preview_request_parse_variant(json_object_array_get_idx(value, (int)i),
                                                   &out_request->variants[i]);
        }
    }
    json_object_put(root);
    material_preview_request_set_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}
