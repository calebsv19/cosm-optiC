#ifndef TOOLS_RAY_TRACING_HEADLESS_SPECTRAL_H
#define TOOLS_RAY_TRACING_HEADLESS_SPECTRAL_H
#include "render/runtime_spectral_transport.h"
#include <stdio.h>
#include "app/agent_render_request.h"
#include "render/runtime_native_3d_render.h"

bool RayTracingHeadlessSpectral_IsOrdinary(const RayTracingAgentRenderRequest *);
bool RayTracingHeadlessSpectral_PrepareNative(const RayTracingAgentRenderRequest *,const RayEvaluatedSceneSnapshot *,RuntimeNative3DPreparedFrame *);
bool RayTracingHeadlessSpectral_RenderNative(const RayTracingAgentRenderRequest *,const char *,const RayEvaluatedSceneSnapshot *,uint8_t *,RuntimeSpectralReadout *,RuntimeSpectralTransportSnapshot *,char *,size_t);
bool RayTracingHeadlessSpectral_SceneSupported(const char *path, const RuntimeSpectralPencil *p);
void RayTracingHeadlessSpectral_Pixel(uint8_t *rgb, Vec3 xyz, double exposure);
bool RayTracingHeadlessSpectral_Run(const RayTracingAgentRenderRequest *request,
                                    const RuntimeNative3DPreparedFrame *frame, uint8_t *pixels,
                                    RuntimeSpectralReadout *out, char *diag, size_t size);
bool RayTracingHeadlessSpectral_Render(const RayTracingAgentRenderRequest *request,const char *image_path,
                                       const RayEvaluatedSceneSnapshot *scene, uint8_t *pixels,
                                       RuntimeSpectralReadout *out, RuntimeSpectralTransportSnapshot *snapshot, char *diag, size_t size);
void RayTracingHeadlessSpectral_Write(FILE *file, const RayTracingAgentRenderRequest *request,
                                      const RuntimeSpectralReadout *out, bool rendered);
#endif
