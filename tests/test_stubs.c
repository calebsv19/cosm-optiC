#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "editor/scene_editor.h"
#include "app/animation.h"
#include "engine/Render/render_pipeline.h"
#include "timer_hud/time_scope.h"

#if defined(__has_include)
#if __has_include("timer_hud/timer_hud_config.h")
#include "timer_hud/timer_hud_config.h"
#else
#define RAY_TRACING_TEST_TIMER_HUD_LEGACY_COMPAT 1
#endif
#else
#define RAY_TRACING_TEST_TIMER_HUD_LEGACY_COMPAT 1
#endif

#ifndef TIMESCOPE_TIMER_HUD_SESSION_TYPEDEF_DONE
#define TIMESCOPE_TIMER_HUD_SESSION_TYPEDEF_DONE 1
typedef struct TimerHUDSession TimerHUDSession;
#endif

typedef struct TimerHUDInitConfig TimerHUDInitConfig;

#if defined(__GNUC__) || defined(__clang__)
#define RAY_TRACING_TEST_WEAK __attribute__((weak))
#else
#define RAY_TRACING_TEST_WEAK
#endif

#if RAY_TRACING_TEST_TIMER_HUD_LEGACY_COMPAT
typedef enum TimerHUDVisualMode {
    TIMER_HUD_VISUAL_MODE_INVALID = -1,
    TIMER_HUD_VISUAL_MODE_TEXT_COMPACT = 0,
    TIMER_HUD_VISUAL_MODE_HISTORY_GRAPH = 1,
    TIMER_HUD_VISUAL_MODE_HYBRID = 2,
} TimerHUDVisualMode;
#endif

RAY_TRACING_TEST_WEAK TimerHUDSession* ts_session_create(void) { return (TimerHUDSession*)1; }
RAY_TRACING_TEST_WEAK void ts_session_destroy(TimerHUDSession* session) { (void)session; }
RAY_TRACING_TEST_WEAK TimerHUDSession* ts_default_session(void) { return (TimerHUDSession*)1; }
RAY_TRACING_TEST_WEAK void ts_session_register_backend(TimerHUDSession* session, const TimerHUDBackend* backend) {
    (void)session;
    (void)backend;
}
RAY_TRACING_TEST_WEAK bool ts_session_apply_init_config(TimerHUDSession* session, const TimerHUDInitConfig* config) {
    (void)session;
    (void)config;
    return true;
}
RAY_TRACING_TEST_WEAK void ts_session_init(TimerHUDSession* session) { (void)session; }
RAY_TRACING_TEST_WEAK void ts_session_shutdown(TimerHUDSession* session) { (void)session; }
RAY_TRACING_TEST_WEAK void ts_session_start_timer(TimerHUDSession* session, const char* name) {
    (void)session;
    (void)name;
}
RAY_TRACING_TEST_WEAK void ts_session_stop_timer(TimerHUDSession* session, const char* name) {
    (void)session;
    (void)name;
}
RAY_TRACING_TEST_WEAK void ts_session_frame_start(TimerHUDSession* session) { (void)session; }
RAY_TRACING_TEST_WEAK void ts_session_frame_end(TimerHUDSession* session) { (void)session; }
RAY_TRACING_TEST_WEAK void ts_session_emit_event(TimerHUDSession* session, const char* tag) {
    (void)session;
    (void)tag;
}
RAY_TRACING_TEST_WEAK void ts_session_render(TimerHUDSession* session) { (void)session; }
RAY_TRACING_TEST_WEAK void ts_session_set_hud_enabled(TimerHUDSession* session, bool enabled) {
    (void)session;
    (void)enabled;
}
RAY_TRACING_TEST_WEAK bool ts_session_is_hud_enabled(const TimerHUDSession* session) {
    (void)session;
    return false;
}
RAY_TRACING_TEST_WEAK bool ts_session_is_log_enabled(const TimerHUDSession* session) {
    (void)session;
    return false;
}
RAY_TRACING_TEST_WEAK bool ts_session_is_event_tagging_enabled(const TimerHUDSession* session) {
    (void)session;
    return false;
}
RAY_TRACING_TEST_WEAK const char* ts_session_get_hud_visual_mode(const TimerHUDSession* session) {
    (void)session;
    return "hybrid";
}
RAY_TRACING_TEST_WEAK const char* ts_session_get_log_filepath(const TimerHUDSession* session) {
    (void)session;
    return "timing.json";
}
RAY_TRACING_TEST_WEAK bool ts_session_set_hud_visual_mode_kind(TimerHUDSession* session, TimerHUDVisualMode mode) {
    (void)session;
    (void)mode;
    return true;
}
RAY_TRACING_TEST_WEAK TimerHUDSession* timer_hud_session(void) { return (TimerHUDSession*)1; }
RAY_TRACING_TEST_WEAK TimerHUDVisualMode ts_visual_mode_from_string(const char* mode) {
    if (!mode) return TIMER_HUD_VISUAL_MODE_INVALID;
    if (strcmp(mode, "text_compact") == 0) return TIMER_HUD_VISUAL_MODE_TEXT_COMPACT;
    if (strcmp(mode, "history_graph") == 0) return TIMER_HUD_VISUAL_MODE_HISTORY_GRAPH;
    if (strcmp(mode, "hybrid") == 0) return TIMER_HUD_VISUAL_MODE_HYBRID;
    return TIMER_HUD_VISUAL_MODE_INVALID;
}

