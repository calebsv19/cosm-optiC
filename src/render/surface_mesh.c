#include "render/surface_mesh.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void MeshEnsureCapacity(SurfaceMesh* mesh, int target) {
    if (!mesh) return;
    if (target <= mesh->capacity) return;
    int newCapacity = mesh->capacity == 0 ? 128 : mesh->capacity;
    while (newCapacity < target) {
        newCapacity *= 2;
    }
    SurfaceSegment* next = (SurfaceSegment*)realloc(mesh->segments,
                                                    (size_t)newCapacity * sizeof(SurfaceSegment));
    if (!next) {
        return;
    }
    mesh->segments = next;
    mesh->capacity = newCapacity;
}

static void MeshReset(SurfaceMesh* mesh) {
    if (!mesh) return;
    mesh->segmentCount = 0;
}

static void AppendSegment(SurfaceMesh* mesh,
                          int objectIndex,
                          int localIndex,
                          double x0,
                          double y0,
                          double x1,
                          double y1,
                          double nx,
                          double ny) {
    if (!mesh) return;
    if (mesh->segmentCount >= mesh->capacity) {
        MeshEnsureCapacity(mesh, mesh->segmentCount + 1);
        if (mesh->segmentCount >= mesh->capacity) {
            return;
        }
    }
    SurfaceSegment* segment = &mesh->segments[mesh->segmentCount++];
    segment->x0 = x0;
    segment->y0 = y0;
    segment->x1 = x1;
    segment->y1 = y1;
    segment->nx = nx;
    segment->ny = ny;
    double dx = x1 - x0;
    double dy = y1 - y0;
    segment->length = hypot(dx, dy);
    segment->objectIndex = objectIndex;
    segment->localIndex = localIndex;
}

static void SubdividePathEdge(SurfaceMesh* mesh,
                              int objectIndex,
                              const PathVertex* v0,
                              const PathVertex* v1,
                              int* localIndex) {
    double dx = v1->x - v0->x;
    double dy = v1->y - v0->y;
    double edgeLength = hypot(dx, dy);
    if (edgeLength < 1e-6) return;
    int segments = (int)ceil(edgeLength / fmax(mesh->maxSegmentLength, 1.0));
    double stepX = dx / segments;
    double stepY = dy / segments;
    for (int i = 0; i < segments; i++) {
        double sx0 = v0->x + stepX * i;
        double sy0 = v0->y + stepY * i;
        double sx1 = v0->x + stepX * (i + 1);
        double sy1 = v0->y + stepY * (i + 1);
        double tMid = ((double)i + 0.5) / (double)segments;
        double nx = v0->nx + (v1->nx - v0->nx) * tMid;
        double ny = v0->ny + (v1->ny - v0->ny) * tMid;
        double nLen = hypot(nx, ny);
        if (nLen > 1e-6) {
            nx /= nLen;
            ny /= nLen;
        } else {
            nx = -stepY;
            ny = stepX;
            double len = hypot(nx, ny);
            if (len > 1e-6) {
                nx /= len;
                ny /= len;
            }
        }
        AppendSegment(mesh,
                      objectIndex,
                      (*localIndex)++,
                      sx0,
                      sy0,
                      sx1,
                      sy1,
                      nx,
                      ny);
    }
}

void SurfaceMeshInit(SurfaceMesh* mesh) {
    if (!mesh) return;
    memset(mesh, 0, sizeof(*mesh));
    mesh->maxSegmentLength = 8.0;
}

void SurfaceMeshFree(SurfaceMesh* mesh) {
    if (!mesh) return;
    free(mesh->segments);
    mesh->segments = NULL;
    mesh->segmentCount = 0;
    mesh->capacity = 0;
    free(mesh->objectOffsets);
    mesh->objectOffsets = NULL;
    mesh->objectCount = 0;
    mesh->offsetsCapacity = 0;
}

bool SurfaceMeshBuild(SurfaceMesh* mesh,
                      SceneObject* objects,
                      int objectCount,
                      double maxSegmentLength) {
    if (!mesh || !objects || objectCount <= 0) {
        return false;
    }
    mesh->maxSegmentLength = (maxSegmentLength > 0.5) ? maxSegmentLength : 5.0;
    MeshReset(mesh);
    SegmentPath path;
    SegmentPathInit(&path);
    if (mesh->offsetsCapacity < objectCount + 1) {
        int newCap = objectCount + 1;
        int* offsets = (int*)realloc(mesh->objectOffsets, (size_t)newCap * sizeof(int));
        if (!offsets) {
            SegmentPathFree(&path);
            return false;
        }
        mesh->objectOffsets = offsets;
        mesh->offsetsCapacity = newCap;
    }
    mesh->objectCount = objectCount;
    mesh->objectOffsets[0] = 0;
    for (int i = 0; i < objectCount; i++) {
        SceneObject* obj = &objects[i];
        int localIndex = 0;
        mesh->objectOffsets[i] = mesh->segmentCount;
        if (!OM_BuildSegmentPath(obj, mesh->maxSegmentLength, &path)) {
            continue;
        }
        for (int v = 0; v < path.count; v++) {
            int next = (v + 1) % path.count;
            SubdividePathEdge(mesh,
                              i,
                              &path.vertices[v],
                              &path.vertices[next],
                              &localIndex);
        }
    }
    SegmentPathFree(&path);
    mesh->objectOffsets[objectCount] = mesh->segmentCount;
    return mesh->segmentCount > 0;
}

const SurfaceSegment* SurfaceMeshFindSegment(const SurfaceMesh* mesh,
                                             int objectIndex,
                                             double px,
                                             double py,
                                             double nx,
                                             double ny) {
    if (!mesh || !mesh->segments || !mesh->objectOffsets) return NULL;
    if (objectIndex < 0 || objectIndex >= mesh->objectCount) return NULL;
    int start = mesh->objectOffsets[objectIndex];
    int end = mesh->objectOffsets[objectIndex + 1];
    if (start < 0 || end > mesh->segmentCount || start >= end) return NULL;
    const SurfaceSegment* best = NULL;
    double maxDist2 = fmax(mesh->maxSegmentLength * mesh->maxSegmentLength, 1.0);
    for (int i = start; i < end; i++) {
        const SurfaceSegment* segment = &mesh->segments[i];
        double dx = segment->x1 - segment->x0;
        double dy = segment->y1 - segment->y0;
        double len2 = fmax(dx * dx + dy * dy, 1e-9);
        double t = ((px - segment->x0) * dx + (py - segment->y0) * dy) / len2;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        double closestX = segment->x0 + dx * t;
        double closestY = segment->y0 + dy * t;
        double cx = px - closestX;
        double cy = py - closestY;
        double dist2 = cx * cx + cy * cy;
        if (dist2 > maxDist2) {
            continue;
        }
        double normalDot = segment->nx * nx + segment->ny * ny;
        if (normalDot < 0.25) {
            continue;
        }
        maxDist2 = dist2;
        best = segment;
    }
    return best;
}
