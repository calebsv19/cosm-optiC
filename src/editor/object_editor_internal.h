#ifndef OBJECT_EDITOR_INTERNAL_H
#define OBJECT_EDITOR_INTERNAL_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "camera/camera.h"
#include "editor/object_editor.h"
#include "editor/object_editor_panels.h"
#include "editor/scene_editor.h"
#include "geo/shape_library.h"
#include "scene/object_manager.h"

extern int selectedObjectIndex;
extern int handleRadius;
extern bool draggingRotationHandle;
extern double lastWorldX;
extern double lastWorldY;
extern bool viewportPanDragging;
extern int viewportPanLastMouseX;
extern int viewportPanLastMouseY;
extern bool renderHandles;

extern SDL_Rect addButton;
extern SDL_Rect deleteButton;
extern SDL_Rect selectButton;

extern SDL_Rect objectHandlesButton;
extern SDL_Rect circleButton;
extern SDL_Rect squareButton;
extern SDL_Rect polygonButton;
extern SDL_Rect confirmPolygonButton;
extern SDL_Rect cancelPolygonButton;

extern SDL_Rect assetPanelRect;
extern SDL_Rect assetToggleRect;
extern SDL_Rect assetCollapseRect;
extern bool showImports;
extern int selectedAssetIndex;
extern ShapeAssetLibrary assetLib;
extern char importNames[][256];
extern int importCount;
extern SDL_Rect materialPanelRect;
extern SDL_Rect materialCollapseRect;
extern int selectedMaterialIndex;
extern bool assetsCollapsed;
extern bool materialsCollapsed;
extern int assetScroll;
extern int materialScroll;
extern ObjectEditorPanelSliderKind activeMaterialSlider;
extern bool polygonCreationActive;
extern double polygonPoints[][2];
extern int polygonPointCount;

bool ObjectEditorAddToolActive(void);
bool ObjectEditorDeleteToolActive(void);
bool ObjectEditorPointInRect(int x, int y, const SDL_Rect* rect);
void ObjectEditorApplySliderValueToSelected(ObjectEditorPanelSliderKind kind,
                                            double slider_value);
const char* ShapeAssetDir(void);
Camera BuildObjectEditorCamera(void);
CameraPoint ScreenToWorldObjectEditor(const Camera* camera, int sx, int sy);
void PanViewportObjectByScreenDelta(int prev_x, int prev_y, int cur_x, int cur_y);
void RefreshAssetLibrary(void);
void FinalizePolygonCreation(void);
void AddSceneObject(ObjectType type,
                    double x,
                    double y,
                    double param1,
                    double param2,
                    double points[][2],
                    int numPoints);
void RemoveSceneObject(int index);

#endif
