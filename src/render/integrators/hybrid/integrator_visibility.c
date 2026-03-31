#include "render/integrators/hybrid/integrator_visibility.h"
#include "render/space_mode_adapter.h"
#include <math.h>

#ifndef PATH_EPSILON
#define PATH_EPSILON 0.0001
#endif

static inline void normalize(double* x, double* y)
{
    double len = sqrt((*x) * (*x) + (*y) * (*y));
    if (len > 1e-12) {
        *x /= len;
        *y /= len;
    }
}

bool IsOccluded(const UniformGrid* grid,
                double originX, double originY,
                double dirX, double dirY,
                double maxDist)
{
    if (!grid) return false;

    Ray2D ray = SpaceModeAdapter_MakeRay(originX, originY, dirX, dirY);
    HitInfo2D tmp;
    SpaceModeAdapter_ResetHit(&tmp);

    return UniformGridTraceRay(grid, &ray, PATH_EPSILON, maxDist, &tmp);
}

bool TraceRayToSurface(const UniformGrid* grid,
                       double worldX, double worldY,
                       double dirX, double dirY,
                       HitInfo2D* outHit,
                       const SceneObject** outObj,
                       double maxDist)
{
    if (!grid) return false;

    Ray2D ray = SpaceModeAdapter_MakeOffsetRay(worldX, worldY, dirX, dirY, PATH_EPSILON);
    HitInfo2D hit;
    SpaceModeAdapter_ResetHit(&hit);
    if (!UniformGridTraceRay(grid, &ray, PATH_EPSILON, maxDist, &hit))
        return false;

    if (outHit) *outHit = hit;

    if (outObj && hit.objectIndex >= 0)
        *outObj = NULL; /* Actual object resolution occurs upstream */

    return true;
}

bool HasDirectLineOfSight(const UniformGrid* grid,
                          double x, double y,
                          const LightSource* light)
{
    if (!grid || !light) return false;

    double dx = light->x - x;
    double dy = light->y - y;
    double dist = sqrt(dx*dx + dy*dy);

    if (dist <= PATH_EPSILON)
        return true;

    normalize(&dx, &dy);

    double ox = x + dx * PATH_EPSILON;
    double oy = y + dy * PATH_EPSILON;
    double maxD = fmax(0.0, dist - 2.0 * PATH_EPSILON);

    return !IsOccluded(grid, ox, oy, dx, dy, maxD);
}

bool SegmentFacesPoint(double x0, double y0,
                       double x1, double y1,
                       double nx, double ny,
                       double px, double py)
{
    double cx = 0.5 * (x0 + x1);
    double cy = 0.5 * (y0 + y1);

    double vx = px - cx;
    double vy = py - cy;

    double dot = vx * nx + vy * ny;

    return dot > -0.05; /* relaxed threshold for smoother GI */
}
