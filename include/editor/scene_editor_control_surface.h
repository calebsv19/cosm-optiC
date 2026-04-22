#ifndef SCENE_EDITOR_CONTROL_SURFACE_H
#define SCENE_EDITOR_CONTROL_SURFACE_H

#include <stdbool.h>

#include "render/ray_tracing_mode_backend.h"

typedef enum SceneEditorControlSurfaceLane {
    SCENE_EDITOR_CONTROL_SURFACE_LANE_CANONICAL_2D = 0,
    SCENE_EDITOR_CONTROL_SURFACE_LANE_CONTROLLED_3D = 1,
    SCENE_EDITOR_CONTROL_SURFACE_LANE_NATIVE_3D_RESERVED = 2
} SceneEditorControlSurfaceLane;

typedef struct SceneEditorControlSurfaceInput {
    int requestedMode;
    bool lockObjectMode;
    SceneSource sceneSource;
    const char* sourceLabel;
    const char* sourcePath;
    int objectCount;
    bool hasSelectedObject;
    int selectedObjectIndex;
    RayTracingRuntimeRoute route;
    RayTracingSceneDigestStatus digestStatus;
} SceneEditorControlSurfaceInput;

typedef struct SceneEditorControlSurfaceContract {
    SceneEditorControlSurfaceLane lane;
    int activeMode;
    bool modeSelectable[3];
    bool previewEnabled;
    bool cycleModeEnabled;
    bool applyEnabled;
    bool saveEnabled;
    bool backToMenuEnabled;
    bool sharedKeyTabCycleEnabled;
    bool sharedKeyEscapeEnabled;
    bool laneKeyFrameEnabled;
    bool laneGestureOrbitEnabled;
    bool laneWheelZoomEnabled;
    bool laneBezierCanvasEditEnabled;
    bool laneObjectCanvasEditEnabled;
    bool laneCameraCanvasEditEnabled;
    bool laneViewportBezierPlacementEnabled;
    bool laneViewportObjectPickEnabled;
    bool laneViewportCameraPlacementEnabled;
    bool laneCanvasEditEnabled;
    const char* paneLeftTitle;
    const char* paneCenterTitle;
    const char* paneRightTitle;
    const char* previewLabel;
    const char* cycleModeLabel;
    const char* applyLabel;
    const char* saveLabel;
    const char* backToMenuLabel;
    char statusTitle[96];
    char statusSource[128];
    char statusPath[256];
    char statusObjects[160];
    char statusRoute[128];
    char statusDigest[256];
    char statusRuntime[192];
    char statusControls[192];
} SceneEditorControlSurfaceContract;

void SceneEditorControlSurfaceBuild(const SceneEditorControlSurfaceInput* input,
                                    SceneEditorControlSurfaceContract* out_contract);

#endif
