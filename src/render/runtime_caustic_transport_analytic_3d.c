#include "render/runtime_caustic_transport_internal_3d.h"

#include <math.h>

void runtime_caustic_transport_sphere_lens_sample(int sample_index,
                                                  int sample_count,
                                                  double* out_aperture_u,
                                                  double* out_aperture_v,
                                                  double* out_lens_u,
                                                  double* out_lens_v) {
    static const double samples[RUNTIME_CAUSTIC_TRANSPORT_SPHERE_LENS_SEED_SAMPLE_COUNT][4] = {
        {0.0, 0.0, 0.0, 0.0},
        {-0.5, -0.5, -0.55, -0.55},
        {0.5, -0.5, 0.55, -0.55},
        {-0.5, 0.5, -0.55, 0.55},
        {0.5, 0.5, 0.55, 0.55},
        {-0.25, 0.0, -0.35, 0.0},
        {0.25, 0.0, 0.35, 0.0},
        {0.0, -0.25, 0.0, -0.35},
        {0.0, 0.25, 0.0, 0.35},
        {-0.75, 0.0, -0.75, 0.0},
        {0.75, 0.0, 0.75, 0.0},
        {0.0, -0.75, 0.0, -0.75},
        {0.0, 0.75, 0.0, 0.75},
        {-0.35, 0.35, -0.45, 0.45},
        {0.35, 0.35, 0.45, 0.45},
        {0.35, -0.35, 0.45, -0.45}
    };
    const double* s = samples[0];
    double t = 0.0;
    double lens_r = 0.0;
    double lens_a = 0.0;
    double aperture_r = 0.0;
    double aperture_a = 0.0;
    if (sample_index >= 0 &&
        sample_index < RUNTIME_CAUSTIC_TRANSPORT_SPHERE_LENS_SEED_SAMPLE_COUNT) {
        s = samples[sample_index];
        if (out_aperture_u) *out_aperture_u = s[0];
        if (out_aperture_v) *out_aperture_v = s[1];
        if (out_lens_u) *out_lens_u = s[2];
        if (out_lens_v) *out_lens_v = s[3];
        return;
    }
    if (sample_count <= 0) sample_count = 1;
    if (sample_index < 0) sample_index = 0;
    t = ((double)sample_index + 0.5) / (double)sample_count;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    lens_r = sqrt(t) * 0.92;
    lens_a = (double)sample_index * 2.39996322972865332;
    aperture_r = sqrt(fmod((double)sample_index * 0.61803398874989485, 1.0)) * 0.85;
    aperture_a = lens_a + 1.173245;
    if (out_aperture_u) *out_aperture_u = aperture_r * cos(aperture_a);
    if (out_aperture_v) *out_aperture_v = aperture_r * sin(aperture_a);
    if (out_lens_u) *out_lens_u = lens_r * cos(lens_a);
    if (out_lens_v) *out_lens_v = lens_r * sin(lens_a);
}

void runtime_caustic_transport_cylinder_lens_focused_sample(int sample_index,
                                                            int sample_count,
                                                            double* out_aperture_u,
                                                            double* out_aperture_v,
                                                            double* out_lens_u,
                                                            double* out_lens_v) {
    double t = 0.5;
    double radial_phase = 0.0;
    if (sample_count <= 0) sample_count = 1;
    if (sample_index < 0) sample_index = 0;
    t = ((double)sample_index + 0.5) / (double)sample_count;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    radial_phase = (double)sample_index * 2.39996322972865332;
    if (out_aperture_u) *out_aperture_u = 0.0;
    if (out_aperture_v) *out_aperture_v = 0.0;
    if (out_lens_u) *out_lens_u = 0.12 * sin(radial_phase);
    if (out_lens_v) *out_lens_v = -0.82 + 1.64 * t;
}

