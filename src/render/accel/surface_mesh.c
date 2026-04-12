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

static void TriangleMeshReset(TriangleMesh* mesh) {
    if (!mesh) return;
    mesh->vertexCount = 0;
    mesh->triangleCount = 0;
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

static bool TriangleMeshEnsureVertices(TriangleMesh* mesh, int target) {
    if (!mesh || target <= 0) return false;
    if (mesh->vertexCapacity >= target) return true;
    int newCap = mesh->vertexCapacity > 0 ? mesh->vertexCapacity : 64;
    while (newCap < target) newCap *= 2;
    TriangleVertex* next = (TriangleVertex*)realloc(mesh->vertices,
                                                    (size_t)newCap * sizeof(TriangleVertex));
    if (!next) return false;
    mesh->vertices = next;
    mesh->vertexCapacity = newCap;
    return true;
}

static bool TriangleMeshEnsureTriangles(TriangleMesh* mesh, int target) {
    if (!mesh || target <= 0) return false;
    if (mesh->triangleCapacity >= target) return true;
    int newCap = mesh->triangleCapacity > 0 ? mesh->triangleCapacity : 64;
    while (newCap < target) newCap *= 2;
    TriangleFace* next = (TriangleFace*)realloc(mesh->triangles,
                                                (size_t)newCap * sizeof(TriangleFace));
    if (!next) return false;
    mesh->triangles = next;
    mesh->triangleCapacity = newCap;
    return true;
}

static double SignedArea(const PathVertex* verts, int count) {
    double area = 0.0;
    for (int i = 0; i < count; i++) {
        int j = (i + 1) % count;
        area += verts[i].x * verts[j].y - verts[j].x * verts[i].y;
    }
    return 0.5 * area;
}

static bool PointInTriangle(double ax, double ay,
                            double bx, double by,
                            double cx, double cy,
                            double px, double py) {
    double v0x = cx - ax;
    double v0y = cy - ay;
    double v1x = bx - ax;
    double v1y = by - ay;
    double v2x = px - ax;
    double v2y = py - ay;

    double dot00 = v0x * v0x + v0y * v0y;
    double dot01 = v0x * v1x + v0y * v1y;
    double dot02 = v0x * v2x + v0y * v2y;
    double dot11 = v1x * v1x + v1y * v1y;
    double dot12 = v1x * v2x + v1y * v2y;

    double denom = dot00 * dot11 - dot01 * dot01;
    if (fabs(denom) < 1e-12) return false;
    double invDenom = 1.0 / denom;
    double u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    double v = (dot00 * dot12 - dot01 * dot02) * invDenom;
    return (u >= -1e-6) && (v >= -1e-6) && (u + v <= 1.0 + 1e-6);
}

static bool TriangulatePath(const SegmentPath* path,
                            TriangleMesh* mesh,
                            int objectIndex,
                            int vertexStart) {
    if (!mesh || !path || path->count < 3) return false;
    int n = path->count;
    int* indices = (int*)malloc((size_t)n * sizeof(int));
    if (!indices) return false;
    for (int i = 0; i < n; i++) indices[i] = i;
    double area = SignedArea(path->vertices, path->count);
    bool ccw = area >= 0.0;
    int guard = 0;
    while (n >= 3 && guard < 10000) {
        bool earFound = false;
        for (int i = 0; i < n; i++) {
            int prev = (i + n - 1) % n;
            int next = (i + 1) % n;
            int ia = indices[prev];
            int ib = indices[i];
            int ic = indices[next];
            double ax = path->vertices[ia].x;
            double ay = path->vertices[ia].y;
            double bx = path->vertices[ib].x;
            double by = path->vertices[ib].y;
            double cx = path->vertices[ic].x;
            double cy = path->vertices[ic].y;
            double cross = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
            if (ccw) {
                if (cross <= 1e-6) continue;
            } else {
                if (cross >= -1e-6) continue;
            }
            bool contains = false;
            for (int j = 0; j < n; j++) {
                if (j == prev || j == i || j == next) continue;
                int idx = indices[j];
                double px = path->vertices[idx].x;
                double py = path->vertices[idx].y;
                if (PointInTriangle(ax, ay, bx, by, cx, cy, px, py)) {
                    contains = true;
                    break;
                }
            }
            if (contains) continue;
            if (!TriangleMeshEnsureTriangles(mesh, mesh->triangleCount + 1)) {
                free(indices);
                return false;
            }
            TriangleFace* face = &mesh->triangles[mesh->triangleCount++];
            face->v0 = vertexStart + ia;
            face->v1 = vertexStart + ib;
            face->v2 = vertexStart + ic;
            face->objectIndex = objectIndex;
            for (int k = i; k < n - 1; k++) {
                indices[k] = indices[k + 1];
            }
            n--;
            earFound = true;
            break;
        }
        if (!earFound) {
            if (n >= 3) {
                if (!TriangleMeshEnsureTriangles(mesh, mesh->triangleCount + (n - 2))) {
                    free(indices);
                    return false;
                }
                for (int k = 1; k < n - 1; k++) {
                    TriangleFace* face = &mesh->triangles[mesh->triangleCount++];
                    face->v0 = vertexStart + indices[0];
                    face->v1 = vertexStart + indices[k];
                    face->v2 = vertexStart + indices[k + 1];
                    face->objectIndex = objectIndex;
                }
            }
            break;
        }
        guard++;
    }
    free(indices);
    return true;
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

void TriangleMeshInit(TriangleMesh* mesh) {
    if (!mesh) return;
    memset(mesh, 0, sizeof(*mesh));
}

void TriangleMeshFree(TriangleMesh* mesh) {
    if (!mesh) return;
    free(mesh->vertices);
    mesh->vertices = NULL;
    mesh->vertexCount = 0;
    mesh->vertexCapacity = 0;
    free(mesh->triangles);
    mesh->triangles = NULL;
    mesh->triangleCount = 0;
    mesh->triangleCapacity = 0;
    free(mesh->vertexOffsets);
    mesh->vertexOffsets = NULL;
    free(mesh->triangleOffsets);
    mesh->triangleOffsets = NULL;
    mesh->objectCount = 0;
    mesh->offsetsCapacity = 0;
}

static bool SurfaceBuildInternal(SurfaceMesh* mesh,
                                 TriangleMesh* triMesh,
                                 SceneObject* objects,
                                 int objectCount,
                                 double maxSegmentLength) {
    if (!mesh || !objects || objectCount <= 0) {
        return false;
    }
    mesh->maxSegmentLength = (maxSegmentLength > 0.5) ? maxSegmentLength : 5.0;
    MeshReset(mesh);
    if (triMesh) {
        TriangleMeshReset(triMesh);
    }
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

    if (triMesh) {
        if (triMesh->offsetsCapacity < objectCount + 1) {
            int newCap = objectCount + 1;
            int* vOffsets = (int*)realloc(triMesh->vertexOffsets,
                                          (size_t)newCap * sizeof(int));
            int* tOffsets = (int*)realloc(triMesh->triangleOffsets,
                                          (size_t)newCap * sizeof(int));
            if (!vOffsets || !tOffsets) {
                free(vOffsets);
                free(tOffsets);
                SegmentPathFree(&path);
                return false;
            }
            triMesh->vertexOffsets = vOffsets;
            triMesh->triangleOffsets = tOffsets;
            triMesh->offsetsCapacity = newCap;
        }
        triMesh->objectCount = objectCount;
        triMesh->vertexOffsets[0] = 0;
        triMesh->triangleOffsets[0] = 0;
    }

    for (int i = 0; i < objectCount; i++) {
        SceneObject* obj = &objects[i];
        int localIndex = 0;
        mesh->objectOffsets[i] = mesh->segmentCount;
        if (triMesh) {
            triMesh->vertexOffsets[i] = triMesh->vertexCount;
            triMesh->triangleOffsets[i] = triMesh->triangleCount;
        }

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

        if (triMesh && path.count >= 3) {
            if (!TriangleMeshEnsureVertices(triMesh,
                                            triMesh->vertexCount + path.count)) {
                SegmentPathFree(&path);
                return false;
            }
            int vertexStart = triMesh->vertexCount;
            for (int v = 0; v < path.count; v++) {
                TriangleVertex* vert = &triMesh->vertices[triMesh->vertexCount++];
                vert->x = path.vertices[v].x;
                vert->y = path.vertices[v].y;
                vert->nx = path.vertices[v].nx;
                vert->ny = path.vertices[v].ny;
            }
            TriangulatePath(&path, triMesh, i, vertexStart);
        }
    }
    SegmentPathFree(&path);
    mesh->objectOffsets[objectCount] = mesh->segmentCount;
    if (triMesh) {
        triMesh->vertexOffsets[objectCount] = triMesh->vertexCount;
        triMesh->triangleOffsets[objectCount] = triMesh->triangleCount;
    }
    return mesh->segmentCount > 0;
}

bool SurfaceMeshBuild(SurfaceMesh* mesh,
                      SceneObject* objects,
                      int objectCount,
                      double maxSegmentLength) {
    return SurfaceBuildInternal(mesh, NULL, objects, objectCount, maxSegmentLength);
}

bool SurfaceBuildMeshes(SurfaceMesh* segments,
                        TriangleMesh* triangles,
                        SceneObject* objects,
                        int objectCount,
                        double maxSegmentLength) {
    return SurfaceBuildInternal(segments, triangles, objects, objectCount, maxSegmentLength);
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
