#ifndef RAY_TRACING2_PREVIEW_PRESENT_INTERNAL_H
#define RAY_TRACING2_PREVIEW_PRESENT_INTERNAL_H

#include "render/pipeline/ray_tracing2_preview_present.h"

#include <stdbool.h>

#include "render/runtime_native_3d_tile_scheduler.h"

bool BeginNative3DEnvPresentationCapture(void);
void WriteNative3DEnvPresentationCapture(bool enabled,
                                         bool render_ok,
                                         const RuntimeNative3DRenderStats* stats);
void RecordNative3DPresentationEvent(
    RayTracing2Native3DPresentationEventKind kind,
    bool renderer_available,
    int render_width,
    int render_height,
    int host_width,
    int host_height,
    int dirty_tile_count,
    const RuntimeNative3DTileSchedulerProgress* progress,
    const SDL_Rect* rect,
    bool reset_dirty_preview,
    bool success);
uint64_t Native3DHostMovementBytes(int width, int height);
void SeedNative3DPreviewHistoryUnderlay(Uint8* host_buffer,
                                        size_t host_pixel_count,
                                        int host_width,
                                        int host_height,
                                        const RuntimeScene3D* scene,
                                        const RuntimeCameraProjector3D* projector,
                                        RuntimeNative3DRenderStats* stats);
bool PromoteNative3DPreviewHistory(const Uint8* host_buffer,
                                   size_t host_pixel_count,
                                   int host_width,
                                   int host_height,
                                   RuntimeNative3DRenderStats* stats);

#endif
