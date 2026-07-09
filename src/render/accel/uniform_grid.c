#include "render/uniform_grid.h"
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

#define GRID_EPSILON 1e-6

static void GridCellAdd(GridCell* cell, int index) {
    if (cell->count + 1 > cell->capacity) {
        int newCap = cell->capacity == 0 ? 4 : cell->capacity * 2;
        int* newData = (int*)realloc(cell->indices, (size_t)newCap * sizeof(int));
        if (!newData) {
            return;
        }
        cell->indices = newData;
        cell->capacity = newCap;
    }
    cell->indices[cell->count++] = index;
}

static void ClearCellArray(GridCell* cells, int count) {
    if (!cells) return;
    for (int i = 0; i < count; i++) {
        free(cells[i].indices);
    }
    free(cells);
}

void UniformGridClear(UniformGrid* grid) {
    if (!grid) return;
    int totalCells = grid->cellsX * grid->cellsY;
    if (totalCells < 0) totalCells = 0;
    ClearCellArray(grid->objectCells, totalCells);
    ClearCellArray(grid->triangleCells, totalCells);
    grid->objectCells = NULL;
    grid->triangleCells = NULL;
    grid->cellsX = grid->cellsY = 0;
    grid->minX = grid->minY = 0.0;
    grid->maxX = grid->maxY = 0.0;
    grid->objectCount = 0;
    grid->triangleMesh = NULL;
}

void UniformGridFree(UniformGrid* grid) {
    UniformGridClear(grid);
}

static void ExpandBounds(UniformGrid* grid, SceneObject* objects, int objectCount) {
    if (objectCount == 0) {
        grid->minX = grid->minY = -1.0;
        grid->maxX = grid->maxY = 1.0;
        return;
    }
    double minX = DBL_MAX, minY = DBL_MAX;
    double maxX = -DBL_MAX, maxY = -DBL_MAX;
    bool seeded = false;
    for (int i = 0; i < objectCount; i++) {
        double objMinX, objMinY, objMaxX, objMaxY;
        if (!SceneObjectParticipatesInRender(&objects[i])) continue;
        ComputeObjectBounds(&objects[i], &objMinX, &objMinY, &objMaxX, &objMaxY);
        if (objMinX < minX) minX = objMinX;
        if (objMinY < minY) minY = objMinY;
        if (objMaxX > maxX) maxX = objMaxX;
        if (objMaxY > maxY) maxY = objMaxY;
        seeded = true;
    }
    if (!seeded) {
        grid->minX = grid->minY = -1.0;
        grid->maxX = grid->maxY = 1.0;
        return;
    }
    double margin = 5.0;
    grid->minX = minX - margin;
    grid->minY = minY - margin;
    grid->maxX = maxX + margin;
    grid->maxY = maxY + margin;
}

