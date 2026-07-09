#include "render/runtime_material_authored_texture_3d_internal.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
void runtime_material_authored_texture_copy_text(char* dst,
                                                 size_t dst_size,
                                                 const char* src) {
    if (!dst || dst_size == 0u) return;
    if (!src) src = "";
    (void)snprintf(dst, dst_size, "%s", src);
}

double runtime_material_authored_texture_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

void runtime_material_authored_texture_face_metadata_reset(
    RuntimeMaterialAuthoredTextureFaceMetadata* metadata) {
    int i = 0;
    if (!metadata) return;
    memset(metadata, 0, sizeof(*metadata));
    for (i = 0; i < RUNTIME_MATERIAL_AUTHORED_TEXTURE_FACE_EDGE_COUNT; ++i) {
        metadata->adjacentFaceGroupIndices[i] = -1;
    }
}

bool RuntimeMaterialAuthoredTextureChannelNameSupported(const char* channel) {
    if (!channel || !channel[0]) return false;
    return RuntimeMaterialAuthoredTextureChannelIsVisual(channel) ||
           RuntimeMaterialAuthoredTextureChannelIsPhysicalScalar(channel) ||
           RuntimeMaterialAuthoredTextureChannelIsShadingNormal(channel);
}

bool RuntimeMaterialAuthoredTextureChannelIsVisual(const char* channel) {
    if (!channel) return false;
    return strcmp(channel, "base_color.rgb") == 0 ||
           strcmp(channel, "base_color.alpha_mask") == 0 ||
           strcmp(channel, "emission.color") == 0;
}

bool RuntimeMaterialAuthoredTextureChannelIsPhysicalScalar(const char* channel) {
    if (!channel) return false;
    return strcmp(channel, "opacity.coverage") == 0 ||
           strcmp(channel, "transmission.weight") == 0 ||
           strcmp(channel, "roughness.scalar") == 0 ||
           strcmp(channel, "reflectivity.compat") == 0 ||
           strcmp(channel, "specular.weight") == 0 ||
           strcmp(channel, "metallic.scalar") == 0 ||
           strcmp(channel, "emission.strength") == 0;
}

bool RuntimeMaterialAuthoredTextureChannelIsShadingNormal(const char* channel) {
    if (!channel) return false;
    return strcmp(channel, "normal.tangent") == 0 ||
           strcmp(channel, "bump.height") == 0;
}

bool RuntimeMaterialAuthoredTextureChannelIsDisplacement(const char* channel) {
    if (!channel) return false;
    return strcmp(channel, "displacement.height") == 0;
}

RuntimeMaterialAuthoredTextureBinding* runtime_material_authored_texture_binding_at(
    int scene_object_index) {
    if (scene_object_index < 0 || scene_object_index >= MAX_OBJECTS) {
        return NULL;
    }
    return &s_authored_texture_bindings[scene_object_index];
}

void runtime_material_authored_texture_face_reset(RuntimeMaterialAuthoredTextureFace* face) {
    if (!face) return;
    free(face->rgba);
    runtime_material_authored_texture_face_metadata_reset(&face->metadata);
    memset(face, 0, sizeof(*face));
    face->faceGroupIndex = -1;
    face->metadata.faceGroupIndex = -1;
    for (int i = 0; i < RUNTIME_MATERIAL_AUTHORED_TEXTURE_FACE_EDGE_COUNT; ++i) {
        face->metadata.adjacentFaceGroupIndices[i] = -1;
    }
}

void runtime_material_authored_texture_binding_reset(RuntimeMaterialAuthoredTextureBinding* binding) {
    int i = 0;
    if (!binding) return;
    for (i = 0; i < RUNTIME_MATERIAL_AUTHORED_TEXTURE_MAX_FACES; ++i) {
        runtime_material_authored_texture_face_reset(&binding->baseFaces[i]);
        runtime_material_authored_texture_face_reset(&binding->overlayFaces[i]);
    }
    memset(binding, 0, sizeof(*binding));
    binding->sceneObjectIndex = -1;
}

void runtime_material_authored_texture_binding_record_invalid(
    RuntimeMaterialAuthoredTextureBinding* binding,
    int scene_object_index,
    const char* manifest_path,
    const char* binding_mode,
    const char* reason) {
    if (!binding) return;
    runtime_material_authored_texture_binding_reset(binding);
    binding->sceneObjectIndex = scene_object_index;
    binding->invalidActive = true;
    runtime_material_authored_texture_copy_text(binding->invalidManifestPath,
                                                sizeof(binding->invalidManifestPath),
                                                manifest_path);
    runtime_material_authored_texture_copy_text(binding->invalidBindingMode,
                                                sizeof(binding->invalidBindingMode),
                                                binding_mode && binding_mode[0] ? binding_mode
                                                                                : "override");
    runtime_material_authored_texture_copy_text(binding->invalidReason,
                                                sizeof(binding->invalidReason),
                                                reason && reason[0] ? reason
                                                                    : "manifest validation failed");
}

