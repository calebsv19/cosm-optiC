#include "app/material_preview_headless.h"

#include <SDL2/SDL.h>
#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "app/animation.h"
#include "config/config_file_io.h"
#include "config/config_manager.h"
#include "editor/scene_editor_material_stack.h"
#include "import/runtime_scene_bridge.h"
#include "material/material_manager.h"
#include "render/material_bsdf.h"
#include "render/runtime_material_texture_3d.h"
#include "render/runtime_material_texture_stack_3d.h"

typedef struct MaterialPreviewResolvedVariant {
    char label[MATERIAL_PREVIEW_MAX_VARIANT_LABEL];
    SceneObject object;
} MaterialPreviewResolvedVariant;

static void material_preview_set_diag(char* out, size_t out_size, const char* message) {
    if (!out || out_size == 0u) return;
    if (!message) message = "material preview error";
    snprintf(out, out_size, "%s", message);
}

static double material_preview_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static void material_preview_apply_variant(SceneObject* object,
                                           const MaterialPreviewVariantOverrides* variant) {
    RuntimeMaterialTexture3DParams params;
    if (!object || !variant) return;
    params = RuntimeMaterialTexture3DParamsFromObject(object);
    if (variant->has_alpha) object->alpha = material_preview_clamp01(variant->alpha);
    if (variant->has_reflectivity) object->reflectivity = material_preview_clamp01(variant->reflectivity);
    if (variant->has_roughness) object->roughness = material_preview_clamp01(variant->roughness);
    if (variant->has_emissive_strength) object->emissiveStrength = material_preview_clamp01(variant->emissive_strength);
    if (variant->has_texture_id) object->textureId = variant->texture_id;
    if (variant->has_texture_strength) object->textureStrength = material_preview_clamp01(variant->texture_strength);
    if (variant->has_texture_scale) object->textureScale = variant->texture_scale;
    if (variant->has_texture_offset_u) object->textureOffsetU = variant->texture_offset_u;
    if (variant->has_texture_offset_v) object->textureOffsetV = variant->texture_offset_v;
    if (variant->has_texture_seed) params.seed = variant->texture_seed;
    if (variant->has_texture_pattern_mode) params.patternMode = variant->texture_pattern_mode;
    if (variant->has_texture_coverage) params.coverage = variant->texture_coverage;
    if (variant->has_texture_grain) params.grain = variant->texture_grain;
    if (variant->has_texture_edge_softness) params.edgeSoftness = variant->texture_edge_softness;
    if (variant->has_texture_contrast) params.contrast = variant->texture_contrast;
    if (variant->has_texture_flow) params.flow = variant->texture_flow;
    if (variant->has_texture_color_depth) params.colorDepth = variant->texture_color_depth;
    if (variant->has_texture_surface_damage) params.surfaceDamage = variant->texture_surface_damage;
    params = RuntimeMaterialTexture3DNormalizeParams(params);
    object->texturePatternMode = params.patternMode;
    object->textureCoverage = params.coverage;
    object->textureGrain = params.grain;
    object->textureEdgeSoftness = params.edgeSoftness;
    object->textureContrast = params.contrast;
    object->textureFlow = params.flow;
    object->textureColorDepth = params.colorDepth;
    object->textureSurfaceDamage = params.surfaceDamage;
    object->textureSeed = params.seed;
    if (!(object->textureScale > 1e-6)) object->textureScale = 1.0;
}

static bool material_preview_find_scene_index(const MaterialPreviewRequest* request,
                                              int* out_scene_index) {
    char object_id[64];
    if (!request || !out_scene_index) return false;
    if (request->has_scene_object_index &&
        request->scene_object_index >= 0 &&
        request->scene_object_index < sceneSettings.objectCount) {
        *out_scene_index = request->scene_object_index;
        return true;
    }
    if (!request->object_id[0]) return false;
    for (int i = 0; i < sceneSettings.objectCount; ++i) {
        memset(object_id, 0, sizeof(object_id));
        if (!runtime_scene_bridge_get_last_object_id_for_scene_index(i, object_id, sizeof(object_id))) continue;
        if (strcmp(object_id, request->object_id) == 0) {
            *out_scene_index = i;
            return true;
        }
    }
    return false;
}

