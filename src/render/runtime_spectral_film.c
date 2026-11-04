#include "render/runtime_spectral_film.h"
#include "import/runtime_mesh_asset_loader.h"
#include "render/runtime_spectral_observer.h"
#include "render/runtime_spectral_volume.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static bool closed_mesh(const char *id) {
    const RayTracingRuntimeMeshAssetSet *assets = ray_tracing_runtime_mesh_assets_last();
    for (int i = 0; assets && i < assets->instance_count; ++i) {
        const RayTracingRuntimeMeshAssetInstance *instance = &assets->instances[i];
        if (strcmp(instance->object_id, id) || instance->asset_index < 0 ||
            instance->asset_index >= assets->asset_count)
            continue;
        return assets->assets[instance->asset_index].document.contract.topology_closed_volume;
    }
    return false;
}

bool RuntimeSpectralFilm_Supported(const RuntimeScene3D *scene, const RuntimeSpectralPencil *p) {
    bool glass = p->volume.kind && !p->glass_id[0], receiver = false;
    if ((!p->map.source_count && scene->primitiveCount != 2) || scene->volume.enabled ||
        scene->curveInstanceCount)
        return false;
    for (int i = 0; i < scene->lightSet.lightCount; ++i) {
        const RuntimeLightSource3D *l = &scene->lightSet.lights[i];
        if (l->enabled && l->origin != RUNTIME_LIGHT_SOURCE_3D_ORIGIN_COMPAT_SCENE_LIGHT)
            return false;
    }
    for (int i = 0; i < scene->primitiveCount; ++i) {
        const RuntimePrimitive3D *prim = &scene->primitives[i];
        if (!strcmp(prim->source.objectId, p->glass_id))
            glass = prim->kind == RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH && closed_mesh(p->glass_id);
        else if (prim->kind != RUNTIME_PRIMITIVE_3D_KIND_PLANE)
            return false;
        if (!strcmp(prim->source.objectId, p->receiver_id))
            receiver = prim->kind == RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    }
    return glass && receiver;
}

/* Explicit RGB compatibility basis for native material textures. Convex
 * interpolation keeps bounded RGB reflectance bounded at every wavelength. */
double RuntimeSpectral_RGBBasis(Vec3 rgb, double nm) {
    double t = fmax(0, fmin(1, (nm - 455) / 90));
    double bg = (1 - t) * rgb.z + t * rgb.y;
    t = fmax(0, fmin(1, (nm - 545) / 65));
    return fmax(0, fmin(1, (1 - t) * bg + t * rgb.x));
}
static uint8_t display(double linear) {
    double c = fmin(1, fmax(0, linear));
    return (uint8_t)lround(255 * (c <= 0.0031308 ? 12.92 * c : 1.055 * pow(c, 1 / 2.4) - 0.055));
}

Vec3 RuntimeSpectral_LinearRGB(Vec3 xyz) {
    return vec3(3.2404542 * xyz.x - 1.5371385 * xyz.y - 0.4985314 * xyz.z,
                -0.9692660 * xyz.x + 1.8760108 * xyz.y + 0.0415560 * xyz.z,
                0.0556434 * xyz.x - 0.2040259 * xyz.y + 1.0572252 * xyz.z);
}

