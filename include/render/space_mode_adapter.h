#ifndef RENDER_SPACE_MODE_ADAPTER_H
#define RENDER_SPACE_MODE_ADAPTER_H

#include <stdbool.h>

#include "camera/camera.h"
#include "config/config_manager.h"
#include "render/ray_types.h"

typedef struct {
    SpaceMode mode;
    const Camera* camera;
    int viewportWidth;
    int viewportHeight;
} SpaceModeViewContext;

SpaceMode SpaceModeAdapter_ResolveMode(int mode_value);
SpaceModeViewContext SpaceModeAdapter_BuildViewContext(const Camera* camera,
                                                       int viewport_width,
                                                       int viewport_height);
SpaceModeViewContext SpaceModeAdapter_BuildViewContextForMode(SpaceMode mode,
                                                              const Camera* camera,
                                                              int viewport_width,
                                                              int viewport_height);
bool SpaceModeAdapter_Is3D(const SpaceModeViewContext* ctx);

CameraPoint SpaceModeAdapter_WorldToScreen(const SpaceModeViewContext* ctx,
                                           double world_x,
                                           double world_y);
CameraPoint SpaceModeAdapter_ScreenToWorld(const SpaceModeViewContext* ctx,
                                           double screen_x,
                                           double screen_y);

Ray2D SpaceModeAdapter_MakeRay(double origin_x,
                               double origin_y,
                               double dir_x,
                               double dir_y);
Ray2D SpaceModeAdapter_MakeOffsetRay(double origin_x,
                                     double origin_y,
                                     double dir_x,
                                     double dir_y,
                                     double epsilon);
Ray2D SpaceModeAdapter_PrimaryRayFromScreen(const SpaceModeViewContext* ctx,
                                            double screen_x,
                                            double screen_y,
                                            double origin_x,
                                            double origin_y);
void SpaceModeAdapter_ResetHit(HitInfo2D* hit);

#endif
