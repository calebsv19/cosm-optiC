#include "render/text_font_cache.h"

#include "render/font_bridge.h"
#include "render/text_draw.h"

#include <SDL2/SDL.h>
#include <stdbool.h>
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

static int path_is_absolute(const char* path) {
    if (!path || !path[0]) {
        return 0;
    }
#if defined(_WIN32)
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':' &&
        (path[2] == '\\' || path[2] == '/')) {
        return 1;
    }
#endif
    return path[0] == '/';
}

static TTF_Font* open_font_with_search(const char* font_path, int point_size, char* out_path, size_t out_path_cap) {
    TTF_Font* font = NULL;
    char* base_path = NULL;
    int depth = 0;

    if (out_path && out_path_cap > 0) {
        out_path[0] = '\0';
    }
    if (!font_path || !font_path[0] || !ensure_ttf_initialized()) {
        return NULL;
    }
    if (point_size < 1) {
        point_size = 1;
    }

    font = TTF_OpenFont(font_path, point_size);
    if (font) {
        if (out_path && out_path_cap > 0) {
            strncpy(out_path, font_path, out_path_cap - 1);
            out_path[out_path_cap - 1] = '\0';
        }
        return font;
    }

    if (strncmp(font_path, "shared/", 7) == 0) {
        char adjusted[384];
        snprintf(adjusted, sizeof(adjusted), "../%s", font_path);
        font = TTF_OpenFont(adjusted, point_size);
        if (font) {
            if (out_path && out_path_cap > 0) {
                strncpy(out_path, adjusted, out_path_cap - 1);
                out_path[out_path_cap - 1] = '\0';
            }
            return font;
        }
    }

    if (path_is_absolute(font_path)) {
        return NULL;
    }

    base_path = SDL_GetBasePath();
    if (!base_path || !base_path[0]) {
        if (base_path) {
            SDL_free(base_path);
        }
        return NULL;
    }

    for (depth = 0; depth <= 8; ++depth) {
        char candidate[2048];
        int written = snprintf(candidate, sizeof(candidate), "%s", base_path);
        int offset = written;
        int i = 0;

        if (written <= 0 || (size_t)written >= sizeof(candidate)) {
            continue;
        }

        for (i = 0; i < depth; ++i) {
            written = snprintf(candidate + offset,
                               sizeof(candidate) - (size_t)offset,
                               "../");
            if (written <= 0 || (size_t)written >= (sizeof(candidate) - (size_t)offset)) {
                offset = -1;
                break;
            }
            offset += written;
        }
        if (offset < 0) {
            continue;
        }

        written = snprintf(candidate + offset,
                           sizeof(candidate) - (size_t)offset,
                           "%s",
                           font_path);
        if (written <= 0 || (size_t)written >= (sizeof(candidate) - (size_t)offset)) {
            continue;
        }

        font = TTF_OpenFont(candidate, point_size);
        if (font) {
            if (out_path && out_path_cap > 0) {
                strncpy(out_path, candidate, out_path_cap - 1);
                out_path[out_path_cap - 1] = '\0';
            }
            SDL_free(base_path);
            return font;
        }
    }

    SDL_free(base_path);
    return NULL;
}

static TTF_Font* open_cached_font_from_resolved(const RayTracingResolvedFont* resolved) {
    const char* candidates[3];
    size_t i = 0;

    if (!resolved) {
        return NULL;
    }

    candidates[0] = resolved->resolved_path[0] ? resolved->resolved_path : NULL;
    candidates[1] = k_system_font_path;
    candidates[2] = k_bundled_font_path;

    for (i = 0; i < 3; ++i) {
        const char* candidate = candidates[i];
        char loaded_path[384];
        TTF_Font* font = NULL;
        if (!candidate || !candidate[0]) {
            continue;
        }
        if (find_cached_font(candidate, resolved->text_run.logical_point_size)) {
            return find_cached_font(candidate, resolved->text_run.logical_point_size);
        }
        font = open_font_with_search(candidate,
                                     resolved->text_run.logical_point_size,
                                     loaded_path,
                                     sizeof(loaded_path));
        if (font) {
            TTF_SetFontStyle(font, TTF_STYLE_NORMAL);
            TTF_SetFontHinting(font, TTF_HINTING_LIGHT);
            TTF_SetFontKerning(font, resolved->text_run.kerning_enabled);
            ray_tracing_text_register_font_source(font,
                                                  loaded_path[0] ? loaded_path : candidate,
                                                  resolved->text_run.logical_point_size,
                                                  resolved->text_run.logical_point_size,
                                                  resolved->text_run.kerning_enabled);
            cache_font(loaded_path[0] ? loaded_path : candidate,
                       resolved->text_run.logical_point_size,
                       font);
            return font;
        }
    }
    return NULL;
}

TTF_Font* ray_tracing_text_font_cache_get_ui_regular(SDL_Renderer* renderer,
                                                     int logical_point_size,
                                                     int min_point_size) {
    RayTracingResolvedFont resolved;
    if (ray_tracing_font_bridge_resolve(renderer,
                                        RAY_TRACING_FONT_SLOT_UI_REGULAR,
                                        logical_point_size,
                                        min_point_size,
                                        &resolved).code != CORE_OK) {
        return NULL;
    }
    return open_cached_font_from_resolved(&resolved);
}

int ray_tracing_text_font_cache_ui_regular_base_point_size(int fallback_point_size) {
    return ray_tracing_font_bridge_base_point_size(RAY_TRACING_FONT_SLOT_UI_REGULAR,
                                                   fallback_point_size);
}

void ray_tracing_text_font_cache_shutdown(void) {
    size_t i = 0;
    for (i = 0; i < g_font_cache_count; ++i) {
        if (g_font_cache[i].font) {
            ray_tracing_text_unregister_font_source(g_font_cache[i].font);
            TTF_CloseFont(g_font_cache[i].font);
            g_font_cache[i].font = NULL;
        }
    }
    g_font_cache_count = 0;
}
