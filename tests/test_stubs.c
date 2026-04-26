#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "editor/scene_editor.h"
#include "app/animation.h"
#include "engine/Render/render_pipeline.h"
#include "timer_hud/time_scope.h"

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
