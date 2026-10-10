#include "render/runtime_native_3d_feature_buffer.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_material_payload_3d.h"

const RuntimeNative3DFeatureBuffer* RuntimeNative3DFeatureBuffer_GuideSource(
    const RuntimeNative3DFeatureBuffer* b, size_t pixel, size_t* local_pixel) {
    if (!b || !local_pixel || pixel >= (size_t)b->width * (size_t)b->height) return NULL;
    *local_pixel = pixel;
    return b->guideSource ? b->guideSource(b->guideContext, pixel, local_pixel) : b;
}

void RuntimeNative3DFeatureBuffer_Init(RuntimeNative3DFeatureBuffer* buffer) {
    if (!buffer) return;
    memset(buffer, 0, sizeof(*buffer));
}

void RuntimeNative3DFeatureBuffer_Free(RuntimeNative3DFeatureBuffer* buffer) {
    if (!buffer) return;
    free(buffer->normalBuffer);
    RuntimeNative3DFeatureBuffer_ReleaseGuides(buffer);

    free(buffer->depthBuffer);
    free(buffer->reflectivityBuffer);
    free(buffer->roughnessBuffer);
    free(buffer->transparencyBuffer);
    free(buffer->hitMaskBuffer);
    free(buffer->directLightVisibilityOutcomeBuffer);
    free(buffer->triangleIndexBuffer);
    free(buffer->sceneObjectIndexBuffer);
    memset(buffer, 0, sizeof(*buffer));
}

static void runtime_native_3d_feature_buffer_clear_identity(RuntimeNative3DFeatureBuffer* buffer,
                                                            size_t pixel_count) {
    if (!buffer || !buffer->triangleIndexBuffer || !buffer->sceneObjectIndexBuffer) {
        return;
    }
    for (size_t i = 0; i < pixel_count; ++i) {
        buffer->triangleIndexBuffer[i] = -1;
        buffer->sceneObjectIndexBuffer[i] = -1;
    }
}

void RuntimeNative3DFeatureBuffer_ReleaseGuides(RuntimeNative3DFeatureBuffer* b) {
    if (!b) return;
    b->guideSource = NULL; b->guideContext = NULL;
    free(b->albedoBuffer); free(b->shadingNormalBuffer); free(b->materialGuideMaskBuffer);
    if (b->guideStorage == RUNTIME_NATIVE_3D_GUIDES_OWNED_STATISTICS) {
        free(b->varianceOfMeanBuffer); free(b->sampleCountBuffer);
    }
    b->albedoBuffer = b->shadingNormalBuffer = b->varianceOfMeanBuffer = NULL;
    b->sampleCountBuffer = NULL; b->materialGuideMaskBuffer = NULL;
    b->borrowedRawM2Buffer = NULL; b->guideStorage = RUNTIME_NATIVE_3D_GUIDES_NONE;
}

static bool ensure_guides(RuntimeNative3DFeatureBuffer* b, RuntimeNative3DGuideStorage mode) {
    if (b->guideStorage == mode) return true;
    if (mode == RUNTIME_NATIVE_3D_GUIDES_NONE) {
        RuntimeNative3DFeatureBuffer_ReleaseGuides(b); return true;
    }
    size_t n = (size_t)b->width * (size_t)b->height;
    float* albedo = calloc(n * 3u, sizeof(float));
    float* shading = calloc(n * 3u, sizeof(float));
    unsigned char* mask = calloc(n, 1);
    float* variance = mode == RUNTIME_NATIVE_3D_GUIDES_OWNED_STATISTICS ? calloc(n, sizeof(float)) : NULL;
    uint16_t* counts = mode == RUNTIME_NATIVE_3D_GUIDES_OWNED_STATISTICS ? calloc(n, sizeof(uint16_t)) : NULL;
    if (!albedo || !shading || !mask ||
        (mode == RUNTIME_NATIVE_3D_GUIDES_OWNED_STATISTICS && (!variance || !counts))) {
        free(albedo); free(shading); free(mask); free(variance); free(counts); return false;
    }
    RuntimeNative3DFeatureBuffer_ReleaseGuides(b);
    b->albedoBuffer = albedo; b->shadingNormalBuffer = shading; b->materialGuideMaskBuffer = mask;
    b->varianceOfMeanBuffer = variance; b->sampleCountBuffer = counts; b->guideStorage = mode;
    return true;
}

bool RuntimeNative3DFeatureBuffer_Ensure(RuntimeNative3DFeatureBuffer* b, int width, int height) {
    return RuntimeNative3DFeatureBuffer_EnsureWithGuides(b, width, height, RUNTIME_NATIVE_3D_GUIDES_NONE);
}

