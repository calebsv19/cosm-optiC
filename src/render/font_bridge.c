#include "render/font_bridge.h"

#include <SDL2/SDL.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config/config_manager.h"
#include "core_font.h"
#include "render/text_upload_policy.h"

typedef struct RayTracingFontBridgeSlotSpec {
    CoreFontRoleId role_id;
    CoreFontTextSizeTier text_tier;
    int min_point_size;
    const char* const* legacy_paths;
    size_t legacy_path_count;
} RayTracingFontBridgeSlotSpec;

static const char* const k_ui_regular_legacy_paths[] = {
    "/System/Library/Fonts/Supplemental/Arial.ttf",
    "config/default.ttf"
};

static CoreResult ray_tracing_font_bridge_invalid(const char* message) {
    CoreResult r = {CORE_ERR_INVALID_ARG, message};
    return r;
}

static int ray_tracing_font_bridge_parse_bool_env(const char* value, int* out_value) {
    char lowered[16];
    size_t i = 0;

    if (!value || !value[0] || !out_value) {
        return 0;
    }

    for (; value[i] && i < sizeof(lowered) - 1; ++i) {
        lowered[i] = (char)tolower((unsigned char)value[i]);
    }
    lowered[i] = '\0';

    if (strcmp(lowered, "1") == 0 || strcmp(lowered, "true") == 0 || strcmp(lowered, "yes") == 0 ||
        strcmp(lowered, "on") == 0) {
        *out_value = 1;
        return 1;
    }
    if (strcmp(lowered, "0") == 0 || strcmp(lowered, "false") == 0 || strcmp(lowered, "no") == 0 ||
        strcmp(lowered, "off") == 0) {
        *out_value = 0;
        return 1;
    }
    return 0;
}

static int ray_tracing_font_bridge_shared_enabled(void) {
    int enabled = 1;

    if (ray_tracing_font_bridge_parse_bool_env(getenv("RAY_TRACING_USE_SHARED_FONT"), &enabled)) {
        return enabled;
    }
    if (ray_tracing_font_bridge_parse_bool_env(getenv("RAY_TRACING_USE_SHARED_THEME_FONT"), &enabled)) {
        return enabled;
    }
    return 1;
}

static void ray_tracing_font_bridge_copy_text(char* dst, size_t dst_cap, const char* src) {
    if (!dst || dst_cap == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_cap - 1);
    dst[dst_cap - 1] = '\0';
}

static CoreFontPresetId ray_tracing_font_bridge_font_preset_id(void) {
    const char* preset_name = getenv("RAY_TRACING_FONT_PRESET");
    CoreFontPreset preset;

    if (preset_name && preset_name[0] &&
        core_font_get_preset_by_name(preset_name, &preset).code == CORE_OK) {
        return preset.id;
    }
    return CORE_FONT_PRESET_IDE;
}

static CoreResult ray_tracing_font_bridge_context_init(KitRenderContext* ctx) {
    CoreResult result;

    if (!ctx) {
        return ray_tracing_font_bridge_invalid("null context");
    }

    result = kit_render_context_init(ctx,
                                     KIT_RENDER_BACKEND_NULL,
                                     CORE_THEME_PRESET_GREYSCALE,
                                     ray_tracing_font_bridge_font_preset_id());
    if (result.code != CORE_OK) {
        return result;
    }

    result = kit_render_set_text_zoom_step(ctx, animSettings.textZoomStep);
    if (result.code != CORE_OK) {
        kit_render_context_shutdown(ctx);
        return result;
    }

    return core_result_ok();
}

static const RayTracingFontBridgeSlotSpec* ray_tracing_font_bridge_slot_spec(RayTracingFontSlot slot) {
    static const RayTracingFontBridgeSlotSpec specs[] = {
        {
            CORE_FONT_ROLE_UI_REGULAR,
            CORE_FONT_TEXT_SIZE_BASIC,
            6,
            k_ui_regular_legacy_paths,
            sizeof(k_ui_regular_legacy_paths) / sizeof(k_ui_regular_legacy_paths[0])
        }
    };

    if ((int)slot < 0 || (size_t)slot >= (sizeof(specs) / sizeof(specs[0]))) {
        return NULL;
    }
    return &specs[(size_t)slot];
}

