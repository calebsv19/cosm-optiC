#ifndef TOOLS_RAY_TRACING_HEADLESS_SPECTRAL_H
#define TOOLS_RAY_TRACING_HEADLESS_SPECTRAL_H
#include <stdio.h>
#include "app/agent_render_request.h"
#include "render/runtime_native_3d_render.h"

bool RayTracingHeadlessSpectral_SceneSupported(const char *path, bool mapping);
void RayTracingHeadlessSpectral_Pixel(uint8_t *rgb, Vec3 xyz, double exposure);
bool RayTracingHeadlessSpectral_Run(const RayTracingAgentRenderRequest *request,
                                    const RuntimeNative3DPreparedFrame *frame, uint8_t *pixels,
                                    RuntimeSpectralReadout *out, char *diag, size_t size);
bool RayTracingHeadlessSpectral_Render(const RayTracingAgentRenderRequest *request,
                                       const RayEvaluatedSceneSnapshot *scene, uint8_t *pixels,
                                       RuntimeSpectralReadout *out, char *diag, size_t size);
void RayTracingHeadlessSpectral_Write(FILE *file, const RayTracingAgentRenderRequest *request,
                                      const RuntimeSpectralReadout *out, bool rendered);
#endif
