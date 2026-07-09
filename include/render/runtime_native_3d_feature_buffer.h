#ifndef RENDER_RUNTIME_NATIVE_3D_FEATURE_BUFFER_H
#define RENDER_RUNTIME_NATIVE_3D_FEATURE_BUFFER_H

#include <stdbool.h>

#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_scene_3d.h"

typedef enum RuntimeNative3DDirectLightVisibilityOutcome {
    RUNTIME_NATIVE_3D_DIRECT_LIGHT_VISIBILITY_UNKNOWN = 0,
    RUNTIME_NATIVE_3D_DIRECT_LIGHT_VISIBILITY_NO_TRACE = 1,
    RUNTIME_NATIVE_3D_DIRECT_LIGHT_VISIBILITY_CLEAR_VISIBLE = 2,
    RUNTIME_NATIVE_3D_DIRECT_LIGHT_VISIBILITY_CLEAR_BLOCKED = 3,
    RUNTIME_NATIVE_3D_DIRECT_LIGHT_VISIBILITY_STABLE_PARTIAL = 4,
    RUNTIME_NATIVE_3D_DIRECT_LIGHT_VISIBILITY_MIXED_PARTIAL = 5
} RuntimeNative3DDirectLightVisibilityOutcome;

typedef struct RuntimeNative3DFeatureBuffer {
    float* normalBuffer;
    float* depthBuffer;
    float* reflectivityBuffer;
    float* roughnessBuffer;
    float* transparencyBuffer;
    unsigned char* hitMaskBuffer;
    unsigned char* directLightVisibilityOutcomeBuffer;
    int* triangleIndexBuffer;
    int* sceneObjectIndexBuffer;
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
void RuntimeNative3DFeatureBuffer_RecordDirectLightVisibilityOutcome(
    RuntimeNative3DFeatureBuffer* buffer,
    int local_x,
    int local_y,
    RuntimeNative3DDirectLightVisibilityOutcome outcome);
RuntimeNative3DDirectLightVisibilityOutcome
RuntimeNative3DFeatureBuffer_ResolveDirectLightVisibilityOutcome(
    int no_trace_count,
    int clear_visible_count,
    int clear_blocked_count,
    int stable_partial_count,
    int mixed_partial_count);

#endif
