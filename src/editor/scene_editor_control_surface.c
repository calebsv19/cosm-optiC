#include "editor/scene_editor_control_surface.h"

#include <stdio.h>
#include <string.h>

#include "editor/editor_mode_router.h"

static const char* SceneEditorControlSurfaceModeLabel(int mode) {
    switch (mode) {
        case 0: return "Bezier";
        case 1: return "Objects";
        case 2: return "Camera";
        default: return "Mode";
    }
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
    if (RayTracingModeBackend_IsNative3D(route)) {
        return SCENE_EDITOR_CONTROL_SURFACE_LANE_NATIVE_3D_RESERVED;
    }
    if (RayTracingModeBackend_IsCompat3DFallback(route)) {
        return SCENE_EDITOR_CONTROL_SURFACE_LANE_CONTROLLED_3D;
    }
    return SCENE_EDITOR_CONTROL_SURFACE_LANE_CANONICAL_2D;
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
    contract.modeSelectable[0] = true;
    contract.modeSelectable[1] = !input->lockObjectMode;
    contract.modeSelectable[2] = true;

    contract.lane = SceneEditorControlSurfaceResolveLane(&input->route);
    contract.previewEnabled = (contract.lane != SCENE_EDITOR_CONTROL_SURFACE_LANE_NATIVE_3D_RESERVED);
    contract.cycleModeEnabled = true;
    contract.applyEnabled = true;
    contract.sharedKeyTabCycleEnabled = true;
    contract.sharedKeyEscapeEnabled = true;
    contract.laneKeyFrameEnabled = (contract.lane != SCENE_EDITOR_CONTROL_SURFACE_LANE_NATIVE_3D_RESERVED);
    contract.laneGestureOrbitEnabled = (contract.lane == SCENE_EDITOR_CONTROL_SURFACE_LANE_CONTROLLED_3D);
    contract.laneWheelZoomEnabled = (contract.lane != SCENE_EDITOR_CONTROL_SURFACE_LANE_NATIVE_3D_RESERVED);
    canonical_2d_lane = (contract.lane == SCENE_EDITOR_CONTROL_SURFACE_LANE_CANONICAL_2D);
    controlled_3d_lane = (contract.lane == SCENE_EDITOR_CONTROL_SURFACE_LANE_CONTROLLED_3D);

    contract.laneBezierCanvasEditEnabled = canonical_2d_lane;
    contract.laneObjectCanvasEditEnabled =
        canonical_2d_lane || (controlled_3d_lane && contract.activeMode == 1);
    contract.laneCameraCanvasEditEnabled = canonical_2d_lane;
    contract.laneViewportBezierPlacementEnabled = controlled_3d_lane && contract.activeMode == 0;
    contract.laneViewportObjectPickEnabled = controlled_3d_lane && contract.activeMode == 1;

    switch (contract.activeMode) {
        case 0:
            contract.laneCanvasEditEnabled =
                contract.laneBezierCanvasEditEnabled || contract.laneViewportBezierPlacementEnabled;
            break;
        case 1:
            contract.laneCanvasEditEnabled = contract.laneObjectCanvasEditEnabled;
            break;
        case 2:
            contract.laneCanvasEditEnabled = contract.laneCameraCanvasEditEnabled;
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
    } else if (contract.lane == SCENE_EDITOR_CONTROL_SURFACE_LANE_NATIVE_3D_RESERVED) {
        snprintf(contract.statusDigest,
                 sizeof(contract.statusDigest),
                 "Digest: unavailable (native 3D lane reserved)");
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
        snprintf(contract.statusRuntime,
                 sizeof(contract.statusRuntime),
                 "Runtime: 3D compat fallback active; shared shell with digest viewport.");
        if (contract.activeMode == 0) {
            snprintf(contract.statusControls,
                     sizeof(contract.statusControls),
                     "Controls: TAB cycle ESC close Alt+drag orbit MMB pan Wheel zoom F frame LMB select bezier Shift+LMB add point Cmd+drag smooth");
        } else if (contract.activeMode == 1) {
            snprintf(contract.statusControls,
                     sizeof(contract.statusControls),
                     "Controls: TAB cycle ESC close Alt+drag orbit MMB pan Wheel zoom F frame LMB pick object");
        } else {
            snprintf(contract.statusControls,
                     sizeof(contract.statusControls),
                     "Controls: TAB cycle ESC close Alt+drag orbit MMB pan Wheel zoom F frame");
        }
    } else {
        snprintf(contract.statusRuntime,
                 sizeof(contract.statusRuntime),
                 "Runtime: 3D native lane reserved; editor control provider pending.");
        snprintf(contract.statusControls,
                 sizeof(contract.statusControls),
                 "Controls: Shared TAB cycle ESC close | native 3D controls pending (preview/frame disabled).");
    }

    *out_contract = contract;
}
