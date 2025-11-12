#ifndef RENDER_SURFACE_MESH_H
#define RENDER_SURFACE_MESH_H

#include "scene/object_manager.h"

typedef struct {
    double x0;
    double y0;
    double x1;
    double y1;
    double nx;
    double ny;
    double length;
    int objectIndex;
    int localIndex;
} SurfaceSegment;

typedef struct {
    SurfaceSegment* segments;
    int segmentCount;
    int capacity;
    double maxSegmentLength;
    int* objectOffsets;
    int objectCount;
    int offsetsCapacity;
} SurfaceMesh;

void SurfaceMeshInit(SurfaceMesh* mesh);
void SurfaceMeshFree(SurfaceMesh* mesh);
bool SurfaceMeshBuild(SurfaceMesh* mesh,
                      SceneObject* objects,
                      int objectCount,
                      double maxSegmentLength);

const SurfaceSegment* SurfaceMeshFindSegment(const SurfaceMesh* mesh,
                                             int objectIndex,
                                             double px,
                                             double py,
                                             double nx,
                                             double ny);

#endif
