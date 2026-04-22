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
void ToggleBezierPathMode(Path* path);

typedef enum BezierEditorHitRegion {
    BEZIER_EDITOR_HIT_NONE = 0,
    BEZIER_EDITOR_HIT_CONTROLS,
    BEZIER_EDITOR_HIT_CANVAS
} BezierEditorHitRegion;

typedef enum BezierEditorSelectionKind {
    BEZIER_EDITOR_SELECTION_NONE = 0,
    BEZIER_EDITOR_SELECTION_POINT,
    BEZIER_EDITOR_SELECTION_HANDLE
} BezierEditorSelectionKind;

BezierEditorHitRegion BezierEditorHitRegionAtPoint(int mx, int my);

// Bézier Path Operations
void AddBezierPoint(Path* path, int x, int y);
void AddBezierPointPrecise(Path* path, double x, double y, double default_handle_length);
void RemoveBezierPoint(Path* path, int index);
void MoveEndPoint(Path* path, int mx, int my, int pointIndex);
void MoveVelocityHandle(Path* path, int mx, int my, int segmentIndex, int handleIndex);
int BezierEditorGetSelectedPointIndex(void);
void BezierEditorSetSelectedPointIndex(int index);
BezierEditorSelectionKind BezierEditorGetSelectionKind(void);
void BezierEditorClearSelection(void);
void BezierEditorSelectHandle(int segmentIndex, int handleIndex);
bool BezierEditorGetSelectedHandle(int* out_segment_index, int* out_handle_index);
bool BezierEditorGetSelectionWorldPosition(double* out_x, double* out_y);
bool BezierEditorGetSelectionWorldPosition3D(double* out_x, double* out_y, double* out_z);
bool BezierEditorMoveSelectionTo(double world_x, double world_y);
bool BezierEditorMoveSelectionTo3D(double world_x, double world_y, double world_z);


#endif // BEZIER_EDITOR_H
