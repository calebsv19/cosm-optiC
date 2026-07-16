#include "editor/scene_editor_viewport3d_bridge.h"

#include <math.h>

static const double SCENE_EDITOR_VIEWPORT3D_HALF_PI =
    1.57079632679489661923132169163975144;

static CoreViewport3DOrientation scene_editor_viewport3d_orientation_from_ray(
    double ray_yaw_rad,
    double ray_pitch_rad) {
    CoreViewport3DOrientation orientation = {
        SCENE_EDITOR_VIEWPORT3D_HALF_PI - ray_yaw_rad,
        -SCENE_EDITOR_VIEWPORT3D_HALF_PI - ray_pitch_rad
    };
    return orientation;
}

static bool scene_editor_viewport3d_build_state(CoreViewport3DVec3d target,
                                                double ray_yaw_rad,
                                                double ray_pitch_rad,
                                                double scale,
                                                double min_scale,
                                                double max_scale,
                                                CoreViewport3DState *out_state) {
    CoreResult result;
    if (!out_state) return false;
    result = core_viewport3d_state_init(
        out_state,
        target,
        scene_editor_viewport3d_orientation_from_ray(ray_yaw_rad, ray_pitch_rad),
        scale,
        min_scale,
        max_scale);
    return result.code == CORE_OK;
}

bool SceneEditorViewport3DBridgeBuildBasis(double ray_yaw_rad,
                                           double ray_pitch_rad,
                                           CoreViewport3DBasis *out_basis) {
    CoreViewport3DOrientation orientation =
        scene_editor_viewport3d_orientation_from_ray(ray_yaw_rad, ray_pitch_rad);
    return core_viewport3d_build_basis(&orientation, out_basis).code == CORE_OK;
}

bool SceneEditorViewport3DBridgeApplyPan(CoreViewport3DVec3d target,
                                        double ray_yaw_rad,
                                        double ray_pitch_rad,
                                        double projector_scale,
                                        double screen_dx,
                                        double screen_dy,
                                        CoreViewport3DVec3d *out_target) {
    CoreViewport3DState state;
    CoreViewport3DState result_state;
    CoreViewport3DCommand command = {0};
    if (!out_target ||
        !scene_editor_viewport3d_build_state(target,
                                             ray_yaw_rad,
                                             ray_pitch_rad,
                                             projector_scale,
                                             projector_scale,
                                             projector_scale,
                                             &state)) {
        return false;
    }
    command.kind = CORE_VIEWPORT3D_COMMAND_PAN;
    command.value.pan.screen_dx = screen_dx;
    command.value.pan.screen_dy = screen_dy;
    if (core_viewport3d_apply(&state, &command, &result_state).code != CORE_OK) {
        return false;
    }
    *out_target = result_state.target;
    return true;
}

bool SceneEditorViewport3DBridgePreserveAnchor(CoreViewport3DVec3d target,
                                               double ray_yaw_rad,
                                               double ray_pitch_rad,
                                               double old_projector_scale,
                                               double new_projector_scale,
                                               double anchor_offset_x,
                                               double anchor_offset_y,
                                               CoreViewport3DVec3d *out_target) {
    CoreViewport3DState state;
    CoreViewport3DState result_state;
    CoreViewport3DCommand command = {0};
    double min_scale = fmin(old_projector_scale, new_projector_scale);
    double max_scale = fmax(old_projector_scale, new_projector_scale);
    if (!out_target ||
        !scene_editor_viewport3d_build_state(target,
                                             ray_yaw_rad,
                                             ray_pitch_rad,
                                             old_projector_scale,
                                             min_scale,
                                             max_scale,
                                             &state)) {
        return false;
    }
    command.kind = CORE_VIEWPORT3D_COMMAND_ZOOM;
    command.value.zoom.factor = new_projector_scale / old_projector_scale;
    command.value.zoom.anchor_offset_x = anchor_offset_x;
    command.value.zoom.anchor_offset_y = anchor_offset_y;
    if (core_viewport3d_apply(&state, &command, &result_state).code != CORE_OK) {
        return false;
    }
    *out_target = result_state.target;
    return true;
}

