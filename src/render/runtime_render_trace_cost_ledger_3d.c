#include "render/runtime_render_trace_cost_ledger_3d.h"

#include "material/material.h"
#include "render/runtime_material_payload_3d.h"

#include <stdlib.h>
#include <string.h>

static RuntimeRenderTraceCostLedger3D gRuntimeRenderTraceCostLedger3D;

const char* RuntimeRenderTraceCostRayClass3DLabel(RuntimeRenderTraceCostRayClass3D ray_class) {
    switch (ray_class) {
        case RUNTIME_RENDER_TRACE_COST_RAY_PRIMARY:
            return "primary";
        case RUNTIME_RENDER_TRACE_COST_RAY_DIRECT_LIGHT_VISIBILITY:
            return "direct_light_visibility";
        case RUNTIME_RENDER_TRACE_COST_RAY_TRANSMISSION:
            return "transmission";
        case RUNTIME_RENDER_TRACE_COST_RAY_REFLECTION_SPECULAR:
            return "reflection_specular";
        case RUNTIME_RENDER_TRACE_COST_RAY_DIFFUSE_SECONDARY:
            return "diffuse_secondary";
        case RUNTIME_RENDER_TRACE_COST_RAY_DISNEY_RECURSIVE:
            return "disney_recursive";
        case RUNTIME_RENDER_TRACE_COST_RAY_CAUSTIC:
            return "caustic";
        case RUNTIME_RENDER_TRACE_COST_RAY_EMISSIVE_AREA:
            return "emissive_area";
        case RUNTIME_RENDER_TRACE_COST_RAY_UNKNOWN:
        default:
            return "unknown";
    }
}

const char* RuntimeRenderTraceCostMaterialFamily3DLabel(
    RuntimeRenderTraceCostMaterialFamily3D family) {
    switch (family) {
        case RUNTIME_RENDER_TRACE_COST_MATERIAL_PRIMITIVE_OPAQUE:
            return "primitive_opaque";
        case RUNTIME_RENDER_TRACE_COST_MATERIAL_RUNTIME_MESH_OPAQUE:
            return "runtime_mesh_opaque";
        case RUNTIME_RENDER_TRACE_COST_MATERIAL_PRIMITIVE_TRANSPARENT:
            return "primitive_transparent";
        case RUNTIME_RENDER_TRACE_COST_MATERIAL_RUNTIME_MESH_TRANSPARENT:
            return "runtime_mesh_transparent";
        case RUNTIME_RENDER_TRACE_COST_MATERIAL_MIRROR_SPECULAR:
            return "mirror_specular";
        case RUNTIME_RENDER_TRACE_COST_MATERIAL_METAL_GLOSSY:
            return "metal_glossy";
        case RUNTIME_RENDER_TRACE_COST_MATERIAL_EMISSIVE:
            return "emissive";
        case RUNTIME_RENDER_TRACE_COST_MATERIAL_UNKNOWN:
        default:
            return "unknown";
    }
}

const char* RuntimeRenderTraceCostPathDepthBucket3DLabel(
    RuntimeRenderTraceCostPathDepthBucket3D bucket) {
    switch (bucket) {
        case RUNTIME_RENDER_TRACE_COST_DEPTH_0:
            return "depth_0";
        case RUNTIME_RENDER_TRACE_COST_DEPTH_1:
            return "depth_1";
        case RUNTIME_RENDER_TRACE_COST_DEPTH_2:
            return "depth_2";
        case RUNTIME_RENDER_TRACE_COST_DEPTH_3_PLUS:
            return "depth_3_plus";
        default:
            return "unknown";
    }
}

void RuntimeRenderTraceCostLedger3D_SetEnabled(bool enabled) {
    gRuntimeRenderTraceCostLedger3D.enabled = enabled;
}

void RuntimeRenderTraceCostLedger3D_SetEnabledFromEnvironment(void) {
    const char* value = getenv("RAY_TRACING_RENDER_TRACE_COST_LEDGER");
    RuntimeRenderTraceCostLedger3D_SetEnabled(value && value[0] != '\0' && value[0] != '0');
}

bool RuntimeRenderTraceCostLedger3D_IsEnabled(void) {
    return gRuntimeRenderTraceCostLedger3D.enabled;
}

void RuntimeRenderTraceCostLedger3D_Reset(void) {
    const bool enabled = gRuntimeRenderTraceCostLedger3D.enabled;
    memset(&gRuntimeRenderTraceCostLedger3D, 0, sizeof(gRuntimeRenderTraceCostLedger3D));
    gRuntimeRenderTraceCostLedger3D.enabled = enabled;
}

void RuntimeRenderTraceCostLedger3D_RecordRay(RuntimeRenderTraceCostRayClass3D ray_class) {
    RuntimeRenderTraceCostLedger3D_RecordRayAtDepth(ray_class, 0);
}

