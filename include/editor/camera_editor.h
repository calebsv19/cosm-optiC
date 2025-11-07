#ifndef CAMERA_EDITOR_H
#define CAMERA_EDITOR_H

#include <SDL2/SDL.h>
#include "config/config_manager.h"

double GetCurrentMarginPixels(void);
void RenderEditorHUD(SDL_Renderer* renderer, const char* label);

// Camera Editor: Handles viewport adjustments and scene framing

// Initialization
void InitializeCameraEditor(void);

// Rendering
void RenderCameraEditor(SDL_Renderer* renderer);

// Event Handling
void HandleCameraEditorEvents(SDL_Event* event);

#endif // CAMERA_EDITOR_H