bool UniformGridBuild(UniformGrid* grid,
                      SceneObject* objects,
                      int objectCount,
                      const TriangleMesh* triangles,
                      double cellSize) {
    if (!grid) return false;
    if (cellSize < 1.0) cellSize = 1.0;

    UniformGridClear(grid);
    grid->objects = objects;
    grid->objectCount = objectCount;
    grid->triangleMesh = triangles;
    grid->cellSize = cellSize;

    ExpandBounds(grid, objects, objectCount);
    double width = grid->maxX - grid->minX;
    double height = grid->maxY - grid->minY;

    grid->cellsX = (int)fmax(1.0, ceil(width / cellSize));
    grid->cellsY = (int)fmax(1.0, ceil(height / cellSize));

    size_t cellCount = (size_t)grid->cellsX * (size_t)grid->cellsY;
    grid->objectCells = (GridCell*)calloc(cellCount, sizeof(GridCell));
    if (!grid->objectCells) {
        grid->cellsX = grid->cellsY = 0;
        return false;
    }
    if (triangles && triangles->triangleCount > 0) {
        grid->triangleCells = (GridCell*)calloc(cellCount, sizeof(GridCell));
        if (!grid->triangleCells) {
            UniformGridClear(grid);
            return false;
        }
    }

    for (int i = 0; i < objectCount; i++) {
        double objMinX, objMinY, objMaxX, objMaxY;
        if (!SceneObjectParticipatesInRender(&objects[i])) continue;
        ComputeObjectBounds(&objects[i], &objMinX, &objMinY, &objMaxX, &objMaxY);
        int minCellX = (int)floor((objMinX - grid->minX) / grid->cellSize);
        int maxCellX = (int)floor((objMaxX - grid->minX) / grid->cellSize);
        int minCellY = (int)floor((objMinY - grid->minY) / grid->cellSize);
        int maxCellY = (int)floor((objMaxY - grid->minY) / grid->cellSize);

        if (minCellX < 0) minCellX = 0;
        if (minCellY < 0) minCellY = 0;
        if (maxCellX >= grid->cellsX) maxCellX = grid->cellsX - 1;
        if (maxCellY >= grid->cellsY) maxCellY = grid->cellsY - 1;

        for (int y = minCellY; y <= maxCellY; y++) {
            for (int x = minCellX; x <= maxCellX; x++) {
                size_t idx = (size_t)y * (size_t)grid->cellsX + (size_t)x;
                GridCellAdd(&grid->objectCells[idx], i);
            }
        }
    }

    if (triangles && triangles->triangleCount > 0 && grid->triangleCells) {
        for (int triIndex = 0; triIndex < triangles->triangleCount; triIndex++) {
            const TriangleFace* face = &triangles->triangles[triIndex];
            const TriangleVertex* v0 = &triangles->vertices[face->v0];
            const TriangleVertex* v1 = &triangles->vertices[face->v1];
            const TriangleVertex* v2 = &triangles->vertices[face->v2];
            double minX = fmin(v0->x, fmin(v1->x, v2->x));
            double maxX = fmax(v0->x, fmax(v1->x, v2->x));
            double minY = fmin(v0->y, fmin(v1->y, v2->y));
            double maxY = fmax(v0->y, fmax(v1->y, v2->y));
            int minCellX = (int)floor((minX - grid->minX) / grid->cellSize);
            int maxCellX = (int)floor((maxX - grid->minX) / grid->cellSize);
            int minCellY = (int)floor((minY - grid->minY) / grid->cellSize);
            int maxCellY = (int)floor((maxY - grid->minY) / grid->cellSize);
            if (minCellX < 0) minCellX = 0;
            if (minCellY < 0) minCellY = 0;
            if (maxCellX >= grid->cellsX) maxCellX = grid->cellsX - 1;
            if (maxCellY >= grid->cellsY) maxCellY = grid->cellsY - 1;
            for (int y = minCellY; y <= maxCellY; y++) {
                for (int x = minCellX; x <= maxCellX; x++) {
                    size_t idx = (size_t)y * (size_t)grid->cellsX + (size_t)x;
                    GridCellAdd(&grid->triangleCells[idx], triIndex);
                }
            }
        }
    }

    return true;
}

const GridCell* UniformGridGetCell(const UniformGrid* grid, int cellX, int cellY) {
    if (!grid || !grid->objectCells) return NULL;
    if (cellX < 0 || cellY < 0 || cellX >= grid->cellsX || cellY >= grid->cellsY) return NULL;
    size_t idx = (size_t)cellY * (size_t)grid->cellsX + (size_t)cellX;
    return &grid->objectCells[idx];
}

bool UniformGridPointTest(const UniformGrid* grid, double x, double y, int** indices, int* count) {
    if (!grid || !grid->objectCells) return false;
    int cellX = (int)floor((x - grid->minX) / grid->cellSize);
    int cellY = (int)floor((y - grid->minY) / grid->cellSize);
    const GridCell* cell = UniformGridGetCell(grid, cellX, cellY);
    if (!cell) return false;
    if (indices) *indices = cell->indices;
    if (count) *count = cell->count;
    return true;
}

static double Cross2(double ax, double ay, double bx, double by) {
    return ax * by - ay * bx;
}

