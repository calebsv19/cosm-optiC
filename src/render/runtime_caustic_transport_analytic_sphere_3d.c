#include "render/runtime_caustic_transport_internal_3d.h"

#include <math.h>

static bool runtime_caustic_transport_emit_analytic_sphere_lens_sample(
    const RuntimeScene3D* scene,
    const RuntimeLightSource3D* light,
    int light_index,
    const RuntimeCausticTransportAnalyticSphere3D* analytic_sphere,
    const RuntimeCausticLensTraversalProfile3D* traversal_profile_override,
    int sample_index,
    int path_budget,
    int sample_count,
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticSurfaceCache3D* surface_cache,
    int max_path_depth,
    double surface_footprint_scale,
    double surface_radiance_scale,
    const RuntimeCausticTransportSurfaceReceiverContext3D* receiver_context,
    RuntimeCausticTransport3DDiagnostics* diagnostics) {
    RuntimeCausticSphereLens3DLight lens_light;
    RuntimeCausticSphereLens3DSample sample;
    RuntimeCausticLensPath3D path;
    RuntimeCausticTransportLensPathDepositContext3D lens_context = {0};
    RuntimeCausticSphereLens3DDescriptor sphere;
    double light_distance = 0.0;
    double aperture_u = 0.0;
    double aperture_v = 0.0;
    double lens_u = 0.0;
    double lens_v = 0.0;
    double sample_weight = 0.0;
    double volume_footprint_radius = 0.0;

    if (!scene || !light || !analytic_sphere || !analytic_sphere->valid ||
        !diagnostics || sample_count <= 0) {
        return false;
    }
    sphere = analytic_sphere->sphere;
    if (traversal_profile_override) {
        sphere.outsideIor = traversal_profile_override->outsideIor;
        sphere.ior = traversal_profile_override->materialIor;
        sphere.fresnelScale = traversal_profile_override->fresnelScale;
        sphere.transmissionScale = traversal_profile_override->transmissionScale;
        sphere.tint = traversal_profile_override->tint;
        sphere.absorptionDistance = traversal_profile_override->absorptionDistance;
        sphere.apertureRadiusScale = traversal_profile_override->apertureRadiusScale;
    }

    RuntimeCausticSphereLens3D_DefaultLight(&lens_light);
    RuntimeCausticSphereLens3D_DefaultSample(&sample);
    runtime_caustic_transport_sphere_lens_sample(sample_index,
                                                 sample_count,
                                                 &aperture_u,
                                                 &aperture_v,
                                                 &lens_u,
                                                 &lens_v);
    light_distance = vec3_length(vec3_sub(sphere.center,
                                          light->position));
    lens_light.position = light->position;
    lens_light.radius = fmax(light->radius, sphere.radius * 0.025);
    lens_light.intensity = runtime_caustic_transport_light_attenuation(light,
                                                                       light_distance);
    lens_light.color = light->color;
    sample_weight = runtime_caustic_transport_analytic_sphere_lens_sample_weight(path_budget,
                                                                                 sample_count);
    sample.apertureU = aperture_u;
    sample.apertureV = aperture_v;
    sample.lensU = lens_u;
    sample.lensV = lens_v;
    sample.sampleWeight = sample_weight;
    sample.receiverPlaneZ = sphere.center.z - sphere.radius * 3.0;

    diagnostics->evaluatedPathCount += 1u;
    diagnostics->analyticSphereLensEvaluatedPathCount += 1u;
    diagnostics->analyticSphereLensSampleWeight = sample_weight;
    diagnostics->analyticSphereLensTotalSampleWeight += sample_weight;
    if (!RuntimeCausticLensTransport3D_SolveSpherePath(&sphere,
                                                       &lens_light,
                                                       &sample,
                                                       analytic_sphere->sceneObjectIndex,
                                                       analytic_sphere->primitiveIndex,
                                                       &path) ||
        !path.valid ||
        !(runtime_caustic_transport_luma(path.throughput) > 1.0e-9)) {
        return false;
    }
    volume_footprint_radius = runtime_caustic_transport_clamp(
        sphere.radius * 0.045 + fmax(light->radius, 0.0) * 0.65,
        0.0,
        sphere.radius * 0.45);

    lens_context.emissionPolicy = RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_SPHERE_LENS;
    lens_context.eventType = "analytic_sphere_lens";
    lens_context.sceneObjectIndex = analytic_sphere->sceneObjectIndex;
    lens_context.primitiveIndex = analytic_sphere->primitiveIndex;
    lens_context.sampleIndex = sample_index;
    lens_context.payload = &analytic_sphere->payload;
    lens_context.writeSphereLensCompatibilityFields = true;
    lens_context.volumeFootprintRadius = volume_footprint_radius;
    lens_context.emittedCounter = &diagnostics->analyticSphereLensEmittedPathCount;
    return runtime_caustic_transport_deposit_lens_path(scene,
                                                       light,
                                                       light_index,
                                                       &path,
                                                       &lens_context,
                                                       cache,
                                                       surface_cache,
                                                       max_path_depth,
                                                       surface_footprint_scale,
                                                       surface_radiance_scale,
                                                       receiver_context,
                                                       diagnostics);
}
void runtime_caustic_transport_emit_analytic_sphere_lens(
    const RuntimeScene3D* scene,
    const RuntimeLightSource3D* light,
    int light_index,
    const RuntimeCausticTransportAnalyticSphere3D* analytic_sphere,
    const RuntimeCausticLensTraversalProfile3D* traversal_profile_override,
    int path_budget,
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticSurfaceCache3D* surface_cache,
    int max_path_depth,
    double surface_footprint_scale,
    double surface_radiance_scale,
    const RuntimeCausticTransportSurfaceReceiverContext3D* receiver_context,
    RuntimeCausticTransport3DDiagnostics* diagnostics) {
    int sample_count = path_budget;
    if (!scene || !light || !analytic_sphere || !diagnostics) return;
    if (sample_count <= 0) {
        sample_count = RUNTIME_CAUSTIC_TRANSPORT_SPHERE_LENS_SEED_SAMPLE_COUNT;
    }
    if (sample_count > RUNTIME_CAUSTIC_TRANSPORT_SPHERE_LENS_MAX_SAMPLE_COUNT) {
        sample_count = RUNTIME_CAUSTIC_TRANSPORT_SPHERE_LENS_MAX_SAMPLE_COUNT;
    }
    for (int sample_i = 0;
         sample_i < sample_count && (int)diagnostics->evaluatedPathCount < path_budget;
         ++sample_i) {
        (void)runtime_caustic_transport_emit_analytic_sphere_lens_sample(scene,
                                                                         light,
                                                                         light_index,
                                                                         analytic_sphere,
                                                                         traversal_profile_override,
                                                                         sample_i,
                                                                         path_budget,
                                                                         sample_count,
                                                                         cache,
                                                                         surface_cache,
                                                                         max_path_depth,
                                                                         surface_footprint_scale,
                                                                         surface_radiance_scale,
                                                                         receiver_context,
                                                                         diagnostics);
    }
}
