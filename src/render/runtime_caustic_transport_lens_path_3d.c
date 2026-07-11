#include "render/runtime_caustic_transport_internal_3d.h"

#include <math.h>
#include <stdio.h>

static void runtime_caustic_transport_fill_lens_debug_path(
    RuntimeCausticTransportDebugPath3D* debug_path,
    const RuntimeLightSource3D* light,
    int light_index,
    const RuntimeCausticLensPath3D* path,
    const RuntimeCausticTransportLensPathDepositContext3D* lens_context,
    RuntimeCausticTransport3DDiagnostics* diagnostics) {
    const RuntimeCausticLensInterfaceEvent3D* entry_event =
        path->interfaceEventCount > 0u ? &path->events[0] : NULL;
    const RuntimeCausticLensInterfaceEvent3D* exit_event =
        path->interfaceEventCount > 1u ? &path->events[1] : NULL;
    const RuntimeMaterialPayload3D* payload = lens_context->payload;

    debug_path->pathId = diagnostics->evaluatedPathCount;
    snprintf(debug_path->emissionPolicy,
             sizeof(debug_path->emissionPolicy),
             "%s",
             RuntimeCausticTransportEmissionPolicy3D_Label(lens_context->emissionPolicy));
    debug_path->lightIndex = light_index;
    snprintf(debug_path->lightId,
             sizeof(debug_path->lightId),
             "%s",
             light->id[0] ? light->id : "compat_light");
    snprintf(debug_path->lightKind,
             sizeof(debug_path->lightKind),
             "%s",
             runtime_caustic_transport_light_kind_label(light->kind));
    debug_path->lightPosition = path->lightSamplePosition;
    debug_path->lightRadius = light->radius;
    debug_path->lightIntensity = light->intensity;
    debug_path->lightColor = light->color;
    debug_path->targetTriangleIndex = -1;
    debug_path->targetPrimitiveIndex = lens_context->primitiveIndex;
    debug_path->targetSceneObjectIndex = lens_context->sceneObjectIndex;
    debug_path->targetSampleIndex = lens_context->sampleIndex;
    debug_path->targetPosition = path->targetPosition;
    debug_path->targetDistance =
        entry_event ? vec3_length(vec3_sub(entry_event->position,
                                           path->lightSamplePosition))
                    : 0.0;
    debug_path->firstHitPosition = entry_event ? entry_event->position : path->targetPosition;
    debug_path->firstHitGeometricNormal =
        entry_event ? entry_event->normal : vec3(0.0, 0.0, 1.0);
    debug_path->firstHitOrientedNormal = debug_path->firstHitGeometricNormal;
    if (payload) {
        debug_path->materialId = payload->materialId;
        debug_path->transparency = payload->transparency;
        debug_path->opticalIor = payload->opticalIor;
        debug_path->bsdfIor = payload->bsdf.ior;
        debug_path->roughness = payload->bsdf.roughness;
        debug_path->reflectivity = payload->bsdf.reflectivity;
    }
    debug_path->eligible = true;
    snprintf(debug_path->eventType,
             sizeof(debug_path->eventType),
             "%s",
             lens_context->eventType ? lens_context->eventType : "analytic_lens");
    debug_path->outgoingDirection = path->postExitDirection;
    debug_path->throughput = path->throughput;
    debug_path->initialRadiance = path->throughput;
    snprintf(debug_path->lensShapeKind,
             sizeof(debug_path->lensShapeKind),
             "%s",
             RuntimeCausticLensTransport3D_ShapeKindLabel(path->shapeKind));
    debug_path->lensSceneObjectIndex = path->sceneObjectIndex;
    debug_path->lensPrimitiveIndex = path->primitiveIndex;
    debug_path->lensInterfaceEventCount = path->interfaceEventCount;
    debug_path->lensEntryPosition = entry_event ? entry_event->position : path->targetPosition;
    debug_path->lensEntryNormal =
        entry_event ? entry_event->normal : vec3(0.0, 0.0, 1.0);
    debug_path->lensEntryIncidentDirection =
        entry_event ? entry_event->incidentDirection : vec3(0.0, 0.0, 0.0);
    debug_path->lensEntryOutgoingDirection =
        entry_event ? entry_event->outgoingDirection : vec3(0.0, 0.0, 0.0);
    debug_path->lensEntryEtaFrom = entry_event ? entry_event->etaFrom : 0.0;
    debug_path->lensEntryEtaTo = entry_event ? entry_event->etaTo : 0.0;
    debug_path->lensEntryFresnel = entry_event ? entry_event->fresnel : 0.0;
    debug_path->lensEntryTotalInternalReflection =
        entry_event ? entry_event->totalInternalReflection : false;
    debug_path->lensExitPosition = exit_event ? exit_event->position : path->postExitOrigin;
    debug_path->lensExitNormal =
        exit_event ? exit_event->normal : vec3(0.0, 0.0, 1.0);
    debug_path->lensExitIncidentDirection =
        exit_event ? exit_event->incidentDirection : vec3(0.0, 0.0, 0.0);
    debug_path->lensExitOutgoingDirection =
        exit_event ? exit_event->outgoingDirection : path->postExitDirection;
    debug_path->lensExitEtaFrom = exit_event ? exit_event->etaFrom : 0.0;
    debug_path->lensExitEtaTo = exit_event ? exit_event->etaTo : 0.0;
    debug_path->lensExitFresnel = exit_event ? exit_event->fresnel : 0.0;
    debug_path->lensExitTotalInternalReflection =
        exit_event ? exit_event->totalInternalReflection : false;
    debug_path->lensPostExitOrigin = path->postExitOrigin;
    debug_path->lensPostExitDirection = path->postExitDirection;
    debug_path->lensReceiverCrossing = path->receiverCrossing;
    debug_path->lensInsideDistance = path->insideDistance;
    debug_path->lensSampleWeight = path->sampleWeight;
    debug_path->lensPathPdf = path->pathPdf;
    debug_path->lensTotalInternalReflection =
        debug_path->lensEntryTotalInternalReflection ||
        debug_path->lensExitTotalInternalReflection;
    debug_path->lensTraversalProfileKind = (int)path->traversalProfile.kind;
    debug_path->lensOutsideIor = path->traversalProfile.outsideIor;
    debug_path->lensMaterialIor = path->traversalProfile.materialIor;
    debug_path->lensFresnelScale = path->traversalProfile.fresnelScale;
    debug_path->lensTransmissionScale = path->traversalProfile.transmissionScale;
    debug_path->lensTint = path->traversalProfile.tint;
    debug_path->lensAbsorptionDistance = path->traversalProfile.absorptionDistance;
    debug_path->lensApertureRadiusScale = path->traversalProfile.apertureRadiusScale;
    debug_path->surfaceReceiverTriangleIndex = -1;
    debug_path->surfaceReceiverPrimitiveIndex = -1;
    debug_path->surfaceReceiverSceneObjectIndex = -1;
    if (lens_context->writeSphereLensCompatibilityFields) {
        debug_path->sphereLensEntryPosition =
            entry_event ? entry_event->position : path->targetPosition;
        debug_path->sphereLensExitPosition = path->postExitOrigin;
        debug_path->sphereLensReceiverCrossing = path->receiverCrossing;
        debug_path->sphereLensInsideDistance = path->insideDistance;
    }
    debug_path->insideSpecularObjectAfterEvent = false;
    debug_path->continuationEventCount = 1u;
    debug_path->exitedSpecularObjectBeforeVolumeDeposit = true;
    debug_path->mediumExitSceneObjectIndex = lens_context->sceneObjectIndex;
    debug_path->mediumExitPosition = path->postExitOrigin;
    debug_path->mediumExitDirection = path->postExitDirection;
}

