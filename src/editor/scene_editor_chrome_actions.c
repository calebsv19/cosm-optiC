#include "editor/scene_editor_chrome_actions.h"

#include <stdio.h>
#include <string.h>

#include "app/animation.h"
#include "config/config_manager.h"
#include "editor/bezier_editor.h"
#include "editor/camera_editor.h"
#include "editor/editor_mode_router.h"
#include "editor/material_editor.h"
#include "editor/object_editor.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_control_surface.h"
#include "editor/scene_editor_mesh_preview_render.h"
#include "editor/scene_editor_runtime_scene_persistence.h"
#include "editor/scene_editor_tool_state.h"

static bool scene_editor_chrome_actions_point_in_rect(int x, int y, const SDL_Rect* rect) {
    if (!rect) return false;
    return x >= rect->x && x <= rect->x + rect->w &&
           y >= rect->y && y <= rect->y + rect->h;
}

static bool scene_editor_chrome_actions_viewport_rect_contains_event_point(
    const SceneEditorChromeActionsEnvironment* env,
    const SDL_Event* event) {
    int mx = 0;
    int my = 0;
    if (!event) return false;
    if (!env || !env->pane_layout_valid || !env->pane_layout) return true;
    if (event->type == SDL_MOUSEMOTION) {
        mx = event->motion.x;
        my = event->motion.y;
    } else if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) {
        mx = event->button.x;
        my = event->button.y;
    } else if (event->type == SDL_MOUSEWHEEL) {
        SDL_GetMouseState(&mx, &my);
    } else {
        return false;
    }
    return scene_editor_chrome_actions_point_in_rect(mx, my, &env->pane_layout->viewport_rect);
}

static bool scene_editor_save_current_authoring(void) {
    char diagnostics[256];

    if (animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE &&
        animSettings.runtimeScenePath[0] != '\0') {
        if (!SceneEditorRuntimeScenePersistAuthoring(diagnostics, sizeof(diagnostics))) {
            fprintf(stderr,
                    "[editor] failed to persist runtime scene authoring '%s': %s\n",
                    animSettings.runtimeScenePath,
                    diagnostics);
            return false;
        }
    }
    SaveAllSettings();
    return true;
}

bool SceneEditorChromeActionsResolve(const SDL_Event* event, SceneEditorChromeAction* out_action) {
    int selected_mode = 0;
    int mx = 0;
    int my = 0;
    SceneEditorControlSurfaceContract contract = {0};
    if (!event || !out_action) return false;
    SceneEditorControlSurfaceBuildCurrent(ObjectEditorGetSelectedObjectIndex(), &contract);
    out_action->kind = SCENE_EDITOR_CHROME_ACTION_NONE;
    out_action->mode_index = -1;
    if (event->type != SDL_MOUSEBUTTONDOWN) return false;
    if (event->button.button != SDL_BUTTON_LEFT) return false;
    mx = event->button.x;
    my = event->button.y;
    selected_mode = SceneEditorChromeShellResolveModeButtonAtPoint(mx, my);
    if (selected_mode >= 0) {
        out_action->kind = SCENE_EDITOR_CHROME_ACTION_MODE_SELECT;
        out_action->mode_index = selected_mode;
        return true;
    }
    if (scene_editor_chrome_actions_point_in_rect(mx, my, &previewButton)) {
        if (!contract.previewEnabled) return false;
        out_action->kind = SCENE_EDITOR_CHROME_ACTION_PREVIEW;
        return true;
    }
    if (scene_editor_chrome_actions_point_in_rect(mx, my, &changeModeButton)) {
        if (!contract.cycleModeEnabled) return false;
        out_action->kind = SCENE_EDITOR_CHROME_ACTION_CYCLE_MODE;
        return true;
    }
    if (scene_editor_chrome_actions_point_in_rect(mx, my, &applyButton)) {
        if (!contract.applyEnabled) return false;
        out_action->kind = SCENE_EDITOR_CHROME_ACTION_APPLY;
        return true;
    }
    if (scene_editor_chrome_actions_point_in_rect(mx, my, &saveButton)) {
        if (!contract.saveEnabled) return false;
        out_action->kind = SCENE_EDITOR_CHROME_ACTION_SAVE;
        return true;
    }
    if (scene_editor_chrome_actions_point_in_rect(mx, my, &backToMenuButton)) {
        if (!contract.backToMenuEnabled) return false;
        out_action->kind = SCENE_EDITOR_CHROME_ACTION_BACK_TO_MENU;
        return true;
    }
    return false;
}

