#ifndef RENDER_RUNTIME_RENDER_TRACE_COST_LEDGER_3D_H
#define RENDER_RUNTIME_RENDER_TRACE_COST_LEDGER_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_ray_3d.h"

typedef enum RuntimeRenderTraceCostRayClass3D {
    RUNTIME_RENDER_TRACE_COST_RAY_PRIMARY = 0,
    RUNTIME_RENDER_TRACE_COST_RAY_DIRECT_LIGHT_VISIBILITY = 1,
    RUNTIME_RENDER_TRACE_COST_RAY_TRANSMISSION = 2,
    RUNTIME_RENDER_TRACE_COST_RAY_REFLECTION_SPECULAR = 3,
    RUNTIME_RENDER_TRACE_COST_RAY_DIFFUSE_SECONDARY = 4,
    RUNTIME_RENDER_TRACE_COST_RAY_DISNEY_RECURSIVE = 5,
    RUNTIME_RENDER_TRACE_COST_RAY_CAUSTIC = 6,
    RUNTIME_RENDER_TRACE_COST_RAY_EMISSIVE_AREA = 7,
    RUNTIME_RENDER_TRACE_COST_RAY_UNKNOWN = 8,
    RUNTIME_RENDER_TRACE_COST_RAY_CLASS_COUNT = 9
} RuntimeRenderTraceCostRayClass3D;

typedef enum RuntimeRenderTraceCostMaterialFamily3D {
    RUNTIME_RENDER_TRACE_COST_MATERIAL_UNKNOWN = 0,
    RUNTIME_RENDER_TRACE_COST_MATERIAL_PRIMITIVE_OPAQUE = 1,
    RUNTIME_RENDER_TRACE_COST_MATERIAL_RUNTIME_MESH_OPAQUE = 2,
    RUNTIME_RENDER_TRACE_COST_MATERIAL_PRIMITIVE_TRANSPARENT = 3,
    RUNTIME_RENDER_TRACE_COST_MATERIAL_RUNTIME_MESH_TRANSPARENT = 4,
    RUNTIME_RENDER_TRACE_COST_MATERIAL_MIRROR_SPECULAR = 5,
    RUNTIME_RENDER_TRACE_COST_MATERIAL_METAL_GLOSSY = 6,
    RUNTIME_RENDER_TRACE_COST_MATERIAL_EMISSIVE = 7,
    RUNTIME_RENDER_TRACE_COST_MATERIAL_COUNT = 8
} RuntimeRenderTraceCostMaterialFamily3D;

typedef enum RuntimeRenderTraceCostPathDepthBucket3D {
    RUNTIME_RENDER_TRACE_COST_DEPTH_0 = 0,
    RUNTIME_RENDER_TRACE_COST_DEPTH_1 = 1,
    RUNTIME_RENDER_TRACE_COST_DEPTH_2 = 2,
    RUNTIME_RENDER_TRACE_COST_DEPTH_3_PLUS = 3,
    RUNTIME_RENDER_TRACE_COST_DEPTH_BUCKET_COUNT = 4
} RuntimeRenderTraceCostPathDepthBucket3D;

typedef struct RuntimeRenderTraceCostLedger3D {
    bool enabled;
    uint64_t totalRays;
    uint64_t rayClassCounts[RUNTIME_RENDER_TRACE_COST_RAY_CLASS_COUNT];
    uint64_t pathDepthCounts[RUNTIME_RENDER_TRACE_COST_DEPTH_BUCKET_COUNT];
    uint64_t rayClassDepthCounts[RUNTIME_RENDER_TRACE_COST_RAY_CLASS_COUNT]
                                [RUNTIME_RENDER_TRACE_COST_DEPTH_BUCKET_COUNT];
    uint64_t materialFamilyCounts[RUNTIME_RENDER_TRACE_COST_MATERIAL_COUNT];
} RuntimeRenderTraceCostLedger3D;

const char* RuntimeRenderTraceCostRayClass3DLabel(RuntimeRenderTraceCostRayClass3D ray_class);
const char* RuntimeRenderTraceCostMaterialFamily3DLabel(
    RuntimeRenderTraceCostMaterialFamily3D family);
const char* RuntimeRenderTraceCostPathDepthBucket3DLabel(
    RuntimeRenderTraceCostPathDepthBucket3D bucket);
void RuntimeRenderTraceCostLedger3D_SetEnabled(bool enabled);
void RuntimeRenderTraceCostLedger3D_SetEnabledFromEnvironment(void);
bool RuntimeRenderTraceCostLedger3D_IsEnabled(void);
void RuntimeRenderTraceCostLedger3D_Reset(void);
void RuntimeRenderTraceCostLedger3D_RecordRay(RuntimeRenderTraceCostRayClass3D ray_class);
void RuntimeRenderTraceCostLedger3D_RecordRayAtDepth(
    RuntimeRenderTraceCostRayClass3D ray_class,
    int path_depth);
void RuntimeRenderTraceCostLedger3D_RecordHitMaterialFamily(const HitInfo3D* hit);
void RuntimeRenderTraceCostLedger3D_Snapshot(RuntimeRenderTraceCostLedger3D* out_ledger);

#endif
