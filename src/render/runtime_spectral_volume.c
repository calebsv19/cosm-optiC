#include "runtime_spectral_volume_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* The density of real scattering events already contains sigma_s. Each event
 * contributes incoming flux times phase per reconstruction volume. */
enum { BUCKETS = 65536, SPECTRAL_MAX_COLLISIONS = 1048576 };
static uint64_t cell_at(const RuntimeSpectralVolume *v, Vec3 p, double *volume) {
    double offsets[] = {p.x - v->lo.x, p.y - v->lo.y, p.z - v->lo.z};
    double extents[] = {v->hi.x - v->lo.x, v->hi.y - v->lo.y, v->hi.z - v->lo.z};
    uint64_t key = 0;
    *volume = 1;
    for (int i = 0; i < 3; ++i) {
        double h = v->config.cell_m;
        int k = (int)fmax(0, fmin(v->dims[i] - 1, floor(offsets[i] / h)));
        key = (key << 12) | (unsigned)k;
        *volume *= fmin(h, extents[i] - k * h);
    }
    return key;
}
unsigned spectral_volume_bucket(uint64_t cell, int band) {
    uint64_t k = cell ^ ((uint64_t)band << 40);
    k = (k ^ (k >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    return (unsigned)((k ^ (k >> 29)) & (BUCKETS - 1));
}
static bool store_point(RuntimeSpectralVolume *v, Vec3 p, Vec3 dir, int band, double watts, bool first) {
    if(!spectral_reconstruction_accepting(v))return false;
    if (!v->heads) {
        v->heads = calloc(BUCKETS, sizeof(int));
        if (!v->heads) return false;
    }
    if (v->count == v->capacity) {
        if (v->count == SPECTRAL_MAX_COLLISIONS) return false;
        int n = v->capacity ? v->capacity * 2 : 1024;
        SpectralCollision *points = realloc(v->points, (size_t)n * sizeof(*points));
        if (!points) return false;
        v->points = points;
        if(v->config.reconstruction) {
            Vec3 *positions=realloc(v->positions,(size_t)n*sizeof(*positions));
            if(!positions)return false;
            v->positions=positions;
        }
        v->capacity = n;
    }
    double vol;
    uint64_t cell = cell_at(v, p, &vol);
    unsigned index = spectral_volume_bucket(cell, band);
    v->points[v->count] = (SpectralCollision){.incoming = dir,
                                              .watts = watts,
                                              .first = first,
                                              .cell = cell,
                                              .band = band,
                                              .next = v->heads[index]};
    if(v->positions)v->positions[v->count]=p;
    v->heads[index] = ++v->count;
    return true;
}
static double phase(double mu, double g) {
    double d = 1 + g * g - 2 * g * fmax(-1, fmin(1, mu));
    return (1 - g * g) / (4 * M_PI * d * sqrt(d));
}
static Vec3 scatter_direction(Vec3 incoming, double g, double u, double phi, double *mu) {
    /* Algebraically expanded HG inversion has a continuous isotropic limit. */
    double q = 2 * u - 1, den = 1 + g * q;
    *mu = fmax(
        -1, fmin(1, (2 * q + g * (q * q + 3) + 2 * g * g * q + g * g * g * (q * q - 1)) / (2 * den * den)));
    Vec3 axis = fabs(incoming.z) < .9 ? vec3(0, 0, 1) : vec3(0, 1, 0);
    Vec3 x = vec3_normalize(vec3_cross(axis, incoming)), y = vec3_cross(incoming, x);
    Vec3 radial = vec3_add(vec3_scale(x, cos(phi)), vec3_scale(y, sin(phi)));
    return vec3_add(vec3_scale(incoming, *mu), vec3_scale(radial, sqrt(fmax(0, 1 - *mu * *mu))));
}
double RuntimeSpectralVolume_Transmission(const RuntimeSpectralVolume *v, Ray3D ray, double t, double nm) {
    if (!v) return 1;
    double st = RuntimeSpectralCurve_Value(&v->config.absorption, nm) +
                RuntimeSpectralCurve_Value(&v->config.scattering, nm);
    SpectralFieldRay it;
    double a, b, mass = 0;
    if (st == 0 || !spectral_field_begin(&it, v, ray, t, false)) return 1;
    while (spectral_field_next(&it, &a, &b))
        mass += spectral_field_mass(&it, a, b);
    return exp(-mass * st);
}
static double source_term(const RuntimeSpectralVolume *v, Ray3D ray, double midpoint, int band,
                          double *first) {
    double vol, sum = 0;
    uint64_t cell = cell_at(v, vec3_add(ray.origin, vec3_scale(ray.direction, midpoint)), &vol);
    for (int k = v->heads[spectral_volume_bucket(cell, band)]; k; k = v->points[k - 1].next) {
        const SpectralCollision *p = &v->points[k - 1];
        if (p->cell == cell && p->band == band) {
            double x = p->watts * phase(-vec3_dot(p->incoming, ray.direction), v->config.g);
            sum += x;
            if (p->first) *first += x;
        }
    }
    *first /= vol;
    return sum / vol;
}
static double camera_length(const SpectralFieldRay *it, double a, double b, double st, double tau) {
    if (tau == 0) return b - a;
    if (it->v->config.kind != SPECTRAL_VOLUME_VF3D) return (b - a) * (-expm1(-tau) / tau);
    /* For thick varying media resolve the optically active interval before
     * quadrature. Deep portions have negligible camera transmittance. */
    if (tau > 2) {
        double mid = a + (b - a) * .5;
        if (mid > a && mid < b) {
            double left = st * spectral_field_mass(it, a, mid);
            double length = camera_length(it, a, mid, st, left);
            if (left < 40)
                length += exp(-left) * camera_length(it, mid, b, st, st * spectral_field_mass(it, mid, b));
            return length;
        }
    }
    /* Integrate camera transmittance to the source term within this field segment. */
    static const double x[] = {.0694318442, .3300094782, .6699905218, .9305681558};
    static const double w[] = {.1739274226, .3260725774, .3260725774, .1739274226};
    double sum = 0;
    for (int i = 0; i < 4; ++i)
        sum += w[i] * exp(-st * spectral_field_mass(it, a, a + (b - a) * x[i]));
    return (b - a) * sum;
}
/* Integrate within both field knots and geometry partitions. Resolve thick
 * intervals before quadrature rather than allowing attenuation to alias. */
static double continuous_camera(RuntimeSpectralVolume *v,Ray3D ray,int band,const SpectralFieldRay *it,
                                double a,double b,double st,double tau,double *first) {
    if(tau>.25) {
        double mid=a+(b-a)*.5;
        if(mid>a && mid<b) {
            double left_tau=st*spectral_field_mass(it,a,mid),left_first=0,right_first=0;
            double left=continuous_camera(v,ray,band,it,a,mid,st,left_tau,&left_first),right=0;
            if(left_tau<40)right=continuous_camera(v,ray,band,it,mid,b,st,st*spectral_field_mass(it,mid,b),&right_first);
            *first+=left_first+exp(-left_tau)*right_first;
            return left+exp(-left_tau)*right;
        }
    }
    static const double q2[]={.21132486540518713,.7886751345948129},w2[]={.5,.5};
    static const double q4[]={.06943184420297371,.33000947820757187,.6699905217924281,.9305681557970262};
    static const double w4[]={.17392742256872693,.32607257743127307,.32607257743127307,.17392742256872693};
    int n=tau<.025?2:4;const double *q=n==2?q2:q4,*weights=n==2?w2:w4;double sum=0;
    for(int i=0;i<n;i++) {
        double t=a+(b-a)*q[i],single=0;
        double value=spectral_reconstruction_source(v,vec3_add(ray.origin,vec3_scale(ray.direction,t)),band,&single);
        if(value==0 && single==0)continue;
        double weight=(b-a)*weights[i]*exp(-st*spectral_field_mass(it,a,t));
        sum+=weight*value;*first+=weight*single;
    }
    return sum;
}
bool RuntimeSpectralVolume_PrepareReconstruction(RuntimeSpectralVolume *v) {
    return !v || !v->config.reconstruction || spectral_reconstruction_build(v);
}
bool RuntimeSpectralVolume_Segment(RuntimeSpectralVolume *v, const RuntimeSpectralPencilLine *line,
                                   Ray3D *ray, double limit, bool camera, int step,
                                   RuntimeSpectralPathReadout *out, bool *restart) {
    *restart = false;
    if(camera && v->config.reconstruction && !spectral_reconstruction_begin_ray(v,*ray))return false;
    double sa = RuntimeSpectralCurve_Value(&v->config.absorption, line->vacuum_nm);
    double ss = RuntimeSpectralCurve_Value(&v->config.scattering, line->vacuum_nm), st = sa + ss;
    SpectralFieldRay it;
    RuntimeSpectralCameraCache *cache = camera ? line->camera_cache : NULL;
    if (!st || !spectral_camera_begin(cache, &it, v, *ray, limit, camera && v->heads)) return true;
    uint64_t id = (line->sample_id * UINT64_C(0x9e3779b97f4a7c15)) ^ UINT64_C(0xc2b2ae3d27d4eb4f) ^
                  ((uint64_t)step << 4);
    double remaining = -log1p(-RuntimeSpectral_Uniform64(id)), a, b;
    double mass;
    while (spectral_camera_next(cache, &it, &a, &b, &mass)) {
        double tau = st * mass;
        if (!(tau > 0)) continue;
        if (camera) {
            if (v->heads && ss > 0) {
                if(v->config.reconstruction) {
                    double first=0;
                    double value=continuous_camera(v,*ray,line->band,&it,a,b,st,tau,&first);
                    out->inscattered+=out->weight*value;
                    out->single_scatter+=out->weight*first;
                } else {
                double first = 0, j = source_term(v, *ray, (a + b) * .5, line->band, &first);
                if (j) {
                    double weight = out->weight * camera_length(&it, a, b, st, tau);
                    out->inscattered += weight * j;
                    out->single_scatter += weight * first;
                }
                }
            }
            out->weight *= exp(-tau);
        } else if (remaining >= tau)
            remaining -= tau;
        else {
            if (RuntimeSpectral_Uniform64(id + 1) >= ss / st) {
                out->volume_absorbed = true;
                return true;
            }
            double t = a + (b - a) * remaining / tau;
            if (v->config.kind == SPECTRAL_VOLUME_VF3D) {
                double lo = a, hi = b;
                for (int i = 0; i < 36; ++i) {
                    t = (lo + hi) * .5;
                    if (st * spectral_field_mass(&it, a, t) < remaining)
                        lo = t;
                    else
                        hi = t;
                }
                t = (lo + hi) * .5;
            }
            Vec3 p = vec3_add(ray->origin, vec3_scale(ray->direction, t));
            if (!store_point(v, p, ray->direction, line->band, out->weight, !out->scatters)) return false;
            double mu;
            Vec3 dir = scatter_direction(ray->direction, v->config.g, RuntimeSpectral_Uniform64(id + 2),
                                         2 * M_PI * RuntimeSpectral_Uniform64(id + 3), &mu);
            RuntimeSpectralBand *row = &v->ledger[line->band];
            ++row->collisions;
            row->cosine_sum += mu;
            if (out->scatters++)
                row->higher_scatter_w += out->weight;
            else
                row->first_scatter_w += out->weight;
            *ray = RuntimeRay3D_Make(p, dir);
            *restart = true;
            return true;
        }
    }
    return true;
}
void RuntimeSpectralVolume_ScatterStats(const RuntimeSpectralVolume *v, RuntimeSpectralMapStats *st) {
    spectral_reconstruction_stats(v,st);
    for (int b = 0; b < st->band_count; ++b) {
        RuntimeSpectralBand *r = &st->bands[b];
        const RuntimeSpectralBand *s = &v->ledger[b];
        r->collisions = s->collisions;
        r->cosine_sum = s->cosine_sum;
        r->first_scatter_w = s->first_scatter_w;
        r->higher_scatter_w = s->higher_scatter_w;
    }
}
