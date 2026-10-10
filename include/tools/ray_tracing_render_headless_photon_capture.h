#ifndef RAY_TRACING_HEADLESS_PHOTON_CAPTURE_H
#define RAY_TRACING_HEADLESS_PHOTON_CAPTURE_H
#include <stdbool.h>
#include "app/agent_render_request.h"
bool ray_tracing_headless_photon_capture_begin(
    const RayTracingAgentRenderRequest* request, const char* selected_ids);
bool ray_tracing_headless_photon_capture_prepared(
    const RayTracingAgentRenderRequest* request);
bool ray_tracing_headless_photon_capture_end(void);
#endif
