#ifndef PATH_SYSTEM_H
#define PATH_SYSTEM_H

#include <stdlib.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include "camera/camera.h"

#define MAX_BEZIER_POINTS 100  // Define max points
#define POINT_RADIUS 10  // Define global radius for point detection

// Defines a point in 2D space
typedef struct {
    double x, y;
} Point;

// Defines a velocity vector
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
    int numPoints;
    BezierMode mode;
} Path;

// Bézier Path Functions
Point GetPositionAlongPath(Path* path, double t);
Point GetPositionAlongPathNormalized(Path* path, double t);
double PathApproximateLength(Path* path);
void DestroyPath(Path* path);

// Bézier Debug Rendering
void RenderBezierPath(SDL_Renderer* renderer, Path* path, bool drawHandles, SDL_Color curveColor);
void RenderBezierPathCamera(SDL_Renderer* renderer,
                            Path* path,
                            bool drawHandles,
                            const Camera* camera,
                            SDL_Color curveColor);

// Bézier Utility Functions
int IsPointWithinRadius(Point a, Point b);

void ToggleBezierMode(Path* path);

#endif // PATH_SYSTEM_H
