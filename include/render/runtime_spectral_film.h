#ifndef RENDER_RUNTIME_SPECTRAL_FILM_H
#define RENDER_RUNTIME_SPECTRAL_FILM_H
#include "render/runtime_native_3d_render.h"
#include "render/runtime_spectral_surface_map.h"

bool RuntimeNative3DPrepareEvaluatedFrameWithProfile(RuntimeNative3DPreparedFrame *out_frame, int width,
                                                     int height,
                                                     const RayEvaluatedSceneSnapshot *evaluated_scene,
                                                     const RuntimeNative3DSamplingContext *sampling,
                                                     RuntimeNative3DSceneMutationFn mutation,
                                                     void *mutation_user_data,
                                                     const RuntimeSpectralPencil *profile);

double RuntimeSpectral_RGBBasis(Vec3 rgb, double nm);
Vec3 RuntimeSpectral_LinearRGB(Vec3 xyz);
void RuntimeSpectral_DisplayPixel(uint8_t *pixel, Vec3 linear);
bool RuntimeSpectralFilm_Supported(const RuntimeScene3D *scene, const RuntimeSpectralPencil *p);
bool RuntimeSpectralFilm_Region(const RuntimeSpectralPencil *p, const RuntimeSpectralSurfaceMap *map,
                                const RuntimeNative3DPreparedFrame *frame, int x0, int y0, int x1, int y1,
                                uint8_t *pixels, float *radiance, int stride, RuntimeSpectralReadout *out,
                                char *diag, size_t size);
typedef struct RuntimeSpectralFrame {
    RuntimeSpectralPencil profile;
    RuntimeSpectralSurfaceMap *map;
    RuntimeSpectralMapStats stats;
} RuntimeSpectralFrame;
RuntimeSpectralFrame *RuntimeSpectralFrame_Create(const RuntimeSpectralPencil *profile,
                                                  const RuntimeScene3D *scene, char *diag, size_t size);
void RuntimeSpectralFrame_Free(RuntimeSpectralFrame *state);
bool RuntimeSpectralFrame_Shade(const RuntimeNative3DPreparedFrame *frame, int x0, int y0, int x1, int y1,
                                float *radiance, int stride, RuntimeNative3DRenderStats *stats);
#endif
