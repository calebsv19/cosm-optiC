#ifndef RENDER_RUNTIME_TRIANGLE_BVH_INTERNAL_3D_H
#define RENDER_RUNTIME_TRIANGLE_BVH_INTERNAL_3D_H

#include "render/runtime_triangle_bvh_3d.h"

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RUNTIME_TRIANGLE_BVH_LEAF_SIZE 4
#ifndef RUNTIME_TRIANGLE_BVH_MAX_STACK
#define RUNTIME_TRIANGLE_BVH_MAX_STACK 128
#endif

typedef struct RuntimeTriangleBVH3DNode {
    Vec3 min;
    Vec3 max;
    int left;
    int right;
    int start;
    int count;
} RuntimeTriangleBVH3DNode;

typedef struct RuntimeTriangleBVH3DBoundsF {
    float x;
    float y;
    float z;
} RuntimeTriangleBVH3DBoundsF;

struct RuntimeTriangleBVH3D {
    RuntimeTriangleBVH3DNode* nodes;
    int nodeCount;
    int nodeCapacity;
    int leafCount;
    int maxDepth;
    int maxLeafTriangleCount;
    double buildCpuMs;
    double allocationCpuMs;
    double centroidBuildCpuMs;
    double treeBuildCpuMs;
    double rangeBoundsCpuMs;
    double sortCpuMs;
    double nodeAppendCpuMs;
    double finalStatsCpuMs;
    double buildUnaccountedCpuMs;
    uint64_t rangeBoundsCalls;
    uint64_t sortCalls;
    uint64_t nodeAppendCalls;
    int maxRangeBoundsCount;
    int maxSortCount;
    uint64_t nodeBytes;
    uint64_t indexBytes;
    uint64_t centroidBytes;
    uint64_t triangleBoundsMinBytes;
    uint64_t triangleBoundsMaxBytes;
    uint64_t sortScratchBytes;
    uint64_t buildScratchBytes;
    uint64_t totalBytes;
    int* indices;
    Vec3* centroids;
    RuntimeTriangleBVH3DBoundsF* triangleBoundsMin;
    RuntimeTriangleBVH3DBoundsF* triangleBoundsMax;
    int indexCount;
};

bool runtime_triangle_bvh_vec3_isfinite(Vec3 value);
void runtime_triangle_bvh_destroy(RuntimeTriangleBVH3D* bvh);

#endif
