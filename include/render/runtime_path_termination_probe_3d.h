#ifndef RUNTIME_PATH_TERMINATION_PROBE_3D_H
#define RUNTIME_PATH_TERMINATION_PROBE_3D_H
#include "render/runtime_native_3d_sampling.h"
#include "math/vec3.h"
#include <stdint.h>

typedef enum {
    RUNTIME_PATH_EXIT_GLASS_START,
    RUNTIME_PATH_EXIT_ENVIRONMENT,
    RUNTIME_PATH_EXIT_EMITTER,
    RUNTIME_PATH_EXIT_RECEIVER,
    RUNTIME_PATH_EXIT_DIFFUSE_DEPTH,
    RUNTIME_PATH_EXIT_SPECULAR_DEPTH,
    RUNTIME_PATH_EXIT_TRANSMISSION_DEPTH,
    RUNTIME_PATH_EXIT_BOUNDARY_SAFETY,
    RUNTIME_PATH_EXIT_INVALID_MEDIUM,
    RUNTIME_PATH_EXIT_INVALID_ATTENUATION,
    RUNTIME_PATH_EXIT_SAMPLE_REJECTED,
    RUNTIME_PATH_EXIT_GEOMETRIC_SIDE,
    RUNTIME_PATH_EXIT_MEDIUM_TRANSITION,
    RUNTIME_PATH_EXIT_RECEIVER_FAILED,
    RUNTIME_PATH_EXIT_ROULETTE_TERMINATED,
    RUNTIME_PATH_EXIT_ROULETTE_SURVIVED,
    RUNTIME_PATH_EXIT_CONTINUATION_SAFETY,
    RUNTIME_PATH_EXIT_GLASS_ROULETTE_TERMINATED,
    RUNTIME_PATH_EXIT_GLASS_ROULETTE_SURVIVED,
    RUNTIME_PATH_EXIT_COUNT
} RuntimePathTermination3DReason;

/* Fixed-size, caller-owned observer. Weight sums are local transport weights,
 * not missing radiance/flux. Events include nested glass walks and receivers. */
typedef struct {
    uint64_t counts[RUNTIME_PATH_EXIT_COUNT];
    Vec3 weights[RUNTIME_PATH_EXIT_COUNT];
    uint64_t depthHistogram[130]; /* 129 includes overflow */
    int maxDepth;
} RuntimePathTermination3DStats;
RuntimePathTermination3DStats *
RuntimePathTermination3D_SetObserver(RuntimePathTermination3DStats *observer);
void RuntimePathTermination3D_Record(RuntimePathTermination3DReason reason, int depth, Vec3 weight);
/* Opt-in selected-pixel JSONL, scoped to one primary shading call/thread. */
void RuntimePathTermination3D_Begin(int x, int y, const RuntimeNative3DSamplingContext *sampling);
void RuntimePathTermination3D_End(void);
#endif
