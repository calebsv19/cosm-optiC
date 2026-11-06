#include "runtime_spectral_volume_internal.h"
#include "render/runtime_volume_3d_sampling.h"
#include "render/runtime_water_body_prepare_3d.h"
#include "import/fluid_volume_import_3d.h"
#include "app/ray_tracing_sha256.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static bool load_water(RuntimeSpectralVolume *v, RayTracingSha256Context *hash, char *diag, size_t size) {
    RuntimeWaterSurfaceFrame f;
    RuntimeWaterSurfaceFrame_Init(&f);
    bool found = false;
    bool ok = RuntimeWaterSurfaceImport_LoadSourceAtFrame(v->config.path, v->config.frame_index, &f, &found,
                                                          diag, size) &&
              found && f.frame_index == (uint64_t)v->config.frame_index && f.grid_w >= 2 && f.grid_d >= 2;
    RuntimeWaterBodyPrepare3DReport report;
    if (ok) ok = ray_tracing_sha256_file(f.frame_path, v->frame_sha);
    if (ok) ok = RuntimeWaterBodyPrepare3D_Field(&f, 0, &v->water, &v->heights, &report, diag, size);
    if (ok) {
        RuntimeWaterBodyMesh3DDesc *w = &v->water;
        v->lo = vec3(w->sample_origin_x, w->sample_origin_z, w->bottom_height);
        double top = w->bottom_height;
        for (uint64_t i = 0; i < f.sample_count; ++i)
            top = fmax(top, v->heights[i]);
        v->hi = vec3(v->lo.x + (w->grid_w - 1) * w->sample_spacing_x,
                     v->lo.y + (w->grid_d - 1) * w->sample_spacing_z, top);
        ray_tracing_sha256_update(hash, &w->grid_w, sizeof(w->grid_w));
        ray_tracing_sha256_update(hash, &w->grid_d, sizeof(w->grid_d));
        ray_tracing_sha256_update(hash, v->heights, f.sample_count * sizeof(float));
        ray_tracing_sha256_update(hash, f.water_body_boundary.medium_id,
                                  strlen(f.water_body_boundary.medium_id));
        v->frame = (int)f.frame_index;
        v->time = f.time_seconds;
        v->dt = f.dt_seconds;
        w->object_id = w->material_id = w->medium_id = NULL;
    }
    RuntimeWaterSurfaceFrame_Free(&f);
    return ok;
}
static bool load_vf(RuntimeSpectralVolume *v, RayTracingSha256Context *hash, char *diag, size_t size) {
    RuntimeVolumeAttachment3D *a = &v->vf;
    char selected[4096];
    if (!fluid_volume_import_3d_resolve_source_frame_path(v->config.path, RUNTIME_VOLUME_3D_SOURCE_NONE,
                                                          v->config.frame_index, selected, sizeof(selected),
                                                          diag, size) ||
        !ray_tracing_sha256_file(selected, v->frame_sha))
        return false;
    if (!fluid_volume_import_3d_load_source_at_frame(selected, RUNTIME_VOLUME_3D_SOURCE_NONE,
                                                     v->config.frame_index, a, diag, size))
        return false;
    char readback[65];
    if (!ray_tracing_sha256_file(selected, readback) || strcmp(readback, v->frame_sha)) return false;
    a->affectsLighting = true;
    if (!RuntimeVolume3D_HasSampleableDensity(a) || a->grid.frameIndex != (uint64_t)v->config.frame_index)
        return false;
    for (uint64_t i = 0; i < a->grid.cellCount; ++i)
        if (!(a->channels.density[i] >= 0) || !isfinite(a->channels.density[i])) return false;
    v->lo = a->grid.boundsMin;
    v->hi = a->grid.boundsMax;
    v->frame = (int)a->grid.frameIndex;
    v->time = a->grid.timeSeconds;
    v->dt = a->grid.dtSeconds;
    ray_tracing_sha256_update(hash, &a->grid.voxelSize, sizeof(double));
    ray_tracing_sha256_update(hash, &a->grid.cellCentered, sizeof(bool));
    ray_tracing_sha256_update(hash, a->channels.density, a->grid.cellCount * sizeof(float));
    if (a->channels.solidMask) ray_tracing_sha256_update(hash, a->channels.solidMask, a->grid.cellCount);
    return true;
}
RuntimeSpectralVolume *RuntimeSpectralVolume_Load(const RuntimeSpectralVolumeConfig *cfg, char *diag,
                                                  size_t size) {
    RuntimeSpectralVolume *v = calloc(1, sizeof(*v));
    if (!v) return NULL;
    v->config = *cfg;
    v->lo = cfg->lo;
    v->hi = cfg->hi;
    v->frame = -1;
    RayTracingSha256Context hash;
    ray_tracing_sha256_init(&hash);
    ray_tracing_sha256_update(&hash, cfg->profile_hash, 64);
    ray_tracing_sha256_update(&hash, cfg->path, strlen(cfg->path));
    if ((cfg->kind != SPECTRAL_VOLUME_BOX && !ray_tracing_sha256_file(cfg->path, v->source_sha)) ||
        (cfg->kind == SPECTRAL_VOLUME_WATER && !load_water(v, &hash, diag, size)) ||
        (cfg->kind == SPECTRAL_VOLUME_VF3D && !load_vf(v, &hash, diag, size))) {
        snprintf(diag, size, "spectral volume requires valid metric field data at exact frame %d",
                 cfg->frame_index);
        goto fail;
    }
    if (cfg->kind != SPECTRAL_VOLUME_BOX) {
        char readback[65];
        if (!ray_tracing_sha256_file(cfg->path, readback) || strcmp(readback, v->source_sha)) {
            snprintf(diag, size, "spectral simulation source changed during preparation");
            goto fail;
        }
    }
    double extent[] = {v->hi.x - v->lo.x, v->hi.y - v->lo.y, v->hi.z - v->lo.z};
    for (int i = 0; i < 3; ++i) {
        double n = ceil(extent[i] / cfg->cell_m);
        if (!(n >= 1 && n <= (cfg->reconstruction ? 4095 : 4096))) {
            snprintf(diag, size, "spectral volume reconstruction requires 1 to 4096 cells per axis (4095 for continuous nodes)");
            goto fail;
        }
        v->dims[i] = (int)n;
    }
    ray_tracing_sha256_update(&hash, v->source_sha, strlen(v->source_sha));
    ray_tracing_sha256_update(&hash, v->frame_sha, strlen(v->frame_sha));
    ray_tracing_sha256_update(&hash, &v->lo, sizeof(Vec3));
    ray_tracing_sha256_update(&hash, &v->hi, sizeof(Vec3));
    ray_tracing_sha256_update(&hash, &v->time, sizeof(double));
    ray_tracing_sha256_update(&hash, &v->dt, sizeof(double));
    ray_tracing_sha256_finish_hex(&hash, v->digest);
    return v;
fail:
    RuntimeSpectralVolume_Free(v);
    return NULL;
}
void RuntimeSpectralVolume_Free(RuntimeSpectralVolume *v) {
    if (!v) return;
    RuntimeVolumeAttachment3D_Free(&v->vf);
    free(v->heights);
    spectral_reconstruction_free(v->reconstruction);
    free(v->positions);
    free(v->points);
    free(v->heads);
    free(v);
}
const char *RuntimeSpectralVolume_Digest(const RuntimeSpectralVolume *v) { return v->digest; }
void RuntimeSpectralVolume_Identity(const RuntimeSpectralVolume *v, RuntimeSpectralMapStats *st) {
    memcpy(st->volume_digest, v->digest, sizeof(v->digest));
    memcpy(st->volume_source_sha, v->source_sha, 65);
    memcpy(st->volume_frame_sha, v->frame_sha, 65);
    st->volume_frame = v->frame;
    st->volume_time = v->time;
    st->volume_dt = v->dt;
}
static bool slab(double o, double d, double lo, double hi, double *enter, double *exit) {
    if (d == 0) return o >= lo && o < hi;
    double a = (lo - o) / d, b = (hi - o) / d;
    *enter = fmax(*enter, fmin(a, b));
    *exit = fmin(*exit, fmax(a, b));
    return *exit > *enter;
}
bool spectral_field_begin(SpectralFieldRay *it, const RuntimeSpectralVolume *v, Ray3D ray, double length,
                          bool cells) {
    *it = (SpectralFieldRay){.v = v, .ray = ray, .end = length, .cells = cells};
    return slab(ray.origin.x, ray.direction.x, v->lo.x, v->hi.x, &it->t, &it->end) &&
           slab(ray.origin.y, ray.direction.y, v->lo.y, v->hi.y, &it->t, &it->end) &&
           slab(ray.origin.z, ray.direction.z, v->lo.z, v->hi.z, &it->t, &it->end);
}
/* Distance to next integer plane. Snap coordinate roundoff at a just-crossed face. */
static double next_plane(double p, double d, double origin, double scale) {
    if (d == 0) return INFINITY;
    double q = (p - origin) / scale, nearby = round(q);
    if (fabs(q - nearby) < 32 * DBL_EPSILON * fmax(1, fabs(q) + (fabs(p) + fabs(origin)) / scale))
        q = nearby;
    double target = d > 0 ? floor(q) + 1 : ceil(q) - 1;
    return (target - q) * scale / d;
}
static double grid_step(Vec3 p, Vec3 d, Vec3 lo, double h) {
    return fmin(next_plane(p.x, d.x, lo.x, h),
                fmin(next_plane(p.y, d.y, lo.y, h), next_plane(p.z, d.z, lo.z, h)));
}
static double water_depth(const RuntimeSpectralVolume *v, Vec3 p) {
    const RuntimeWaterBodyMesh3DDesc *w = &v->water;
    double x = fmax(0, (p.x - v->lo.x) / w->sample_spacing_x),
           y = fmax(0, (p.y - v->lo.y) / w->sample_spacing_z);
    uint32_t i = (uint32_t)fmin(w->grid_w - 2, floor(x)), j = (uint32_t)fmin(w->grid_d - 2, floor(y));
    double u = x - i, t = y - j;
    const float *h = v->heights + (size_t)j * w->grid_w + i;
    double top = u >= t ? (1 - u) * h[0] + (u - t) * h[1] + t * h[w->grid_w + 1]
                        : (1 - t) * h[0] + (t - u) * h[w->grid_w] + u * h[w->grid_w + 1];
    return top - p.z;
}
static Vec3 at(Ray3D r, double t) { return vec3_add(r.origin, vec3_scale(r.direction, t)); }
bool spectral_field_next(SpectralFieldRay *it, double *a, double *b) {
    const RuntimeSpectralVolume *v = it->v;
    while (it->t < it->end) {
        double start = it->t, step = it->end - start;
        Vec3 p = at(it->ray, start), d = it->ray.direction;
        if (it->cells) step = fmin(step, grid_step(p, d, v->lo, v->config.cell_m));
        if (v->config.kind == SPECTRAL_VOLUME_VF3D) {
            double h = v->vf.grid.voxelSize, k = v->vf.grid.cellCentered ? .5 * h : 0;
            step = fmin(step, grid_step(p, d, vec3_add(v->lo, vec3(k, k, k)), h));
        } else if (v->config.kind == SPECTRAL_VOLUME_WATER) {
            double hx = v->water.sample_spacing_x, hy = v->water.sample_spacing_z;
            step = fmin(step, next_plane(p.x, d.x, v->lo.x, hx));
            step = fmin(step, next_plane(p.y, d.y, v->lo.y, hy));
            step = fmin(step,
                        next_plane((p.x - v->lo.x) / hx - (p.y - v->lo.y) / hy, d.x / hx - d.y / hy, 0, 1));
        }
        if (it->cells && v->reconstruction)
            step = fmin(step, spectral_reconstruction_boundary((RuntimeSpectralVolume *)v,it->ray,start,start+step)-start);
        *a = start;
        *b = it->t = fmin(it->end, fmax(nextafter(start, INFINITY), start + step));
        if (v->config.kind == SPECTRAL_VOLUME_WATER) {
            double da = water_depth(v, at(it->ray, *a)), db = water_depth(v, at(it->ray, *b));
            if (da <= 0 && db <= 0) continue;
            if (da < 0 || db < 0) {
                double cross = *a + (*b - *a) * da / (da - db);
                if (da < 0)
                    *a = cross;
                else
                    *b = cross;
            }
        }
        if (*b > *a) return true;
    }
    return false;
}
double spectral_field_mass(const SpectralFieldRay *it, double a, double b) {
    if (it->v->config.kind != SPECTRAL_VOLUME_VF3D) return b - a;
    /* A trilinear field along a line is cubic between consecutive knots. */
    double mid = a + (b - a) * .5, offset = (b - a) * .28867513459481287;
    return (b - a) * .5 *
           (RuntimeVolume3D_SampleDensityAtPosition(&it->v->vf, at(it->ray, mid - offset)) +
            RuntimeVolume3D_SampleDensityAtPosition(&it->v->vf, at(it->ray, mid + offset)));
}

