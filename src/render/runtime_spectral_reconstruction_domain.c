#include "render/runtime_scene_accel_3d_internal.h"
#include "render/runtime_volume_3d_sampling.h"
#include "runtime_spectral_reconstruction_internal.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static double component(Vec3 v, int a) { return a == 0 ? v.x : a == 1 ? v.y : v.z; }
static bool overlap(Vec3 a, Vec3 b, Vec3 lo, Vec3 hi) {
    return a.x < hi.x && b.x > lo.x && a.y < hi.y && b.y > lo.y && a.z < hi.z && b.z > lo.z;
}
static int compare(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}
static int cell(const SpectralDomainAxis *a, double x) {
    if (x < a->knot[0] || x >= a->knot[a->count - 1])
        return -1;
    int lo = 0, hi = a->count - 1;
    while (hi - lo > 1) {
        int mid = (lo + hi) / 2;
        if (x < a->knot[mid])
            hi = mid;
        else
            lo = mid;
    }
    return lo;
}
static size_t index_at(const SpectralDomain *d, int x, int y, int z) {
    return x + (size_t)(d->axis[0].count - 1) * (y + (size_t)(d->axis[1].count - 1) * z);
}
uint16_t spectral_domain_region(const SpectralDomain *d, Vec3 p) {
    /* A few complete rectangular components admit an exact bounds lookup.
     * Nonrectangular occupancy retains the interpolation-cell label oracle. */
    if (d->rectangular_lookup) {
        for (int i = 1; i <= d->region_count; i++) {
            const SpectralDomainRegion *c = &d->regions[i];
            if (p.x >= c->lo.x && p.x < c->hi.x && p.y >= c->lo.y && p.y < c->hi.y &&
                p.z >= c->lo.z && p.z < c->hi.z)
                return (uint16_t)i;
        }
        return 0;
    }
    int x = cell(&d->axis[0], p.x), y = cell(&d->axis[1], p.y), z = cell(&d->axis[2], p.z);
    return x < 0 || y < 0 || z < 0 ? 0 : d->labels[index_at(d, x, y, z)];
}
void spectral_domain_free(SpectralDomain *d) {
    for (int a = 0; a < 3; a++) {
        free(d->axis[a].knot);
        free(d->axis[a].wall);
    }
    free(d->labels);
    free(d->regions);
    memset(d, 0, sizeof(*d));
}
/* Surface admission is deliberately conservative. AABB overlap with any curved,
 * oblique or finite interior occluder refuses continuous reconstruction. */
