#include "render/space_mode_adapter.h"

#include <math.h>
#include <string.h>

SpaceMode SpaceModeAdapter_ResolveMode(int mode_value) {
    return (SpaceMode)animation_config_space_mode_clamp(mode_value);
}

SpaceModeViewContext SpaceModeAdapter_BuildViewContext(const Camera* camera,
                                                       int viewport_width,
                                                       int viewport_height) {
    return SpaceModeAdapter_BuildViewContextForMode(
        SpaceModeAdapter_ResolveMode(animSettings.spaceMode),
        camera,
        viewport_width,
        viewport_height);
}

SpaceModeViewContext SpaceModeAdapter_BuildViewContextForMode(SpaceMode mode,
                                                              const Camera* camera,
                                                              int viewport_width,
                                                              int viewport_height) {
    SpaceModeViewContext ctx;
    ctx.mode = SpaceModeAdapter_ResolveMode(mode);
    ctx.camera = camera;
    ctx.viewportWidth = viewport_width;
    ctx.viewportHeight = viewport_height;
    return ctx;
}

bool SpaceModeAdapter_Is3D(const SpaceModeViewContext* ctx) {
    if (!ctx) return false;
    return ctx->mode == SPACE_MODE_3D;
}

CameraPoint SpaceModeAdapter_WorldToScreen(const SpaceModeViewContext* ctx,
                                           double world_x,
                                           double world_y) {
    if (!ctx) {
        CameraPoint point = {world_x, world_y};
        return point;
    }

    switch (ctx->mode) {
        case SPACE_MODE_3D:
            // RT-U2 seam: route 3D through adapter now; specialized 3D projection lands in a later slice.
            return CameraWorldToScreen(ctx->camera,
                                       world_x,
                                       world_y,
                                       ctx->viewportWidth,
                                       ctx->viewportHeight);
        case SPACE_MODE_2D:
        default:
            return CameraWorldToScreen(ctx->camera,
                                       world_x,
                                       world_y,
                                       ctx->viewportWidth,
                                       ctx->viewportHeight);
    }
}

CameraPoint SpaceModeAdapter_ScreenToWorld(const SpaceModeViewContext* ctx,
                                           double screen_x,
                                           double screen_y) {
    if (!ctx) {
        CameraPoint point = {screen_x, screen_y};
        return point;
    }

    switch (ctx->mode) {
        case SPACE_MODE_3D:
            // RT-U2 seam: keep behavioral parity with 2D until 3D world contract is introduced.
            return CameraScreenToWorld(ctx->camera,
                                       screen_x,
                                       screen_y,
                                       ctx->viewportWidth,
                                       ctx->viewportHeight);
        case SPACE_MODE_2D:
        default:
            return CameraScreenToWorld(ctx->camera,
                                       screen_x,
                                       screen_y,
                                       ctx->viewportWidth,
                                       ctx->viewportHeight);
    }
}

Ray2D SpaceModeAdapter_MakeRay(double origin_x,
                               double origin_y,
                               double dir_x,
                               double dir_y) {
    Ray2D ray;
    ray.ox = origin_x;
    ray.oy = origin_y;
    ray.dx = dir_x;
    ray.dy = dir_y;
    return ray;
}

Ray2D SpaceModeAdapter_MakeOffsetRay(double origin_x,
                                     double origin_y,
                                     double dir_x,
                                     double dir_y,
                                     double epsilon) {
    Ray2D ray;
    ray.ox = origin_x + dir_x * epsilon;
    ray.oy = origin_y + dir_y * epsilon;
    ray.dx = dir_x;
    ray.dy = dir_y;
    return ray;
}

Ray2D SpaceModeAdapter_PrimaryRayFromScreen(const SpaceModeViewContext* ctx,
                                            double screen_x,
                                            double screen_y,
                                            double origin_x,
                                            double origin_y) {
    CameraPoint world = SpaceModeAdapter_ScreenToWorld(ctx, screen_x, screen_y);
    double dx = world.x - origin_x;
    double dy = world.y - origin_y;
    double length = sqrt(dx * dx + dy * dy);
    if (length > 1e-9) {
        dx /= length;
        dy /= length;
    } else {
        dx = 0.0;
        dy = 1.0;
    }
    return SpaceModeAdapter_MakeRay(origin_x, origin_y, dx, dy);
}

void SpaceModeAdapter_ResetHit(HitInfo2D* hit) {
    if (!hit) return;
    memset(hit, 0, sizeof(*hit));
    hit->objectIndex = -1;
    hit->triangleIndex = -1;
    hit->baryW = 1.0;
}
