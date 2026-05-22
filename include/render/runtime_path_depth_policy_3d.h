#ifndef RENDER_RUNTIME_PATH_DEPTH_POLICY_3D_H
#define RENDER_RUNTIME_PATH_DEPTH_POLICY_3D_H

typedef struct {
    int diffuseDepth;
    int specularDepth;
    int transmissionDepth;
} RuntimePathDepthPolicy3D;

RuntimePathDepthPolicy3D RuntimePathDepthPolicy3D_Resolve(void);

#endif
