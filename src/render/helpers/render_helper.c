#include "render/render_helper.h"
#include "config/config_manager.h"
#include "camera/camera.h"
#include "math/vec2.h"
#include "render/text_font_cache.h"
#include "render/text_upload_policy.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vk_renderer.h"

static double clamp_double(double value, double min_value, double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static TTF_Font* RenderHelperOpenUIFontAtPointSize(int point_size) {
    return ray_tracing_text_font_cache_get_ui_regular(point_size);
}

double RenderHelper_DepthScaleForObjectZ(double object_z) {
    /* NP-4: controlled 3D projection scale over canonical 2D backend. */
    double scale = 1.0 - (object_z * 0.08);
    return clamp_double(scale, 0.45, 1.80);
}

double RenderHelper_DepthYOffsetPixelsForObjectZ(double object_z, double camera_zoom) {
    if (camera_zoom < 0.01) camera_zoom = 0.01;
    return object_z * 18.0 * camera_zoom;
}

static void RenderSurface(SDL_Renderer* renderer, SDL_Surface* surface, const SDL_Rect* dst, float raster_scale) {
    if (!renderer || !surface || !dst) return;
#if USE_VULKAN
    VkRendererTexture texture;
    (void)raster_scale;
    if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer*)renderer, surface, &texture,
                                                   ray_tracing_text_upload_filter(renderer)) != VK_SUCCESS) {
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

static CameraPoint ToScreenDepthProjected(double worldX, double worldY, double object_z) {
    CameraPoint base = ToScreen(worldX, worldY);
    if (animSettings.spaceMode != SPACE_MODE_3D) {
        return base;
    }
    base.y -= RenderHelper_DepthYOffsetPixelsForObjectZ(object_z, sceneSettings.camera.zoom);
    return base;
}

static void BuildScreenShapePoints(SceneObject* obj, int screenPoints[MAX_POINTS][2]) {
    CameraPoint base_center = ToScreen(obj->x, obj->y);
    CameraPoint projected_center = ToScreenDepthProjected(obj->x, obj->y, obj->z);
    double depth_scale = (animSettings.spaceMode == SPACE_MODE_3D)
                             ? RenderHelper_DepthScaleForObjectZ(obj->z)
                             : 1.0;

    for (int i = 0; i < obj->numPoints; i++) {
        double worldX = obj->x + obj->shapePoints[i][0];
        double worldY = obj->y + obj->shapePoints[i][1];
        CameraPoint screen = ToScreen(worldX, worldY);
        if (animSettings.spaceMode == SPACE_MODE_3D) {
            double offset_x = screen.x - base_center.x;
            double offset_y = screen.y - base_center.y;
            screen.x = projected_center.x + (offset_x * depth_scale);
            screen.y = projected_center.y + (offset_y * depth_scale);
        }
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
        double depth_scale = (animSettings.spaceMode == SPACE_MODE_3D)
                                 ? RenderHelper_DepthScaleForObjectZ(obj->z)
                                 : 1.0;
        CameraPoint center = ToScreenDepthProjected(obj->x, obj->y, obj->z);
        int radius = (int)lround(obj->radius * obj->scale * sceneSettings.camera.zoom * depth_scale);
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

static int RenderTextBlockWithColor(SDL_Renderer* renderer,
                                    SDL_Rect area,
                                    const char* text,
                                    SDL_Color textColor,
                                    int maxFontSize,
                                    bool wrapped,
                                    bool center_horizontal,
                                    bool center_vertical) {
    int minFontSize = 10;  // Minimum readable font size
    int baseFontSize = 0;
    float raster_scale = 1.0f;
    int rasterFontSize = 0;
    int minRasterFontSize = 0;
    int chosenRasterFontSize = 0;
    TTF_Font* font = NULL;
    int measured_w = 0;
    int measured_h = 0;
    SDL_Surface* textSurface = NULL;
    SDL_Rect textRect = {0};
    int wrap_width = 0;

    if (!renderer || !text || !text[0]) return 0;
    if (area.w <= 0 || area.h <= 0) return 0;
    baseFontSize = animation_config_scale_text_point_size(&animSettings, maxFontSize, minFontSize);
    if (baseFontSize < minFontSize) baseFontSize = minFontSize;
    raster_scale = ray_tracing_text_raster_scale(renderer);
    rasterFontSize = ray_tracing_text_raster_point_size(renderer, baseFontSize, minFontSize);
    minRasterFontSize = ray_tracing_text_raster_point_size(renderer, minFontSize, minFontSize);
    if (minRasterFontSize < 4) minRasterFontSize = 4;
    if (rasterFontSize < minRasterFontSize) rasterFontSize = minRasterFontSize;

    chosenRasterFontSize = rasterFontSize;
    if (!wrapped) {
        while (chosenRasterFontSize > minRasterFontSize) {
            int draw_width = 0;
            int draw_width_logical = 0;
            TTF_Font* tempFont = RenderHelperOpenUIFontAtPointSize(chosenRasterFontSize);
            if (!tempFont) return 0;
            if (TTF_SizeUTF8(tempFont, text, &draw_width, NULL) != 0) {
                return 0;
            }
            draw_width_logical = ray_tracing_text_logical_pixels(renderer, draw_width);
            if (draw_width_logical <= area.w - 10) {
                break;
            }
            chosenRasterFontSize--;
        }
    }

    font = RenderHelperOpenUIFontAtPointSize(chosenRasterFontSize);
    if (!font) return 0;

    if (wrapped) {
        wrap_width = (int)lroundf((float)area.w * raster_scale);
        if (wrap_width < 1) wrap_width = 1;
        textSurface = TTF_RenderUTF8_Blended_Wrapped(font, text, textColor, (Uint32)wrap_width);
    } else {
        textSurface = TTF_RenderUTF8_Blended(font, text, textColor);
    }
    if (!textSurface) {
        return 0;
    }
    measured_w = ray_tracing_text_logical_pixels(renderer, textSurface->w);
    measured_h = ray_tracing_text_logical_pixels(renderer, textSurface->h);
    if (measured_w < 1) measured_w = 1;
    if (measured_h < 1) measured_h = 1;
    if (center_horizontal) {
        textRect.x = area.x + (area.w - measured_w) / 2;
    } else {
        textRect.x = area.x;
    }
    if (center_vertical) {
        textRect.y = area.y + (area.h - measured_h) / 2;
    } else {
        textRect.y = area.y;
    }
    textRect.w = measured_w;
    textRect.h = measured_h;
    RenderSurface(renderer, textSurface, &textRect, raster_scale);
    SDL_FreeSurface(textSurface);
    return measured_h;
}

static void RenderTextWithColor(SDL_Renderer* renderer, SDL_Rect button, const char* text, SDL_Color textColor, int maxFontSize) {
    (void)RenderTextBlockWithColor(renderer,
                                   button,
                                   text,
                                   textColor,
                                   maxFontSize,
                                   false,
                                   true,
                                   true);
}

void RenderButtonText(SDL_Renderer* renderer, SDL_Rect button, const char* text) {
    SDL_Color textColor = {0, 0, 0, 255};  // Black text
    RenderTextWithColor(renderer, button, text, textColor, 18);
}

void RenderButtonTextWithColor(SDL_Renderer* renderer,
                               SDL_Rect button,
                               const char* text,
                               SDL_Color text_color) {
    RenderTextWithColor(renderer, button, text, text_color, 18);
}

void RenderLabelText(SDL_Renderer* renderer, SDL_Rect area, const char* text, SDL_Color color) {
    SDL_Rect previous_clip = {0, 0, 0, 0};
    SDL_bool clip_was_enabled = SDL_FALSE;
    if (!renderer) return;
    clip_was_enabled = SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer, &previous_clip);
    if (area.w > 0 && area.h > 0) {
        SDL_RenderSetClipRect(renderer, &area);
    }
    RenderTextWithColor(renderer, area, text, color, 16);
    if (clip_was_enabled) {
        SDL_RenderSetClipRect(renderer, &previous_clip);
    } else {
        SDL_RenderSetClipRect(renderer, NULL);
    }
}

int RenderLabelTextLeft(SDL_Renderer* renderer, SDL_Rect area, const char* text, SDL_Color color) {
    SDL_Rect previous_clip = {0, 0, 0, 0};
    SDL_bool clip_was_enabled = SDL_FALSE;
    int used_height = 0;
    if (!renderer) return 0;
    clip_was_enabled = SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer, &previous_clip);
    if (area.w > 0 && area.h > 0) {
        SDL_RenderSetClipRect(renderer, &area);
    }
    used_height = RenderTextBlockWithColor(renderer,
                                           area,
                                           text,
                                           color,
                                           16,
                                           false,
                                           false,
                                           false);
    if (clip_was_enabled) {
        SDL_RenderSetClipRect(renderer, &previous_clip);
    } else {
        SDL_RenderSetClipRect(renderer, NULL);
    }
    return used_height;
}

int RenderLabelTextWrappedLeft(SDL_Renderer* renderer, SDL_Rect area, const char* text, SDL_Color color) {
    SDL_Rect previous_clip = {0, 0, 0, 0};
    SDL_bool clip_was_enabled = SDL_FALSE;
    int used_height = 0;
    if (!renderer) return 0;
    clip_was_enabled = SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer, &previous_clip);
    if (area.w > 0 && area.h > 0) {
        SDL_RenderSetClipRect(renderer, &area);
    }
    used_height = RenderTextBlockWithColor(renderer,
                                           area,
                                           text,
                                           color,
                                           16,
                                           true,
                                           false,
                                           false);
    if (clip_was_enabled) {
        SDL_RenderSetClipRect(renderer, &previous_clip);
    } else {
        SDL_RenderSetClipRect(renderer, NULL);
    }
    return used_height;
}
                

