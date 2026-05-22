#include "render/runtime_path_depth_policy_3d.h"

#include "config/config_manager.h"

static int runtime_path_depth_policy_3d_clamp(int value, int min_value, int max_value) {
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
                                           RUNTIME_3D_TRANSMISSION_DEPTH_MIN,
                                           RUNTIME_3D_TRANSMISSION_DEPTH_MAX);
    return policy;
}
