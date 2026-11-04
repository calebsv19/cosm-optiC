#include "runtime_spectral_reconstruction_internal.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double axis(Vec3 p, int a) { return a == 0 ? p.x : a == 1 ? p.y : p.z; }
bool spectral_reconstruction_fail(SpectralReconstruction *r, const char *message) {
    snprintf(r->error, sizeof(r->error), "%s", message);
    return false;
}
void *spectral_reconstruction_resize(SpectralReconstruction *r, void *old, size_t old_bytes,
                                     size_t bytes) {
    size_t transport = (size_t)r->volume->capacity *
                           (sizeof(SpectralCollision) + (r->volume->positions ? sizeof(Vec3) : 0)) +
                       (r->volume->heads ? 65536 * sizeof(int) : 0);
    if (bytes > r->budget_bytes || r->owned_bytes + transport > r->budget_bytes - bytes) {
        spectral_reconstruction_fail(r, "continuous reconstruction memory budget exceeded");
        return NULL;
    }
    void *p = calloc(1, bytes);
    if (!p) {
        spectral_reconstruction_fail(r, "continuous reconstruction allocation failed");
        return NULL;
    }
    r->peak_bytes = fmax(r->peak_bytes, r->owned_bytes + transport + bytes);
    if (old) {
        memcpy(p, old, old_bytes < bytes ? old_bytes : bytes);
        free(old);
    }
    r->owned_bytes += bytes - old_bytes;
    return p;
}
void spectral_reconstruction_free(SpectralReconstruction *r) {
    if (!r)
        return;
    spectral_domain_free(&r->domain);
    free(r->nodes);
    free(r->links);
    free(r->slots);
    free(r);
}
bool RuntimeSpectralVolume_BindScene(RuntimeSpectralVolume *v, const RuntimeScene3D *s, char *diag,
                                     size_t size) {
    if (!v || !v->config.reconstruction)
        return true;
    if (v->reconstruction) {
        snprintf(diag, size, "continuous volume scene already bound");
        return false;
    }
    SpectralReconstruction *r = calloc(1, sizeof(*r));
    if (!r) {
        snprintf(diag, size, "continuous reconstruction allocation failed");
        return false;
    }
    v->reconstruction = r;
    r->volume = v;
    r->owned_bytes = sizeof(*r);
    r->budget_bytes = 256u * 1024u * 1024u;
    if (!spectral_domain_build(r, s)) {
        snprintf(diag, size, "%s", r->error);
        return false;
    }
    return true;
}
const char *RuntimeSpectralVolume_Error(const RuntimeSpectralVolume *v) {
    return v && v->reconstruction && v->reconstruction->error[0]
               ? v->reconstruction->error
               : "spectral volume reconstruction failed";
}
void spectral_reconstruction_basis(const RuntimeSpectralVolume *v, double x, int a, int level,
                                   int id[2], double w[2]) {
    double lo = axis(v->lo, a), hi = axis(v->hi, a), h = v->config.cell_m * (level ? 2 : 1);
    int last = (int)ceil((hi - lo) / h), k = (int)fmax(0, fmin(last - 1, floor((x - lo) / h)));
    double left = lo + k * h, right = fmin(hi, left + h),
           u = fmax(0, fmin(1, (x - left) / (right - left)));
    id[0] = k;
    id[1] = k + 1;
    w[0] = 1 - u;
    w[1] = u;
}
static uint64_t key_make(int x, int y, int z, int band, int level, uint16_t region) {
    return 1 + ((uint64_t)x << 24 | (uint64_t)y << 12 | (uint64_t)z | (uint64_t)band << 36 |
                (uint64_t)level << 44 | (uint64_t)region << 45);
}
static uint64_t hash(uint64_t k) {
    k = (k ^ (k >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    return (k ^ (k >> 27)) * UINT64_C(0x94d049bb133111eb);
}
static size_t slot(const SpectralReconstruction *r, uint64_t key) {
    size_t i = hash(key) & (r->slot_capacity - 1);
    while (r->slots[i] && r->nodes[r->slots[i] - 1].key != key)
        i = (i + 1) & (r->slot_capacity - 1);
    return i;
}
static bool grow_slots(SpectralReconstruction *r) {
    size_t old_count = r->slot_capacity, next = old_count ? old_count * 2 : 65536;
    uint32_t *old = r->slots;
    /* Keep the old table until allocation succeeds, including its peak budget. */
    uint32_t *p = spectral_reconstruction_resize(r, NULL, 0, next * sizeof(*p));
    if (!p)
        return false;
    r->slots = p;
    r->slot_capacity = next;
    for (size_t i = 0; i < r->node_count; i++)
        r->slots[slot(r, r->nodes[i].key)] = (uint32_t)i + 1;
    free(old);
    r->owned_bytes -= old_count * sizeof(*old);
    return true;
}
static int level_for(const RuntimeSpectralVolume *v, const SpectralCollision *p) {
    if (v->config.reconstruction != SPECTRAL_RECONSTRUCTION_ADAPTIVE)
        return 0;
    int count = 0;
    for (int k = v->heads[spectral_volume_bucket(p->cell, p->band)]; k; k = v->points[k - 1].next) {
        const SpectralCollision *q = &v->points[k - 1];
        if (q->cell == p->cell && q->band == p->band && ++count >= 8)
            return 0;
    }
    return 1;
}
bool spectral_reconstruction_build(RuntimeSpectralVolume *v) {
    SpectralReconstruction *r = v->reconstruction;
    if (!r)
        return !v->config.reconstruction;
    if (r->built)
        return true;
    if (!grow_slots(r))
        return false;
    for (int i = 0; i < v->count; i++) {
        const SpectralCollision *p = &v->points[i];
        uint16_t region = spectral_domain_region(&r->domain, v->positions[i]);
        if (!region)
            return spectral_reconstruction_fail(
                r, "scattering event outside admitted occupied component");
        int level = level_for(v, p), ids[3][2];
        double w[3][2];
        for (int a = 0; a < 3; a++)
            spectral_reconstruction_basis(v, axis(v->positions[i], a), a, level, ids[a], w[a]);
        for (int z = 0; z < 2; z++)
            for (int y = 0; y < 2; y++)
                for (int x = 0; x < 2; x++) {
                    if (!(w[0][x] * w[1][y] * w[2][z] > 0))
                        continue;
                    if ((r->node_count + 1) * 2 > r->slot_capacity && !grow_slots(r))
                        return false;
                    uint64_t key =
                        key_make(ids[0][x], ids[1][y], ids[2][z], p->band, level, region);
                    size_t s = slot(r, key);
                    uint32_t ni = r->slots[s];
                    if (!ni) {
                        if (r->node_count == r->node_capacity) {
                            size_t next = r->node_capacity ? r->node_capacity * 2 : 4096;
                            void *q = spectral_reconstruction_resize(
                                r, r->nodes, r->node_capacity * sizeof(*r->nodes),
                                next * sizeof(*r->nodes));
                            if (!q)
                                return false;
                            r->nodes = q;
                            r->node_capacity = next;
                        }
                        int at[] = {ids[0][x], ids[1][y], ids[2][z]};
                        double norm = spectral_domain_norm(r, at, level, region);
                        if (r->error[0])
                            return false;
                        if (!(norm > 0) || !isfinite(norm))
                            return spectral_reconstruction_fail(
                                r, "continuous node has invalid occupied-domain normalization");
                        ni = (uint32_t)++r->node_count;
                        r->slots[s] = ni;
                        r->nodes[ni - 1] = (SpectralReconstructionNode){.key = key, .norm = norm};
                    }
                    if (r->link_count == r->link_capacity) {
                        size_t next = r->link_capacity ? r->link_capacity * 2 : 4096;
                        void *q = spectral_reconstruction_resize(
                            r, r->links, r->link_capacity * sizeof(*r->links),
                            next * sizeof(*r->links));
                        if (!q)
                            return false;
                        r->links = q;
                        r->link_capacity = next;
                    }
                    SpectralReconstructionNode *node = &r->nodes[ni - 1];
                    r->links[r->link_count] = (SpectralReconstructionLink){
                        .flux = p->watts * w[0][x] * w[1][y] * w[2][z] / node->norm,
                        .photon = (uint32_t)i,
                        .next = node->head};
                    node->head = (uint32_t)++r->link_count;
                }
    }
    free(v->positions);
    v->positions = NULL;
    r->built = true;
    return true;
}
static void decode(uint64_t key, int id[3], int *level) {
    --key;
    id[0] = (key >> 24) & 4095;
    id[1] = (key >> 12) & 4095;
    id[2] = key & 4095;
    *level = (key >> 44) & 1;
}
static Vec3 node_position(const RuntimeSpectralVolume *v, const SpectralReconstructionNode *n) {
    int id[3], level;
    decode(n->key, id, &level);
    double h = v->config.cell_m * (level ? 2 : 1);
    return vec3(fmin(v->hi.x, v->lo.x + id[0] * h), fmin(v->hi.y, v->lo.y + id[1] * h),
                fmin(v->hi.z, v->lo.z + id[2] * h));
}
static double phase(double mu, double g) {
    double d = 1 + g * g - 2 * g * fmax(-1, fmin(1, mu));
    return (1 - g * g) / (4 * M_PI * d * sqrt(d));
}
static Vec3 reference_direction(const SpectralReconstruction *r,
                                const SpectralReconstructionNode *n) {
    Vec3 d = vec3_sub(r->view_origin, node_position(r->volume, n));
    return vec3_dot(d, d) > 0 ? vec3_normalize(d) : vec3(0, 0, 1);
}
static double exact_node(const SpectralReconstruction *r, const SpectralReconstructionNode *n,
                         Vec3 outgoing, double *first) {
    const RuntimeSpectralVolume *v = r->volume;
    double total = 0;
    for (uint32_t k = n->head; k; k = r->links[k - 1].next) {
        const SpectralCollision *p = &v->points[r->links[k - 1].photon];
        double value = r->links[k - 1].flux * phase(vec3_dot(p->incoming, outgoing), v->config.g);
        total += value;
        if (p->first)
            *first += value;
    }
    return total;
}
bool spectral_reconstruction_begin_ray(RuntimeSpectralVolume *v, Ray3D ray) {
    SpectralReconstruction *r = v->reconstruction;
    if (!r || !spectral_reconstruction_build(v))
        return false;
    if (!r->view_ready) {
        r->view_origin = ray.origin;
        for (size_t i = 0; i < r->node_count; i++) {
            SpectralReconstructionNode *n = &r->nodes[i];
            n->cached_total = exact_node(r, n, reference_direction(r, n), &n->cached_first);
        }
        /* |d log HG / d theta| <= 3|g|/(1-g^2). Thus the positive weighted
         * sum is within the requested relative phase tolerance whenever this
         * angular test passes, independently of incoming directions/weights. */
        double g = fabs(v->config.g),
               angle = g ? log1p(v->config.phase_tolerance) * (1 - g * g) / (3 * g) : M_PI;
        r->angular_cos_limit = cos(fmin(M_PI, angle));
        r->view_ready = true;
    }
    if (!r->ray_ready || memcmp(&ray.origin, &r->ray.origin, sizeof(Vec3)) ||
        memcmp(&ray.direction, &r->ray.direction, sizeof(Vec3))) {
        r->ray = ray;
        r->ray_ready = true;
        if (++r->generation == 0) {
            memset(r->angular, 0, sizeof(r->angular));
            r->generation = 1;
        }
    }
    return true;
}
static void angular_node(SpectralReconstruction *r, uint32_t ni, double *total, double *first) {
    SpectralAngularCache *c = &r->angular[hash(ni) & 2047];
    if (c->generation != r->generation || c->node != ni) {
        const SpectralReconstructionNode *n = &r->nodes[ni - 1];
        Vec3 outgoing = vec3_scale(r->ray.direction, -1);
        bool approximate =
            r->volume->config.g == 0 || (r->volume->config.phase_tolerance > 0 &&
                                         vec3_dot(reference_direction(r, n), outgoing) >
                                             r->angular_cos_limit + 64 * DBL_EPSILON);
        c->generation = r->generation;
        c->node = ni;
        c->first = 0;
        if (approximate) {
            c->total = n->cached_total;
            c->first = n->cached_first;
            r->cached_queries++;
        } else {
            c->total = exact_node(r, n, outgoing, &c->first);
            r->exact_queries++;
        }
    }
    *total = c->total;
    *first = c->first;
}
double spectral_reconstruction_source(RuntimeSpectralVolume *v, Vec3 p, int band, double *first) {
    SpectralReconstruction *r = v->reconstruction;
    uint16_t region = spectral_domain_region(&r->domain, p);
    if (!region)
        return 0;
    double total = 0;
    for (int level = 0; level <= (v->config.reconstruction == SPECTRAL_RECONSTRUCTION_ADAPTIVE);
         level++) {
        int id[3][2];
        double w[3][2];
        for (int a = 0; a < 3; a++)
            spectral_reconstruction_basis(v, axis(p, a), a, level, id[a], w[a]);
        for (int z = 0; z < 2; z++)
            for (int y = 0; y < 2; y++)
                for (int x = 0; x < 2; x++) {
                    uint64_t key = key_make(id[0][x], id[1][y], id[2][z], band, level, region);
                    uint32_t ni = r->slots[slot(r, key)];
                    if (!ni)
                        continue;
                    double sum, single, weight = w[0][x] * w[1][y] * w[2][z];
                    angular_node(r, ni, &sum, &single);
                    total += weight * sum;
                    *first += weight * single;
                }
    }
    return total;
}
double spectral_reconstruction_boundary(RuntimeSpectralVolume *v, Ray3D ray, double a, double b) {
    return v->reconstruction ? spectral_domain_next(&v->reconstruction->domain, ray, a, b) : b;
}
void spectral_reconstruction_stats(const RuntimeSpectralVolume *v, RuntimeSpectralMapStats *st) {
    const SpectralReconstruction *r = v->reconstruction;
    if (!r)
        return;
    st->reconstruction_bytes =
        r->owned_bytes +
        (size_t)v->capacity * (sizeof(SpectralCollision) + (v->positions ? sizeof(Vec3) : 0)) +
        (v->heads ? 65536 * sizeof(int) : 0);
    st->reconstruction_peak_bytes = r->peak_bytes;
    st->reconstruction_nodes = r->node_count;
    st->reconstruction_links = r->link_count;
    st->reconstruction_cached_queries = r->cached_queries;
    st->reconstruction_exact_queries = r->exact_queries;
    st->reconstruction_regions = r->domain.region_count;
}

bool spectral_reconstruction_accepting(const RuntimeSpectralVolume *v) {
    return !v->reconstruction || !v->reconstruction->built;
}
