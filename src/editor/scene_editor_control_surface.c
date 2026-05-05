#include "editor/scene_editor_control_surface.h"

#include <stdio.h>
#include <string.h>

#include "app/animation.h"
#include "config/config_manager.h"
#include "editor/editor_mode_router.h"
#include "editor/material_editor.h"

bool SceneEditorControlSurfaceLocksObjectMode(void) {
    return AnimationUseFluidScene();
}

const char* SceneEditorControlSurfaceModeLabel(int mode) {
    switch (mode) {
        case EDITOR_MODE_PATH: return "Bezier";
        case EDITOR_MODE_OBJECT: return "Objects";
        case EDITOR_MODE_CAMERA: return "Camera";
        case EDITOR_MODE_MATERIAL: return "Material";
        default: return "Mode";
    }
}

static const char* SceneEditorControlSurfaceSourceLabel(void) {
    switch (animSettings.sceneSource) {
        case SCENE_SOURCE_FLUID_MANIFEST:
            return "Fluid Manifest";
        case SCENE_SOURCE_RUNTIME_SCENE:
            return "Runtime Scene";
        case SCENE_SOURCE_CONFIG_2D:
        default:
            return "2D Config";
    }
}

static const char* SceneEditorControlSurfaceSourcePath(void) {
    if (animSettings.sceneSource == SCENE_SOURCE_FLUID_MANIFEST &&
        animSettings.fluidManifest[0]) {
        return animSettings.fluidManifest;
    }
    if (animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE &&
        animSettings.runtimeScenePath[0]) {
        return animSettings.runtimeScenePath;
    }
    return "(default)";
}

static const char* SceneEditorControlSurfaceSafeText(const char* text, const char* fallback) {
    if (text && text[0]) return text;
    return fallback;
}

static const char* SceneEditorControlSurfaceResolvedSourcePath(
    const SceneEditorControlSurfaceInput* input) {
    if (!input) return "(default 2D config)";
    if (input->sourcePath && input->sourcePath[0] && strcmp(input->sourcePath, "(default)") != 0) {
        return input->sourcePath;
    }
    switch (input->sceneSource) {
        case SCENE_SOURCE_RUNTIME_SCENE:
            return "(runtime scene: none selected)";
        case SCENE_SOURCE_FLUID_MANIFEST:
            return "(fluid manifest: none selected)";
        case SCENE_SOURCE_CONFIG_2D:
        default:
            return "(default 2D config)";
    }
}

static const char* SceneEditorControlSurfaceCompactPath(const char* path,
                                                        char* buffer,
                                                        size_t buffer_size) {
    const char* name = NULL;
    size_t len = 0;
    if (!path || !path[0]) return "(default 2D config)";
    if (!buffer || buffer_size < 8) return path;
    len = strlen(path);
    if (len <= 58) return path;
    name = strrchr(path, '/');
    if (name && name[1]) {
        name += 1;
    } else {
        name = path + (len > 50 ? (len - 50) : 0);
    }
    snprintf(buffer, buffer_size, ".../%s", name);
    return buffer;
}

static SceneEditorControlSurfaceLane SceneEditorControlSurfaceResolveLane(
    const RayTracingRuntimeRoute* route) {
    if (RayTracingModeBackend_IsControlled3D(route)) {
        return SCENE_EDITOR_CONTROL_SURFACE_LANE_CONTROLLED_3D;
    }
    return SCENE_EDITOR_CONTROL_SURFACE_LANE_CANONICAL_2D;
}

void SceneEditorControlSurfaceBuildCurrent(int selected_object_index,
                                           SceneEditorControlSurfaceContract* out_contract) {
    SceneEditorControlSurfaceInput input = {0};
    RayTracingRuntimeRoute route = {0};

    if (!out_contract) return;

    route = RayTracingModeBackend_ResolveRoute();
    input.requestedMode = animSettings.editorMode;
    if (EditorModeRouter_ClampEditorMode(input.requestedMode,
                                         SceneEditorControlSurfaceLocksObjectMode()) ==
        EDITOR_MODE_MATERIAL) {
        int focused_index = MaterialEditorResolveFocusedObjectIndex();
        if (focused_index >= 0) {
            selected_object_index = focused_index;
        }
    }
    input.lockObjectMode = SceneEditorControlSurfaceLocksObjectMode();
    input.sceneSource = animSettings.sceneSource;
    input.sourceLabel = SceneEditorControlSurfaceSourceLabel();
    input.sourcePath = SceneEditorControlSurfaceSourcePath();
    input.objectCount = sceneSettings.objectCount;
    input.hasSelectedObject = (selected_object_index >= 0 &&
                               selected_object_index < sceneSettings.objectCount);
    input.selectedObjectIndex = selected_object_index;
    input.route = route;
    input.digestStatus = RayTracingModeBackend_BuildSceneDigestStatus(&route);
    SceneEditorControlSurfaceBuild(&input, out_contract);
}

