#include "render/runtime_spectral_surface_map.h"
#include "app/ray_tracing_sha256.h"
#include "render/runtime_scene_accel_3d_internal.h"
#include "render/runtime_caustic_photon_surface_kernel_3d.h"
#include "render/runtime_spectral_volume.h"
#include "render/runtime_spectral_observer.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "runtime_spectral_surface_map_internal.h"

static void wide_free(RuntimeSpectralSurfaceMap *);
size_t RuntimeSpectralSurfaceMap_RecordBytes(void) { return sizeof(Deposit); }

/* Hash evaluated transport geometry, including mesh winding and receiver coordinate frames.
 * Cache lifetime is one CLI process on one ABI. No serialized pointer or file identity. */
static void surface_map_key(const RuntimeSpectralPencil *p, const RuntimeScene3D *s,
                            bool include_volume, char key[65]) {
    RayTracingSha256Context h;
    ray_tracing_sha256_init(&h);
    const char version[] = "optic-spectral-surface-v1";
    ray_tracing_sha256_update(&h, version, sizeof(version));
    ray_tracing_sha256_update(&h, p->map.config_hash, 64);
    if (p->map.adaptive_scattered) ray_tracing_sha256_update(&h, "adaptive_scattered", sizeof("adaptive_scattered"));
    if (p->map.scattered_support_m>0) {
        ray_tracing_sha256_update(&h,&p->map.scattered_support_m,sizeof(double));
        ray_tracing_sha256_update(&h,&p->map.scattered_neighbors,sizeof(int));
    }
    if (include_volume && p->active_volume)
        ray_tracing_sha256_update(&h, RuntimeSpectralVolume_Digest(p->active_volume), 64);
    ray_tracing_sha256_update(&h, p->glass_id, strlen(p->glass_id) + 1);
    ray_tracing_sha256_update(&h, p->receiver_id, strlen(p->receiver_id) + 1);
    ray_tracing_sha256_update(&h, &p->constant_ior, sizeof(p->constant_ior));
    ray_tracing_sha256_update(&h, &p->use_n_bk7, sizeof(p->use_n_bk7));
    ray_tracing_sha256_update(&h, &p->receiver_radius_m, sizeof(p->receiver_radius_m));
    ray_tracing_sha256_update(&h, &p->exterior_ior, sizeof(p->exterior_ior));
    ray_tracing_sha256_update(&h, &p->glass_roughness, sizeof(p->glass_roughness));
    if (p->fresnel_camera_samples)
        ray_tracing_sha256_update(&h, "fresnel-sampled-v1", sizeof("fresnel-sampled-v1"));
    if (p->surface_path.enabled) {
        const char area_tag[] = "ordinary-rectangular-area-v1";
        ray_tracing_sha256_update(&h, area_tag, sizeof(area_tag));
        if (p->surface_path.caustic_guided)
            ray_tracing_sha256_update(&h,"guided_cone_90",sizeof("guided_cone_90"));
        const RuntimeSpectralSurfacePathConfig *a = &p->surface_path;
        Vec3 frame[] = {a->origin, a->axis_u, a->axis_v, a->normal};
        double size[] = {a->width, a->height};
        const RuntimeSpectralLamp *lamp = &p->map.sources[0];
        ray_tracing_sha256_update(&h, frame, sizeof(frame));
        ray_tracing_sha256_update(&h, size, sizeof(size));
        ray_tracing_sha256_update(&h, &p->map.samples, sizeof(p->map.samples));
        ray_tracing_sha256_update(&h, &p->map.seed, sizeof(p->map.seed));
        ray_tracing_sha256_update(&h, &p->map.bins, sizeof(p->map.bins));
        ray_tracing_sha256_update(&h, &p->map.absorption_per_m, sizeof(double));
        ray_tracing_sha256_update(&h, &lamp->nlines, sizeof(lamp->nlines));
        ray_tracing_sha256_update(&h, &lamp->nknots, sizeof(lamp->nknots));
        ray_tracing_sha256_update(&h, lamp->lines, (size_t)lamp->nlines * sizeof(lamp->lines[0]));
        ray_tracing_sha256_update(&h, lamp->knots, (size_t)lamp->nknots * sizeof(lamp->knots[0]));
    }
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

void RuntimeSpectralSurfaceMap_Key(const RuntimeSpectralPencil *p, const RuntimeScene3D *s, char key[65]) {
    surface_map_key(p, s, true, key);
}

void RuntimeSpectralSurfaceMap_LaunchKey(const RuntimeSpectralPencil *p, const RuntimeScene3D *s, char key[65]) {
    surface_map_key(p, s, false, key);
}

static uint64_t mix(uint64_t n) {
    n = (n ^ (n >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    n = (n ^ (n >> 27)) * UINT64_C(0x94d049bb133111eb);
    return n ^ (n >> 31);
}
size_t spectral_surface_bucket(const RuntimeSpectralSurfaceMap *m, int primitive, int band, int64_t u, int64_t v) {
    uint64_t group = ((uint64_t)(unsigned)primitive << 32) | (unsigned)band;
    return (mix(group + 1) ^ mix((uint64_t)u + 17) ^ mix((uint64_t)v + 43)) & m->mask;
}
double RuntimeSpectral_Uniform64(uint64_t n) { return (double)(mix(n) >> 11) * 0x1p-53; }
bool spectral_surface_cell(const RuntimePlane3D *plane, Vec3 pos, double radius, int64_t *u, int64_t *v) {
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
/* Uniform points on the rectangle and cosine-weighted directions on its
 * emitting hemisphere. Together these produce L * pi * area watts. */
static Ray3D area_launch(const RuntimeSpectralSurfacePathConfig *a, uint64_t id) {
    double x = RuntimeSpectral_Uniform64(id ^ UINT64_C(0x0f19b59c3d24a613));
    double y = RuntimeSpectral_Uniform64(id ^ UINT64_C(0x80ea91a72de712cf));
    double r = sqrt(RuntimeSpectral_Uniform64(id ^ UINT64_C(0x5bad0bb4c1f6ce79)));
    double phi = 2 * M_PI * RuntimeSpectral_Uniform64(id ^ UINT64_C(0xdd3e0bfa34ff112d));
    Vec3 position = vec3_add(a->origin, vec3_add(vec3_scale(a->axis_u, (x-.5)*a->width),
                                              vec3_scale(a->axis_v, (y-.5)*a->height)));
    Vec3 direction = vec3_add(vec3_scale(a->normal, sqrt(1-r*r)),
                         vec3_add(vec3_scale(a->axis_u, r*cos(phi)), vec3_scale(a->axis_v, r*sin(phi))));
    return RuntimeRay3D_Make(position, direction);
}
typedef struct {Vec3 center; double radius;} LaunchGuide;

static LaunchGuide launch_guide(const RuntimeScene3D *scene,const char *id) {
    LaunchGuide g={0};
    RuntimeSceneAcceleration3DInstanceBounds *bounds=NULL;
    int count=0;
    if (!RuntimeSceneAcceleration3D_CaptureInstanceBounds(scene,&bounds,&count,NULL)) return g;
    for (int i=0;i<count;++i) if (!strcmp(bounds[i].objectId,id)) {
        g.center=vec3_scale(vec3_add(bounds[i].min,bounds[i].max),.5);
        g.radius=.500001*vec3_length(vec3_sub(bounds[i].max,bounds[i].min));
        break;
    }
    RuntimeSceneAcceleration3D_FreeInstanceIdentityMaps(bounds,count);
    free(bounds);
    return g;
}

/* The cosine component keeps full support. The cone encloses the glass;
 * the physical cosine density divided by the complete mixture preserves flux. */
static Ray3D guided_launch(const RuntimeSpectralSurfacePathConfig *a, LaunchGuide g,
                           uint64_t id,double *weight) {
    Ray3D ray=area_launch(a,id);
    *weight=1;
    Vec3 delta=vec3_sub(g.center,ray.origin);
    double distance=vec3_length(delta);
    if (!(g.radius>0 && distance>g.radius)) return ray;
    Vec3 axis=vec3_scale(delta,1/distance);
    double s=g.radius/distance;
    double span=s*s/(1+sqrt(1-s*s));
    if (!(span>0)) return ray;
    const double mix=.90;
    if (RuntimeSpectral_Uniform64(id ^ UINT64_C(0xd6e8feb86659fd93))<mix) {
        double drop=span*RuntimeSpectral_Uniform64(id ^ UINT64_C(0x589965cc75374cc3));
        double c=1-drop,r=sqrt(drop*(2-drop));
        double phi=2*M_PI*RuntimeSpectral_Uniform64(id ^ UINT64_C(0xa0761d6478bd642f));
        Vec3 guide=fabs(axis.z)<.9?vec3(0,0,1):vec3(0,1,0);
        Vec3 u=vec3_normalize(vec3_cross(guide,axis)),v=vec3_cross(axis,u);
        ray.direction=vec3_add(vec3_scale(axis,c),
                       vec3_add(vec3_scale(u,r*cos(phi)),vec3_scale(v,r*sin(phi))));
    }
    double qcos=fmax(0,vec3_dot(ray.direction,a->normal))/M_PI;
    double qcone=vec3_dot(ray.direction,axis)>=1-span ? 1/(2*M_PI*span) : 0;
    double q=(1-mix)*qcos+mix*qcone;
    *weight=q>0?qcos/q:0;
    return ray;
}

static bool populate(RuntimeSpectralSurfaceMap *m, const RuntimeSpectralPencil *p,
                     const RuntimeScene3D *scene, int source, const RuntimeSpectralProfile *profile,
                     RuntimeSpectralMapStats *st) {
    const RuntimeSpectralMapConfig *cfg = &p->map;
    if (cfg->scattered_support_m!=0 &&
        (!cfg->adaptive_scattered || !isfinite(cfg->scattered_support_m) ||
         !(isfinite(p->receiver_radius_m) && p->receiver_radius_m>0) ||
         cfg->scattered_support_m<p->receiver_radius_m || cfg->scattered_support_m>64*p->receiver_radius_m ||
         (cfg->scattered_neighbors && (cfg->scattered_neighbors<4 || cfg->scattered_neighbors>64)))) return NULL;
    const RuntimeSpectralLamp *s = &cfg->sources[source];
    Vec3 up = fabs(s->direction.z) < .9 ? vec3(0, 0, 1) : vec3(0, 1, 0);
    Vec3 u = vec3_normalize(vec3_cross(up, s->direction)), v = vec3_cross(s->direction, u);
    /* Random shift of a uniform probability lattice gives at most one
     * sample of endpoint error per contiguous CDF interval. Disk variates
     * use disjoint dimensions and stay unchanged when spectra change. */
    double shift = RuntimeSpectral_Uniform64(((uint64_t)cfg->seed << 32) + 1);
    bool guided=p->surface_path.enabled && p->surface_path.caustic_guided;
    LaunchGuide guide=guided?launch_guide(scene,p->glass_id):(LaunchGuide){0};
    if (guided && !(guide.radius>0)) return false;
    for (int j = 0; j < cfg->samples; ++j) {
        uint64_t id = ((uint64_t)cfg->seed << 32) + (unsigned)j * 3u + 1;
        /* Area-map budgets share a deterministic launch prefix. The earlier
         * collimated instrument keeps its stratified wavelengths unchanged. */
        double q = p->surface_path.enabled ?
            RuntimeSpectral_Uniform64(id ^ UINT64_C(0xa28fc35db672f421)) :
            fmin(nextafter(1.0, 0.0), (j + shift) / cfg->samples);
        RuntimeSpectralDraw draw;
        double watts;
        if (!RuntimeSpectral_Sample(profile, q, &draw) ||
            !RuntimeSpectral_EmissionWatts(profile, draw, 1, cfg->samples, &watts))
            return false;
        double r = sqrt(RuntimeSpectral_Uniform64(id + 1)) * s->aperture_m,
               theta = 2 * M_PI * RuntimeSpectral_Uniform64(id + 2);
        Vec3 pos =
            vec3_add(s->origin, vec3_add(vec3_scale(u, r * cos(theta)), vec3_scale(v, r * sin(theta))));
        int b = band_for(m, st, draw.nm, draw.measure);
        if (b < 0) return false;
        RuntimeSpectralPencilLine line = {draw.nm, watts,
                                          RuntimeSpectralSurfaceMap_Absorption(cfg, draw.nm), id, b, NULL};
        Vec3 direction = s->direction;
        if (s->angular_radius_rad > 0)
            direction =
                disk_direction(s, RuntimeSpectral_Uniform64(id ^ UINT64_C(0x4a17589eda160000)),
                               2 * M_PI * RuntimeSpectral_Uniform64(id ^ UINT64_C(0xce0bd07533c30000)));
        if (p->fresnel_camera_samples)
            line.sample_id ^= (uint64_t)(source + 1) * UINT64_C(0xa48d43df8523ccad);
        RuntimeSpectralPathReadout hit={0};
        Ray3D ray;
        if (guided) {
            double correction;
            ray=guided_launch(&p->surface_path,guide,id,&correction);
            line.watts=watts*=correction;
        } else {
            ray=p->surface_path.enabled ? area_launch(&p->surface_path,id) :
                                           RuntimeRay3D_Make(pos,direction);
        }
        if (watts>0 && !RuntimeSpectralPencil_Trace(scene,p,&line,RuntimeSpectralPencil_Index(p,draw.nm),
                                                    ray,false,&hit)) return false;
        ++st->launched;
        if (p->active_volume || p->fresnel_camera_samples) {
            RuntimeSpectralBand *r = &st->bands[b];
            r->launched_w += watts;
            r->optical_loss_w += watts - hit.weight - hit.glass_absorbed_w -
                hit.medium_absorbed_w - hit.medium_scattered_w;
            r->medium_absorbed_w += hit.medium_absorbed_w;
            r->medium_scattered_w += hit.medium_scattered_w;
            r->glass_absorbed_w += hit.glass_absorbed_w;
            r->fresnel_reflections += hit.fresnel_reflections;
            r->total_internal_reflections += hit.total_internal_reflections;
            if (hit.volume_absorbed)
                r->absorbed_w += hit.weight;
            else if (hit.depth_limited)
                r->limited_w += hit.weight;
            else if (hit.receiver)
                r->receiver_w += hit.weight;
            else
                r->escaped_w += hit.weight;
        }
        if (hit.depth_limited) {
            ++st->depth_limited;
            continue;
        }
        if (!hit.receiver) {
            ++st->missed;
            continue;
        }
        if (!hit.interfaces && !hit.scatters) {
            ++st->direct_arrivals;
            continue;
        }

        Deposit *d = &m->records[m->count];
        *d = (Deposit){.position = hit.position,
                       .incident = hit.direction,
                       .vacuum_nm = draw.nm,
                       .watts = hit.weight,
                       .primitive = hit.primitive,
                       .band = b,
                       .source = source,
                       .front = hit.receiver_front, .scattered = hit.scatters > 0, .scale = 1};
        if (!spectral_surface_cell(&scene->primitives[hit.primitive].shape.plane, hit.position, p->receiver_radius_m, &d->u,
                  &d->v))
            return false;
        size_t k = spectral_surface_bucket(m, d->primitive, b, d->u, d->v);
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
        wide_free(m);
        free(m->records);
        free(m->heads);
        free(m);
    }
}

/* Sample-point bandwidth: each scattered record owns one normalized kernel. */
static bool clear_of_other_surfaces(const Deposit *deposit, const RuntimeScene3D *scene,
                                    const RuntimeSceneAcceleration3DInstanceBounds *bounds,
                                    int count, double reach) {
    Vec3 x = deposit->position;
    for (int i = 0; i < scene->primitiveCount; ++i) {
        if (i == deposit->primitive || scene->primitives[i].kind != RUNTIME_PRIMITIVE_3D_KIND_PLANE)
            continue;
        const RuntimePlane3D *plane = &scene->primitives[i].shape.plane;
        if (fabs(vec3_dot(vec3_sub(x, plane->origin), plane->normal)) <= reach)
            return false;
    }
    for (int i = 0; i < count; ++i) {
        if (bounds[i].primitiveIndex == deposit->primitive) continue;
        Vec3 lo = bounds[i].min, hi = bounds[i].max;
        if (x.x >= lo.x - reach && x.x <= hi.x + reach &&
            x.y >= lo.y - reach && x.y <= hi.y + reach &&
            x.z >= lo.z - reach && x.z <= hi.z + reach) return false;
    }
    return true;
}

#include "runtime_spectral_surface_gather.h"

static int assign_support(RuntimeSpectralSurfaceMap *m, const RuntimeScene3D *scene, double radius) {
    RuntimeSceneAcceleration3DInstanceBounds *bounds = NULL;
    int count = 0, expanded = 0;
    if (!RuntimeSceneAcceleration3D_CaptureInstanceBounds(scene, &bounds, &count, NULL)) return 0;
    for (int i = 0; i < scene->primitiveCount; ++i) {
        if (scene->primitives[i].kind == RUNTIME_PRIMITIVE_3D_KIND_PLANE) continue;
        int j = 0;
        while (j < count && bounds[j].primitiveIndex != i) ++j;
        if (j == count) goto done; /* Missing bounds retain the fixed kernel for this map. */
    }
    for (int i = 0; i < m->count; i++) {
        Deposit *d = &m->records[i];
        d->scale = 1;
        if (!d->scattered || !clear_of_other_surfaces(d, scene, bounds, count, 2 * radius))
            continue;
        const RuntimePlane3D *plane = &scene->primitives[d->primitive].shape.plane;
        Vec3 loc = vec3_sub(d->position, plane->origin);
        if (plane->width / 2 - fabs(vec3_dot(loc, plane->axisU)) < 2 * radius ||
            plane->height / 2 - fabs(vec3_dot(loc, plane->axisV)) < 2 * radius)
            continue;
        int n = 0;
        for (int v = -1; v <= 1 && n < 16; v++)
            for (int u = -1; u <= 1 && n < 16; u++) {
                size_t k = spectral_surface_bucket(m, d->primitive, d->band, d->u + u, d->v + v);
                for (int j = m->heads[k]; j >= 0 && n < 16; j = m->records[j].next) {
                    const Deposit *q = &m->records[j];
                    if (q->u != d->u + u || q->v != d->v + v || q->primitive != d->primitive ||
                        q->band != d->band || q->front != d->front)
                        continue;
                    Vec3 delta = vec3_sub(d->position, q->position);
                    if (vec3_dot(delta, delta) < radius * radius)
                        n++;
                }
            }
        d->scale = (float)fmin(2, sqrt(16.0 / fmax(1, n)));
        expanded += d->scale > 1;
    }
done:
    RuntimeSceneAcceleration3D_FreeInstanceIdentityMaps(bounds, count);
    free(bounds);
    return expanded;
}
RuntimeSpectralSurfaceMap *RuntimeSpectralSurfaceMap_Build(const RuntimeSpectralPencil *p,
                                                           const RuntimeScene3D *scene,
                                                           RuntimeSpectralMapStats *st) {
    const RuntimeSpectralMapConfig *cfg = &p->map;
    if (cfg->scattered_support_m!=0 &&
        (!cfg->adaptive_scattered || !isfinite(cfg->scattered_support_m) ||
         !(isfinite(p->receiver_radius_m) && p->receiver_radius_m>0) ||
         cfg->scattered_support_m<p->receiver_radius_m || cfg->scattered_support_m>64*p->receiver_radius_m ||
         (cfg->scattered_neighbors && (cfg->scattered_neighbors<4 || cfg->scattered_neighbors>64)))) return NULL;
    RuntimeSpectralSurfaceMap *m = calloc(1, sizeof(*m));
    if (!m)
        return NULL;
    st->adaptive_scattered = cfg->adaptive_scattered;
    bands(m, cfg, st);
    for (int b = 0; b < st->band_count; ++b)
        m->band_nm[b] = st->bands[b].nm;
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
    if (!RuntimeSpectralVolume_PrepareReconstruction(p->active_volume)) goto fail;
    if (p->active_volume) RuntimeSpectralVolume_ScatterStats(p->active_volume, st);
    if (p->map.adaptive_scattered) {
        if(cfg->scattered_support_m>0) {
            if(!spectral_surface_wide_prepare(m,p,scene,&st->expanded_deposits))goto fail;
        } else st->expanded_deposits = assign_support(m, scene, p->receiver_radius_m);
        m->has_expanded = st->expanded_deposits > 0;
    }
    st->fresnel_transport = p->fresnel_camera_samples > 0;
    st->deposits = m->count;
    return m;
fail:
    RuntimeSpectralSurfaceMap_Free(m);
    return NULL;
}

static double direct_direction(const RuntimeSpectralVolume *volume, double nm, Vec3 direction,
                               const RuntimeSpectralLamp *s, double watts, const RuntimeScene3D *scene,
                               const RuntimeSpectralPathReadout *hit, const RuntimePlane3D *plane) {
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
    if (volume) watts *= RuntimeSpectralVolume_Transmission(volume, ray, t, nm);
    return watts * fabs(cosine) / projected / (M_PI * M_PI * s->aperture_m * s->aperture_m);
}
double RuntimeSpectralSurfaceMap_Gather(const RuntimeSpectralSurfaceMap *m,
                                          const RuntimeSpectralPencil *p, const RuntimeScene3D *scene,
                                          const RuntimeSpectralPathReadout *hit, int band,
                                          unsigned long long *candidates, double *direct,
                                          RuntimeSpectralDepositProbe *probe, RuntimeSpectralDepositColor *color) {
    const RuntimePlane3D *plane = &scene->primitives[hit->primitive].shape.plane;
    double L = 0, r = p->receiver_radius_m;
    /* Ordinary area direct illumination belongs solely to camera NEE/MIS. */
    for (int i = 0; !p->surface_path.enabled && i < p->map.source_count; ++i) {
        const RuntimeSpectralLamp *s = &p->map.sources[i];
        double watts = m->direct_watts[i][band];
        if (s->angular_radius_rad == 0)
            L += direct_direction(p->active_volume, m->band_nm[band], s->direction, s, watts, scene, hit,
                                  plane);
        else if (watts > 0) {
            /* Equal projected-area quadrature across the disk, including penumbra. */
            for (int j = 0; j < 64; ++j) {
                Vec3 direction = disk_direction(s, (j + .5) / 64, j * 2.399963229728653);
                L += direct_direction(p->active_volume, m->band_nm[band], direction, s, watts / 64, scene,
                                      hit, plane);
            }
        }
    }
    if (direct) *direct = L;
    int64_t u, v;
    if (!m->count || !spectral_surface_cell(plane, hit->position, r, &u, &v)) return L;
    int reach = m->has_expanded && !m->wide ? 2 : 1;
    for (int dv = -reach; dv <= reach; ++dv)
        for (int du = -reach; du <= reach; ++du) {
            size_t key = spectral_surface_bucket(m, hit->primitive, band, u + du, v + dv);
            for (int i = m->heads[key]; i >= 0; i = m->records[i].next) {
                const Deposit *d = &m->records[i];
                if ((m->wide && d->scattered) || d->primitive != hit->primitive || d->front != hit->receiver_front || d->band != band ||
                    d->u != u + du || d->v != v + dv)
                    continue;
                ++*candidates;
                double distance = vec3_length(vec3_sub(d->position, hit->position));
                double support = d->scale * r;
                double kernel = RuntimeCausticPhotonSurfaceKernel3D_Weight(distance, support);
                double value = d->watts * kernel / M_PI;
                L += value;
                probe_deposit(probe,d,i,support,kernel,value);
                deposit_color(color,d,value);
            }
        }
    if(m->wide)L+=wide_radiance(m,p,scene,hit,band,candidates,probe,color);
    return L;
}
double RuntimeSpectralSurfaceMap_ProbeRadiance(const RuntimeSpectralSurfaceMap *m,
    const RuntimeSpectralPencil *p, const RuntimeScene3D *scene, const RuntimeSpectralPathReadout *hit,
    int band, unsigned long long *candidates, double *direct, RuntimeSpectralDepositProbe *deposit) {
    return RuntimeSpectralSurfaceMap_Gather(m,p,scene,hit,band,candidates,direct,deposit,NULL);
}
double RuntimeSpectralSurfaceMap_Radiance(const RuntimeSpectralSurfaceMap *m,
    const RuntimeSpectralPencil *p, const RuntimeScene3D *scene, const RuntimeSpectralPathReadout *hit,
    int band, unsigned long long *candidates) {
    return RuntimeSpectralSurfaceMap_ProbeRadiance(m,p,scene,hit,band,candidates,NULL,NULL);
}

/* Use the same spectral normalization for both ordinary map consumers.
 * A line atom and continuum bin at the same wavelength remain distinct. */
static int area_band(const RuntimeSpectralSurfaceMap *m, const RuntimeSpectralPencil *p,
                     double nm, RuntimeSpectralMeasure measure) {
    if (measure == SPECTRAL_CONTINUUM && m->continuum_bins > 0)
        return (int)fmin(m->continuum_bins - 1, floor((nm - 380) * m->continuum_bins / 400));
    if (measure == SPECTRAL_LINE) {
        for (int i = 0; i < p->map.sources[0].nlines; ++i)
            if (p->map.sources[0].lines[i].nm == nm) return m->continuum_bins + i;
    }
    return -1;
}

static double area_band_radiance(const RuntimeSpectralSurfaceMap *m, const RuntimeSpectralPencil *p, int band) {
    if (band < 0 || !(m->direct_watts[0][band] > 0)) return 0;
    double area = p->surface_path.width * p->surface_path.height;
    return m->direct_watts[0][band] / (M_PI * area);
}

double RuntimeSpectralSurfaceMap_AreaResponse(const RuntimeSpectralSurfaceMap *m,
    const RuntimeSpectralPencil *p, const RuntimeScene3D *scene, const RuntimeSpectralPathReadout *hit,
    double nm, RuntimeSpectralMeasure measure, unsigned long long *candidates) {
    int band = area_band(m, p, nm, measure);
    double radiance = area_band_radiance(m, p, band);
    return radiance > 0 ? RuntimeSpectralSurfaceMap_Radiance(m,p,scene,hit,band,candidates) / radiance : 0;
}

bool RuntimeSpectralSurfaceMap_AreaVolumeResponse(const RuntimeSpectralSurfaceMap *m,
    const RuntimeSpectralPencil *p, Ray3D ray, double limit, double nm, RuntimeSpectralMeasure measure,
    double *response) {
    *response = 0;
    int band = area_band(m, p, nm, measure);
    double radiance = area_band_radiance(m, p, band);
    if (!(radiance > 0)) return true;
    RuntimeSpectralPencilLine line = {.vacuum_nm=nm, .band=band};
    RuntimeSpectralPathReadout result = {.weight=1};
    bool restart;
    if (!RuntimeSpectralVolume_Segment(p->active_volume, &line, &ray, limit, true, 0, &result, &restart))
        return false;
    *response = result.inscattered / radiance;
    return true;
}
