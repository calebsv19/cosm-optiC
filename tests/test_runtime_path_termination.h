#pragma once
#include "render/runtime_path_termination_probe_3d.h"
#include "render/runtime_path_depth_policy_3d.h"
#include "config/config_manager.h"
#include <pthread.h>

static void *runtime_path_termination_thread(void *argument) {
    RuntimePathTermination3DStats *stats = argument;
    RuntimePathTermination3DStats *old = RuntimePathTermination3D_SetObserver(stats);
    RuntimePathTermination3D_Record(RUNTIME_PATH_EXIT_TRANSMISSION_DEPTH, 64, vec3(.2, .3, .4));
    RuntimePathTermination3D_SetObserver(old);
    return NULL;
}
static void test_runtime_path_termination(void) {
    int diffuse = animSettings.bounceDepth3D, specular = animSettings.specularDepth3D;
    int transmission = animSettings.transmissionDepth3D;
    const int depths[] = {8, 12, 24, 32, 48, 64};
    for (unsigned i = 0; i < sizeof(depths)/sizeof(depths[0]); ++i) {
        int d = depths[i];
        animSettings.bounceDepth3D = animSettings.specularDepth3D = animSettings.transmissionDepth3D = d;
        RuntimePathDepthPolicy3D policy = RuntimePathDepthPolicy3D_Resolve();
        assert_true("path_deep_policy_retains_request", policy.diffuseDepth == d &&
                    policy.specularDepth == d && policy.transmissionDepth == d);
        assert_true("path_deep_execution_not_history_limited",
                    RuntimePathDepthPolicy3D_ContinuationBudget(&policy) == (d > 8 ? 64 : 8));
        for (int lobe = 1; lobe <= 3; ++lobe) {
            assert_true("path_deep_policy_last_allowed", RuntimePathDepthPolicy3D_AllowsDepth(&policy, lobe, d));
            assert_true("path_deep_policy_first_refused", !RuntimePathDepthPolicy3D_AllowsDepth(&policy, lobe, d+1));
        }
    }
    animSettings.bounceDepth3D = animSettings.specularDepth3D = animSettings.transmissionDepth3D = 999;
    RuntimePathDepthPolicy3D policy = RuntimePathDepthPolicy3D_Resolve();
    assert_true("path_deep_policy_safety_clamp", policy.diffuseDepth == 64 && policy.specularDepth == 64 && policy.transmissionDepth == 64);
    animSettings.bounceDepth3D = diffuse; animSettings.specularDepth3D = specular;
    animSettings.transmissionDepth3D = transmission;
    assert_true("path_deep_defaults_preserved", RUNTIME_3D_BOUNCE_DEPTH_DEFAULT == 3 &&
                RUNTIME_3D_SPECULAR_DEPTH_DEFAULT == 3 && RUNTIME_3D_TRANSMISSION_DEPTH_DEFAULT == 8);

    RuntimePathTermination3DStats parent = {0}, child = {0}, threaded = {0};
    RuntimePathTermination3DStats *old = RuntimePathTermination3D_SetObserver(&parent);
    RuntimePathTermination3D_Record(RUNTIME_PATH_EXIT_SPECULAR_DEPTH, 8, vec3(.1, .2, .3));
    pthread_t thread;
    int started = pthread_create(&thread, NULL, runtime_path_termination_thread, &threaded);
    assert_true("path_observer_thread_start", started == 0);
    if (!started) pthread_join(thread, NULL);
    RuntimePathTermination3DStats *saved = RuntimePathTermination3D_SetObserver(&child);
    RuntimePathTermination3D_Record(RUNTIME_PATH_EXIT_ENVIRONMENT, 32, vec3(1, 1, 1));
    RuntimePathTermination3D_SetObserver(saved);
    RuntimePathTermination3D_Record(RUNTIME_PATH_EXIT_RECEIVER, 12, vec3(.5, .5, .5));
    RuntimePathTermination3D_SetObserver(old);
    assert_true("path_observer_scope_and_thread_isolation",
                parent.counts[RUNTIME_PATH_EXIT_SPECULAR_DEPTH] == 1 &&
                parent.counts[RUNTIME_PATH_EXIT_RECEIVER] == 1 &&
                parent.counts[RUNTIME_PATH_EXIT_ENVIRONMENT] == 0 &&
                parent.counts[RUNTIME_PATH_EXIT_TRANSMISSION_DEPTH] == 0 &&
                child.counts[RUNTIME_PATH_EXIT_ENVIRONMENT] == 1 &&
                threaded.counts[RUNTIME_PATH_EXIT_TRANSMISSION_DEPTH] == 1 && threaded.maxDepth == 64);
    assert_close("path_observer_weight_is_local_proxy", parent.weights[RUNTIME_PATH_EXIT_SPECULAR_DEPTH].y, .2, 1e-15);
    assert_true("path_observer_depth_histogram", parent.depthHistogram[8] == 1 && parent.depthHistogram[12] == 1);
}