bool SceneEditorViewport3DBridgeApplyOrbit(double ray_yaw_rad,
                                          double ray_pitch_rad,
                                          double ray_yaw_delta_rad,
                                          double ray_pitch_delta_rad,
                                          double *out_ray_yaw_rad,
                                          double *out_ray_pitch_rad) {
    CoreViewport3DState state;
    CoreViewport3DState result_state;
    CoreViewport3DCommand command = {0};
    if (!out_ray_yaw_rad || !out_ray_pitch_rad ||
        !scene_editor_viewport3d_build_state((CoreViewport3DVec3d){0.0, 0.0, 0.0},
                                             ray_yaw_rad,
                                             ray_pitch_rad,
                                             1.0,
                                             1.0,
                                             1.0,
                                             &state)) {
        return false;
    }
    command.kind = CORE_VIEWPORT3D_COMMAND_ORBIT;
    command.value.orbit.azimuth_delta_rad = -ray_yaw_delta_rad;
    command.value.orbit.elevation_delta_rad = -ray_pitch_delta_rad;
    if (core_viewport3d_apply(&state, &command, &result_state).code != CORE_OK) {
        return false;
    }
    *out_ray_yaw_rad = SCENE_EDITOR_VIEWPORT3D_HALF_PI -
                       result_state.orientation.azimuth_rad;
    *out_ray_pitch_rad = -SCENE_EDITOR_VIEWPORT3D_HALF_PI -
                         result_state.orientation.elevation_rad;
    return isfinite(*out_ray_yaw_rad) && isfinite(*out_ray_pitch_rad);
}

bool SceneEditorViewport3DBridgeApplyFrame(CoreViewport3DVec3d current_target,
                                          double ray_yaw_rad,
                                          double ray_pitch_rad,
                                          double current_scale,
                                          double min_scale,
                                          double max_scale,
                                          CoreViewport3DVec3d frame_target,
                                          double frame_scale,
                                          CoreViewport3DVec3d *out_target,
                                          double *out_scale) {
    CoreViewport3DState state;
    CoreViewport3DState result_state;
    CoreViewport3DCommand command = {0};
    if (!out_target || !out_scale ||
        !scene_editor_viewport3d_build_state(current_target,
                                             ray_yaw_rad,
                                             ray_pitch_rad,
                                             current_scale,
                                             min_scale,
                                             max_scale,
                                             &state)) {
        return false;
    }
    command.kind = CORE_VIEWPORT3D_COMMAND_FRAME;
    command.value.frame.target = frame_target;
    command.value.frame.scale_px_per_world_unit = frame_scale;
    if (core_viewport3d_apply(&state, &command, &result_state).code != CORE_OK) {
        return false;
    }
    *out_target = result_state.target;
    *out_scale = result_state.scale_px_per_world_unit;
    return true;
}

bool SceneEditorViewport3DBridgeApplyResize(CoreViewport3DVec3d target,
                                           double ray_yaw_rad,
                                           double ray_pitch_rad,
                                           double scale,
                                           double min_scale,
                                           double max_scale,
                                           CoreViewport3DVec3d *out_target,
                                           double *out_scale) {
    CoreViewport3DState state;
    CoreViewport3DState result_state;
    CoreViewport3DCommand command = {0};
    if (!out_target || !out_scale ||
        !scene_editor_viewport3d_build_state(target,
                                             ray_yaw_rad,
                                             ray_pitch_rad,
                                             scale,
                                             min_scale,
                                             max_scale,
                                             &state)) {
        return false;
    }
    command.kind = CORE_VIEWPORT3D_COMMAND_RESIZE;
    if (core_viewport3d_apply(&state, &command, &result_state).code != CORE_OK) {
        return false;
    }
    *out_target = result_state.target;
    *out_scale = result_state.scale_px_per_world_unit;
    return true;
}
