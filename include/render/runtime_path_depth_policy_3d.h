#ifndef RENDER_RUNTIME_PATH_DEPTH_POLICY_3D_H
#define RENDER_RUNTIME_PATH_DEPTH_POLICY_3D_H

#include <stdbool.h>

typedef enum {
    RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_DIFFUSE = 1,
    RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_SPECULAR = 2,
    RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_TRANSMISSION = 3
} RuntimePathDepthPolicy3DLobe;

typedef struct {
    int diffuseDepth;
    int specularDepth;
    int transmissionDepth;
    int minDepthBeforeRoulette;
    double rouletteThreshold;
} RuntimePathDepthPolicy3D;

RuntimePathDepthPolicy3D RuntimePathDepthPolicy3D_Resolve(void);

int RuntimePathDepthPolicy3D_MaxDepthForLobe(const RuntimePathDepthPolicy3D* policy,
                                             RuntimePathDepthPolicy3DLobe lobe);

bool RuntimePathDepthPolicy3D_AllowsDepth(const RuntimePathDepthPolicy3D* policy,
                                          RuntimePathDepthPolicy3DLobe lobe,
                                          int depth);

double RuntimePathDepthPolicy3D_SurvivalProbability(const RuntimePathDepthPolicy3D* policy,
                                                    int depth,
                                                    double throughput_luma);

bool RuntimePathDepthPolicy3D_ShouldTerminate(const RuntimePathDepthPolicy3D* policy,
                                              int depth,
                                              double throughput_luma,
                                              double roulette_sample,
                                              double* out_survival_probability);

#endif
