#ifndef SCENE_EDITOR_VIEWPORT3D_BRIDGE_H
#define SCENE_EDITOR_VIEWPORT3D_BRIDGE_H

#include <stdbool.h>

#include "core_viewport3d.h"

bool SceneEditorViewport3DBridgeBuildBasis(double ray_yaw_rad,
                                           double ray_pitch_rad,
                                           CoreViewport3DBasis *out_basis);
bool SceneEditorViewport3DBridgeApplyPan(CoreViewport3DVec3d target,
                                        double ray_yaw_rad,
                                        double ray_pitch_rad,
                                        double projector_scale,
                                        double screen_dx,
                                        double screen_dy,
                                        CoreViewport3DVec3d *out_target);
bool SceneEditorViewport3DBridgePreserveAnchor(CoreViewport3DVec3d target,
                                               double ray_yaw_rad,
                                               double ray_pitch_rad,
                                               double old_projector_scale,
                                               double new_projector_scale,
                                               double anchor_offset_x,
                                               double anchor_offset_y,
                                               CoreViewport3DVec3d *out_target);
bool SceneEditorViewport3DBridgeApplyOrbit(double ray_yaw_rad,
                                          double ray_pitch_rad,
                                          double ray_yaw_delta_rad,
                                          double ray_pitch_delta_rad,
                                          double *out_ray_yaw_rad,
                                          double *out_ray_pitch_rad);
bool SceneEditorViewport3DBridgeApplyFrame(CoreViewport3DVec3d current_target,
                                          double ray_yaw_rad,
                                          double ray_pitch_rad,
                                          double current_scale,
                                          double min_scale,
                                          double max_scale,
                                          CoreViewport3DVec3d frame_target,
                                          double frame_scale,
                                          CoreViewport3DVec3d *out_target,
                                          double *out_scale);
bool SceneEditorViewport3DBridgeApplyResize(CoreViewport3DVec3d target,
                                           double ray_yaw_rad,
                                           double ray_pitch_rad,
                                           double scale,
                                           double min_scale,
                                           double max_scale,
                                           CoreViewport3DVec3d *out_target,
                                           double *out_scale);

#endif
