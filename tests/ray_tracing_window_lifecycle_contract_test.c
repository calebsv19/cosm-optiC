#include "app/ray_tracing_window_lifecycle.h"
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include "render/pipeline/ray_tracing2_buffers.h"
#include <stdio.h>
#include <string.h>
static Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_VULKAN;
static Uint8 keys[SDL_NUM_SCANCODES];
static unsigned fullscreen_calls;
Uint32 SDL_GetWindowID(SDL_Window *window) { (void)window; return 7; }
void SDL_GetWindowSize(SDL_Window *window, int *w, int *h) { (void)window; *w=600; *h=400; }
Uint32 SDL_GetWindowFlags(SDL_Window *window) { (void)window; return flags; }
int SDL_GetWindowDisplayIndex(SDL_Window *window) { (void)window; return 0; }
void SDL_Vulkan_GetDrawableSize(SDL_Window *window, int *w, int *h) { (void)window; *w=1200; *h=800; }
SDL_Renderer *SDL_GetRenderer(SDL_Window *window) { (void)window; return NULL; }
int SDL_GetRendererOutputSize(SDL_Renderer *renderer, int *w, int *h) { (void)renderer; (void)w; (void)h; return -1; }
const Uint8 *SDL_GetKeyboardState(int *count) { *count=SDL_NUM_SCANCODES; return keys; }
int SDL_SetWindowFullscreen(SDL_Window *window, Uint32 mode) {
    (void)window; fullscreen_calls++; flags=(flags & ~SDL_WINDOW_FULLSCREEN_DESKTOP) | mode; return 0;
}
int main(void) {
    Uint8 *pixels=NULL, *preview=NULL; float *energy=NULL, *direct=NULL, *reflection=NULL; size_t capacity=0;
    assert(RayTracing2BuffersEnsureFrameCapacity(&pixels,&preview,&energy,&direct,&reflection,&capacity,16));
    direct=malloc(16*sizeof(float)); reflection=malloc(16*sizeof(float)); assert(direct && reflection);
    assert(RayTracing2BuffersEnsureFrameCapacity(&pixels,&preview,&energy,&direct,&reflection,&capacity,4096));
    assert(capacity==4096 && direct==NULL && reflection==NULL); pixels[4095]=17; preview[4095]=19; energy[4095]=3;
    Uint8 *saved=pixels;
    assert(RayTracing2BuffersEnsureFrameCapacity(&pixels,&preview,&energy,&direct,&reflection,&capacity,32));
    assert(pixels==saved && pixels[4095]==17);
    assert(!RayTracing2BuffersEnsureFrameCapacity(&pixels,&preview,&energy,&direct,&reflection,&capacity,SIZE_MAX));
    RayTracing2BuffersResetFrameBuffers(&pixels,&preview,&energy,&direct);
    SDL_Event event={0};
    ray_tracing_window_lifecycle_bind((SDL_Window *)1);
    event.type=SDL_KEYDOWN; event.key.windowID=8; event.key.keysym.sym=SDLK_ESCAPE;
    assert(ray_tracing_window_lifecycle_filter(&event));
    event.key.windowID=7;
    assert(!ray_tracing_window_lifecycle_filter(&event));
    event.key.keysym.sym=SDLK_F11;
    assert(ray_tracing_window_lifecycle_filter(&event)); assert(fullscreen_calls==1);
    event.key.repeat=1;
    assert(ray_tracing_window_lifecycle_filter(&event)); assert(fullscreen_calls==1);
    flags=SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN;
    event.key.repeat=0; event.key.keysym.sym=SDLK_ESCAPE;
    assert(ray_tracing_window_lifecycle_filter(&event));
    event.type=SDL_QUIT;
    assert(!ray_tracing_window_lifecycle_filter(&event));
    flags=SDL_WINDOW_SHOWN | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_VULKAN;
    keys[SDL_SCANCODE_A]=1;
    event.type=SDL_WINDOWEVENT; event.window.windowID=7; event.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
    assert(!ray_tracing_window_lifecycle_filter(&event));
    event.type=SDL_KEYDOWN; event.key.windowID=7; event.key.keysym.scancode=SDL_SCANCODE_A;
    assert(ray_tracing_window_lifecycle_filter(&event));
    event.type=SDL_KEYUP; assert(ray_tracing_window_lifecycle_filter(&event));
    event.type=SDL_KEYDOWN; assert(!ray_tracing_window_lifecycle_filter(&event));
    memset(&event,0,sizeof(event)); event.type=SDL_MOUSEMOTION; event.motion.windowID=7; event.motion.state=SDL_BUTTON_LMASK;
    assert(ray_tracing_window_lifecycle_filter(&event));
    event.type=SDL_MOUSEBUTTONDOWN; event.button.windowID=7;
    assert(!ray_tracing_window_lifecycle_filter(&event));
    event.type=SDL_MOUSEMOTION; event.motion.windowID=7; event.motion.state=SDL_BUTTON_LMASK;
    assert(!ray_tracing_window_lifecycle_filter(&event));
    ray_tracing_window_lifecycle_bind(NULL);
    puts("window lifecycle event scope, hidden input, held-key release, drag cancellation and F11 repeat: PASS");
    return 0;
}