void SceneEditorChromeActionsApply(SceneEditor* editor,
                                   const SceneEditorChromeAction* action,
                                   const SceneEditorChromeActionsEnvironment* env) {
    SceneEditorControlSurfaceContract contract = {0};
    if (!editor || !action || action->kind == SCENE_EDITOR_CHROME_ACTION_NONE) return;
    SceneEditorControlSurfaceBuildCurrent(ObjectEditorGetSelectedObjectIndex(), &contract);
    if (action->kind == SCENE_EDITOR_CHROME_ACTION_MODE_SELECT) {
        bool selectable = (action->mode_index >= 0 &&
                           action->mode_index < EDITOR_MODE_COUNT &&
                           contract.modeSelectable[action->mode_index]);
        if (selectable) {
            int clamped_mode = EditorModeRouter_ClampEditorMode(action->mode_index,
                                                                SceneEditorControlSurfaceLocksObjectMode());
            editor->currentMode = clamped_mode;
            animSettings.editorMode = clamped_mode;
            if (env && env->initialize_editor_mode) {
                env->initialize_editor_mode(editor);
            }
            printf("Changed Mode to %d via mode router\n", editor->currentMode);
        } else {
            printf("Mode %d currently unavailable in this scene source.\n", action->mode_index);
        }
        return;
    }
    if (action->kind == SCENE_EDITOR_CHROME_ACTION_PREVIEW) {
        if (!contract.previewEnabled) {
            return;
        }
        RunPreviewModeEmbedded(editor->window, editor->renderer);
        if (env && env->resume_after_preview) {
            env->resume_after_preview(editor);
        }
        return;
    }
    if (action->kind == SCENE_EDITOR_CHROME_ACTION_CYCLE_MODE) {
        if (!contract.cycleModeEnabled) {
            return;
        }
        editor->currentMode = EditorModeRouter_NextEditorMode(editor->currentMode,
                                                              false,
                                                              SceneEditorControlSurfaceLocksObjectMode());
        animSettings.editorMode = editor->currentMode;
        if (env && env->initialize_editor_mode) {
            env->initialize_editor_mode(editor);
        }
        printf("Changed Mode to %d\n", editor->currentMode);
        return;
    }
    if (action->kind == SCENE_EDITOR_CHROME_ACTION_APPLY) {
        if (!contract.applyEnabled) {
            return;
        }
        if (!scene_editor_save_current_authoring()) {
            SceneEditorChromeShellSetActionFeedback("Scene apply failed", 2200);
            return;
        }
        SceneEditorChromeShellSetActionFeedback("Scene changes applied", 1800);
        printf("Applied scene editor authoring in-app for mode %d\n", editor->currentMode);
        return;
    }
    if (action->kind == SCENE_EDITOR_CHROME_ACTION_SAVE) {
        if (!contract.saveEnabled) {
            return;
        }
        if (!scene_editor_save_current_authoring()) {
            SceneEditorChromeShellSetActionFeedback("Scene save failed", 2200);
            return;
        }
        SceneEditorChromeShellSetActionFeedback("Scene saved", 1800);
        printf("Saved scene editor authoring in mode %d\n", editor->currentMode);
        return;
    }
    if (action->kind == SCENE_EDITOR_CHROME_ACTION_BACK_TO_MENU) {
        if (!contract.backToMenuEnabled) {
            return;
        }
        editor->running = false;
        sceneEditorExitFlag = true;
        printf("Exited scene editor from mode %d\n", editor->currentMode);
    }
}

static bool scene_editor_dispatch_bezier_pane_command(const SceneEditorPaneCommand* command) {
    if (!command || !command->event) return false;
    switch (command->kind) {
        case SCENE_EDITOR_PANE_COMMAND_POINTER_DOWN:
        case SCENE_EDITOR_PANE_COMMAND_POINTER_UP:
        case SCENE_EDITOR_PANE_COMMAND_POINTER_DRAG:
        case SCENE_EDITOR_PANE_COMMAND_KEY:
            HandleBezierEditorEvents(command->event, &draggingPoint, &draggingVelocity);
            return true;
        default:
            return false;
    }
}

