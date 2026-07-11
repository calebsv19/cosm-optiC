#ifndef APP_AGENT_RENDER_REQUEST_INTERNAL_H
#define APP_AGENT_RENDER_REQUEST_INTERNAL_H

#include "app/agent_render_request.h"
#include "app/ray_tracing_request_utils.h"

#include <json-c/json.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "import/fluid_volume_import_3d.h"
#include "render/runtime_scene_3d.h"

enum {
    RAY_TRACING_AGENT_RENDER_PRESET_NONE = 0,
    RAY_TRACING_AGENT_RENDER_PRESET_GLASS_PREVIEW = 1,
    RAY_TRACING_AGENT_RENDER_PRESET_GLASS_REVIEW = 2
};

bool agent_render_request_json_get_rgb(json_object* owner,
                                       const char* key,
                                       double* out_r,
                                       double* out_g,
                                       double* out_b);
int agent_render_request_parse_volume_source_kind(const char* kind_label,
                                                  const char* path);
RayTracing3DIntegratorId agent_render_request_parse_integrator_3d(const char* label);
int agent_render_request_parse_inspection_preset(const char* label);
bool agent_render_request_parse_trace_route(const char* label,
                                            RuntimeRay3DTraceRoute* out_route);
RuntimeDisneyV2CausticMode3D agent_render_request_caustic_mode_to_disney_v2_mode(
    RuntimeCausticMode3D mode);
int agent_render_request_parse_environment_light_mode(const char* label);
int agent_render_request_parse_environment_preset(const char* label);
void agent_render_request_set_diagf(char* out, size_t out_size, const char* format, ...);
int agent_render_request_clamp_secondary_diffuse_samples_3d_override(int value);
int agent_render_request_clamp_transmission_samples_3d_override(int value);
bool agent_render_request_finalize_loaded(RayTracingAgentRenderRequest* request,
                                          json_object* root,
                                          const char* request_path,
                                          char* out_diagnostics,
                                          size_t out_diagnostics_size);

#endif