bool runtime_caustic_transport_deposit_lens_path(
    const RuntimeScene3D* scene,
    const RuntimeLightSource3D* light,
    int light_index,
    const RuntimeCausticLensPath3D* path,
    const RuntimeCausticTransportLensPathDepositContext3D* lens_context,
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticSurfaceCache3D* surface_cache,
    int max_path_depth,
    double surface_footprint_scale,
    double surface_radiance_scale,
    const RuntimeCausticTransportSurfaceReceiverContext3D* receiver_context,
    RuntimeCausticTransport3DDiagnostics* diagnostics) {
    const RuntimeCausticLensInterfaceEvent3D* exit_event = NULL;
    RuntimeCausticTransportDebugPath3D debug_path = {0};
    Ray3D outgoing = {0};
    bool emitted = false;
    bool debug_enabled = RuntimeCausticTransportDebug3D_IsEnabled();

    if (!scene || !light || !path || !path->valid || !lens_context || !diagnostics) {
        return false;
    }

    exit_event = path->interfaceEventCount > 1u ? &path->events[1] : NULL;
    diagnostics->transparentHitCount += path->interfaceEventCount;
    diagnostics->specularEventCount += path->interfaceEventCount;
    outgoing = RuntimeRay3D_MakeOffset(path->postExitOrigin,
                                       exit_event ? exit_event->normal : vec3(0.0, 0.0, 1.0),
                                       path->postExitDirection,
                                       1.0e-4);

    if (debug_enabled) {
        runtime_caustic_transport_fill_lens_debug_path(&debug_path,
                                                       light,
                                                       light_index,
                                                       path,
                                                       lens_context,
                                                       diagnostics);
    }

    if (cache) {
        emitted = runtime_caustic_transport_deposit_segment(
                      scene,
                      cache,
                      &outgoing,
                      path->throughput,
                      lens_context->volumeFootprintRadius,
                      diagnostics,
                      debug_enabled ? &debug_path : NULL) ||
                  emitted;
    }
    if (surface_cache &&
        runtime_caustic_transport_deposit_surface(scene,
                                                  surface_cache,
                                                  &outgoing,
                                                  path->throughput,
                                                  false,
                                                  lens_context->sceneObjectIndex,
                                                  max_path_depth,
                                                  surface_footprint_scale,
                                                  surface_radiance_scale,
                                                  receiver_context,
                                                  diagnostics,
                                                  debug_enabled ? &debug_path : NULL)) {
        emitted = true;
    }

    if (!emitted) return false;
    diagnostics->emittedPathCount += 1u;
    if (lens_context->emittedCounter) {
        *lens_context->emittedCounter += 1u;
    }
    if (debug_enabled) {
        RuntimeCausticTransportDebug3D_RecordPath(&debug_path);
    }
    return true;
}
