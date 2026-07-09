#include "render/runtime_caustic_transport_internal_3d.h"

#include <string.h>

static RuntimeCausticTransportProviderKind3D runtime_caustic_transport_provider_kind_for_policy(
    RuntimeCausticTransportEmissionPolicy3D emission_policy,
    bool* out_focused_profile) {
    if (out_focused_profile) *out_focused_profile = false;
    switch (emission_policy) {
        case RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_SPHERE_LENS:
            return RUNTIME_CAUSTIC_TRANSPORT_PROVIDER_ANALYTIC_SPHERE_LENS;
        case RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_CYLINDER_LENS_FOCUSED:
            if (out_focused_profile) *out_focused_profile = true;
            return RUNTIME_CAUSTIC_TRANSPORT_PROVIDER_ANALYTIC_CYLINDER_LENS;
        case RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_CYLINDER_LENS:
            return RUNTIME_CAUSTIC_TRANSPORT_PROVIDER_ANALYTIC_CYLINDER_LENS;
        case RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_PRISM_LENS:
            return RUNTIME_CAUSTIC_TRANSPORT_PROVIDER_ANALYTIC_PRISM_LENS;
        case RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_BOWL_LENS:
            return RUNTIME_CAUSTIC_TRANSPORT_PROVIDER_ANALYTIC_BOWL_LENS;
        case RUNTIME_CAUSTIC_TRANSPORT_EMISSION_MESH_DIELECTRIC_LENS:
            return RUNTIME_CAUSTIC_TRANSPORT_PROVIDER_MESH_DIELECTRIC_LENS;
        case RUNTIME_CAUSTIC_TRANSPORT_EMISSION_TRIANGLE_TARGETS:
        default:
            return RUNTIME_CAUSTIC_TRANSPORT_PROVIDER_TRIANGLE_TARGETS;
    }
}

bool runtime_caustic_transport_resolve_provider(
    const RuntimeScene3D* scene,
    RuntimeCausticTransportEmissionPolicy3D emission_policy,
    RuntimeCausticTransportProvider3D* out_provider,
    RuntimeCausticTransport3DDiagnostics* diagnostics) {
    RuntimeCausticTransportProvider3D provider;
    bool focused_profile = false;

    if (!out_provider) return false;
    memset(&provider, 0, sizeof(provider));
    provider.emissionPolicy = emission_policy;
    provider.kind =
        runtime_caustic_transport_provider_kind_for_policy(emission_policy,
                                                           &focused_profile);
    provider.focusedProfile = focused_profile;

    switch (provider.kind) {
        case RUNTIME_CAUSTIC_TRANSPORT_PROVIDER_ANALYTIC_SPHERE_LENS:
            if (runtime_caustic_transport_resolve_analytic_sphere(
                    scene,
                    &provider.analyticSphere)) {
                if (diagnostics) diagnostics->analyticSphereLensResolvedCount = 1u;
                *out_provider = provider;
                return true;
            }
            if (diagnostics) diagnostics->analyticSphereLensRejectedCount = 1u;
            *out_provider = provider;
            return false;
        case RUNTIME_CAUSTIC_TRANSPORT_PROVIDER_ANALYTIC_CYLINDER_LENS:
            if (runtime_caustic_transport_resolve_analytic_cylinder(
                    scene,
                    &provider.analyticCylinder)) {
                if (diagnostics) diagnostics->analyticCylinderLensResolvedCount = 1u;
                *out_provider = provider;
                return true;
            }
            if (diagnostics) diagnostics->analyticCylinderLensRejectedCount = 1u;
            *out_provider = provider;
            return false;
        case RUNTIME_CAUSTIC_TRANSPORT_PROVIDER_ANALYTIC_PRISM_LENS:
            if (runtime_caustic_transport_resolve_analytic_prism(scene,
                                                                 &provider.analyticPrism)) {
                if (diagnostics) diagnostics->analyticPrismLensResolvedCount = 1u;
                *out_provider = provider;
                return true;
            }
            if (diagnostics) diagnostics->analyticPrismLensRejectedCount = 1u;
            *out_provider = provider;
            return false;
        case RUNTIME_CAUSTIC_TRANSPORT_PROVIDER_ANALYTIC_BOWL_LENS:
            if (runtime_caustic_transport_resolve_analytic_bowl(scene,
                                                                &provider.analyticBowl)) {
                if (diagnostics) diagnostics->analyticBowlLensResolvedCount = 1u;
                *out_provider = provider;
                return true;
            }
            if (diagnostics) diagnostics->analyticBowlLensRejectedCount = 1u;
            *out_provider = provider;
            return false;
        case RUNTIME_CAUSTIC_TRANSPORT_PROVIDER_MESH_DIELECTRIC_LENS:
            if (runtime_caustic_transport_resolve_mesh_dielectric(
                    scene,
                    &provider.meshDielectric)) {
                if (diagnostics) diagnostics->meshDielectricLensResolvedCount = 1u;
                *out_provider = provider;
                return true;
            }
            if (diagnostics) diagnostics->meshDielectricLensRejectedCount = 1u;
            *out_provider = provider;
            return false;
        case RUNTIME_CAUSTIC_TRANSPORT_PROVIDER_TRIANGLE_TARGETS:
        default:
            *out_provider = provider;
            return true;
    }
}