static bool scene_editor_dispatch_object_pane_command(const SceneEditorPaneCommand* command) {
    if (!command || !command->event) return false;
    switch (command->kind) {
        case SCENE_EDITOR_PANE_COMMAND_POINTER_DOWN:
        case SCENE_EDITOR_PANE_COMMAND_POINTER_UP:
        case SCENE_EDITOR_PANE_COMMAND_POINTER_DRAG:
        case SCENE_EDITOR_PANE_COMMAND_WHEEL:
        case SCENE_EDITOR_PANE_COMMAND_KEY:
            HandleObjectEditorEvents(command->event);
            return true;
        default:
            return false;
    }
}

static bool scene_editor_dispatch_camera_pane_command(const SceneEditorPaneCommand* command) {
    if (!command || !command->event) return false;
    switch (command->kind) {
        case SCENE_EDITOR_PANE_COMMAND_POINTER_DOWN:
        case SCENE_EDITOR_PANE_COMMAND_POINTER_UP:
        case SCENE_EDITOR_PANE_COMMAND_POINTER_DRAG:
        case SCENE_EDITOR_PANE_COMMAND_WHEEL:
        case SCENE_EDITOR_PANE_COMMAND_KEY:
            HandleCameraEditorEvents(command->event);
            return true;
        default:
            return false;
    }
}

static bool scene_editor_dispatch_material_pane_command(const SceneEditorPaneCommand* command) {
    if (!command || !command->event) return false;
    switch (command->kind) {
        case SCENE_EDITOR_PANE_COMMAND_POINTER_DOWN:
        case SCENE_EDITOR_PANE_COMMAND_POINTER_UP:
        case SCENE_EDITOR_PANE_COMMAND_POINTER_DRAG:
        case SCENE_EDITOR_PANE_COMMAND_WHEEL:
            HandleMaterialEditorEvents(command->event);
            return true;
        default:
            return false;
    }
}

static bool scene_editor_contract_canvas_allowed_for_target(
    const SceneEditorControlSurfaceContract* contract,
    SceneEditorInputTarget target) {
    if (!contract) return false;
    switch (target) {
        case SCENE_EDITOR_INPUT_TARGET_BEZIER_PANE:
            return contract->laneBezierCanvasEditEnabled;
        case SCENE_EDITOR_INPUT_TARGET_OBJECT_PANE:
            return contract->laneObjectCanvasEditEnabled;
        case SCENE_EDITOR_INPUT_TARGET_CAMERA_PANE:
            return contract->laneCameraCanvasEditEnabled;
        case SCENE_EDITOR_INPUT_TARGET_MATERIAL_PANE:
            return contract->activeMode == EDITOR_MODE_MATERIAL && contract->laneCanvasEditEnabled;
        default:
            return false;
    }
}

static bool scene_editor_dispatch_controlled_3d_object_canvas_command(
    const SceneEditorChromeActionsEnvironment* env,
    const SceneEditorPaneCommand* command) {
    RuntimeSceneBridge3DDigestState digest = {0};
    SceneEditorDigestOverlayProjector projector = {0};
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    double plane_z = 0.0;
    SceneEditorTool active_tool = SceneEditorToolStateGetEffective(SDL_GetModState());
    int pick = -1;
    double world_x = 0.0;
    double world_y = 0.0;
    double world_z = 0.0;
    if (!env || !env->pane_layout || !env->viewport_nav_state || !env->digest_hover_object_index) return false;
    if (!command || !command->event) return false;
    if (command->kind != SCENE_EDITOR_PANE_COMMAND_POINTER_DOWN) return false;
    if (command->event->type != SDL_MOUSEBUTTONDOWN ||
        command->event->button.button != SDL_BUTTON_LEFT) {
        return false;
    }
    if (!scene_editor_chrome_actions_viewport_rect_contains_event_point(env, command->event)) return false;
    if (!SceneEditorDigestOverlayResolve(&digest)) return false;
    if (!SceneEditorDigestOverlayBuildProjector(&digest,
                                                &env->pane_layout->viewport_rect,
                                                env->viewport_nav_state,
                                                &projector)) return false;
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(&digest, &projector);
    plane_z = SceneEditorDigestOverlayResolveEditPlaneZ(&digest, &projector);

    if (SceneEditorMeshPreviewHandleModeClick(&projector.viewport,
                                              command->event->button.x,
                                              command->event->button.y)) {
        return true;
    }

    pick = SceneEditorDigestOverlayPickObjectIndex(&projector,
                                                   &digest,
                                                   command->event->button.x,
                                                   command->event->button.y);
    if (pick < 0) {
        pick = *env->digest_hover_object_index;
    }
    if (pick >= 0) {
        if (active_tool == SCENE_EDITOR_TOOL_DELETE) {
            return ObjectEditorDeleteObjectIndex(pick);
        }
        ObjectEditorSetSelectedObjectIndex(pick);
        return true;
    }
    if (active_tool != SCENE_EDITOR_TOOL_ADD) {
        ObjectEditorSetSelectedObjectIndex(-1);
        return true;
    }
    if (!SceneEditorDigestOverlayScreenRayToPlanePoint(&projector,
                                                       command->event->button.x,
                                                       command->event->button.y,
                                                       plane_z,
                                                       &world_x,
                                                       &world_y,
                                                       &world_z)) {
        return false;
    }
    world_x = SceneEditorDigestOverlayQuantizeWorldValue(world_x, metrics.snap_step);
    world_y = SceneEditorDigestOverlayQuantizeWorldValue(world_y, metrics.snap_step);
    return ObjectEditorAddPlacementAt(world_x, world_y);
}

