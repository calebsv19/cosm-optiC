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

typedef struct {
    double x;
    double y;
    double nx;
    double ny;
} TriangleVertex;

typedef struct {
    int v0;
    int v1;
    int v2;
    int objectIndex;
} TriangleFace;

typedef struct {
    TriangleVertex* vertices;
    int vertexCount;
    int vertexCapacity;
    TriangleFace* triangles;
    int triangleCount;
    int triangleCapacity;
    int* vertexOffsets;
    int* triangleOffsets;
    int objectCount;
    int offsetsCapacity;
} TriangleMesh;

void SurfaceMeshInit(SurfaceMesh* mesh);
void SurfaceMeshFree(SurfaceMesh* mesh);
bool SurfaceMeshBuild(SurfaceMesh* mesh,
                      SceneObject* objects,
                      int objectCount,
                      double maxSegmentLength);
void TriangleMeshInit(TriangleMesh* mesh);
void TriangleMeshFree(TriangleMesh* mesh);
bool SurfaceBuildMeshes(SurfaceMesh* segments,
                        TriangleMesh* triangles,
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
