#include "render/runtime_native_3d_tile_scheduler_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int s_runtime_native_3d_black_pixel_diag_progress_initial_count = 0;
static int s_runtime_native_3d_black_pixel_diag_progress_later_count = 0;
static int s_runtime_native_3d_black_pixel_diag_final_count = 0;

static FILE* runtime_native_3d_tile_scheduler_open_black_pixel_diag(void) {
    const char* env_path = getenv("RAY_TRACING_NATIVE3D_BLACK_PIXEL_CAPTURE_PATH");
    const char* home = getenv("HOME");
    char default_path[1024];

    if (env_path && env_path[0]) {
        return fopen(env_path, "a");
    }
    if (!home || !home[0]) {
        return NULL;
    }
    snprintf(default_path,
             sizeof(default_path),
             "%s/Library/Logs/RayTracing/native3d_black_pixel_diag.jsonl",
             home);
    return fopen(default_path, "a");
}

bool runtime_native_3d_tile_scheduler_capture_black_hit_pixels(
    const RuntimeNative3DTileSchedulerJob* job,
    const uint8_t* pixel_buffer,
    int pixel_width,
    int subpass_index,
    const char* phase) {
    enum {
        kBlackPixelDiagProgressInitialLimit = 128,
        kBlackPixelDiagProgressLaterLimit = 1024,
        kBlackPixelDiagFinalLimit = 1024
    };
    FILE* f = NULL;
    int* counter = NULL;
    int limit = 0;
    bool found_black_geometry_hit = false;

    if (!job || !pixel_buffer || pixel_width <= 0 || !phase) {
        return false;
    }
    if (strcmp(phase, "final_resolve") == 0) {
        counter = &s_runtime_native_3d_black_pixel_diag_final_count;
        limit = kBlackPixelDiagFinalLimit;
    } else if (subpass_index <= 0) {
        counter = &s_runtime_native_3d_black_pixel_diag_progress_initial_count;
        limit = kBlackPixelDiagProgressInitialLimit;
    } else {
        counter = &s_runtime_native_3d_black_pixel_diag_progress_later_count;
        limit = kBlackPixelDiagProgressLaterLimit;
    }
    if (!counter) {
        return false;
    }
    if (!job->renderUnit.accumulation.sampleCountBuffer ||
        !job->renderUnit.featureBuffer.hitMaskBuffer ||
        !job->renderUnit.featureBuffer.triangleIndexBuffer ||
        !job->renderUnit.featureBuffer.sceneObjectIndexBuffer ||
        !job->renderUnit.featureBuffer.depthBuffer ||
        !job->renderUnit.featureBuffer.normalBuffer) {
        return false;
    }

    for (int local_y = 0; local_y < job->tile.height; ++local_y) {
        for (int local_x = 0; local_x < job->tile.width; ++local_x) {
            const int pixel_x = job->tile.originX + local_x;
            const int pixel_y = job->tile.originY + local_y;
            const size_t local_index =
                (size_t)local_y * (size_t)job->tile.width + (size_t)local_x;
            const size_t pixel_base =
                ((size_t)pixel_y * (size_t)pixel_width + (size_t)pixel_x) *
                (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
            const size_t normal_base = local_index * 3u;
            const bool black_pixel =
                pixel_buffer[pixel_base] == 0u &&
                pixel_buffer[pixel_base + 1u] == 0u &&
                pixel_buffer[pixel_base + 2u] == 0u;

            if (!black_pixel || !job->renderUnit.featureBuffer.hitMaskBuffer[local_index]) {
                continue;
            }
            found_black_geometry_hit = true;
            if (*counter >= limit) {
                return true;
            }
            if (!f) {
                f = runtime_native_3d_tile_scheduler_open_black_pixel_diag();
                if (!f) {
                    return true;
                }
            }
            fprintf(f,
                    "{\"phase\":\"%s\",\"subpass\":%d,"
                    "\"tile\":{\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d},"
                    "\"pixel\":{\"x\":%d,\"y\":%d,\"local_x\":%d,\"local_y\":%d},"
                    "\"sample_count\":%u,\"active_pixels\":%d,\"active_tiles\":%d,"
                    "\"adaptive_state\":{\"flags\":%u,\"mean_luma\":%.9g,"
                    "\"radiance_delta\":%.9g,\"risk\":%.9g},"
                    "\"feature\":{\"triangle\":%d,\"object\":%d,\"depth\":%.9g,"
                    "\"normal\":[%.9g,%.9g,%.9g]},"
                    "\"rgba\":[%u,%u,%u,%u],\"committed_subpasses\":%d,"
                    "\"use_adaptive\":%s,\"use_denoise\":%s}\n",
                    phase,
                    subpass_index,
                    job->tile.originX,
                    job->tile.originY,
                    job->tile.width,
                    job->tile.height,
                    pixel_x,
                    pixel_y,
                    local_x,
                    local_y,
                    (unsigned int)job->renderUnit.accumulation.sampleCountBuffer[local_index],
                    job->activePixelCount,
                    job->activeTileCount,
                    job->renderUnit.adaptivePixelState.pixels
                        ? (unsigned int)job->renderUnit.adaptivePixelState.pixels[local_index].flags
                        : 0u,
                    job->renderUnit.adaptivePixelState.pixels
                        ? (double)job->renderUnit.adaptivePixelState.pixels[local_index].meanLuma
                        : 0.0,
                    job->renderUnit.adaptivePixelState.pixels
                        ? (double)job->renderUnit.adaptivePixelState.pixels[local_index].radianceDelta
                        : 0.0,
                    job->renderUnit.adaptivePixelState.pixels
                        ? (double)job->renderUnit.adaptivePixelState.pixels[local_index].risk
                        : 0.0,
                    job->renderUnit.featureBuffer.triangleIndexBuffer[local_index],
                    job->renderUnit.featureBuffer.sceneObjectIndexBuffer[local_index],
                    (double)job->renderUnit.featureBuffer.depthBuffer[local_index],
                    (double)job->renderUnit.featureBuffer.normalBuffer[normal_base],
                    (double)job->renderUnit.featureBuffer.normalBuffer[normal_base + 1u],
                    (double)job->renderUnit.featureBuffer.normalBuffer[normal_base + 2u],
                    (unsigned int)pixel_buffer[pixel_base],
                    (unsigned int)pixel_buffer[pixel_base + 1u],
                    (unsigned int)pixel_buffer[pixel_base + 2u],
                    (unsigned int)pixel_buffer[pixel_base + 3u],
                    job->renderUnit.committedSubpasses,
                    job->renderUnit.useAdaptiveSampling ? "true" : "false",
                    job->renderUnit.useDenoise ? "true" : "false");
            *counter += 1;
            if (*counter >= limit) {
                fclose(f);
                return true;
            }
        }
    }
    if (f) {
        fclose(f);
    }
    return found_black_geometry_hit;
}