bool RuntimeNative3DFeatureBuffer_EnsureWithGuides(
    RuntimeNative3DFeatureBuffer* b, int width, int height, RuntimeNative3DGuideStorage mode) {
    if (!b || width <= 0 || height <= 0 || mode < RUNTIME_NATIVE_3D_GUIDES_NONE ||
        mode > RUNTIME_NATIVE_3D_GUIDES_BORROWED_STATISTICS ||
        (size_t)width > SIZE_MAX / (size_t)height / (3u * sizeof(float))) return false;
    if (b->normalBuffer && b->width == width && b->height == height) return ensure_guides(b, mode);
    RuntimeNative3DFeatureBuffer next = {0};
    size_t n = (size_t)width * (size_t)height;
    next.width = width; next.height = height;
    next.normalBuffer = calloc(n * 3u, sizeof(float));
    next.depthBuffer = calloc(n, sizeof(float));
    next.reflectivityBuffer = calloc(n, sizeof(float));
    next.roughnessBuffer = calloc(n, sizeof(float));
    next.transparencyBuffer = calloc(n, sizeof(float));
    next.hitMaskBuffer = calloc(n, 1); next.directLightVisibilityOutcomeBuffer = calloc(n, 1);
    next.triangleIndexBuffer = calloc(n, sizeof(int)); next.sceneObjectIndexBuffer = calloc(n, sizeof(int));
    if (!next.normalBuffer || !next.depthBuffer || !next.reflectivityBuffer || !next.roughnessBuffer ||
        !next.transparencyBuffer || !next.hitMaskBuffer || !next.directLightVisibilityOutcomeBuffer ||
        !next.triangleIndexBuffer || !next.sceneObjectIndexBuffer || !ensure_guides(&next, mode)) {
        RuntimeNative3DFeatureBuffer_Free(&next); return false;
    }
    RuntimeNative3DFeatureBuffer_Free(b); *b = next;
    runtime_native_3d_feature_buffer_clear_identity(b, n);
    return true;
}

void RuntimeNative3DFeatureBuffer_BorrowSamplingStatistics(
    RuntimeNative3DFeatureBuffer* b, const float* raw_m2, uint16_t* counts) {
    if (!b || b->guideStorage != RUNTIME_NATIVE_3D_GUIDES_BORROWED_STATISTICS) return;
    b->borrowedRawM2Buffer = raw_m2; b->sampleCountBuffer = counts;
}

static float variance_of_mean(const float* raw_m2, uint16_t count) {
    if (!raw_m2 || count < 2u) return INFINITY;
    float maximum = 0.0f;
    for (size_t c = 0; c < 3u; ++c) {
        float m2 = raw_m2[c];
        if (!isfinite(m2) || m2 < 0.0f) return INFINITY;
        float v = m2 / ((float)count * (float)(count - 1u));
        maximum = fmaxf(maximum, v);
    }
    return maximum;
}

float RuntimeNative3DFeatureBuffer_VarianceAt(const RuntimeNative3DFeatureBuffer* b, size_t pixel) {
    if (!b) return INFINITY;
    if (b->varianceOfMeanBuffer) return b->varianceOfMeanBuffer[pixel];
    if (b->borrowedRawM2Buffer && b->sampleCountBuffer)
        return variance_of_mean(b->borrowedRawM2Buffer + pixel * 3u, b->sampleCountBuffer[pixel]);
    return INFINITY;
}

size_t RuntimeNative3DFeatureBuffer_AllocatedBytes(const RuntimeNative3DFeatureBuffer* b) {
    if (!b || !b->normalBuffer) return 0u;
    size_t bytes = 38u;
    if (b->albedoBuffer) bytes += 25u;
    if (b->guideStorage == RUNTIME_NATIVE_3D_GUIDES_OWNED_STATISTICS) bytes += 6u;
    return (size_t)b->width * (size_t)b->height * bytes;
}