void RenderSceneObject(SDL_Renderer* renderer, SceneObject* obj, bool fillObjects) {


    if (strcmp(obj->type, "circle") == 0) {
        double depth_scale = (animSettings.spaceMode == SPACE_MODE_3D)
                                 ? RenderHelper_DepthScaleForObjectZ(obj->z)
                                 : 1.0;
        CameraPoint center = ToScreenDepthProjected(obj->x, obj->y, obj->z);
        int radius = (int)lround(obj->radius * obj->scale * sceneSettings.camera.zoom * depth_scale);
        RenderCircle(renderer, (int)lround(center.x), (int)lround(center.y), radius, fillObjects);
    } else {
        RenderShape(renderer, obj, fillObjects);
    }
}

static int compare_object_index_by_depth(const void* lhs, const void* rhs) {
    int li = *(const int*)lhs;
    int ri = *(const int*)rhs;
    double lz = sceneSettings.sceneObjects[li].z;
    double rz = sceneSettings.sceneObjects[ri].z;
    if (lz < rz) return 1;   /* far(+z) first */
    if (lz > rz) return -1;
    return li - ri;
}

void RenderSceneObjects(SDL_Renderer* renderer, bool fillObjects) {
    int draw_order[MAX_OBJECTS];
    int count = sceneSettings.objectCount;
    if (count > MAX_OBJECTS) count = MAX_OBJECTS;
    for (int i = 0; i < count; ++i) {
        draw_order[i] = i;
    }
    if (animSettings.spaceMode == SPACE_MODE_3D && count > 1) {
        qsort(draw_order, (size_t)count, sizeof(draw_order[0]), compare_object_index_by_depth);
    }

    for (int i = 0; i < count; i++) {
        SceneObject* obj = &sceneSettings.sceneObjects[draw_order[i]];

        if (strcmp(obj->type, "circle") == 0) {
            double depth_scale = (animSettings.spaceMode == SPACE_MODE_3D)
                                     ? RenderHelper_DepthScaleForObjectZ(obj->z)
                                     : 1.0;
            CameraPoint center = ToScreenDepthProjected(obj->x, obj->y, obj->z);
            int radius = (int)lround(obj->radius * obj->scale * sceneSettings.camera.zoom * depth_scale);
            RenderCircle(renderer, (int)lround(center.x), (int)lround(center.y), radius, fillObjects);
        } else {
            RenderShape(renderer, obj, fillObjects);
        }
    }
}