static bool scene_editor_dispatch_controlled_3d_camera_canvas_command(
    const SceneEditorChromeActionsEnvironment* env,
    const SceneEditorPaneCommand* command) {
    RuntimeSceneBridge3DDigestState digest = {0};
    SceneEditorDigestOverlayProjector projector = {0};
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    double plane_z = 0.0;
    SDL_Keymod mods = SDL_GetModState();
    SceneEditorTool active_tool = SceneEditorToolStateGetEffective(mods);
    SceneEditorBezier3DGizmoAxis axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    int pick = -1;
    int handle_segment = -1;
    int handle_index = -1;
    int rotation_pick = -1;
    double world_x = 0.0;
    double world_y = 0.0;
    double world_z = 0.0;
    if (!env || !env->pane_layout || !env->viewport_nav_state || !env->camera_gizmo_state) return false;
    if (!command || !command->event) return false;
    if (!scene_editor_chrome_actions_viewport_rect_contains_event_point(env, command->event)) return false;
    if (!SceneEditorDigestOverlayResolve(&digest)) return false;
    if (!SceneEditorDigestOverlayBuildProjector(&digest,
                                                &env->pane_layout->viewport_rect,
                                                env->viewport_nav_state,
                                                &projector)) return false;
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(&digest, &projector);
    plane_z = SceneEditorDigestOverlayResolveEditPlaneZ(&digest, &projector);

    if (command->kind == SCENE_EDITOR_PANE_COMMAND_POINTER_UP) {
        if (command->event->type == SDL_MOUSEBUTTONUP &&
            command->event->button.button == SDL_BUTTON_LEFT &&
            env->camera_gizmo_state->dragging) {
            memset(env->camera_gizmo_state, 0, sizeof(*env->camera_gizmo_state));
            env->camera_gizmo_state->drag_axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
            return true;
        }
        return false;
    }
    if (command->kind == SCENE_EDITOR_PANE_COMMAND_POINTER_DRAG) {
        if (command->event->type == SDL_MOUSEMOTION &&
            env->camera_gizmo_state->dragging &&
            (command->event->motion.state & SDL_BUTTON_LMASK)) {
            return SceneEditorDigestOverlayApplyCameraGizmoDrag(&projector,
                                                                &digest,
                                                                env->camera_gizmo_state,
                                                                command->event->motion.x,
                                                                command->event->motion.y);
        }
        return false;
    }
    if (command->kind != SCENE_EDITOR_PANE_COMMAND_POINTER_DOWN) return false;
    if (command->event->type != SDL_MOUSEBUTTONDOWN ||
        command->event->button.button != SDL_BUTTON_LEFT) {
        return false;
    }

    if (active_tool == SCENE_EDITOR_TOOL_SELECT) {
        axis = SceneEditorDigestOverlayPickCameraGizmoAxis(&projector,
                                                           &digest,
                                                           command->event->button.x,
                                                           command->event->button.y);
        if (axis != SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE) {
            if (SceneEditorDigestOverlayResolveSelectedCameraGizmoWorldPosition(&projector,
                                                                                &digest,
                                                                                &env->camera_gizmo_state->drag_start_world_x,
                                                                                &env->camera_gizmo_state->drag_start_world_y,
                                                                                &env->camera_gizmo_state->drag_start_world_z)) {
                env->camera_gizmo_state->dragging = true;
                env->camera_gizmo_state->drag_axis = axis;
                env->camera_gizmo_state->smooth_drag = ((mods & (KMOD_GUI | KMOD_CTRL)) != 0);
                env->camera_gizmo_state->drag_start_mouse_x = command->event->button.x;
                env->camera_gizmo_state->drag_start_mouse_y = command->event->button.y;
                return true;
            }
            return false;
        }

        if (SceneEditorDigestOverlayPickCameraBezierHandle(&projector,
                                                           command->event->button.x,
                                                           command->event->button.y,
                                                           &handle_segment,
                                                           &handle_index)) {
            memset(env->camera_gizmo_state, 0, sizeof(*env->camera_gizmo_state));
            env->camera_gizmo_state->drag_axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
            return CameraEditorSelectBezierHandle(handle_segment, handle_index);
        }

        rotation_pick = SceneEditorDigestOverlayPickCameraRotationHandle(&projector,
                                                                         &digest,
                                                                         command->event->button.x,
                                                                         command->event->button.y);
        if (rotation_pick >= 0) {
            memset(env->camera_gizmo_state, 0, sizeof(*env->camera_gizmo_state));
            env->camera_gizmo_state->drag_axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
            return CameraEditorSelectRotationHandle(rotation_pick);
        }
    }

    pick = SceneEditorDigestOverlayPickCameraPointIndex(&projector,
                                                        command->event->button.x,
                                                        command->event->button.y);
    if (pick >= 0) {
        memset(env->camera_gizmo_state, 0, sizeof(*env->camera_gizmo_state));
        env->camera_gizmo_state->drag_axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
        if (active_tool == SCENE_EDITOR_TOOL_DELETE) {
            int old_count = sceneSettings.cameraPath.numPoints;
            CameraPath3D_RemovePoint(&sceneSettings.cameraPath3D, pick, old_count);
            RemoveBezierPoint(&sceneSettings.cameraPath, pick);
            CameraEditorClearSelection();
            return true;
        }
        CameraEditorSetSelectedPointIndex(pick);
        return true;
    }

    if (active_tool != SCENE_EDITOR_TOOL_ADD) {
        memset(env->camera_gizmo_state, 0, sizeof(*env->camera_gizmo_state));
        env->camera_gizmo_state->drag_axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
        CameraEditorClearSelection();
        return true;
    }

    if (!SceneEditorDigestOverlayScreenRayToPlanePoint(&projector,
                                                       command->event->button.x,
                                                       command->event->button.y,
                                                       plane_z,
                                                       &world_x,
                                                       &world_y,
                                                       &world_z)) {
        return false;
    }
    world_x = SceneEditorDigestOverlayQuantizeWorldValue(world_x, metrics.snap_step);
    world_y = SceneEditorDigestOverlayQuantizeWorldValue(world_y, metrics.snap_step);
    world_z = SceneEditorDigestOverlayQuantizeWorldValue(world_z, metrics.snap_step);
    if (!CameraPath3D_InsertPoint(&sceneSettings.cameraPath3D,
                                  &sceneSettings.cameraPath,
                                  world_x,
                                  world_y,
                                  world_z,
                                  metrics.default_handle_length)) {
        return false;
    }
    if (sceneSettings.cameraPath.numPoints > 0) {
        int new_index = sceneSettings.cameraPath.numPoints - 1;
        double seed_rot = (new_index > 0)
                              ? sceneSettings.cameraPath.rotations[new_index - 1]
                              : sceneSettings.camera.rotation;
        sceneSettings.cameraPath.rotations[new_index] = seed_rot;
        sceneSettings.cameraPath.rotationSet[new_index] = true;
        CameraEditorSetSelectedPointIndex(new_index);
        return true;
    }
    return false;
}

