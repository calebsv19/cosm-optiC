#include "import/runtime_scene_bridge_internal.h"

#include "camera/camera_path_3d.h"
#include "config/config_manager.h"
#include "config/config_scene_path_io.h"
#include "editor/scene_editor_material_face_placement.h"
#include "editor/scene_editor_material_stack.h"
#include "import/runtime_scene_bridge_json_utils.h"
#include "material/material_manager.h"
#include "render/runtime_material_authored_texture_3d.h"
#include "render/runtime_material_texture_stack_3d.h"

#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static double runtime_scene_bridge_authoring_zero_length(void) {
    double zero [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    return zero;
}

static double runtime_scene_bridge_authoring_scale_scene_length(
    double scene_length [[fisics::dim(length)]] [[fisics::unit(meter)]],
    double world_scale) {
    double authored_length [[fisics::dim(length)]] [[fisics::unit(meter)]] = scene_length;
    return authored_length * world_scale;
}

static void scale_path_world_units(Path *path, double world_scale) {
    int i = 0;
    if (!path) return;
    for (i = 0; i < path->numPoints && i < MAX_BEZIER_POINTS; ++i) {
        double point_x [[fisics::dim(length)]] [[fisics::unit(meter)]] = path->points[i].x;
        double point_y [[fisics::dim(length)]] [[fisics::unit(meter)]] = path->points[i].y;
        path->points[i].x = runtime_scene_bridge_authoring_scale_scene_length(point_x, world_scale);
        path->points[i].y = runtime_scene_bridge_authoring_scale_scene_length(point_y, world_scale);
        if (i < MAX_BEZIER_POINTS - 1) {
            double h0x [[fisics::dim(length)]] [[fisics::unit(meter)]] = path->handles[i][0].vx;
            double h0y [[fisics::dim(length)]] [[fisics::unit(meter)]] = path->handles[i][0].vy;
            double h1x [[fisics::dim(length)]] [[fisics::unit(meter)]] = path->handles[i][1].vx;
            double h1y [[fisics::dim(length)]] [[fisics::unit(meter)]] = path->handles[i][1].vy;
            path->handles[i][0].vx =
                runtime_scene_bridge_authoring_scale_scene_length(h0x, world_scale);
            path->handles[i][0].vy =
                runtime_scene_bridge_authoring_scale_scene_length(h0y, world_scale);
            path->handles[i][1].vx =
                runtime_scene_bridge_authoring_scale_scene_length(h1x, world_scale);
            path->handles[i][1].vy =
                runtime_scene_bridge_authoring_scale_scene_length(h1y, world_scale);
        }
    }
}

static bool apply_authoring_path_scaled(json_object *authoring_obj,
                                        const char *key,
                                        Path *target_path,
                                        double world_scale,
                                        bool allow_empty) {
    json_object *path_obj = NULL;
    Path loaded = {0};
    if (!authoring_obj || !key || !target_path) return false;
    if (!json_object_object_get_ex(authoring_obj, key, &path_obj)) {
        return false;
    }
    if (!config_scene_load_path_from_json_object(path_obj, &loaded, allow_empty)) {
        return false;
    }
    scale_path_world_units(&loaded, world_scale);
    *target_path = loaded;
    return true;
}

static void apply_ray_authoring_light_settings(json_object *authoring, double world_scale) {
    json_object *light_settings = NULL;
    json_object *intensity_obj = NULL;
    json_object *radius_obj = NULL;
    if (!authoring) return;
    if (!json_object_object_get_ex(authoring, "light_settings", &light_settings) ||
        !json_object_is_type(light_settings, json_type_object)) {
        return;
    }
    if (json_object_object_get_ex(light_settings, "intensity", &intensity_obj) &&
        (json_object_is_type(intensity_obj, json_type_int) ||
         json_object_is_type(intensity_obj, json_type_double))) {
        animSettings.lightIntensity = json_object_get_double(intensity_obj);
    }
    (void)world_scale;
    if (json_object_object_get_ex(light_settings, "radius", &radius_obj) &&
        (json_object_is_type(radius_obj, json_type_int) ||
         json_object_is_type(radius_obj, json_type_double))) {
        animSettings.lightRadius = json_object_get_double(radius_obj);
        if (animSettings.lightRadius < 0.0) {
            animSettings.lightRadius = 0.0;
        }
    }
}

static int runtime_scene_bridge_color_from_material_preset(int material_id) {
    const Material *preset = MaterialManagerGet(material_id);
    int r = 255;
    int g = 255;
    int b = 255;
    if (!preset) return 0xFFFFFF;
    r = runtime_scene_bridge_clamp_color_channel(preset->base_color.x);
    g = runtime_scene_bridge_clamp_color_channel(preset->base_color.y);
    b = runtime_scene_bridge_clamp_color_channel(preset->base_color.z);
    return (r << 16) | (g << 8) | b;
}

static void runtime_scene_bridge_apply_object_material_preset(SceneObject *out_object,
                                                              int material_id) {
    if (!out_object) return;
    if (material_id < 0 || material_id >= MaterialManagerCount()) {
        material_id = MaterialManagerDefaultId();
    }
    out_object->material_id = material_id;
    out_object->color = runtime_scene_bridge_color_from_material_preset(material_id);
}

static bool runtime_scene_bridge_parse_double_field_any(json_object *obj,
                                                        const char *key_a,
                                                        const char *key_b,
                                                        double *out_value) {
    if (!obj || !out_value) return false;
    if (key_a && runtime_scene_bridge_parse_double_field(obj, key_a, out_value)) return true;
    if (key_b && runtime_scene_bridge_parse_double_field(obj, key_b, out_value)) return true;
    return false;
}

