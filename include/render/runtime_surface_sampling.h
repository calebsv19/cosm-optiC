#pragma once
#include "render/runtime_material_payload_3d.h"
/* Immutable resources prepared with the retained scene. No shading-time IO. */
bool RuntimeSurfaceSamplingActive(int index);
bool RuntimeSurfaceSamplingResolve(const HitInfo3D *hit, RuntimeMaterialPayload3D *payload);
bool RuntimeSurfaceSamplingApply(const HitInfo3D *hit, RuntimeMaterialPayload3D *payload);
bool RuntimeSurfaceSamplingCoordinates(const HitInfo3D *hit, double uv[2], double dx[2],
                                       double dy[2], Vec3 *du, Vec3 *dv);
unsigned long long RuntimeSurfaceSamplingBuildCount(void);
size_t RuntimeSurfaceSamplingPreparedBytes(void);

/* Application-owned immutable cache. Bytes count unique resident pyramids;
 * live bytes include all active, validating and staged generation references.
 * Temporary decoder/build scratch is not resident cache storage. */
typedef struct RuntimeSurfaceSamplingCacheStats {
    unsigned long long cache_hits, decodes, image_builds, program_builds, evictions;
    size_t bytes, live_bytes, entries, live_entries, budget_bytes;
} RuntimeSurfaceSamplingCacheStats;
void RuntimeSurfaceSamplingGetCacheStats(RuntimeSurfaceSamplingCacheStats *out);
void RuntimeSurfaceSamplingResetCacheStats(void);
void RuntimeSurfaceSamplingPurgeUnusedResources(void);
/* Test controls never evict referenced resources; reset requires no live resources. */
bool RuntimeSurfaceSamplingSetCacheBudgetForTests(size_t bytes);
bool RuntimeSurfaceSamplingCacheResetForTests(void);
/* Explicit scene-file context for relative resources. NULL clears the context.
 * File entry points scope this automatically; JSON-only callers must opt in.
 * Relative channels cannot escape the scene's parent directory. */
bool RuntimeSurfaceSamplingSetScenePathContext(const char *scene_path);
const char *RuntimeSurfaceSamplingScenePathContext(void);
