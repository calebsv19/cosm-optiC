#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_font.h"
#include "engine/Render/render_pipeline.h"
#include "render/text_upload_policy.h"
#include <SDL2/SDL_ttf.h>
#include <string.h>

static int measureRange(const char* text, int length, int* outWidth) {
    TTF_Font* font = getActiveFont();
    RenderContext* ctx = getRenderContext();
    SDL_Renderer* renderer = ctx ? ctx->renderer : NULL;
    if (!font || !text || length <= 0) return 0;
    int w = 0;
    TTF_SizeUTF8(font, text, &w, NULL);
    w = ray_tracing_text_logical_pixels(renderer, w);
    if (outWidth) *outWidth = w;
    return w;
}

int getTextWidth(const char* text) {
    int width = 0;
    measureRange(text, text ? (int)strlen(text) : 0, &width);
    return width;
}

int getTextWidthN(const char* text, int n) {
    if (!text || n <= 0) return 0;
    char buffer[512];
    int len = n;
    if (len >= (int)sizeof(buffer)) len = (int)sizeof(buffer) - 1;
    memcpy(buffer, text, (size_t)len);
    buffer[len] = '\0';
    int width = 0;
    measureRange(buffer, len, &width);
    return width;
}

size_t getTextClampedLength(const char* text, int maxWidth) {
    if (!text || maxWidth <= 0) return 0;
    size_t length = strlen(text);
    for (size_t i = 1; i <= length; i++) {
        int width = getTextWidthN(text, (int)i);
        if (width > maxWidth) {
            return i > 1 ? i - 1 : 0;
        }
    }
    return length;
}