static bool runtime_scene_bridge_parse_position_or_transform_position(json_object *obj,
                                                                      double *out_x,
                                                                      double *out_y,
                                                                      double *out_z) {
    json_object *transform = NULL;
    if (!obj || !out_x || !out_y || !out_z) return false;
    if (runtime_scene_bridge_parse_vec3(obj, "position", out_x, out_y, out_z)) {
        return true;
    }
    if (json_object_object_get_ex(obj, "transform", &transform) &&
        json_object_is_type(transform, json_type_object) &&
        runtime_scene_bridge_parse_vec3(transform, "position", out_x, out_y, out_z)) {
        return true;
    }
    return false;
}

static bool runtime_scene_bridge_parse_transform_rotation(json_object *obj,
                                                          double *out_x,
                                                          double *out_y,
                                                          double *out_z) {
    json_object *transform = NULL;
    if (!obj || !out_x || !out_y || !out_z) return false;
    if (json_object_object_get_ex(obj, "transform", &transform) &&
        json_object_is_type(transform, json_type_object) &&
        runtime_scene_bridge_parse_vec3(transform, "rotation", out_x, out_y, out_z)) {
        return true;
    }
    return false;
}

static bool runtime_scene_bridge_parse_camera_seed_yaw(json_object *obj, double *out_yaw) {
    double rx = 0.0;
    double ry = 0.0;
    double rz = 0.0;
    if (!obj || !out_yaw) return false;
    if (runtime_scene_bridge_parse_double_field(obj, "yaw", out_yaw)) return true;
    if (runtime_scene_bridge_parse_double_field(obj, "rotation_z", out_yaw)) return true;
    if (runtime_scene_bridge_parse_double_field(obj, "rotationZ", out_yaw)) return true;
    if (runtime_scene_bridge_parse_transform_rotation(obj, &rx, &ry, &rz)) {
        *out_yaw = rz;
        return true;
    }
    return false;
}

static bool runtime_scene_bridge_parse_camera_seed_pitch(json_object *obj, double *out_pitch) {
    double rx = 0.0;
    double ry = 0.0;
    double rz = 0.0;
    if (!obj || !out_pitch) return false;
    if (runtime_scene_bridge_parse_double_field(obj, "look_pitch", out_pitch)) return true;
    if (runtime_scene_bridge_parse_double_field(obj, "lookPitch", out_pitch)) return true;
    if (runtime_scene_bridge_parse_double_field(obj, "pitch", out_pitch)) return true;
    if (runtime_scene_bridge_parse_double_field(obj, "rotation_x", out_pitch)) return true;
    if (runtime_scene_bridge_parse_double_field(obj, "rotationX", out_pitch)) return true;
    if (runtime_scene_bridge_parse_transform_rotation(obj, &rx, &ry, &rz)) {
        *out_pitch = rx;
        return true;
    }
    return false;
}

