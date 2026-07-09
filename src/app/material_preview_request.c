#include "app/material_preview_request.h"
#include "app/ray_tracing_request_utils.h"

#include <json-c/json.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void material_preview_set_diagf(char* out, size_t out_size, const char* format, ...) {
    va_list args;
    if (!out || out_size == 0u || !format) return;
    va_start(args, format);
    vsnprintf(out, out_size, format, args);
    va_end(args);
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
    out_variant->has_material_id =
        RayTracingJsonGetIntAny(variant_obj, "material_id", "materialId", &out_variant->material_id);
    out_variant->has_object_color =
        RayTracingJsonGetIntAny(variant_obj, "object_color", "objectColor", &out_variant->object_color);
    out_variant->has_alpha =
        RayTracingJsonGetDoubleAny(variant_obj, "alpha", "transparency", &out_variant->alpha);
    out_variant->has_reflectivity =
        RayTracingJsonGetDoubleAny(variant_obj, "reflectivity", NULL, &out_variant->reflectivity);
    out_variant->has_roughness =
        RayTracingJsonGetDoubleAny(variant_obj, "roughness", NULL, &out_variant->roughness);
    out_variant->has_emissive_strength =
        RayTracingJsonGetDoubleAny(variant_obj, "emissive_strength", "emissiveStrength", &out_variant->emissive_strength);
    out_variant->has_texture_id =
        RayTracingJsonGetIntAny(variant_obj, "texture_id", "textureId", &out_variant->texture_id);
    out_variant->has_texture_strength =
        RayTracingJsonGetDoubleAny(variant_obj, "texture_strength", "textureStrength", &out_variant->texture_strength);
    out_variant->has_texture_scale =
        RayTracingJsonGetDoubleAny(variant_obj, "texture_scale", "textureScale", &out_variant->texture_scale);
    out_variant->has_texture_offset_u =
        RayTracingJsonGetDoubleAny(variant_obj, "texture_offset_u", "textureOffsetU", &out_variant->texture_offset_u);
    out_variant->has_texture_offset_v =
        RayTracingJsonGetDoubleAny(variant_obj, "texture_offset_v", "textureOffsetV", &out_variant->texture_offset_v);
    out_variant->has_texture_seed =
        RayTracingJsonGetIntAny(variant_obj, "texture_seed", "textureSeed", &out_variant->texture_seed);
    out_variant->has_texture_pattern_mode =
        RayTracingJsonGetIntAny(variant_obj, "texture_pattern_mode", "texturePatternMode", &out_variant->texture_pattern_mode);
    out_variant->has_texture_coverage =
        RayTracingJsonGetDoubleAny(variant_obj, "texture_coverage", "textureCoverage", &out_variant->texture_coverage);
    out_variant->has_texture_grain =
        RayTracingJsonGetDoubleAny(variant_obj, "texture_grain", "textureGrain", &out_variant->texture_grain);
    out_variant->has_texture_edge_softness =
        RayTracingJsonGetDoubleAny(variant_obj, "texture_edge_softness", "textureEdgeSoftness", &out_variant->texture_edge_softness);
    out_variant->has_texture_contrast =
        RayTracingJsonGetDoubleAny(variant_obj, "texture_contrast", "textureContrast", &out_variant->texture_contrast);
    out_variant->has_texture_flow =
        RayTracingJsonGetDoubleAny(variant_obj, "texture_flow", "textureFlow", &out_variant->texture_flow);
    out_variant->has_texture_color_depth =
        RayTracingJsonGetDoubleAny(variant_obj, "texture_color_depth", "textureColorDepth", &out_variant->texture_color_depth);
    out_variant->has_texture_surface_damage =
        RayTracingJsonGetDoubleAny(variant_obj, "texture_surface_damage", "textureSurfaceDamage", &out_variant->texture_surface_damage);
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
        RayTracingJsonGetDoubleAny(preview_overlay_obj, "opacity", NULL, &out_variant->preview_overlay_opacity);
        out_variant->has_preview_overlay_scale =
            RayTracingJsonGetDoubleAny(preview_overlay_obj, "scale", NULL, &out_variant->preview_overlay_scale);
        out_variant->has_preview_overlay_strength =
            RayTracingJsonGetDoubleAny(preview_overlay_obj, "strength", NULL, &out_variant->preview_overlay_strength);
        out_variant->has_preview_overlay_roughness_influence =
            RayTracingJsonGetDoubleAny(preview_overlay_obj,
                                       "roughness_influence",
                                       "roughnessInfluence",
                                       &out_variant->preview_overlay_roughness_influence);
        out_variant->has_preview_overlay_reflectivity_influence =
            RayTracingJsonGetDoubleAny(preview_overlay_obj,
                                       "reflectivity_influence",
                                       "reflectivityInfluence",
                                       &out_variant->preview_overlay_reflectivity_influence);
        out_variant->has_preview_overlay_specular_influence =
            RayTracingJsonGetDoubleAny(preview_overlay_obj,
                                       "specular_influence",
                                       "specularInfluence",
                                       &out_variant->preview_overlay_specular_influence);
        out_variant->has_preview_overlay_diffuse_influence =
            RayTracingJsonGetDoubleAny(preview_overlay_obj,
                                       "diffuse_influence",
                                       "diffuseInfluence",
                                       &out_variant->preview_overlay_diffuse_influence);
        out_variant->has_preview_overlay_transparency_influence =
            RayTracingJsonGetDoubleAny(preview_overlay_obj,
                                       "transparency_influence",
                                       "transparencyInfluence",
                                       &out_variant->preview_overlay_transparency_influence);
        out_variant->has_preview_overlay_offset_u =
            RayTracingJsonGetDoubleAny(preview_overlay_obj, "offset_u", "offsetU", &out_variant->preview_overlay_offset_u);
        out_variant->has_preview_overlay_offset_v =
            RayTracingJsonGetDoubleAny(preview_overlay_obj, "offset_v", "offsetV", &out_variant->preview_overlay_offset_v);
        out_variant->has_preview_overlay_pattern_mode =
            RayTracingJsonGetIntAny(preview_overlay_obj, "pattern_mode", "patternMode", &out_variant->preview_overlay_pattern_mode);
        out_variant->has_preview_overlay_coverage =
            RayTracingJsonGetDoubleAny(preview_overlay_obj, "coverage", NULL, &out_variant->preview_overlay_coverage);
        out_variant->has_preview_overlay_grain =
            RayTracingJsonGetDoubleAny(preview_overlay_obj, "grain", NULL, &out_variant->preview_overlay_grain);
        out_variant->has_preview_overlay_edge_softness =
            RayTracingJsonGetDoubleAny(preview_overlay_obj, "edge_softness", "edgeSoftness", &out_variant->preview_overlay_edge_softness);
        out_variant->has_preview_overlay_contrast =
            RayTracingJsonGetDoubleAny(preview_overlay_obj, "contrast", NULL, &out_variant->preview_overlay_contrast);
        out_variant->has_preview_overlay_flow =
            RayTracingJsonGetDoubleAny(preview_overlay_obj, "flow", NULL, &out_variant->preview_overlay_flow);
        out_variant->has_preview_overlay_color_depth =
            RayTracingJsonGetDoubleAny(preview_overlay_obj, "color_depth", "colorDepth", &out_variant->preview_overlay_color_depth);
        out_variant->has_preview_overlay_surface_damage =
            RayTracingJsonGetDoubleAny(preview_overlay_obj, "surface_damage", "surfaceDamage", &out_variant->preview_overlay_surface_damage);
        out_variant->has_preview_overlay_seed =
            RayTracingJsonGetIntAny(preview_overlay_obj, "seed", NULL, &out_variant->preview_overlay_seed);
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
    RayTracingDirnameOf(request_path, request_dir, sizeof(request_dir));
    file = fopen(request_path, "rb");
    if (!file) {
        material_preview_set_diagf(out_diagnostics,
                                   out_diagnostics_size,
                                   "request=%s field=<file> failed to open request",
                                   request_path);
        return false;
    }
    root = json_object_from_file(request_path);
    fclose(file);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        material_preview_set_diagf(out_diagnostics,
                                   out_diagnostics_size,
                                   "request=%s field=<root> failed to parse request json object",
                                   request_path);
        return false;
    }
    if (!json_object_object_get_ex(root, "schema", &value) ||
        !json_object_is_type(value, json_type_string) ||
        strcmp(json_object_get_string(value), MATERIAL_PREVIEW_REQUEST_SCHEMA) != 0) {
        const char* actual_schema =
            (value && json_object_is_type(value, json_type_string))
                ? json_object_get_string(value)
                : "<missing-or-non-string>";
        material_preview_set_diagf(out_diagnostics,
                                   out_diagnostics_size,
                                   "request=%s field=schema expected=%s actual=%s",
                                   request_path,
                                   MATERIAL_PREVIEW_REQUEST_SCHEMA,
                                   actual_schema ? actual_schema : "<null>");
        json_object_put(root);
        return false;
    }
    if (!json_object_object_get_ex(root, "runtime_scene_path", &value) ||
        !json_object_is_type(value, json_type_string)) {
        json_object_put(root);
        material_preview_set_diagf(out_diagnostics,
                                   out_diagnostics_size,
                                   "request=%s field=runtime_scene_path missing or non-string",
                                   request_path);
        return false;
    }
    if (!RayTracingResolveExistingRequestPath(request_dir,
                                             json_object_get_string(value),
                                             out_request->runtime_scene_path,
                                             sizeof(out_request->runtime_scene_path))) {
        json_object_put(root);
        material_preview_set_diagf(out_diagnostics,
                                   out_diagnostics_size,
                                   "request=%s field=runtime_scene_path not found or path too long",
                                   request_path);
        return false;
    }
    if (json_object_object_get_ex(root, "object_id", &value) &&
        json_object_is_type(value, json_type_string)) {
        snprintf(out_request->object_id, sizeof(out_request->object_id), "%s", json_object_get_string(value));
    }
    if (RayTracingJsonGetIntAny(root,
                                               "scene_object_index",
                                               "sceneObjectIndex",
                                               &out_request->scene_object_index)) {
        out_request->has_scene_object_index = true;
    }
    if (!out_request->object_id[0] && !out_request->has_scene_object_index) {
        json_object_put(root);
        material_preview_set_diagf(out_diagnostics,
                                   out_diagnostics_size,
                                   "request=%s field=object_id|scene_object_index missing target object",
                                   request_path);
        return false;
    }
    if (!json_object_object_get_ex(root, "output_path", &value) ||
        !json_object_is_type(value, json_type_string)) {
        json_object_put(root);
        material_preview_set_diagf(out_diagnostics,
                                   out_diagnostics_size,
                                   "request=%s field=output_path missing or non-string",
                                   request_path);
        return false;
    }
    if (!RayTracingResolveRequestOutputPath(request_dir,
                                           json_object_get_string(value),
                                           out_request->output_path,
                                           sizeof(out_request->output_path))) {
        json_object_put(root);
        material_preview_set_diagf(out_diagnostics,
                                   out_diagnostics_size,
                                   "request=%s field=output_path invalid or path too long",
                                   request_path);
        return false;
    }
    if (json_object_object_get_ex(root, "summary_path", &value) &&
        json_object_is_type(value, json_type_string)) {
        if (!RayTracingResolveRequestOutputPath(request_dir,
                                               json_object_get_string(value),
                                               out_request->summary_path,
                                               sizeof(out_request->summary_path))) {
            json_object_put(root);
            material_preview_set_diagf(out_diagnostics,
                                       out_diagnostics_size,
                                       "request=%s field=summary_path invalid or path too long",
                                       request_path);
            return false;
        }
    }
    RayTracingJsonGetIntAny(root, "width", "cell_width", &out_request->cell_width);
    RayTracingJsonGetIntAny(root, "height", "cell_height", &out_request->cell_height);
    RayTracingJsonGetIntAny(root, "columns", NULL, &out_request->columns);
    {
        int background_color = 0;
        if (RayTracingJsonGetIntAny(root,
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
    RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}
