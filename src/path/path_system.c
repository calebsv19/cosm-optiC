
#include "path/path_system.h"
#include "config/config_manager.h"
#include "camera/camera.h"
#include <math.h>
#include <stdio.h>
#include <SDL2/SDL.h>

const int basic = 0;

static double ScaleHandle(double value) {
    const double c = 1.0 / 30.0;  // Growth factor
    return (value >= 0) ? (exp(value * c) - 1) : -(exp(-value * c) - 1);
}

// Generalized de Casteljau for single segment interpolation
static Point DeCasteljau(Point* controlPoints, int numPoints, double t) {
    Point tempPoints[numPoints];
    for (int i = 0; i < numPoints; i++)
        tempPoints[i] = controlPoints[i];

    for (int k = 1; k < numPoints; k++) {
        for (int i = 0; i < numPoints - k; i++) {
            tempPoints[i].x = (1 - t) * tempPoints[i].x + t * tempPoints[i + 1].x;
            tempPoints[i].y = (1 - t) * tempPoints[i].y + t * tempPoints[i + 1].y;
        }
    }

    return tempPoints[0];
}

Point GetPositionAlongPath(Path* path, double t) {
    if (!path || path->numPoints < 2) {
        printf("Invalid path data.\n");
        return (Point){0, 0};
    }

    int segmentCount = path->numPoints - 1;
    double segmentT = t * segmentCount;
    int segmentIndex = (int)segmentT;
    double localT = segmentT - segmentIndex;

    if (segmentIndex >= segmentCount) segmentIndex = segmentCount - 1;
    if (segmentIndex < 0) segmentIndex = 0;

    Point p0 = path->points[segmentIndex];
    Point p3 = path->points[segmentIndex + 1];

    // Scale handle offsets non-linearly (existing logic)
    double scaled_vx1 = ScaleHandle(path->handles[segmentIndex][0].vx);
    double scaled_vy1 = ScaleHandle(path->handles[segmentIndex][0].vy);
    double scaled_vx2 = ScaleHandle(path->handles[segmentIndex][1].vx);
    double scaled_vy2 = ScaleHandle(path->handles[segmentIndex][1].vy);

    Point p1 = {p0.x + scaled_vx1, p0.y + scaled_vy1};
    Point p2 = {p3.x + scaled_vx2, p3.y + scaled_vy2};

    Point controlPoints[4];
    int controlPointCount = 0;

    if (path->mode == BEZIER_CUBIC) {
        controlPoints[0] = p0;
        controlPoints[1] = p1;
        controlPoints[2] = p2;
        controlPoints[3] = p3;
        controlPointCount = 4;
    } else {
        controlPoints[0] = p0;
        controlPoints[1] = p1;
        controlPoints[2] = p3;
        controlPointCount = 3;
    }

    return DeCasteljau(controlPoints, controlPointCount, localT);
}


// Frees the memory used by the path (future-proofing)
void DestroyPath(Path* path) {
    if (path) {
        free(path);
    }
}

void ToggleBezierMode(Path* path) {
    if (!path) return;
                
    if (path->mode == BEZIER_QUADRATIC) {
        path->mode = BEZIER_CUBIC;
    } else {
        path->mode = BEZIER_QUADRATIC;
        printf("Switched to Quadratic Bézier Mode.\n");
    }
}  


static SDL_Point ToCameraPoint(double x, double y, const Camera* camera) {
    if (!camera) {
        return (SDL_Point){(int)lround(x), (int)lround(y)};
    }
    CameraPoint mapped = CameraWorldToScreen(camera,
                                             x,
                                             y,
                                             sceneSettings.windowWidth,
                                             sceneSettings.windowHeight);
    return (SDL_Point){(int)lround(mapped.x), (int)lround(mapped.y)};
}

