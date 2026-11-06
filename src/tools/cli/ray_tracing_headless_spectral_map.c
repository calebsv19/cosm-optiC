#include "render/runtime_spectral_film.h"
#include "render/runtime_spectral_convergence.h"
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
bool RayTracingHeadlessSpectralMap_Run(const RayTracingAgentRenderRequest *agent,
                                       const RuntimeNative3DPreparedFrame *frame, uint8_t *pixels,
                                       RuntimeSpectralReadout *out, char *diag, size_t size) {
    const RuntimeSpectralPencil *request = &agent->spectral;
    if (pixels && request->sampling.enabled)
        return RayTracingHeadlessSpectralMap_Converge(agent,frame,pixels,out,diag,size);
    const RuntimeScene3D *scene = &frame->scene;
    RuntimeSpectralMapStats *st = &out->map;
    RuntimeSpectralPencil config = *request;
    RuntimeSpectralPencil *p = &config;
    if (p->volume.kind) {
        p->active_volume = RuntimeSpectralVolume_Load(&p->volume, diag, size);
        if (!p->active_volume) return false;
        if(!RuntimeSpectralVolume_BindScene(p->active_volume,scene,diag,size)) {
            RuntimeSpectralVolume_Free(p->active_volume); return false;
        }
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
                     "spectral map preparation failed: %s", p->volume.reconstruction
                         ? RuntimeSpectralVolume_Error(p->active_volume)
                         : "invalid transport, source or allocation");
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
    out->sampling.enabled=request->sampling.enabled;
    if (!pixels) return true;
    double film_start = now_ms();
    if (!RuntimeSpectralFilm_Region(p, cached, frame, 0, 0, frame->width, frame->height,
                                    pixels, NULL, 0, out, diag, size)) return false;
    st->film_ms = now_ms() - film_start;
    return true;
}
void RayTracingHeadlessSpectralMap_Write(FILE *f, const RuntimeSpectralMapStats *st, bool rendered) {
    fprintf(f, "  \"spectral_camera_cache\":{\"bytes\":%zu,\"hits\":%llu,\"misses\":%llu,\"reused_segments\":%llu,\"overflows\":%llu},\n",
            st->camera_cache_bytes,st->camera_cache_hits,st->camera_cache_misses,st->camera_cache_segments,st->camera_cache_overflows);
    if (st->reconstruction_bytes) {
        fprintf(f,"  \"spectral_reconstruction\":{\"owned_bytes\":%llu,\"peak_bytes\":%llu,"
                  "\"nodes\":%llu,\"links\":%llu,\"regions\":%d,\"cached_node_queries\":%llu,\"exact_node_queries\":%llu,\"spatial_hits\":%llu,\"spatial_misses\":%llu},\n",
                st->reconstruction_bytes,st->reconstruction_peak_bytes,st->reconstruction_nodes,
                st->reconstruction_links,st->reconstruction_regions,st->reconstruction_cached_queries,st->reconstruction_exact_queries,
                st->reconstruction_spatial_hits,st->reconstruction_spatial_misses);
    }
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
            "\"gather\":\"%s\",\"expanded_deposits\":%d,\"deposits\":%d,\"direct_arrivals\":%d,\"missed\":%d,\"depth_limited\":%d,"
            "\"record_bytes\":%zu,\"gather_candidates\":%llu,\"bands\":[",
            st->key, st->reused ? "true" : "false", st->launched,
            st->adaptive_scattered ? "adaptive_scattered" : "fixed", st->expanded_deposits,
            st->deposits, st->direct_arrivals,
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
