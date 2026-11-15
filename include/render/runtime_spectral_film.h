#ifndef RENDER_RUNTIME_SPECTRAL_FILM_H
#define RENDER_RUNTIME_SPECTRAL_FILM_H
#include "render/runtime_native_3d_render.h"
#include "render/runtime_spectral_surface_map.h"
#include <SDL2/SDL_mutex.h>

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
void RuntimeSpectral_DisplayPixelWithTransform(uint8_t *pixel, Vec3 linear, bool soft);
bool RuntimeSpectralFilm_IsClosedMesh(const char *id);
bool RuntimeSpectralFilm_Supported(const RuntimeScene3D *scene, const RuntimeSpectralPencil *p);
bool RuntimeSpectralFilm_Region(const RuntimeSpectralPencil *p, const RuntimeSpectralSurfaceMap *map,
                                const RuntimeNative3DPreparedFrame *frame, int x0, int y0, int x1, int y1,
                                uint8_t *pixels, float *radiance, int stride, RuntimeSpectralReadout *out,
                                char *diag, size_t size);
/* Sparse camera masks are native samples; linear values retain signed RGB. */
bool RuntimeSpectralFilm_Sample(const RuntimeSpectralPencil *p, const RuntimeSpectralSurfaceMap *map,
    const RuntimeNative3DPreparedFrame *frame, int y0, int y1, double dx, double dy,
    const RuntimeNative3DSamplingContext *camera_sampling, const uint8_t *mask, float *radiance, RuntimeSpectralReadout *out, char *diag, size_t size);
typedef enum {RUNTIME_SPECTRAL_EXEC_PREPARED,RUNTIME_SPECTRAL_EXEC_RUNNING,RUNTIME_SPECTRAL_EXEC_COMPLETED,
    RUNTIME_SPECTRAL_EXEC_CANCELED,RUNTIME_SPECTRAL_EXEC_FAILED} RuntimeSpectralExecutionStatus;
typedef struct RuntimeSpectralFrame {
    SDL_mutex *query_lock; /* Continuous volume caches are owned by one shading tile at a time. */
    RuntimeSpectralPencil profile;
    RuntimeSpectralSurfaceMap *map;
    RuntimeSpectralMapStats stats;
    RuntimeSpectralSamplingStats sampling;
    struct RuntimeSpectralSurfacePath *surface_path;
    RuntimeSpectralReadout readout;
    char diagnostic[256];
    RuntimeSpectralExecutionStatus execution_status;
    bool preview;
    int requested_samples;
} RuntimeSpectralFrame;
RuntimeSpectralFrame *RuntimeSpectralFrame_Create(const RuntimeSpectralPencil *profile,
                                                  const RuntimeScene3D *scene, char *diag, size_t size);
RuntimeSpectralFrame *RuntimeSpectralFrame_CreateForFilm(const RuntimeSpectralPencil *,
    const RuntimeScene3D *,int,int,char *,size_t);
void RuntimeSpectralFrame_Free(RuntimeSpectralFrame *state);
bool RuntimeSpectralFrame_Shade(const RuntimeNative3DPreparedFrame *frame, int x0, int y0, int x1, int y1,
                                float *radiance, int stride, RuntimeNative3DRenderStats *stats);
#endif
