#include "render/runtime_spectral_film.h"
#include "render/runtime_spectral_diffuse.h"
#include "import/runtime_mesh_asset_loader.h"
#include "render/runtime_spectral_observer.h"
#include "render/runtime_spectral_volume.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
bool RuntimeSpectralFilm_IsClosedMesh(const char *id) {
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
    bool glass = p->volume.kind && !p->glass_id[0], receiver = false, panel = !p->diffuse.enabled;
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
            glass = prim->kind == RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH && RuntimeSpectralFilm_IsClosedMesh(p->glass_id);
        else if (prim->kind != RUNTIME_PRIMITIVE_3D_KIND_PLANE)
            return false;
        if (p->diffuse.enabled && !strcmp(prim->source.objectId, p->diffuse.panel_id))
            panel = prim->kind == RUNTIME_PRIMITIVE_3D_KIND_PLANE;
        if (!strcmp(prim->source.objectId, p->receiver_id))
            receiver = prim->kind == RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    }
    return glass && receiver && panel;
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
void RuntimeSpectral_DisplayPixelWithTransform(uint8_t *pixel, Vec3 rgb, bool soft) {
    if (soft) {
        /* Move out-of-gamut colors toward a neutral of the same luminance
         * until the lowest channel reaches zero. Use one exposure scale
         * for all channels so highlights roll off without channel plateaus. */
        double low = fmin(rgb.x, fmin(rgb.y, rgb.z));
        if (low < 0) {
            double y = fmax(0, .2126729 * rgb.x + .7151522 * rgb.y + .0721750 * rgb.z);
            double chroma = y / (y - low);
            rgb = vec3(chroma * (rgb.x - low), chroma * (rgb.y - low), chroma * (rgb.z - low));
        }
        double peak = fmax(rgb.x, fmax(rgb.y, rgb.z));
        rgb = vec3_scale(rgb, 1 / (1 + peak));
    }
    RuntimeSpectral_DisplayPixel(pixel, rgb);
}

