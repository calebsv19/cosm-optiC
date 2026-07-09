#include "render/runtime_disney_v2_caustic_sidecar_3d.h"

#include <math.h>
#include <float.h>
#include <string.h>

#include "render/runtime_light_set_3d.h"
#include "render/runtime_material_payload_3d.h"
#include "scene/object_manager.h"

static RuntimeDisneyV2CausticMode3D g_caustic_mode =
    RUNTIME_DISNEY_V2_CAUSTIC_MODE_ANALYTIC;
static double g_caustic_sidecar_strength = 1.0;
static RuntimeDisneyV2CausticSidecarDiagnostics3D g_caustic_sidecar_diagnostics = {0};

static double caustic_sidecar_clamp(double value, double min_value, double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

const char* RuntimeDisneyV2_3D_CausticModeLabel(RuntimeDisneyV2CausticMode3D mode) {
    switch (mode) {
        case RUNTIME_DISNEY_V2_CAUSTIC_MODE_OFF:
            return "off";
        case RUNTIME_DISNEY_V2_CAUSTIC_MODE_ANALYTIC:
            return "analytic";
        case RUNTIME_DISNEY_V2_CAUSTIC_MODE_TRANSPORT:
            return "transport";
        default:
            return "unknown";
    }
}

void RuntimeDisneyV2_3D_SetCausticSidecar(bool enabled, double strength) {
    RuntimeDisneyV2_3D_SetCausticMode(enabled ? RUNTIME_DISNEY_V2_CAUSTIC_MODE_ANALYTIC
                                              : RUNTIME_DISNEY_V2_CAUSTIC_MODE_OFF,
                                      strength);
}

void RuntimeDisneyV2_3D_SetCausticMode(RuntimeDisneyV2CausticMode3D mode,
                                       double strength) {
    if (mode != RUNTIME_DISNEY_V2_CAUSTIC_MODE_OFF &&
        mode != RUNTIME_DISNEY_V2_CAUSTIC_MODE_ANALYTIC &&
        mode != RUNTIME_DISNEY_V2_CAUSTIC_MODE_TRANSPORT) {
        mode = RUNTIME_DISNEY_V2_CAUSTIC_MODE_ANALYTIC;
    }
    g_caustic_mode = mode;
    g_caustic_sidecar_strength = caustic_sidecar_clamp(strength, 0.0, 16.0);
}

bool RuntimeDisneyV2_3D_CausticSidecarEnabled(void) {
    return g_caustic_mode == RUNTIME_DISNEY_V2_CAUSTIC_MODE_ANALYTIC;
}

RuntimeDisneyV2CausticMode3D RuntimeDisneyV2_3D_CausticMode(void) {
    return g_caustic_mode;
}

double RuntimeDisneyV2_3D_CausticSidecarStrength(void) {
    return g_caustic_sidecar_strength;
}

void RuntimeDisneyV2_3D_ResetCausticSidecarDiagnostics(void) {
    memset(&g_caustic_sidecar_diagnostics, 0, sizeof(g_caustic_sidecar_diagnostics));
}

void RuntimeDisneyV2_3D_SnapshotCausticSidecarDiagnostics(
    RuntimeDisneyV2CausticSidecarDiagnostics3D* out_diagnostics) {
    if (!out_diagnostics) return;
    *out_diagnostics = g_caustic_sidecar_diagnostics;
}

static bool caustic_sidecar_scene_object_is_transmissive(int scene_object_index) {
    RuntimeMaterialPayload3D payload = {0};
    g_caustic_sidecar_diagnostics.materialResolveCount += 1u;
    if (!RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(scene_object_index, &payload)) {
        return false;
    }
    return payload.valid && payload.transparency > 0.01;
}

static bool caustic_sidecar_scene_object_is_transmissive_cached(
    const RuntimeScene3D* scene,
    int scene_object_index,
    signed char transmissive_cache[MAX_OBJECTS]) {
    if (scene_object_index < 0 || scene_object_index >= MAX_OBJECTS) {
        return false;
    }
    if (transmissive_cache[scene_object_index] < 0) {
        g_caustic_sidecar_diagnostics.objectTransmissiveLookupCount += 1u;
        if (scene &&
            scene->objectMaterialSummariesValid &&
            scene->objectMaterialSummaries[scene_object_index].seen &&
            scene->objectMaterialSummaries[scene_object_index].resolved) {
            transmissive_cache[scene_object_index] =
                (scene->objectMaterialSummaries[scene_object_index].valid &&
                 scene->objectMaterialSummaries[scene_object_index].transparency > 0.01)
                    ? 1
                    : 0;
        } else {
            transmissive_cache[scene_object_index] =
                caustic_sidecar_scene_object_is_transmissive(scene_object_index) ? 1 : 0;
        }
    }
    return transmissive_cache[scene_object_index] > 0;
}

static bool caustic_sidecar_resolve_light(const RuntimeScene3D* scene,
                                          RuntimeDisneyV2CausticSidecarProbe3D* probe) {
    const RuntimeLightSource3D* source = NULL;
    if (!scene || !probe) return false;
    source = RuntimeLightSet3D_GetEnabled(&scene->lightSet, 0);
    if (source) {
        probe->lightPosition = source->position;
        probe->lightColor = source->color;
        probe->lightIntensity = source->intensity;
        return probe->lightIntensity > 0.0;
    }
    if (scene->hasLight && scene->light.intensity > 0.0) {
        probe->lightPosition = scene->light.position;
        probe->lightColor = vec3(1.0, 1.0, 1.0);
        probe->lightIntensity = scene->light.intensity;
        return true;
    }
    return false;
}

static bool runtime_disney_v2_3d_build_caustic_sidecar_probe(
    const RuntimeScene3D* scene,
    RuntimeDisneyV2CausticSidecarProbe3D* out_probe,
    bool require_analytic_mode) {
    RuntimeDisneyV2CausticSidecarProbe3D probe = {0};
    Vec3 min_bound = vec3(DBL_MAX, DBL_MAX, DBL_MAX);
    Vec3 max_bound = vec3(-DBL_MAX, -DBL_MAX, -DBL_MAX);
    int selected_scene_object = -1;
    int selected_triangle_count = 0;
    signed char transmissive_cache[MAX_OBJECTS];

    if (!out_probe) return false;
    *out_probe = probe;
    g_caustic_sidecar_diagnostics.probeBuildCount += 1u;
    if ((require_analytic_mode && g_caustic_mode != RUNTIME_DISNEY_V2_CAUSTIC_MODE_ANALYTIC) ||
        !scene ||
        !scene->capabilities.hasTransmissionSurfaces ||
        scene->triangleMesh.triangleCount <= 0 || !scene->triangleMesh.triangles) {
        return false;
    }
    if (!caustic_sidecar_resolve_light(scene, &probe)) {
        return false;
    }

    memset(transmissive_cache, -1, sizeof(transmissive_cache));
    for (int i = 0; i < scene->triangleMesh.triangleCount; ++i) {
        const RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[i];
        const int scene_object_index = triangle->sceneObjectIndex;
        const Vec3 points[3] = {triangle->p0, triangle->p1, triangle->p2};
        g_caustic_sidecar_diagnostics.triangleScanCount += 1u;
        if (scene_object_index < 0) continue;
        if (selected_scene_object >= 0 && scene_object_index != selected_scene_object) {
            continue;
        }
        if (!caustic_sidecar_scene_object_is_transmissive_cached(scene,
                                                                 scene_object_index,
                                                                 transmissive_cache)) {
            continue;
        }
        if (selected_scene_object < 0) {
            selected_scene_object = scene_object_index;
        }
        for (int p = 0; p < 3; ++p) {
            if (points[p].x < min_bound.x) min_bound.x = points[p].x;
            if (points[p].y < min_bound.y) min_bound.y = points[p].y;
            if (points[p].z < min_bound.z) min_bound.z = points[p].z;
            if (points[p].x > max_bound.x) max_bound.x = points[p].x;
            if (points[p].y > max_bound.y) max_bound.y = points[p].y;
            if (points[p].z > max_bound.z) max_bound.z = points[p].z;
        }
        selected_triangle_count += 1;
    }

    if (selected_scene_object < 0 || selected_triangle_count <= 0) {
        return false;
    }

    probe.center = vec3((min_bound.x + max_bound.x) * 0.5,
                        (min_bound.y + max_bound.y) * 0.5,
                        (min_bound.z + max_bound.z) * 0.5);
    probe.radius = fmax(fmax(max_bound.x - min_bound.x, max_bound.y - min_bound.y),
                        max_bound.z - min_bound.z) * 0.5;
    probe.receiverZ = min_bound.z - probe.radius;
    probe.strength = g_caustic_sidecar_strength;
    probe.valid = probe.radius > 1.0e-6;
    *out_probe = probe;
    return probe.valid;
}

bool RuntimeDisneyV2_3D_BuildCausticSidecarProbe(
    const RuntimeScene3D* scene,
    RuntimeDisneyV2CausticSidecarProbe3D* out_probe) {
    return runtime_disney_v2_3d_build_caustic_sidecar_probe(scene, out_probe, true);
}

bool RuntimeDisneyV2_3D_BuildCausticSidecarProbeForSpatialCache(
    const RuntimeScene3D* scene,
    RuntimeDisneyV2CausticSidecarProbe3D* out_probe) {
    return runtime_disney_v2_3d_build_caustic_sidecar_probe(scene, out_probe, false);
}

bool RuntimeDisneyV2_3D_EvaluateCausticSidecar(
    const RuntimeDisneyV2CausticSidecarProbe3D* probe,
    const HitInfo3D* receiver_hit,
    RuntimeDisneyV2CausticSidecarContribution3D* out_contribution) {
    RuntimeDisneyV2CausticSidecarContribution3D contribution = {0};
    double dx = 0.0;
    double dy = 0.0;
    double radius = 0.0;
    double d2 = 0.0;
    double focus = 0.0;
    double height_gain = 0.0;
    double luma = 0.0;

    if (!out_contribution) return false;
    *out_contribution = contribution;
    if (!probe || !probe->valid || !receiver_hit || probe->strength <= 0.0) {
        return false;
    }
    if (receiver_hit->position.z > probe->center.z - probe->radius * 0.25) {
        return false;
    }

    dx = receiver_hit->position.x - probe->center.x;
    dy = receiver_hit->position.y - probe->center.y;
    radius = caustic_sidecar_clamp(probe->radius * 0.34, 0.035, 0.5);
    d2 = dx * dx + dy * dy;
    focus = exp(-d2 / (2.0 * radius * radius));
    if (focus < 1.0e-4) {
        return false;
    }

    height_gain = caustic_sidecar_clamp((probe->lightPosition.z - probe->center.z) /
                                            fmax(probe->radius, 1.0e-6),
                                        0.25,
                                        5.0);
    luma = probe->lightIntensity * probe->strength * focus * height_gain * 0.035;
    contribution.r = luma * probe->lightColor.x;
    contribution.g = luma * probe->lightColor.y;
    contribution.b = luma * probe->lightColor.z;
    contribution.luma = fmax(fmax(contribution.r, contribution.g), contribution.b);
    *out_contribution = contribution;
    return contribution.luma > 1.0e-9;
}
