#include "tools/ray_tracing_headless_spectral_map.h"
#include "render/runtime_spectral_surface_map.h"
#include "render/runtime_spectral_observer.h"
#include "render/runtime_spectral_volume.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

/* The local CLI runs preflight and render sequentially. Only this process owns the cached records. */
static RuntimeSpectralSurfaceMap *cached;
static RuntimeSpectralMapStats built;
static bool registered;
static RuntimeSpectralVolume *cached_volume;
static void release_map(void) {
    RuntimeSpectralSurfaceMap_Free(cached);
    cached = NULL;
    RuntimeSpectralVolume_Free(cached_volume);
    cached_volume = NULL;
}

static double now_ms(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec * 1e3 + now.tv_nsec * 1e-6;
}
/* Explicit RGB compatibility basis for native material textures. Convex
 * interpolation keeps bounded RGB reflectance bounded at every wavelength. */
static double rgb_reflectance(Vec3 rgb, double nm) {
    double t = fmax(0, fmin(1, (nm - 455) / 90));
    double bg = (1 - t) * rgb.z + t * rgb.y;
    t = fmax(0, fmin(1, (nm - 545) / 65));
    return fmax(0, fmin(1, (1 - t) * bg + t * rgb.x));
}
bool RayTracingHeadlessSpectralMap_Run(const RuntimeSpectralPencil *request,
                                       const RuntimeNative3DPreparedFrame *frame, uint8_t *pixels,
                                       RuntimeSpectralReadout *out, char *diag, size_t size) {
    const RuntimeScene3D *scene = &frame->scene;
    RuntimeSpectralMapStats *st = &out->map;
    RuntimeSpectralPencil config = *request;
    RuntimeSpectralPencil *p = &config;
    if (p->volume.kind) {
        p->active_volume = RuntimeSpectralVolume_Load(&p->volume, diag, size);
        if (!p->active_volume) return false;
    }
    char key[65];
    RuntimeSpectralSurfaceMap_Key(p, scene, key);
    bool reuse = cached && !strcmp(key, built.key);
    if (!reuse) {
        release_map();
        cached_volume = p->active_volume;
        memset(&built, 0, sizeof(built));
        if (cached_volume) RuntimeSpectralVolume_Identity(cached_volume, &built);
        if (!registered) {
            if (atexit(release_map) != 0) {
                release_map();
                return false;
            }
            registered = true;
        }
        double started = now_ms();
        cached = RuntimeSpectralSurfaceMap_Build(p, scene, &built);
        if (!cached) {
            snprintf(diag, size,
                     "spectral map preparation failed: invalid transport, source or allocation");
            release_map();
            return false;
        }
        built.build_ms = now_ms() - started;
        memcpy(built.key, key, sizeof(key));
    }
    if (reuse) {
        RuntimeSpectralVolume_Free(p->active_volume);
        p->active_volume = cached_volume;
    }
    *st = built;
    st->reused = reuse;
    if (!pixels) return true;
    double film_start = now_ms();
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
    for (int y = 0; y < frame->height; ++y)
        for (int x = 0; x < frame->width; ++x) {
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
                    if (!b && !hit.interfaces) samples = 1;
                    double radiance =
                        hit.receiver
                            ? RuntimeSpectralSurfaceMap_Radiance(cached, p, scene, &hit, b, &st->candidates)
                            : 0;
                    if (hit.receiver && (p->reflectance.count || p->use_scene_texture) &&
                        !strcmp(scene->primitives[hit.primitive].source.objectId, p->receiver_id))
                        radiance *= RuntimeSpectralCurve_Value(&p->reflectance, line.vacuum_nm) *
                                    rgb_reflectance(hit.receiver_rgb, line.vacuum_nm);
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
            RayTracingHeadlessSpectral_Pixel(pixels + ((size_t)y * frame->width + x) *
                                                          RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES,
                                             xyz, p->exposure);
        }
    st->film_ms = now_ms() - film_start;
    return true;
}
void RayTracingHeadlessSpectralMap_Write(FILE *f, const RuntimeSpectralMapStats *st, bool rendered) {
    if (st->volume_digest[0]) {
        fprintf(
            f,
            "  \"spectral_volume\":{\"field_sha256\":\"%s\",\"source_sha256\":\"%s\",\"frame_sha256\":\"%s\",\"frame_index\":%d,\"time_s\":%.17g,\"dt_s\":%.17g,\"bands\":[",
            st->volume_digest, st->volume_source_sha, st->volume_frame_sha, st->volume_frame,
            st->volume_time, st->volume_dt);
        for (int b = 0; b < st->band_count; ++b) {
            const RuntimeSpectralBand *r = &st->bands[b];
            fprintf(
                f,
                "%s{\"vacuum_nm\":%.17g,\"launched_w\":%.17g,\"absorbed_w\":%.17g,\"receiver_w\":%.17g,\"escaped_w\":%.17g,\"depth_limited_w\":%.17g,\"surface_loss_w\":%.17g,\"first_scatter_w\":%.17g,\"higher_scatter_w\":%.17g,\"scatter_events\":%llu,\"sum_cosine\":%.17g",
                b ? "," : "", r->nm, r->launched_w, r->absorbed_w, r->receiver_w, r->escaped_w,
                r->limited_w, r->optical_loss_w, r->first_scatter_w, r->higher_scatter_w, r->collisions,
                r->cosine_sum);
            if (rendered)
                fprintf(f, ",\"center_single_radiance\":%.17g,\"center_multiple_radiance\":%.17g",
                        r->center_single, r->center_multiple);
            fputc('}', f);
        }
        fprintf(f, "]},\n");
    }
    fprintf(f,
            "  \"spectral_surface_map\":{\"key\":\"%s\",\"reused\":%s,\"launched\":%d,"
            "\"deposits\":%d,\"direct_arrivals\":%d,\"missed\":%d,\"depth_limited\":%d,"
            "\"record_bytes\":%zu,\"gather_candidates\":%llu,\"bands\":[",
            st->key, st->reused ? "true" : "false", st->launched, st->deposits, st->direct_arrivals,
            st->missed, st->depth_limited, RuntimeSpectralSurfaceMap_RecordBytes(), st->candidates);
    for (int b = 0; b < st->band_count; ++b) {
        const RuntimeSpectralBand *row = &st->bands[b];
        fprintf(f,
                "%s{\"vacuum_nm\":%.17g,\"width_nm\":%.17g,\"deposits\":%u,\"watts\":%.17g,"
                "\"centroid_m\":",
                b ? "," : "", row->nm, row->width_nm, row->count, row->watts);
        if (row->watts > 0)
            fprintf(f, "[%.17g,%.17g,%.17g]", row->moment.x / row->watts, row->moment.y / row->watts,
                    row->moment.z / row->watts);
        else
            fprintf(f, "null");
        if (rendered) fprintf(f, ",\"center_band_radiance\":%.17g", row->center_radiance);
        fputc('}', f);
    }
    fprintf(f, "],\"build_ms\":%.6f,\"film_ms\":%.6f},\n", st->build_ms, st->film_ms);
}
