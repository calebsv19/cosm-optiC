
#include "path/path_system.h"
#include "config/config_manager.h"
#include "camera/camera.h"
#include "math/vec2.h"
#include "math/math_utils.h"
#include <math.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#if USE_VULKAN
#include "vk_renderer.h"
#endif

const int basic = 0;

static double ScaleHandle(double value) {
    const double c = 1.0 / 30.0;  // Growth factor
    return (value >= 0) ? (exp(value * c) - 1) : -(exp(-value * c) - 1);
}

static double ResolveHandleForPathEvaluation(double value) {
    /* Controlled-3D authoring uses scene-relative world units directly. */
    if (animSettings.spaceMode == SPACE_MODE_3D) {
        return value;
    }
    return ScaleHandle(value);
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
    double clamped_t = 0.0;
    int segmentCount = 0;
    double segmentT = 0.0;
    int segmentIndex = 0;
    double localT = 0.0;
    if (!path || path->numPoints < 2) {
        printf("Invalid path data.\n");
        return (Point){0, 0};
    }

    segmentCount = path->numPoints - 1;
    clamped_t = clampd(t, 0.0, 1.0);
    if (clamped_t >= 1.0) {
        segmentIndex = segmentCount - 1;
        localT = 1.0;
    } else {
        segmentT = clamped_t * segmentCount;
        segmentIndex = (int)segmentT;
        localT = segmentT - segmentIndex;
        if (segmentIndex >= segmentCount) segmentIndex = segmentCount - 1;
        if (segmentIndex < 0) segmentIndex = 0;
    }

    Vec2 p0 = PointToVec2(path->points[segmentIndex]);
    Vec2 p3 = PointToVec2(path->points[segmentIndex + 1]);

    // Legacy 2D keeps compressed handle response; controlled-3D uses authored units directly.
    double scaled_vx1 = ResolveHandleForPathEvaluation(path->handles[segmentIndex][0].vx);
    double scaled_vy1 = ResolveHandleForPathEvaluation(path->handles[segmentIndex][0].vy);
    double scaled_vx2 = ResolveHandleForPathEvaluation(path->handles[segmentIndex][1].vx);
    double scaled_vy2 = ResolveHandleForPathEvaluation(path->handles[segmentIndex][1].vy);

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

Point GetPositionAlongPathNormalized(Path* path, double t) {
    int seg = 0;
    double localT = 0.0;
    PathMapNormalizedT(path, t, &seg, &localT);

    int segments = (path && path->numPoints >= 2) ? (path->numPoints - 1) : 1;
    double globalT = ((double)seg + localT) / (double)segments;
    return GetPositionAlongPath(path, globalT);
}

double GetRotationAlongPathNormalized(Path* path, double t) {
    if (!path || path->numPoints < 1) return 0.0;
    int seg = 0;
    double localT = 0.0;
    PathMapNormalizedT(path, t, &seg, &localT);
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

static void DrawFilledCircle(SDL_Renderer* renderer, SDL_Point center, int radius) {
    for (int dx = -radius; dx <= radius; dx++) {
        for (int dy = -radius; dy <= radius; dy++) {
            if (dx * dx + dy * dy <= radius * radius) {
                SDL_RenderDrawPoint(renderer, center.x + dx, center.y + dy);
            }
        }
    }
}

static void DrawBezierCurveCamera(SDL_Renderer* renderer,
                                  Path* path,
                                  const Camera* camera,
                                  SDL_Color curve) {
    SDL_SetRenderDrawColor(renderer, curve.r, curve.g, curve.b, curve.a);
    for (int i = 0; i < path->numPoints - 1; i++) {
        int dashLength = 6;
        int dashCounter = 0;
        bool drawDash = true;

        Point prevWorld = GetPositionAlongPath(path, (double)i / (path->numPoints - 1));
        SDL_Point prevPoint = ToCameraPoint(prevWorld.x, prevWorld.y, camera);

        for (double t = 0.01; t < 1.0; t += 0.01) {
            Point curveWorld = GetPositionAlongPath(path, (i + t) / (path->numPoints - 1));
            SDL_Point curvePoint = ToCameraPoint(curveWorld.x, curveWorld.y, camera);

            if (drawDash) {
                SDL_RenderDrawLine(renderer, prevPoint.x, prevPoint.y,
                                   curvePoint.x, curvePoint.y);
            }
            prevPoint = curvePoint;
            dashCounter++;

            if (dashCounter >= dashLength) {
                drawDash = !drawDash;
                dashCounter = 0;
            }
        }
    }
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
#if USE_VULKAN
    static int s_bezier_debug_logs = 0;
    uint32_t start_calls = 0;
    if (s_bezier_debug_logs < 6 && renderer) {
        VkRenderer* vk_renderer = (VkRenderer*)renderer;
        start_calls = vk_renderer->draw_state.draw_call_count;
        SDL_Point p0 = ToCameraPoint(path->points[0].x, path->points[0].y, camera);
        SDL_Point p1 = ToCameraPoint(path->points[path->numPoints - 1].x,
                                     path->points[path->numPoints - 1].y,
                                     camera);
        fprintf(stderr,
                "[bezier] points=%d handles=%d sel=%d start=(%d,%d) end=(%d,%d)\n",
                path->numPoints,
                drawHandles ? 1 : 0,
                selectedIndex,
                p0.x, p0.y,
                p1.x, p1.y);
    }
#endif

    const int radius = ResolveRadius(pointRadius);
    const int handleRadius = HANDLE_VIS_RADIUS;

    SDL_Color curve = UseColor(curveColor, (SDL_Color){0, 255, 0, 255});
    SDL_Color handles = UseColor(handleColor, (SDL_Color){255, 165, 0, 255});
    SDL_Color selCol = UseColor(selectedColor, (SDL_Color){255, 255, 120, 255});

    DrawBezierCurveCamera(renderer, path, camera, curve);

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

    SDL_Color startCol = {0, 200, 0, 220};
    SDL_Color endCol = {220, 40, 40, 220};

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
        DrawFilledCircle(renderer, center, radius);
    }

    if (drawHandles) {
        SDL_SetRenderDrawColor(renderer, handles.r, handles.g, handles.b, handles.a); // Handle points
        for (int i = 0; i < path->numPoints - 1; i++) {
            Point handle1w = {path->points[i].x + path->handles[i][0].vx, path->points[i].y + path->handles[i][0].vy};
            Point handle2w = {path->points[i + 1].x + path->handles[i][1].vx, path->points[i + 1].y + path->handles[i][1].vy};
            SDL_Point handle1 = ToCameraPoint(handle1w.x, handle1w.y, camera);
            SDL_Point handle2 = ToCameraPoint(handle2w.x, handle2w.y, camera);

            DrawFilledCircle(renderer, handle1, handleRadius);
            DrawFilledCircle(renderer, handle2, handleRadius);
        }
    }

#if USE_VULKAN
    if (s_bezier_debug_logs < 6 && renderer) {
        VkRenderer* vk_renderer = (VkRenderer*)renderer;
        uint32_t end_calls = vk_renderer->draw_state.draw_call_count;
        fprintf(stderr, "[bezier] draw calls delta=%u\n", (unsigned)(end_calls - start_calls));
        s_bezier_debug_logs++;
    }
#endif
}

void RenderBezierPathCameraPassive(SDL_Renderer* renderer,
                                   Path* path,
                                   const Camera* camera,
                                   SDL_Color curveColor,
                                   int endpointRadius) {
    SDL_Color curve = {0};
    SDL_Color startCol = {0, 200, 0, 220};
    SDL_Color endCol = {220, 40, 40, 220};
    SDL_Point start = {0};
    SDL_Point end = {0};
    int radius = 0;

    if (!path || path->numPoints < 2) return;

    curve = UseColor(curveColor, (SDL_Color){0, 255, 0, 255});
    radius = ResolveRadius(endpointRadius);

    DrawBezierCurveCamera(renderer, path, camera, curve);

    start = ToCameraPoint(path->points[0].x, path->points[0].y, camera);
    end = ToCameraPoint(path->points[path->numPoints - 1].x,
                        path->points[path->numPoints - 1].y,
                        camera);

    SDL_SetRenderDrawColor(renderer, startCol.r, startCol.g, startCol.b, startCol.a);
    DrawFilledCircle(renderer, start, radius);
    SDL_SetRenderDrawColor(renderer, endCol.r, endCol.g, endCol.b, endCol.a);
    DrawFilledCircle(renderer, end, radius);
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
