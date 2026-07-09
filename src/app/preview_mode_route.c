#include "app/preview_mode_route.h"

#include <stdio.h>
#include <string.h>

bool PreviewModeRouteSelect(const RayTracingRuntimeRoute* route,
                            const RayTracingSceneDigestStatus* digest_status,
                            bool projector_ready,
                            PreviewModeRouteDecision* out_decision) {
    PreviewModeRouteDecision decision;
    memset(&decision, 0, sizeof(decision));

    if (!route || !out_decision) return false;
    decision.route = *route;
    if (digest_status) {
        decision.digestStatus = *digest_status;
        decision.digestValid = digest_status->valid;
    }
    decision.projectorReady = projector_ready;
    decision.requested3D = (route->requestedMode == SPACE_MODE_3D);
    decision.retained3DReady = decision.requested3D && decision.projectorReady && decision.digestValid;

    if (!decision.requested3D) {
        decision.branch = PREVIEW_RENDER_BRANCH_LEGACY_2D;
        snprintf(decision.branchLabel, sizeof(decision.branchLabel), "Preview: 2D");
        snprintf(decision.statusLine,
                 sizeof(decision.statusLine),
                 "Route %s using legacy 2D preview branch.",
                 RayTracingModeBackend_Name(route));
    } else if (decision.retained3DReady) {
        decision.branch = PREVIEW_RENDER_BRANCH_RETAINED_3D;
        snprintf(decision.branchLabel, sizeof(decision.branchLabel), "Preview: 3D Retained");
        snprintf(decision.statusLine,
                 sizeof(decision.statusLine),
                 "Route %s with retained digest primitives=%d.",
                 RayTracingModeBackend_Name(route),
                 decision.digestStatus.digestPrimitiveCount);
    } else {
        decision.branch = PREVIEW_RENDER_BRANCH_FALLBACK_2D;
        snprintf(decision.branchLabel, sizeof(decision.branchLabel), "Preview: 3D Fallback");
        snprintf(decision.statusLine,
                 sizeof(decision.statusLine),
                 "Route %s fallback: projector=%s digest=%s.",
                 RayTracingModeBackend_Name(route),
                 decision.projectorReady ? "ok" : "missing",
                 decision.digestValid ? "ok" : "missing");
    }

    *out_decision = decision;
    return true;
}
