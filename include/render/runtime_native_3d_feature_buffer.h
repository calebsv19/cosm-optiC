#ifndef RENDER_RUNTIME_NATIVE_3D_FEATURE_BUFFER_H
#define RENDER_RUNTIME_NATIVE_3D_FEATURE_BUFFER_H

#include <stdbool.h>

#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_scene_3d.h"

typedef struct {
    float* normalBuffer;
    float* depthBuffer;
    unsigned char* hitMaskBuffer;
    int width;
    int height;
} RuntimeNative3DFeatureBuffer;

void RuntimeNative3DFeatureBuffer_Init(RuntimeNative3DFeatureBuffer* buffer);
void RuntimeNative3DFeatureBuffer_Free(RuntimeNative3DFeatureBuffer* buffer);
bool RuntimeNative3DFeatureBuffer_Ensure(RuntimeNative3DFeatureBuffer* buffer,
                                         int width,
                                         int height);
void RuntimeNative3DFeatureBuffer_Clear(RuntimeNative3DFeatureBuffer* buffer);
bool RuntimeNative3DFeatureBuffer_RenderRegion(RuntimeNative3DFeatureBuffer* buffer,
                                               const RuntimeScene3D* scene,
                                               const RuntimeCameraProjector3D* projector,
                                               int start_x,
                                               int start_y,
                                               int end_x,
                                               int end_y);

#endif