static bool IntersectTriangleFace(const TriangleMesh* mesh,
                                  int triangleIndex,
                                  const Ray2D* ray,
                                  double tMin,
                                  double tMax,
                                  HitInfo2D* hit) {
    if (!mesh || triangleIndex < 0 || triangleIndex >= mesh->triangleCount) {
        return false;
    }
    const TriangleFace* face = &mesh->triangles[triangleIndex];
    const TriangleVertex* v0 = &mesh->vertices[face->v0];
    const TriangleVertex* v1 = &mesh->vertices[face->v1];
    const TriangleVertex* v2 = &mesh->vertices[face->v2];
    const TriangleVertex* verts[3] = {v0, v1, v2};

    bool found = false;
    double bestT = tMax;
    double hitPx = 0.0;
    double hitPy = 0.0;

    for (int e = 0; e < 3; e++) {
        const TriangleVertex* a = verts[e];
        const TriangleVertex* b = verts[(e + 1) % 3];
        double sx = b->x - a->x;
        double sy = b->y - a->y;
        double denom = Cross2(ray->dx, ray->dy, sx, sy);
        if (fabs(denom) < GRID_EPSILON) continue;
        double ax = a->x - ray->ox;
        double ay = a->y - ray->oy;
        double t = Cross2(ax, ay, sx, sy) / denom;
        double u = Cross2(ax, ay, ray->dx, ray->dy) / denom;
        if (t < tMin || t > bestT) continue;
        if (u < 0.0 || u > 1.0) continue;
        found = true;
        bestT = t;
        hitPx = ray->ox + ray->dx * t;
        hitPy = ray->oy + ray->dy * t;
    }

    if (found && hit) {
        hit->t = bestT;
        hit->px = hitPx;
        hit->py = hitPy;
        hit->triangleIndex = triangleIndex;
        double denom = (v1->y - v2->y) * (v0->x - v2->x) + (v2->x - v1->x) * (v0->y - v2->y);
        double u = 0.0;
        double v = 0.0;
        if (fabs(denom) > GRID_EPSILON) {
            u = ((v1->y - v2->y) * (hitPx - v2->x) + (v2->x - v1->x) * (hitPy - v2->y)) / denom;
            v = ((v2->y - v0->y) * (hitPx - v2->x) + (v0->x - v2->x) * (hitPy - v2->y)) / denom;
        }
        double w = 1.0 - u - v;
        hit->baryU = u;
        hit->baryV = v;
        hit->baryW = w;
        double nx = u * v0->nx + v * v1->nx + w * v2->nx;
        double ny = u * v0->ny + v * v1->ny + w * v2->ny;
        double len = sqrt(nx * nx + ny * ny);
        if (len > GRID_EPSILON) {
            nx /= len;
            ny /= len;
        } else {
            nx = v1->y - v0->y;
            ny = -(v1->x - v0->x);
            len = sqrt(nx * nx + ny * ny);
            if (len > GRID_EPSILON) {
                nx /= len;
                ny /= len;
            }
        }
        double dot = nx * ray->dx + ny * ray->dy;
        if (dot > 0.0) {
            nx = -nx;
            ny = -ny;
        }
        hit->nx = nx;
        hit->ny = ny;
        hit->objectIndex = face->objectIndex;
    }
    return found;
}

static bool IntersectCircle(const SceneObject* obj, const Ray2D* ray, double tMin, double tMax, HitInfo2D* hit) {
    double r = obj->radius * obj->scale;
    double cx = obj->x;
    double cy = obj->y;
    double ox = ray->ox - cx;
    double oy = ray->oy - cy;

    double a = ray->dx * ray->dx + ray->dy * ray->dy;
    double b = 2.0 * (ox * ray->dx + oy * ray->dy);
    double c = ox * ox + oy * oy - r * r;

    double disc = b * b - 4.0 * a * c;
    if (disc < 0.0) return false;
    double sqrtDisc = sqrt(disc);
    double invA = 0.5 / a;
    double t0 = (-b - sqrtDisc) * invA;
    double t1 = (-b + sqrtDisc) * invA;
    if (t0 > t1) {
        double tmp = t0;
        t0 = t1;
        t1 = tmp;
    }
    double t = t0;
    if (t < tMin || t > tMax) {
        t = t1;
        if (t < tMin || t > tMax) return false;
    }
    if (hit) {
        hit->triangleIndex = -1;
        hit->baryU = 0.0;
        hit->baryV = 0.0;
        hit->baryW = 1.0;
        hit->t = t;
        hit->px = ray->ox + ray->dx * t;
        hit->py = ray->oy + ray->dy * t;
        double nx = (hit->px - cx);
        double ny = (hit->py - cy);
        double len = sqrt(nx * nx + ny * ny);
        if (len > GRID_EPSILON) {
            nx /= len;
            ny /= len;
        }
        hit->nx = nx;
        hit->ny = ny;
    }
    return true;
}

