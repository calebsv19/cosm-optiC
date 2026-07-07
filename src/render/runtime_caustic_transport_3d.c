#include "render/runtime_caustic_transport_3d.h"

#include <math.h>
#include <string.h>

#include "render/runtime_material_payload_3d.h"
#include "render/runtime_caustic_transport_debug_3d.h"
#include "render/runtime_caustic_transport_internal_3d.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_render_trace_cost_ledger_3d.h"
#include "render/runtime_volume_3d_sampling.h"

static RuntimeCausticTransport3DRequestState g_caustic_transport_state = {0};
static RuntimeCausticTransportSurfaceReceiverContext3D g_caustic_transport_surface_context = {0};

void RuntimeCausticTransport3D_ResetRequestState(void) {
    memset(&g_caustic_transport_state, 0, sizeof(g_caustic_transport_state));
    g_caustic_transport_state.mode = RUNTIME_CAUSTIC_MODE_OFF;
    g_caustic_transport_state.surfaceRadianceScale = 1.0;
    g_caustic_transport_state.surfaceFootprintScale = 1.0;
    g_caustic_transport_state.surfaceReceiverFallbackEnabled = true;
    RuntimeCausticTransportDebug3D_Reset();
}

void RuntimeCausticTransport3D_SetRequestState(const RuntimeCausticSettings3D* settings) {
    RuntimeCausticSettings3D defaults;
    const RuntimeCausticSettings3D* src = settings;
    if (!src) {
        RuntimeCausticSettings3D_Default(&defaults);
        src = &defaults;
    }
    memset(&g_caustic_transport_state, 0, sizeof(g_caustic_transport_state));
    g_caustic_transport_state.mode = src->mode;
    g_caustic_transport_state.volumeCacheRequested = src->volumeCacheEnabled;
    g_caustic_transport_state.surfaceCacheRequested = src->surfaceCacheEnabled;
    g_caustic_transport_state.sampleBudget = src->sampleBudget;
    g_caustic_transport_state.maxPathDepth = src->maxPathDepth;
    g_caustic_transport_state.emissionPolicy = src->emissionPolicy;
    g_caustic_transport_state.surfaceRadianceScale =
        runtime_caustic_transport_clamp(src->surfaceRadianceScale, 0.0, 128.0);
    g_caustic_transport_state.surfaceFootprintScale =
        runtime_caustic_transport_clamp(src->surfaceFootprintScale, 0.1, 16.0);
    g_caustic_transport_state.surfaceReceiverFallbackEnabled =
        src->surfaceReceiverFallbackEnabled;
    g_caustic_transport_state.debugExportEnabled = src->debugExportEnabled;
    RuntimeCausticTransportDebug3D_SetEnabled(src->debugExportEnabled);
    RuntimeCausticTransportDebug3D_BeginFrame();
    g_caustic_transport_state.enabled =
        src->mode == RUNTIME_CAUSTIC_MODE_TRANSPORT &&
        (src->volumeCacheEnabled || src->surfaceCacheEnabled);
}

RuntimeCausticTransport3DRequestState RuntimeCausticTransport3D_RequestState(void) {
    return g_caustic_transport_state;
}

bool RuntimeCausticTransport3D_PopulateVolumeCache(
    const RuntimeScene3D* scene,
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticTransport3DDiagnostics* out_diagnostics) {
    return RuntimeCausticTransport3D_PopulateCaches(scene, cache, NULL, out_diagnostics);
}

