#include "render/render_helper.h"
#include "config/config_manager.h"
#include "camera/camera.h"
#include "math/vec2.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "vk_renderer.h"

static void RenderSurface(SDL_Renderer* renderer, SDL_Surface* surface, const SDL_Rect* dst) {
    if (!renderer || !surface || !dst) return;
#if USE_VULKAN
    VkRendererTexture texture;
    if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer*)renderer, surface, &texture,
                                                   VK_FILTER_LINEAR) != VK_SUCCESS) {
        return;
    }
    vk_renderer_draw_texture((VkRenderer*)renderer, &texture, NULL, dst);
    vk_renderer_queue_texture_destroy((VkRenderer*)renderer, &texture);
#else
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!textTexture) return;
    SDL_RenderCopy(renderer, textTexture, NULL, dst);
    SDL_DestroyTexture(textTexture);
#endif
}

int CalculateObjectBrightness(SceneObject* obj, double lightX, double lightY) {
    Vec2 o = vec2(obj->x, obj->y);
    Vec2 l = vec2(lightX, lightY);
    double distance = vec2_length(vec2_sub(o, l));

    // Normalize distance for a smooth fade effect
    double maxDistance = sqrt(sceneSettings.windowWidth * sceneSettings.windowWidth +
                              sceneSettings.windowHeight * sceneSettings.windowHeight) * 0.5;

    double intensity = 255 - ((distance / maxDistance) * (255 - 100));

    // Clamp intensity within the range [100, 255]
    if (intensity < 100) intensity = 100;
    if (intensity > 255) intensity = 255;

    return (int)intensity;
}


static CameraPoint ToScreen(double worldX, double worldY) {
    Vec2 screen = CameraWorldToScreenVec2(&sceneSettings.camera,
                                          vec2(worldX, worldY),
                                          sceneSettings.windowWidth,
                                          sceneSettings.windowHeight);
    CameraPoint out = {screen.x, screen.y};
    return out;
}

static void BuildScreenShapePoints(SceneObject* obj, int screenPoints[MAX_POINTS][2]) {
    for (int i = 0; i < obj->numPoints; i++) {
        double worldX = obj->x + obj->shapePoints[i][0];
        double worldY = obj->y + obj->shapePoints[i][1];
        CameraPoint screen = ToScreen(worldX, worldY);
        screenPoints[i][0] = (int)lround(screen.x);
        screenPoints[i][1] = (int)lround(screen.y);
    }
}

void RenderFillShape(SDL_Renderer* renderer, SceneObject* obj) {
    if (obj->numPoints < 3) {
        return;
    }

    int screenPoints[MAX_POINTS][2];
    BuildScreenShapePoints(obj, screenPoints);

    int minY = screenPoints[0][1];
    int maxY = screenPoints[0][1];

    for (int i = 1; i < obj->numPoints; i++) {
        int y = screenPoints[i][1];
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
    }

    for (int y = minY; y <= maxY; y++) {
        int* edgeX = (int*)malloc(obj->numPoints * sizeof(int));
        if (!edgeX) {
            printf("Memory allocation failed for edgeX array.\n");
            return;
        }

        int edgeCount = 0;

        for (int i = 0; i < obj->numPoints; i++) {
            int nextIndex = (i + 1) % obj->numPoints;
            int x1 = screenPoints[i][0]; 
            int y1 = screenPoints[i][1]; 
            int x2 = screenPoints[nextIndex][0];
            int y2 = screenPoints[nextIndex][1];

            if ((y1 < y && y2 >= y) || (y2 < y && y1 >= y)) {
                float t = (float)(y - y1) / (float)(y2 - y1);
                int xIntersect = x1 + t * (x2 - x1);
                edgeX[edgeCount++] = xIntersect;
            }
        }

        qsort(edgeX, edgeCount, sizeof(int), (int(*)(const void*, const void*))compareInts);

        for (int i = 0; i < edgeCount - 1; i += 2) {
            SDL_RenderDrawLine(renderer, edgeX[i], y, edgeX[i + 1], y);
        }

        free(edgeX);
    }
}


int compareInts(const void* a, const void* b) {
    return (*(int*)a - *(int*)b);
}


void RenderShape(SDL_Renderer* renderer, SceneObject* obj, bool fillObjects) {
    if (fillObjects) {
        RenderFillShape(renderer, obj);
    } else {
        RenderDrawShape(renderer, obj);
    }
}


void RenderCircle(SDL_Renderer* renderer, int x, int y, int radius, bool fillObjects) {
    if (fillObjects) {
        RenderFillCircle(renderer, x, y, radius);
    } else {
        RenderDrawCircle(renderer, x, y, radius);
    }
}

void RenderDrawCircle(SDL_Renderer* renderer, int x, int y, int radius) {
    int offsetX = 0;
    int offsetY = radius;
    int d = 3 - 2 * radius;

    while (offsetY >= offsetX) {
        // ✅ Draw symmetric points in all 8 octants
        SDL_RenderDrawPoint(renderer, x + offsetX, y + offsetY);
        SDL_RenderDrawPoint(renderer, x - offsetX, y + offsetY);
        SDL_RenderDrawPoint(renderer, x + offsetX, y - offsetY);
        SDL_RenderDrawPoint(renderer, x - offsetX, y - offsetY);
        SDL_RenderDrawPoint(renderer, x + offsetY, y + offsetX);
        SDL_RenderDrawPoint(renderer, x - offsetY, y + offsetX);
        SDL_RenderDrawPoint(renderer, x + offsetY, y - offsetX);
        SDL_RenderDrawPoint(renderer, x - offsetY, y - offsetX);

        if (d < 0) {  
            d += 4 * offsetX + 6;
        } else {
            d += 4 * (offsetX - offsetY) + 10;
            offsetY--;
        }
        offsetX++;
    }
}

