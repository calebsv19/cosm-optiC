#include "camera/camera.h"
#include <math.h>

static const double kDefaultZoom = 1.0;
static const double kDefaultMinPreviewRatio = 0.05;

void CameraInit(Camera* camera, double centerX, double centerY, double zoom) {
    if (!camera) return;
    camera->x = centerX;
    camera->y = centerY;
    camera->zoom = (zoom > 0.0) ? zoom : kDefaultZoom;
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

CameraPoint CameraWorldToScreen(const Camera* camera,
                                double worldX, double worldY,
                                double viewportWidth, double viewportHeight) {
    CameraPoint point = {worldX, worldY};
    if (!camera) {
        return point;
    }

    double dx = (worldX - camera->x) * camera->zoom;
    double dy = (worldY - camera->y) * camera->zoom;

    point.x = viewportWidth * 0.5 + dx;
    point.y = viewportHeight * 0.5 + dy;
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
    CameraPoint point = {screenX, screenY};
    if (!camera) {
        return point;
    }

    double dx = screenX - viewportWidth * 0.5;
    double dy = screenY - viewportHeight * 0.5;

    point.x = camera->x + (dx / camera->zoom);
    point.y = camera->y + (dy / camera->zoom);
    return point;
}