bool RuntimeCausticTransport3D_PopulateCaches(
    const RuntimeScene3D* scene,
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticSurfaceCache3D* surface_cache,
    RuntimeCausticTransport3DDiagnostics* out_diagnostics) {
    RuntimeCausticTransport3DDiagnostics diagnostics = {0};
    int enabled_light_count = 0;
    int path_budget = RUNTIME_CAUSTIC_TRANSPORT_DEFAULT_PATH_BUDGET;
    bool volume_cache_active = false;
    RuntimeLightSource3D compat_light;
    RuntimeCausticTransportAnalyticSphere3D analytic_sphere = {0};
    RuntimeCausticTransportAnalyticCylinder3D analytic_cylinder = {0};
    RuntimeCausticTransportAnalyticPrism3D analytic_prism = {0};
    RuntimeCausticTransportAnalyticBowl3D analytic_bowl = {0};
    bool use_analytic_sphere_lens =
        g_caustic_transport_state.emissionPolicy ==
        RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_SPHERE_LENS;
    bool use_analytic_cylinder_lens =
        g_caustic_transport_state.emissionPolicy ==
            RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_CYLINDER_LENS ||
        g_caustic_transport_state.emissionPolicy ==
            RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_CYLINDER_LENS_FOCUSED;
    bool use_focused_cylinder_lens =
        g_caustic_transport_state.emissionPolicy ==
        RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_CYLINDER_LENS_FOCUSED;
    bool use_analytic_prism_lens =
        g_caustic_transport_state.emissionPolicy ==
        RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_PRISM_LENS;
    bool use_analytic_bowl_lens =
        g_caustic_transport_state.emissionPolicy ==
        RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_BOWL_LENS;

    if (out_diagnostics) *out_diagnostics = diagnostics;
    diagnostics.requested = g_caustic_transport_state.enabled;
    diagnostics.active = g_caustic_transport_state.enabled;
    if (!g_caustic_transport_state.enabled || !scene) {
        if (out_diagnostics) *out_diagnostics = diagnostics;
        return false;
    }
    RuntimeCausticTransportDebug3D_BeginFrame();
    volume_cache_active = g_caustic_transport_state.volumeCacheRequested &&
                          cache &&
                          RuntimeVolume3D_HasSampleableDensity(&scene->volume);
    diagnostics.volumeCacheSuppressedNoSampleableVolume =
        g_caustic_transport_state.volumeCacheRequested && !volume_cache_active;
    if (g_caustic_transport_state.volumeCacheRequested &&
        !volume_cache_active &&
        !g_caustic_transport_state.surfaceCacheRequested) {
        if (out_diagnostics) *out_diagnostics = diagnostics;
        return false;
    }
    if (g_caustic_transport_state.sampleBudget > 0) {
        path_budget = g_caustic_transport_state.sampleBudget;
    }
    if (path_budget > RUNTIME_CAUSTIC_TRANSPORT_MAX_PATH_BUDGET) {
        path_budget = RUNTIME_CAUSTIC_TRANSPORT_MAX_PATH_BUDGET;
    }

    if (volume_cache_active) {
        if (!RuntimeCausticVolumeCache3D_AllocateFromVolume(cache, &scene->volume)) {
            RuntimeCausticVolumeCache3D_SnapshotDiagnostics(cache, &diagnostics.cache);
            if (out_diagnostics) *out_diagnostics = diagnostics;
            return false;
        }
        diagnostics.cacheAllocated = true;
    }
    if (g_caustic_transport_state.surfaceCacheRequested) {
        uint64_t capacity = 0u;
        if (!surface_cache) {
            if (out_diagnostics) *out_diagnostics = diagnostics;
            return false;
        }
        capacity = (uint64_t)fmax((double)path_budget * 2.0, 64.0);
        if (!RuntimeCausticSurfaceCache3D_Allocate(surface_cache, capacity)) {
            RuntimeCausticSurfaceCache3D_SnapshotDiagnostics(surface_cache,
                                                             &diagnostics.surfaceCache);
            if (out_diagnostics) *out_diagnostics = diagnostics;
            return false;
        }
        diagnostics.surfaceCacheAllocated = true;
    }
    runtime_caustic_transport_prepare_surface_receiver_fallback(
        &g_caustic_transport_surface_context,
        scene);
    if (!g_caustic_transport_state.surfaceReceiverFallbackEnabled) {
        runtime_caustic_transport_disable_surface_receiver_fallback(
            &g_caustic_transport_surface_context);
    }
    if (use_analytic_sphere_lens) {
        if (runtime_caustic_transport_resolve_analytic_sphere(scene, &analytic_sphere)) {
            diagnostics.analyticSphereLensResolvedCount = 1u;
        } else {
            diagnostics.analyticSphereLensRejectedCount = 1u;
            RuntimeCausticVolumeCache3D_SnapshotDiagnostics(cache, &diagnostics.cache);
            RuntimeCausticSurfaceCache3D_SnapshotDiagnostics(surface_cache,
                                                             &diagnostics.surfaceCache);
            (void)RuntimeCausticTransportDebug3D_WriteArtifacts(&g_caustic_transport_state,
                                                                &diagnostics);
            if (out_diagnostics) *out_diagnostics = diagnostics;
            return false;
        }
    }
    if (use_analytic_cylinder_lens) {
        if (runtime_caustic_transport_resolve_analytic_cylinder(scene, &analytic_cylinder)) {
            diagnostics.analyticCylinderLensResolvedCount = 1u;
        } else {
            diagnostics.analyticCylinderLensRejectedCount = 1u;
            RuntimeCausticVolumeCache3D_SnapshotDiagnostics(cache, &diagnostics.cache);
            RuntimeCausticSurfaceCache3D_SnapshotDiagnostics(surface_cache,
                                                             &diagnostics.surfaceCache);
            (void)RuntimeCausticTransportDebug3D_WriteArtifacts(&g_caustic_transport_state,
                                                                &diagnostics);
            if (out_diagnostics) *out_diagnostics = diagnostics;
            return false;
        }
    }
    if (use_analytic_prism_lens) {
        if (runtime_caustic_transport_resolve_analytic_prism(scene, &analytic_prism)) {
            diagnostics.analyticPrismLensResolvedCount = 1u;
        } else {
            diagnostics.analyticPrismLensRejectedCount = 1u;
            RuntimeCausticVolumeCache3D_SnapshotDiagnostics(cache, &diagnostics.cache);
            RuntimeCausticSurfaceCache3D_SnapshotDiagnostics(surface_cache,
                                                             &diagnostics.surfaceCache);
            (void)RuntimeCausticTransportDebug3D_WriteArtifacts(&g_caustic_transport_state,
                                                                &diagnostics);
            if (out_diagnostics) *out_diagnostics = diagnostics;
            return false;
        }
    }
    if (use_analytic_bowl_lens) {
        if (runtime_caustic_transport_resolve_analytic_bowl(scene, &analytic_bowl)) {
            diagnostics.analyticBowlLensResolvedCount = 1u;
        } else {
            diagnostics.analyticBowlLensRejectedCount = 1u;
            RuntimeCausticVolumeCache3D_SnapshotDiagnostics(cache, &diagnostics.cache);
            RuntimeCausticSurfaceCache3D_SnapshotDiagnostics(surface_cache,
                                                             &diagnostics.surfaceCache);
            (void)RuntimeCausticTransportDebug3D_WriteArtifacts(&g_caustic_transport_state,
                                                                &diagnostics);
            if (out_diagnostics) *out_diagnostics = diagnostics;
            return false;
        }
    }

    enabled_light_count = RuntimeLightSet3D_EnabledCount(&scene->lightSet);
    if (enabled_light_count > 0) {
        for (int light_i = 0; light_i < enabled_light_count; ++light_i) {
            const RuntimeLightSource3D* light =
                RuntimeLightSet3D_GetEnabled(&scene->lightSet, light_i);
            if (!light || light->kind == RUNTIME_LIGHT_SOURCE_3D_KIND_MESH_EMISSIVE) continue;
            diagnostics.lightCount += 1u;
            if (use_analytic_sphere_lens) {
                runtime_caustic_transport_emit_analytic_sphere_lens(
                    scene,
                    light,
                    light_i,
                    &analytic_sphere,
                    path_budget,
                    volume_cache_active ? cache : NULL,
                    g_caustic_transport_state.surfaceCacheRequested ? surface_cache : NULL,
                    g_caustic_transport_state.maxPathDepth,
                    g_caustic_transport_state.surfaceFootprintScale,
                    g_caustic_transport_state.surfaceRadianceScale,
                    &g_caustic_transport_surface_context,
                    &diagnostics);
            } else if (use_analytic_cylinder_lens) {
                runtime_caustic_transport_emit_analytic_cylinder_lens(
                    scene,
                    light,
                    light_i,
                    &analytic_cylinder,
                    path_budget,
                    use_focused_cylinder_lens,
                    volume_cache_active ? cache : NULL,
                    g_caustic_transport_state.surfaceCacheRequested ? surface_cache : NULL,
                    g_caustic_transport_state.maxPathDepth,
                    g_caustic_transport_state.surfaceFootprintScale,
                    g_caustic_transport_state.surfaceRadianceScale,
                    &g_caustic_transport_surface_context,
                    &diagnostics);
            } else if (use_analytic_prism_lens) {
                runtime_caustic_transport_emit_analytic_prism_lens(
                    scene,
                    light,
                    light_i,
                    &analytic_prism,
                    path_budget,
                    volume_cache_active ? cache : NULL,
                    g_caustic_transport_state.surfaceCacheRequested ? surface_cache : NULL,
                    g_caustic_transport_state.maxPathDepth,
                    g_caustic_transport_state.surfaceFootprintScale,
                    g_caustic_transport_state.surfaceRadianceScale,
                    &g_caustic_transport_surface_context,
                    &diagnostics);
            } else if (use_analytic_bowl_lens) {
                runtime_caustic_transport_emit_analytic_bowl_lens(
                    scene,
                    light,
                    light_i,
                    &analytic_bowl,
                    path_budget,
                    volume_cache_active ? cache : NULL,
                    g_caustic_transport_state.surfaceCacheRequested ? surface_cache : NULL,
                    g_caustic_transport_state.maxPathDepth,
                    g_caustic_transport_state.surfaceFootprintScale,
                    g_caustic_transport_state.surfaceRadianceScale,
                    &g_caustic_transport_surface_context,
                    &diagnostics);
            } else {
                for (int tri_i = 0; tri_i < scene->triangleMesh.triangleCount; ++tri_i) {
                    if ((int)diagnostics.evaluatedPathCount >= path_budget) break;
                    runtime_caustic_transport_emit_to_triangle(scene,
                                                               light,
                                                               light_i,
                                                               tri_i,
                                                               path_budget,
                                                               volume_cache_active ? cache : NULL,
                                                               g_caustic_transport_state.surfaceCacheRequested ? surface_cache : NULL,
                                                               g_caustic_transport_state.maxPathDepth,
                                                               g_caustic_transport_state.surfaceFootprintScale,
                                                               g_caustic_transport_state.surfaceRadianceScale,
                                                               &g_caustic_transport_surface_context,
                                                               &diagnostics);
                }
            }
        }
    } else if (scene->hasLight) {
        RuntimeLightSource3D_Init(&compat_light);
        compat_light.kind = scene->light.radius > 0.0 ? RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE
                                                      : RUNTIME_LIGHT_SOURCE_3D_KIND_POINT;
        compat_light.position = scene->light.position;
        compat_light.radius = scene->light.radius;
        compat_light.intensity = scene->light.intensity;
        compat_light.falloffDistance = scene->light.falloffDistance;
        compat_light.falloffMode = scene->light.falloffMode;
        diagnostics.lightCount = 1u;
        if (use_analytic_sphere_lens) {
            runtime_caustic_transport_emit_analytic_sphere_lens(
                scene,
                &compat_light,
                0,
                &analytic_sphere,
                path_budget,
                volume_cache_active ? cache : NULL,
                g_caustic_transport_state.surfaceCacheRequested ? surface_cache : NULL,
                g_caustic_transport_state.maxPathDepth,
                g_caustic_transport_state.surfaceFootprintScale,
                g_caustic_transport_state.surfaceRadianceScale,
                &g_caustic_transport_surface_context,
                &diagnostics);
        } else if (use_analytic_cylinder_lens) {
            runtime_caustic_transport_emit_analytic_cylinder_lens(
                scene,
                &compat_light,
                0,
                &analytic_cylinder,
                path_budget,
                use_focused_cylinder_lens,
                volume_cache_active ? cache : NULL,
                g_caustic_transport_state.surfaceCacheRequested ? surface_cache : NULL,
                g_caustic_transport_state.maxPathDepth,
                g_caustic_transport_state.surfaceFootprintScale,
                g_caustic_transport_state.surfaceRadianceScale,
                &g_caustic_transport_surface_context,
                &diagnostics);
        } else if (use_analytic_prism_lens) {
            runtime_caustic_transport_emit_analytic_prism_lens(
                scene,
                &compat_light,
                0,
                &analytic_prism,
                path_budget,
                volume_cache_active ? cache : NULL,
                g_caustic_transport_state.surfaceCacheRequested ? surface_cache : NULL,
                g_caustic_transport_state.maxPathDepth,
                g_caustic_transport_state.surfaceFootprintScale,
                g_caustic_transport_state.surfaceRadianceScale,
                &g_caustic_transport_surface_context,
                &diagnostics);
        } else if (use_analytic_bowl_lens) {
            runtime_caustic_transport_emit_analytic_bowl_lens(
                scene,
                &compat_light,
                0,
                &analytic_bowl,
                path_budget,
                volume_cache_active ? cache : NULL,
                g_caustic_transport_state.surfaceCacheRequested ? surface_cache : NULL,
                g_caustic_transport_state.maxPathDepth,
                g_caustic_transport_state.surfaceFootprintScale,
                g_caustic_transport_state.surfaceRadianceScale,
                &g_caustic_transport_surface_context,
                &diagnostics);
        } else {
            for (int tri_i = 0; tri_i < scene->triangleMesh.triangleCount; ++tri_i) {
                if ((int)diagnostics.evaluatedPathCount >= path_budget) break;
                runtime_caustic_transport_emit_to_triangle(scene,
                                                           &compat_light,
                                                           0,
                                                           tri_i,
                                                           path_budget,
                                                           volume_cache_active ? cache : NULL,
                                                           g_caustic_transport_state.surfaceCacheRequested ? surface_cache : NULL,
                                                           g_caustic_transport_state.maxPathDepth,
                                                           g_caustic_transport_state.surfaceFootprintScale,
                                                           g_caustic_transport_state.surfaceRadianceScale,
                                                           &g_caustic_transport_surface_context,
                                                           &diagnostics);
            }
        }
    }

    RuntimeCausticVolumeCache3D_SnapshotDiagnostics(cache, &diagnostics.cache);
    RuntimeCausticSurfaceCache3D_SnapshotDiagnostics(surface_cache, &diagnostics.surfaceCache);
    (void)RuntimeCausticTransportDebug3D_WriteArtifacts(&g_caustic_transport_state,
                                                        &diagnostics);
    if (out_diagnostics) *out_diagnostics = diagnostics;
    return diagnostics.emittedPathCount > 0u &&
           (!volume_cache_active ||
            diagnostics.cache.nonZeroCellCount > 0u) &&
           (!g_caustic_transport_state.surfaceCacheRequested ||
            diagnostics.surfaceCache.recordCount > 0u);
}