void runtime_caustic_transport_emit_provider_for_light(
    const RuntimeScene3D* scene,
    const RuntimeLightSource3D* light,
    int light_index,
    const RuntimeCausticTransportProvider3D* provider,
    const RuntimeCausticLensTraversalProfile3D* traversal_profile_override,
    int path_budget,
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticSurfaceCache3D* surface_cache,
    int max_path_depth,
    double surface_footprint_scale,
    double surface_radiance_scale,
    const RuntimeCausticTransportSurfaceReceiverContext3D* receiver_context,
    RuntimeCausticTransport3DDiagnostics* diagnostics) {
    if (!scene || !light || !provider || !diagnostics) return;

    switch (provider->kind) {
        case RUNTIME_CAUSTIC_TRANSPORT_PROVIDER_ANALYTIC_SPHERE_LENS:
            runtime_caustic_transport_emit_analytic_sphere_lens(scene,
                                                                light,
                                                                light_index,
                                                                &provider->analyticSphere,
                                                                traversal_profile_override,
                                                                path_budget,
                                                                cache,
                                                                surface_cache,
                                                                max_path_depth,
                                                                surface_footprint_scale,
                                                                surface_radiance_scale,
                                                                receiver_context,
                                                                diagnostics);
            break;
        case RUNTIME_CAUSTIC_TRANSPORT_PROVIDER_ANALYTIC_CYLINDER_LENS:
            runtime_caustic_transport_emit_analytic_cylinder_lens(
                scene,
                light,
                light_index,
                &provider->analyticCylinder,
                traversal_profile_override,
                path_budget,
                provider->focusedProfile,
                cache,
                surface_cache,
                max_path_depth,
                surface_footprint_scale,
                surface_radiance_scale,
                receiver_context,
                diagnostics);
            break;
        case RUNTIME_CAUSTIC_TRANSPORT_PROVIDER_ANALYTIC_PRISM_LENS:
            runtime_caustic_transport_emit_analytic_prism_lens(scene,
                                                               light,
                                                               light_index,
                                                               &provider->analyticPrism,
                                                               traversal_profile_override,
                                                               path_budget,
                                                               cache,
                                                               surface_cache,
                                                               max_path_depth,
                                                               surface_footprint_scale,
                                                               surface_radiance_scale,
                                                               receiver_context,
                                                               diagnostics);
            break;
        case RUNTIME_CAUSTIC_TRANSPORT_PROVIDER_ANALYTIC_BOWL_LENS:
            runtime_caustic_transport_emit_analytic_bowl_lens(scene,
                                                              light,
                                                              light_index,
                                                              &provider->analyticBowl,
                                                              traversal_profile_override,
                                                              path_budget,
                                                              cache,
                                                              surface_cache,
                                                              max_path_depth,
                                                              surface_footprint_scale,
                                                              surface_radiance_scale,
                                                              receiver_context,
                                                              diagnostics);
            break;
        case RUNTIME_CAUSTIC_TRANSPORT_PROVIDER_MESH_DIELECTRIC_LENS:
            runtime_caustic_transport_emit_mesh_dielectric_lens(scene,
                                                               light,
                                                               light_index,
                                                               &provider->meshDielectric,
                                                               traversal_profile_override,
                                                               path_budget,
                                                               cache,
                                                               surface_cache,
                                                               max_path_depth,
                                                               surface_footprint_scale,
                                                               surface_radiance_scale,
                                                               receiver_context,
                                                               diagnostics);
            break;
        case RUNTIME_CAUSTIC_TRANSPORT_PROVIDER_TRIANGLE_TARGETS:
        default:
            for (int tri_i = 0; tri_i < scene->triangleMesh.triangleCount; ++tri_i) {
                if ((int)diagnostics->evaluatedPathCount >= path_budget) break;
                runtime_caustic_transport_emit_to_triangle(scene,
                                                           light,
                                                           light_index,
                                                           tri_i,
                                                           path_budget,
                                                           cache,
                                                           surface_cache,
                                                           max_path_depth,
                                                           surface_footprint_scale,
                                                           surface_radiance_scale,
                                                           receiver_context,
                                                           diagnostics);
            }
            break;
    }
}