void RenderBezierPathCamera(SDL_Renderer* renderer, Path* path, bool drawHandles, const Camera* camera) {
    if (!path || path->numPoints < 2) return;

    // **Draw Bézier curve as dashed line**
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); // Green for curve
    for (int i = 0; i < path->numPoints - 1; i++) {
        int dashLength = 6;  // Length of each visible dash segment
        // int gapLength = 4; 
        int dashCounter = 0;
        bool drawDash = true;
        
        Point prevWorld = GetPositionAlongPath(path, (double)i / (path->numPoints - 1)); // ✅ Get first curve point
        SDL_Point prevPoint = ToCameraPoint(prevWorld.x, prevWorld.y, camera);

        for (double t = 0.01; t < 1.0; t += 0.01) {  // ✅ Start at t = 0.01 to avoid extra lines
            Point curveWorld = GetPositionAlongPath(path, (i + t) / (path->numPoints - 1));
            SDL_Point curve_point = ToCameraPoint(curveWorld.x, curveWorld.y, camera);

            if (drawDash) {
                SDL_RenderDrawLine(renderer, prevPoint.x, prevPoint.y,
                                   curve_point.x, curve_point.y);
            }
            prevPoint = curve_point;
            dashCounter++;

            if (dashCounter >= dashLength) {
                drawDash = !drawDash; // Toggle drawing on/off
                dashCounter = 0;
            }
        }
    }

    if (drawHandles) {
        SDL_SetRenderDrawColor(renderer, 255, 165, 0, 255); // Orange for handle points
        for (int i = 0; i < path->numPoints - 1; i++) {
            Point handle1w = {path->points[i].x + path->handles[i][0].vx, path->points[i].y + path->handles[i][0].vy};
            Point handle2w = {path->points[i + 1].x + path->handles[i][1].vx, path->points[i + 1].y + path->handles[i][1].vy};

            SDL_Point start1 = ToCameraPoint(path->points[i].x, path->points[i].y, camera);
            SDL_Point start2 = ToCameraPoint(path->points[i + 1].x, path->points[i + 1].y, camera);
            SDL_Point handle1 = ToCameraPoint(handle1w.x, handle1w.y, camera);
            SDL_Point handle2 = ToCameraPoint(handle2w.x, handle2w.y, camera);

            SDL_RenderDrawLine(renderer, start1.x, start1.y, handle1.x, handle1.y);
            SDL_RenderDrawLine(renderer, start2.x, start2.y, handle2.x, handle2.y);

            SDL_RenderDrawPoint(renderer, handle1.x, handle1.y);
            SDL_RenderDrawPoint(renderer, handle2.x, handle2.y);
        }
    }

    // **Draw control points as circles**
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // Red for control points
    for (int i = 0; i < path->numPoints; i++) {
        SDL_Point center = ToCameraPoint(path->points[i].x, path->points[i].y, camera);
        for (int dx = -POINT_RADIUS; dx <= POINT_RADIUS; dx++) {
            for (int dy = -POINT_RADIUS; dy <= POINT_RADIUS; dy++) {
                if (dx * dx + dy * dy <= POINT_RADIUS * POINT_RADIUS) {
                    SDL_RenderDrawPoint(renderer, center.x + dx, center.y + dy);
                }
            }
        }
    }

    // **Draw handle points (only if handles should be drawn)**
    if (drawHandles) {
        SDL_SetRenderDrawColor(renderer, 255, 165, 0, 255); // Orange for handle points
        for (int i = 0; i < path->numPoints - 1; i++) {
            Point handle1w = {path->points[i].x + path->handles[i][0].vx, path->points[i].y + path->handles[i][0].vy};
            Point handle2w = {path->points[i + 1].x + path->handles[i][1].vx, path->points[i + 1].y + path->handles[i][1].vy};
            SDL_Point handle1 = ToCameraPoint(handle1w.x, handle1w.y, camera);
            SDL_Point handle2 = ToCameraPoint(handle2w.x, handle2w.y, camera);

            for (int dx = -POINT_RADIUS; dx <= POINT_RADIUS; dx++) {
                for (int dy = -POINT_RADIUS; dy <= POINT_RADIUS; dy++) {
                    if (dx * dx + dy * dy <= POINT_RADIUS * POINT_RADIUS) {
                        SDL_RenderDrawPoint(renderer, handle1.x + dx, handle1.y + dy);
                        SDL_RenderDrawPoint(renderer, handle2.x + dx, handle2.y + dy);
                    }
                }
            }
        }
    }
}

void RenderBezierPath(SDL_Renderer* renderer, Path* path, bool drawHandles) {
    RenderBezierPathCamera(renderer, path, drawHandles, NULL);
}
