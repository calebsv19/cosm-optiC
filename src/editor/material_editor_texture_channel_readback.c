#include "editor/material_editor_internal.h"

#include <stdio.h>
#include <string.h>

#include "editor/material_editor_authored_texture_binding.h"
#include "editor/scene_editor_material_stack.h"
#include "material/material.h"
#include "render/runtime_material_authored_texture_3d.h"

static bool material_editor_channel_summary_has(const char* summary, const char* channel) {
    size_t channel_len = channel ? strlen(channel) : 0u;
    const char* cursor = summary;
    if (!summary || !channel || channel_len == 0u) return false;
    while ((cursor = strstr(cursor, channel)) != NULL) {
        char before = cursor == summary ? ' ' : cursor[-1];
        char after = cursor[channel_len];
        if ((before == ' ' || before == '\0') && (after == ' ' || after == '\0')) {
            return true;
        }
        cursor += channel_len;
    }
    return false;
}

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

static const char* material_editor_glass_layer_channel_intent(
    RuntimeMaterialTextureLayerKind kind) {
    switch (kind) {
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID:
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE:
            return "Base glass: tint from color/base_color, roughness drives frost, transmission remains guarded.";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG:
            return "Fog: roughness/frost and clarity-loss intent; transmission stays preset/readback.";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SCRATCHES:
            return "Scratches: roughness/spec breakup; normal/bump refs remain future detail.";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL:
            return "Oil: specular streak/tint intent plus roughness variation on the active overlay.";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME:
            return "Grime: tint/darkening plus clarity or coverage intent on the active overlay.";
        default:
            return "Selected layer maps through generic stack response; no Glass-specific channel promotion.";
    }
}

static void material_editor_build_glass_authored_mapping(
    MaterialEditorTextureChannelReadback* readback,
    const char* channel_summary) {
    char authored[320];
    char* cursor = authored;
    size_t remaining = sizeof(authored);
    int written = 0;
    if (!readback) return;
    authored[0] = '\0';
#define APPEND_INTENT(text)                                                                  \
    do {                                                                                     \
        int n = snprintf(cursor, remaining, "%s%s", written > 0 ? "; " : "", (text));      \
        if (n > 0) {                                                                         \
            size_t step = (size_t)n < remaining ? (size_t)n : remaining - 1u;                \
            cursor += step;                                                                  \
            remaining -= step;                                                               \
            written += 1;                                                                    \
        }                                                                                    \
    } while (0)
    if (material_editor_channel_summary_has(channel_summary, "base_color.rgb")) {
        APPEND_INTENT("base_color.rgb -> glass tint");
    }
    if (material_editor_channel_summary_has(channel_summary, "base_color.alpha_mask") ||
        material_editor_channel_summary_has(channel_summary, "opacity.coverage")) {
        APPEND_INTENT("alpha/coverage -> physical coverage only");
    }
    if (material_editor_channel_summary_has(channel_summary, "roughness.scalar")) {
        APPEND_INTENT("roughness.scalar -> clarity/frost");
    }
    if (material_editor_channel_summary_has(channel_summary, "transmission.weight")) {
        APPEND_INTENT("transmission.weight -> guarded transmission intent");
    }
    if (material_editor_channel_summary_has(channel_summary, "normal.tangent") ||
        material_editor_channel_summary_has(channel_summary, "bump.height")) {
        APPEND_INTENT("normal/bump -> future scratch/frost detail");
    }
    if (written == 0) {
        snprintf(readback->glass_authored_mapping,
                 sizeof(readback->glass_authored_mapping),
                 "Authored channels present, but none map to promoted Glass channel intent.");
    } else {
        snprintf(readback->glass_authored_mapping,
                 sizeof(readback->glass_authored_mapping),
                 "%s",
                 authored);
    }
#undef APPEND_INTENT
}

static void material_editor_build_glass_channel_mapping(
    int focused_object_index,
    const char* channel_summary,
    MaterialEditorTextureChannelReadback* readback) {
    const SceneObject* obj = NULL;
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialTextureLayer layer = {0};
    int active_index = 0;
    if (!readback || focused_object_index < 0 || focused_object_index >= sceneSettings.objectCount) {
        return;
    }
    obj = &sceneSettings.sceneObjects[focused_object_index];
    if (obj->material_id != MATERIAL_PRESET_TRANSPARENT) return;
    if (!material_editor_get_active_layer(obj, &stack, &layer, &active_index)) return;

    readback->has_glass_mapping = true;
    if (readback->has_authored_channels) {
        material_editor_build_glass_authored_mapping(readback, channel_summary);
    } else {
        snprintf(readback->glass_authored_mapping,
                 sizeof(readback->glass_authored_mapping),
                 "No authored channels: Glass uses preset transparency plus selected stack layer intent.");
    }
    snprintf(readback->glass_procedural_mapping,
             sizeof(readback->glass_procedural_mapping),
             "%s",
             material_editor_glass_layer_channel_intent(layer.kind));
    snprintf(readback->glass_deferred_mapping,
             sizeof(readback->glass_deferred_mapping),
             "Opacity.coverage is cutout/coverage; transmission.weight is readback intent until owner promotion.");
    (void)stack;
    (void)active_index;
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

    material_editor_build_glass_channel_mapping(focused_object_index,
                                                channel_summary,
                                                out_readback);

    return out_readback->has_authored_channels || out_readback->has_procedural_source;
}
