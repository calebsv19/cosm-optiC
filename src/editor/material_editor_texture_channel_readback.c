#include "editor/material_editor_internal.h"

#include <stdio.h>
#include <string.h>

#include "editor/material_editor_authored_texture_binding.h"
#include "render/runtime_material_authored_texture_3d.h"

static void material_editor_channel_append(char* out,
                                           size_t out_size,
                                           const char* channel,
                                           int* count) {
    size_t used = out ? strlen(out) : 0u;
    if (!out || out_size == 0u || !channel || !channel[0]) return;
    snprintf(out + used,
             out_size - used,
             "%s%s",
             used > 0u ? " " : "",
             channel);
    if (count) *count += 1;
}

static void material_editor_channel_classify(MaterialEditorTextureChannelReadback* readback,
                                             const char* channel) {
    if (!readback || !channel || !channel[0]) return;
    if (RuntimeMaterialAuthoredTextureChannelIsVisual(channel)) {
        material_editor_channel_append(readback->visual_channels,
                                       sizeof(readback->visual_channels),
                                       channel,
                                       &readback->visual_count);
    } else if (RuntimeMaterialAuthoredTextureChannelIsPhysicalScalar(channel)) {
        material_editor_channel_append(readback->physical_channels,
                                       sizeof(readback->physical_channels),
                                       channel,
                                       &readback->physical_count);
    } else if (RuntimeMaterialAuthoredTextureChannelIsShadingNormal(channel)) {
        material_editor_channel_append(readback->future_channels,
                                       sizeof(readback->future_channels),
                                       channel,
                                       &readback->future_count);
    }
}

bool MaterialEditorBuildTextureChannelReadback(int focused_object_index,
                                               MaterialEditorTextureChannelReadback* out_readback) {
    MaterialEditorActiveLayerReadback layer = {0};
    char channel_summary[256];
    char token[80];
    const char* cursor = channel_summary;
    if (!out_readback) return false;
    memset(out_readback, 0, sizeof(*out_readback));
    snprintf(out_readback->visual_channels, sizeof(out_readback->visual_channels), "none");
    snprintf(out_readback->physical_channels, sizeof(out_readback->physical_channels), "none");
    snprintf(out_readback->future_channels, sizeof(out_readback->future_channels), "none");
    snprintf(out_readback->deferred_channels,
             sizeof(out_readback->deferred_channels),
             "displacement.height deferred");

    if (MaterialEditorAuthoredTextureBindingGetChannelSummary(focused_object_index,
                                                              channel_summary,
                                                              sizeof(channel_summary))) {
        out_readback->has_authored_channels = true;
        out_readback->visual_channels[0] = '\0';
        out_readback->physical_channels[0] = '\0';
        out_readback->future_channels[0] = '\0';
        while (*cursor) {
            size_t len = 0u;
            while (*cursor == ' ') cursor++;
            while (cursor[len] && cursor[len] != ' ' && len + 1u < sizeof(token)) {
                token[len] = cursor[len];
                len += 1u;
            }
            token[len] = '\0';
            material_editor_channel_classify(out_readback, token);
            cursor += len;
            while (*cursor && *cursor != ' ') cursor++;
        }
        if (!out_readback->visual_channels[0]) {
            snprintf(out_readback->visual_channels, sizeof(out_readback->visual_channels), "none");
        }
        if (!out_readback->physical_channels[0]) {
            snprintf(out_readback->physical_channels, sizeof(out_readback->physical_channels), "none");
        }
        if (!out_readback->future_channels[0]) {
            snprintf(out_readback->future_channels, sizeof(out_readback->future_channels), "none");
        }
    }

    if (MaterialEditorBuildActiveLayerReadback(&layer)) {
        snprintf(out_readback->procedural_source,
                 sizeof(out_readback->procedural_source),
                 "%s | placement + procedural params",
                 layer.detail);
        out_readback->has_procedural_source = true;
    } else {
        snprintf(out_readback->procedural_source,
                 sizeof(out_readback->procedural_source),
                 "legacy object texture fallback when no stack layer exists");
    }

    return out_readback->has_authored_channels || out_readback->has_procedural_source;
}