RAY_TRACING_TEST_WEAK void ts_init(void) {}
RAY_TRACING_TEST_WEAK void ts_start_timer(const char* name) { (void)name; }
RAY_TRACING_TEST_WEAK void ts_stop_timer(const char* name) { (void)name; }
RAY_TRACING_TEST_WEAK void ts_frame_start(void) {}
RAY_TRACING_TEST_WEAK void ts_frame_end(void) {}
RAY_TRACING_TEST_WEAK void ts_emit_event(const char* tag) { (void)tag; }
RAY_TRACING_TEST_WEAK void ts_render(void) {}

// UI/editor symbols needed when linking the broad RayTracing test target.
RAY_TRACING_TEST_WEAK bool InitializeSceneEditor(SceneEditor* editor) { (void)editor; return true; }
RAY_TRACING_TEST_WEAK void SceneEditorLoop(SceneEditor* editor) { (void)editor; }
RAY_TRACING_TEST_WEAK void RunPreviewMode(void) {}
RAY_TRACING_TEST_WEAK void RunPreviewModeEmbedded(SDL_Window* host_window, SDL_Renderer* host_renderer) {
    (void)host_window;
    (void)host_renderer;
}

// Font symbols used by timer_hud_adapter.
RAY_TRACING_TEST_WEAK bool initFontSystem(void) { return true; }
RAY_TRACING_TEST_WEAK TTF_Font* getActiveFont(void) { return NULL; }
RAY_TRACING_TEST_WEAK void invalidateActiveFontHandle(void) {}

// TimerHUD global registration symbols.
RAY_TRACING_TEST_WEAK void ts_register_backend(const TimerHUDBackend* backend) { (void)backend; }
RAY_TRACING_TEST_WEAK void ts_set_settings_path(const char* path) { (void)path; }
RAY_TRACING_TEST_WEAK bool ts_apply_init_config(const TimerHUDInitConfig* config) { (void)config; return true; }

// Animation/render pipeline symbols used by ray_tracing2 in tests.
RAY_TRACING_TEST_WEAK bool AnimationUseFluidScene(void) { return false; }
int frameCounter = 0;
int loopCount = 0;
double currentTime = 0.0;
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
RAY_TRACING_TEST_WEAK void AnimationParseArgs(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
}
RAY_TRACING_TEST_WEAK void AnimationLoadRuntimeDefaults(void) {}
RAY_TRACING_TEST_WEAK int AnimationRunAppSession(void) { return 0; }
RAY_TRACING_TEST_WEAK double AnimationCurrentNormalizedT(void) { return 0.0; }
RAY_TRACING_TEST_WEAK int AnimationCurrentAbsoluteFrameIndex(void) { return 0; }
RAY_TRACING_TEST_WEAK int AnimationConfiguredPathFrameCount(void) { return 0; }
RAY_TRACING_TEST_WEAK RenderContext* getRenderContext(void) { return NULL; }
RAY_TRACING_TEST_WEAK bool render_begin_frame(void) { return true; }
RAY_TRACING_TEST_WEAK bool render_end_frame(void) { return true; }
RAY_TRACING_TEST_WEAK bool render_device_lost(void) { return false; }
RAY_TRACING_TEST_WEAK void render_set_clear_color(SDL_Renderer* renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    (void)renderer;
    (void)r;
    (void)g;
    (void)b;
    (void)a;
}

// Job-runner support helpers are linked by app status/path objects in the broad test target.
RAY_TRACING_TEST_WEAK bool copy_string(char* dst, size_t dst_size, const char* src) {
    if (!dst || dst_size == 0u || !src) return false;
    if (snprintf(dst, dst_size, "%s", src) >= (int)dst_size) {
        dst[0] = '\0';
        return false;
    }
    return true;
}

RAY_TRACING_TEST_WEAK bool read_text_file(const char* path, char** out_text) {
    FILE* file = NULL;
    long size = 0;
    char* text = NULL;
    size_t read_count = 0u;
    if (!path || !path[0] || !out_text) return false;
    *out_text = NULL;
    file = fopen(path, "rb");
    if (!file) return false;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }
    text = (char*)malloc((size_t)size + 1u);
    if (!text) {
        fclose(file);
        return false;
    }
    read_count = fread(text, 1u, (size_t)size, file);
    fclose(file);
    if (read_count != (size_t)size) {
        free(text);
        return false;
    }
    text[size] = '\0';
    *out_text = text;
    return true;
}

RAY_TRACING_TEST_WEAK void json_write_string(FILE* file, const char* value) {
    const unsigned char* cursor = (const unsigned char*)(value ? value : "");
    if (!file) return;
    fputc('"', file);
    while (*cursor) {
        switch (*cursor) {
            case '\\': fputs("\\\\", file); break;
            case '"': fputs("\\\"", file); break;
            case '\n': fputs("\\n", file); break;
            case '\r': fputs("\\r", file); break;
            case '\t': fputs("\\t", file); break;
            default:
                if (*cursor < 0x20u) {
                    fprintf(file, "\\u%04x", (unsigned int)*cursor);
                } else {
                    fputc((int)*cursor, file);
                }
                break;
        }
        cursor++;
    }
    fputc('"', file);
}

RAY_TRACING_TEST_WEAK bool utc_now_string(char* out, size_t out_size) {
    time_t now = 0;
    struct tm tm_utc;
    if (!out || out_size == 0u) return false;
    out[0] = '\0';
    now = time(NULL);
    if (now == (time_t)-1) return false;
#if defined(__APPLE__) || defined(__unix__)
    if (gmtime_r(&now, &tm_utc) == NULL) return false;
#else
    {
        struct tm* tmp = gmtime(&now);
        if (!tmp) return false;
        tm_utc = *tmp;
    }
#endif
    return strftime(out, out_size, "%Y-%m-%dT%H:%M:%SZ", &tm_utc) > 0u;
}
