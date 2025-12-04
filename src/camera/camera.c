#include "camera/camera.h"
#include "math/vec2.h"
#include "math/mat3.h"
#include "math/math_utils.h"
#include <math.h>

static const double kDefaultZoom = 1.0;
static const double kDefaultMinPreviewRatio = 0.05;

static Vec2 CameraCenter(const Camera* camera) {
    return vec2(camera ? camera->x : 0.0, camera ? camera->y : 0.0);
}

void CameraInit(Camera* camera, double centerX, double centerY, double zoom) {
    if (!camera) return;
    camera->x = centerX;
    camera->y = centerY;
    camera->zoom = (zoom > 0.0) ? zoom : kDefaultZoom;
    camera->rotation = 0.0;
}

void CameraSetPosition(Camera* camera, double x, double y) {
    if (!camera) return;
    camera->x = x;
    camera->y = y;
}

void CameraPan(Camera* camera, double dx, double dy) {
    if (!camera) return;
    camera->x += dx;
    camera->y += dy;
}

void CameraSetZoom(Camera* camera, double zoom) {
    if (!camera) return;
    if (zoom <= 0.0) zoom = kDefaultZoom;
    camera->zoom = zoom;
}

void CameraZoom(Camera* camera, double zoomDelta, double minZoom, double maxZoom) {
    if (!camera) return;
    double target = camera->zoom + zoomDelta;
    if (minZoom > 0.0 && target < minZoom) target = minZoom;
    if (maxZoom > 0.0 && target > maxZoom) target = maxZoom;
    CameraSetZoom(camera, target);
}

void CameraSetRotation(Camera* camera, double radians) {
    if (!camera) return;
    camera->rotation = radians;
}

void CameraRotate(Camera* camera, double deltaRadians) {
    if (!camera) return;
    camera->rotation += deltaRadians;
}

CameraPoint CameraWorldToScreen(const Camera* camera,
                                double worldX, double worldY,
                                double viewportWidth, double viewportHeight) {
    Vec2 screen = CameraWorldToScreenVec2(camera,
                                          vec2(worldX, worldY),
                                          viewportWidth,
                                          viewportHeight);
    CameraPoint point = {screen.x, screen.y};
    return point;
}

double CameraClampMarginPixels(double marginPixels, double viewportWidth, double viewportHeight) {
    if (viewportWidth <= 0.0 || viewportHeight <= 0.0)
        return 0.0;

    double maxMargin = fmin(viewportWidth, viewportHeight) * 0.45;
    if (maxMargin < 1.0) maxMargin = 1.0;

    if (marginPixels < 0.0) marginPixels = 0.0;
    if (marginPixels > maxMargin) marginPixels = maxMargin;
    return marginPixels;
}

double CameraMarginFromRatio(double ratio, double viewportWidth, double viewportHeight) {
    double margin = fmin(viewportWidth, viewportHeight) * ratio;
    return CameraClampMarginPixels(margin, viewportWidth, viewportHeight);
}

Camera CameraBuildPreviewCamera(const Camera* base,
                                double marginPixels,
                                double viewportWidth,
                                double viewportHeight) {
    Camera preview = *base;
    double clampedMargin = CameraClampMarginPixels(marginPixels, viewportWidth, viewportHeight);

    if (viewportWidth <= 0.0 || viewportHeight <= 0.0)
        return preview;

    double widthRatio = (viewportWidth - 2.0 * clampedMargin) / viewportWidth;
    double heightRatio = (viewportHeight - 2.0 * clampedMargin) / viewportHeight;
    double ratio = fmin(widthRatio, heightRatio);

    if (!isfinite(ratio) || ratio <= kDefaultMinPreviewRatio) ratio = kDefaultMinPreviewRatio;
    if (ratio > 1.0) ratio = 1.0;

    preview.zoom *= ratio;
    return preview;
}

CameraPoint CameraScreenToWorld(const Camera* camera,
                                double screenX, double screenY,
                                double viewportWidth, double viewportHeight) {
    Vec2 world = CameraScreenToWorldVec2(camera,
                                         vec2(screenX, screenY),
                                         viewportWidth,
                                         viewportHeight);
    CameraPoint point = {world.x, world.y};
    return point;
}

Vec2 CameraWorldToScreenVec2(const Camera* camera,
                             Vec2 world,
                             double viewportWidth,
                             double viewportHeight) {
    if (!camera) return world;

    Vec2 center = CameraCenter(camera);
    Vec2 translated = vec2_sub(world, center);
    double c = cos(camera->rotation);
    double s = sin(camera->rotation);
    Vec2 rotated = vec2(translated.x * c - translated.y * s,
                        translated.x * s + translated.y * c);
    Vec2 scaled = vec2_scale(rotated, camera->zoom);
    Vec2 offset = vec2(viewportWidth * 0.5, viewportHeight * 0.5);
    return vec2_add(scaled, offset);
}

Vec2 CameraScreenToWorldVec2(const Camera* camera,
                             Vec2 screen,
                             double viewportWidth,
                             double viewportHeight) {
    if (!camera) return screen;

    Vec2 offset = vec2(viewportWidth * 0.5, viewportHeight * 0.5);
    Vec2 centered = vec2_sub(screen, offset);
    Vec2 unscaled = vec2_scale(centered, (camera->zoom != 0.0) ? (1.0 / camera->zoom) : 1.0);
    double c = cos(-camera->rotation);
    double s = sin(-camera->rotation);
    Vec2 rotated = vec2(unscaled.x * c - unscaled.y * s,
                        unscaled.x * s + unscaled.y * c);
    Vec2 center = CameraCenter(camera);
    return vec2_add(rotated, center);
}
