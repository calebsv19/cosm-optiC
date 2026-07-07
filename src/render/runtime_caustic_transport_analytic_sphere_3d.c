#include "render/runtime_caustic_transport_internal_3d.h"

#include <math.h>
#include <stdio.h>

static bool runtime_caustic_transport_emit_analytic_sphere_lens_sample(
    const RuntimeScene3D* scene,
    const RuntimeLightSource3D* light,
    int light_index,
    const RuntimeCausticTransportAnalyticSphere3D* analytic_sphere,
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
    const RuntimeCausticLensInterfaceEvent3D* entry_event = NULL;
    const RuntimeCausticLensInterfaceEvent3D* exit_event = NULL;
    Ray3D outgoing = {0};
    double light_distance = 0.0;
    double aperture_u = 0.0;
    double aperture_v = 0.0;
    double lens_u = 0.0;
    double lens_v = 0.0;
    double sample_weight = 0.0;
    double volume_footprint_radius = 0.0;
    bool emitted = false;
    bool debug_enabled = RuntimeCausticTransportDebug3D_IsEnabled();
    RuntimeCausticTransportDebugPath3D debug_path = {0};

    if (!scene || !light || !analytic_sphere || !analytic_sphere->valid ||
        !diagnostics || sample_count <= 0) {
        return false;
    }

    RuntimeCausticSphereLens3D_DefaultLight(&lens_light);
    RuntimeCausticSphereLens3D_DefaultSample(&sample);
    runtime_caustic_transport_sphere_lens_sample(sample_index,
                                                 sample_count,
                                                 &aperture_u,
                                                 &aperture_v,
                                                 &lens_u,
                                                 &lens_v);
    light_distance = vec3_length(vec3_sub(analytic_sphere->sphere.center,
                                          light->position));
    lens_light.position = light->position;
    lens_light.radius = fmax(light->radius, analytic_sphere->sphere.radius * 0.025);
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
    sample.receiverPlaneZ = analytic_sphere->sphere.center.z - analytic_sphere->sphere.radius * 3.0;

    diagnostics->evaluatedPathCount += 1u;
    diagnostics->analyticSphereLensEvaluatedPathCount += 1u;
    diagnostics->analyticSphereLensSampleWeight = sample_weight;
    diagnostics->analyticSphereLensTotalSampleWeight += sample_weight;
    if (!RuntimeCausticLensTransport3D_SolveSpherePath(&analytic_sphere->sphere,
                                                       &lens_light,
                                                       &sample,
                                                       analytic_sphere->sceneObjectIndex,
                                                       analytic_sphere->primitiveIndex,
                                                       &path) ||
        !path.valid ||
        !(runtime_caustic_transport_luma(path.throughput) > 1.0e-9)) {
        return false;
    }
    entry_event = path.interfaceEventCount > 0u ? &path.events[0] : NULL;
    exit_event = path.interfaceEventCount > 1u ? &path.events[1] : NULL;

    diagnostics->transparentHitCount += 2u;
    diagnostics->specularEventCount += 2u;
    outgoing = RuntimeRay3D_MakeOffset(path.postExitOrigin,
                                       exit_event ? exit_event->normal : vec3(0.0, 0.0, 1.0),
                                       path.postExitDirection,
                                       1.0e-4);
    volume_footprint_radius = runtime_caustic_transport_clamp(
        analytic_sphere->sphere.radius * 0.045 + fmax(light->radius, 0.0) * 0.65,
        0.0,
        analytic_sphere->sphere.radius * 0.45);

    if (debug_enabled) {
        debug_path.pathId = diagnostics->evaluatedPathCount;
        snprintf(debug_path.emissionPolicy,
                 sizeof(debug_path.emissionPolicy),
                 "%s",
                 RuntimeCausticTransportEmissionPolicy3D_Label(
                     RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_SPHERE_LENS));
        debug_path.lightIndex = light_index;
        snprintf(debug_path.lightId,
                 sizeof(debug_path.lightId),
                 "%s",
                 light->id[0] ? light->id : "compat_light");
        snprintf(debug_path.lightKind,
                 sizeof(debug_path.lightKind),
                 "%s",
                 runtime_caustic_transport_light_kind_label(light->kind));
        debug_path.lightPosition = path.lightSamplePosition;
        debug_path.lightRadius = light->radius;
        debug_path.lightIntensity = light->intensity;
        debug_path.lightColor = light->color;
        debug_path.targetTriangleIndex = -1;
        debug_path.targetPrimitiveIndex = analytic_sphere->primitiveIndex;
        debug_path.targetSceneObjectIndex = analytic_sphere->sceneObjectIndex;
        debug_path.targetSampleIndex = sample_index;
        debug_path.targetPosition = path.targetPosition;
        debug_path.targetDistance = entry_event
                                        ? vec3_length(vec3_sub(entry_event->position,
                                                               path.lightSamplePosition))
                                        : 0.0;
        debug_path.firstHitPosition = entry_event ? entry_event->position : path.targetPosition;
        debug_path.firstHitGeometricNormal =
            entry_event ? entry_event->normal : vec3(0.0, 0.0, 1.0);
        debug_path.firstHitOrientedNormal = debug_path.firstHitGeometricNormal;
        debug_path.materialId = analytic_sphere->payload.materialId;
        debug_path.transparency = analytic_sphere->payload.transparency;
        debug_path.opticalIor = analytic_sphere->payload.opticalIor;
        debug_path.bsdfIor = analytic_sphere->payload.bsdf.ior;
        debug_path.roughness = analytic_sphere->payload.bsdf.roughness;
        debug_path.reflectivity = analytic_sphere->payload.bsdf.reflectivity;
        debug_path.eligible = true;
        snprintf(debug_path.eventType,
                 sizeof(debug_path.eventType),
                 "%s",
                 "analytic_sphere_lens");
        debug_path.outgoingDirection = path.postExitDirection;
        debug_path.throughput = path.throughput;
        debug_path.initialRadiance = path.throughput;
        snprintf(debug_path.lensShapeKind,
                 sizeof(debug_path.lensShapeKind),
                 "%s",
                 RuntimeCausticLensTransport3D_ShapeKindLabel(path.shapeKind));
        debug_path.lensSceneObjectIndex = path.sceneObjectIndex;
        debug_path.lensPrimitiveIndex = path.primitiveIndex;
        debug_path.lensInterfaceEventCount = path.interfaceEventCount;
        debug_path.lensEntryPosition = entry_event ? entry_event->position : path.targetPosition;
        debug_path.lensEntryNormal =
            entry_event ? entry_event->normal : vec3(0.0, 0.0, 1.0);
        debug_path.lensEntryIncidentDirection =
            entry_event ? entry_event->incidentDirection : vec3(0.0, 0.0, 0.0);
        debug_path.lensEntryOutgoingDirection =
            entry_event ? entry_event->outgoingDirection : vec3(0.0, 0.0, 0.0);
        debug_path.lensEntryEtaFrom = entry_event ? entry_event->etaFrom : 0.0;
        debug_path.lensEntryEtaTo = entry_event ? entry_event->etaTo : 0.0;
        debug_path.lensEntryFresnel = entry_event ? entry_event->fresnel : 0.0;
        debug_path.lensEntryTotalInternalReflection =
            entry_event ? entry_event->totalInternalReflection : false;
        debug_path.lensExitPosition = exit_event ? exit_event->position : path.postExitOrigin;
        debug_path.lensExitNormal =
            exit_event ? exit_event->normal : vec3(0.0, 0.0, 1.0);
        debug_path.lensExitIncidentDirection =
            exit_event ? exit_event->incidentDirection : vec3(0.0, 0.0, 0.0);
        debug_path.lensExitOutgoingDirection =
            exit_event ? exit_event->outgoingDirection : path.postExitDirection;
        debug_path.lensExitEtaFrom = exit_event ? exit_event->etaFrom : 0.0;
        debug_path.lensExitEtaTo = exit_event ? exit_event->etaTo : 0.0;
        debug_path.lensExitFresnel = exit_event ? exit_event->fresnel : 0.0;
        debug_path.lensExitTotalInternalReflection =
            exit_event ? exit_event->totalInternalReflection : false;
        debug_path.lensPostExitOrigin = path.postExitOrigin;
        debug_path.lensPostExitDirection = path.postExitDirection;
        debug_path.lensReceiverCrossing = path.receiverCrossing;
        debug_path.lensInsideDistance = path.insideDistance;
        debug_path.lensSampleWeight = path.sampleWeight;
        debug_path.lensPathPdf = path.pathPdf;
        debug_path.lensTotalInternalReflection =
            debug_path.lensEntryTotalInternalReflection ||
            debug_path.lensExitTotalInternalReflection;
        debug_path.sphereLensEntryPosition =
            entry_event ? entry_event->position : path.targetPosition;
        debug_path.sphereLensExitPosition = path.postExitOrigin;
        debug_path.sphereLensReceiverCrossing = path.receiverCrossing;
        debug_path.sphereLensInsideDistance = path.insideDistance;
        debug_path.insideSpecularObjectAfterEvent = false;
        debug_path.continuationEventCount = 1u;
        debug_path.exitedSpecularObjectBeforeVolumeDeposit = true;
        debug_path.mediumExitSceneObjectIndex = analytic_sphere->sceneObjectIndex;
        debug_path.mediumExitPosition = path.postExitOrigin;
        debug_path.mediumExitDirection = path.postExitDirection;
    }

    if (cache) {
        emitted = runtime_caustic_transport_deposit_segment(scene,
                                                            cache,
                                                            &outgoing,
                                                            path.throughput,
                                                            volume_footprint_radius,
                                                            diagnostics,
                                                            debug_enabled ? &debug_path : NULL) ||
                  emitted;
    }
    if (surface_cache &&
        runtime_caustic_transport_deposit_surface(scene,
                                                  surface_cache,
                                                  &outgoing,
                                                  path.throughput,
                                                  false,
                                                  analytic_sphere->sceneObjectIndex,
                                                  max_path_depth,
                                                  surface_footprint_scale,
                                                  surface_radiance_scale,
                                                  receiver_context,
                                                  diagnostics)) {
        emitted = true;
    }

    if (emitted) {
        diagnostics->emittedPathCount += 1u;
        diagnostics->analyticSphereLensEmittedPathCount += 1u;
        if (debug_enabled) {
            RuntimeCausticTransportDebug3D_RecordPath(&debug_path);
        }
        return true;
    }
    return false;
}
void runtime_caustic_transport_emit_analytic_sphere_lens(
    const RuntimeScene3D* scene,
    const RuntimeLightSource3D* light,
    int light_index,
    const RuntimeCausticTransportAnalyticSphere3D* analytic_sphere,
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
