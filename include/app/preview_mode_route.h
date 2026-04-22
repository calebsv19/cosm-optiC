#ifndef PREVIEW_MODE_ROUTE_H
#define PREVIEW_MODE_ROUTE_H

#include <stdbool.h>

#include "render/ray_tracing_mode_backend.h"

typedef enum PreviewRenderBranch {
    PREVIEW_RENDER_BRANCH_LEGACY_2D = 0,
    PREVIEW_RENDER_BRANCH_RETAINED_3D = 1,
    PREVIEW_RENDER_BRANCH_FALLBACK_2D = 2
} PreviewRenderBranch;

typedef struct PreviewModeRouteDecision {
    PreviewRenderBranch branch;
    bool requested3D;
    bool retained3DReady;
    bool projectorReady;
    bool digestValid;
    RayTracingRuntimeRoute route;
    RayTracingSceneDigestStatus digestStatus;
    char branchLabel[48];
    char statusLine[160];
} PreviewModeRouteDecision;

bool PreviewModeRouteSelect(const RayTracingRuntimeRoute* route,
                            const RayTracingSceneDigestStatus* digest_status,
                            bool projector_ready,
                            PreviewModeRouteDecision* out_decision);

#endif
