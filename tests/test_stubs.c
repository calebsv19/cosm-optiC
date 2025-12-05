#include <SDL2/SDL.h>
#include <stddef.h>

void ts_init(void) {}
void ts_start_timer(const char* name) { (void)name; }
void ts_stop_timer(const char* name) { (void)name; }
void ts_frame_start(void) {}
void ts_frame_end(void) {}
void ts_emit_event(const char* tag) { (void)tag; }
void ts_render(SDL_Renderer* renderer) { (void)renderer; }
