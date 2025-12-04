#ifndef CAMERA_H
#define CAMERA_H

#include <stdbool.h>
#include "math/vec2.h"

typedef struct {
    double x;       // World-space focus point (center of the view)
    double y;
    double zoom;    // Uniform zoom factor (> 0)
    double rotation; // Radians, camera orientation (0 = upright)
} Camera;

typedef struct {
    double x;
    double y;
} CameraPoint;

void CameraInit(Camera* camera, double centerX, double centerY, double zoom);
void CameraSetPosition(Camera* camera, double x, double y);
void CameraPan(Camera* camera, double dx, double dy);
void CameraSetZoom(Camera* camera, double zoom);
void CameraZoom(Camera* camera, double zoomDelta, double minZoom, double maxZoom);
void CameraSetRotation(Camera* camera, double radians);
void CameraRotate(Camera* camera, double deltaRadians);

CameraPoint CameraWorldToScreen(const Camera* camera,
                                double worldX, double worldY,
                                double viewportWidth, double viewportHeight);

CameraPoint CameraScreenToWorld(const Camera* camera,
                                double screenX, double screenY,
                                double viewportWidth, double viewportHeight);

// Vector variants (for shared math migration)
Vec2 CameraWorldToScreenVec2(const Camera* camera,
                             Vec2 world,
                             double viewportWidth,
                             double viewportHeight);

Vec2 CameraScreenToWorldVec2(const Camera* camera,
                             Vec2 screen,
                             double viewportWidth,
                             double viewportHeight);

double CameraClampMarginPixels(double marginPixels, double viewportWidth, double viewportHeight);
double CameraMarginFromRatio(double ratio, double viewportWidth, double viewportHeight);
Camera CameraBuildPreviewCamera(const Camera* base,
                                double marginPixels,
                                double viewportWidth,
                                double viewportHeight);

#endif // CAMERA_H