static void material_preview_checker_color(int x,
                                           int y,
                                           const MaterialPreviewRequest* request,
                                           Uint8* r,
                                           Uint8* g,
                                           Uint8* b) {
    if (request && request->has_background_color) {
        if (r) *r = (Uint8)((request->background_color_rgb >> 16) & 0xFFu);
        if (g) *g = (Uint8)((request->background_color_rgb >> 8) & 0xFFu);
        if (b) *b = (Uint8)(request->background_color_rgb & 0xFFu);
        return;
    }
    int block = ((x / 24) + (y / 24)) & 1;
    Uint8 light = 232;
    Uint8 dark = 188;
    Uint8 value = block ? light : dark;
    if (r) *r = value;
    if (g) *g = value;
    if (b) *b = value;
}

static void material_preview_eval_surface(const SceneObject* object,
                                          int scene_index,
                                          const MaterialPreviewVariantOverrides* variant,
                                          double u,
                                          double v,
                                          RuntimeMaterialSurfaceEval* out_eval) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialSurfaceEval base_eval;
    RuntimeMaterialSurfaceEval surface_eval = {0};
    MaterialBSDF bsdf;
    const Material* material = NULL;
    double transparency = 0.0;
    MaterialBSDFInitFromSceneObject(object, &bsdf);
    material = MaterialManagerGet(object->material_id);
    transparency = material ? material_preview_clamp01(material->transparency * material_preview_clamp01(object->alpha)) : 0.0;
    base_eval = RuntimeMaterialSurfaceEvalMakeBase(bsdf.baseColorR,
                                                   bsdf.baseColorG,
                                                   bsdf.baseColorB,
                                                   bsdf.roughness,
                                                   bsdf.reflectivity,
                                                   bsdf.specWeight,
                                                   bsdf.diffuseWeight,
                                                   transparency);
    if (!SceneEditorMaterialStackGetEffectiveObjectStack(object, scene_index, &stack)) {
        RuntimeMaterialTextureStackBuildLegacyFromObject(object, &stack);
    }
    if (variant && variant->has_preview_overlay &&
        stack.layerCount < RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS) {
        RuntimeMaterialTextureLayer overlay =
            RuntimeMaterialTextureLayerMakeOverlay(
                RuntimeMaterialTextureLayerKindFromStableId(variant->preview_overlay_kind));
        overlay.opacity = material_preview_clamp01(variant->preview_overlay_opacity);
        if (variant->has_preview_overlay_scale) overlay.placement.scale = variant->preview_overlay_scale;
        if (variant->has_preview_overlay_strength) overlay.placement.strength = variant->preview_overlay_strength;
        if (variant->has_preview_overlay_offset_u) overlay.placement.offsetU = variant->preview_overlay_offset_u;
        if (variant->has_preview_overlay_offset_v) overlay.placement.offsetV = variant->preview_overlay_offset_v;
        if (variant->has_preview_overlay_pattern_mode) overlay.params.patternMode = variant->preview_overlay_pattern_mode;
        if (variant->has_preview_overlay_coverage) overlay.params.coverage = variant->preview_overlay_coverage;
        if (variant->has_preview_overlay_grain) overlay.params.grain = variant->preview_overlay_grain;
        if (variant->has_preview_overlay_edge_softness) overlay.params.edgeSoftness = variant->preview_overlay_edge_softness;
        if (variant->has_preview_overlay_contrast) overlay.params.contrast = variant->preview_overlay_contrast;
        if (variant->has_preview_overlay_flow) overlay.params.flow = variant->preview_overlay_flow;
        if (variant->has_preview_overlay_color_depth) overlay.params.colorDepth = variant->preview_overlay_color_depth;
        if (variant->has_preview_overlay_surface_damage) overlay.params.surfaceDamage = variant->preview_overlay_surface_damage;
        if (variant->has_preview_overlay_seed) overlay.params.seed = variant->preview_overlay_seed;
        overlay.params = RuntimeMaterialTexture3DNormalizeParams(overlay.params);
        overlay.placement.params = overlay.params;
        overlay = RuntimeMaterialTextureLayerNormalize(overlay);
        if (overlay.kind != RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE) {
            stack.layers[stack.layerCount++] = overlay;
            stack = RuntimeMaterialTextureStackNormalize(stack);
        }
    }
    if (!RuntimeMaterialTextureStackEvaluatePlacedUV(&stack,
                                                     object,
                                                     u,
                                                     v,
                                                     object->textureSeed != 0 ? object->textureSeed : scene_index + 1,
                                                     &base_eval,
                                                     &surface_eval)) {
        surface_eval = base_eval;
    }
    if (out_eval) *out_eval = surface_eval;
}