void RenderFillCircle(SDL_Renderer* renderer, int x, int y, int radius) {
    int offsetX = 0;
    int offsetY = radius;
    int d = 3 - 2 * radius;

    while (offsetY >= offsetX) {
        // ✅ Draw horizontal scan lines for filling
        SDL_RenderDrawLine(renderer, x - offsetX, y + offsetY, x + offsetX, y + offsetY);
        SDL_RenderDrawLine(renderer, x - offsetX, y - offsetY, x + offsetX, y - offsetY);
        SDL_RenderDrawLine(renderer, x - offsetY, y + offsetX, x + offsetY, y + offsetX);
        SDL_RenderDrawLine(renderer, x - offsetY, y - offsetX, x + offsetY, y - offsetX);

        if (d < 0) {  
            d += 4 * offsetX + 6;
        } else {
            d += 4 * (offsetX - offsetY) + 10;
            offsetY--;
        }
        offsetX++;
    }
}

void RenderDrawShape(SDL_Renderer* renderer, SceneObject* obj) {
    if (strcmp(obj->type, "circle") == 0) {
        CameraPoint center = ToScreen(obj->x, obj->y);
        int radius = (int)lround(obj->radius * obj->scale * sceneSettings.camera.zoom);
        RenderDrawCircle(renderer, (int)lround(center.x), (int)lround(center.y), radius);
    } else if (obj->numPoints > 1) {
        int screenPoints[MAX_POINTS][2];
        BuildScreenShapePoints(obj, screenPoints);
        for (int i = 0; i < obj->numPoints; i++) {
            int nextIndex = (i + 1) % obj->numPoints;
            int x1 = screenPoints[i][0];
            int y1 = screenPoints[i][1];
            int x2 = screenPoints[nextIndex][0];
            int y2 = screenPoints[nextIndex][1];

            SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        }
    }
}

void RenderStaticScene(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
    SDL_Rect bg = {0, 0, sceneSettings.windowWidth, sceneSettings.windowHeight};
    SDL_RenderFillRect(renderer, &bg);
        
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);  // White objects

    RenderSceneObjects(renderer, true);    
}

static void RenderTextWithColor(SDL_Renderer* renderer, SDL_Rect button, const char* text, SDL_Color textColor, int maxFontSize) {
    int minFontSize = 10;  // Minimum readable font size

    int fontSize = animation_config_scale_text_point_size(&animSettings, maxFontSize, minFontSize);
    if (fontSize < minFontSize) fontSize = minFontSize;
    while (fontSize > minFontSize) {
        TTF_Font* tempFont = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", fontSize);
        if (!tempFont) return;

        SDL_Surface* tempSurface = TTF_RenderText_Solid(tempFont, text, textColor);
        if (!tempSurface) {
            TTF_CloseFont(tempFont);
            return;
        }
        int textWidth = tempSurface->w;
        SDL_FreeSurface(tempSurface);
        TTF_CloseFont(tempFont);

        if (textWidth <= button.w - 10) {  // Leave a margin of 5 pixels on each side
            break;
        }
        fontSize--;
    }

    TTF_Font* font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", fontSize);
    if (!font) return;

    SDL_Surface* textSurface = TTF_RenderText_Solid(font, text, textColor);
    if (!textSurface) {
        TTF_CloseFont(font);
        return;
    }

    SDL_Rect textRect = {
        button.x + (button.w - textSurface->w) / 2,
        button.y + (button.h - textSurface->h) / 2,
        textSurface->w, textSurface->h
    };

    RenderSurface(renderer, textSurface, &textRect);
    SDL_FreeSurface(textSurface);
    TTF_CloseFont(font);
}

void RenderButtonText(SDL_Renderer* renderer, SDL_Rect button, const char* text) {
    SDL_Color textColor = {0, 0, 0, 255};  // Black text
    RenderTextWithColor(renderer, button, text, textColor, 24);
}

void RenderLabelText(SDL_Renderer* renderer, SDL_Rect area, const char* text, SDL_Color color) {
    RenderTextWithColor(renderer, area, text, color, 22);
}
                

void RenderSceneObject(SDL_Renderer* renderer, SceneObject* obj, bool fillObjects) {


    if (strcmp(obj->type, "circle") == 0) {
        CameraPoint center = ToScreen(obj->x, obj->y);
        int radius = (int)lround(obj->radius * obj->scale * sceneSettings.camera.zoom);
        RenderCircle(renderer, (int)lround(center.x), (int)lround(center.y), radius, fillObjects);
    } else {
        RenderShape(renderer, obj, fillObjects);
    }
}

void RenderSceneObjects(SDL_Renderer* renderer, bool fillObjects) {
    for (int i = 0; i < sceneSettings.objectCount; i++) {
        SceneObject* obj = &sceneSettings.sceneObjects[i];

        if (strcmp(obj->type, "circle") == 0) {
            CameraPoint center = ToScreen(obj->x, obj->y);
            int radius = (int)lround(obj->radius * obj->scale * sceneSettings.camera.zoom);
            RenderCircle(renderer, (int)lround(center.x), (int)lround(center.y), radius, fillObjects);
        } else {
            RenderShape(renderer, obj, fillObjects);
        }
    }
}
