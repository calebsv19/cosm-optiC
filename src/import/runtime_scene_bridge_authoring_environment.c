#include "import/runtime_scene_bridge_authoring_environment.h"

#include "config/config_manager.h"
#include "render/runtime_scene_3d.h"

#include <math.h>

static bool runtime_scene_bridge_parse_color_rgb(json_object *owner,
                                                 const char *key,
                                                 double *out_r,
                                                 double *out_g,
                                                 double *out_b) {
    json_object *obj = NULL;
    if (!owner || !key || !out_r || !out_g || !out_b ||
        !json_object_object_get_ex(owner, key, &obj)) {
        return false;
    }
    if (json_object_is_type(obj, json_type_object)) {
        json_object *r_obj = NULL;
        json_object *g_obj = NULL;
        json_object *b_obj = NULL;
        if (!json_object_object_get_ex(obj, "r", &r_obj) ||
            !json_object_object_get_ex(obj, "g", &g_obj) ||
            !json_object_object_get_ex(obj, "b", &b_obj)) {
            return false;
        }
        *out_r = json_object_get_double(r_obj);
        *out_g = json_object_get_double(g_obj);
        *out_b = json_object_get_double(b_obj);
        return true;
    }
    if (json_object_is_type(obj, json_type_array) &&
        json_object_array_length(obj) >= 3u) {
        *out_r = json_object_get_double(json_object_array_get_idx(obj, 0));
        *out_g = json_object_get_double(json_object_array_get_idx(obj, 1));
        *out_b = json_object_get_double(json_object_array_get_idx(obj, 2));
        return true;
    }
    return false;
}

void runtime_scene_bridge_apply_ray_authoring_light_settings(json_object *authoring,
                                                             double world_scale) {
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

void runtime_scene_bridge_apply_ray_authoring_environment_settings(json_object *authoring) {
    json_object *environment = NULL;
    json_object *light_mode_obj = NULL;
    json_object *environment_preset_obj = NULL;
    json_object *ambient_brightness_obj = NULL;
    json_object *ambient_strength_obj = NULL;
    json_object *background_auto_obj = NULL;
    json_object *background_brightness_obj = NULL;
    json_object *top_fill_strength_obj = NULL;
    bool has_background_auto = false;
    double color_r = 1.0;
    double color_g = 1.0;
    double color_b = 1.0;
    if (!authoring) return;
    if (!json_object_object_get_ex(authoring, "environment", &environment) ||
        !json_object_is_type(environment, json_type_object)) {
        return;
    }
    animSettings.environmentPreset = ENVIRONMENT_PRESET_SKY;
    animSettings.environmentBackgroundBrightnessAuto = true;
    animSettings.environmentBackgroundBrightness = 0.0;
    animSettings.environmentBackgroundColorR = 1.0;
    animSettings.environmentBackgroundColorG = 1.0;
    animSettings.environmentBackgroundColorB = 1.0;
    if (json_object_object_get_ex(environment, "light_mode", &light_mode_obj) &&
        (json_object_is_type(light_mode_obj, json_type_int) ||
         json_object_is_type(light_mode_obj, json_type_double))) {
        animSettings.environmentLightMode =
            animation_config_environment_light_mode_clamp(json_object_get_int(light_mode_obj));
    }
    if (json_object_object_get_ex(environment, "ambient_strength", &ambient_strength_obj) &&
        (json_object_is_type(ambient_strength_obj, json_type_int) ||
         json_object_is_type(ambient_strength_obj, json_type_double))) {
        animSettings.environmentBrightness =
            255.0 * fmax(0.0, fmin(1.0, json_object_get_double(ambient_strength_obj)));
    } else if (json_object_object_get_ex(environment,
                                         "ambient_brightness",
                                         &ambient_brightness_obj) &&
        (json_object_is_type(ambient_brightness_obj, json_type_int) ||
         json_object_is_type(ambient_brightness_obj, json_type_double))) {
        animSettings.environmentBrightness =
            fmax(0.0, fmin(255.0, json_object_get_double(ambient_brightness_obj)));
    }
    if (json_object_object_get_ex(environment, "environment_preset", &environment_preset_obj)) {
        if (json_object_is_type(environment_preset_obj, json_type_string)) {
            animSettings.environmentPreset =
                RuntimeEnvironment3DPresetFromLabel(json_object_get_string(environment_preset_obj));
        } else if (json_object_is_type(environment_preset_obj, json_type_int) ||
                   json_object_is_type(environment_preset_obj, json_type_double)) {
            animSettings.environmentPreset =
                animation_config_environment_preset_clamp(json_object_get_int(environment_preset_obj));
        }
    }
    has_background_auto =
        json_object_object_get_ex(environment, "background_brightness_auto", &background_auto_obj) &&
        json_object_is_type(background_auto_obj, json_type_boolean);
    if (has_background_auto) {
        animSettings.environmentBackgroundBrightnessAuto =
            json_object_get_boolean(background_auto_obj) != 0;
    }
    if (json_object_object_get_ex(environment, "background_brightness", &background_brightness_obj) &&
        (json_object_is_type(background_brightness_obj, json_type_int) ||
         json_object_is_type(background_brightness_obj, json_type_double))) {
        if (!has_background_auto) {
            animSettings.environmentBackgroundBrightnessAuto = false;
        }
        if (!animSettings.environmentBackgroundBrightnessAuto) {
            animSettings.environmentBackgroundBrightness =
                fmax(0.0, fmin(4.0, json_object_get_double(background_brightness_obj)));
        }
    }
    if (runtime_scene_bridge_parse_color_rgb(environment,
                                            "background_color",
                                            &color_r,
                                            &color_g,
                                            &color_b)) {
        animSettings.environmentBackgroundColorR = fmax(0.0, fmin(1.0, color_r));
        animSettings.environmentBackgroundColorG = fmax(0.0, fmin(1.0, color_g));
        animSettings.environmentBackgroundColorB = fmax(0.0, fmin(1.0, color_b));
    }
    if (json_object_object_get_ex(environment, "top_fill_strength", &top_fill_strength_obj) &&
        (json_object_is_type(top_fill_strength_obj, json_type_int) ||
         json_object_is_type(top_fill_strength_obj, json_type_double))) {
        animSettings.topFillStrength =
            fmax(0.0, fmin(20.0, json_object_get_double(top_fill_strength_obj)));
    }
}