static void material_preview_shade_pixel(const RuntimeMaterialSurfaceEval* eval,
                                         const SceneObject* object,
                                         double u,
                                         double v,
                                         Uint8 bg_r,
                                         Uint8 bg_g,
                                         Uint8 bg_b,
                                         Uint8* out_r,
                                         Uint8* out_g,
                                         Uint8* out_b) {
    double x = (u - 0.5) * 1.6;
    double y = (v - 0.5) * 1.6;
    double nz = 1.0 / sqrt(1.0 + x * x + y * y);
    double nx = x * nz;
    double ny = y * nz;
    double lx = -0.42;
    double ly = -0.58;
    double lz = 0.70;
    double ll = sqrt(lx * lx + ly * ly + lz * lz);
    double ndotl = 0.0;
    double hx = 0.0;
    double hy = 0.0;
    double hz = 0.0;
    double hlen = 0.0;
    double spec = 0.0;
    double shininess = 0.0;
    double diffuse = 0.0;
    double emissive = 0.0;
    double alpha = 1.0 - material_preview_clamp01(eval ? eval->transparency : 0.0);
    double base_r = eval ? eval->colorR : 1.0;
    double base_g = eval ? eval->colorG : 1.0;
    double base_b = eval ? eval->colorB : 1.0;
    double roughness = eval ? material_preview_clamp01(eval->roughness) : material_preview_clamp01(object->roughness);
    double reflectivity = eval ? material_preview_clamp01(eval->reflectivity) : material_preview_clamp01(object->reflectivity);
    lx /= ll;
    ly /= ll;
    lz /= ll;
    ndotl = fmax(0.0, nx * lx + ny * ly + nz * lz);
    diffuse = 0.30 + ndotl * 0.70;
    hx = lx;
    hy = ly;
    hz = lz + 1.0;
    hlen = sqrt(hx * hx + hy * hy + hz * hz);
    hx /= hlen;
    hy /= hlen;
    hz /= hlen;
    shininess = 8.0 + ((1.0 - roughness) * 88.0);
    spec = pow(fmax(0.0, nx * hx + ny * hy + nz * hz), shininess) * (0.08 + reflectivity * 0.92);
    emissive = material_preview_clamp01(object->emissiveStrength) * 0.28;
    base_r = material_preview_clamp01(base_r * diffuse + spec + emissive);
    base_g = material_preview_clamp01(base_g * diffuse + spec + emissive);
    base_b = material_preview_clamp01(base_b * diffuse + spec + emissive);
    *out_r = (Uint8)lround(((double)bg_r * (1.0 - alpha)) + (base_r * 255.0 * alpha));
    *out_g = (Uint8)lround(((double)bg_g * (1.0 - alpha)) + (base_g * 255.0 * alpha));
    *out_b = (Uint8)lround(((double)bg_b * (1.0 - alpha)) + (base_b * 255.0 * alpha));
}

static bool material_preview_write_surface(const char* output_path,
                                           int width,
                                           int height,
                                           Uint32* pixels) {
    SDL_Surface* surface = NULL;
    if (!output_path || !pixels) return false;
    if (!config_io_ensure_parent_directory_for_file(output_path)) return false;
    surface = SDL_CreateRGBSurfaceFrom((void*)pixels,
                                       width,
                                       height,
                                       32,
                                       width * (int)sizeof(Uint32),
                                       0x00FF0000,
                                       0x0000FF00,
                                       0x000000FF,
                                       0xFF000000);
    if (!surface) return false;
    if (SDL_SaveBMP(surface, output_path) != 0) {
        SDL_FreeSurface(surface);
        return false;
    }
    SDL_FreeSurface(surface);
    return true;
}