CoreResult ray_tracing_font_bridge_resolve(SDL_Renderer* renderer,
                                           RayTracingFontSlot slot,
                                           int logical_point_size,
                                           int min_point_size,
                                           RayTracingResolvedFont* out_resolved) {
    const RayTracingFontBridgeSlotSpec* spec = ray_tracing_font_bridge_slot_spec(slot);
    KitRenderContext render_ctx;
    KitRenderResolvedTextRun text_run;
    CoreResult result;
    const char* shared_paths[2];
    size_t i = 0;
    int chosen_point_size = logical_point_size;

    if (!spec || !out_resolved) {
        return ray_tracing_font_bridge_invalid("invalid font bridge resolve request");
    }

    memset(out_resolved, 0, sizeof(*out_resolved));

    result = ray_tracing_font_bridge_context_init(&render_ctx);
    if (result.code != CORE_OK) {
        return result;
    }

    result = kit_render_resolve_text_run(&render_ctx,
                                         spec->role_id,
                                         spec->text_tier,
                                         ray_tracing_text_raster_scale(renderer),
                                         &text_run);
    kit_render_context_shutdown(&render_ctx);
    if (result.code != CORE_OK) {
        return result;
    }

    if (min_point_size < spec->min_point_size) {
        min_point_size = spec->min_point_size;
    }
    if (chosen_point_size <= 0) {
        chosen_point_size = text_run.logical_point_size;
    }
    if (chosen_point_size < min_point_size) {
        chosen_point_size = min_point_size;
    }

    text_run.logical_point_size = chosen_point_size;
    text_run.raster_point_size = ray_tracing_text_raster_point_size(renderer,
                                                                    chosen_point_size,
                                                                    min_point_size);
    text_run.raster_scale =
        (chosen_point_size > 0) ? ((float)text_run.raster_point_size / (float)chosen_point_size)
                                : 1.0f;
    text_run.render_scale = text_run.raster_scale;
    text_run.upload_filter = (text_run.raster_scale > 1.0f) ? KIT_RENDER_TEXT_UPLOAD_FILTER_NEAREST
                                                            : KIT_RENDER_TEXT_UPLOAD_FILTER_LINEAR;

    shared_paths[0] = text_run.role_spec.primary_path;
    shared_paths[1] = text_run.role_spec.fallback_path;
    if (ray_tracing_font_bridge_shared_enabled()) {
        for (i = 0; i < 2; ++i) {
            if (shared_paths[i] && shared_paths[i][0]) {
                ray_tracing_font_bridge_copy_text(out_resolved->resolved_path,
                                                  sizeof(out_resolved->resolved_path),
                                                  shared_paths[i]);
                out_resolved->used_shared_font = 1;
                break;
            }
        }
    }

    if (!out_resolved->resolved_path[0]) {
        for (i = 0; i < spec->legacy_path_count; ++i) {
            if (spec->legacy_paths[i] && spec->legacy_paths[i][0]) {
                ray_tracing_font_bridge_copy_text(out_resolved->resolved_path,
                                                  sizeof(out_resolved->resolved_path),
                                                  spec->legacy_paths[i]);
                out_resolved->used_shared_font = 0;
                break;
            }
        }
    }

    out_resolved->text_run = text_run;
    if (!out_resolved->resolved_path[0]) {
        return ray_tracing_font_bridge_invalid("failed to resolve bridge font path");
    }
    return core_result_ok();
}

int ray_tracing_font_bridge_base_point_size(RayTracingFontSlot slot, int fallback_point_size) {
    RayTracingResolvedFont resolved;
    if (ray_tracing_font_bridge_resolve(NULL, slot, 0, fallback_point_size, &resolved).code == CORE_OK &&
        resolved.text_run.logical_point_size > 0) {
        return resolved.text_run.logical_point_size;
    }
    return fallback_point_size;
}