void SceneEditorControlSurfaceBuild(const SceneEditorControlSurfaceInput* input,
                                    SceneEditorControlSurfaceContract* out_contract) {
    SceneEditorControlSurfaceContract contract = {0};
    const char* bounds_state = "off";
    const char* plane_axis = "?";
    bool canonical_2d_lane = false;
    bool controlled_3d_lane = false;
    int mode = 0;
    char compact_path[96] = {0};
    const char* resolved_source_path = NULL;

    if (!out_contract) return;
    if (!input) {
        memset(out_contract, 0, sizeof(*out_contract));
        return;
    }

    mode = EditorModeRouter_ClampEditorMode(input->requestedMode, input->lockObjectMode);
    contract.activeMode = mode;
    contract.modeSelectable[EDITOR_MODE_PATH] = true;
    contract.modeSelectable[EDITOR_MODE_OBJECT] = !input->lockObjectMode;
    contract.modeSelectable[EDITOR_MODE_CAMERA] = true;
    contract.modeSelectable[EDITOR_MODE_MATERIAL] = !input->lockObjectMode;

    contract.lane = SceneEditorControlSurfaceResolveLane(&input->route);
    contract.previewEnabled = true;
    contract.cycleModeEnabled = true;
    contract.applyEnabled = true;
    contract.saveEnabled = true;
    contract.backToMenuEnabled = true;
    contract.sharedKeyTabCycleEnabled = true;
    contract.sharedKeyEscapeEnabled = true;
    contract.laneKeyFrameEnabled = true;
    contract.laneGestureOrbitEnabled = (contract.lane == SCENE_EDITOR_CONTROL_SURFACE_LANE_CONTROLLED_3D);
    contract.laneWheelZoomEnabled = true;
    canonical_2d_lane = (contract.lane == SCENE_EDITOR_CONTROL_SURFACE_LANE_CANONICAL_2D);
    controlled_3d_lane = (contract.lane == SCENE_EDITOR_CONTROL_SURFACE_LANE_CONTROLLED_3D);

    contract.laneBezierCanvasEditEnabled = canonical_2d_lane;
    contract.laneObjectCanvasEditEnabled =
        canonical_2d_lane || (controlled_3d_lane && contract.activeMode == EDITOR_MODE_OBJECT);
    contract.laneCameraCanvasEditEnabled =
        canonical_2d_lane || (controlled_3d_lane && contract.activeMode == EDITOR_MODE_CAMERA);
    contract.laneViewportBezierPlacementEnabled =
        controlled_3d_lane && contract.activeMode == EDITOR_MODE_PATH;
    contract.laneViewportObjectPickEnabled =
        controlled_3d_lane && contract.activeMode == EDITOR_MODE_OBJECT;
    contract.laneViewportCameraPlacementEnabled =
        controlled_3d_lane && contract.activeMode == EDITOR_MODE_CAMERA;

    switch (contract.activeMode) {
        case EDITOR_MODE_PATH:
            contract.laneCanvasEditEnabled =
                contract.laneBezierCanvasEditEnabled || contract.laneViewportBezierPlacementEnabled;
            break;
        case EDITOR_MODE_OBJECT:
            contract.laneCanvasEditEnabled = contract.laneObjectCanvasEditEnabled;
            break;
        case EDITOR_MODE_CAMERA:
            contract.laneCanvasEditEnabled = contract.laneCameraCanvasEditEnabled;
            break;
        case EDITOR_MODE_MATERIAL:
            contract.laneCanvasEditEnabled = input->hasSelectedObject;
            break;
        default:
            contract.laneCanvasEditEnabled = false;
            break;
    }

    contract.paneLeftTitle = "Left Pane: Editor Controls";
    contract.paneCenterTitle = "Center Pane: Viewport";
    contract.paneRightTitle = "Right Pane: Program / Scene";
    contract.previewLabel = "Preview";
    contract.cycleModeLabel = "Cycle Mode";
    contract.applyLabel = "Apply";
    contract.saveLabel = "Save";
    contract.backToMenuLabel = "Back to Menu";

    snprintf(contract.statusTitle, sizeof(contract.statusTitle), "ray_tracing Scene Editor");
    snprintf(contract.statusSource,
             sizeof(contract.statusSource),
             "Source: %s",
             SceneEditorControlSurfaceSafeText(input->sourceLabel, "2D Config"));
    resolved_source_path = SceneEditorControlSurfaceResolvedSourcePath(input);
    snprintf(contract.statusPath,
             sizeof(contract.statusPath),
             "Path: %s",
             SceneEditorControlSurfaceCompactPath(resolved_source_path,
                                                  compact_path,
                                                  sizeof(compact_path)));
    snprintf(contract.statusObjects,
             sizeof(contract.statusObjects),
             "Objects: %d  Active: %s",
             input->objectCount,
             SceneEditorControlSurfaceModeLabel(contract.activeMode));
    if (input->hasSelectedObject &&
        input->selectedObjectIndex >= 0 &&
        input->selectedObjectIndex < input->objectCount) {
        snprintf(contract.statusObjects,
                 sizeof(contract.statusObjects),
                 "Objects: %d  Active: %s  Selected: #%d",
                 input->objectCount,
                 SceneEditorControlSurfaceModeLabel(contract.activeMode),
                 input->selectedObjectIndex);
    }
    snprintf(contract.statusRoute,
             sizeof(contract.statusRoute),
             "Route: %s",
             RayTracingModeBackend_Name(&input->route));

    if (contract.lane == SCENE_EDITOR_CONTROL_SURFACE_LANE_CANONICAL_2D) {
        snprintf(contract.statusDigest, sizeof(contract.statusDigest), "Digest: n/a (2D lane)");
    } else if (!input->digestStatus.valid) {
        snprintf(contract.statusDigest, sizeof(contract.statusDigest), "Digest: pending runtime 3D payload");
    } else {
        bounds_state = (input->digestStatus.hasSceneBounds && input->digestStatus.boundsEnabled) ? "on" : "off";
        if (input->digestStatus.constructionPlaneAxis[0]) {
            plane_axis = input->digestStatus.constructionPlaneAxis;
        }
        if (input->digestStatus.hasSceneBounds) {
            snprintf(contract.statusDigest,
                     sizeof(contract.statusDigest),
                     "Digest: prim=%d p=%d r=%d bounds=%s plane=%s@%.2f scaf=%d",
                     input->digestStatus.digestPrimitiveCount,
                     input->digestStatus.planePrimitiveCount,
                     input->digestStatus.rectPrismPrimitiveCount,
                     bounds_state,
                     plane_axis,
                     input->digestStatus.constructionPlaneOffset,
                     input->digestStatus.scaffoldPrimitiveCount);
        } else {
            snprintf(contract.statusDigest,
                     sizeof(contract.statusDigest),
                     "Digest: prim=%d p=%d r=%d b=%s plane=%s@%.2f scaf=%d",
                     input->digestStatus.digestPrimitiveCount,
                     input->digestStatus.planePrimitiveCount,
                     input->digestStatus.rectPrismPrimitiveCount,
                     bounds_state,
                     plane_axis,
                     input->digestStatus.constructionPlaneOffset,
                     input->digestStatus.scaffoldPrimitiveCount);
        }
    }

    if (contract.lane == SCENE_EDITOR_CONTROL_SURFACE_LANE_CANONICAL_2D) {
        snprintf(contract.statusRuntime, sizeof(contract.statusRuntime), "Runtime: 2D lane active.");
        snprintf(contract.statusControls,
                 sizeof(contract.statusControls),
                 "Controls: Shared TAB cycle ESC close | 2D LMB edit MMB pan Wheel zoom F frame");
    } else if (contract.lane == SCENE_EDITOR_CONTROL_SURFACE_LANE_CONTROLLED_3D) {
        if (RayTracingModeBackend_IsNative3D(&input->route)) {
            snprintf(contract.statusRuntime,
                     sizeof(contract.statusRuntime),
                     "Runtime: 3D native render route active; editor uses retained digest viewport controls.");
        } else {
            snprintf(contract.statusRuntime,
                     sizeof(contract.statusRuntime),
                     "Runtime: 3D compat fallback active; shared shell with digest viewport.");
        }
        if (contract.activeMode == EDITOR_MODE_PATH) {
            snprintf(contract.statusControls,
                     sizeof(contract.statusControls),
                     "Controls: TAB cycle ESC close Alt+drag orbit MMB pan Wheel zoom F frame LMB select bezier Shift+LMB add point Cmd+drag smooth");
        } else if (contract.activeMode == EDITOR_MODE_OBJECT) {
            snprintf(contract.statusControls,
                     sizeof(contract.statusControls),
                     "Controls: TAB cycle ESC close Alt+drag orbit MMB pan Wheel zoom F frame LMB pick object");
        } else if (contract.activeMode == EDITOR_MODE_CAMERA) {
            snprintf(contract.statusControls,
                     sizeof(contract.statusControls),
                     "Controls: TAB cycle ESC close Alt+drag orbit MMB pan Wheel zoom F frame LMB select camera Shift+LMB add camera point Cmd+drag smooth");
        } else {
            snprintf(contract.statusControls,
                     sizeof(contract.statusControls),
                     "Controls: TAB cycle ESC close Alt+drag orbit MMB pan Wheel zoom F frame | Material controls edit selected object");
        }
    }

    *out_contract = contract;
}