static bool spectral_film_region(const RuntimeSpectralPencil *p, const RuntimeSpectralSurfaceMap *cached,
                                const RuntimeNative3DPreparedFrame *frame, int x0, int y0, int x1, int y1,
                                uint8_t *pixels, float *radiance, int stride, RuntimeSpectralReadout *out,
                                char *diag, size_t size, double dx, double dy, const uint8_t *mask, bool signed_rgb,
                                const RuntimeNative3DSamplingContext *camera_sampling) {
    const RuntimeScene3D *scene = frame->traceScene ? frame->traceScene : &frame->scene;
    RuntimeSpectralMapStats *st = &out->map;
    RuntimeSpectralSkyLighting *sky = p->diffuse.enabled
        ? RuntimeSpectralSkyLighting_Create(p, scene) : NULL;
    if (p->diffuse.enabled && !sky) {
        snprintf(diag, size, "cannot prepare mixed sky panel");
        return false;
    }
    RuntimeSpectralCameraCache *camera_cache = p->active_volume && !p->volume.disable_camera_cache
        ? RuntimeSpectralCameraCache_Create() : NULL; /* Allocation failure retains scalar traversal. */
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
            if (mask && !mask[(size_t)y * frame->width + x]) continue;
            Vec3 xyz = vec3(0, 0, 0);
            double px = x + dx, py = y + dy;
            if (camera_sampling)
                RuntimeNative3DSampling_PixelPosition(camera_sampling, x, y, &px, &py);
            RuntimeSpectralProbeReadout *probe = pixels && p->probe.enabled &&
                x == p->probe.x && y == p->probe.y ? &out->probe : NULL;
            if (probe) *probe = (RuntimeSpectralProbeReadout){.valid=true, .primitive=-1};
            double signals[4] = {0};
            int grid = p->pixel_grid > 0 ? p->pixel_grid : 1;
            int taps = grid * grid;
            for (int tap = 0; tap < taps; ++tap) {
                double sx = px + ((tap % grid + .5) / grid - .5);
                double sy = py + ((tap / grid + .5) / grid - .5);
                Ray3D ray = RuntimeCameraProjector3D_MakePrimaryRay(&frame->projector, sx, sy);
                int samples = p->fresnel_camera_samples;
                if (!samples) samples = p->glass_roughness > 0 ? 16 : 1;
                for (int j = 0; j < samples; ++j) {
                    RuntimeSpectralPathReadout hit = {0};
                    for (int b = 0; b < st->band_count; ++b) {
                        RuntimeSpectralPencilLine line = {st->bands[b].nm, 1, sigma[b],
                                                          UINT64_C(0x8fea67c3b21d0000) ^
                                                              ((uint64_t)p->map.seed << 32) ^
                                                              (((uint64_t)y * frame->width + x) *
                                                               (p->fresnel_camera_samples ? 64 : 16) + j),
                                                          b, camera_cache};
                        if (tap != 0)
                            line.sample_id ^= (uint64_t)tap * UINT64_C(0xd1b54a32d192ed03);
                        /* An air-only camera segment is achromatic. A dielectric path is retraced for each
                         * band.
                         */
                        if ((!b || hit.interfaces || p->active_volume) &&
                            !RuntimeSpectralPencil_Trace(scene, p, &line, indices[b], ray, true, &hit)) {
                            const char *volume_error = RuntimeSpectralVolume_Error(p->active_volume);
                            snprintf(diag, size, "spectral camera pixel (%d,%d), band %d: %s",
                                     x, y, b, strcmp(volume_error, "spectral volume reconstruction failed") != 0 ? volume_error :
                                     "inconsistent glass boundary");
                            RuntimeSpectralCameraCache_Free(camera_cache, NULL);
                            RuntimeSpectralSkyLighting_Free(sky);
                            return false;
                        }
                        /* A direct camera path never samples a rough interface. */
                        if (!b && !hit.interfaces)
                            samples = 1;
                        if (probe && tap == 0 && j == 0 && b == 0 && hit.receiver) {
                            probe->hit = true;
                            probe->primitive = hit.primitive;
                            probe->position = hit.position;
                            probe->base_support_m = p->receiver_radius_m;
                            probe->scattered_support_m = p->map.scattered_support_m;
                        }
                        double normalizer = samples * taps;
                        unsigned long long before = st->candidates;
                        double direct = 0, reflectance = 1;
                        RuntimeSpectralDepositProbe deposit;
                        RuntimeSpectralDepositColor color = {0};
                        bool exact = p->exact_surface_observer && st->bands[b].width_nm > 0;
                        if (probe) deposit = (RuntimeSpectralDepositProbe){0};
                        double radiance =
                            hit.receiver
                                ? RuntimeSpectralSurfaceMap_Gather(cached, p, scene, &hit, b, &st->candidates,
                                    probe ? &direct : NULL, probe ? &deposit : NULL, exact ? &color : NULL)
                                : 0;
                        if (hit.receiver && (p->reflectance.count || p->use_scene_texture) &&
                            !strcmp(scene->primitives[hit.primitive].source.objectId, p->receiver_id)) {
                            reflectance = RuntimeSpectralCurve_Value(&p->reflectance, line.vacuum_nm) *
                                          RuntimeSpectral_RGBBasis(hit.receiver_rgb, line.vacuum_nm);
                            radiance *= reflectance;
                        }
                        if (sky && hit.receiver &&
                            !strcmp(scene->primitives[hit.primitive].source.objectId, p->diffuse.panel_id)) {
                            reflectance = RuntimeSpectralCurve_Value(&p->diffuse.panel_reflectance, line.vacuum_nm);
                            radiance *= reflectance;
                        }
                        double sky_direct = 0, sky_indirect = 0;
                        if (st->bands[b].width_nm == 0)
                            RuntimeSpectralSkyLighting_Evaluate(sky, &hit, line.vacuum_nm, &sky_direct, &sky_indirect);
                        double surface = radiance;
                        if (sky) surface += (sky_direct + sky_indirect) * reflectance;
                        double contribution = (hit.inscattered + hit.weight * surface) / normalizer;
                        if (probe) {
                            RuntimeSpectralProbeBand *row = &probe->bands[b];
                            row->direct += hit.weight * direct * reflectance / normalizer;
                            row->sky_direct += hit.weight * sky_direct * reflectance / normalizer;
                            row->indirect += hit.weight * sky_indirect * reflectance / normalizer;
                            row->mapped_surface += hit.weight * (radiance - direct * reflectance) / normalizer;
                            if (hit.receiver) row->mean_receiver_reflectance += reflectance / normalizer;
                            deposit.radiance *= hit.weight * reflectance / normalizer;
                            if (deposit.radiance > row->largest_deposit.radiance) row->largest_deposit = deposit;
                            row->single_scatter += hit.single_scatter / normalizer;
                            row->multiple_scatter += (hit.inscattered - hit.single_scatter) / normalizer;
                            row->camera_throughput += hit.weight / normalizer;
                            row->candidates += st->candidates - before;
                            ++row->samples;
                            row->interfaces += hit.interfaces;
                            row->depth_limited += hit.depth_limited;
                        }
                        for (int i = 0; i < p->sensor_count; ++i)
                            signals[i] += responses[i][b] * contribution;
                        if (x == frame->width / 2 && y == frame->height / 2) {
                            st->bands[b].center_radiance += contribution;
                            st->bands[b].center_single += hit.single_scatter / normalizer;
                            st->bands[b].center_multiple += (hit.inscattered - hit.single_scatter) / normalizer;
                        }
                        Vec3 band_xyz = vec3_scale(observer[b], contribution);
                        if (exact && color.radiance > 0) {
                            /* Replace only the gathered surface observer term.
                             * The camera weight, material response and sensor
                             * integrals keep their original band evaluation. */
                            Vec3 delta = vec3_sub(color.xyz, vec3_scale(observer[b], color.radiance));
                            band_xyz = vec3_add(band_xyz, vec3_scale(delta, hit.weight * reflectance / normalizer));
                        }
                        xyz = vec3_add(xyz, band_xyz);
                    }
                }
            }
            if (x == frame->width / 2 && y == frame->height / 2) {
                out->center_pixel_xyz = xyz;
                memcpy(out->sensor_signal, signals, sizeof(signals));
            }
            if (y == p->strip_y && x >= p->strip_x && x - p->strip_x < p->strip_count)
                out->strip_xyz[x - p->strip_x] = xyz;
            if (probe) { probe->xyz = xyz; probe->signed_rgb = RuntimeSpectral_LinearRGB(xyz); }
            if (p->display_sensor >= 0)
                xyz = vec3_scale(vec3(.95047, 1, 1.08883), signals[p->display_sensor]);
            Vec3 rgb = RuntimeSpectral_LinearRGB(vec3_scale(xyz, p->exposure));
            if (pixels)
                RuntimeSpectral_DisplayPixelWithTransform(pixels + ((size_t)y * frame->width + x) * 4, rgb, p->soft_display);
            else {
                size_t i = ((size_t)(y - y0) * stride + x - x0) * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
                radiance[i] = signed_rgb ? rgb.x : fmax(0, rgb.x);
                radiance[i + 1] = signed_rgb ? rgb.y : fmax(0, rgb.y);
                radiance[i + 2] = signed_rgb ? rgb.z : fmax(0, rgb.z);
                radiance[i + 3] = 0;
            }
        }
    RuntimeSpectralCameraCache_Free(camera_cache, st);
    RuntimeSpectralSkyLighting_Free(sky);
    if(p->active_volume)RuntimeSpectralVolume_ScatterStats(p->active_volume,st);
    return true;
}

bool RuntimeSpectralFilm_Region(const RuntimeSpectralPencil *p, const RuntimeSpectralSurfaceMap *map,
    const RuntimeNative3DPreparedFrame *frame, int x0, int y0, int x1, int y1,
    uint8_t *pixels, float *radiance, int stride, RuntimeSpectralReadout *out, char *diag, size_t size) {
    return spectral_film_region(p, map, frame, x0, y0, x1, y1, pixels, radiance, stride,
                                out, diag, size, 0, 0, NULL, false, NULL);
}
bool RuntimeSpectralFilm_Sample(const RuntimeSpectralPencil *p, const RuntimeSpectralSurfaceMap *map,
    const RuntimeNative3DPreparedFrame *frame, int y0, int y1, double dx, double dy,
    const RuntimeNative3DSamplingContext *camera_sampling, const uint8_t *mask, float *radiance, RuntimeSpectralReadout *out, char *diag, size_t size) {
    return spectral_film_region(p, map, frame, 0, y0, frame->width, y1, NULL,
        radiance + (size_t)y0 * frame->width * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS,
        frame->width, out, diag, size, dx, dy, mask, true, camera_sampling);
}