void RuntimeSpectral_DisplayPixel(uint8_t *pixel, Vec3 rgb) {
    pixel[0] = display(rgb.x);
    pixel[1] = display(rgb.y);
    pixel[2] = display(rgb.z);
    pixel[3] = 255;
}
bool RuntimeSpectralFilm_Region(const RuntimeSpectralPencil *p, const RuntimeSpectralSurfaceMap *cached,
                                const RuntimeNative3DPreparedFrame *frame, int x0, int y0, int x1, int y1,
                                uint8_t *pixels, float *radiance, int stride, RuntimeSpectralReadout *out,
                                char *diag, size_t size) {
    const RuntimeScene3D *scene = frame->traceScene ? frame->traceScene : &frame->scene;
    RuntimeSpectralMapStats *st = &out->map;
    double sigma[SPECTRAL_MAP_MAX_BANDS];
    double indices[SPECTRAL_MAP_MAX_BANDS];
    Vec3 observer[SPECTRAL_MAP_MAX_BANDS];
    for (int b = 0; b < st->band_count; ++b) {
        sigma[b] = RuntimeSpectralSurfaceMap_Absorption(&p->map, st->bands[b].nm);
        indices[b] = RuntimeSpectralPencil_Index(p, st->bands[b].nm);
        observer[b] = RuntimeSpectral_CIE1931(st->bands[b].nm);
    }
    double responses[4][SPECTRAL_MAP_MAX_BANDS];
    for (int i = 0; i < p->sensor_count; ++i)
        for (int b = 0; b < st->band_count; ++b) {
            double nm = st->bands[b].nm;
            responses[i][b] = RuntimeSpectralCurve_Value(&p->sensors[i].response, nm);
            if (p->sensors[i].quantum_efficiency)
                responses[i][b] *= nm * 1e-9 / (6.62607015e-34 * 299792458.);
        }
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            Vec3 xyz = vec3(0, 0, 0);
            Ray3D ray = RuntimeCameraProjector3D_MakePrimaryRay(&frame->projector, x, y);
            double signals[4] = {0};
            int samples = p->glass_roughness > 0 ? 16 : 1;
            for (int j = 0; j < samples; ++j) {
                RuntimeSpectralPathReadout hit = {0};
                for (int b = 0; b < st->band_count; ++b) {
                    RuntimeSpectralPencilLine line = {st->bands[b].nm, 1, sigma[b],
                                                      UINT64_C(0x8fea67c3b21d0000) ^
                                                          ((uint64_t)p->map.seed << 32) ^
                                                          (((uint64_t)y * frame->width + x) * 16 + j),
                                                      b};
                    /* An air-only camera segment is achromatic. A dielectric path is retraced for each
                     * band.
                     */
                    if ((!b || hit.interfaces || p->active_volume) &&
                        !RuntimeSpectralPencil_Trace(scene, p, &line, indices[b], ray, true, &hit)) {
                        snprintf(diag, size,
                                 "spectral camera path encountered an inconsistent glass boundary");
                        return false;
                    }
                    /* A direct camera path never samples a rough interface. */
                    if (!b && !hit.interfaces)
                        samples = 1;
                    double radiance =
                        hit.receiver
                            ? RuntimeSpectralSurfaceMap_Radiance(cached, p, scene, &hit, b, &st->candidates)
                            : 0;
                    if (hit.receiver && (p->reflectance.count || p->use_scene_texture) &&
                        !strcmp(scene->primitives[hit.primitive].source.objectId, p->receiver_id))
                        radiance *= RuntimeSpectralCurve_Value(&p->reflectance, line.vacuum_nm) *
                                    RuntimeSpectral_RGBBasis(hit.receiver_rgb, line.vacuum_nm);
                    double contribution = (hit.inscattered + hit.weight * radiance) / samples;
                    for (int i = 0; i < p->sensor_count; ++i)
                        signals[i] += responses[i][b] * contribution;
                    if (x == frame->width / 2 && y == frame->height / 2) {
                        st->bands[b].center_radiance += contribution;
                        st->bands[b].center_single += hit.single_scatter / samples;
                        st->bands[b].center_multiple += (hit.inscattered - hit.single_scatter) / samples;
                    }
                    xyz = vec3_add(xyz, vec3_scale(observer[b], contribution));
                }
            }
            if (x == frame->width / 2 && y == frame->height / 2) {
                out->center_pixel_xyz = xyz;
                memcpy(out->sensor_signal, signals, sizeof(signals));
            }
            if (p->display_sensor >= 0)
                xyz = vec3_scale(vec3(.95047, 1, 1.08883), signals[p->display_sensor]);
            Vec3 rgb = RuntimeSpectral_LinearRGB(vec3_scale(xyz, p->exposure));
            if (pixels)
                RuntimeSpectral_DisplayPixel(pixels + ((size_t)y * frame->width + x) * 4, rgb);
            else {
                size_t i = ((size_t)(y - y0) * stride + x - x0) * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
                radiance[i] = fmax(0, rgb.x);
                radiance[i + 1] = fmax(0, rgb.y);
                radiance[i + 2] = fmax(0, rgb.z);
                radiance[i + 3] = 0;
            }
        }
    if(p->active_volume)RuntimeSpectralVolume_ScatterStats(p->active_volume,st);
    return true;
}