static RuntimeRenderTraceCostPathDepthBucket3D runtime_render_trace_cost_depth_bucket(
    int path_depth) {
    if (path_depth <= 0) return RUNTIME_RENDER_TRACE_COST_DEPTH_0;
    if (path_depth == 1) return RUNTIME_RENDER_TRACE_COST_DEPTH_1;
    if (path_depth == 2) return RUNTIME_RENDER_TRACE_COST_DEPTH_2;
    return RUNTIME_RENDER_TRACE_COST_DEPTH_3_PLUS;
}

void RuntimeRenderTraceCostLedger3D_RecordRayAtDepth(
    RuntimeRenderTraceCostRayClass3D ray_class,
    int path_depth) {
    RuntimeRenderTraceCostPathDepthBucket3D depth_bucket =
        runtime_render_trace_cost_depth_bucket(path_depth);
    if (!gRuntimeRenderTraceCostLedger3D.enabled) return;
    if (ray_class < 0 || ray_class >= RUNTIME_RENDER_TRACE_COST_RAY_CLASS_COUNT) {
        ray_class = RUNTIME_RENDER_TRACE_COST_RAY_UNKNOWN;
    }
    gRuntimeRenderTraceCostLedger3D.totalRays += 1u;
    gRuntimeRenderTraceCostLedger3D.rayClassCounts[ray_class] += 1u;
    gRuntimeRenderTraceCostLedger3D.pathDepthCounts[depth_bucket] += 1u;
    gRuntimeRenderTraceCostLedger3D.rayClassDepthCounts[ray_class][depth_bucket] += 1u;
}

static bool runtime_render_trace_cost_hit_is_runtime_mesh(const HitInfo3D* hit) {
    return hit && hit->source.kind == RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
}

static RuntimeRenderTraceCostMaterialFamily3D
runtime_render_trace_cost_material_family_from_hit(const HitInfo3D* hit,
                                                   const RuntimeMaterialPayload3D* payload) {
    const bool runtime_mesh = runtime_render_trace_cost_hit_is_runtime_mesh(hit);
    if (!hit || !payload || !payload->valid) {
        return RUNTIME_RENDER_TRACE_COST_MATERIAL_UNKNOWN;
    }
    if (payload->emissive > 0.0 || payload->bsdf.emissive > 0.0 ||
        payload->materialId == MATERIAL_PRESET_EMISSIVE) {
        return RUNTIME_RENDER_TRACE_COST_MATERIAL_EMISSIVE;
    }
    if (payload->materialId == MATERIAL_PRESET_MIRROR ||
        payload->bsdf.reflectivity >= 0.85) {
        return RUNTIME_RENDER_TRACE_COST_MATERIAL_MIRROR_SPECULAR;
    }
    if (payload->materialId == MATERIAL_PRESET_ROUGH_METAL ||
        payload->materialId == MATERIAL_PRESET_GLOSSY) {
        return RUNTIME_RENDER_TRACE_COST_MATERIAL_METAL_GLOSSY;
    }
    if (payload->transparency > 0.0 ||
        payload->materialId == MATERIAL_PRESET_TRANSPARENT) {
        return runtime_mesh ? RUNTIME_RENDER_TRACE_COST_MATERIAL_RUNTIME_MESH_TRANSPARENT
                            : RUNTIME_RENDER_TRACE_COST_MATERIAL_PRIMITIVE_TRANSPARENT;
    }
    return runtime_mesh ? RUNTIME_RENDER_TRACE_COST_MATERIAL_RUNTIME_MESH_OPAQUE
                        : RUNTIME_RENDER_TRACE_COST_MATERIAL_PRIMITIVE_OPAQUE;
}

void RuntimeRenderTraceCostLedger3D_RecordHitMaterialFamily(const HitInfo3D* hit) {
    RuntimeMaterialPayload3D payload = {0};
    RuntimeRenderTraceCostMaterialFamily3D family =
        RUNTIME_RENDER_TRACE_COST_MATERIAL_UNKNOWN;
    if (!gRuntimeRenderTraceCostLedger3D.enabled) return;
    if (RuntimeMaterialPayload3D_ResolveFromHit(hit, &payload)) {
        family = runtime_render_trace_cost_material_family_from_hit(hit, &payload);
    }
    if (family < 0 || family >= RUNTIME_RENDER_TRACE_COST_MATERIAL_COUNT) {
        family = RUNTIME_RENDER_TRACE_COST_MATERIAL_UNKNOWN;
    }
    gRuntimeRenderTraceCostLedger3D.materialFamilyCounts[family] += 1u;
}

void RuntimeRenderTraceCostLedger3D_Snapshot(RuntimeRenderTraceCostLedger3D* out_ledger) {
    if (!out_ledger) return;
    *out_ledger = gRuntimeRenderTraceCostLedger3D;
}
