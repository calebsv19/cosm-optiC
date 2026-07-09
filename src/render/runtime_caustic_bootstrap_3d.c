#include "render/runtime_caustic_bootstrap_3d.h"

#include <math.h>
#include <string.h>

static RuntimeCausticBootstrap3DRequestState g_caustic_bootstrap_state = {0};

static double runtime_caustic_bootstrap_clamp(double value, double min_value, double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static Vec3 runtime_caustic_bootstrap_cell_center(const RuntimeVolumeGrid3D* grid,
                                                  uint32_t x,
                                                  uint32_t y,
                                                  uint32_t z) {
    const double step = grid ? grid->voxelSize : 0.0;
    return vec3(grid->origin.x + (((double)x + 0.5) * step),
                grid->origin.y + (((double)y + 0.5) * step),
                grid->origin.z + (((double)z + 0.5) * step));
}

void RuntimeCausticBootstrap3D_ResetRequestState(void) {
    memset(&g_caustic_bootstrap_state, 0, sizeof(g_caustic_bootstrap_state));
    g_caustic_bootstrap_state.mode = RUNTIME_CAUSTIC_MODE_OFF;
}

void RuntimeCausticBootstrap3D_SetRequestState(const RuntimeCausticSettings3D* settings) {
    RuntimeCausticSettings3D defaults;
    const RuntimeCausticSettings3D* src = settings;
    if (!src) {
        RuntimeCausticSettings3D_Default(&defaults);
        src = &defaults;
    }
    memset(&g_caustic_bootstrap_state, 0, sizeof(g_caustic_bootstrap_state));
    g_caustic_bootstrap_state.mode = src->mode;
    g_caustic_bootstrap_state.volumeCacheRequested = src->volumeCacheEnabled;
    g_caustic_bootstrap_state.sampleBudget = src->sampleBudget;
    g_caustic_bootstrap_state.temporaryAnalyticBridge =
        src->mode == RUNTIME_CAUSTIC_MODE_SPATIAL_CACHE && src->volumeCacheEnabled;
    g_caustic_bootstrap_state.enabled =
        g_caustic_bootstrap_state.temporaryAnalyticBridge;
}

RuntimeCausticBootstrap3DRequestState RuntimeCausticBootstrap3D_RequestState(void) {
    return g_caustic_bootstrap_state;
}

bool RuntimeCausticBootstrap3D_PopulateAnalyticVolumeCache(
    const RuntimeScene3D* scene,
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticBootstrap3DDiagnostics* out_diagnostics) {
    RuntimeCausticBootstrap3DDiagnostics diagnostics = {0};
    RuntimeDisneyV2CausticSidecarProbe3D probe = {0};
    Vec3 funnel_start = vec3(0.0, 0.0, 0.0);
    Vec3 funnel_end = vec3(0.0, 0.0, 0.0);
    Vec3 axis = vec3(0.0, 0.0, 0.0);
    double axis_len2 = 0.0;
    double axis_len = 0.0;
    uint64_t accepted_budget = 0u;

    if (out_diagnostics) *out_diagnostics = diagnostics;
    diagnostics.requested = g_caustic_bootstrap_state.enabled;
    diagnostics.enabled = g_caustic_bootstrap_state.enabled;
    diagnostics.temporaryAnalyticBridge = g_caustic_bootstrap_state.temporaryAnalyticBridge;
    if (!g_caustic_bootstrap_state.enabled || !scene || !cache) {
        if (out_diagnostics) *out_diagnostics = diagnostics;
        return false;
    }
    if (!scene->volume.enabled || !scene->volume.hasData ||
        !RuntimeVolumeGrid3D_IsConfigured(&scene->volume.grid)) {
        if (out_diagnostics) *out_diagnostics = diagnostics;
        return false;
    }
    if (!RuntimeDisneyV2_3D_BuildCausticSidecarProbeForSpatialCache(scene, &probe)) {
        if (out_diagnostics) *out_diagnostics = diagnostics;
        return false;
    }
    diagnostics.probeBuilt = true;
    diagnostics.probe = probe;

    if (!RuntimeCausticVolumeCache3D_AllocateFromVolume(cache, &scene->volume)) {
        RuntimeCausticVolumeCache3D_SnapshotDiagnostics(cache, &diagnostics.cache);
        if (out_diagnostics) *out_diagnostics = diagnostics;
        return false;
    }
    diagnostics.cacheAllocated = true;

    funnel_start = probe.center;
    funnel_end = vec3(probe.center.x, probe.center.y, probe.receiverZ);
    axis = vec3_sub(funnel_end, funnel_start);
    axis_len2 = vec3_dot(axis, axis);
    axis_len = sqrt(axis_len2);
    diagnostics.funnelStart = funnel_start;
    diagnostics.funnelEnd = funnel_end;
    diagnostics.funnelLength = axis_len;
    diagnostics.sourceRadius = runtime_caustic_bootstrap_clamp(probe.radius * 0.20, 0.025, 0.35);
    diagnostics.receiverRadius = runtime_caustic_bootstrap_clamp(probe.radius * 0.42, 0.050, 0.65);

    if (!(axis_len2 > 1.0e-12)) {
        RuntimeCausticVolumeCache3D_SnapshotDiagnostics(cache, &diagnostics.cache);
        if (out_diagnostics) *out_diagnostics = diagnostics;
        return false;
    }
    if (g_caustic_bootstrap_state.sampleBudget > 0) {
        accepted_budget = (uint64_t)g_caustic_bootstrap_state.sampleBudget;
    }

    for (uint32_t z = 0; z < scene->volume.grid.gridD; ++z) {
        for (uint32_t y = 0; y < scene->volume.grid.gridH; ++y) {
            for (uint32_t x = 0; x < scene->volume.grid.gridW; ++x) {
                const Vec3 p = runtime_caustic_bootstrap_cell_center(&scene->volume.grid, x, y, z);
                const Vec3 from_start = vec3_sub(p, funnel_start);
                const double t = vec3_dot(from_start, axis) / axis_len2;
                double radius = 0.0;
                double radial2 = 0.0;
                double focus = 0.0;
                double energy = 0.0;
                Vec3 closest = vec3(0.0, 0.0, 0.0);
                Vec3 radial = vec3(0.0, 0.0, 0.0);

                diagnostics.candidateCellCount += 1u;
                if (accepted_budget > 0u &&
                    diagnostics.depositAcceptedCount >= accepted_budget) {
                    continue;
                }
                if (t < 0.0 || t > 1.0) {
                    continue;
                }
                radius = diagnostics.sourceRadius +
                         ((diagnostics.receiverRadius - diagnostics.sourceRadius) * t);
                closest = vec3_add(funnel_start, vec3_scale(axis, t));
                radial = vec3_sub(p, closest);
                radial2 = vec3_dot(radial, radial);
                focus = exp(-radial2 / (2.0 * radius * radius));
                if (focus < 1.0e-4) {
                    continue;
                }
                energy = probe.lightIntensity * probe.strength * focus *
                         (1.0 - 0.35 * t) * 0.020;
                diagnostics.depositAttemptCount += 1u;
                if (RuntimeCausticVolumeCache3D_DepositAtPosition(
                        cache,
                        p,
                        energy * probe.lightColor.x,
                        energy * probe.lightColor.y,
                        energy * probe.lightColor.z)) {
                    diagnostics.depositAcceptedCount += 1u;
                    diagnostics.totalRadianceR += energy * probe.lightColor.x;
                    diagnostics.totalRadianceG += energy * probe.lightColor.y;
                    diagnostics.totalRadianceB += energy * probe.lightColor.z;
                    if (energy > diagnostics.maxRadiance) diagnostics.maxRadiance = energy;
                } else {
                    diagnostics.depositRejectedCount += 1u;
                }
            }
        }
    }

    RuntimeCausticVolumeCache3D_SnapshotDiagnostics(cache, &diagnostics.cache);
    diagnostics.populated = diagnostics.cache.nonZeroCellCount > 0u;
    if (out_diagnostics) *out_diagnostics = diagnostics;
    return diagnostics.populated;
}
