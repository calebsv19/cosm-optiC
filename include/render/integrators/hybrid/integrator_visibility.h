#ifndef INTEGRATOR_VISIBILITY_H
#define INTEGRATOR_VISIBILITY_H

#include <stdbool.h>
#include "render/integrator_common.h"

#ifdef __cplusplus
extern "C" {
#endif

bool IsOccluded(const UniformGrid* grid,
                double originX, double originY,
                double dirX, double dirY,
                double maxDist);

bool TraceRayToSurface(const UniformGrid* grid,
                       double worldX, double worldY,
                       double dirX, double dirY,
                       HitInfo2D* outHit,
                       const SceneObject** outObj,
                       double maxDist);

bool HasDirectLineOfSight(const UniformGrid* grid,
                          double x, double y,
                          const LightSource* light);

bool SegmentFacesPoint(double x0, double y0,
                       double x1, double y1,
                       double nx, double ny,
                       double px, double py);

#ifdef __cplusplus
}
#endif

#endif
