#include "render/runtime_native_3d_render_internal.h"

#include <math.h>
#include <stddef.h>

#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_volume_3d_integrate.h"
#include "render/runtime_volume_3d_scatter.h"

bool runtime_native_3d_render_trace_visible_emitter(
    const RuntimeScene3D* scene,
    const RuntimeCameraProjector3D* projector,
    double pixel_x,
    double pixel_y,
    RuntimeLightEmitterHit3DResult* out_emitter_hit) {
    RuntimeLightEmitterTrace3DResult trace = {0};
    Ray3D primary_ray = {0};

    if (!scene || !projector || !out_emitter_hit) return false;

    primary_ray = RuntimeCameraProjector3D_MakePrimaryRay(projector, pixel_x, pixel_y);
    if (!RuntimeLightEmitter3D_ResolveFirstHit(scene,
                                               &primary_ray,
                                               projector->nearPlane,
                                               HUGE_VAL,
                                               &trace) ||
        !trace.emitterWins) {
        return false;
    }

    *out_emitter_hit = trace.emitterHitInfo;
    {
        const RuntimeVisibility3DTransmittance transmittance =
            RuntimeVolume3D_TransmittanceAlongRayRGB(&scene->volume,
                                                     &primary_ray,
                                                     projector->nearPlane,
                                                     trace.emitterHitInfo.t);
        out_emitter_hit->radiance *= transmittance.luma;
    }
    return true;
}

