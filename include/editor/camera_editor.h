#ifndef CAMERA_EDITOR_H
#define CAMERA_EDITOR_H

#include <SDL2/SDL.h>
#include "config/config_manager.h"

// Camera Editor: Handles viewport adjustments and scene framing

// Initialization
void InitializeCameraEditor(void);

// Rendering
void RenderCameraEditor(SDL_Renderer* renderer);

// Event Handling
void HandleCameraEditorEvents(SDL_Event* event);

// Camera Controls
void MoveCamera(int deltaX, int deltaY);
void ZoomCamera(float zoomFactor);
void ResetCamera(void);

#endif // CAMERA_EDITOR_H
