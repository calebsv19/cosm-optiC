#ifndef PATH_SYSTEM_H
#define PATH_SYSTEM_H

#include <stdlib.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include "camera/camera.h"

#define MAX_BEZIER_POINTS 100  // Define max points
#define POINT_VIS_RADIUS 5     // Draw radius for control points
#define HANDLE_VIS_RADIUS 4    // Draw radius for velocity handles
#define POINT_HIT_RADIUS 8     // Hit radius for points/handles (screen space)

#include "math/vec2.h"

// Defines a point in 2D space (alias to vec2 for compatibility)
typedef struct {
    double x, y;
} Point;

// Defines a velocity vector (alias to vec2 for compatibility)
typedef struct {
    double vx, vy;
} Velocity;

typedef enum {
    BEZIER_QUADRATIC,
    BEZIER_CUBIC
} BezierMode;

static const char *BEZIER_MODE_STRINGS[] = {
    "Quadratic",
    "Cubic"
};


typedef struct {
    Point points[MAX_BEZIER_POINTS];  // Stores all control points
    Velocity handles[MAX_BEZIER_POINTS][2];  // Stores outgoing & incoming handles per segment
    bool handleLink[MAX_BEZIER_POINTS]; // Mirror handles around a point when true
    double rotations[MAX_BEZIER_POINTS];     // Orientation per point (radians)
    bool rotationSet[MAX_BEZIER_POINTS];     // Whether rotation is explicitly authored
    int numPoints;
    BezierMode mode;
} Path;

// Bézier Path Functions
Point GetPositionAlongPath(Path* path, double t);
Point GetPositionAlongPathNormalized(Path* path, double t);
double GetRotationAlongPathNormalized(Path* path, double t);
double PathApproximateLength(Path* path);
void DestroyPath(Path* path);

// Bézier Debug Rendering
void RenderBezierPath(SDL_Renderer* renderer,
                      Path* path,
                      bool drawHandles,
                      SDL_Color curveColor,
                      SDL_Color handleColor,
                      int selectedIndex,
                      SDL_Color selectedColor);
void RenderBezierPathCameraStyled(SDL_Renderer* renderer,
                                  Path* path,
                                  bool drawHandles,
                                  const Camera* camera,
                                  SDL_Color curveColor,
                                  SDL_Color handleColor,
                                  int selectedIndex,
                                  SDL_Color selectedColor,
                                  int pointRadius);
void RenderBezierPathCamera(SDL_Renderer* renderer,
                            Path* path,
                            bool drawHandles,
                            const Camera* camera,
                            SDL_Color curveColor,
                            SDL_Color handleColor,
                            int selectedIndex,
                            SDL_Color selectedColor);

// Bézier Utility Functions
int IsPointWithinRadius(Point a, Point b);

void ToggleBezierMode(Path* path);

#endif // PATH_SYSTEM_H