static bool IntersectPolygon(const SceneObject* obj, const Ray2D* ray, double tMin, double tMax, HitInfo2D* hit) {
    bool found = false;
    double bestT = tMax;
    double hitPx = 0.0, hitPy = 0.0, hitNx = 0.0, hitNy = 0.0;
    for (int i = 0; i < obj->numPoints; i++) {
        int next = (i + 1) % obj->numPoints;
        double ax = obj->shapePoints[i][0] + obj->x;
        double ay = obj->shapePoints[i][1] + obj->y;
        double bx = obj->shapePoints[next][0] + obj->x;
        double by = obj->shapePoints[next][1] + obj->y;
        double sx = bx - ax;
        double sy = by - ay;
        double denom = Cross2(ray->dx, ray->dy, sx, sy);
        if (fabs(denom) < GRID_EPSILON) continue;
        double axo = ax - ray->ox;
        double ayo = ay - ray->oy;
        double t = Cross2(axo, ayo, sx, sy) / denom;
        double u = Cross2(axo, ayo, ray->dx, ray->dy) / denom;
        if (t < tMin || t > bestT) continue;
        if (u < 0.0 || u > 1.0) continue;

        found = true;
        bestT = t;
        hitPx = ray->ox + ray->dx * t;
        hitPy = ray->oy + ray->dy * t;
        hitNx = sy;
        hitNy = -sx;
        double len = sqrt(hitNx * hitNx + hitNy * hitNy);
        if (len > GRID_EPSILON) {
            hitNx /= len;
            hitNy /= len;
        }
        double dot = hitNx * ray->dx + hitNy * ray->dy;
        if (dot > 0.0) {
            hitNx = -hitNx;
            hitNy = -hitNy;
        }
    }
    if (found && hit) {
        hit->triangleIndex = -1;
        hit->baryU = 0.0;
        hit->baryV = 0.0;
        hit->baryW = 1.0;
        hit->t = bestT;
        hit->px = hitPx;
        hit->py = hitPy;
        hit->nx = hitNx;
        hit->ny = hitNy;
    }
    return found;
}

static bool IntersectSceneObject(const SceneObject* obj, const Ray2D* ray, double tMin, double tMax, HitInfo2D* hit) {
    if (strcmp(obj->type, "circle") == 0) {
        return IntersectCircle(obj, ray, tMin, tMax, hit);
    }
    return IntersectPolygon(obj, ray, tMin, tMax, hit);
}

