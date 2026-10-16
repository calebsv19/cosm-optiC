#include "render/runtime_path_termination_probe_3d.h"
#include "render/runtime_native_3d_pixel_probe.h"
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static _Thread_local RuntimePathTermination3DStats *observer;
static _Thread_local struct {
    RuntimePathTermination3DStats stats;
    RuntimePathTermination3DStats *previous;
    bool active;
    int x, y;
    unsigned sequence, subpass;
} probe;
static pthread_once_t once = PTHREAD_ONCE_INIT;
static pthread_mutex_t output = PTHREAD_MUTEX_INITIALIZER;
static bool enabled;
static const char *names[RUNTIME_PATH_EXIT_COUNT] = {
    "glass_start", "environment", "emitter", "receiver", "diffuse_depth",
    "specular_depth", "transmission_depth", "boundary_safety", "invalid_medium",
    "invalid_attenuation", "sample_rejected", "geometric_side", "medium_transition",
    "receiver_failed", "roulette_terminated", "roulette_survived", "continuation_safety",
    "glass_roulette_terminated", "glass_roulette_survived"
};
static void initialize(void) {
    const char *value = getenv("RAY_TRACING_PATH_TERMINATION_PROBE");
    enabled = value && strcmp(value, "1") == 0;
}
RuntimePathTermination3DStats *
RuntimePathTermination3D_SetObserver(RuntimePathTermination3DStats *next) {
    RuntimePathTermination3DStats *previous = observer;
    observer = next;
    return previous;
}
void RuntimePathTermination3D_Record(RuntimePathTermination3DReason reason, int depth, Vec3 weight) {
    if (!observer || reason < 0 || reason >= RUNTIME_PATH_EXIT_COUNT) return;
    ++observer->counts[reason];
    if (isfinite(weight.x) && isfinite(weight.y) && isfinite(weight.z))
        observer->weights[reason] = vec3_add(observer->weights[reason], weight);
    if (depth < 0) depth = 0;
    if (depth > observer->maxDepth) observer->maxDepth = depth;
    ++observer->depthHistogram[depth < 129 ? depth : 129];
}
void RuntimePathTermination3D_Begin(int x, int y, const RuntimeNative3DSamplingContext *sampling) {
    pthread_once(&once, initialize);
    if (!enabled || !RuntimeNative3DPixelProbe_IsSelected(x, y)) return;
    memset(&probe.stats, 0, sizeof(probe.stats));
    probe.x = x; probe.y = y;
    probe.sequence = sampling ? sampling->sampleSequence : 0;
    probe.subpass = sampling ? sampling->temporalSubpassIndex : 0;
    probe.previous = RuntimePathTermination3D_SetObserver(&probe.stats);
    probe.active = true;
}
void RuntimePathTermination3D_End(void) {
    if (!probe.active) return;
    RuntimePathTermination3D_SetObserver(probe.previous);
    probe.active = false;
    pthread_mutex_lock(&output);
    flockfile(stderr);
    fprintf(stderr, "PATH_TERMINATION {\"x\":%d,\"y\":%d,\"sequence\":%u,\"subpass\":%u,\"max_depth\":%d,\"events\":{",
            probe.x, probe.y, probe.sequence, probe.subpass, probe.stats.maxDepth);
    bool comma = false;
    for (int i = 0; i < RUNTIME_PATH_EXIT_COUNT; ++i) {
        if (!probe.stats.counts[i]) continue;
        Vec3 w = probe.stats.weights[i];
        fprintf(stderr, "%s\"%s\":{\"count\":%llu,\"weight_sum\":[%.17g,%.17g,%.17g]}",
                comma ? "," : "", names[i], (unsigned long long)probe.stats.counts[i], w.x, w.y, w.z);
        comma = true;
    }
    fputs("},\"depth_histogram\":{", stderr);
    comma = false;
    for (int i = 0; i < 130; ++i) {
        if (!probe.stats.depthHistogram[i]) continue;
        fprintf(stderr, "%s\"%d\":%llu", comma ? "," : "", i,
                (unsigned long long)probe.stats.depthHistogram[i]);
        comma = true;
    }
    fputs("}}\n", stderr);
    funlockfile(stderr);
    pthread_mutex_unlock(&output);
}
