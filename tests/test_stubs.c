#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

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

#if RAY_TRACING_TEST_TIMER_HUD_LEGACY_COMPAT
typedef enum TimerHUDVisualMode {
    TIMER_HUD_VISUAL_MODE_INVALID = -1,
    TIMER_HUD_VISUAL_MODE_TEXT_COMPACT = 0,
    TIMER_HUD_VISUAL_MODE_HISTORY_GRAPH = 1,
    TIMER_HUD_VISUAL_MODE_HYBRID = 2,
} TimerHUDVisualMode;
#endif

TimerHUDSession* ts_session_create(void) { return (TimerHUDSession*)1; }
void ts_session_destroy(TimerHUDSession* session) { (void)session; }
TimerHUDSession* ts_default_session(void) { return (TimerHUDSession*)1; }
void ts_session_register_backend(TimerHUDSession* session, const TimerHUDBackend* backend) {
    (void)session;
    (void)backend;
}
bool ts_session_apply_init_config(TimerHUDSession* session, const TimerHUDInitConfig* config) {
    (void)session;
    (void)config;
    return true;
}
void ts_session_init(TimerHUDSession* session) { (void)session; }
void ts_session_shutdown(TimerHUDSession* session) { (void)session; }
void ts_session_start_timer(TimerHUDSession* session, const char* name) {
    (void)session;
    (void)name;
}
void ts_session_stop_timer(TimerHUDSession* session, const char* name) {
    (void)session;
    (void)name;
}
void ts_session_frame_start(TimerHUDSession* session) { (void)session; }
void ts_session_frame_end(TimerHUDSession* session) { (void)session; }
void ts_session_emit_event(TimerHUDSession* session, const char* tag) {
    (void)session;
    (void)tag;
}
void ts_session_render(TimerHUDSession* session) { (void)session; }
void ts_session_set_hud_enabled(TimerHUDSession* session, bool enabled) {
    (void)session;
    (void)enabled;
}
bool ts_session_is_hud_enabled(const TimerHUDSession* session) {
    (void)session;
    return false;
}
bool ts_session_is_log_enabled(const TimerHUDSession* session) {
    (void)session;
    return false;
}
bool ts_session_is_event_tagging_enabled(const TimerHUDSession* session) {
    (void)session;
    return false;
}
const char* ts_session_get_hud_visual_mode(const TimerHUDSession* session) {
    (void)session;
    return "hybrid";
}
const char* ts_session_get_log_filepath(const TimerHUDSession* session) {
    (void)session;
    return "timing.json";
}
bool ts_session_set_hud_visual_mode_kind(TimerHUDSession* session, TimerHUDVisualMode mode) {
    (void)session;
    (void)mode;
    return true;
}
TimerHUDSession* timer_hud_session(void) { return (TimerHUDSession*)1; }
TimerHUDVisualMode ts_visual_mode_from_string(const char* mode) {
    if (!mode) return TIMER_HUD_VISUAL_MODE_INVALID;
    if (strcmp(mode, "text_compact") == 0) return TIMER_HUD_VISUAL_MODE_TEXT_COMPACT;
    if (strcmp(mode, "history_graph") == 0) return TIMER_HUD_VISUAL_MODE_HISTORY_GRAPH;
    if (strcmp(mode, "hybrid") == 0) return TIMER_HUD_VISUAL_MODE_HYBRID;
    return TIMER_HUD_VISUAL_MODE_INVALID;
}

void ts_init(void) {}
void ts_start_timer(const char* name) { (void)name; }
void ts_stop_timer(const char* name) { (void)name; }
void ts_frame_start(void) {}
void ts_frame_end(void) {}
void ts_emit_event(const char* tag) { (void)tag; }
void ts_render(void) {}

// UI/editor symbols needed when linking the broad RayTracing test target.
bool InitializeSceneEditor(SceneEditor* editor) { (void)editor; return true; }
void SceneEditorLoop(SceneEditor* editor) { (void)editor; }
void RunPreviewMode(void) {}
void RunPreviewModeEmbedded(SDL_Window* host_window, SDL_Renderer* host_renderer) {
    (void)host_window;
    (void)host_renderer;
}

// Font symbols used by timer_hud_adapter.
bool initFontSystem(void) { return true; }
TTF_Font* getActiveFont(void) { return NULL; }

// TimerHUD global registration symbols.
void ts_register_backend(const TimerHUDBackend* backend) { (void)backend; }
void ts_set_settings_path(const char* path) { (void)path; }
bool ts_apply_init_config(const TimerHUDInitConfig* config) { (void)config; return true; }

// Animation/render pipeline symbols used by ray_tracing2 in tests.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
bool AnimationUseFluidScene(void) { return false; }
int frameCounter = 0;
int loopCount = 0;
double currentTime = 0.0;
SDL_Renderer* renderer = NULL;
double AnimationCurrentNormalizedT(void) { return 0.0; }
RenderContext* getRenderContext(void) { return NULL; }
bool render_begin_frame(void) { return true; }
void render_end_frame(void) {}
bool render_device_lost(void) { return false; }
void render_set_clear_color(SDL_Renderer* renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    (void)renderer;
    (void)r;
    (void)g;
    (void)b;
    (void)a;
}
