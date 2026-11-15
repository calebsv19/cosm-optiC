#pragma once
#include "runtime_spectral_volume_internal.h"
#include <stddef.h>
#include <stdint.h>

typedef struct SpectralReconstruction SpectralReconstruction;
#include "runtime_spectral_reconstruction_mesh.h"

/* Serial render-context-owned state. Never shared by simultaneous camera
 * workers. */
typedef struct {
    double *knot;
    unsigned char *wall;
    int count;
} SpectralDomainAxis;
typedef struct {
    Vec3 lo, hi;
    bool rectangular;
} SpectralDomainRegion;
typedef struct {
    SpectralDomainAxis axis[3];
    SpectralMeshBoundary mesh;
    uint16_t *labels;
    SpectralDomainRegion *regions;
    size_t cells;
    int region_count;
    bool rectangular_lookup;
} SpectralDomain;
typedef struct {
    uint64_t key;
    /* Normalization is needed only while building. Cache coordinates replace it
     * after freeze, keeping the extra per-node storage bounded. */
    union {
        double norm;
        uint64_t angular_key;
    };
    double cached_total, cached_first;
    uint32_t head, angular_ready;
} SpectralReconstructionNode;
typedef struct {
    double flux;
    uint32_t photon, next;
} SpectralReconstructionLink;
typedef struct {
    uint64_t generation;
    uint32_t node;
    double total, first;
} SpectralAngularCache;
enum { SPECTRAL_SPATIAL_CACHE_SIZE = 8192 };
typedef struct {
    uint64_t key;
    uint32_t nodes[8];
} SpectralSpatialCache;
/* Ordered exact-position geometry only; band and angular values are not cached.
 * The owning reconstruction is immutable after build and owns its lifetime. */
enum { SPECTRAL_SOURCE_CACHE_SIZE = 1024 };
typedef struct {
    uint64_t cell_key;
    double weights[8];
} SpectralSourceGroup;
typedef struct {
    Vec3 position;
    bool valid;
    int count;
    SpectralSourceGroup groups[2];
} SpectralSourceStencil;
struct SpectralReconstruction {
    RuntimeSpectralVolume *volume;
    SpectralDomain domain;
    SpectralReconstructionNode *nodes;
    SpectralReconstructionLink *links;
    uint32_t *slots;
    size_t node_count, node_capacity, link_count, link_capacity, slot_capacity;
    size_t owned_bytes, peak_bytes, budget_bytes;
    uint64_t normalization_visits, cached_queries, exact_queries, generation;
    uint64_t spatial_hits, spatial_misses;
    void *mesh_quadrature;
    size_t mesh_quadrature_bytes;
    struct {
        uint64_t key;
        double value;
    } mesh_norm_cache[2048];
    SpectralSourceStencil *source_cache;
    bool source_cache_attempted;
    uint64_t source_hits, source_misses, source_overflows;
    size_t source_cursor, source_cached_count;
    bool built, view_ready, ray_ready;
    Vec3 phase_direction;
    uint64_t phase_key;
    Ray3D ray;
    double angular_cos_limit, angular_quantum;
    SpectralAngularCache angular[2048];
    SpectralSpatialCache spatial[SPECTRAL_SPATIAL_CACHE_SIZE];
    struct SpectralKernelState *kernel;
    struct SpectralSupportCache *support_cache;
    size_t support_cache_size;
    struct SpectralSupportCell *support_cells;
    uint32_t *support_next;
    size_t support_cell_capacity;
    void *support_bitmap[2];
    size_t support_bitmap_dims[2][3];
    unsigned support_bitmap_word_bytes;
    double *support_flux;
    uint8_t *support_levels;
    char error[192];
};
static inline double spectral_reconstruction_spacing(const RuntimeSpectralVolume *v) {
    return v->config.reconstruction_cell_m > 0 ? v->config.reconstruction_cell_m : v->config.cell_m;
}
void *spectral_reconstruction_resize(SpectralReconstruction *, void *, size_t, size_t);
bool spectral_reconstruction_fail(SpectralReconstruction *, const char *);
bool spectral_domain_build(SpectralReconstruction *, const RuntimeScene3D *);
void spectral_domain_free(SpectralDomain *);
uint16_t spectral_domain_region(const SpectralDomain *, Vec3);
double spectral_domain_next(const SpectralDomain *, Ray3D, double, double);
double spectral_domain_norm(SpectralReconstruction *, const int[3], int, uint16_t);
double spectral_domain_part_norm(SpectralReconstruction *, const int[3], int,
                                 uint16_t, int);
void spectral_reconstruction_basis(const RuntimeSpectralVolume *, double, int, int, int[2],
                                   double[2]);
double spectral_reconstruction_hat_integral(double, double, double, double, double);

double spectral_domain_tent_norm(SpectralReconstruction *, const double[3], const double[3], const double[3], uint16_t, int);
