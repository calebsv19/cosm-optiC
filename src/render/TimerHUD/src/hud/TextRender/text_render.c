#include "text_render.h"
#include "engine/Render/render_font.h"

#include <SDL2/SDL_ttf.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Redirect all font use to your global IDE font system
#define currentFont getActiveFont()

bool Text_Init(void) {
    if (TTF_WasInit() == 0) {
        if (TTF_Init() == -1) {
            fprintf(stderr, "SDL_ttf init failed: %s\n", TTF_GetError());
            return false;
        }
    }
    return true;
}

void Text_Quit(void) {
    // Fonts managed elsewhere. Do not shut down SDL_ttf globally here.
}

SDL_Rect Text_Measure(const char* text) {
    SDL_Rect rect = {0, 0, 0, 0};
    if (!text || !currentFont) return rect;

    TTF_SizeText(currentFont, text, &rect.w, &rect.h);
    return rect;
}

void Text_Draw(SDL_Renderer* renderer, const char* text, int x, int y, int alignFlags, SDL_Color color) {
    if (!text || !currentFont || !renderer) return;

    SDL_Surface* surface = TTF_RenderUTF8_Blended(currentFont, text, color);
    if (!surface) return;

    SDL_Rect dst = {x, y, surface->w, surface->h};

    if (alignFlags & ALIGN_CENTER)  dst.x -= surface->w / 2;
    if (alignFlags & ALIGN_RIGHT)   dst.x -= surface->w;
    if (alignFlags & ALIGN_MIDDLE)  dst.y -= surface->h / 2;
    if (alignFlags & ALIGN_BOTTOM)  dst.y -= surface->h;

#if USE_VULKAN
    VkRendererTexture texture = {0};
    VkResult uploadResult = vk_renderer_upload_sdl_surface_with_filter(
        renderer, surface, &texture, VK_FILTER_NEAREST);
    if (uploadResult == VK_SUCCESS) {
        vk_renderer_draw_texture(renderer, &texture, NULL, &dst);
        vk_renderer_queue_texture_destroy(renderer, &texture);
    }
#else
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_RenderCopy(renderer, texture, NULL, &dst);
        SDL_DestroyTexture(texture);
    }
#endif

    SDL_FreeSurface(surface);
}