void RuntimeMaterialAuthoredTextureResetAll(void) {
    int i = 0;
    for (i = 0; i < MAX_OBJECTS; ++i) {
        runtime_material_authored_texture_binding_reset(&s_authored_texture_bindings[i]);
    }
}

bool RuntimeMaterialAuthoredTextureClearBindingForObject(int scene_object_index) {
    RuntimeMaterialAuthoredTextureBinding* binding =
        runtime_material_authored_texture_binding_at(scene_object_index);
    if (!binding) {
        return false;
    }
    runtime_material_authored_texture_binding_reset(binding);
    return true;
}

bool RuntimeMaterialAuthoredTextureGetFaceChannels(
    int scene_object_index,
    int face_group_index,
    RuntimeMaterialAuthoredTextureChannelRef* out_channels,
    size_t max_channels,
    int* out_channel_count) {
    RuntimeMaterialAuthoredTextureBinding* binding =
        runtime_material_authored_texture_binding_at(scene_object_index);
    RuntimeMaterialAuthoredTextureFace* face = NULL;
    int count = 0;
    if (out_channel_count) {
        *out_channel_count = 0;
    }
    if (!binding || !binding->active || face_group_index < 0 ||
        face_group_index >= RUNTIME_MATERIAL_AUTHORED_TEXTURE_MAX_FACES) {
        return false;
    }
    face = &binding->baseFaces[face_group_index];
    if (!face->active || face->metadata.channelRefCount <= 0) {
        return false;
    }
    count = face->metadata.channelRefCount;
    if (out_channel_count) {
        *out_channel_count = count;
    }
    if (out_channels && max_channels > 0u) {
        size_t copy_count = (size_t)count < max_channels ? (size_t)count : max_channels;
        for (size_t i = 0u; i < copy_count; ++i) {
            out_channels[i] = face->metadata.channelRefs[i];
        }
    }
    return true;
}

static bool runtime_material_authored_texture_summary_contains(const char* summary,
                                                               const char* channel) {
    const char* cursor = summary;
    size_t channel_len = channel ? strlen(channel) : 0u;
    if (!summary || !channel || channel_len == 0u) return false;
    while ((cursor = strstr(cursor, channel)) != NULL) {
        char before = cursor == summary ? '\0' : cursor[-1];
        char after = cursor[channel_len];
        if ((before == '\0' || before == ' ') && (after == '\0' || after == ' ')) {
            return true;
        }
        cursor += channel_len;
    }
    return false;
}

bool RuntimeMaterialAuthoredTextureGetChannelSummary(int scene_object_index,
                                                     char* out_summary,
                                                     size_t out_summary_size) {
    RuntimeMaterialAuthoredTextureBinding* binding =
        runtime_material_authored_texture_binding_at(scene_object_index);
    bool wrote = false;
    if (!out_summary || out_summary_size == 0u) return false;
    out_summary[0] = '\0';
    if (!binding || !binding->active) {
        return false;
    }
    for (int face_index = 0; face_index < RUNTIME_MATERIAL_AUTHORED_TEXTURE_MAX_FACES;
         ++face_index) {
        RuntimeMaterialAuthoredTextureFace* face = &binding->baseFaces[face_index];
        if (!face->active || face->metadata.channelRefCount <= 0) {
            continue;
        }
        for (int channel_index = 0; channel_index < face->metadata.channelRefCount;
             ++channel_index) {
            const char* channel = face->metadata.channelRefs[channel_index].channel;
            size_t used = strlen(out_summary);
            if (!channel[0] ||
                runtime_material_authored_texture_summary_contains(out_summary, channel)) {
                continue;
            }
            snprintf(out_summary + used,
                     out_summary_size - used,
                     "%s%s",
                     used > 0u ? " " : "",
                     channel);
            wrote = true;
            if (strlen(out_summary) + 2u >= out_summary_size) {
                return wrote;
            }
        }
    }
    return wrote;
}