static bool runtime_caustic_transport_emit_analytic_cylinder_lens_sample(
    const RuntimeScene3D* scene,
    const RuntimeLightSource3D* light,
    int light_index,
    const RuntimeCausticTransportAnalyticCylinder3D* analytic_cylinder,
    const RuntimeCausticLensTraversalProfile3D* traversal_profile_override,
    int sample_index,
    int path_budget,
    int sample_count,
    bool focused_profile,
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticSurfaceCache3D* surface_cache,
    int max_path_depth,
    double surface_footprint_scale,
    double surface_radiance_scale,
    const RuntimeCausticTransportSurfaceReceiverContext3D* receiver_context,
    RuntimeCausticTransport3DDiagnostics* diagnostics) {
    RuntimeCausticLensLightSample3D lens_light;
    RuntimeCausticLensSample3D sample;
    RuntimeCausticLensPath3D path;
    RuntimeCausticTransportLensPathDepositContext3D lens_context = {0};
    RuntimeCausticLensShape3D shape;
    double light_distance = 0.0;
    double aperture_u = 0.0;
    double aperture_v = 0.0;
    double lens_u = 0.0;
    double lens_v = 0.0;
    double sample_weight = 0.0;
    double volume_footprint_radius = 0.0;

    if (!scene || !light || !analytic_cylinder || !analytic_cylinder->valid ||
        !diagnostics || sample_count <= 0) {
        return false;
    }
    shape = analytic_cylinder->shape;
    if (traversal_profile_override) {
        shape.hasTraversalProfileOverride = true;
        shape.traversalProfileOverride = *traversal_profile_override;
    }

    RuntimeCausticLensTransport3D_DefaultLightSample(&lens_light);
    RuntimeCausticLensTransport3D_DefaultSample(&sample);
    if (focused_profile) {
        runtime_caustic_transport_cylinder_lens_focused_sample(sample_index,
                                                               sample_count,
                                                               &aperture_u,
                                                               &aperture_v,
                                                               &lens_u,
                                                               &lens_v);
    } else {
        runtime_caustic_transport_sphere_lens_sample(sample_index,
                                                     sample_count,
                                                     &aperture_u,
                                                     &aperture_v,
                                                     &lens_u,
                                                     &lens_v);
    }
    light_distance = vec3_length(vec3_sub(shape.center, light->position));
    lens_light.position = light->position;
    lens_light.radius = fmax(light->radius, shape.radius * 0.025);
    lens_light.intensity = runtime_caustic_transport_light_attenuation(light,
                                                                       light_distance);
    lens_light.color = light->color;
    lens_light.lightIndex = light_index;
    sample_weight = runtime_caustic_transport_analytic_sphere_lens_sample_weight(path_budget,
                                                                                 sample_count);
    sample.apertureU = aperture_u;
    sample.apertureV = aperture_v;
    sample.lensU = lens_u;
    sample.lensV = lens_v;
    sample.sampleWeight = sample_weight;
    sample.receiverDistance = shape.radius * 4.0;

    diagnostics->evaluatedPathCount += 1u;
    diagnostics->analyticCylinderLensEvaluatedPathCount += 1u;
    diagnostics->analyticCylinderLensSampleWeight = sample_weight;
    diagnostics->analyticCylinderLensTotalSampleWeight += sample_weight;
    if (!RuntimeCausticLensTransport3D_SolveCylinderPath(&shape,
                                                         &lens_light,
                                                         &sample,
                                                         &path) ||
        !path.valid ||
        !(runtime_caustic_transport_luma(path.throughput) > 1.0e-9)) {
        return false;
    }
    volume_footprint_radius = runtime_caustic_transport_clamp(
        shape.radius * 0.045 + fmax(light->radius, 0.0) * 0.65,
        0.0,
        shape.radius * 0.45);

    lens_context.emissionPolicy =
        focused_profile ? RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_CYLINDER_LENS_FOCUSED
                        : RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_CYLINDER_LENS;
    lens_context.eventType = "analytic_cylinder_lens";
    lens_context.sceneObjectIndex = analytic_cylinder->sceneObjectIndex;
    lens_context.primitiveIndex = analytic_cylinder->primitiveIndex;
    lens_context.sampleIndex = sample_index;
    lens_context.payload = &analytic_cylinder->payload;
    lens_context.volumeFootprintRadius = volume_footprint_radius;
    lens_context.emittedCounter = &diagnostics->analyticCylinderLensEmittedPathCount;
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

void runtime_caustic_transport_emit_analytic_cylinder_lens(
    const RuntimeScene3D* scene,
    const RuntimeLightSource3D* light,
    int light_index,
    const RuntimeCausticTransportAnalyticCylinder3D* analytic_cylinder,
    const RuntimeCausticLensTraversalProfile3D* traversal_profile_override,
    int path_budget,
    bool focused_profile,
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticSurfaceCache3D* surface_cache,
    int max_path_depth,
    double surface_footprint_scale,
    double surface_radiance_scale,
    const RuntimeCausticTransportSurfaceReceiverContext3D* receiver_context,
    RuntimeCausticTransport3DDiagnostics* diagnostics) {
    int sample_count = path_budget;
    if (!scene || !light || !analytic_cylinder || !diagnostics) return;
    if (sample_count <= 0) {
        sample_count = RUNTIME_CAUSTIC_TRANSPORT_SPHERE_LENS_SEED_SAMPLE_COUNT;
    }
    if (sample_count > RUNTIME_CAUSTIC_TRANSPORT_SPHERE_LENS_MAX_SAMPLE_COUNT) {
        sample_count = RUNTIME_CAUSTIC_TRANSPORT_SPHERE_LENS_MAX_SAMPLE_COUNT;
    }
    for (int sample_i = 0;
         sample_i < sample_count && (int)diagnostics->evaluatedPathCount < path_budget;
         ++sample_i) {
        (void)runtime_caustic_transport_emit_analytic_cylinder_lens_sample(
            scene,
            light,
            light_index,
            analytic_cylinder,
            traversal_profile_override,
            sample_i,
            path_budget,
            sample_count,
            focused_profile,
            cache,
            surface_cache,
            max_path_depth,
                                                                         surface_footprint_scale,
                                                                         surface_radiance_scale,
                                                                         receiver_context,
                                                                         diagnostics);
    }
}

static bool runtime_caustic_transport_emit_analytic_prism_lens_sample(
    const RuntimeScene3D* scene,
    const RuntimeLightSource3D* light,
    int light_index,
    const RuntimeCausticTransportAnalyticPrism3D* analytic_prism,
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
    RuntimeCausticLensLightSample3D lens_light;
    RuntimeCausticLensSample3D sample;
    RuntimeCausticLensPath3D path;
    RuntimeCausticTransportLensPathDepositContext3D lens_context = {0};
    RuntimeCausticLensShape3D shape;
    double light_distance = 0.0;
    double aperture_u = 0.0;
    double aperture_v = 0.0;
    double lens_u = 0.0;
    double lens_v = 0.0;
    double sample_weight = 0.0;
    double volume_footprint_radius = 0.0;

    if (!scene || !light || !analytic_prism || !analytic_prism->valid ||
        !diagnostics || sample_count <= 0) {
        return false;
    }
    shape = analytic_prism->shape;
    if (traversal_profile_override) {
        shape.hasTraversalProfileOverride = true;
        shape.traversalProfileOverride = *traversal_profile_override;
    }

    RuntimeCausticLensTransport3D_DefaultLightSample(&lens_light);
    RuntimeCausticLensTransport3D_DefaultSample(&sample);
    runtime_caustic_transport_sphere_lens_sample(sample_index,
                                                 sample_count,
                                                 &aperture_u,
                                                 &aperture_v,
                                                 &lens_u,
                                                 &lens_v);
    light_distance = vec3_length(vec3_sub(shape.center, light->position));
    lens_light.position = light->position;
    lens_light.radius = fmax(light->radius, shape.radius * 0.015);
    lens_light.intensity = runtime_caustic_transport_light_attenuation(light,
                                                                       light_distance);
    lens_light.color = light->color;
    lens_light.lightIndex = light_index;
    sample_weight = runtime_caustic_transport_analytic_sphere_lens_sample_weight(path_budget,
                                                                                 sample_count);
    sample.apertureU = aperture_u * 0.35;
    sample.apertureV = aperture_v * 0.35;
    sample.lensU = lens_u * 0.70;
    sample.lensV = lens_v;
    sample.sampleWeight = sample_weight;
    sample.receiverDistance = shape.radius * 5.0;

    diagnostics->evaluatedPathCount += 1u;
    diagnostics->analyticPrismLensEvaluatedPathCount += 1u;
    diagnostics->analyticPrismLensSampleWeight = sample_weight;
    diagnostics->analyticPrismLensTotalSampleWeight += sample_weight;
    if (!RuntimeCausticLensTransport3D_SolvePrismPath(&shape,
                                                      &lens_light,
                                                      &sample,
                                                      &path) ||
        !path.valid ||
        !(runtime_caustic_transport_luma(path.throughput) > 1.0e-9)) {
        return false;
    }
    volume_footprint_radius = runtime_caustic_transport_clamp(
        shape.radius * 0.035 + fmax(light->radius, 0.0) * 0.45,
        0.0,
        shape.radius * 0.35);

    lens_context.emissionPolicy = RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_PRISM_LENS;
    lens_context.eventType = "analytic_prism_lens";
    lens_context.sceneObjectIndex = analytic_prism->sceneObjectIndex;
    lens_context.primitiveIndex = analytic_prism->primitiveIndex;
    lens_context.sampleIndex = sample_index;
    lens_context.payload = &analytic_prism->payload;
    lens_context.volumeFootprintRadius = volume_footprint_radius;
    lens_context.emittedCounter = &diagnostics->analyticPrismLensEmittedPathCount;
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

void runtime_caustic_transport_emit_analytic_prism_lens(
    const RuntimeScene3D* scene,
    const RuntimeLightSource3D* light,
    int light_index,
    const RuntimeCausticTransportAnalyticPrism3D* analytic_prism,
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
    if (!scene || !light || !analytic_prism || !diagnostics) return;
    if (sample_count <= 0) {
        sample_count = RUNTIME_CAUSTIC_TRANSPORT_SPHERE_LENS_SEED_SAMPLE_COUNT;
    }
    if (sample_count > RUNTIME_CAUSTIC_TRANSPORT_SPHERE_LENS_MAX_SAMPLE_COUNT) {
        sample_count = RUNTIME_CAUSTIC_TRANSPORT_SPHERE_LENS_MAX_SAMPLE_COUNT;
    }
    for (int sample_i = 0;
         sample_i < sample_count && (int)diagnostics->evaluatedPathCount < path_budget;
         ++sample_i) {
        (void)runtime_caustic_transport_emit_analytic_prism_lens_sample(
            scene,
            light,
            light_index,
            analytic_prism,
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

static bool runtime_caustic_transport_emit_analytic_bowl_lens_sample(
    const RuntimeScene3D* scene,
    const RuntimeLightSource3D* light,
    int light_index,
    const RuntimeCausticTransportAnalyticBowl3D* analytic_bowl,
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
    RuntimeCausticLensLightSample3D lens_light;
    RuntimeCausticLensSample3D sample;
    RuntimeCausticLensPath3D path;
    RuntimeCausticTransportLensPathDepositContext3D lens_context = {0};
    RuntimeCausticLensShape3D shape;
    double light_distance = 0.0;
    double aperture_u = 0.0;
    double aperture_v = 0.0;
    double lens_u = 0.0;
    double lens_v = 0.0;
    double sample_weight = 0.0;
    double volume_footprint_radius = 0.0;

    if (!scene || !light || !analytic_bowl || !analytic_bowl->valid ||
        !diagnostics || sample_count <= 0) {
        return false;
    }
    shape = analytic_bowl->shape;
    if (traversal_profile_override) {
        shape.hasTraversalProfileOverride = true;
        shape.traversalProfileOverride = *traversal_profile_override;
    }

    RuntimeCausticLensTransport3D_DefaultLightSample(&lens_light);
    RuntimeCausticLensTransport3D_DefaultSample(&sample);
    runtime_caustic_transport_sphere_lens_sample(sample_index,
                                                 sample_count,
                                                 &aperture_u,
                                                 &aperture_v,
                                                 &lens_u,
                                                 &lens_v);
    light_distance = vec3_length(vec3_sub(shape.center, light->position));
    lens_light.position = light->position;
    lens_light.radius = fmax(light->radius, shape.radius * 0.018);
    lens_light.intensity = runtime_caustic_transport_light_attenuation(light,
                                                                       light_distance);
    lens_light.color = light->color;
    lens_light.lightIndex = light_index;
    sample_weight = runtime_caustic_transport_analytic_sphere_lens_sample_weight(path_budget,
                                                                                 sample_count);
    sample.apertureU = aperture_u * 0.42;
    sample.apertureV = aperture_v * 0.42;
    sample.lensU = lens_u * 0.82;
    sample.lensV = lens_v * 0.82;
    sample.sampleWeight = sample_weight;
    sample.receiverDistance = shape.radius * 5.0;

    diagnostics->evaluatedPathCount += 1u;
    diagnostics->analyticBowlLensEvaluatedPathCount += 1u;
    diagnostics->analyticBowlLensSampleWeight = sample_weight;
    diagnostics->analyticBowlLensTotalSampleWeight += sample_weight;
    if (!RuntimeCausticLensTransport3D_SolveBowlPath(&shape,
                                                     &lens_light,
                                                     &sample,
                                                     &path) ||
        !path.valid ||
        !(runtime_caustic_transport_luma(path.throughput) > 1.0e-9)) {
        return false;
    }
    volume_footprint_radius = runtime_caustic_transport_clamp(
        shape.radius * 0.040 + fmax(light->radius, 0.0) * 0.48,
        0.0,
        shape.radius * 0.35);

    lens_context.emissionPolicy = RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_BOWL_LENS;
    lens_context.eventType = "analytic_bowl_lens";
    lens_context.sceneObjectIndex = analytic_bowl->sceneObjectIndex;
    lens_context.primitiveIndex = analytic_bowl->primitiveIndex;
    lens_context.sampleIndex = sample_index;
    lens_context.payload = &analytic_bowl->payload;
    lens_context.volumeFootprintRadius = volume_footprint_radius;
    lens_context.emittedCounter = &diagnostics->analyticBowlLensEmittedPathCount;
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

void runtime_caustic_transport_emit_analytic_bowl_lens(
    const RuntimeScene3D* scene,
    const RuntimeLightSource3D* light,
    int light_index,
    const RuntimeCausticTransportAnalyticBowl3D* analytic_bowl,
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
    if (!scene || !light || !analytic_bowl || !diagnostics) return;
    if (sample_count <= 0) {
        sample_count = RUNTIME_CAUSTIC_TRANSPORT_SPHERE_LENS_SEED_SAMPLE_COUNT;
    }
    if (sample_count > RUNTIME_CAUSTIC_TRANSPORT_SPHERE_LENS_MAX_SAMPLE_COUNT) {
        sample_count = RUNTIME_CAUSTIC_TRANSPORT_SPHERE_LENS_MAX_SAMPLE_COUNT;
    }
    for (int sample_i = 0;
         sample_i < sample_count && (int)diagnostics->evaluatedPathCount < path_budget;
         ++sample_i) {
        (void)runtime_caustic_transport_emit_analytic_bowl_lens_sample(
            scene,
            light,
            light_index,
            analytic_bowl,
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