static bool scene_editor_dispatch_controlled_3d_bezier_canvas_command(
    const SceneEditorChromeActionsEnvironment* env,
    const SceneEditorPaneCommand* command) {
    RuntimeSceneBridge3DDigestState digest = {0};
    SceneEditorDigestOverlayProjector projector = {0};
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    double plane_z = 0.0;
    SDL_Keymod mods = SDL_GetModState();
    SceneEditorTool active_tool = SceneEditorToolStateGetEffective(mods);
    SceneEditorBezier3DGizmoAxis axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    int handle_segment = -1;
    int handle_index = -1;
    int pick = -1;
    double world_x = 0.0;
    double world_y = 0.0;
    double world_z = 0.0;
    int previous_point_count = 0;
    if (!env || !env->pane_layout || !env->viewport_nav_state || !env->bezier_gizmo_state) return false;
    if (!command || !command->event) return false;
    if (!scene_editor_chrome_actions_viewport_rect_contains_event_point(env, command->event)) return false;
    if (!SceneEditorDigestOverlayResolve(&digest)) return false;
    if (!SceneEditorDigestOverlayBuildProjector(&digest,
                                                &env->pane_layout->viewport_rect,
                                                env->viewport_nav_state,
                                                &projector)) return false;
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(&digest, &projector);
    plane_z = SceneEditorDigestOverlayResolveEditPlaneZ(&digest, &projector);
    if (command->kind == SCENE_EDITOR_PANE_COMMAND_POINTER_UP) {
        if (command->event->type == SDL_MOUSEBUTTONUP &&
            command->event->button.button == SDL_BUTTON_LEFT &&
            env->bezier_gizmo_state->dragging) {
            memset(env->bezier_gizmo_state, 0, sizeof(*env->bezier_gizmo_state));
            env->bezier_gizmo_state->drag_axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
            return true;
        }
        return false;
    }
    if (command->kind == SCENE_EDITOR_PANE_COMMAND_POINTER_DRAG) {
        if (command->event->type == SDL_MOUSEMOTION &&
            env->bezier_gizmo_state->dragging &&
            (command->event->motion.state & SDL_BUTTON_LMASK)) {
            return SceneEditorDigestOverlayApplyBezierGizmoDrag(&projector,
                                                                &digest,
                                                                env->bezier_gizmo_state,
                                                                command->event->motion.x,
                                                                command->event->motion.y);
        }
        return false;
    }
    if (command->kind != SCENE_EDITOR_PANE_COMMAND_POINTER_DOWN) return false;
    if (command->event->type != SDL_MOUSEBUTTONDOWN ||
        command->event->button.button != SDL_BUTTON_LEFT) {
        return false;
    }
    if (active_tool == SCENE_EDITOR_TOOL_SELECT) {
        axis = SceneEditorDigestOverlayPickBezierGizmoAxis(&projector,
                                                           &digest,
                                                           command->event->button.x,
                                                           command->event->button.y);
        if (axis != SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE) {
            if (!SceneEditorDigestOverlayBezierGizmoAxisLocked(axis) &&
                BezierEditorGetSelectionWorldPosition3D(&env->bezier_gizmo_state->drag_start_world_x,
                                                        &env->bezier_gizmo_state->drag_start_world_y,
                                                        &env->bezier_gizmo_state->drag_start_world_z)) {
                env->bezier_gizmo_state->dragging = true;
                env->bezier_gizmo_state->drag_axis = axis;
                env->bezier_gizmo_state->smooth_drag = ((mods & (KMOD_GUI | KMOD_CTRL)) != 0);
                env->bezier_gizmo_state->drag_start_mouse_x = command->event->button.x;
                env->bezier_gizmo_state->drag_start_mouse_y = command->event->button.y;
                return true;
            }
            return false;
        }
        if (SceneEditorDigestOverlayPickBezierHandle(&projector,
                                                     &sceneSettings.bezierPath,
                                                     &sceneSettings.bezierPath3D,
                                                     plane_z,
                                                     command->event->button.x,
                                                     command->event->button.y,
                                                     &handle_segment,
                                                     &handle_index) >= 0) {
            memset(env->bezier_gizmo_state, 0, sizeof(*env->bezier_gizmo_state));
            env->bezier_gizmo_state->drag_axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
            BezierEditorSelectHandle(handle_segment, handle_index);
            return true;
        }
    }
    pick = SceneEditorDigestOverlayPickBezierPointIndex(&projector,
                                                        &sceneSettings.bezierPath,
                                                        &sceneSettings.bezierPath3D,
                                                        plane_z,
                                                        command->event->button.x,
                                                        command->event->button.y);
    if (pick >= 0) {
        memset(env->bezier_gizmo_state, 0, sizeof(*env->bezier_gizmo_state));
        env->bezier_gizmo_state->drag_axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
        if (active_tool == SCENE_EDITOR_TOOL_DELETE) {
            RemoveBezierPoint(&sceneSettings.bezierPath, pick);
            BezierEditorClearSelection();
            return true;
        }
        BezierEditorSetSelectedPointIndex(pick);
        return true;
    }
    if (active_tool != SCENE_EDITOR_TOOL_ADD) {
        memset(env->bezier_gizmo_state, 0, sizeof(*env->bezier_gizmo_state));
        env->bezier_gizmo_state->drag_axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
        BezierEditorClearSelection();
        return true;
    }
    if (!SceneEditorDigestOverlayScreenRayToPlanePoint(&projector,
                                                       command->event->button.x,
                                                       command->event->button.y,
                                                       plane_z,
                                                       &world_x,
                                                       &world_y,
                                                       &world_z)) {
        return false;
    }
    previous_point_count = sceneSettings.bezierPath.numPoints;
    world_x = SceneEditorDigestOverlayQuantizeWorldValue(world_x, metrics.snap_step);
    world_y = SceneEditorDigestOverlayQuantizeWorldValue(world_y, metrics.snap_step);
    world_z = SceneEditorDigestOverlayQuantizeWorldValue(world_z, metrics.snap_step);
    CameraPath3D_InsertPoint(&sceneSettings.bezierPath3D,
                             &sceneSettings.bezierPath,
                             world_x,
                             world_y,
                             world_z,
                             metrics.default_handle_length);
    if (sceneSettings.bezierPath.numPoints > previous_point_count) {
        memset(env->bezier_gizmo_state, 0, sizeof(*env->bezier_gizmo_state));
        env->bezier_gizmo_state->drag_axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
        BezierEditorSetSelectedPointIndex(sceneSettings.bezierPath.numPoints - 1);
        return true;
    }
    return false;
}

