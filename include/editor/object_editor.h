#ifndef OBJECT_EDITOR_H
#define OBJECT_EDITOR_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include "config/config_manager.h"

struct SceneEditor;

// Object Types
#define OBJECT_CIRCLE 0
#define OBJECT_POLYGON 1

typedef enum {
    SHAPE_CIRCLE,
    SHAPE_SQUARE,
    SHAPE_POLYGON
} ShapeMode;

extern ShapeMode shapeMode;

typedef enum ObjectEditorHitRegion {
    OBJECT_EDITOR_HIT_NONE = 0,
    OBJECT_EDITOR_HIT_CONTROLS,
    OBJECT_EDITOR_HIT_ASSET_PANEL,
    OBJECT_EDITOR_HIT_MATERIAL_PANEL,
    OBJECT_EDITOR_HIT_CANVAS
} ObjectEditorHitRegion;

// Object Editor Functions
void InitializeObjectEditor(void);
void RenderObjectEditor(SDL_Renderer* renderer);
void HandleObjectEditorEvents(SDL_Event* event);
ObjectEditorHitRegion ObjectEditorHitRegionAtPoint(int mx, int my);

// Sub-functions for event handling
void HandleObjectEditorMouseClick(SDL_Event* event);
void HandleObjectEditorMouseDrag(SDL_Event* event);
void HandleObjectEditorMouseRelease(SDL_Event* event);
void HandleObjectEditorKeyPress(SDL_Event* event);

// Object Manipulation
void AddObject(int type, int x, int y);
void RemoveObject(int index);

// Selection & Interaction
void SelectObject(int index);
void DeselectObject(void);
bool IsInsideHandle(SceneObject* obj, int mx, int my);
bool CheckObjectClick(double mx, double my);
int ObjectEditorGetSelectedObjectIndex(void);
void ObjectEditorSetSelectedObjectIndex(int index);
void ObjectEditorAssignMaterialToSelected(int material_id);

#endif // OBJECT_EDITOR_H
