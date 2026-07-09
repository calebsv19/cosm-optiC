#include "render/pipeline/ray_tracing2_buffers.h"

#include <stdlib.h>
#include <string.h>

#include "render/runtime_native_3d_render.h"

static bool ray_tracing2_buffers_ensure_native3d(Uint8** buffer,
                                                 size_t* capacity,
                                                 size_t pixel_count) {
    Uint8* resized = NULL;
    size_t byte_count = 0u;
    if (!buffer || !capacity || pixel_count == 0u) {
        return false;
    }
    if (*buffer && *capacity >= pixel_count) {
        return true;
    }
    byte_count = pixel_count * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
    resized = (Uint8*)realloc(*buffer, byte_count * sizeof(Uint8));
    if (!resized) {
        return false;
    }
    *buffer = resized;
    *capacity = pixel_count;
    return true;
}

bool RayTracing2BuffersEnsureFrameBuffers(Uint8** pixel_buffer,
                                          Uint8** tile_preview_buffer,
                                          float** energy_buffer,
                                          size_t pixel_count) {
    if (!pixel_buffer || !tile_preview_buffer || !energy_buffer || pixel_count == 0u) {
        return false;
    }
    if (!*pixel_buffer) {
        *pixel_buffer = (Uint8*)malloc(pixel_count * sizeof(Uint8));
    }
    if (!*tile_preview_buffer) {
        *tile_preview_buffer = (Uint8*)malloc(pixel_count * sizeof(Uint8));
    }
    if (!*energy_buffer) {
        *energy_buffer = (float*)malloc(pixel_count * sizeof(float));
    }
    return *pixel_buffer && *tile_preview_buffer && *energy_buffer;
}

void RayTracing2BuffersClearFrameBuffers(Uint8* pixel_buffer,
                                         Uint8* tile_preview_buffer,
                                         float* energy_buffer,
                                         size_t pixel_count) {
    if (pixel_buffer) {
        memset(pixel_buffer, 0, pixel_count * sizeof(Uint8));
    }
    if (tile_preview_buffer) {
        memset(tile_preview_buffer, 0, pixel_count * sizeof(Uint8));
    }
    if (energy_buffer) {
        memset(energy_buffer, 0, pixel_count * sizeof(float));
    }
}

void RayTracing2BuffersResetFrameBuffers(Uint8** pixel_buffer,
                                         Uint8** tile_preview_buffer,
                                         float** energy_buffer,
                                         float** direct_energy_buffer) {
    if (pixel_buffer && *pixel_buffer) {
        free(*pixel_buffer);
        *pixel_buffer = NULL;
    }
    if (tile_preview_buffer && *tile_preview_buffer) {
        free(*tile_preview_buffer);
        *tile_preview_buffer = NULL;
    }
    if (energy_buffer && *energy_buffer) {
        free(*energy_buffer);
        *energy_buffer = NULL;
    }
    if (direct_energy_buffer && *direct_energy_buffer) {
        free(*direct_energy_buffer);
        *direct_energy_buffer = NULL;
    }
}

bool RayTracing2BuffersEnsureNative3DRenderBuffer(Uint8** buffer,
                                                  size_t* capacity,
                                                  size_t pixel_count) {
    return ray_tracing2_buffers_ensure_native3d(buffer, capacity, pixel_count);
}

bool RayTracing2BuffersEnsureNative3DPreviewBuffer(Uint8** buffer,
                                                   size_t* capacity,
                                                   size_t pixel_count) {
    return ray_tracing2_buffers_ensure_native3d(buffer, capacity, pixel_count);
}

void RayTracing2BuffersResetNative3D(Uint8** render_buffer,
                                     size_t* render_capacity,
                                     Uint8** preview_buffer,
                                     size_t* preview_capacity,
                                     int* preview_width,
                                     int* preview_height) {
    if (render_buffer && *render_buffer) {
        free(*render_buffer);
        *render_buffer = NULL;
    }
    if (render_capacity) {
        *render_capacity = 0u;
    }
    if (preview_buffer && *preview_buffer) {
        free(*preview_buffer);
        *preview_buffer = NULL;
    }
    if (preview_capacity) {
        *preview_capacity = 0u;
    }
    if (preview_width) {
        *preview_width = 0;
    }
    if (preview_height) {
        *preview_height = 0;
    }
}
