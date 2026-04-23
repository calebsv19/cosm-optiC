#ifndef CAMERA_EDITOR_H
#define CAMERA_EDITOR_H

#include <SDL2/SDL.h>
#include "config/config_manager.h"

typedef enum CameraEditorHitRegion {
    CAMERA_EDITOR_HIT_NONE = 0,
    CAMERA_EDITOR_HIT_CONTROLS,
    CAMERA_EDITOR_HIT_CANVAS
} CameraEditorHitRegion;

typedef enum CameraEditorSelectionKind {
    CAMERA_EDITOR_SELECTION_NONE = 0,
    CAMERA_EDITOR_SELECTION_POINT,
    CAMERA_EDITOR_SELECTION_BEZIER_HANDLE,
    CAMERA_EDITOR_SELECTION_ROTATION_HANDLE
} CameraEditorSelectionKind;

double GetCurrentMarginPixels(void);

// Camera Editor: Handles viewport adjustments and scene framing

// Initialization
void InitializeCameraEditor(void);

// Rendering
void RenderCameraEditor(SDL_Renderer* renderer);
int CameraEditorRenderPaneControls(SDL_Renderer* renderer, SDL_Rect content_bounds, int top_y, int bottom_y);

// Event Handling
void HandleCameraEditorEvents(SDL_Event* event);
CameraEditorHitRegion CameraEditorHitRegionAtPoint(int mx, int my);
int CameraEditorGetSelectedPointIndex(void);
void CameraEditorSetSelectedPointIndex(int index);
CameraEditorSelectionKind CameraEditorGetSelectionKind(void);
bool CameraEditorSelectBezierHandle(int segment_index, int handle_index);
bool CameraEditorSelectRotationHandle(int point_index);
void CameraEditorClearSelection(void);
bool CameraEditorMoveSelectedPointTo(double x, double y);
bool CameraEditorGetSelectedWorldPosition(double* out_x, double* out_y, double* out_z);
bool CameraEditorGetSelectedGizmoWorldPosition(double* out_x, double* out_y, double* out_z);
bool CameraEditorMoveSelectedGizmoTo(double x, double y, double z);
void CameraEditorSetSelectedPointZ(double z);
double CameraEditorGetPointRotation(int index);
double CameraEditorGetPointPitch(int index);

#endif // CAMERA_EDITOR_H