void RuntimeNative3DFeatureBuffer_Clear(RuntimeNative3DFeatureBuffer* buffer) {
    size_t pixel_count = 0;
    if (!buffer || !buffer->normalBuffer || !buffer->depthBuffer ||
        !buffer->reflectivityBuffer || !buffer->roughnessBuffer ||
        !buffer->transparencyBuffer || !buffer->hitMaskBuffer ||
        !buffer->directLightVisibilityOutcomeBuffer ||
        !buffer->triangleIndexBuffer || !buffer->sceneObjectIndexBuffer ||
        buffer->width <= 0 || buffer->height <= 0) {
        return;
    }
    pixel_count = (size_t)buffer->width * (size_t)buffer->height;
    if (buffer->albedoBuffer) memset(buffer->albedoBuffer, 0, pixel_count * 3u * sizeof(float));
    if (buffer->shadingNormalBuffer) memset(buffer->shadingNormalBuffer, 0, pixel_count * 3u * sizeof(float));
    if (buffer->varianceOfMeanBuffer) memset(buffer->varianceOfMeanBuffer, 0, pixel_count * sizeof(float));
    if (buffer->guideStorage == RUNTIME_NATIVE_3D_GUIDES_OWNED_STATISTICS && buffer->sampleCountBuffer)
        memset(buffer->sampleCountBuffer, 0, pixel_count * sizeof(uint16_t));
    if (buffer->materialGuideMaskBuffer) memset(buffer->materialGuideMaskBuffer, 0, pixel_count);
    memset(buffer->normalBuffer, 0, pixel_count * 3u * sizeof(*buffer->normalBuffer));
    memset(buffer->depthBuffer, 0, pixel_count * sizeof(*buffer->depthBuffer));
    memset(buffer->reflectivityBuffer, 0, pixel_count * sizeof(*buffer->reflectivityBuffer));
    memset(buffer->roughnessBuffer, 0, pixel_count * sizeof(*buffer->roughnessBuffer));
    memset(buffer->transparencyBuffer, 0, pixel_count * sizeof(*buffer->transparencyBuffer));
    memset(buffer->hitMaskBuffer, 0, pixel_count * sizeof(*buffer->hitMaskBuffer));
    memset(buffer->directLightVisibilityOutcomeBuffer,
           0,
           pixel_count * sizeof(*buffer->directLightVisibilityOutcomeBuffer));
    runtime_native_3d_feature_buffer_clear_identity(buffer, pixel_count);
}

bool RuntimeNative3DFeatureBuffer_RenderRegion(RuntimeNative3DFeatureBuffer* buffer,
                                               const RuntimeScene3D* scene,
                                               const RuntimeCameraProjector3D* projector,
                                               int start_x,
                                               int start_y,
                                               int end_x,
                                               int end_y) {
    const int region_width = end_x - start_x;
    const int region_height = end_y - start_y;
    if (!buffer || !scene || !projector) return false;
    if (region_width <= 0 || region_height <= 0) return false;
    if (!RuntimeNative3DFeatureBuffer_EnsureWithGuides(buffer, region_width, region_height, buffer->guideStorage)) {
        return false;
    }
    RuntimeNative3DFeatureBuffer_Clear(buffer);

    for (int y = start_y; y < end_y; ++y) {
        const int local_y = y - start_y;
        for (int x = start_x; x < end_x; ++x) {
            RuntimeLightEmitterTrace3DResult trace = {0};
            RuntimeMaterialPayload3D payload = {0};
            Ray3D primary_ray = RuntimeCameraProjector3D_MakePrimaryRay(projector,
                                                                        (double)x,
                                                                        (double)y);
            const int local_x = x - start_x;
            const size_t pixel_index =
                (size_t)local_y * (size_t)region_width + (size_t)local_x;
            const size_t normal_base = pixel_index * 3u;
            Vec3 normal = vec3(0.0, 0.0, 0.0);
            double depth = 0.0;

            if (!RuntimeLightEmitter3D_ResolveFirstHit(scene,
                                                       &primary_ray,
                                                       projector->nearPlane,
                                                       HUGE_VAL,
                                                       &trace)) {
                continue;
            }

            if (trace.emitterWins) {
                normal = trace.emitterHitInfo.normal;
                depth = trace.emitterHitInfo.t;
            } else if (trace.geometryHit) {
                normal = trace.geometryHitInfo.normal;
                depth = trace.geometryHitInfo.t;
                buffer->triangleIndexBuffer[pixel_index] = trace.geometryHitInfo.triangleIndex;
                buffer->sceneObjectIndexBuffer[pixel_index] =
                    trace.geometryHitInfo.sceneObjectIndex;
                if (RuntimeMaterialPayload3D_ResolveFromHit(&trace.geometryHitInfo, &payload) &&
                    payload.valid) {
                    if (buffer->albedoBuffer) {
                    buffer->albedoBuffer[normal_base] = (float)payload.baseColorR;
                    buffer->albedoBuffer[normal_base + 1u] = (float)payload.baseColorG;
                    buffer->albedoBuffer[normal_base + 2u] = (float)payload.baseColorB;
                    (void)RuntimeMaterialPayload3D_ApplyShadingNormal(&payload, &trace.geometryHitInfo);
                    const Vec3 shading = trace.geometryHitInfo.normal;
                    buffer->shadingNormalBuffer[normal_base] = (float)shading.x;
                    buffer->shadingNormalBuffer[normal_base + 1u] = (float)shading.y;
                    buffer->shadingNormalBuffer[normal_base + 2u] = (float)shading.z;
                    /* Surface guides cannot describe mixed volume radiance. Preserve
                     * the existing filter policy until volume signals are separated. */
                    buffer->materialGuideMaskBuffer[pixel_index] =
                        !(scene->volume.enabled && scene->volume.hasData);
                    }
                    buffer->reflectivityBuffer[pixel_index] = (float)fmax(payload.bsdf.reflectivity, 0.0);
                    buffer->roughnessBuffer[pixel_index] = (float)fmax(payload.bsdf.roughness, 0.0);
                    buffer->transparencyBuffer[pixel_index] = (float)fmax(payload.transparency, 0.0);
                }
            } else {
                continue;
            }

            buffer->hitMaskBuffer[pixel_index] = 1u;
            buffer->depthBuffer[pixel_index] = (float)fmax(depth, 0.0);
            buffer->normalBuffer[normal_base] = (float)normal.x;
            buffer->normalBuffer[normal_base + 1u] = (float)normal.y;
            buffer->normalBuffer[normal_base + 2u] = (float)normal.z;
        }
    }
    return true;
}

