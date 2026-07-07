#include "render/runtime_native_3d_feature_buffer.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_material_payload_3d.h"

void RuntimeNative3DFeatureBuffer_Init(RuntimeNative3DFeatureBuffer* buffer) {
    if (!buffer) return;
    memset(buffer, 0, sizeof(*buffer));
}

void RuntimeNative3DFeatureBuffer_Free(RuntimeNative3DFeatureBuffer* buffer) {
    if (!buffer) return;
    free(buffer->normalBuffer);
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

bool RuntimeNative3DFeatureBuffer_Ensure(RuntimeNative3DFeatureBuffer* buffer,
                                         int width,
                                         int height) {
    float* normals = NULL;
    float* depths = NULL;
    float* reflectivity = NULL;
    float* roughness = NULL;
    float* transparency = NULL;
    unsigned char* hit_mask = NULL;
    unsigned char* direct_light_visibility = NULL;
    int* triangle_index = NULL;
    int* scene_object_index = NULL;
    size_t pixel_count = 0;
    if (!buffer || width <= 0 || height <= 0) return false;
    if (buffer->normalBuffer &&
        buffer->depthBuffer &&
        buffer->reflectivityBuffer &&
        buffer->roughnessBuffer &&
        buffer->transparencyBuffer &&
        buffer->hitMaskBuffer &&
        buffer->directLightVisibilityOutcomeBuffer &&
        buffer->triangleIndexBuffer &&
        buffer->sceneObjectIndexBuffer &&
        buffer->width == width &&
        buffer->height == height) {
        return true;
    }

    pixel_count = (size_t)width * (size_t)height;
    normals = (float*)calloc(pixel_count * 3u, sizeof(*normals));
    depths = (float*)calloc(pixel_count, sizeof(*depths));
    reflectivity = (float*)calloc(pixel_count, sizeof(*reflectivity));
    roughness = (float*)calloc(pixel_count, sizeof(*roughness));
    transparency = (float*)calloc(pixel_count, sizeof(*transparency));
    hit_mask = (unsigned char*)calloc(pixel_count, sizeof(*hit_mask));
    direct_light_visibility =
        (unsigned char*)calloc(pixel_count, sizeof(*direct_light_visibility));
    triangle_index = (int*)calloc(pixel_count, sizeof(*triangle_index));
    scene_object_index = (int*)calloc(pixel_count, sizeof(*scene_object_index));
    if (!normals || !depths || !reflectivity || !roughness || !transparency || !hit_mask ||
        !direct_light_visibility || !triangle_index || !scene_object_index) {
        free(normals);
        free(depths);
        free(reflectivity);
        free(roughness);
        free(transparency);
        free(hit_mask);
        free(direct_light_visibility);
        free(triangle_index);
        free(scene_object_index);
        return false;
    }

    free(buffer->normalBuffer);
    free(buffer->depthBuffer);
    free(buffer->reflectivityBuffer);
    free(buffer->roughnessBuffer);
    free(buffer->transparencyBuffer);
    free(buffer->hitMaskBuffer);
    free(buffer->directLightVisibilityOutcomeBuffer);
    free(buffer->triangleIndexBuffer);
    free(buffer->sceneObjectIndexBuffer);
    buffer->normalBuffer = normals;
    buffer->depthBuffer = depths;
    buffer->reflectivityBuffer = reflectivity;
    buffer->roughnessBuffer = roughness;
    buffer->transparencyBuffer = transparency;
    buffer->hitMaskBuffer = hit_mask;
    buffer->directLightVisibilityOutcomeBuffer = direct_light_visibility;
    buffer->triangleIndexBuffer = triangle_index;
    buffer->sceneObjectIndexBuffer = scene_object_index;
    buffer->width = width;
    buffer->height = height;
    runtime_native_3d_feature_buffer_clear_identity(buffer, pixel_count);
    return true;
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
    if (!RuntimeNative3DFeatureBuffer_Ensure(buffer, region_width, region_height)) {
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
