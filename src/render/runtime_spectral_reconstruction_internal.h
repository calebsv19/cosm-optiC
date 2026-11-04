#pragma once
#include "runtime_spectral_volume_internal.h"
#include <stddef.h>
#include <stdint.h>

/* Serial render-context-owned state. Never shared by simultaneous camera workers. */
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
    uint16_t *labels;
    SpectralDomainRegion *regions;
    size_t cells;
    int region_count;
} SpectralDomain;
typedef struct {
    uint64_t key;
    double norm, cached_total, cached_first;
    uint32_t head;
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
struct SpectralReconstruction {
    RuntimeSpectralVolume *volume;
    SpectralDomain domain;
    SpectralReconstructionNode *nodes;
    SpectralReconstructionLink *links;
    uint32_t *slots;
    size_t node_count, node_capacity, link_count, link_capacity, slot_capacity;
    size_t owned_bytes, peak_bytes, budget_bytes;
    uint64_t normalization_visits, cached_queries, exact_queries, generation;
    bool built, view_ready, ray_ready;
    Vec3 view_origin;
    Ray3D ray;
    double angular_cos_limit;
    SpectralAngularCache angular[2048];
    char error[192];
};
void *spectral_reconstruction_resize(SpectralReconstruction *, void *, size_t, size_t);
bool spectral_reconstruction_fail(SpectralReconstruction *, const char *);
bool spectral_domain_build(SpectralReconstruction *, const RuntimeScene3D *);
void spectral_domain_free(SpectralDomain *);
uint16_t spectral_domain_region(const SpectralDomain *, Vec3);
double spectral_domain_next(const SpectralDomain *, Ray3D, double, double);
double spectral_domain_norm(SpectralReconstruction *, const int[3], int, uint16_t);
void spectral_reconstruction_basis(const RuntimeSpectralVolume *, double, int, int, int[2],
                                   double[2]);
double spectral_reconstruction_hat_integral(double, double, double, double, double);
