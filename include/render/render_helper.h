#ifndef RENDER_HELPER_H
#define RENDER_HELPER_H

#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <SDL2/SDL_ttf.h>
#include "config/config_manager.h"

// ✅ Declare rendering functions
void RenderSceneObject(SDL_Renderer* renderer, SceneObject* obj, bool fillObjects);
void RenderSceneObjects(SDL_Renderer* renderer, bool fillObjects);
void RenderShape(SDL_Renderer* renderer, SceneObject* obj, bool fillObjects);
void RenderCircle(SDL_Renderer* renderer, int x, int y, int radius, bool fillObjects);
void RenderDrawCircle(SDL_Renderer* renderer, int x, int y, int radius);
void RenderFillCircle(SDL_Renderer* renderer, int x, int y, int radius);
void RenderDrawShape(SDL_Renderer* renderer, SceneObject* obj);
void RenderFillShape(SDL_Renderer* renderer, SceneObject* obj);
void RenderStaticScene(SDL_Renderer* renderer);
void RenderButtonText(SDL_Renderer* renderer, SDL_Rect button, const char* text);
void RenderButtonTextWithColor(SDL_Renderer* renderer,
                               SDL_Rect button,
                               const char* text,
                               SDL_Color text_color);
void RenderLabelText(SDL_Renderer* renderer, SDL_Rect area, const char* text, SDL_Color color);
int RenderLabelTextLeft(SDL_Renderer* renderer, SDL_Rect area, const char* text, SDL_Color color);
int RenderLabelTextWrappedLeft(SDL_Renderer* renderer, SDL_Rect area, const char* text, SDL_Color color);
int CalculateObjectBrightness(SceneObject* obj, double lightX, double lightY);
int compareInts(const void* a, const void* b);
double RenderHelper_DepthScaleForObjectZ(double object_z);
double RenderHelper_DepthYOffsetPixelsForObjectZ(double object_z, double camera_zoom);

#endif // RENDER_HELPER_H
