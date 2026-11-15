#include "runtime_spectral_reconstruction_internal.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void kernel_free(SpectralReconstruction *);
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
    kernel_free(r);
    spectral_domain_free(&r->domain);
    free(r->source_cache);
    free(r->support_cache);
    free(r->support_cells);
    free(r->support_next);
    free(r->support_bitmap[0]);
    free(r->support_bitmap[1]);
    free(r->support_flux);
    free(r->support_levels);
    free(r->mesh_quadrature);
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
    if (v->config.reconstruction_cell_m > 0 && v->config.reconstruction_cell_m != v->config.cell_m && r->domain.mesh.count) {
        snprintf(diag, size, "independent reconstruction spacing currently requires no intersecting mesh");
        return false;
    }
    if (v->config.reconstruction_cell_m > 0 && v->config.reconstruction_cell_m != v->config.cell_m)
        for (int i=1;i<=r->domain.region_count;i++)
            if (!r->domain.regions[i].rectangular) {
                snprintf(diag,size,"independent support requires rectangular occupied components");
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
    double lo = axis(v->lo, a), hi = axis(v->hi, a), h = spectral_reconstruction_spacing(v) * (level ? 2 : 1);
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
#include "runtime_spectral_reconstruction_support.h"
#include "runtime_spectral_reconstruction_kernel.h"
bool spectral_reconstruction_build(RuntimeSpectralVolume *v) {
    SpectralReconstruction *r = v->reconstruction;
    if (!r)
        return !v->config.reconstruction;
    if (r->built)
        return true;
    if (v->config.reconstruction==SPECTRAL_RECONSTRUCTION_KERNEL) return kernel_build(v);
    if (support_independent(v)) {
        if (!support_prepare(v)) return false;
        if (!v->count) { r->built=true;return true; }
        size_t transport=(size_t)v->capacity*(sizeof(SpectralCollision)+sizeof(Vec3))+
                         (v->heads?65536*sizeof(int):0);
        size_t available=r->owned_bytes+transport<r->budget_bytes
                             ? r->budget_bytes-r->owned_bytes-transport : 0;
        size_t geometry_bytes=SPECTRAL_SOURCE_CACHE_SIZE*sizeof(SpectralSourceStencil);
        if (!v->config.disable_camera_cache && available>geometry_bytes) available-=geometry_bytes;
        r->support_cache_size=SUPPORT_CACHE_SIZE;
        while (r->support_cache_size>1024 &&
               r->support_cache_size*sizeof(*r->support_cache)>available)
            r->support_cache_size/=2;
        size_t bytes=r->support_cache_size*sizeof(*r->support_cache);
        r->support_cache=spectral_reconstruction_resize(r,NULL,0,bytes);
        if (!r->support_cache) return false;
        r->built=true;
        return true;
    }
    if (!grow_slots(r))
        return false;
    for (int i = 0; i < v->count; i++) {
        const SpectralCollision *p = &v->points[i];
        uint16_t region = spectral_domain_region(&r->domain, v->positions[i]);
        if (!region)
            return spectral_reconstruction_fail(
                r, "scattering event outside admitted occupied component");
        double face_weights[SPECTRAL_MESH_PARTS];
        if (!spectral_mesh_weights(&r->domain.mesh, v->positions[i],
                                   face_weights))
            return spectral_reconstruction_fail(
                r, "scattering event inside convex mesh obstacle");
        int level = level_for(v, p), ids[3][2];
        double w[3][2];
        for (int a = 0; a < 3; a++)
            spectral_reconstruction_basis(v, axis(v->positions[i], a), a, level, ids[a], w[a]);
        for (int z = 0; z < 2; z++)
            for (int y = 0; y < 2; y++)
                for (int x = 0; x < 2; x++) {
                    if (!(w[0][x] * w[1][y] * w[2][z] > 0))
                        continue;
                    int at[] = {ids[0][x], ids[1][y], ids[2][z]};
                    unsigned parts = spectral_mesh_node_parts(v, at, level);
                    for (int part = 0; part <= r->domain.mesh.count; part++) {
                        if (!(parts & (1u << part)) ||
                            !(face_weights[part] > 0))
                            continue;
                        if ((r->node_count + 1) * 2 > r->slot_capacity &&
                            !grow_slots(r))
                            return false;
                        uint64_t key =
                            key_make(ids[0][x], ids[1][y], ids[2][z], p->band,
                                     level, region + part * 6553);
                        size_t s = slot(r, key);
                        uint32_t ni = r->slots[s];
                        if (!ni) {
                            if (r->node_count == r->node_capacity) {
                                size_t next = r->node_capacity
                                                  ? r->node_capacity * 2
                                                  : 4096;
                                void *q = spectral_reconstruction_resize(
                                    r, r->nodes,
                                    r->node_capacity * sizeof(*r->nodes),
                                    next * sizeof(*r->nodes));
                                if (!q)
                                    return false;
                                r->nodes = q;
                                r->node_capacity = next;
                            }
                            uint64_t norm_key =
                                key_make(at[0], at[1], at[2], 0, level,
                                         region + part * 6553);
                            size_t norm_slot = hash(norm_key) & 2047;
                            double norm;
                            if (part &&
                                r->mesh_norm_cache[norm_slot].key == norm_key)
                                norm = r->mesh_norm_cache[norm_slot].value;
                            else {
                                norm = spectral_domain_part_norm(r, at, level,
                                                                 region, part);
                                if (part) {
                                    r->mesh_norm_cache[norm_slot].key =
                                        norm_key;
                                    r->mesh_norm_cache[norm_slot].value = norm;
                                }
                            }
                            if (r->error[0])
                                return false;
                            if (!(norm > 0) || !isfinite(norm))
                                return spectral_reconstruction_fail(
                                    r, "continuous node has invalid "
                                       "occupied-domain normalization");
                            ni = (uint32_t)++r->node_count;
                            r->slots[s] = ni;
                            r->nodes[ni - 1] = (SpectralReconstructionNode){
                                .key = key, .norm = norm};
                        }
                        if (r->link_count == r->link_capacity) {
                            size_t next =
                                r->link_capacity ? r->link_capacity * 2 : 4096;
                            void *q = spectral_reconstruction_resize(
                                r, r->links,
                                r->link_capacity * sizeof(*r->links),
                                next * sizeof(*r->links));
                            if (!q)
                                return false;
                            r->links = q;
                            r->link_capacity = next;
                        }
                        SpectralReconstructionNode *node = &r->nodes[ni - 1];
                        r->links[r->link_count] = (SpectralReconstructionLink){
                            .flux = p->watts * w[0][x] * w[1][y] * w[2][z] *
                                    face_weights[part] / node->norm,
                            .photon = (uint32_t)i,
                            .next = node->head};
                        node->head = (uint32_t)++r->link_count;
                    }
                }
    }
    free(v->positions);
    v->positions = NULL;
    free(r->mesh_quadrature);
    r->mesh_quadrature = NULL;
    r->owned_bytes -= r->mesh_quadrature_bytes;
    r->mesh_quadrature_bytes = 0;
    r->built = true;
    return true;
}
static double phase(double mu, double g) {
    double d = 1 + g * g - 2 * g * fmax(-1, fmin(1, mu));
    return (1 - g * g) / (4 * M_PI * d * sqrt(d));
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
    /* Camera bands begin the same sample sequence. Exact-position checks also
     * handle different optical-depth subdivision and truncated segments. */
    r->source_cursor = 0;
    if (!r->view_ready) {
        /* |d log HG / d theta| <= 3|g|/(1-g^2), for every incoming direction. */
        double g = fabs(v->config.g);
        double angle = g ? log1p(v->config.phase_tolerance) * (1 - g * g) / (3 * g) : M_PI;
        r->angular_cos_limit = cos(fmin(M_PI, angle));
        r->angular_quantum = 2 * sin(fmin(M_PI, angle) * .5) / sqrt(3.0);
        r->view_ready = true;
    }
    if (!r->ray_ready || memcmp(&ray.origin, &r->ray.origin, sizeof(Vec3)) ||
        memcmp(&ray.direction, &r->ray.direction, sizeof(Vec3))) {
        r->ray = ray;
        r->ray_ready = true;
        r->source_cached_count = 0;
        Vec3 outgoing = vec3_scale(ray.direction, -1);
        r->phase_direction = outgoing;
        r->phase_key = 0;
        if (v->config.g == 0) {
            r->phase_direction = vec3(0, 0, 1);
            r->phase_key = 1;
        } else if (v->config.phase_tolerance > 0 && r->angular_quantum > 1e-6) {
            double h = r->angular_quantum;
            int q[3] = {(int)floor(outgoing.x / h + .5), (int)floor(outgoing.y / h + .5),
                        (int)floor(outgoing.z / h + .5)};
            Vec3 target = vec3_normalize(vec3(q[0] * h, q[1] * h, q[2] * h));
            /* A fixed direction cell makes values query-order independent.
             * Check the same HG bound after rounding; otherwise use the actual
             * direction. The quantum floor bounds each signed key to 21 bits. */
            if ((q[0] || q[1] || q[2]) &&
                vec3_dot(target, outgoing) > r->angular_cos_limit + 64 * DBL_EPSILON) {
                r->phase_direction = target;
                r->phase_key = 1 + ((uint64_t)(q[0] + 1048575) << 42 |
                                    (uint64_t)(q[1] + 1048575) << 21 | (uint64_t)(q[2] + 1048575));
            }
        }
        if (++r->generation == 0) {
            memset(r->angular, 0, sizeof(r->angular));
            if (r->support_cache)
                memset(r->support_cache,0,r->support_cache_size*sizeof(*r->support_cache));
            kernel_reset_phases(r);
            r->generation = 1;
        }
    }
    return true;
}
static void angular_node(SpectralReconstruction *r, uint32_t ni, double *total, double *first) {
    SpectralAngularCache *c = &r->angular[hash(ni) & 2047];
    if (c->generation != r->generation || c->node != ni) {
        SpectralReconstructionNode *n = &r->nodes[ni - 1];
        bool reuse = r->phase_key && n->angular_ready && n->angular_key == r->phase_key;
        c->generation = r->generation;
        c->node = ni;
        if (reuse)
            r->cached_queries++;
        else {
            n->cached_first = 0;
            n->cached_total = exact_node(r, n, r->phase_direction, &n->cached_first);
            n->angular_key = r->phase_key;
            n->angular_ready = 1;
            r->exact_queries++;
        }
        c->total = n->cached_total;
        c->first = n->cached_first;
    }
    *total = c->total;
    *first = c->first;
}
static double source_reference(RuntimeSpectralVolume *v, Vec3 p, int band, double *first) {
    SpectralReconstruction *r = v->reconstruction;
    uint16_t region = spectral_domain_region(&r->domain, p);
    if (!region)
        return 0;
    double face_weights[SPECTRAL_MESH_PARTS];
    if (!spectral_mesh_weights(&r->domain.mesh, p, face_weights))
        return 0;
    double total = 0;
    for (int level = 0; level <= (v->config.reconstruction == SPECTRAL_RECONSTRUCTION_ADAPTIVE);
         level++) {
        int id[3][2];
        double w[3][2];
        for (int a = 0; a < 3; a++)
            spectral_reconstruction_basis(v, axis(p, a), a, level, id[a], w[a]);
        double h = v->config.cell_m * (level ? 2 : 1);
        const SpectralMeshBoundary *mesh = &r->domain.mesh;
        int last = mesh->count;
        if (p.x < mesh->lo.x - 2 * h || p.x > mesh->hi.x + 2 * h ||
            p.y < mesh->lo.y - 2 * h || p.y > mesh->hi.y + 2 * h ||
            p.z < mesh->lo.z - 2 * h || p.z > mesh->hi.z + 2 * h)
            last = 0;
        for (int face = 0; face < last; face++) {
            SpectralMeshPlane plane = mesh->plane[face];
            double margin =
                2 * h * (fabs(plane.n.x) + fabs(plane.n.y) + fabs(plane.n.z));
            if (vec3_dot(plane.n, p) - plane.d > margin) {
                last = 0;
                break;
            }
        }
        for (int part = 0; part <= last; part++) {
            if (!(face_weights[part] > 0))
                continue;
            uint16_t key_region = region + part * 6553;
            uint64_t cell_key =
                key_make(id[0][0], id[1][0], id[2][0], band, level, key_region);
            SpectralSpatialCache *cache =
                &r->spatial[hash(cell_key) & (SPECTRAL_SPATIAL_CACHE_SIZE - 1)];
            if (cache->key != cell_key) {
                r->spatial_misses++;
                cache->key = cell_key;
                for (int z = 0; z < 2; z++)
                    for (int y = 0; y < 2; y++)
                        for (int x = 0; x < 2; x++) {
                            uint64_t key =
                                key_make(id[0][x], id[1][y], id[2][z], band,
                                         level, key_region);
                            cache->nodes[x + 2 * y + 4 * z] =
                                r->slots[slot(r, key)];
                        }
            } else
                r->spatial_hits++;
            for (int z = 0; z < 2; z++)
                for (int y = 0; y < 2; y++)
                    for (int x = 0; x < 2; x++) {
                        uint32_t ni = cache->nodes[x + 2 * y + 4 * z];
                        if (!ni)
                            continue;
                        double sum, single,
                            weight = w[0][x] * w[1][y] * w[2][z] *
                                     face_weights[part];
                        angular_node(r, ni, &sum, &single);
                        total += weight * sum;
                        *first += weight * single;
                    }
        }
    }
    return total;
}
static void source_prepare(RuntimeSpectralVolume *v, Vec3 p, SpectralSourceStencil *c) {
    c->position = p;
    c->valid = true;
    c->count = 0;
    SpectralReconstruction *r = v->reconstruction;
    uint16_t region = spectral_domain_region(&r->domain, p);
    if (!region)
        return;
    double face_weights[SPECTRAL_MESH_PARTS];
    if (!spectral_mesh_weights(&r->domain.mesh, p, face_weights))
        return;
    for (int level = 0; level <= (v->config.reconstruction == SPECTRAL_RECONSTRUCTION_ADAPTIVE);
         level++) {
        int id[3][2];
        double w[3][2];
        for (int a = 0; a < 3; a++)
            spectral_reconstruction_basis(v, axis(p, a), a, level, id[a], w[a]);
        double h = v->config.cell_m * (level ? 2 : 1);
        const SpectralMeshBoundary *mesh = &r->domain.mesh;
        int last = mesh->count;
        if (p.x < mesh->lo.x - 2 * h || p.x > mesh->hi.x + 2 * h ||
            p.y < mesh->lo.y - 2 * h || p.y > mesh->hi.y + 2 * h ||
            p.z < mesh->lo.z - 2 * h || p.z > mesh->hi.z + 2 * h)
            last = 0;
        for (int face = 0; face < last; face++) {
            SpectralMeshPlane plane = mesh->plane[face];
            double margin =
                2 * h * (fabs(plane.n.x) + fabs(plane.n.y) + fabs(plane.n.z));
            if (vec3_dot(plane.n, p) - plane.d > margin) {
                last = 0;
                break;
            }
        }
        /* Keep the common achromatic stencil compact. Mesh-boundary parts use
         * the retained scalar path rather than inflating every cache entry. */
        if (last) {
            c->count = -1;
            return;
        }
        for (int part = 0; part <= last; part++) {
            if (!(face_weights[part] > 0))
                continue;
            uint16_t key_region = region + part * 6553;
            SpectralSourceGroup *g = &c->groups[c->count++];
            g->cell_key = key_make(id[0][0], id[1][0], id[2][0], 0, level, key_region);
            for (int z = 0; z < 2; z++)
                for (int y = 0; y < 2; y++)
                    for (int x = 0; x < 2; x++) {
                        int i = x + 2 * y + 4 * z;
                        g->weights[i] = w[0][x] * w[1][y] * w[2][z] * face_weights[part];
                    }
        }
    }
}
/* Optional bounded storage must not turn a formerly admissible map into an
 * allocation failure. A missing cache uses the unchanged scalar reference. */
static bool source_cache_ready(SpectralReconstruction *r) {
    if (!r->source_cache_attempted) {
        r->source_cache_attempted = true;
        size_t bytes = SPECTRAL_SOURCE_CACHE_SIZE * sizeof(*r->source_cache);
        size_t transport = (size_t)r->volume->capacity *
                               (sizeof(SpectralCollision)+(r->volume->positions?sizeof(Vec3):0)) +
                           (r->volume->heads ? 65536 * sizeof(int) : 0);
        if (r->built && bytes <= r->budget_bytes &&
            r->owned_bytes + transport <= r->budget_bytes - bytes) {
            r->source_cache = calloc(SPECTRAL_SOURCE_CACHE_SIZE, sizeof(*r->source_cache));
            if (r->source_cache) {
                r->owned_bytes += bytes;
                r->peak_bytes = fmax(r->peak_bytes, r->owned_bytes + transport);
            }
        }
    }
    return r->source_cache != NULL;
}
double spectral_reconstruction_source(RuntimeSpectralVolume *v, Vec3 p, int band, double *first) {
    SpectralReconstruction *r = v->reconstruction;
    if (v->config.reconstruction==SPECTRAL_RECONSTRUCTION_KERNEL) return kernel_source(v,p,band,first);
    if (support_independent(v)) return support_source(v,p,band,first);
    if (v->config.disable_camera_cache || !source_cache_ready(r))
        return source_reference(v, p, band, first);
    if (r->source_cursor == SPECTRAL_SOURCE_CACHE_SIZE) {
        r->source_overflows++;
        return source_reference(v, p, band, first);
    }
    size_t index = r->source_cursor++;
    SpectralSourceStencil *c = &r->source_cache[index];
    if (index >= r->source_cached_count || memcmp(&c->position, &p, sizeof(p))) {
        r->source_misses++;
        source_prepare(v, p, c);
        if (index >= r->source_cached_count)
            r->source_cached_count = index + 1;
    } else
        r->source_hits++;
    if (c->count < 0)
        return source_reference(v, p, band, first);
    double total = 0;
    uint64_t band_key = (uint64_t)band << 36;
    for (int j = 0; j < c->count; j++) {
        const SpectralSourceGroup *g = &c->groups[j];
        uint64_t cell_key = g->cell_key + band_key;
        SpectralSpatialCache *cache = &r->spatial[hash(cell_key) & (SPECTRAL_SPATIAL_CACHE_SIZE - 1)];
        if (cache->key != cell_key) {
            r->spatial_misses++;
            cache->key = cell_key;
            for (int i = 0; i < 8; i++) {
                uint64_t offset = ((uint64_t)(i & 1) << 24) |
                                  ((uint64_t)((i >> 1) & 1) << 12) | (uint64_t)(i >> 2);
                cache->nodes[i] = r->slots[slot(r, cell_key + offset)];
            }
        } else
            r->spatial_hits++;
        for (int i = 0; i < 8; i++) {
            uint32_t ni = cache->nodes[i];
            if (!ni)
                continue;
            double sum, single;
            angular_node(r, ni, &sum, &single);
            total += g->weights[i] * sum;
            *first += g->weights[i] * single;
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
    st->reconstruction_spatial_hits = r->spatial_hits;
    st->reconstruction_spatial_misses = r->spatial_misses;
}

bool spectral_reconstruction_accepting(const RuntimeSpectralVolume *v) {
    return !v->reconstruction || !v->reconstruction->built;
}
