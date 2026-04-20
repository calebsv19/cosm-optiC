#include "render/text_font_cache.h"

#include "render/text_font_quality.h"
#include "ui/shared_theme_font_adapter.h"

#include <stdio.h>
#include <string.h>

typedef struct RayTracingFontCacheEntry {
    char path[384];
    int point_size;
    TTF_Font* font;
} RayTracingFontCacheEntry;

static const char* k_system_font_path = "/System/Library/Fonts/Supplemental/Arial.ttf";
static const char* k_bundled_font_path = "config/default.ttf";
static RayTracingFontCacheEntry g_font_cache[256];
static size_t g_font_cache_count = 0;

static bool ensure_ttf_initialized(void) {
    if (TTF_WasInit() != 0) {
        return true;
    }
    if (TTF_Init() != 0) {
        fprintf(stderr, "[font_cache] TTF_Init failed: %s\n", TTF_GetError());
        return false;
    }
    return true;
}

static bool same_path(const char* a, const char* b) {
    if (!a || !b) {
        return false;
    }
    return strcmp(a, b) == 0;
}

static TTF_Font* find_cached_font(const char* path, int point_size) {
    size_t i = 0;
    for (i = 0; i < g_font_cache_count; ++i) {
        RayTracingFontCacheEntry* entry = &g_font_cache[i];
        if (entry->point_size == point_size && same_path(entry->path, path)) {
            return entry->font;
        }
    }
    return NULL;
}

static void cache_font(const char* path, int point_size, TTF_Font* font) {
    RayTracingFontCacheEntry* entry = NULL;
    if (!path || !font || point_size <= 0) {
        return;
    }
    if (g_font_cache_count >= (sizeof(g_font_cache) / sizeof(g_font_cache[0]))) {
        fprintf(stderr,
                "[font_cache] capacity reached; using uncached font for path=%s size=%d\n",
                path,
                point_size);
        return;
    }
    entry = &g_font_cache[g_font_cache_count++];
    strncpy(entry->path, path, sizeof(entry->path) - 1);
    entry->path[sizeof(entry->path) - 1] = '\0';
    entry->point_size = point_size;
    entry->font = font;
}

static TTF_Font* open_cached_font_with_fallback(const char* preferred_path, int point_size) {
    const char* candidates[3];
    size_t i = 0;
    TTF_Font* font = NULL;
    if (!ensure_ttf_initialized()) {
        return NULL;
    }
    if (point_size < 1) {
        point_size = 1;
    }

    candidates[0] = preferred_path && preferred_path[0] ? preferred_path : k_system_font_path;
    candidates[1] = k_system_font_path;
    candidates[2] = k_bundled_font_path;

    for (i = 0; i < 3; ++i) {
        const char* candidate = candidates[i];
        bool already_seen = false;
        size_t j = 0;
        if (!candidate || !candidate[0]) {
            continue;
        }
        for (j = 0; j < i; ++j) {
            if (same_path(candidate, candidates[j])) {
                already_seen = true;
                break;
            }
        }
        if (already_seen) {
            continue;
        }

        font = find_cached_font(candidate, point_size);
        if (font) {
            return font;
        }

        font = TTF_OpenFont(candidate, point_size);
        if (font) {
            ray_tracing_text_apply_font_quality(font);
            cache_font(candidate, point_size, font);
            return font;
        }
    }
    return NULL;
}

TTF_Font* ray_tracing_text_font_cache_get_ui_regular(int point_size) {
    char shared_font_path[256];
    int shared_point_size = point_size;
    const char* preferred_path = k_system_font_path;

    if (ray_tracing_shared_font_resolve_ui_regular(
            shared_font_path, sizeof(shared_font_path), &shared_point_size)) {
        preferred_path = shared_font_path;
    }
    return open_cached_font_with_fallback(preferred_path, point_size);
}

int ray_tracing_text_font_cache_ui_regular_base_point_size(int fallback_point_size) {
    char shared_font_path[256];
    int resolved_point_size = fallback_point_size;
    if (resolved_point_size < 1) {
        resolved_point_size = 1;
    }
    if (ray_tracing_shared_font_resolve_ui_regular(
            shared_font_path, sizeof(shared_font_path), &resolved_point_size) &&
        resolved_point_size > 0) {
        return resolved_point_size;
    }
    return fallback_point_size;
}

void ray_tracing_text_font_cache_shutdown(void) {
    size_t i = 0;
    for (i = 0; i < g_font_cache_count; ++i) {
        if (g_font_cache[i].font) {
            TTF_CloseFont(g_font_cache[i].font);
            g_font_cache[i].font = NULL;
        }
    }
    g_font_cache_count = 0;
}
