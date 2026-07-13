#ifndef APP_RAY_TRACING_DEEP_RENDER_DESKTOP_RENDER_INTERNAL_H
#define APP_RAY_TRACING_DEEP_RENDER_DESKTOP_RENDER_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app/ray_tracing_deep_render_session.h"
#include "config/config_manager.h"
#include "render/runtime_native_3d_async_render_bridge.h"
#include "render/runtime_native_3d_async_render_job.h"

typedef enum RayTracingDeepRenderDesktopStartStatus {
    RAY_TRACING_DEEP_RENDER_DESKTOP_START_READY = 0,
    RAY_TRACING_DEEP_RENDER_DESKTOP_START_UNSUPPORTED,
    RAY_TRACING_DEEP_RENDER_DESKTOP_START_FAILED,
} RayTracingDeepRenderDesktopStartStatus;

typedef struct RayTracingDeepRenderDesktopStartDesc {
    uint64_t generation;
    int localFrameIndex;
    int absoluteFrameIndex;
    int frameCount;
    double frameDurationSeconds;
    double animationTimeSeconds;
    double normalizedT;
    double lightX;
    double lightY;
    int outputWidth;
    int outputHeight;
    const char* outputRoot;
    const char* frameDirectory;
    const char* finalFramePath;
} RayTracingDeepRenderDesktopStartDesc;

typedef struct RayTracingDeepRenderDesktopRenderUnit {
    RuntimeNative3DAsyncRenderProgressBuffer* progress;
    RayTracingDeepRenderSession* session;
    uint8_t* renderPixels;
    uint8_t* hostPixels;
    size_t renderCapacity;
    size_t hostCapacity;
    uint64_t generation;
    int renderWidth;
    int renderHeight;
    int hostWidth;
    int hostHeight;
    int temporalFrames;
    int tileSize;
    RayTracing3DIntegratorId integratorId;
    Runtime3DUpscaleMode upscaleMode;
} RayTracingDeepRenderDesktopRenderUnit;

void RayTracingDeepRenderDesktopRenderUnit_Init(
    RayTracingDeepRenderDesktopRenderUnit* unit);
void RayTracingDeepRenderDesktopRenderUnit_Destroy(
    RayTracingDeepRenderDesktopRenderUnit* unit);

RayTracingDeepRenderDesktopStartStatus
RayTracingDeepRenderDesktopRenderUnit_Start(
    RayTracingDeepRenderDesktopRenderUnit* unit,
    RayTracingDeepRenderSession* session,
    RuntimeNative3DAsyncRenderJob* job,
    const RayTracingDeepRenderDesktopStartDesc* desc,
    const char** out_reason);

#endif