static bool material_preview_write_summary(const MaterialPreviewRequest* request,
                                           int scene_index,
                                           const MaterialPreviewResolvedVariant* variants,
                                           int variant_count) {
    json_object* root = NULL;
    json_object* previews = NULL;
    const char* selected_object_id = request->object_id[0] ? request->object_id : "";
    if (!request->summary_path[0]) return true;
    if (!config_io_ensure_parent_directory_for_file(request->summary_path)) return false;
    root = json_object_new_object();
    previews = json_object_new_array();
    json_object_object_add(root, "schema", json_object_new_string("ray_tracing_material_preview_summary_v1"));
    json_object_object_add(root, "runtime_scene_path", json_object_new_string(request->runtime_scene_path));
    json_object_object_add(root, "object_id", json_object_new_string(selected_object_id));
    json_object_object_add(root, "scene_object_index", json_object_new_int(scene_index));
    json_object_object_add(root, "output_path", json_object_new_string(request->output_path));
    json_object_object_add(root, "cell_width", json_object_new_int(request->cell_width));
    json_object_object_add(root, "cell_height", json_object_new_int(request->cell_height));
    json_object_object_add(root, "variant_count", json_object_new_int(variant_count));
    if (request->has_background_color) {
        json_object_object_add(root,
                               "background_color",
                               json_object_new_int((int)request->background_color_rgb));
    }
    for (int i = 0; i < variant_count; ++i) {
        const SceneObject* object = &variants[i].object;
        RuntimeMaterialTexture3DParams params = RuntimeMaterialTexture3DParamsFromObject(object);
        json_object* entry = json_object_new_object();
        json_object_object_add(entry, "label", json_object_new_string(variants[i].label[0] ? variants[i].label : "base"));
        json_object_object_add(entry, "alpha", json_object_new_double(object->alpha));
        json_object_object_add(entry, "reflectivity", json_object_new_double(object->reflectivity));
        json_object_object_add(entry, "roughness", json_object_new_double(object->roughness));
        json_object_object_add(entry, "emissive_strength", json_object_new_double(object->emissiveStrength));
        json_object_object_add(entry, "texture_id", json_object_new_int(object->textureId));
        json_object_object_add(entry, "texture_strength", json_object_new_double(object->textureStrength));
        json_object_object_add(entry, "texture_scale", json_object_new_double(object->textureScale));
        json_object_object_add(entry, "texture_offset_u", json_object_new_double(object->textureOffsetU));
        json_object_object_add(entry, "texture_offset_v", json_object_new_double(object->textureOffsetV));
        json_object_object_add(entry, "texture_seed", json_object_new_int(params.seed));
        json_object_object_add(entry, "texture_pattern_mode", json_object_new_int(params.patternMode));
        json_object_object_add(entry, "texture_coverage", json_object_new_double(params.coverage));
        json_object_object_add(entry, "texture_grain", json_object_new_double(params.grain));
        json_object_object_add(entry, "texture_edge_softness", json_object_new_double(params.edgeSoftness));
        json_object_object_add(entry, "texture_contrast", json_object_new_double(params.contrast));
        json_object_object_add(entry, "texture_flow", json_object_new_double(params.flow));
        json_object_object_add(entry, "texture_color_depth", json_object_new_double(params.colorDepth));
        json_object_object_add(entry, "texture_surface_damage", json_object_new_double(params.surfaceDamage));
        if (request->variant_count > i && request->variants[i].has_preview_overlay) {
            const MaterialPreviewVariantOverrides* source_variant = &request->variants[i];
            json_object* overlay = json_object_new_object();
            json_object_object_add(overlay, "kind", json_object_new_string(source_variant->preview_overlay_kind));
            json_object_object_add(overlay, "opacity", json_object_new_double(source_variant->preview_overlay_opacity));
            if (source_variant->has_preview_overlay_scale) {
                json_object_object_add(overlay, "scale", json_object_new_double(source_variant->preview_overlay_scale));
            }
            if (source_variant->has_preview_overlay_strength) {
                json_object_object_add(overlay, "strength", json_object_new_double(source_variant->preview_overlay_strength));
            }
            if (source_variant->has_preview_overlay_pattern_mode) {
                json_object_object_add(overlay, "pattern_mode", json_object_new_int(source_variant->preview_overlay_pattern_mode));
            }
            if (source_variant->has_preview_overlay_coverage) {
                json_object_object_add(overlay, "coverage", json_object_new_double(source_variant->preview_overlay_coverage));
            }
            if (source_variant->has_preview_overlay_grain) {
                json_object_object_add(overlay, "grain", json_object_new_double(source_variant->preview_overlay_grain));
            }
            if (source_variant->has_preview_overlay_contrast) {
                json_object_object_add(overlay, "contrast", json_object_new_double(source_variant->preview_overlay_contrast));
            }
            if (source_variant->has_preview_overlay_surface_damage) {
                json_object_object_add(overlay, "surface_damage", json_object_new_double(source_variant->preview_overlay_surface_damage));
            }
            json_object_object_add(entry, "preview_overlay", overlay);
        }
        json_object_array_add(previews, entry);
    }
    json_object_object_add(root, "variants", previews);
    json_object_to_file_ext(request->summary_path, root, JSON_C_TO_STRING_PRETTY);
    json_object_put(root);
    return true;
}

