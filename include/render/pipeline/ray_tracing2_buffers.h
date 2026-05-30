#ifndef RENDER_PIPELINE_RAY_TRACING2_BUFFERS_H
#define RENDER_PIPELINE_RAY_TRACING2_BUFFERS_H

#include <stdbool.h>
#include <stddef.h>

#include <SDL2/SDL.h>

bool RayTracing2BuffersEnsureNative3DRenderBuffer(Uint8** buffer,
                                                  size_t* capacity,
                                                  size_t pixel_count);
bool RayTracing2BuffersEnsureNative3DPreviewBuffer(Uint8** buffer,
                                                   size_t* capacity,
                                                   size_t pixel_count);
bool RayTracing2BuffersEnsureFrameBuffers(Uint8** pixel_buffer,
                                          Uint8** tile_preview_buffer,
                                          float** energy_buffer,
                                          size_t pixel_count);
void RayTracing2BuffersClearFrameBuffers(Uint8* pixel_buffer,
                                         Uint8* tile_preview_buffer,
                                         float* energy_buffer,
                                         size_t pixel_count);
void RayTracing2BuffersResetFrameBuffers(Uint8** pixel_buffer,
                                         Uint8** tile_preview_buffer,
                                         float** energy_buffer,
                                         float** direct_energy_buffer);
void RayTracing2BuffersResetNative3D(Uint8** render_buffer,
                                     size_t* render_capacity,
                                     Uint8** preview_buffer,
                                     size_t* preview_capacity,
                                     int* preview_width,
                                     int* preview_height);

#endif