static void runtime_material_authored_texture_sample_channel(
    const RuntimeMaterialAuthoredTextureFace* face,
    double x,
    double y,
    double* out_r,
    double* out_g,
    double* out_b,
    double* out_a) {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    double tx = 0.0;
    double ty = 0.0;
    double accum_r = 0.0;
    double accum_g = 0.0;
    double accum_b = 0.0;
    double accum_a = 0.0;
    int sx[2];
    int sy[2];
    double wx[2];
    double wy[2];
    if (!face || !face->rgba || face->width <= 0 || face->height <= 0) {
        return;
    }
    x = runtime_material_authored_texture_clamp01(x) * (double)(face->width - 1);
    y = runtime_material_authored_texture_clamp01(y) * (double)(face->height - 1);
    x0 = (int)floor(x);
    y0 = (int)floor(y);
    x1 = x0 < face->width - 1 ? x0 + 1 : x0;
    y1 = y0 < face->height - 1 ? y0 + 1 : y0;
    tx = x - (double)x0;
    ty = y - (double)y0;
    sx[0] = x0;
    sx[1] = x1;
    sy[0] = y0;
    sy[1] = y1;
    wx[0] = 1.0 - tx;
    wx[1] = tx;
    wy[0] = 1.0 - ty;
    wy[1] = ty;
    for (int iy = 0; iy < 2; ++iy) {
        for (int ix = 0; ix < 2; ++ix) {
            size_t pixel_index = ((size_t)sy[iy] * (size_t)face->width + (size_t)sx[ix]) * 4u;
            double weight = wx[ix] * wy[iy];
            accum_r += ((double)face->rgba[pixel_index + 0u] / 255.0) * weight;
            accum_g += ((double)face->rgba[pixel_index + 1u] / 255.0) * weight;
            accum_b += ((double)face->rgba[pixel_index + 2u] / 255.0) * weight;
            accum_a += ((double)face->rgba[pixel_index + 3u] / 255.0) * weight;
        }
    }
    if (out_r) *out_r = accum_r;
    if (out_g) *out_g = accum_g;
    if (out_b) *out_b = accum_b;
    if (out_a) *out_a = accum_a;
}

static bool runtime_material_authored_texture_sample_face_array(
    const RuntimeMaterialAuthoredTextureBinding* binding,
    const RuntimeMaterialAuthoredTextureFace* faces,
    int scene_object_index,
    int face_group_index,
    double u,
    double v,
    RuntimeMaterialAuthoredTextureSample* out_sample) {
    const RuntimeMaterialAuthoredTextureFace* face = NULL;
    if (out_sample) {
        memset(out_sample, 0, sizeof(*out_sample));
        out_sample->sceneObjectIndex = scene_object_index;
        out_sample->faceGroupIndex = face_group_index;
        out_sample->u = u;
        out_sample->v = v;
    }
    if (!binding || !binding->active || !faces || face_group_index < 0 ||
        face_group_index >= RUNTIME_MATERIAL_AUTHORED_TEXTURE_MAX_FACES) {
        return false;
    }
    face = &faces[face_group_index];
    if (!face->active || !face->rgba) {
        return false;
    }
    if (out_sample) {
        out_sample->active = true;
        runtime_material_authored_texture_sample_channel(face,
                                                         u,
                                                         v,
                                                         &out_sample->colorR,
                                                         &out_sample->colorG,
                                                         &out_sample->colorB,
                                                         &out_sample->alpha);
    }
    return true;
}

bool RuntimeMaterialAuthoredTextureSampleFace(int scene_object_index,
                                              int face_group_index,
                                              double u,
                                              double v,
                                              RuntimeMaterialAuthoredTextureSample* out_sample) {
    RuntimeMaterialAuthoredTextureBinding* binding =
        runtime_material_authored_texture_binding_at(scene_object_index);
    return runtime_material_authored_texture_sample_face_array(binding,
                                                               binding ? binding->baseFaces : NULL,
                                                               scene_object_index,
                                                               face_group_index,
                                                               u,
                                                               v,
                                                               out_sample);
}

bool RuntimeMaterialAuthoredTextureSampleOverlayFace(int scene_object_index,
                                                     int face_group_index,
                                                     double u,
                                                     double v,
                                                     RuntimeMaterialAuthoredTextureSample* out_sample) {
    RuntimeMaterialAuthoredTextureBinding* binding =
        runtime_material_authored_texture_binding_at(scene_object_index);
    return runtime_material_authored_texture_sample_face_array(binding,
                                                               binding ? binding->overlayFaces : NULL,
                                                               scene_object_index,
                                                               face_group_index,
                                                               u,
                                                               v,
                                                               out_sample);
}