bool MaterialPreviewHeadlessRun(const MaterialPreviewRequest* request,
                                char* out_diagnostics,
                                size_t out_diagnostics_size) {
    MaterialPreviewResolvedVariant resolved[MATERIAL_PREVIEW_MAX_VARIANTS];
    int scene_index = -1;
    int variant_count = 0;
    int columns = 0;
    int rows = 0;
    int out_width = 0;
    int out_height = 0;
    Uint32* pixels = NULL;
    if (!request) return false;
    LoadAllSettings();
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.interactiveMode = false;
    if (!AnimationSelectSceneSource(SCENE_SOURCE_RUNTIME_SCENE,
                                    request->runtime_scene_path,
                                    true)) {
        material_preview_set_diag(out_diagnostics, out_diagnostics_size, "failed to load runtime scene");
        return false;
    }
    if (!material_preview_find_scene_index(request, &scene_index)) {
        material_preview_set_diag(out_diagnostics, out_diagnostics_size, "failed to resolve target object");
        return false;
    }
    variant_count = request->variant_count > 0 ? request->variant_count : 1;
    columns = request->columns > 0 ? request->columns : 5;
    if (columns > variant_count) columns = variant_count;
    rows = (variant_count + columns - 1) / columns;
    out_width = columns * request->cell_width;
    out_height = rows * request->cell_height;
    pixels = (Uint32*)calloc((size_t)out_width * (size_t)out_height, sizeof(Uint32));
    if (!pixels) {
        material_preview_set_diag(out_diagnostics, out_diagnostics_size, "failed to allocate preview buffer");
        return false;
    }
    for (int variant_index = 0; variant_index < variant_count; ++variant_index) {
        const MaterialPreviewVariantOverrides* request_variant =
            request->variant_count > 0 ? &request->variants[variant_index] : NULL;
        MaterialPreviewResolvedVariant* variant = &resolved[variant_index];
        int col = variant_index % columns;
        int row = variant_index / columns;
        int origin_x = col * request->cell_width;
        int origin_y = row * request->cell_height;
        int inset = request->cell_width < request->cell_height ? request->cell_width : request->cell_height;
        int pad = inset / 10;
        int swatch_x0 = origin_x + pad;
        int swatch_y0 = origin_y + pad;
        int swatch_x1 = origin_x + request->cell_width - pad;
        int swatch_y1 = origin_y + request->cell_height - pad;
        variant->object = sceneSettings.sceneObjects[scene_index];
        if (request_variant) {
            snprintf(variant->label,
                     sizeof(variant->label),
                     "%s",
                     request_variant->label[0] ? request_variant->label : "variant");
            material_preview_apply_variant(&variant->object, request_variant);
        } else {
            snprintf(variant->label, sizeof(variant->label), "base");
        }
        for (int y = origin_y; y < origin_y + request->cell_height; ++y) {
            for (int x = origin_x; x < origin_x + request->cell_width; ++x) {
                Uint8 bg_r = 0u, bg_g = 0u, bg_b = 0u;
                Uint8 out_r = 0u, out_g = 0u, out_b = 0u;
                RuntimeMaterialSurfaceEval eval = {0};
                material_preview_checker_color(x - origin_x,
                                               y - origin_y,
                                               request,
                                               &bg_r,
                                               &bg_g,
                                               &bg_b);
                if (x >= swatch_x0 && x < swatch_x1 && y >= swatch_y0 && y < swatch_y1) {
                    double u = (double)(x - swatch_x0) / (double)((swatch_x1 - swatch_x0) - 1);
                    double v = (double)(y - swatch_y0) / (double)((swatch_y1 - swatch_y0) - 1);
                    material_preview_eval_surface(&variant->object,
                                                  scene_index,
                                                  request_variant,
                                                  u,
                                                  v,
                                                  &eval);
                    material_preview_shade_pixel(&eval,
                                                 &variant->object,
                                                 u,
                                                 v,
                                                 bg_r,
                                                 bg_g,
                                                 bg_b,
                                                 &out_r,
                                                 &out_g,
                                                 &out_b);
                } else {
                    out_r = bg_r;
                    out_g = bg_g;
                    out_b = bg_b;
                }
                pixels[(size_t)y * (size_t)out_width + (size_t)x] =
                    0xFF000000u | ((Uint32)out_r << 16) | ((Uint32)out_g << 8) | (Uint32)out_b;
            }
        }
    }
    if (!material_preview_write_surface(request->output_path, out_width, out_height, pixels)) {
        free(pixels);
        material_preview_set_diag(out_diagnostics, out_diagnostics_size, "failed to write preview bmp");
        return false;
    }
    free(pixels);
    if (!material_preview_write_summary(request, scene_index, resolved, variant_count)) {
        material_preview_set_diag(out_diagnostics, out_diagnostics_size, "failed to write preview summary");
        return false;
    }
    material_preview_set_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}
