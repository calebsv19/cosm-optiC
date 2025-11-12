#ifndef UNIFORM_GRID_H
#define UNIFORM_GRID_H

#include <stdbool.h>
#include "scene/object_manager.h"
#include "render/ray_types.h"
#include "render/surface_mesh.h"

typedef struct {
    int* indices;
    int count;
    int capacity;
} GridCell;

typedef struct {
    double minX, minY;
    double maxX, maxY;
    double cellSize;
    int cellsX;
    int cellsY;
    GridCell* objectCells;
    GridCell* triangleCells;
    SceneObject* objects;
    int objectCount;
    const TriangleMesh* triangleMesh;
} UniformGrid;

bool UniformGridBuild(UniformGrid* grid,
                      SceneObject* objects,
                      int objectCount,
                      const TriangleMesh* triangles,
                      double cellSize);
void UniformGridClear(UniformGrid* grid);
void UniformGridFree(UniformGrid* grid);

const GridCell* UniformGridGetCell(const UniformGrid* grid, int cellX, int cellY);
bool UniformGridPointTest(const UniformGrid* grid, double x, double y, int** indices, int* count);
bool UniformGridTraceRay(const UniformGrid* grid, const Ray2D* ray, double tMin, double tMax, HitInfo2D* hit);

#endif // UNIFORM_GRID_H
