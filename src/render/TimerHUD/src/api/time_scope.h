#ifndef TIMESCOPE_H
#define TIMESCOPE_H

#ifdef __cplusplus
extern "C" {
#endif


#include "../core/frame_tracker.h"
#include "../core/timer_manager.h"
#include "../events/event_tracker.h"
#include "../hud/hud_renderer.h"
#include "../logging/logger.h"
#include "../config/settings_loader.h"



#include <stdint.h>

// --- Initialization ---
void ts_init(void);              // Call once on app start

// --- Timer Controls ---
void ts_start_timer(const char* name);  // Begin named timer
void ts_stop_timer(const char* name);   // End named timer

// --- Frame Markers (optional) ---
void ts_frame_start(void);       // Mark beginning of frame (future use)
void ts_frame_end(void);         // Mark end of frame and flush logs/stats

// --- Event Tagging ---
void ts_emit_event(const char* tag);  // Optional Phase 4

// --- HUD Rendering (SDL2-based) ---
struct SDL_Renderer;
void ts_render(struct SDL_Renderer* renderer);  // Optional Phase 2

#ifdef __cplusplus
}
#endif

#endif // TIMESCOPE_H