static bool barriers(SpectralReconstruction *r, const RuntimeScene3D *s, double planes[3][64],
                     int count[3]) {
    const RuntimeSpectralVolume *v = r->volume;
    RuntimeSceneAcceleration3DInstanceBounds *bounds = NULL;
    int n = 0;
    if (!RuntimeSceneAcceleration3D_CaptureInstanceBounds(s, &bounds, &n, NULL))
        return spectral_reconstruction_fail(r, "continuous volume cannot inspect scene boundaries");
    bool ok = true;
    for (int k = 0; k < s->primitiveCount; k++)
        if (s->primitives[k].kind != RUNTIME_PRIMITIVE_3D_KIND_PLANE) {
            bool found = false;
            for (int i = 0; i < n; i++)
                if (bounds[i].primitiveIndex == k)
                    found = true;
            if (!found)
                ok = false;
        }
    for (int i = 0; i < n && ok; i++) {
        if (!overlap(bounds[i].min, bounds[i].max, v->lo, v->hi))
            continue;
        int k = bounds[i].primitiveIndex;
        if (k >= 0 && k < s->primitiveCount &&
            s->primitives[k].kind == RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH) {
            if (!spectral_mesh_admit(r, s, k)) {
                ok = false;
                break;
            }
            continue;
        }
        if (k < 0 || k >= s->primitiveCount ||
            s->primitives[k].kind != RUNTIME_PRIMITIVE_3D_KIND_PLANE) {
            ok = false;
            break;
        }
    }
    RuntimeSceneAcceleration3D_FreeInstanceIdentityMaps(bounds, n);
    free(bounds);
    if (!ok && r->error[0])
        return false;
    if (!ok)
        return spectral_reconstruction_fail(
            r, "continuous volume does not admit intersecting curved or mesh boundaries");
    for (int i = 0; i < s->primitiveCount; i++) {
        const RuntimePrimitive3D *p = &s->primitives[i];
        if (p->kind != RUNTIME_PRIMITIVE_3D_KIND_PLANE)
            continue;
        const RuntimePlane3D *q = &p->shape.plane;
        Vec3 e = vec3_add(
            vec3_scale(vec3(fabs(q->axisU.x), fabs(q->axisU.y), fabs(q->axisU.z)), q->width * .5),
            vec3_scale(vec3(fabs(q->axisV.x), fabs(q->axisV.y), fabs(q->axisV.z)), q->height * .5));
        Vec3 lo = vec3_sub(q->origin, e), hi = vec3_add(q->origin, e);
        if (!overlap(lo, hi, v->lo, v->hi))
            continue;
        int a = -1;
        for (int j = 0; j < 3; j++)
            if (fabs(fabs(component(q->normal, j)) - 1) < 1e-12)
                a = j;
        if (a < 0)
            return spectral_reconstruction_fail(
                r, "continuous volume requires axis-aligned interior planes");
        for (int j = 0; j < 3; j++)
            if (j != a &&
                (component(lo, j) > component(v->lo, j) || component(hi, j) < component(v->hi, j)))
                return spectral_reconstruction_fail(
                    r, "continuous volume interior planes must span the complete field");
        for (int corner = 0; corner < 8; corner++) {
            Vec3 point = vec3(corner & 1 ? v->hi.x : v->lo.x, corner & 2 ? v->hi.y : v->lo.y,
                              corner & 4 ? v->hi.z : v->lo.z);
            Vec3 offset = vec3_sub(point, q->origin);
            if (fabs(vec3_dot(offset, q->axisU)) > q->width * .5 ||
                fabs(vec3_dot(offset, q->axisV)) > q->height * .5)
                return spectral_reconstruction_fail(
                    r, "continuous volume interior planes must span the complete field");
        }
        if (count[a] == 64)
            return spectral_reconstruction_fail(r, "continuous volume boundary count exceeded");
        planes[a][count[a]++] = component(q->origin, a);
    }
    return true;
}
bool spectral_domain_build(SpectralReconstruction *r, const RuntimeScene3D *s) {
    RuntimeSpectralVolume *v = r->volume;
    SpectralDomain *d = &r->domain;
    if (v->config.kind == SPECTRAL_VOLUME_WATER || v->vf.hasPlacement)
        return spectral_reconstruction_fail(
            r, "continuous reconstruction currently admits untransformed boxes and VF3D fields");
    double planes[3][64];
    int counts[3] = {0};
    if (!barriers(r, s, planes, counts))
        return false;
    uint32_t sizes[] = {v->vf.grid.gridW, v->vf.grid.gridH, v->vf.grid.gridD};
    size_t cells = 1;
    for (int a = 0; a < 3; a++) {
        SpectralDomainAxis *ax = &d->axis[a];
        size_t capacity =
            (size_t)4 + counts[a] + (v->config.kind == SPECTRAL_VOLUME_VF3D ? sizes[a] : 0);
        if (capacity > 1000002)
            return spectral_reconstruction_fail(r, "continuous domain axis capacity exceeded");
        ax->knot = spectral_reconstruction_resize(r, NULL, 0, capacity * sizeof(double));
        if (!ax->knot)
            return false;
        double lo = component(v->lo, a), hi = component(v->hi, a);
        ax->knot[ax->count++] = lo;
        ax->knot[ax->count++] = hi;
        if (v->config.kind == SPECTRAL_VOLUME_VF3D) {
            double base = component(v->vf.grid.origin, a), h = v->vf.grid.voxelSize;
            /* Include sampler validity bounds and the clamped edge plateaus. */
            double ends[] = {base, base + sizes[a] * h};
            for (int k = 0; k < 2; k++)
                if (ends[k] > lo && ends[k] < hi)
                    ax->knot[ax->count++] = ends[k];
            for (uint32_t k = 0; k < sizes[a]; k++) {
                double x = base + (k + (v->vf.grid.cellCentered ? .5 : 0)) * h;
                if (x > lo && x < hi)
                    ax->knot[ax->count++] = x;
            }
        }
        for (int k = 0; k < counts[a]; k++)
            ax->knot[ax->count++] = planes[a][k];
        qsort(ax->knot, ax->count, sizeof(double), compare);
        int n = 1;
        for (int k = 1; k < ax->count; k++)
            if (ax->knot[k] > ax->knot[n - 1])
                ax->knot[n++] = ax->knot[k];
        ax->count = n;
        ax->wall = spectral_reconstruction_resize(r, NULL, 0, n);
        if (!ax->wall)
            return false;
        for (int k = 0; k < n; k++)
            for (int j = 0; j < counts[a]; j++)
                if (ax->knot[k] == planes[a][j])
                    ax->wall[k] = 1;
        if (cells > 1000000u / (n - 1))
            return spectral_reconstruction_fail(
                r, "continuous domain exceeds one million interpolation cells");
        cells *= n - 1;
    }
    d->cells = cells;
    d->labels = spectral_reconstruction_resize(r, NULL, 0, cells * sizeof(*d->labels));
    uint32_t *queue = spectral_reconstruction_resize(r, NULL, 0, cells * sizeof(*queue));
    if (!d->labels || !queue) {
        free(queue);
        return false;
    }
    int dims[] = {d->axis[0].count - 1, d->axis[1].count - 1, d->axis[2].count - 1};
    for (int z = 0; z < dims[2]; z++)
        for (int y = 0; y < dims[1]; y++)
            for (int x = 0; x < dims[0]; x++) {
                Vec3 p = vec3((d->axis[0].knot[x] + d->axis[0].knot[x + 1]) * .5,
                              (d->axis[1].knot[y] + d->axis[1].knot[y + 1]) * .5,
                              (d->axis[2].knot[z] + d->axis[2].knot[z + 1]) * .5);
                if (v->config.kind == SPECTRAL_VOLUME_BOX ||
                    RuntimeVolume3D_SampleDensityAtPosition(&v->vf, p) > 0)
                    d->labels[index_at(d, x, y, z)] = UINT16_MAX;
            }
    size_t capacity = 0;
    for (size_t start = 0; start < cells; start++)
        if (d->labels[start] == UINT16_MAX) {
            if (d->region_count == (d->mesh.count ? 6553 : UINT16_MAX - 1)) {
                free(queue);
                return spectral_reconstruction_fail(r, "continuous component capacity exceeded");
            }
            int id = ++d->region_count;
            if ((size_t)id >= capacity) {
                size_t next = capacity ? capacity * 2 : 16;
                void *p = spectral_reconstruction_resize(
                    r, d->regions, capacity * sizeof(*d->regions), next * sizeof(*d->regions));
                if (!p) {
                    free(queue);
                    return false;
                }
                d->regions = p;
                capacity = next;
            }
            size_t head = 0, tail = 0;
            queue[tail++] = (uint32_t)start;
            d->labels[start] = (uint16_t)id;
            int min[3] = {dims[0], dims[1], dims[2]}, max[3] = {0};
            while (head < tail) {
                size_t cur = queue[head++];
                int xyz[] = {cur % dims[0], (cur / dims[0]) % dims[1],
                             cur / ((size_t)dims[0] * dims[1])};
                for (int a = 0; a < 3; a++) {
                    if (xyz[a] < min[a])
                        min[a] = xyz[a];
                    if (xyz[a] > max[a])
                        max[a] = xyz[a];
                    for (int sign = -1; sign <= 1; sign += 2) {
                        int nb[3] = {xyz[0], xyz[1], xyz[2]};
                        nb[a] += sign;
                        if (nb[a] < 0 || nb[a] >= dims[a] || d->axis[a].wall[xyz[a] + (sign > 0)])
                            continue;
                        size_t j = index_at(d, nb[0], nb[1], nb[2]);
                        if (d->labels[j] == UINT16_MAX) {
                            d->labels[j] = (uint16_t)id;
                            queue[tail++] = (uint32_t)j;
                        }
                    }
                }
            }
            SpectralDomainRegion *c = &d->regions[id];
            c->lo = vec3(d->axis[0].knot[min[0]], d->axis[1].knot[min[1]], d->axis[2].knot[min[2]]);
            c->hi = vec3(d->axis[0].knot[max[0] + 1], d->axis[1].knot[max[1] + 1],
                         d->axis[2].knot[max[2] + 1]);
            c->rectangular = tail == (size_t)(max[0] - min[0] + 1) * (max[1] - min[1] + 1) *
                                         (max[2] - min[2] + 1);
        }
    d->rectangular_lookup = d->region_count <= 4;
    for (int i = 1; i <= d->region_count; i++)
        if (!d->regions[i].rectangular)
            d->rectangular_lookup = false;
    free(queue);
    r->owned_bytes -= cells * sizeof(*queue);
    return true;
}
/* Integral of a nodal hat over [a,b], including clipped/partial end cells. */
double spectral_reconstruction_hat_integral(double left, double peak, double right, double a,
                                            double b) {
    double sum = 0, lo = fmax(a, left), hi = fmin(b, peak);
    if (hi > lo && peak > left)
        sum += (hi - lo) * ((lo - left) + (hi - left)) / (2 * (peak - left));
    lo = fmax(a, peak);
    hi = fmin(b, right);
    if (hi > lo && right > peak)
        sum += (hi - lo) * ((right - lo) + (right - hi)) / (2 * (right - peak));
    return sum;
}
double spectral_domain_norm(SpectralReconstruction *r, const int id[3],
                            int level, uint16_t region) {
    return spectral_domain_part_norm(r, id, level, region, 0);
}
double spectral_domain_part_norm(SpectralReconstruction *r, const int id[3],
                                 int level, uint16_t region, int part) {
    RuntimeSpectralVolume *v = r->volume;
    SpectralDomain *d = &r->domain;
    double h = v->config.cell_m * (level ? 2 : 1), left[3], peak[3], right[3];
    for (int a = 0; a < 3; a++) {
        double lo = component(v->lo, a), hi = component(v->hi, a);
        peak[a] = fmin(hi, lo + id[a] * h);
        left[a] = id[a] ? lo + (id[a] - 1) * h : lo;
        right[a] = fmin(hi, peak[a] + h);
    }
    const SpectralDomainRegion *c = &d->regions[region];
    if (c->rectangular) {
        if (part)
            return spectral_mesh_integral(r, left, peak, right, c->lo, c->hi,
                                          part);
        double norm = 1;
        for (int a = 0; a < 3; a++)
            norm *= spectral_reconstruction_hat_integral(left[a], peak[a], right[a],
                                                         component(c->lo, a), component(c->hi, a));
        return norm;
    }
    int begin[3], end[3];
    for (int a = 0; a < 3; a++) {
        begin[a] = cell(&d->axis[a], fmax(d->axis[a].knot[0], left[a]));
        end[a] = cell(&d->axis[a],
                      nextafter(fmin(d->axis[a].knot[d->axis[a].count - 1], right[a]), -INFINITY));
        if (begin[a] < 0 || end[a] < 0)
            return 0;
    }
    double sum = 0;
    for (int z = begin[2]; z <= end[2]; z++)
        for (int y = begin[1]; y <= end[1]; y++)
            for (int x = begin[0]; x <= end[0]; x++) {
                if (++r->normalization_visits > 64000000) {
                    spectral_reconstruction_fail(r,
                                                 "continuous normalization work budget exceeded");
                    return 0;
                }
                if (d->labels[index_at(d, x, y, z)] != region)
                    continue;
                int xyz[] = {x, y, z};
                if (part) {
                    Vec3 lo = vec3(d->axis[0].knot[x], d->axis[1].knot[y],
                                   d->axis[2].knot[z]);
                    Vec3 hi =
                        vec3(d->axis[0].knot[x + 1], d->axis[1].knot[y + 1],
                             d->axis[2].knot[z + 1]);
                    sum += spectral_mesh_integral(r, left, peak, right, lo, hi,
                                                  part);
                    continue;
                }
                double vol = 1;
                for (int a = 0; a < 3; a++)
                    vol *= spectral_reconstruction_hat_integral(left[a], peak[a], right[a],
                                                                d->axis[a].knot[xyz[a]],
                                                                d->axis[a].knot[xyz[a] + 1]);
                sum += vol;
            }
    return sum;
}
double spectral_domain_next(const SpectralDomain *d, Ray3D ray, double a, double b) {
    Vec3 p = vec3_add(ray.origin, vec3_scale(ray.direction, a));
    for (int i = 0; i < d->mesh.count; i++) {
        SpectralMeshPlane plane = d->mesh.plane[i];
        double denominator = vec3_dot(plane.n, ray.direction);
        if (denominator) {
            double t = (plane.d - vec3_dot(plane.n, ray.origin)) / denominator;
            if (t > a + 64 * DBL_EPSILON * fmax(1, fabs(a)))
                b = fmin(b, t);
        }
    }
    for (int axis = 0; axis < 3; axis++) {
        double x = component(p, axis), dir = component(ray.direction, axis);
        if (!dir)
            continue;
        const SpectralDomainAxis *ax = &d->axis[axis];
        double eps = 64 * DBL_EPSILON * fmax(fabs(x), ax->knot[ax->count - 1] - ax->knot[0]);
        int i = cell(ax, fmax(ax->knot[0], fmin(nextafter(ax->knot[ax->count - 1], -INFINITY), x)));
        int k = dir > 0 ? i + 1 : i;
        while (k >= 0 && k < ax->count &&
               (dir > 0 ? ax->knot[k] <= x + eps : ax->knot[k] >= x - eps))
            k += dir > 0 ? 1 : -1;
        if (k >= 0 && k < ax->count)
            b = fmin(b, a + (ax->knot[k] - x) / dir);
    }
    return b;
}
