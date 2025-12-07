
#include "path/path_system.h"
#include "config/config_manager.h"
#include "camera/camera.h"
#include "math/vec2.h"
#include "math/math_utils.h"
#include <math.h>
#include <stdio.h>
#include <SDL2/SDL.h>

const int basic = 0;

static double ScaleHandle(double value) {
    const double c = 1.0 / 30.0;  // Growth factor
    return (value >= 0) ? (exp(value * c) - 1) : -(exp(-value * c) - 1);
}

static Vec2 PointToVec2(Point p) { return vec2(p.x, p.y); }
static Point Vec2ToPoint(Vec2 v) { return (Point){v.x, v.y}; }
static double LerpAngle(double a, double b, double t) {
    double delta = atan2(sin(b - a), cos(b - a));
    return a + delta * t;
}

// Generalized de Casteljau for single segment interpolation
static Vec2 DeCasteljau(Vec2* controlPoints, int numPoints, double t) {
    Vec2 tempPoints[numPoints];
    for (int i = 0; i < numPoints; i++)
        tempPoints[i] = controlPoints[i];

    for (int k = 1; k < numPoints; k++) {
        for (int i = 0; i < numPoints - k; i++) {
            tempPoints[i] = vec2_lerp(tempPoints[i], tempPoints[i + 1], t);
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

    Vec2 p0 = PointToVec2(path->points[segmentIndex]);
    Vec2 p3 = PointToVec2(path->points[segmentIndex + 1]);

    // Scale handle offsets non-linearly (existing logic)
    double scaled_vx1 = ScaleHandle(path->handles[segmentIndex][0].vx);
    double scaled_vy1 = ScaleHandle(path->handles[segmentIndex][0].vy);
    double scaled_vx2 = ScaleHandle(path->handles[segmentIndex][1].vx);
    double scaled_vy2 = ScaleHandle(path->handles[segmentIndex][1].vy);

    Vec2 p1 = vec2(p0.x + scaled_vx1, p0.y + scaled_vy1);
    Vec2 p2 = vec2(p3.x + scaled_vx2, p3.y + scaled_vy2);

    Vec2 controlPoints[4];
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

    Vec2 result = DeCasteljau(controlPoints, controlPointCount, localT);
    return Vec2ToPoint(result);
}

static double Distance(Point a, Point b) {
    Vec2 va = PointToVec2(a);
    Vec2 vb = PointToVec2(b);
    return vec2_length(vec2_sub(va, vb));
}

static void MapNormalizedT(Path* path, double t, int* outSegment, double* outLocalT) {
    if (!path || path->numPoints < 2) {
        if (outSegment) *outSegment = 0;
        if (outLocalT) *outLocalT = 0.0;
        return;
    }

    double totalLen = PathApproximateLength(path);
    if (totalLen <= 1e-6) {
        if (outSegment) *outSegment = 0;
        if (outLocalT) *outLocalT = clampd(t, 0.0, 1.0);
        return;
    }

    double target = clampd(t, 0.0, 1.0) * totalLen;
    const int samplesPerSegment = 20;
    int segments = path->numPoints - 1;

    double accumulated = 0.0;
    Point prev = path->points[0];

    for (int seg = 0; seg < segments; seg++) {
        for (int s = 1; s <= samplesPerSegment; s++) {
            double localT = (double)s / (double)samplesPerSegment;
            double globalT = ((double)seg + localT) / (double)segments;
            Point p = GetPositionAlongPath(path, globalT);
            double segLen = Distance(prev, p);

            if (accumulated + segLen >= target) {
                double remaining = target - accumulated;
                double frac = (segLen > 1e-6) ? remaining / segLen : 0.0;
                if (outSegment) *outSegment = seg;
                if (outLocalT) *outLocalT = ((double)(s - 1) + frac) / (double)samplesPerSegment;
                return;
            }

            accumulated += segLen;
            prev = p;
        }
    }

    if (outSegment) *outSegment = segments - 1;
    if (outLocalT) *outLocalT = 1.0;
}

double PathApproximateLength(Path* path) {
    if (!path || path->numPoints < 2) return 0.0;
    const int samplesPerSegment = 20;
    int segments = path->numPoints - 1;
    Point prev = path->points[0];
    double length = 0.0;

    for (int seg = 0; seg < segments; seg++) {
        for (int s = 1; s <= samplesPerSegment; s++) {
            double localT = (double)s / (double)samplesPerSegment;
            double globalT = ((double)seg + localT) / (double)segments;
            Point p = GetPositionAlongPath(path, globalT);
            length += Distance(prev, p);
            prev = p;
        }
    }
    return length;
}

Point GetPositionAlongPathNormalized(Path* path, double t) {
    int seg = 0;
    double localT = 0.0;
    MapNormalizedT(path, t, &seg, &localT);

    int segments = (path && path->numPoints >= 2) ? (path->numPoints - 1) : 1;
    double globalT = ((double)seg + localT) / (double)segments;
    return GetPositionAlongPath(path, globalT);
}

double GetRotationAlongPathNormalized(Path* path, double t) {
    if (!path || path->numPoints < 1) return 0.0;
    int seg = 0;
    double localT = 0.0;
    MapNormalizedT(path, t, &seg, &localT);
    int next = (seg + 1 < path->numPoints) ? (seg + 1) : seg;
    double a0 = path->rotations[seg];
    double a1 = path->rotations[next];
    return LerpAngle(a0, a1, localT);
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

static SDL_Color UseColor(SDL_Color color, SDL_Color fallback) {
    if (color.r == 0 && color.g == 0 && color.b == 0 && color.a == 0) {
        return fallback;
    }
    return color;
}

static int ResolveRadius(int radius) {
    if (radius <= 0) return POINT_VIS_RADIUS;
    return radius;
}

void RenderBezierPathCameraStyled(SDL_Renderer* renderer,
                                  Path* path,
                                  bool drawHandles,
                                  const Camera* camera,
                                  SDL_Color curveColor,
                                  SDL_Color handleColor,
                                  int selectedIndex,
                                  SDL_Color selectedColor,
                                  int pointRadius) {
    if (!path || path->numPoints < 2) return;

    const int radius = ResolveRadius(pointRadius);
    const int handleRadius = HANDLE_VIS_RADIUS;

    SDL_Color curve = UseColor(curveColor, (SDL_Color){0, 255, 0, 255});
    SDL_Color handles = UseColor(handleColor, (SDL_Color){255, 165, 0, 255});
    SDL_Color selCol = UseColor(selectedColor, (SDL_Color){255, 255, 120, 255});

    // **Draw Bézier curve as dashed line**
    SDL_SetRenderDrawColor(renderer, curve.r, curve.g, curve.b, curve.a); // Curve color
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
        SDL_SetRenderDrawColor(renderer, handles.r, handles.g, handles.b, handles.a); // Handle points
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
    SDL_Color startCol = {0, 200, 0, 220};
    SDL_Color endCol = {220, 40, 40, 220};

    // Control points (with selection highlight)
    for (int i = 0; i < path->numPoints; i++) {
        SDL_Color pointColor = curve;
        if (i == selectedIndex) {
            pointColor = selCol;
        } else if (i == 0) {
            pointColor = startCol;
        } else if (i == path->numPoints - 1) {
            pointColor = endCol;
        }
        SDL_SetRenderDrawColor(renderer, pointColor.r, pointColor.g, pointColor.b, pointColor.a);
        SDL_Point center = ToCameraPoint(path->points[i].x, path->points[i].y, camera);
        for (int dx = -radius; dx <= radius; dx++) {
            for (int dy = -radius; dy <= radius; dy++) {
                if (dx * dx + dy * dy <= radius * radius) {
                    SDL_RenderDrawPoint(renderer, center.x + dx, center.y + dy);
                }
            }
        }
    }

    // **Draw handle points (only if handles should be drawn)**
    if (drawHandles) {
        SDL_SetRenderDrawColor(renderer, handles.r, handles.g, handles.b, handles.a); // Handle points
        for (int i = 0; i < path->numPoints - 1; i++) {
            Point handle1w = {path->points[i].x + path->handles[i][0].vx, path->points[i].y + path->handles[i][0].vy};
            Point handle2w = {path->points[i + 1].x + path->handles[i][1].vx, path->points[i + 1].y + path->handles[i][1].vy};
            SDL_Point handle1 = ToCameraPoint(handle1w.x, handle1w.y, camera);
            SDL_Point handle2 = ToCameraPoint(handle2w.x, handle2w.y, camera);

            for (int dx = -handleRadius; dx <= handleRadius; dx++) {
                for (int dy = -handleRadius; dy <= handleRadius; dy++) {
                    if (dx * dx + dy * dy <= handleRadius * handleRadius) {
                        SDL_RenderDrawPoint(renderer, handle1.x + dx, handle1.y + dy);
                        SDL_RenderDrawPoint(renderer, handle2.x + dx, handle2.y + dy);
                    }
                }
            }
        }
    }
}

void RenderBezierPathCamera(SDL_Renderer* renderer,
                            Path* path,
                            bool drawHandles,
                            const Camera* camera,
                            SDL_Color curveColor,
                            SDL_Color handleColor,
                            int selectedIndex,
                            SDL_Color selectedColor) {
    RenderBezierPathCameraStyled(renderer,
                                 path,
                                 drawHandles,
                                 camera,
                                 curveColor,
                                 handleColor,
                                 selectedIndex,
                                 selectedColor,
                                 POINT_VIS_RADIUS);
}

void RenderBezierPath(SDL_Renderer* renderer,
                      Path* path,
                      bool drawHandles,
                      SDL_Color curveColor,
                      SDL_Color handleColor,
                      int selectedIndex,
                      SDL_Color selectedColor) {
    RenderBezierPathCameraStyled(renderer,
                                 path,
                                 drawHandles,
                                 NULL,
                                 curveColor,
                                 handleColor,
                                 selectedIndex,
                                 selectedColor,
                                 POINT_VIS_RADIUS);
}