void runtime_native_3d_render_write_radiance_rgb(float* radiance_buffer,
                                                 size_t pixel_index,
                                                 double radiance_r,
                                                 double radiance_g,
                                                 double radiance_b,
                                                 double background_floor) {
    const size_t base = pixel_index * (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
    if (!radiance_buffer) return;
    radiance_buffer[base] = (float)radiance_r;
    radiance_buffer[base + 1u] = (float)radiance_g;
    radiance_buffer[base + 2u] = (float)radiance_b;
    radiance_buffer[base + RUNTIME_NATIVE_3D_RADIANCE_BACKGROUND_FLOOR_CHANNEL] =
        (float)background_floor;
}

double runtime_native_3d_render_peak_rgb(double radiance_r,
                                         double radiance_g,
                                         double radiance_b) {
    double peak = radiance_r;
    if (radiance_g > peak) peak = radiance_g;
    if (radiance_b > peak) peak = radiance_b;
    return peak;
}

double runtime_native_3d_render_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

void runtime_native_3d_render_resolve_hit_tint(const HitInfo3D* hit,
                                               double* out_r,
                                               double* out_g,
                                               double* out_b) {
    RuntimeMaterialPayload3D payload = {0};
    double r = 1.0;
    double g = 1.0;
    double b = 1.0;

    if (hit &&
        hit->sceneObjectIndex >= 0 &&
        hit->sceneObjectIndex < sceneSettings.objectCount &&
        RuntimeMaterialPayload3D_ResolveFromHit(hit, &payload) &&
        payload.valid) {
        r = payload.baseColorR;
        g = payload.baseColorG;
        b = payload.baseColorB;
    }

    if (out_r) *out_r = r;
    if (out_g) *out_g = g;
    if (out_b) *out_b = b;
}

void runtime_native_3d_render_apply_ambient_hit_lighting(const RuntimeScene3D* scene,
                                                         const HitInfo3D* hit,
                                                         double* io_radiance_r,
                                                         double* io_radiance_g,
                                                         double* io_radiance_b,
                                                         double* io_peak_radiance,
                                                         bool* io_visible) {
    double tint_r = 1.0;
    double tint_g = 1.0;
    double tint_b = 1.0;
    double intensity = 0.0;
    double bias = 0.0;
    double up_facing = 0.0;
    double directional = 0.0;
    double ambient_r = 0.0;
    double ambient_g = 0.0;
    double ambient_b = 0.0;

    if (!scene || !hit || !io_radiance_r || !io_radiance_g || !io_radiance_b ||
        !io_peak_radiance) {
        return;
    }
    if (scene->environment.lightMode != ENVIRONMENT_LIGHT_MODE_AMBIENT || hit->triangleIndex < 0) {
        return;
    }

    intensity = RuntimeEnvironment3D_AmbientStrength(&scene->environment);
    if (!(intensity > 0.0)) return;

    bias = runtime_native_3d_render_clamp01(scene->environment.topDownBias);
    up_facing = runtime_native_3d_render_clamp01(hit->normal.z);
    directional = (1.0 - bias) + (bias * up_facing);
    runtime_native_3d_render_resolve_hit_tint(hit, &tint_r, &tint_g, &tint_b);

    ambient_r = intensity * directional * scene->environment.ambientColor.x * tint_r;
    ambient_g = intensity * directional * scene->environment.ambientColor.y * tint_g;
    ambient_b = intensity * directional * scene->environment.ambientColor.z * tint_b;
    *io_radiance_r += ambient_r;
    *io_radiance_g += ambient_g;
    *io_radiance_b += ambient_b;
    *io_peak_radiance = runtime_native_3d_render_peak_rgb(*io_radiance_r,
                                                          *io_radiance_g,
                                                          *io_radiance_b);
    if (io_visible && (ambient_r > 1e-6 || ambient_g > 1e-6 || ambient_b > 1e-6)) {
        *io_visible = true;
    }
}

void runtime_native_3d_render_background_rgb(const RuntimeScene3D* scene,
                                             const Ray3D* primary_ray,
                                             double* out_r,
                                             double* out_g,
                                             double* out_b) {
    double mix_t = 0.0;
    double strength = 0.0;
    Vec3 color = vec3(0.0, 0.0, 0.0);

    if (out_r) *out_r = 0.0;
    if (out_g) *out_g = 0.0;
    if (out_b) *out_b = 0.0;
    if (!scene || !primary_ray || scene->environment.lightMode != ENVIRONMENT_LIGHT_MODE_AMBIENT) {
        return;
    }

    strength = RuntimeEnvironment3D_BackgroundBrightness(&scene->environment);
    if (!(strength > 0.0)) return;

    mix_t = runtime_native_3d_render_clamp01((primary_ray->direction.z + 1.0) * 0.5);
    color = vec3_add(vec3_scale(scene->environment.backgroundBottomColor, 1.0 - mix_t),
                     vec3_scale(scene->environment.backgroundTopColor, mix_t));
    color = vec3_scale(color, strength);
    if (out_r) *out_r = color.x;
    if (out_g) *out_g = color.y;
    if (out_b) *out_b = color.z;
}

RuntimeVolume3DScatterResult runtime_native_3d_render_primary_scatter(
    const RuntimeScene3D* scene,
    const RuntimeCameraProjector3D* projector,
    double pixel_x,
    double pixel_y,
    double t_max,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeCausticVolumeCache3D* caustic_cache) {
    Ray3D primary_ray = {0};

    if (!scene || !projector) {
        RuntimeVolume3DScatterResult zero = {0};
        return zero;
    }
    if (scene->capabilities.valid &&
        scene->capabilities.canSkipVolumeScatter &&
        !RuntimeVolume3D_HasActiveExtinction(&scene->volume)) {
        RuntimeVolume3DScatterResult zero = {0};
        return zero;
    }

    primary_ray = RuntimeCameraProjector3D_MakePrimaryRay(projector, pixel_x, pixel_y);
    if (scene->volume.debugOverlayEnabled) {
        return RuntimeVolume3D_AccumulateDensityDebugAlongRayRGB(scene,
                                                                 &primary_ray,
                                                                 projector->nearPlane,
                                                                 t_max);
    }
    return RuntimeVolume3D_AccumulateSingleScatterAlongRayWithCausticCacheRGB(
        scene,
        &primary_ray,
        projector->nearPlane,
        t_max,
        sampling,
        caustic_cache);
}

void runtime_native_3d_render_record_scatter_stats(
    RuntimeNative3DRenderStats* io_stats,
    const RuntimeVolume3DScatterResult* scatter) {
    if (!io_stats || !scatter) return;
    io_stats->volumeScatterDirectSampleCount += scatter->directSampleCount;
    io_stats->totalDirectVolumeScatterRadianceR += scatter->directRadianceR;
    io_stats->totalDirectVolumeScatterRadianceG += scatter->directRadianceG;
    io_stats->totalDirectVolumeScatterRadianceB += scatter->directRadianceB;
    io_stats->causticVolumeScatterSampleCount += scatter->causticSampleCount;
    io_stats->causticVolumeScatterContributingSampleCount +=
        scatter->causticContributingSampleCount;
    io_stats->totalCausticVolumeScatterRadianceR += scatter->causticRadianceR;
    io_stats->totalCausticVolumeScatterRadianceG += scatter->causticRadianceG;
    io_stats->totalCausticVolumeScatterRadianceB += scatter->causticRadianceB;
}

void runtime_native_3d_render_apply_scatter_rgb(
    double* io_radiance_r,
    double* io_radiance_g,
    double* io_radiance_b,
    double* io_peak_radiance,
    bool* io_visible,
    const RuntimeVolume3DScatterResult* scatter) {
    double radiance_r = 0.0;
    double radiance_g = 0.0;
    double radiance_b = 0.0;

    if (!io_radiance_r || !io_radiance_g || !io_radiance_b || !io_peak_radiance || !scatter ||
        !scatter->active) {
        return;
    }

    radiance_r = *io_radiance_r + scatter->radianceR;
    radiance_g = *io_radiance_g + scatter->radianceG;
    radiance_b = *io_radiance_b + scatter->radianceB;
    *io_radiance_r = radiance_r;
    *io_radiance_g = radiance_g;
    *io_radiance_b = radiance_b;
    *io_peak_radiance = runtime_native_3d_render_peak_rgb(radiance_r, radiance_g, radiance_b);
    if (io_visible) {
        *io_visible = true;
    }
}

void runtime_native_3d_render_write_background_radiance(float* radiance_buffer,
                                                        size_t pixel_index,
                                                        const RuntimeScene3D* scene,
                                                        const RuntimeCameraProjector3D* projector,
                                                        const Ray3D* primary_ray,
                                                        const RuntimeVolume3DScatterResult* scatter) {
    double background_r = 0.0;
    double background_g = 0.0;
    double background_b = 0.0;
    double scatter_r = 0.0;
    double scatter_g = 0.0;
    double scatter_b = 0.0;

    if (scene && projector && primary_ray) {
        const RuntimeVisibility3DTransmittance transmittance =
            RuntimeVolume3D_TransmittanceAlongRayRGB(&scene->volume,
                                                     primary_ray,
                                                     projector->nearPlane,
                                                     HUGE_VAL);
        runtime_native_3d_render_background_rgb(scene,
                                                primary_ray,
                                                &background_r,
                                                &background_g,
                                                &background_b);
        background_r *= transmittance.r;
        background_g *= transmittance.g;
        background_b *= transmittance.b;
    }
    if (scatter && scatter->active) {
        scatter_r = scatter->radianceR;
        scatter_g = scatter->radianceG;
        scatter_b = scatter->radianceB;
    }
    runtime_native_3d_render_write_radiance_rgb(radiance_buffer,
                                                pixel_index,
                                                background_r + scatter_r,
                                                background_g + scatter_g,
                                                background_b + scatter_b,
                                                0.0);
}

void runtime_native_3d_render_write_emitter_radiance_with_scatter(
    float* radiance_buffer,
    size_t pixel_index,
    const RuntimeScene3D* scene,
    const RuntimeCameraProjector3D* projector,
    double pixel_x,
    double pixel_y,
    const RuntimeLightEmitterHit3DResult* hit,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeCausticVolumeCache3D* caustic_cache,
    RuntimeNative3DRenderStats* io_stats) {
    RuntimeVolume3DScatterResult scatter = {0};
    double radiance_r = 0.0;
    double radiance_g = 0.0;
    double radiance_b = 0.0;
    double peak = 0.0;
    double background_floor = 0.0;

    if (!radiance_buffer || !scene || !projector || !hit || !io_stats) return;

    scatter = runtime_native_3d_render_primary_scatter(scene,
                                                       projector,
                                                       pixel_x,
                                                       pixel_y,
                                                       hit->t,
                                                       sampling,
                                                       caustic_cache);
    runtime_native_3d_render_record_scatter_stats(io_stats, &scatter);
    radiance_r = hit->radiance;
    radiance_g = hit->radiance;
    radiance_b = hit->radiance;
    peak = hit->radiance;
    runtime_native_3d_render_apply_scatter_rgb(&radiance_r,
                                               &radiance_g,
                                               &radiance_b,
                                               &peak,
                                               NULL,
                                               &scatter);
    io_stats->hitPixelCount += 1;
    io_stats->visiblePixelCount += 1;
    if (peak > io_stats->maxRadiance) {
        io_stats->maxRadiance = peak;
    }
    background_floor = RuntimeEnvironment3D_BackgroundBrightness(&scene->environment);
    background_floor = runtime_native_3d_render_clamp01(background_floor);
    runtime_native_3d_render_write_radiance_rgb(radiance_buffer,
                                                pixel_index,
                                                radiance_r,
                                                radiance_g,
                                                radiance_b,
                                                background_floor);
}
