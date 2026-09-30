#ifndef MOTION_ROUTE_GEOMETRY_H
#define MOTION_ROUTE_GEOMETRY_H
#include "motion/scene_motion_paths.h"
#define MOTION_ROUTE_LEAF_CAPACITY 32768
/* New planned-motion analysis only. Legacy runtime arc sampling is unchanged. */
typedef struct MotionRouteLeaf {
    double begin, end; /* segment + local cubic parameter */
    double distance, length, error, curvature_bound;
} MotionRouteLeaf;
typedef struct MotionRouteGeometry {
    MotionPath path; /* World-scaled copy. */
    size_t count;
    MotionRouteLeaf leaves[MOTION_ROUTE_LEAF_CAPACITY];
    double length, length_error, curvature_bound;
    double point_distances[MOTION_POINT_CAPACITY];
    double regular_curvature_bound, stop_curvature_factor;
    bool required_stop[MOTION_POINT_CAPACITY];
    int curvature_stop_point[MOTION_POINT_CAPACITY-1]; /* -1 or required rest origin */
    bool stopping_certificate; /* Every singular leaf covered by a stop bound. */
    bool corner[MOTION_POINT_CAPACITY];
    bool singular; /* Infinite curvature bound: not a certified timing plan. */
} MotionRouteGeometry;
typedef enum MotionRouteStatus {
    MOTION_ROUTE_OK, MOTION_ROUTE_INVALID, MOTION_ROUTE_CAPACITY,
    MOTION_ROUTE_NUMERIC
} MotionRouteStatus;
/* Output unchanged on failure. Curvature may be infinite at singular tangents;
 * callers must not claim bounded acceleration from such an analysis. */
MotionRouteStatus MotionRouteGeometryBuild(const MotionPath *,double world_scale,MotionRouteGeometry *);
/* Exact cubic position/derivatives in parameter coordinates, no arc inversion. */
bool MotionRouteGeometryParameter(const MotionRouteGeometry *,double parameter,double position[3],double first[3],double second[3]);
typedef struct MotionRouteFrame {
    double position[3], tangent[3], curvature[3], parameter;
    bool regular; /* At a singular rest endpoint the curvature is undefined. */
} MotionRouteFrame;
bool MotionRouteGeometryDistanceSide(const MotionRouteGeometry *,double distance,bool left,MotionRouteFrame *);
bool MotionRouteGeometryDistance(const MotionRouteGeometry *,double distance,MotionRouteFrame *);
#endif