static bool scene_editor_dispatch_material_canvas_command(
    const SceneEditorChromeActionsEnvironment* env,
    const SceneEditorPaneCommand* command) {
    RuntimeSceneBridge3DDigestState digest = {0};
    SceneEditorDigestOverlayProjector projector = {0};
    int focused_object_index = -1;
    bool focused_origin = false;
    bool additive = false;
    if (!env || !env->pane_layout || !env->viewport_nav_state) return false;
    if (!command || !command->event) return false;
    if (command->kind != SCENE_EDITOR_PANE_COMMAND_POINTER_DOWN) return false;
    if (command->event->type != SDL_MOUSEBUTTONDOWN ||
        command->event->button.button != SDL_BUTTON_LEFT) {
        return false;
    }
    if (!scene_editor_chrome_actions_viewport_rect_contains_event_point(env, command->event)) {
        return false;
    }
    focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    if (focused_object_index < 0) return false;
    if (!SceneEditorDigestOverlayResolve(&digest)) return false;
    focused_origin = MaterialEditorGetViewMode() == MATERIAL_EDITOR_VIEW_FOCUSED_ORIGIN;
    if (!SceneEditorDigestOverlayBuildObjectProjector(&digest,
                                                      &env->pane_layout->viewport_rect,
                                                      env->viewport_nav_state,
                                                      focused_object_index,
                                                      focused_origin,
                                                      &projector)) {
        return false;
    }
    if (SceneEditorMeshPreviewHandleModeClick(&projector.viewport,
                                              command->event->button.x,
                                              command->event->button.y)) {
        return true;
    }
    additive = (SDL_GetModState() & KMOD_SHIFT) != 0;
    return MaterialEditorHandleCanvasPointerDown(&projector,
                                                 command->event->button.x,
                                                 command->event->button.y,
                                                 additive);
}

