#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_BUDGET_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_BUDGET_3D_H
#include <stdbool.h>
#include <stdint.h>

/* Explicit bounded surface-reference expansion; beam capacity is unchanged. */
#define RUNTIME_PHOTON_LEGACY_LIMIT UINT64_C(262144)
#define RUNTIME_PHOTON_SURFACE_LIMIT UINT64_C(1048576)
static inline uint64_t RuntimePhotonBudget3D_Limit(bool volume_enabled) {
    return volume_enabled ? RUNTIME_PHOTON_LEGACY_LIMIT : RUNTIME_PHOTON_SURFACE_LIMIT;
}
static inline uint64_t RuntimePhotonBudget3D_Effective(uint64_t requested, bool volume_enabled) {
    uint64_t limit=RuntimePhotonBudget3D_Limit(volume_enabled);
    return requested<limit ? requested : limit;
}
static inline uint64_t RuntimePhotonBudget3D_MapCapacity(uint64_t effective, uint64_t depth,
                                                       bool volume_enabled) {
    /* Keep the old allocation/output path at every previously supported budget.
     * Bound multiplication before doing it; support arbitrary caller integers. */
    uint64_t cap=volume_enabled || effective<=RUNTIME_PHOTON_LEGACY_LIMIT ?
        RUNTIME_PHOTON_LEGACY_LIMIT : RUNTIME_PHOTON_SURFACE_LIMIT;
    if(depth==0)depth=1;
    return effective>cap/depth ? cap : effective*depth;
}
#endif
