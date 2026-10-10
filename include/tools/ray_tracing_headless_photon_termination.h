#ifndef RAY_TRACING_HEADLESS_PHOTON_TERMINATION_H
#define RAY_TRACING_HEADLESS_PHOTON_TERMINATION_H
#include "app/agent_render_request.h"
#include "render/runtime_caustic_photon_scene_trace_3d.h"
bool ray_tracing_headless_photon_termination_begin(const RayTracingAgentRenderRequest* request);
void ray_tracing_headless_photon_termination_observe(const RuntimeCausticPhotonSample3D* sample,
                                                   const RuntimeCausticPhotonSceneTrace3D* trace);
bool ray_tracing_headless_photon_termination_end(void);
#endif
