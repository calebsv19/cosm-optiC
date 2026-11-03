#include "render/runtime_spectral_surface_map.h"
#include "app/ray_tracing_sha256.h"
#include "render/runtime_caustic_photon_surface_kernel_3d.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Immutable after population. Links are indices so replays sum in a fixed order. */
typedef struct {
    Vec3 position, incident;
    double vacuum_nm, watts;
    int64_t u, v;
    int primitive, band, source, next;
    bool front;
} Deposit;

struct RuntimeSpectralSurfaceMap {
    Deposit *records;
    int *heads;
    size_t mask;
    int count, continuum_bins;
    double direct_watts[SPECTRAL_MAP_LIGHTS][SPECTRAL_MAP_MAX_BANDS];
};

size_t RuntimeSpectralSurfaceMap_RecordBytes(void) { return sizeof(Deposit); }

/* Hash evaluated transport geometry, including mesh winding and receiver coordinate frames.
 * Cache lifetime is one CLI process on one ABI. No serialized pointer or file identity. */
void RuntimeSpectralSurfaceMap_Key(const RuntimeSpectralPencil *p, const RuntimeScene3D *s, char key[65]) {
    RayTracingSha256Context h;
    ray_tracing_sha256_init(&h);
    const char version[] = "optic-spectral-surface-v1";
    ray_tracing_sha256_update(&h, version, sizeof(version));
    ray_tracing_sha256_update(&h, p->map.config_hash, 64);
    ray_tracing_sha256_update(&h, p->glass_id, strlen(p->glass_id) + 1);
    ray_tracing_sha256_update(&h, p->receiver_id, strlen(p->receiver_id) + 1);
    ray_tracing_sha256_update(&h, &p->constant_ior, sizeof(p->constant_ior));
    ray_tracing_sha256_update(&h, &p->use_n_bk7, sizeof(p->use_n_bk7));
    ray_tracing_sha256_update(&h, &p->receiver_radius_m, sizeof(p->receiver_radius_m));
    ray_tracing_sha256_update(&h, &p->exterior_ior, sizeof(p->exterior_ior));
    ray_tracing_sha256_update(&h, &p->glass_roughness, sizeof(p->glass_roughness));
    for (int i = 0; i < s->primitiveCount; ++i) {
        const RuntimePrimitive3D *prim = &s->primitives[i];
        ray_tracing_sha256_update(&h, prim->source.objectId, strlen(prim->source.objectId) + 1);
        ray_tracing_sha256_update(&h, &prim->kind, sizeof(prim->kind));
        if (prim->kind == RUNTIME_PRIMITIVE_3D_KIND_PLANE)
            ray_tracing_sha256_update(&h, &prim->shape.plane, sizeof(prim->shape.plane));
    }
    for (int i = 0; i < s->triangleMesh.triangleCount; ++i) {
        const RuntimeTriangle3D *t = &s->triangleMesh.triangles[i];
        Vec3 shape[] = {t->p0, t->p1, t->p2, t->normal};
        ray_tracing_sha256_update(&h, shape, sizeof(shape));
        ray_tracing_sha256_update(&h, &t->primitiveIndex, sizeof(t->primitiveIndex));
        ray_tracing_sha256_update(&h, &t->twoSided, sizeof(t->twoSided));
    }
    ray_tracing_sha256_finish_hex(&h, key);
}