void SceneEditorChromeActionsRoutePaneEvent(SceneEditor* editor,
                                            const SceneEditorChromeActionsEnvironment* env,
                                            const SceneEditorPaneCommand* command,
                                            SceneEditorInputRoutingResult* result) {
    SceneEditorControlSurfaceContract contract = {0};
    bool canvas_allowed_for_target = false;
    bool controlled_3d_viewport_command_region = false;
    if (!editor || !env || !command || !command->event || !result) return;
    SceneEditorControlSurfaceBuildCurrent(ObjectEditorGetSelectedObjectIndex(), &contract);
    if (env->handle_viewport_navigation && env->handle_viewport_navigation(editor, command, result)) {
        return;
    }
    controlled_3d_viewport_command_region =
        (contract.laneViewportObjectPickEnabled ||
         contract.laneViewportBezierPlacementEnabled ||
         contract.laneViewportCameraPlacementEnabled) &&
        (command->pane_hit_region == SCENE_EDITOR_PANE_HIT_CANVAS ||
         command->pane_hit_region == SCENE_EDITOR_PANE_HIT_DRAG);
    if (controlled_3d_viewport_command_region) {
        if (contract.laneViewportBezierPlacementEnabled &&
            command->target == SCENE_EDITOR_INPUT_TARGET_BEZIER_PANE) {
            result->consumed = scene_editor_dispatch_controlled_3d_bezier_canvas_command(env, command);
        } else if (contract.laneViewportObjectPickEnabled &&
                   command->target == SCENE_EDITOR_INPUT_TARGET_OBJECT_PANE) {
            result->consumed = scene_editor_dispatch_controlled_3d_object_canvas_command(env, command);
        } else if (contract.laneViewportCameraPlacementEnabled &&
                   command->target == SCENE_EDITOR_INPUT_TARGET_CAMERA_PANE) {
            result->consumed = scene_editor_dispatch_controlled_3d_camera_canvas_command(env, command);
        } else {
            result->consumed = false;
        }
        if (!result->consumed) {
            return;
        }
        result->target = command->target;
        result->pane_hit_region = (uint8_t)command->pane_hit_region;
        result->requested_target_invalidation = true;
        result->invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_PANE;
        if (command->pane_hit_region == SCENE_EDITOR_PANE_HIT_DRAG) {
            result->invalidation_class = SCENE_EDITOR_INVALIDATION_TARGET_INTERACTION;
            result->invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_PANE_DRAG;
        } else {
            result->invalidation_class = SCENE_EDITOR_INVALIDATION_TARGET_PANE;
            result->invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_PANE_CANVAS;
        }
        return;
    }
    canvas_allowed_for_target = scene_editor_contract_canvas_allowed_for_target(&contract, command->target);
    if (command->kind != SCENE_EDITOR_PANE_COMMAND_KEY &&
        command->pane_hit_region != SCENE_EDITOR_PANE_HIT_CONTROLS &&
        command->pane_hit_region != SCENE_EDITOR_PANE_HIT_LIST_PANEL &&
        !canvas_allowed_for_target) {
        return;
    }
    if (command->target == SCENE_EDITOR_INPUT_TARGET_MATERIAL_PANE &&
        command->pane_hit_region == SCENE_EDITOR_PANE_HIT_CANVAS) {
        result->consumed = scene_editor_dispatch_material_canvas_command(env, command);
        if (result->consumed) {
            result->target = command->target;
            result->pane_hit_region = (uint8_t)command->pane_hit_region;
            result->requested_target_invalidation = true;
            result->invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_PANE |
                                                SCENE_EDITOR_INVALIDATE_REASON_PANE_CANVAS;
            result->invalidation_class = SCENE_EDITOR_INVALIDATION_TARGET_PANE;
        }
        return;
    }
    switch (command->target) {
        case SCENE_EDITOR_INPUT_TARGET_BEZIER_PANE:
            result->consumed = scene_editor_dispatch_bezier_pane_command(command);
            break;
        case SCENE_EDITOR_INPUT_TARGET_OBJECT_PANE:
            result->consumed = scene_editor_dispatch_object_pane_command(command);
            break;
        case SCENE_EDITOR_INPUT_TARGET_CAMERA_PANE:
            result->consumed = scene_editor_dispatch_camera_pane_command(command);
            break;
        case SCENE_EDITOR_INPUT_TARGET_MATERIAL_PANE:
            result->consumed = scene_editor_dispatch_material_pane_command(command);
            break;
        default:
            result->consumed = false;
            break;
    }
    if (result->consumed) {
        result->target = command->target;
        result->pane_hit_region = (uint8_t)command->pane_hit_region;
        result->requested_target_invalidation = true;
        result->invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_PANE;
        if (command->pane_hit_region == SCENE_EDITOR_PANE_HIT_CONTROLS ||
            command->pane_hit_region == SCENE_EDITOR_PANE_HIT_LIST_PANEL) {
            result->invalidation_class = SCENE_EDITOR_INVALIDATION_TARGET_UI;
            result->invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_PANE_CONTROLS;
        } else if (command->pane_hit_region == SCENE_EDITOR_PANE_HIT_DRAG) {
            result->invalidation_class = SCENE_EDITOR_INVALIDATION_TARGET_INTERACTION;
            result->invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_PANE_DRAG;
        } else {
            result->invalidation_class = SCENE_EDITOR_INVALIDATION_TARGET_PANE;
            result->invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_PANE_CANVAS;
        }
    }
}
