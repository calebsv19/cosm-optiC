#ifndef CAMERA_EDITOR_H
#define CAMERA_EDITOR_H

#include <SDL2/SDL.h>
#include "config/config_manager.h"

typedef enum CameraEditorHitRegion {
    CAMERA_EDITOR_HIT_NONE = 0,
    CAMERA_EDITOR_HIT_CONTROLS,
    CAMERA_EDITOR_HIT_SLIDER,
    CAMERA_EDITOR_HIT_CANVAS
} CameraEditorHitRegion;

double GetCurrentMarginPixels(void);
void RenderEditorHUD(SDL_Renderer* renderer, const char* label, bool showRotation);

// Camera Editor: Handles viewport adjustments and scene framing

// Initialization
void InitializeCameraEditor(void);

// Rendering
void RenderCameraEditor(SDL_Renderer* renderer);

// Event Handling
void HandleCameraEditorEvents(SDL_Event* event);
CameraEditorHitRegion CameraEditorHitRegionAtPoint(int mx, int my);

#endif // CAMERA_EDITOR_H