static uint64_t mix(uint64_t n) {
    n = (n ^ (n >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    n = (n ^ (n >> 27)) * UINT64_C(0x94d049bb133111eb);
    return n ^ (n >> 31);
}
static size_t bucket(const RuntimeSpectralSurfaceMap *m, int primitive, int band, int64_t u, int64_t v) {
    uint64_t group = ((uint64_t)(unsigned)primitive << 32) | (unsigned)band;
    return (mix(group + 1) ^ mix((uint64_t)u + 17) ^ mix((uint64_t)v + 43)) & m->mask;
}
double RuntimeSpectral_Uniform64(uint64_t n) { return (double)(mix(n) >> 11) * 0x1p-53; }
static bool cell(const RuntimePlane3D *plane, Vec3 pos, double radius, int64_t *u, int64_t *v) {
    Vec3 local = vec3_sub(pos, plane->origin);
    double x = floor(vec3_dot(local, plane->axisU) / radius),
           y = floor(vec3_dot(local, plane->axisV) / radius);
    if (!(fabs(x) < 0x1p62 && fabs(y) < 0x1p62)) return false;
    *u = (int64_t)x;
    *v = (int64_t)y;
    return true;
}
static int band_for(const RuntimeSpectralSurfaceMap *m, const RuntimeSpectralMapStats *st, double nm,
                    RuntimeSpectralMeasure measure) {
    if (measure == SPECTRAL_CONTINUUM)
        return (int)fmin(m->continuum_bins - 1, floor((nm - 380) * m->continuum_bins / 400));
    for (int i = m->continuum_bins; i < st->band_count; ++i)
        if (st->bands[i].nm == nm) return i;
    return -1;
}
static void bands(RuntimeSpectralSurfaceMap *map, const RuntimeSpectralMapConfig *cfg,
                  RuntimeSpectralMapStats *st) {
    for (int i = 0; i < cfg->source_count; ++i)
        if (cfg->sources[i].nknots) map->continuum_bins = cfg->bins;
    st->band_count = map->continuum_bins;
    for (int b = 0; b < map->continuum_bins; ++b) {
        double w = 400.0 / cfg->bins;
        st->bands[b] = (RuntimeSpectralBand){.nm = 380 + (b + .5) * w, .width_nm = w};
    }
    for (int i = 0; i < cfg->source_count; ++i) {
        const RuntimeSpectralLamp *src = &cfg->sources[i];
        for (int j = 0; j < src->nlines; ++j) {
            double nm = src->lines[j].nm;
            int b = band_for(map, st, nm, SPECTRAL_LINE);
            if (b < 0) {
                b = st->band_count++;
                st->bands[b].nm = nm;
            }
            map->direct_watts[i][b] = src->lines[j].watts;
        }
        for (int b = 0; b < map->continuum_bins; ++b) {
            double lo = 380 + 400.0 * b / cfg->bins, hi = 380 + 400.0 * (b + 1) / cfg->bins;
            for (int j = 1; j < src->nknots; ++j) {
                RuntimeSpectralKnot a = src->knots[j - 1], z = src->knots[j];
                double x = fmax(a.nm, lo), y = fmin(z.nm, hi);
                if (y > x) {
                    double t = ((x + y) * .5 - a.nm) / (z.nm - a.nm);
                    map->direct_watts[i][b] += (y - x) * ((1 - t) * a.watts_per_nm + t * z.watts_per_nm);
                }
            }
        }
    }
}
double RuntimeSpectralSurfaceMap_Absorption(const RuntimeSpectralMapConfig *cfg, double nm) {
    for (int i = 1; i < cfg->absorption_knots; ++i) {
        const double *lo = cfg->absorption_curve[i - 1], *hi = cfg->absorption_curve[i];
        if (nm <= hi[0]) {
            double t = (nm - lo[0]) / (hi[0] - lo[0]);
            return (1 - t) * lo[1] + t * hi[1];
        }
    }
    return cfg->absorption_per_m;
}
/* Constant radiance over a cone and a planar emitting aperture gives the
 * flux distribution cos(theta) dOmega. Sample uniformly on its projected disk. */
static Vec3 disk_direction(const RuntimeSpectralLamp *s, double area, double azimuth) {
    double sin_theta = sin(s->angular_radius_rad) * sqrt(area);
    Vec3 guide = fabs(s->direction.z) < .9 ? vec3(0, 0, 1) : vec3(0, 1, 0);
    Vec3 u = vec3_normalize(vec3_cross(guide, s->direction)), v = vec3_cross(s->direction, u);
    Vec3 radial = vec3_add(vec3_scale(u, cos(azimuth)), vec3_scale(v, sin(azimuth)));
    return vec3_add(vec3_scale(s->direction, sqrt(1 - sin_theta * sin_theta)),
                    vec3_scale(radial, sin_theta));
}
static bool populate(RuntimeSpectralSurfaceMap *m, const RuntimeSpectralPencil *p,
                     const RuntimeScene3D *scene, int source, const RuntimeSpectralProfile *profile,
                     RuntimeSpectralMapStats *st) {
    const RuntimeSpectralMapConfig *cfg = &p->map;
    const RuntimeSpectralLamp *s = &cfg->sources[source];
    Vec3 up = fabs(s->direction.z) < .9 ? vec3(0, 0, 1) : vec3(0, 1, 0);
    Vec3 u = vec3_normalize(vec3_cross(up, s->direction)), v = vec3_cross(s->direction, u);
    /* Random shift of a uniform probability lattice gives at most one
     * sample of endpoint error per contiguous CDF interval. Disk variates
     * use disjoint dimensions and stay unchanged when spectra change. */
    double shift = RuntimeSpectral_Uniform64(((uint64_t)cfg->seed << 32) + 1);
    for (int j = 0; j < cfg->samples; ++j) {
        uint64_t id = ((uint64_t)cfg->seed << 32) + (unsigned)j * 3u + 1;
        double q = fmin(nextafter(1.0, 0.0), (j + shift) / cfg->samples);
        RuntimeSpectralDraw draw;
        double watts;
        if (!RuntimeSpectral_Sample(profile, q, &draw) ||
            !RuntimeSpectral_EmissionWatts(profile, draw, 1, cfg->samples, &watts))
            return false;
        double r = sqrt(RuntimeSpectral_Uniform64(id + 1)) * s->aperture_m,
               theta = 2 * M_PI * RuntimeSpectral_Uniform64(id + 2);
        Vec3 pos =
            vec3_add(s->origin, vec3_add(vec3_scale(u, r * cos(theta)), vec3_scale(v, r * sin(theta))));
        RuntimeSpectralPencilLine line = {draw.nm, watts,
                                          RuntimeSpectralSurfaceMap_Absorption(cfg, draw.nm), id};
        Vec3 direction = s->direction;
        if (s->angular_radius_rad > 0)
            direction =
                disk_direction(s, RuntimeSpectral_Uniform64(id ^ UINT64_C(0x4a17589eda160000)),
                               2 * M_PI * RuntimeSpectral_Uniform64(id ^ UINT64_C(0xce0bd07533c30000)));
        RuntimeSpectralPathReadout hit;
        if (!RuntimeSpectralPencil_Trace(scene, p, &line, RuntimeSpectralPencil_Index(p, draw.nm),
                                         RuntimeRay3D_Make(pos, direction), false, &hit))
            return false;
        ++st->launched;
        if (hit.depth_limited) {
            ++st->depth_limited;
            continue;
        }
        if (!hit.receiver) {
            ++st->missed;
            continue;
        }
        if (!hit.interfaces) {
            ++st->direct_arrivals;
            continue;
        }
        int b = band_for(m, st, draw.nm, draw.measure);
        if (b < 0) return false;
        Deposit *d = &m->records[m->count];
        *d = (Deposit){.position = hit.position,
                       .incident = hit.direction,
                       .vacuum_nm = draw.nm,
                       .watts = hit.weight,
                       .primitive = hit.primitive,
                       .band = b,
                       .source = source,
                       .front = hit.receiver_front};
        if (!cell(&scene->primitives[hit.primitive].shape.plane, hit.position, p->receiver_radius_m, &d->u,
                  &d->v))
            return false;
        size_t k = bucket(m, d->primitive, b, d->u, d->v);
        d->next = m->heads[k];
        m->heads[k] = m->count++;
        RuntimeSpectralBand *row = &st->bands[b];
        row->watts += d->watts;
        row->moment = vec3_add(row->moment, vec3_scale(d->position, d->watts));
        ++row->count;
    }
    return true;
}
void RuntimeSpectralSurfaceMap_Free(RuntimeSpectralSurfaceMap *m) {
    if (m) {
        free(m->records);
        free(m->heads);
        free(m);
    }
}
RuntimeSpectralSurfaceMap *RuntimeSpectralSurfaceMap_Build(const RuntimeSpectralPencil *p,
                                                           const RuntimeScene3D *scene,
                                                           RuntimeSpectralMapStats *st) {
    const RuntimeSpectralMapConfig *cfg = &p->map;
    RuntimeSpectralSurfaceMap *m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    bands(m, cfg, st);
    if (!cfg->enabled) return m;
    size_t n = (size_t)cfg->source_count * cfg->samples, capacity = 1;
    while (capacity < n)
        capacity *= 2;
    m->mask = capacity - 1;
    m->heads = malloc(capacity * sizeof(int));
    m->records = malloc(n * sizeof(Deposit));
    if (!m->heads || !m->records) goto fail;
    for (size_t i = 0; i < capacity; ++i)
        m->heads[i] = -1;
    for (int i = 0; i < cfg->source_count; ++i) {
        const RuntimeSpectralLamp *s = &cfg->sources[i];
        RuntimeSpectralProfile *profile = RuntimeSpectral_Prepare(s->knots, s->nknots, s->lines, s->nlines);
        bool ok =
            profile && (RuntimeSpectral_TotalWatts(profile) == 0 || populate(m, p, scene, i, profile, st));
        RuntimeSpectral_Free(profile);
        if (!ok) goto fail;
    }
    st->deposits = m->count;
    return m;
fail:
    RuntimeSpectralSurfaceMap_Free(m);
    return NULL;
}

static double direct_direction(Vec3 direction, const RuntimeSpectralLamp *s, double watts,
                               const RuntimeScene3D *scene, const RuntimeSpectralPathReadout *hit,
                               const RuntimePlane3D *plane) {
    if (!watts) return 0;
    double cosine = vec3_dot(direction, plane->normal);
    double projected = vec3_dot(s->direction, direction);
    if ((cosine < 0) != hit->receiver_front) return 0;
    Vec3 delta = vec3_sub(hit->position, s->origin);
    double t = vec3_dot(delta, s->direction) / projected;
    if (t <= 0) return 0;
    Vec3 offset = vec3_sub(delta, vec3_scale(direction, t));
    if (vec3_dot(offset, offset) > s->aperture_m * s->aperture_m) return 0;
    Ray3D ray = RuntimeRay3D_Make(vec3_add(s->origin, offset), direction);
    HitInfo3D first;
    if (!RuntimeRay3D_TraceSceneFirstHit(scene, &ray, 1e-8, RUNTIME_RAY_3D_UNBOUNDED_SCENE_DISTANCE,
                                         &first) ||
        first.primitiveIndex != hit->primitive)
        return 0;
    return watts * fabs(cosine) / projected / (M_PI * M_PI * s->aperture_m * s->aperture_m);
}
double RuntimeSpectralSurfaceMap_Radiance(const RuntimeSpectralSurfaceMap *m,
                                          const RuntimeSpectralPencil *p, const RuntimeScene3D *scene,
                                          const RuntimeSpectralPathReadout *hit, int band,
                                          unsigned long long *candidates) {
    const RuntimePlane3D *plane = &scene->primitives[hit->primitive].shape.plane;
    double L = 0, r = p->receiver_radius_m;
    for (int i = 0; i < p->map.source_count; ++i) {
        const RuntimeSpectralLamp *s = &p->map.sources[i];
        double watts = m->direct_watts[i][band];
        if (s->angular_radius_rad == 0)
            L += direct_direction(s->direction, s, watts, scene, hit, plane);
        else if (watts > 0) {
            /* Equal projected-area quadrature across the disk, including penumbra. */
            for (int j = 0; j < 64; ++j) {
                Vec3 direction = disk_direction(s, (j + .5) / 64, j * 2.399963229728653);
                L += direct_direction(direction, s, watts / 64, scene, hit, plane);
            }
        }
    }
    int64_t u, v;
    if (!m->count || !cell(plane, hit->position, r, &u, &v)) return L;
    for (int dv = -1; dv <= 1; ++dv)
        for (int du = -1; du <= 1; ++du) {
            size_t key = bucket(m, hit->primitive, band, u + du, v + dv);
            for (int i = m->heads[key]; i >= 0; i = m->records[i].next) {
                const Deposit *d = &m->records[i];
                if (d->primitive != hit->primitive || d->front != hit->receiver_front || d->band != band ||
                    d->u != u + du || d->v != v + dv)
                    continue;
                ++*candidates;
                double distance = vec3_length(vec3_sub(d->position, hit->position));
                L += d->watts * RuntimeCausticPhotonSurfaceKernel3D_Weight(distance, r) / M_PI;
            }
        }
    return L;
}