static bool runtime_scene_bridge_parse_focus_target(json_object *obj,
                                                    double world_scale,
                                                    double *out_x,
                                                    double *out_y,
                                                    double *out_z) {
    json_object *target = NULL;
    double x [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    double y [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    double z [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    if (!obj || !out_x || !out_y || !out_z) return false;
    if (!json_object_object_get_ex(obj, "camera_focus_target", &target) ||
        !json_object_is_type(target, json_type_object)) {
        return false;
    }
    if (!runtime_scene_bridge_parse_position_or_transform_position(target, &x, &y, &z) &&
        !runtime_scene_bridge_parse_double_field(target, "x", &x)) {
        return false;
    }
    if (!runtime_scene_bridge_parse_position_or_transform_position(target, &x, &y, &z)) {
        if (!runtime_scene_bridge_parse_double_field(target, "y", &y) ||
            !runtime_scene_bridge_parse_double_field(target, "z", &z)) {
            return false;
        }
    }
    *out_x = runtime_scene_bridge_authoring_scale_scene_length(x, world_scale);
    *out_y = runtime_scene_bridge_authoring_scale_scene_length(y, world_scale);
    *out_z = runtime_scene_bridge_authoring_scale_scene_length(z, world_scale);
    return true;
}

static void runtime_scene_bridge_parse_texture_parameters(json_object *owner,
                                                          RuntimeMaterialTexture3DParams *params) {
    json_object *parameters = NULL;
    json_object *field = NULL;
    if (!owner || !params) return;
    if (!json_object_object_get_ex(owner, "parameters", &parameters) ||
        !json_object_is_type(parameters, json_type_object)) {
        return;
    }
    if (json_object_object_get_ex(parameters, "pattern_mode", &field) &&
        (json_object_is_type(field, json_type_int) ||
         json_object_is_type(field, json_type_double))) {
        params->patternMode = json_object_get_int(field);
    }
    if (json_object_object_get_ex(parameters, "patternMode", &field) &&
        (json_object_is_type(field, json_type_int) ||
         json_object_is_type(field, json_type_double))) {
        params->patternMode = json_object_get_int(field);
    }
    runtime_scene_bridge_parse_double_field(parameters, "coverage", &params->coverage);
    runtime_scene_bridge_parse_double_field(parameters, "grain", &params->grain);
    runtime_scene_bridge_parse_double_field_any(parameters,
                                                "edge_softness",
                                                "edgeSoftness",
                                                &params->edgeSoftness);
    runtime_scene_bridge_parse_double_field(parameters, "contrast", &params->contrast);
    runtime_scene_bridge_parse_double_field(parameters, "flow", &params->flow);
    runtime_scene_bridge_parse_double_field_any(parameters,
                                                "color_depth",
                                                "colorDepth",
                                                &params->colorDepth);
    runtime_scene_bridge_parse_double_field_any(parameters,
                                                "surface_damage",
                                                "surfaceDamage",
                                                &params->surfaceDamage);
    if (json_object_object_get_ex(parameters, "seed", &field) &&
        (json_object_is_type(field, json_type_int) ||
         json_object_is_type(field, json_type_double))) {
        params->seed = json_object_get_int(field);
    }
    *params = RuntimeMaterialTexture3DNormalizeParams(*params);
}

static void runtime_scene_bridge_parse_texture_placement(
    json_object *owner,
    RuntimeMaterialTexture3DPlacement *placement) {
    json_object *placement_obj = NULL;
    if (!owner || !placement) return;
    if (json_object_object_get_ex(owner, "placement", &placement_obj) &&
        json_object_is_type(placement_obj, json_type_object)) {
        runtime_scene_bridge_parse_double_field_any(placement_obj,
                                                    "offset_u",
                                                    "offsetU",
                                                    &placement->offsetU);
        runtime_scene_bridge_parse_double_field_any(placement_obj,
                                                    "offset_v",
                                                    "offsetV",
                                                    &placement->offsetV);
        runtime_scene_bridge_parse_double_field(placement_obj, "scale", &placement->scale);
        runtime_scene_bridge_parse_double_field(placement_obj, "strength", &placement->strength);
        runtime_scene_bridge_parse_double_field(placement_obj, "rotation", &placement->rotation);
    }
    runtime_scene_bridge_parse_double_field_any(owner, "offset_u", "offsetU", &placement->offsetU);
    runtime_scene_bridge_parse_double_field_any(owner, "offset_v", "offsetV", &placement->offsetV);
    runtime_scene_bridge_parse_double_field(owner, "scale", &placement->scale);
    runtime_scene_bridge_parse_double_field(owner, "strength", &placement->strength);
    runtime_scene_bridge_parse_double_field(owner, "rotation", &placement->rotation);
}

static bool apply_ray_authoring_object_material_stack(json_object *entry,
                                                      int scene_index) {
    json_object *stack_obj = NULL;
    json_object *layers = NULL;
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    size_t count = 0u;

    if (!entry || scene_index < 0 || scene_index >= sceneSettings.objectCount) return false;
    if (!json_object_object_get_ex(entry, "material_texture_stack", &stack_obj)) {
        json_object_object_get_ex(entry, "materialTextureStack", &stack_obj);
    }
    if (!stack_obj ||
        !json_object_is_type(stack_obj, json_type_object) ||
        !json_object_object_get_ex(stack_obj, "layers", &layers) ||
        !json_object_is_type(layers, json_type_array)) {
        SceneEditorMaterialStackClearObjectStack(scene_index);
        return false;
    }

    count = json_object_array_length(layers);
    for (size_t i = 0u;
         i < count && stack.layerCount < RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS;
         ++i) {
        json_object *layer_obj = json_object_array_get_idx(layers, i);
        json_object *value = NULL;
        const char *kind_id = NULL;
        RuntimeMaterialTextureLayerKind kind = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE;
        RuntimeMaterialTextureLayer layer;

        if (!layer_obj || !json_object_is_type(layer_obj, json_type_object)) continue;
        if (json_object_object_get_ex(layer_obj, "kind", &value) &&
            json_object_is_type(value, json_type_string)) {
            kind_id = json_object_get_string(value);
        }
        kind = RuntimeMaterialTextureLayerKindFromStableId(kind_id);
        if (kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE) continue;

        layer = RuntimeMaterialTextureLayerKindIsBase(kind)
                    ? RuntimeMaterialTextureLayerMakeBase(kind)
                    : RuntimeMaterialTextureLayerMakeOverlay(kind);
        if (json_object_object_get_ex(layer_obj, "id", &value) &&
            json_object_is_type(value, json_type_string)) {
            snprintf(layer.layerId, sizeof(layer.layerId), "%s", json_object_get_string(value));
        }
        if (json_object_object_get_ex(layer_obj, "name", &value) &&
            json_object_is_type(value, json_type_string)) {
            snprintf(layer.displayName,
                     sizeof(layer.displayName),
                     "%s",
                     json_object_get_string(value));
        }
        if (json_object_object_get_ex(layer_obj, "blend", &value) &&
            json_object_is_type(value, json_type_string)) {
            layer.blendMode =
                RuntimeMaterialTextureLayerBlendModeFromStableId(json_object_get_string(value));
        }
        if (json_object_object_get_ex(layer_obj, "enabled", &value) &&
            json_object_is_type(value, json_type_boolean)) {
            layer.enabled = json_object_get_boolean(value) != 0;
        }
        runtime_scene_bridge_parse_double_field(layer_obj, "opacity", &layer.opacity);
        runtime_scene_bridge_parse_texture_placement(layer_obj, &layer.placement);
        runtime_scene_bridge_parse_texture_parameters(layer_obj, &layer.params);
        layer.placement.params = layer.params;
        runtime_scene_bridge_parse_double_field_any(layer_obj,
                                                    "roughness_influence",
                                                    "roughnessInfluence",
                                                    &layer.roughnessInfluence);
        runtime_scene_bridge_parse_double_field_any(layer_obj,
                                                    "reflectivity_influence",
                                                    "reflectivityInfluence",
                                                    &layer.reflectivityInfluence);
        runtime_scene_bridge_parse_double_field_any(layer_obj,
                                                    "specular_influence",
                                                    "specularInfluence",
                                                    &layer.specularInfluence);
        runtime_scene_bridge_parse_double_field_any(layer_obj,
                                                    "diffuse_influence",
                                                    "diffuseInfluence",
                                                    &layer.diffuseInfluence);
        runtime_scene_bridge_parse_double_field_any(layer_obj,
                                                    "transparency_influence",
                                                    "transparencyInfluence",
                                                    &layer.transparencyInfluence);
        stack.layers[stack.layerCount++] = RuntimeMaterialTextureLayerNormalize(layer);
    }

    if (stack.layerCount <= 0) {
        SceneEditorMaterialStackClearObjectStack(scene_index);
        return false;
    }
    return SceneEditorMaterialStackSetObjectStack(scene_index, &stack);
}

static void apply_ray_authoring_object_procedural_texture(json_object *entry,
                                                          int scene_index) {
    json_object *procedural_texture = NULL;
    json_object *texture_id_obj = NULL;
    json_object *face_placements = NULL;
    SceneObject *object = NULL;
    bool has_flat_override = false;

    if (!entry || scene_index < 0 || scene_index >= sceneSettings.objectCount) return;
    if (json_object_object_get_ex(entry, "texture_id", &texture_id_obj) &&
        (json_object_is_type(texture_id_obj, json_type_int) ||
         json_object_is_type(texture_id_obj, json_type_double))) {
        has_flat_override = true;
    } else if (json_object_object_get_ex(entry, "textureId", &texture_id_obj) &&
               (json_object_is_type(texture_id_obj, json_type_int) ||
                json_object_is_type(texture_id_obj, json_type_double))) {
        has_flat_override = true;
    }
    has_flat_override =
        has_flat_override ||
        runtime_scene_bridge_parse_double_field_any(entry,
                                                    "texture_strength",
                                                    "textureStrength",
                                                    &(double){0.0}) ||
        runtime_scene_bridge_parse_double_field_any(entry,
                                                    "texture_scale",
                                                    "textureScale",
                                                    &(double){0.0}) ||
        runtime_scene_bridge_parse_double_field_any(entry,
                                                    "texture_offset_u",
                                                    "textureOffsetU",
                                                    &(double){0.0}) ||
        runtime_scene_bridge_parse_double_field_any(entry,
                                                    "texture_offset_v",
                                                    "textureOffsetV",
                                                    &(double){0.0}) ||
        runtime_scene_bridge_parse_double_field_any(entry,
                                                    "texture_coverage",
                                                    "textureCoverage",
                                                    &(double){0.0}) ||
        runtime_scene_bridge_parse_double_field_any(entry,
                                                    "texture_grain",
                                                    "textureGrain",
                                                    &(double){0.0}) ||
        runtime_scene_bridge_parse_double_field_any(entry,
                                                    "texture_edge_softness",
                                                    "textureEdgeSoftness",
                                                    &(double){0.0}) ||
        runtime_scene_bridge_parse_double_field_any(entry,
                                                    "texture_contrast",
                                                    "textureContrast",
                                                    &(double){0.0}) ||
        runtime_scene_bridge_parse_double_field_any(entry,
                                                    "texture_flow",
                                                    "textureFlow",
                                                    &(double){0.0}) ||
        runtime_scene_bridge_parse_double_field_any(entry,
                                                    "texture_color_depth",
                                                    "textureColorDepth",
                                                    &(double){0.0}) ||
        runtime_scene_bridge_parse_double_field_any(entry,
                                                    "texture_surface_damage",
                                                    "textureSurfaceDamage",
                                                    &(double){0.0}) ||
        json_object_object_get_ex(entry, "texture_pattern_mode", &texture_id_obj) ||
        json_object_object_get_ex(entry, "texturePatternMode", &texture_id_obj) ||
        json_object_object_get_ex(entry, "texture_seed", &texture_id_obj) ||
        json_object_object_get_ex(entry, "textureSeed", &texture_id_obj);

    if ((!json_object_object_get_ex(entry, "procedural_texture", &procedural_texture) ||
         !json_object_is_type(procedural_texture, json_type_object)) &&
        !has_flat_override) {
        return;
    }

    object = &sceneSettings.sceneObjects[scene_index];
    if (has_flat_override) {
        if (json_object_object_get_ex(entry, "texture_id", &texture_id_obj) &&
            (json_object_is_type(texture_id_obj, json_type_int) ||
             json_object_is_type(texture_id_obj, json_type_double))) {
            object->textureId = json_object_get_int(texture_id_obj);
        } else if (json_object_object_get_ex(entry, "textureId", &texture_id_obj) &&
                   (json_object_is_type(texture_id_obj, json_type_int) ||
                    json_object_is_type(texture_id_obj, json_type_double))) {
            object->textureId = json_object_get_int(texture_id_obj);
        }
        runtime_scene_bridge_parse_double_field_any(entry,
                                                    "texture_offset_u",
                                                    "textureOffsetU",
                                                    &object->textureOffsetU);
        runtime_scene_bridge_parse_double_field_any(entry,
                                                    "texture_offset_v",
                                                    "textureOffsetV",
                                                    &object->textureOffsetV);
        runtime_scene_bridge_parse_double_field_any(entry,
                                                    "texture_scale",
                                                    "textureScale",
                                                    &object->textureScale);
        runtime_scene_bridge_parse_double_field_any(entry,
                                                    "texture_strength",
                                                    "textureStrength",
                                                    &object->textureStrength);
        {
            RuntimeMaterialTexture3DParams params = RuntimeMaterialTexture3DParamsFromObject(object);
            json_object *seed_obj = NULL;
            json_object *pattern_mode_obj = NULL;
            if (json_object_object_get_ex(entry, "texture_pattern_mode", &pattern_mode_obj) &&
                (json_object_is_type(pattern_mode_obj, json_type_int) ||
                 json_object_is_type(pattern_mode_obj, json_type_double))) {
                params.patternMode = json_object_get_int(pattern_mode_obj);
            } else if (json_object_object_get_ex(entry, "texturePatternMode", &pattern_mode_obj) &&
                       (json_object_is_type(pattern_mode_obj, json_type_int) ||
                        json_object_is_type(pattern_mode_obj, json_type_double))) {
                params.patternMode = json_object_get_int(pattern_mode_obj);
            }
            runtime_scene_bridge_parse_double_field_any(entry,
                                                        "texture_coverage",
                                                        "textureCoverage",
                                                        &params.coverage);
            runtime_scene_bridge_parse_double_field_any(entry,
                                                        "texture_grain",
                                                        "textureGrain",
                                                        &params.grain);
            runtime_scene_bridge_parse_double_field_any(entry,
                                                        "texture_edge_softness",
                                                        "textureEdgeSoftness",
                                                        &params.edgeSoftness);
            runtime_scene_bridge_parse_double_field_any(entry,
                                                        "texture_contrast",
                                                        "textureContrast",
                                                        &params.contrast);
            runtime_scene_bridge_parse_double_field_any(entry,
                                                        "texture_flow",
                                                        "textureFlow",
                                                        &params.flow);
            runtime_scene_bridge_parse_double_field_any(entry,
                                                        "texture_color_depth",
                                                        "textureColorDepth",
                                                        &params.colorDepth);
            runtime_scene_bridge_parse_double_field_any(entry,
                                                        "texture_surface_damage",
                                                        "textureSurfaceDamage",
                                                        &params.surfaceDamage);
            if (json_object_object_get_ex(entry, "texture_seed", &seed_obj) &&
                (json_object_is_type(seed_obj, json_type_int) ||
                 json_object_is_type(seed_obj, json_type_double))) {
                params.seed = json_object_get_int(seed_obj);
            } else if (json_object_object_get_ex(entry, "textureSeed", &seed_obj) &&
                       (json_object_is_type(seed_obj, json_type_int) ||
                        json_object_is_type(seed_obj, json_type_double))) {
                params.seed = json_object_get_int(seed_obj);
            }
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
        }
    }

    if (procedural_texture && json_object_is_type(procedural_texture, json_type_object)) {
        if (json_object_object_get_ex(procedural_texture, "texture_id", &texture_id_obj) &&
            (json_object_is_type(texture_id_obj, json_type_int) ||
             json_object_is_type(texture_id_obj, json_type_double))) {
            object->textureId = json_object_get_int(texture_id_obj);
        }
        runtime_scene_bridge_parse_double_field(procedural_texture,
                                                "offset_u",
                                                &object->textureOffsetU);
        runtime_scene_bridge_parse_double_field(procedural_texture,
                                                "offset_v",
                                                &object->textureOffsetV);
        runtime_scene_bridge_parse_double_field(procedural_texture,
                                                "scale",
                                                &object->textureScale);
        runtime_scene_bridge_parse_double_field(procedural_texture,
                                                "strength",
                                                &object->textureStrength);
        {
            RuntimeMaterialTexture3DParams params = RuntimeMaterialTexture3DParamsFromObject(object);
            runtime_scene_bridge_parse_texture_parameters(procedural_texture, &params);
            object->texturePatternMode = params.patternMode;
            object->textureCoverage = params.coverage;
            object->textureGrain = params.grain;
            object->textureEdgeSoftness = params.edgeSoftness;
            object->textureContrast = params.contrast;
            object->textureFlow = params.flow;
            object->textureColorDepth = params.colorDepth;
            object->textureSurfaceDamage = params.surfaceDamage;
            object->textureSeed = params.seed;
        }
    }
    if (!(object->textureScale > 1e-6)) object->textureScale = 1.0;
    if (object->textureStrength < 0.0) object->textureStrength = 0.0;
    if (object->textureStrength > 1.0) object->textureStrength = 1.0;

    SceneEditorMaterialFacePlacementResetObject(scene_index);
    if (!procedural_texture ||
        !json_object_is_type(procedural_texture, json_type_object) ||
        !json_object_object_get_ex(procedural_texture, "face_placements", &face_placements) ||
        !json_object_is_type(face_placements, json_type_array)) {
        return;
    }
    for (size_t i = 0u; i < json_object_array_length(face_placements); ++i) {
        json_object *face_entry = json_object_array_get_idx(face_placements, i);
        json_object *face_group_index = NULL;
        json_object *face_texture_id = NULL;
        SceneEditorMaterialFacePlacement placement;
        if (!face_entry || !json_object_is_type(face_entry, json_type_object)) continue;
        if (!json_object_object_get_ex(face_entry, "face_group_index", &face_group_index) ||
            (!json_object_is_type(face_group_index, json_type_int) &&
             !json_object_is_type(face_group_index, json_type_double))) {
            continue;
        }
        memset(&placement, 0, sizeof(placement));
        placement.hasOverride = true;
        placement.sceneObjectIndex = scene_index;
        placement.faceGroupIndex = json_object_get_int(face_group_index);
        placement.textureId = object->textureId;
        placement.scale = object->textureScale;
        placement.strength = object->textureStrength;
        placement.offsetU = object->textureOffsetU;
        placement.offsetV = object->textureOffsetV;
        placement.params = RuntimeMaterialTexture3DParamsFromObject(object);
        if (json_object_object_get_ex(face_entry, "texture_id", &face_texture_id) &&
            (json_object_is_type(face_texture_id, json_type_int) ||
             json_object_is_type(face_texture_id, json_type_double))) {
            placement.textureId = json_object_get_int(face_texture_id);
        }
        runtime_scene_bridge_parse_double_field(face_entry, "offset_u", &placement.offsetU);
        runtime_scene_bridge_parse_double_field(face_entry, "offset_v", &placement.offsetV);
        runtime_scene_bridge_parse_double_field(face_entry, "scale", &placement.scale);
        runtime_scene_bridge_parse_double_field(face_entry, "strength", &placement.strength);
        runtime_scene_bridge_parse_double_field(face_entry, "rotation", &placement.rotation);
        runtime_scene_bridge_parse_texture_parameters(face_entry, &placement.params);
        SceneEditorMaterialFacePlacementSetOverride(&placement);
    }
}

static void apply_ray_authoring_object_authored_texture(json_object* entry,
                                                        int scene_index,
                                                        const char* object_id) {
    json_object* authored_texture = NULL;
    json_object* manifest_path_obj = NULL;
    json_object* binding_mode_obj = NULL;
    const char* manifest_path = NULL;
    const char* binding_mode = NULL;
    if (!entry || !object_id || !object_id[0] || scene_index < 0 ||
        scene_index >= sceneSettings.objectCount) {
        return;
    }
    if (!json_object_object_get_ex(entry, "authored_texture", &authored_texture) ||
        !json_object_is_type(authored_texture, json_type_object) ||
        !json_object_object_get_ex(authored_texture, "manifest_path", &manifest_path_obj) ||
        !json_object_is_type(manifest_path_obj, json_type_string)) {
        return;
    }
    manifest_path = json_object_get_string(manifest_path_obj);
    if (json_object_object_get_ex(authored_texture, "binding_mode", &binding_mode_obj) &&
        json_object_is_type(binding_mode_obj, json_type_string)) {
        binding_mode = json_object_get_string(binding_mode_obj);
    }
    (void)RuntimeMaterialAuthoredTextureBindManifestForObject(scene_index,
                                                              object_id,
                                                              manifest_path,
                                                              binding_mode);
}

static void apply_ray_authoring_object_materials(json_object *authoring) {
    json_object *object_materials = NULL;
    size_t i = 0;
    if (!authoring) return;
    if (!json_object_object_get_ex(authoring, "object_materials", &object_materials) ||
        !json_object_is_type(object_materials, json_type_array)) {
        return;
    }
    for (i = 0; i < json_object_array_length(object_materials); ++i) {
        json_object *entry = json_object_array_get_idx(object_materials, i);
        json_object *object_id_obj = NULL;
        json_object *material_id_obj = NULL;
        json_object *object_color_obj = NULL;
        json_object *alpha_obj = NULL;
        json_object *transparency_obj = NULL;
        json_object *reflectivity_obj = NULL;
        json_object *roughness_obj = NULL;
        json_object *emissive_strength_obj = NULL;
        const char *object_id = NULL;
        int material_id = MaterialManagerDefaultId();
        int object_color = 0xFFFFFF;
        bool has_object_color = false;
        double alpha = 1.0;
        double reflectivity = 0.35;
        double roughness = 0.65;
        double emissive_strength = 0.0;
        int scene_index = 0;
        if (!entry || !json_object_is_type(entry, json_type_object)) continue;
        if (!json_object_object_get_ex(entry, "object_id", &object_id_obj) ||
            !json_object_is_type(object_id_obj, json_type_string)) {
            continue;
        }
        if (!json_object_object_get_ex(entry, "material_id", &material_id_obj) ||
            (!json_object_is_type(material_id_obj, json_type_int) &&
             !json_object_is_type(material_id_obj, json_type_double))) {
            continue;
        }
        object_id = json_object_get_string(object_id_obj);
        material_id = json_object_get_int(material_id_obj);
        if (json_object_object_get_ex(entry, "object_color", &object_color_obj) &&
            (json_object_is_type(object_color_obj, json_type_int) ||
             json_object_is_type(object_color_obj, json_type_double))) {
            object_color = json_object_get_int(object_color_obj) & 0xFFFFFF;
            has_object_color = true;
        }
        if (json_object_object_get_ex(entry, "alpha", &alpha_obj) &&
            (json_object_is_type(alpha_obj, json_type_int) ||
             json_object_is_type(alpha_obj, json_type_double))) {
            alpha = json_object_get_double(alpha_obj);
        } else if (json_object_object_get_ex(entry, "transparency", &transparency_obj) &&
                   (json_object_is_type(transparency_obj, json_type_int) ||
                    json_object_is_type(transparency_obj, json_type_double))) {
            alpha = json_object_get_double(transparency_obj);
        }
        if (json_object_object_get_ex(entry, "reflectivity", &reflectivity_obj) &&
            (json_object_is_type(reflectivity_obj, json_type_int) ||
             json_object_is_type(reflectivity_obj, json_type_double))) {
            reflectivity = json_object_get_double(reflectivity_obj);
        }
        if (json_object_object_get_ex(entry, "roughness", &roughness_obj) &&
            (json_object_is_type(roughness_obj, json_type_int) ||
             json_object_is_type(roughness_obj, json_type_double))) {
            roughness = json_object_get_double(roughness_obj);
        }
        if (json_object_object_get_ex(entry, "emissive_strength", &emissive_strength_obj) &&
            (json_object_is_type(emissive_strength_obj, json_type_int) ||
             json_object_is_type(emissive_strength_obj, json_type_double))) {
            emissive_strength = json_object_get_double(emissive_strength_obj);
        }
        if (!object_id || !object_id[0]) continue;
        for (scene_index = 0;
             scene_index < sceneSettings.objectCount && scene_index < g_last_runtime_object_id_count;
             ++scene_index) {
            if (strcmp(g_last_runtime_object_ids[scene_index], object_id) == 0) {
                bool preserve_helper_tint =
                    SceneObjectIsGuideOnly(&sceneSettings.sceneObjects[scene_index]);
                int preserved_color = sceneSettings.sceneObjects[scene_index].color;
                runtime_scene_bridge_apply_object_material_preset(
                    &sceneSettings.sceneObjects[scene_index], material_id);
                if (preserve_helper_tint) {
                    /* Physics-sim helper emitters keep their program-assigned tint. */
                    sceneSettings.sceneObjects[scene_index].color = preserved_color;
                } else if (has_object_color) {
                    sceneSettings.sceneObjects[scene_index].color = object_color;
                }
                sceneSettings.sceneObjects[scene_index].alpha = fmax(0.0, fmin(1.0, alpha));
                sceneSettings.sceneObjects[scene_index].reflectivity =
                    fmax(0.0, fmin(1.0, reflectivity));
                sceneSettings.sceneObjects[scene_index].roughness =
                    fmax(0.0, fmin(1.0, roughness));
                sceneSettings.sceneObjects[scene_index].emissiveStrength =
                    fmax(0.0, fmin(1.0, emissive_strength));
                apply_ray_authoring_object_authored_texture(entry, scene_index, object_id);
                apply_ray_authoring_object_procedural_texture(entry, scene_index);
                apply_ray_authoring_object_material_stack(entry, scene_index);
                break;
            }
        }
    }
}

void runtime_scene_bridge_apply_scene3d_extension_digest(json_object *root,
                                                         double world_scale) {
    json_object *extensions = NULL;
    json_object *line_drawing = NULL;
    json_object *scene3d = NULL;
    json_object *bounds = NULL;
    json_object *construction_plane = NULL;

    if (!root || !json_object_is_type(root, json_type_object)) return;
    if (!json_object_object_get_ex(root, "extensions", &extensions) ||
        !json_object_is_type(extensions, json_type_object)) {
        return;
    }
    if (!json_object_object_get_ex(extensions, "line_drawing", &line_drawing) ||
        !json_object_is_type(line_drawing, json_type_object)) {
        return;
    }
    if (!json_object_object_get_ex(line_drawing, "scene3d", &scene3d) ||
        !json_object_is_type(scene3d, json_type_object)) {
        return;
    }

    if (json_object_object_get_ex(scene3d, "bounds", &bounds) &&
        json_object_is_type(bounds, json_type_object)) {
        double min_x [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
        double min_y [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
        double min_z [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
        double max_x [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
        double max_y [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
        double max_z [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
        bool has_enabled = false;
        bool has_clamp = false;
        bool enabled = false;
        bool clamp_on_edit = false;
        g_last_3d_digest.has_scene_bounds = true;
        has_enabled = runtime_scene_bridge_parse_bool_field(bounds, "enabled", &enabled);
        has_clamp = runtime_scene_bridge_parse_bool_field(bounds, "clamp_on_edit", &clamp_on_edit);
        g_last_3d_digest.bounds_enabled = has_enabled && enabled;
        g_last_3d_digest.bounds_clamp_on_edit = has_clamp && clamp_on_edit;
        if (runtime_scene_bridge_parse_vec3(bounds, "min", &min_x, &min_y, &min_z)) {
            g_last_3d_digest.bounds_min_x =
                runtime_scene_bridge_authoring_scale_scene_length(min_x, world_scale);
            g_last_3d_digest.bounds_min_y =
                runtime_scene_bridge_authoring_scale_scene_length(min_y, world_scale);
            g_last_3d_digest.bounds_min_z =
                runtime_scene_bridge_authoring_scale_scene_length(min_z, world_scale);
        }
        if (runtime_scene_bridge_parse_vec3(bounds, "max", &max_x, &max_y, &max_z)) {
            g_last_3d_digest.bounds_max_x =
                runtime_scene_bridge_authoring_scale_scene_length(max_x, world_scale);
            g_last_3d_digest.bounds_max_y =
                runtime_scene_bridge_authoring_scale_scene_length(max_y, world_scale);
            g_last_3d_digest.bounds_max_z =
                runtime_scene_bridge_authoring_scale_scene_length(max_z, world_scale);
        }
    }

    if (json_object_object_get_ex(scene3d, "construction_plane", &construction_plane) &&
        json_object_is_type(construction_plane, json_type_object)) {
        const char *mode = runtime_scene_bridge_json_string_field_or_null(construction_plane, "mode");
        const char *axis = runtime_scene_bridge_json_string_field_or_null(construction_plane, "axis");
        double offset = 0.0;
        g_last_3d_digest.has_construction_plane = true;
        if (mode && mode[0]) {
            snprintf(g_last_3d_digest.construction_plane_mode,
                     sizeof(g_last_3d_digest.construction_plane_mode),
                     "%s",
                     mode);
        }
        if (axis && axis[0]) {
            snprintf(g_last_3d_digest.construction_plane_axis,
                     sizeof(g_last_3d_digest.construction_plane_axis),
                     "%s",
                     axis);
        }
        if (runtime_scene_bridge_parse_double_field(construction_plane, "offset", &offset)) {
            double scene_offset [[fisics::dim(length)]] [[fisics::unit(meter)]] = offset;
            g_last_3d_digest.construction_plane_offset =
                runtime_scene_bridge_authoring_scale_scene_length(scene_offset, world_scale);
        }
    }
}

void runtime_scene_bridge_apply_space_mode(json_object *root) {
    json_object *space_mode = NULL;
    const char *mode = NULL;
    if (!root) return;
    if (!json_object_object_get_ex(root, "space_mode_default", &space_mode) ||
        !json_object_is_type(space_mode, json_type_string)) {
        animSettings.spaceMode = SPACE_MODE_2D;
        return;
    }
    mode = json_object_get_string(space_mode);
    animSettings.spaceMode = (mode && strcmp(mode, "3d") == 0) ? SPACE_MODE_3D : SPACE_MODE_2D;
}

void runtime_scene_bridge_apply_light_seed_scaled(json_object *lights_array,
                                                  double world_scale) {
    json_object *light0 = NULL;
    double lx [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    double ly [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    double lz [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    double zero_length = runtime_scene_bridge_authoring_zero_length();
    if (!lights_array || !json_object_is_type(lights_array, json_type_array) ||
        json_object_array_length(lights_array) == 0u) {
        return;
    }
    light0 = json_object_array_get_idx(lights_array, 0);
    if (!light0 || !json_object_is_type(light0, json_type_object)) return;
    if (runtime_scene_bridge_parse_position_or_transform_position(light0, &lx, &ly, &lz)) {
        sceneSettings.bezierPath.numPoints = 1;
        sceneSettings.bezierPath.points[0].x =
            runtime_scene_bridge_authoring_scale_scene_length(lx, world_scale);
        sceneSettings.bezierPath.points[0].y =
            runtime_scene_bridge_authoring_scale_scene_length(ly, world_scale);
        sceneSettings.bezierPath3D.point_z[0] =
            runtime_scene_bridge_authoring_scale_scene_length(lz, world_scale);
        if (sceneSettings.bezierPath3D.point_z[0] > zero_length) {
            animSettings.lightHeight = sceneSettings.bezierPath3D.point_z[0];
        }
    }
}

void runtime_scene_bridge_apply_camera_seed_scaled(json_object *cameras_array,
                                                   double world_scale) {
    json_object *camera0 = NULL;
    double cx [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    double cy [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    double cz [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    double yaw = 0.0;
    double pitch = 0.0;
    bool has_yaw = false;
    bool has_pitch = false;
    if (!cameras_array || !json_object_is_type(cameras_array, json_type_array) ||
        json_object_array_length(cameras_array) == 0u) {
        return;
    }
    camera0 = json_object_array_get_idx(cameras_array, 0);
    if (!camera0 || !json_object_is_type(camera0, json_type_object)) return;
    if (runtime_scene_bridge_parse_position_or_transform_position(camera0, &cx, &cy, &cz)) {
        sceneSettings.camera.x =
            runtime_scene_bridge_authoring_scale_scene_length(cx, world_scale);
        sceneSettings.camera.y =
            runtime_scene_bridge_authoring_scale_scene_length(cy, world_scale);
        sceneSettings.cameraZ =
            runtime_scene_bridge_authoring_scale_scene_length(cz, world_scale);
        sceneSettings.cameraPath.numPoints = 1;
        sceneSettings.cameraPath.points[0].x = sceneSettings.camera.x;
        sceneSettings.cameraPath.points[0].y = sceneSettings.camera.y;
        sceneSettings.cameraPath.rotationSet[0] = false;
        sceneSettings.cameraPath3D.point_z[0] = sceneSettings.cameraZ;
        sceneSettings.cameraPath3D.point_pitch[0] = 0.0;
        g_last_3d_scaffold.has_camera_seed = true;
        g_last_3d_scaffold.camera_z = sceneSettings.cameraZ;
    }
    has_yaw = runtime_scene_bridge_parse_camera_seed_yaw(camera0, &yaw);
    has_pitch = runtime_scene_bridge_parse_camera_seed_pitch(camera0, &pitch);
    if (has_yaw) {
        sceneSettings.camera.rotation = yaw;
        sceneSettings.cameraPath.rotations[0] = yaw;
        sceneSettings.cameraPath.rotationSet[0] = true;
        g_last_3d_scaffold.has_camera_rotation_seed = true;
    }
    if (has_pitch) {
        sceneSettings.cameraPath3D.point_pitch[0] = pitch;
        g_last_3d_scaffold.has_camera_pitch_seed = true;
    }
}

void runtime_scene_bridge_apply_ray_authoring_paths(json_object *root,
                                                    double world_scale) {
    json_object *extensions = NULL;
    json_object *ray_tracing = NULL;
    json_object *authoring = NULL;
    json_object *light_path_depth = NULL;
    json_object *camera_path_depth = NULL;
    double focus_x = 0.0;
    double focus_y = 0.0;
    double focus_z = 0.0;
    if (!root) return;
    if (!json_object_object_get_ex(root, "extensions", &extensions) ||
        !json_object_is_type(extensions, json_type_object) ||
        !json_object_object_get_ex(extensions, "ray_tracing", &ray_tracing) ||
        !json_object_is_type(ray_tracing, json_type_object) ||
        !json_object_object_get_ex(ray_tracing, "authoring", &authoring) ||
        !json_object_is_type(authoring, json_type_object)) {
        return;
    }

    if (runtime_scene_bridge_parse_focus_target(authoring,
                                                world_scale,
                                                &focus_x,
                                                &focus_y,
                                                &focus_z)) {
        g_last_3d_scaffold.has_camera_focus_target = true;
        g_last_3d_scaffold.camera_focus_target_x = focus_x;
        g_last_3d_scaffold.camera_focus_target_y = focus_y;
        g_last_3d_scaffold.camera_focus_target_z = focus_z;
    }

    if (apply_authoring_path_scaled(authoring,
                                    "light_path",
                                    &sceneSettings.bezierPath,
                                    world_scale,
                                    true)) {
        if (json_object_object_get_ex(authoring, "light_path_depth", &light_path_depth) &&
            CameraPath3D_LoadFromJsonObject(light_path_depth,
                                            &sceneSettings.bezierPath3D,
                                            &sceneSettings.bezierPath,
                                            true)) {
            CameraPath3D_ScaleWorldUnits(&sceneSettings.bezierPath3D,
                                         &sceneSettings.bezierPath,
                                         world_scale);
        } else {
            CameraPath3D_Reset(&sceneSettings.bezierPath3D);
            CameraPath3D_SyncDefaults(&sceneSettings.bezierPath3D,
                                      &sceneSettings.bezierPath,
                                      animSettings.lightHeight);
        }
        if (sceneSettings.bezierPath.numPoints > 0) {
            animSettings.lightHeight = sceneSettings.bezierPath3D.point_z[0];
        }
    }
    if (apply_authoring_path_scaled(authoring,
                                    "camera_path",
                                    &sceneSettings.cameraPath,
                                    world_scale,
                                    true)) {
        if (json_object_object_get_ex(authoring, "camera_path_depth", &camera_path_depth) &&
            CameraPath3D_LoadFromJsonObject(camera_path_depth,
                                            &sceneSettings.cameraPath3D,
                                            &sceneSettings.cameraPath,
                                            true)) {
            CameraPath3D_ScaleWorldUnits(&sceneSettings.cameraPath3D,
                                         &sceneSettings.cameraPath,
                                         world_scale);
        } else {
            CameraPath3D_Reset(&sceneSettings.cameraPath3D);
            CameraPath3D_SyncDefaults(&sceneSettings.cameraPath3D,
                                      &sceneSettings.cameraPath,
                                      sceneSettings.cameraZ);
        }
        if (sceneSettings.cameraPath.numPoints > 0) {
            sceneSettings.camera.x = sceneSettings.cameraPath.points[0].x;
            sceneSettings.camera.y = sceneSettings.cameraPath.points[0].y;
            sceneSettings.cameraZ = sceneSettings.cameraPath3D.point_z[0];
            if (sceneSettings.cameraPath.rotationSet[0]) {
                sceneSettings.camera.rotation = sceneSettings.cameraPath.rotations[0];
            }
        }
    }
    apply_ray_authoring_light_settings(authoring, world_scale);
    apply_ray_authoring_object_materials(authoring);
}
