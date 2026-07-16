#ifndef SCENE_EDITOR_VIEWPORT_NAV_MATH_H
#define SCENE_EDITOR_VIEWPORT_NAV_MATH_H

#include <math.h>
#include <stdbool.h>

typedef struct SceneEditorViewportNavVec3 {
    double x;
    double y;
    double z;
} SceneEditorViewportNavVec3;

typedef struct SceneEditorViewportNavBasis {
    SceneEditorViewportNavVec3 right;
    SceneEditorViewportNavVec3 screen_down;
    SceneEditorViewportNavVec3 forward;
} SceneEditorViewportNavBasis;

static inline bool SceneEditorViewportNavMathVec3IsFinite(SceneEditorViewportNavVec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static inline bool SceneEditorViewportNavMathBuildBasis(double yaw_rad,
                                                        double pitch_rad,
                                                        SceneEditorViewportNavBasis* out_basis) {
    SceneEditorViewportNavBasis candidate = {0};
    double cos_yaw = 0.0;
    double sin_yaw = 0.0;
    double cos_pitch = 0.0;
    double sin_pitch = 0.0;
    if (!out_basis || !isfinite(yaw_rad) || !isfinite(pitch_rad)) return false;
    cos_yaw = cos(yaw_rad);
    sin_yaw = sin(yaw_rad);
    cos_pitch = cos(pitch_rad);
    sin_pitch = sin(pitch_rad);
    candidate.right = (SceneEditorViewportNavVec3){cos_yaw, -sin_yaw, 0.0};
    candidate.screen_down = (SceneEditorViewportNavVec3){sin_yaw * cos_pitch,
                                                         cos_yaw * cos_pitch,
                                                         -sin_pitch};
    candidate.forward = (SceneEditorViewportNavVec3){-sin_yaw * sin_pitch,
                                                     -cos_yaw * sin_pitch,
                                                     -cos_pitch};
    if (!SceneEditorViewportNavMathVec3IsFinite(candidate.right) ||
        !SceneEditorViewportNavMathVec3IsFinite(candidate.screen_down) ||
        !SceneEditorViewportNavMathVec3IsFinite(candidate.forward)) {
        return false;
    }
    *out_basis = candidate;
    return true;
}

static inline bool SceneEditorViewportNavMathPanTarget(
    const SceneEditorViewportNavVec3* target,
    const SceneEditorViewportNavBasis* basis,
    double projector_scale,
    double screen_dx,
    double screen_dy,
    SceneEditorViewportNavVec3* out_target) {
    SceneEditorViewportNavVec3 candidate = {0};
    double world_dx = 0.0;
    double world_dy = 0.0;
    if (!target || !basis || !out_target ||
        !SceneEditorViewportNavMathVec3IsFinite(*target) ||
        !SceneEditorViewportNavMathVec3IsFinite(basis->right) ||
        !SceneEditorViewportNavMathVec3IsFinite(basis->screen_down) ||
        !isfinite(projector_scale) || projector_scale <= 1e-9 ||
        !isfinite(screen_dx) || !isfinite(screen_dy)) {
        return false;
    }
    world_dx = -screen_dx / projector_scale;
    world_dy = -screen_dy / projector_scale;
    candidate.x = target->x + basis->right.x * world_dx + basis->screen_down.x * world_dy;
    candidate.y = target->y + basis->right.y * world_dx + basis->screen_down.y * world_dy;
    candidate.z = target->z + basis->right.z * world_dx + basis->screen_down.z * world_dy;
    if (!SceneEditorViewportNavMathVec3IsFinite(candidate)) return false;
    *out_target = candidate;
    return true;
}

static inline bool SceneEditorViewportNavMathPreserveScreenAnchor(
    const SceneEditorViewportNavVec3* target,
    const SceneEditorViewportNavBasis* basis,
    double old_scale,
    double new_scale,
    double screen_offset_x,
    double screen_offset_y,
    SceneEditorViewportNavVec3* out_target) {
    SceneEditorViewportNavVec3 candidate = {0};
    double scale_delta = 0.0;
    if (!target || !basis || !out_target ||
        !SceneEditorViewportNavMathVec3IsFinite(*target) ||
        !SceneEditorViewportNavMathVec3IsFinite(basis->right) ||
        !SceneEditorViewportNavMathVec3IsFinite(basis->screen_down) ||
        !isfinite(old_scale) || old_scale <= 1e-9 ||
        !isfinite(new_scale) || new_scale <= 1e-9 ||
        !isfinite(screen_offset_x) || !isfinite(screen_offset_y)) {
        return false;
    }
    scale_delta = (1.0 / old_scale) - (1.0 / new_scale);
    candidate.x = target->x +
                  basis->right.x * screen_offset_x * scale_delta +
                  basis->screen_down.x * screen_offset_y * scale_delta;
    candidate.y = target->y +
                  basis->right.y * screen_offset_x * scale_delta +
                  basis->screen_down.y * screen_offset_y * scale_delta;
    candidate.z = target->z +
                  basis->right.z * screen_offset_x * scale_delta +
                  basis->screen_down.z * screen_offset_y * scale_delta;
    if (!SceneEditorViewportNavMathVec3IsFinite(candidate)) return false;
    *out_target = candidate;
    return true;
}

#endif