/* Exact ordered segment reuse, privately owned by a camera region. No quantized
 * keys, wavelength-dependent values, global cache, or cross-frame lifetime. */
enum { CAMERA_SEGMENTS = 4096 };
struct RuntimeSpectralCameraCache {
  const RuntimeSpectralVolume *volume;
  Ray3D ray;
  double limit;
  bool cells, valid, replay, overflow;
  size_t count, cursor;
  unsigned long long hits, misses, reused, overflows;
  struct {
    double a, b, mass;
  } segments[CAMERA_SEGMENTS];
};
RuntimeSpectralCameraCache *RuntimeSpectralCameraCache_Create(void) {
  return calloc(1, sizeof(RuntimeSpectralCameraCache));
}
void RuntimeSpectralCameraCache_Free(RuntimeSpectralCameraCache *c,
                                     RuntimeSpectralMapStats *st) {
  if (!c)
    return;
  if (st) {
    st->camera_cache_hits += c->hits;
    st->camera_cache_misses += c->misses;
    st->camera_cache_segments += c->reused;
    st->camera_cache_overflows += c->overflows;
    st->camera_cache_bytes = sizeof(*c);
  }
  free(c);
}
static bool same_vec(Vec3 a, Vec3 b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}
bool spectral_camera_begin(RuntimeSpectralCameraCache *c, SpectralFieldRay *it,
                           const RuntimeSpectralVolume *v, Ray3D ray,
                           double limit, bool cells) {
  if (!c)
    return spectral_field_begin(it, v, ray, limit, cells);
  c->cursor = 0;
  c->replay = c->valid && c->volume == v && c->limit == limit &&
              c->cells == cells && same_vec(c->ray.origin, ray.origin) &&
              same_vec(c->ray.direction, ray.direction);
  if (c->replay) {
    ++c->hits;
    *it = (SpectralFieldRay){.v = v, .ray = ray, .cells = cells};
    return c->count != 0;
  }
  ++c->misses;
  c->volume = v;
  c->ray = ray;
  c->limit = limit;
  c->cells = cells;
  c->valid = false;
  c->overflow = false;
  c->count = 0;
  if (spectral_field_begin(it, v, ray, limit, cells))
    return true;
  c->valid = true;
  return false;
}
bool spectral_camera_next(RuntimeSpectralCameraCache *c, SpectralFieldRay *it,
                          double *a, double *b, double *mass) {
  if (c && c->replay) {
    if (c->cursor == c->count)
      return false;
    size_t i = c->cursor++;
    *a = c->segments[i].a;
    *b = c->segments[i].b;
    *mass = c->segments[i].mass;
    ++c->reused;
    return true;
  }
  if (!spectral_field_next(it, a, b)) {
    if (c)
      c->valid = !c->overflow;
    return false;
  }
  *mass = spectral_field_mass(it, *a, *b);
  if (c && !c->overflow) {
    if (c->count == CAMERA_SEGMENTS) {
      c->overflow = true;
      ++c->overflows;
    } else {
      size_t i = c->count++;
      c->segments[i].a = *a;
      c->segments[i].b = *b;
      c->segments[i].mass = *mass;
    }
  }
  return true;
}
