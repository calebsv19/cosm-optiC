#include "render/runtime_path_depth_policy_3d.h"

#include "config/config_manager.h"

static const int kRuntimePathDepthPolicy3DMinDepthBeforeRoulette = 2;

static int runtime_path_depth_policy_3d_clamp(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double runtime_path_depth_policy_3d_clamp_double(double value,
                                                        double min_value,
                                                        double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

RuntimePathDepthPolicy3D RuntimePathDepthPolicy3D_Resolve(void) {
    RuntimePathDepthPolicy3D policy = {0};

    policy.diffuseDepth = runtime_path_depth_policy_3d_clamp(animSettings.bounceDepth3D,
                                                             RUNTIME_3D_BOUNCE_DEPTH_MIN,
                                                             RUNTIME_3D_BOUNCE_DEPTH_MAX);
    policy.specularDepth = runtime_path_depth_policy_3d_clamp(animSettings.specularDepth3D,
                                                              RUNTIME_3D_SPECULAR_DEPTH_MIN,
                                                              RUNTIME_3D_SPECULAR_DEPTH_MAX);
    policy.transmissionDepth =
        runtime_path_depth_policy_3d_clamp(animSettings.transmissionDepth3D,
                                           0,
                                           RUNTIME_3D_TRANSMISSION_DEPTH_MAX);
    policy.minDepthBeforeRoulette = kRuntimePathDepthPolicy3DMinDepthBeforeRoulette;
    policy.rouletteThreshold =
        runtime_path_depth_policy_3d_clamp_double(animSettings.rouletteThreshold3D,
                                                  RUNTIME_3D_ROULETTE_THRESHOLD_MIN,
                                                  RUNTIME_3D_ROULETTE_THRESHOLD_MAX);
    return policy;
}

int RuntimePathDepthPolicy3D_MaxDepthForLobe(const RuntimePathDepthPolicy3D* policy,
                                             RuntimePathDepthPolicy3DLobe lobe) {
    RuntimePathDepthPolicy3D resolved = {0};
    if (!policy) {
        resolved = RuntimePathDepthPolicy3D_Resolve();
        policy = &resolved;
    }
    switch (lobe) {
        case RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_SPECULAR:
            return policy->specularDepth;
        case RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_TRANSMISSION:
            return policy->transmissionDepth;
        case RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_DIFFUSE:
        default:
            return policy->diffuseDepth;
    }
}

bool RuntimePathDepthPolicy3D_AllowsDepth(const RuntimePathDepthPolicy3D* policy,
                                          RuntimePathDepthPolicy3DLobe lobe,
                                          int depth) {
    if (depth < 1) return false;
    return depth <= RuntimePathDepthPolicy3D_MaxDepthForLobe(policy, lobe);
}

double RuntimePathDepthPolicy3D_SurvivalProbability(const RuntimePathDepthPolicy3D* policy,
                                                    int depth,
                                                    double throughput_luma) {
    RuntimePathDepthPolicy3D resolved = {0};
    double threshold = 0.0;

    if (!policy) {
        resolved = RuntimePathDepthPolicy3D_Resolve();
        policy = &resolved;
    }
    threshold = policy->rouletteThreshold;
    if (!(threshold > 0.0) || depth < policy->minDepthBeforeRoulette) {
        return 1.0;
    }
    if (!(throughput_luma < threshold)) {
        return 1.0;
    }
    return runtime_path_depth_policy_3d_clamp_double(throughput_luma / threshold, 1e-6, 1.0);
}

bool RuntimePathDepthPolicy3D_ShouldTerminate(const RuntimePathDepthPolicy3D* policy,
                                              int depth,
                                              double throughput_luma,
                                              double roulette_sample,
                                              double* out_survival_probability) {
    const double survival =
        RuntimePathDepthPolicy3D_SurvivalProbability(policy, depth, throughput_luma);
    if (out_survival_probability) {
        *out_survival_probability = survival;
    }
    return roulette_sample > survival;
}
