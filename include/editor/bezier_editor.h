#ifndef BEZIER_EDITOR_H
#define BEZIER_EDITOR_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "path/path_system.h"
#include "config/config_manager.h"

// Global dragging states (accessible by scene_editor.c)
extern int draggingPoint;
extern int draggingVelocity;

// Bézier Editor Mode Handling
void InitializeBezierEditor(void);
void RenderBezierEditor(SDL_Renderer* renderer);
void HandleBezierEditorEvents(SDL_Event* event, int* draggingPoint, int* draggingVelocity);
void ToggleBezierPathMode(void);

// Bézier Path Operations
void AddBezierPoint(int x, int y);
void RemoveBezierPoint(int index);
void MoveEndPoint(int mx, int my, int pointIndex);
void MoveVelocityHandle(int mx, int my, int segmentIndex, int handleIndex);


#endif // BEZIER_EDITOR_H
