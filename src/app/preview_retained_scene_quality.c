#include "app/preview_retained_scene_quality.h"

#include <math.h>
#include <string.h>

static double preview_retained_scene_clamp01(double value) {
    if (!isfinite(value) || value <= 0.0) return 0.0;
    if (value >= 1.0) return 1.0;
    return value;
}

static Uint8 preview_retained_scene_color_byte(double value) {
    if (!isfinite(value) || value <= 0.0) return 0;
    if (value >= 255.0) return 255;
    return (Uint8)lround(value);
}

PreviewRetainedSceneQuality PreviewRetainedSceneQualityNormalize(
    PreviewRetainedSceneQuality quality) {
    if (quality < PREVIEW_RETAINED_SCENE_QUALITY_WIREFRAME ||
        quality >= PREVIEW_RETAINED_SCENE_QUALITY_COUNT) {
        return PREVIEW_RETAINED_SCENE_QUALITY_WIREFRAME;
    }
    return quality;
}

PreviewRetainedSceneQuality PreviewRetainedSceneQualityCycle(
    PreviewRetainedSceneQuality quality) {
    quality = PreviewRetainedSceneQualityNormalize(quality);
    return (PreviewRetainedSceneQuality)(
        ((int)quality + 1) % (int)PREVIEW_RETAINED_SCENE_QUALITY_COUNT);
}

const char* PreviewRetainedSceneQualityLabel(
    PreviewRetainedSceneQuality quality) {
    switch (PreviewRetainedSceneQualityNormalize(quality)) {
        case PREVIEW_RETAINED_SCENE_QUALITY_SOLID:
            return "Solid";
        case PREVIEW_RETAINED_SCENE_QUALITY_INTERACTIVE_SHADED:
            return "Interactive shaded";
        case PREVIEW_RETAINED_SCENE_QUALITY_WIREFRAME:
        default:
            return "Wireframe";
    }
}

bool PreviewRetainedSceneQualityUsesSurface(
    PreviewRetainedSceneQuality quality) {
    quality = PreviewRetainedSceneQualityNormalize(quality);
    return quality == PREVIEW_RETAINED_SCENE_QUALITY_SOLID ||
           quality == PREVIEW_RETAINED_SCENE_QUALITY_INTERACTIVE_SHADED;
}

bool PreviewRetainedSceneFrameBuild(
    PreviewRetainedSceneQuality quality,
    const RayEvaluatedSceneSnapshot* evaluated_scene,
    PreviewRetainedSceneFrame* out_frame) {
    if (out_frame) memset(out_frame, 0, sizeof(*out_frame));
    if (!evaluated_scene || !out_frame ||
        RayEvaluatedSceneSnapshotValidate(evaluated_scene) != TIMELINE_STATUS_OK) {
        return false;
    }
    out_frame->quality = PreviewRetainedSceneQualityNormalize(quality);
    out_frame->evaluated_scene = *evaluated_scene;
    return true;
}

SDL_Color PreviewRetainedSceneShadeColor(
    SDL_Color base,
    double normal_x,
    double normal_y,
    double normal_z,
    double world_x,
    double world_y,
    double world_z,
    const PreviewRetainedSceneFrame* frame) {
    const RayEvaluatedLight* light = frame ? &frame->evaluated_scene.light : NULL;
    PreviewRetainedSceneQuality quality = frame
                                              ? PreviewRetainedSceneQualityNormalize(
                                                    frame->quality)
                                              : PREVIEW_RETAINED_SCENE_QUALITY_WIREFRAME;
    double normal_length = sqrt(normal_x * normal_x +
                                normal_y * normal_y +
                                normal_z * normal_z);
    double light_x = 0.0;
    double light_y = 0.0;
    double light_z = 1.0;
    double light_length = 1.0;
    double diffuse = 0.0;
    double energy = 0.0;
    double factor = 0.82;
    double color_r = 1.0;
    double color_g = 1.0;
    double color_b = 1.0;

    if (quality != PREVIEW_RETAINED_SCENE_QUALITY_INTERACTIVE_SHADED ||
        !light || !light->valid) {
        return (SDL_Color){
            preview_retained_scene_color_byte((double)base.r * factor),
            preview_retained_scene_color_byte((double)base.g * factor),
            preview_retained_scene_color_byte((double)base.b * factor),
            base.a};
    }
    if (normal_length > 1e-12) {
        normal_x /= normal_length;
        normal_y /= normal_length;
        normal_z /= normal_length;
    }
    light_x = light->position.x - world_x;
    light_y = light->position.y - world_y;
    light_z = light->position.z - world_z;
    light_length = sqrt(light_x * light_x + light_y * light_y + light_z * light_z);
    if (light_length > 1e-12) {
        light_x /= light_length;
        light_y /= light_length;
        light_z /= light_length;
    }
    diffuse = fmax(0.0, normal_x * light_x + normal_y * light_y + normal_z * light_z);
    energy = light->enabled
                 ? preview_retained_scene_clamp01(light->intensity /
                                                  (fabs(light->intensity) + 1.0))
                 : 0.0;
    factor = 0.22 + 0.78 * diffuse * energy;
    color_r = preview_retained_scene_clamp01(light->color.x);
    color_g = preview_retained_scene_clamp01(light->color.y);
    color_b = preview_retained_scene_clamp01(light->color.z);
    return (SDL_Color){
        preview_retained_scene_color_byte((double)base.r * factor * color_r),
        preview_retained_scene_color_byte((double)base.g * factor * color_g),
        preview_retained_scene_color_byte((double)base.b * factor * color_b),
        base.a};
}
