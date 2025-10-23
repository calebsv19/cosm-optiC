#include "render/render_helper.h"
#include "config/config_manager.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

int CalculateObjectBrightness(SceneObject* obj, double lightX, double lightY) {
    double dx = obj->x - lightX;
    double dy = obj->y - lightY;
    double distance = sqrt(dx * dx + dy * dy);

    // Normalize distance for a smooth fade effect
    double maxDistance = sqrt(sceneSettings.windowWidth * sceneSettings.windowWidth +
                              sceneSettings.windowHeight * sceneSettings.windowHeight) * 0.5;

    double intensity = 255 - ((distance / maxDistance) * (255 - 100));

    // Clamp intensity within the range [100, 255]
    if (intensity < 100) intensity = 100;
    if (intensity > 255) intensity = 255;

    return (int)intensity;
}


void RenderFillShape(SDL_Renderer* renderer, SceneObject* obj) {
		
    if (obj->numPoints < 3) {
        return;
    }

    int minY = obj->y + obj->shapePoints[0][1];
    int maxY = minY;

    for (int i = 1; i < obj->numPoints; i++) {
        int y = obj->y + obj->shapePoints[i][1];
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
            int x1 = obj->x + obj->shapePoints[i][0]; 
            int y1 = obj->y + obj->shapePoints[i][1]; 
            int x2 = obj->x + obj->shapePoints[nextIndex][0];
            int y2 = obj->y + obj->shapePoints[nextIndex][1];

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
        RenderDrawCircle(renderer, obj->x, obj->y, obj->radius* obj->scale);
    } else if (obj->numPoints > 1) {
        // ✅ Draw shape outline by connecting points
        for (int i = 0; i < obj->numPoints; i++) {
            int nextIndex = (i + 1) % obj->numPoints;
            int x1 = obj->x + obj->shapePoints[i][0];
            int y1 = obj->y + obj->shapePoints[i][1];
            int x2 = obj->x + obj->shapePoints[nextIndex][0];
            int y2 = obj->y + obj->shapePoints[nextIndex][1];

            SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        }
    }
}

void RenderStaticScene(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
    SDL_RenderClear(renderer);
        
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);  // White objects

    RenderSceneObjects(renderer, true);    
}

void RenderButtonText(SDL_Renderer* renderer, SDL_Rect button, const char* text) {
    SDL_Color textColor = {0, 0, 0, 255};  // Black text

    // Dynamically calculate font size based on button width and text length
    int maxFontSize = 24;  // Default max font size
    int minFontSize = 10;  // Minimum readable font size
    // int textLength = strlen(text);

    // Adjust font size so the text fits within the button width
    int fontSize = maxFontSize;
    while (fontSize > minFontSize) {
        TTF_Font* tempFont = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", fontSize);
        if (!tempFont) return;

        SDL_Surface* tempSurface = TTF_RenderText_Solid(tempFont, text, textColor);
        int textWidth = tempSurface->w;
        SDL_FreeSurface(tempSurface);
        TTF_CloseFont(tempFont);

        if (textWidth <= button.w - 10) {  // Leave a margin of 5 pixels on each side
            break;
        }
        fontSize--;
    }

    // Load font with calculated size
    TTF_Font* font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", fontSize);
    if (!font) return;

    // Render text surface
    SDL_Surface* textSurface = TTF_RenderText_Solid(font, text, textColor);
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    
    SDL_Rect textRect = {
        button.x + (button.w - textSurface->w) / 2,  // Center horizontally
        button.y + (button.h - textSurface->h) / 2,  // Center vertically
        textSurface->w, textSurface->h
    };

    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);

    // Cleanup
    SDL_FreeSurface(textSurface);
    SDL_DestroyTexture(textTexture);
    TTF_CloseFont(font);
}
                

void RenderSceneObject(SDL_Renderer* renderer, SceneObject* obj, bool fillObjects) {


    if (strcmp(obj->type, "circle") == 0) {
        RenderCircle(renderer, obj->x, obj->y, obj->radius * obj->scale, fillObjects);
    } else {
        RenderShape(renderer, obj, fillObjects);
    }
}

void RenderSceneObjects(SDL_Renderer* renderer, bool fillObjects) {
    for (int i = 0; i < sceneSettings.objectCount; i++) {
        SceneObject* obj = &sceneSettings.sceneObjects[i];

        if (strcmp(obj->type, "circle") == 0) {
            RenderCircle(renderer, obj->x, obj->y, obj->radius * obj->scale, fillObjects);
        } else {
            RenderShape(renderer, obj, fillObjects);
        }
    }
}