void RuntimeNative3DFeatureBuffer_RecordDirectLightVisibilityOutcome(
    RuntimeNative3DFeatureBuffer* buffer,
    int local_x,
    int local_y,
    RuntimeNative3DDirectLightVisibilityOutcome outcome) {
    size_t pixel_index = 0;
    if (!buffer || !buffer->directLightVisibilityOutcomeBuffer ||
        local_x < 0 || local_y < 0 ||
        local_x >= buffer->width || local_y >= buffer->height) {
        return;
    }
    pixel_index = (size_t)local_y * (size_t)buffer->width + (size_t)local_x;
    buffer->directLightVisibilityOutcomeBuffer[pixel_index] = (unsigned char)outcome;
}

RuntimeNative3DDirectLightVisibilityOutcome
RuntimeNative3DFeatureBuffer_ResolveDirectLightVisibilityOutcome(
    int no_trace_count,
    int clear_visible_count,
    int clear_blocked_count,
    int stable_partial_count,
    int mixed_partial_count) {
    if (mixed_partial_count > 0) {
        return RUNTIME_NATIVE_3D_DIRECT_LIGHT_VISIBILITY_MIXED_PARTIAL;
    }
    if (stable_partial_count > 0) {
        return RUNTIME_NATIVE_3D_DIRECT_LIGHT_VISIBILITY_STABLE_PARTIAL;
    }
    if (clear_blocked_count > 0 && clear_visible_count > 0) {
        return RUNTIME_NATIVE_3D_DIRECT_LIGHT_VISIBILITY_MIXED_PARTIAL;
    }
    if (clear_blocked_count > 0) {
        return RUNTIME_NATIVE_3D_DIRECT_LIGHT_VISIBILITY_CLEAR_BLOCKED;
    }
    if (clear_visible_count > 0) {
        return RUNTIME_NATIVE_3D_DIRECT_LIGHT_VISIBILITY_CLEAR_VISIBLE;
    }
    if (no_trace_count > 0) {
        return RUNTIME_NATIVE_3D_DIRECT_LIGHT_VISIBILITY_NO_TRACE;
    }
    return RUNTIME_NATIVE_3D_DIRECT_LIGHT_VISIBILITY_UNKNOWN;
}

/* The current native path uses the same fixed primary ray for every lighting
 * subpass. These material guides therefore match its beauty footprint. Future
 * camera jitter / motion blur must accumulate guides with the beauty samples. */
void RuntimeNative3DFeatureBuffer_RecordSamplingStatistics(
    const RuntimeNative3DFeatureBuffer* buffer, const float* raw_m2, const uint16_t* counts) {
    if (!buffer || !buffer->varianceOfMeanBuffer || !buffer->sampleCountBuffer || !raw_m2 || !counts) return;
    size_t pixels = (size_t)buffer->width * (size_t)buffer->height;
    for (size_t i = 0; i < pixels; ++i) {
        buffer->sampleCountBuffer[i] = counts[i];
        buffer->varianceOfMeanBuffer[i] = variance_of_mean(raw_m2 + i * 3u, counts[i]);
    }
}