bool UniformGridTraceRay(const UniformGrid* grid, const Ray2D* ray, double tMin, double tMax, HitInfo2D* hit) {
    bool hasObjects = (grid && grid->objectCells);
    bool hasTriangles = (grid && grid->triangleCells && grid->triangleMesh && grid->triangleMesh->triangleCount > 0);
    if (!grid || grid->cellsX == 0 || grid->cellsY == 0 || (!hasObjects && !hasTriangles)) {
        return false;
    }

    double dirX = ray->dx;
    double dirY = ray->dy;
    double originX = ray->ox;
    double originY = ray->oy;

    double t0 = tMin;
    double t1 = (tMax > 0.0) ? tMax : DBL_MAX;

    double gridMinX = grid->minX;
    double gridMinY = grid->minY;
    double gridMaxX = grid->minX + grid->cellsX * grid->cellSize;
    double gridMaxY = grid->minY + grid->cellsY * grid->cellSize;

    double invDirX = fabs(dirX) < GRID_EPSILON ? DBL_MAX : 1.0 / dirX;
    double invDirY = fabs(dirY) < GRID_EPSILON ? DBL_MAX : 1.0 / dirY;

    double tNearX = (gridMinX - originX) * invDirX;
    double tFarX = (gridMaxX - originX) * invDirX;
    if (tNearX > tFarX) {
        double tmp = tNearX;
        tNearX = tFarX;
        tFarX = tmp;
    }

    double tNearY = (gridMinY - originY) * invDirY;
    double tFarY = (gridMaxY - originY) * invDirY;
    if (tNearY > tFarY) {
        double tmp = tNearY;
        tNearY = tFarY;
        tFarY = tmp;
    }

    double tEnter = fmax(tNearX, tNearY);
    double tExit = fmin(tFarX, tFarY);

    if (tExit < tEnter || tExit < t0) {
        return false;
    }

    if (tEnter > t0) {
        t0 = tEnter;
    }

    double startX = originX + dirX * t0;
    double startY = originY + dirY * t0;

    int cellX = (int)floor((startX - grid->minX) / grid->cellSize);
    int cellY = (int)floor((startY - grid->minY) / grid->cellSize);

    if (cellX < 0) cellX = 0;
    if (cellY < 0) cellY = 0;
    if (cellX >= grid->cellsX) cellX = grid->cellsX - 1;
    if (cellY >= grid->cellsY) cellY = grid->cellsY - 1;

    int stepX = (dirX > 0.0) ? 1 : (dirX < 0.0 ? -1 : 0);
    int stepY = (dirY > 0.0) ? 1 : (dirY < 0.0 ? -1 : 0);

    double nextBoundaryX = (stepX > 0)
        ? grid->minX + (cellX + 1) * grid->cellSize
        : grid->minX + cellX * grid->cellSize;
    double nextBoundaryY = (stepY > 0)
        ? grid->minY + (cellY + 1) * grid->cellSize
        : grid->minY + cellY * grid->cellSize;

    double tMaxX = (fabs(dirX) < GRID_EPSILON) ? DBL_MAX : (nextBoundaryX - startX) / dirX + t0;
    double tMaxY = (fabs(dirY) < GRID_EPSILON) ? DBL_MAX : (nextBoundaryY - startY) / dirY + t0;
    double tDeltaX = (fabs(dirX) < GRID_EPSILON) ? DBL_MAX : grid->cellSize / fabs(dirX);
    double tDeltaY = (fabs(dirY) < GRID_EPSILON) ? DBL_MAX : grid->cellSize / fabs(dirY);

    bool hitFound = false;
    double bestT = t1;
    HitInfo2D tempHit = {0};
    tempHit.objectIndex = -1;
    tempHit.triangleIndex = -1;
    tempHit.baryW = 1.0;

    while (cellX >= 0 && cellX < grid->cellsX &&
           cellY >= 0 && cellY < grid->cellsY) {
        double cellExitT = fmin(tMaxX, tMaxY);
        size_t cellIndex = (size_t)cellY * (size_t)grid->cellsX + (size_t)cellX;
        const GridCell* objCell = hasObjects ? &grid->objectCells[cellIndex] : NULL;
        if (objCell && objCell->count > 0) {
            for (int i = 0; i < objCell->count; i++) {
                int objIndex = objCell->indices[i];
                if (objIndex < 0 || objIndex >= grid->objectCount) continue;
                HitInfo2D localHit = {0};
                localHit.objectIndex = -1;
                localHit.triangleIndex = -1;
                localHit.baryW = 1.0;
                if (IntersectSceneObject(&grid->objects[objIndex], ray, t0, fmin(cellExitT, bestT), &localHit)) {
                    if (localHit.t < bestT && localHit.t >= t0 + GRID_EPSILON) {
                        bestT = localHit.t;
                        tempHit = localHit;
                        tempHit.objectIndex = objIndex;
                        hitFound = true;
                    }
                }
            }
        }

        const GridCell* triCell = hasTriangles ? &grid->triangleCells[cellIndex] : NULL;
        if (!hitFound && triCell && triCell->count > 0) {
            for (int i = 0; i < triCell->count; i++) {
                int triIndex = triCell->indices[i];
                HitInfo2D localHit = {0};
                localHit.objectIndex = -1;
                localHit.triangleIndex = -1;
                localHit.baryW = 1.0;
                if (IntersectTriangleFace(grid->triangleMesh,
                                          triIndex,
                                          ray,
                                          t0,
                                          fmin(cellExitT, bestT),
                                          &localHit)) {
                    if (localHit.t < bestT && localHit.t >= t0 + GRID_EPSILON) {
                        bestT = localHit.t;
                        tempHit = localHit;
                        hitFound = true;
                    }
                }
            }
        }
        if (hitFound) {
            break;
        }

        if (tMaxX < tMaxY) {
            cellX += stepX;
            t0 = tMaxX;
            tMaxX += tDeltaX;
        } else {
            cellY += stepY;
            t0 = tMaxY;
            tMaxY += tDeltaY;
        }

        if (t0 > t1) break;
    }

    if (hitFound && hit) {
        *hit = tempHit;
        return true;
    }
    return hitFound;
}
