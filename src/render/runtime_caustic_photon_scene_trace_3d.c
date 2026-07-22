#include "render/runtime_caustic_photon_scene_trace_3d.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "render/runtime_caustic_lens_transport_3d.h"
#include "render/runtime_caustic_photon_path_weight_3d.h"
#include "render/runtime_scene_accel_3d.h"

static double photon_scene_trace_clamp(double value, double min_value, double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double photon_scene_trace_luma(Vec3 value) {
    return 0.2126 * value.x + 0.7152 * value.y + 0.0722 * value.z;
}

static double photon_scene_trace_vec_error(Vec3 a, Vec3 b) {
    return fmax(fabs(a.x - b.x), fmax(fabs(a.y - b.y), fabs(a.z - b.z)));
}

static bool photon_scene_trace_default_material_resolver(
    const HitInfo3D* hit,
    RuntimeMaterialPayload3D* out_payload,
    void* user_data) {
    (void)user_data;
    return RuntimeMaterialPayload3D_ResolveFromHit(hit, out_payload);
}

static bool photon_scene_trace_append_event(RuntimeCausticPhotonTrace3D* trace,
                                            const RuntimeCausticPhotonEvent3D* event) {
    if (!trace || !event ||
        trace->eventCount >= RUNTIME_CAUSTIC_PHOTON_TRACE_MAX_EVENTS) {
        return false;
    }
    trace->events[trace->eventCount++] = *event;
    return true;
}

static RuntimeCausticPhotonSceneHitEvent3D* photon_scene_trace_append_hit(
    RuntimeCausticPhotonSceneTrace3D* result,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* material,
    uint32_t depth) {
    RuntimeCausticPhotonSceneHitEvent3D* hit_event = NULL;
    if (!result || !hit || !material ||
        result->readback.hitEventCount >=
            RUNTIME_CAUSTIC_PHOTON_TRACE_MAX_DIELECTRIC_EVENTS) {
        return NULL;
    }
    hit_event = &result->hitEvents[result->readback.hitEventCount++];
    memset(hit_event, 0, sizeof(*hit_event));
    hit_event->depth = depth;
    hit_event->hit = *hit;
    hit_event->material = *material;
    return hit_event;
}

static bool photon_scene_trace_append_dielectric(
    RuntimeCausticPhotonTrace3D* trace,
    const RuntimeCausticPhotonDielectricEvent3D* dielectric) {
    if (!trace || !dielectric ||
        trace->dielectricEventCount >= RUNTIME_CAUSTIC_PHOTON_TRACE_MAX_DIELECTRIC_EVENTS) {
        return false;
    }
    trace->dielectricEvents[trace->dielectricEventCount++] = *dielectric;
    return true;
}

static void photon_scene_trace_set_terminal(
    RuntimeCausticPhotonSceneTrace3D* result,
    RuntimeCausticPhotonSceneTermination3D termination,
    RuntimeCausticPhotonRejectReason3D reject_reason) {
    RuntimeCausticPhotonTrace3D* trace = NULL;
    if (!result) return;
    trace = &result->trace;
    result->readback.termination = termination;
    trace->finalState.active = false;
    trace->finalState.terminated = true;
    trace->finalState.rejectReason = reject_reason;
    trace->debug.rejectedPhotonCount = 1u;
    trace->debug.lastRejectReason = reject_reason;
    trace->debug.rejectedFlux = trace->finalState.throughput;
    trace->valid = false;
}

void RuntimeCausticPhotonSceneTrace3D_DefaultSettings(
    RuntimeCausticPhotonSceneTraceSettings3D* settings) {
    if (!settings) return;
    memset(settings, 0, sizeof(*settings));
    settings->maxDepth = RUNTIME_CAUSTIC_PHOTON_TRACE_MAX_DIELECTRIC_EVENTS;
    settings->tMin = 1.0e-6;
    settings->tMax = 1.0e6;
    settings->rayOffset = 1.0e-5;
    settings->minTransportWeightLuma = 0.0;
    settings->traceRoute = RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS;
    settings->materialResolver = photon_scene_trace_default_material_resolver;
}

const char* RuntimeCausticPhotonSceneTermination3D_Label(
    RuntimeCausticPhotonSceneTermination3D termination) {
    switch (termination) {
        case RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_DIELECTRIC_EXIT:
            return "dielectric_exit";
        case RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_ESCAPED:
            return "escaped";
        case RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_MATERIAL_UNRESOLVED:
            return "material_unresolved";
        case RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_OPAQUE_SURFACE:
            return "opaque_surface";
        case RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_MAX_DEPTH:
            return "max_depth";
        case RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_DIFFERENT_OBJECT_BEFORE_EXIT:
            return "different_object_before_exit";
        case RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_TIR_DEFERRED:
            return "tir_deferred";
        case RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_EVENT_READY:
            return "bsdf_event_ready";
        case RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_EMISSIVE:
            return "bsdf_emissive";
        case RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_ABSORBED:
            return "bsdf_absorbed";
        case RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_DIRECTION_INVALID:
            return "bsdf_direction_invalid";
        case RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_ROULETTE_TERMINATED:
            return "bsdf_roulette_terminated";
        case RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_MEDIUM_TRANSITION_REJECTED:
            return "medium_transition_rejected";
        case RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_TRACE_ERROR:
            return "trace_error";
        case RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_NONE:
        default:
            return "none";
    }
}

bool RuntimeCausticPhotonSceneTrace3D_TraceDeterministicDielectric(
    const RuntimeScene3D* scene,
    const RuntimeCausticPhotonSample3D* sample,
    const RuntimeCausticPhotonSceneTraceSettings3D* settings,
    RuntimeCausticPhotonSceneTrace3D* out_trace) {
    RuntimeCausticPhotonSceneTraceSettings3D defaults;
    const RuntimeCausticPhotonSceneTraceSettings3D* active = settings;
    RuntimeCausticPhotonSceneTrace3D result;
    RuntimeCausticPhotonTrace3D* trace = &result.trace;
    RuntimeRay3DTraceContext ray_context;
    RuntimeCausticPhotonPathState3D state;
    RuntimeCausticPhotonEvent3D event;
    Ray3D ray;
    int active_object = -1;
    Vec3 entry_position = vec3(0.0, 0.0, 0.0);
    RuntimeCausticLensTraversalProfile3D profile;

    if (out_trace) memset(out_trace, 0, sizeof(*out_trace));
    if (!scene || !sample || !out_trace) return false;
    if (!active) {
        RuntimeCausticPhotonSceneTrace3D_DefaultSettings(&defaults);
        active = &defaults;
    }

    memset(&result, 0, sizeof(result));
    result.readback.attempted = true;
    RuntimeCausticPhotonMediumStack3D_Init(&result.initialMediumStack);
    result.finalMediumStack = result.initialMediumStack;
    trace->sample = *sample;
    RuntimeCausticPhotonTrace3D_InitPathState(sample, &state);
    trace->initialState = state;
    trace->finalState = state;
    trace->debug.photonId = sample->photonId;
    trace->debug.emittedFlux = sample->flux;

    memset(&event, 0, sizeof(event));
    event.photonId = sample->photonId;
    event.kind = RUNTIME_CAUSTIC_PHOTON_EVENT_EMISSION;
    event.sceneObjectIndex = -1;
    event.primitiveIndex = -1;
    event.triangleIndex = -1;
    event.position = sample->position;
    event.incidentDirection = state.direction;
    event.outgoingDirection = state.direction;
    event.throughput = state.throughput;
    event.pathPdf = state.pathPdf;
    if (!photon_scene_trace_append_event(trace, &event)) return false;

    RuntimeRay3DTraceContext_Init(&ray_context);
    RuntimeRay3DTraceContext_SetTraceRoute(&ray_context, active->traceRoute);
    RuntimeRay3DTraceContext_SetSceneAccelerationTraceFirstHit(
        &ray_context,
        (RuntimeRay3DSceneAccelerationTraceFirstHitFn)
            RuntimeSceneAcceleration3D_TraceFirstHit);
    ray = RuntimeRay3D_Make(state.position, state.direction);
    RuntimeCausticLensTransport3D_DefaultTraversalProfile(&profile);

    for (uint32_t depth = 1u; depth <= active->maxDepth; ++depth) {
        HitInfo3D hit;
        RuntimeMaterialPayload3D material;
        RuntimeCausticPhotonSceneHitEvent3D* hit_event = NULL;
        RuntimeCausticPhotonDielectricEvent3D dielectric;
        bool found = false;
        bool entering = active_object < 0;
        bool tir = false;
        Vec3 outgoing = vec3(0.0, 0.0, 0.0);
        Vec3 interface_normal;
        Vec3 throughput_before = state.throughput;
        Vec3 throughput_after = state.throughput;
        double eta_from = 1.0;
        double eta_to = 1.0;
        double fresnel = 0.0;
        double branch_pdf = 0.0;
        double distance_in_medium = 0.0;

        HitInfo3D_Reset(&hit);
        result.readback.intersectionCount++;
        found = RuntimeRay3D_TraceSceneFirstHitWithContext(&ray_context,
                                                           scene,
                                                           &ray,
                                                           active->tMin,
                                                           active->tMax,
                                                           &hit);
        if (!found) {
            photon_scene_trace_set_terminal(&result,
                                            RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_ESCAPED,
                                            RUNTIME_CAUSTIC_PHOTON_REJECT_ESCAPED_SCENE);
            break;
        }

        RuntimeMaterialPayload3D_Reset(&material);
        if (!active->materialResolver ||
            !active->materialResolver(&hit,
                                      &material,
                                      active->materialResolverUserData) ||
            !material.valid) {
            result.readback.materialResolveFailureCount++;
            photon_scene_trace_set_terminal(
                &result,
                RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_MATERIAL_UNRESOLVED,
                RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_MEDIUM);
            break;
        }
        result.readback.materialResolveCount++;
        hit_event = photon_scene_trace_append_hit(&result, &hit, &material, depth);
        if (!hit_event) {
            photon_scene_trace_set_terminal(
                &result,
                RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_MAX_DEPTH,
                RUNTIME_CAUSTIC_PHOTON_REJECT_MAX_DEPTH);
            break;
        }

        if (!(material.transparency > 1.0e-9) && !material.thinWalled) {
            memset(&event, 0, sizeof(event));
            event.photonId = sample->photonId;
            event.depth = depth;
            event.kind = RUNTIME_CAUSTIC_PHOTON_EVENT_SURFACE;
            event.sceneObjectIndex = hit.sceneObjectIndex;
            event.primitiveIndex = hit.primitiveIndex;
            event.triangleIndex = hit.triangleIndex;
            event.position = hit.position;
            event.normal = hit.normal;
            event.incidentDirection = ray.direction;
            event.throughput = state.throughput;
            event.pathPdf = state.pathPdf;
            photon_scene_trace_append_event(trace, &event);
            hit_event->termination =
                RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_OPAQUE_SURFACE;
            photon_scene_trace_set_terminal(
                &result,
                RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_OPAQUE_SURFACE,
                RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_MEDIUM);
            break;
        }

        if (!entering && hit.sceneObjectIndex != active_object) {
            hit_event->termination =
                RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_DIFFERENT_OBJECT_BEFORE_EXIT;
            photon_scene_trace_set_terminal(
                &result,
                RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_DIFFERENT_OBJECT_BEFORE_EXIT,
                RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_MEDIUM);
            break;
        }

        if (entering) {
            RuntimeCausticLensTransport3D_ResolveTraversalProfileFromPayload(&material,
                                                                             &profile);
            active_object = hit.sceneObjectIndex;
            entry_position = hit.position;
            eta_from = profile.outsideIor;
            eta_to = profile.materialIor;
            interface_normal = hit.normal;
        } else {
            eta_from = profile.materialIor;
            eta_to = profile.outsideIor;
            interface_normal = vec3_scale(hit.normal, -1.0);
            distance_in_medium = vec3_length(vec3_sub(hit.position, entry_position));
            throughput_before = RuntimeCausticLensTransport3D_ApplyAbsorptionTintProfile(
                throughput_before,
                distance_in_medium,
                &profile);
        }

        fresnel = RuntimeCausticLensTransport3D_FresnelSchlick(ray.direction,
                                                               interface_normal,
                                                               eta_from,
                                                               eta_to);
        if (!RuntimeCausticLensTransport3D_Refract(ray.direction,
                                                   interface_normal,
                                                   eta_from,
                                                   eta_to,
                                                   &outgoing,
                                                   &tir)) {
            memset(&dielectric, 0, sizeof(dielectric));
            dielectric.photonId = sample->photonId;
            dielectric.depth = depth;
            dielectric.sceneObjectIndex = hit.sceneObjectIndex;
            dielectric.primitiveIndex = hit.primitiveIndex;
            dielectric.triangleIndex = hit.triangleIndex;
            dielectric.position = hit.position;
            dielectric.normal = interface_normal;
            dielectric.incidentDirection = ray.direction;
            dielectric.selectedDirection = ray.direction;
            dielectric.throughputBefore = throughput_before;
            dielectric.throughputAfter = throughput_before;
            dielectric.etaFrom = eta_from;
            dielectric.etaTo = eta_to;
            dielectric.fresnel = fresnel;
            dielectric.branchPdf = 1.0;
            dielectric.distanceInMedium = distance_in_medium;
            dielectric.selectedBranch = tir
                                            ? RUNTIME_CAUSTIC_PHOTON_BRANCH_TOTAL_INTERNAL_REFLECTION
                                            : RUNTIME_CAUSTIC_PHOTON_BRANCH_ABSORBED;
            dielectric.totalInternalReflection = tir;
            photon_scene_trace_append_dielectric(trace, &dielectric);
            hit_event->dielectric = dielectric;
            hit_event->termination =
                tir ? RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_TIR_DEFERRED
                    : RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_TRACE_ERROR;
            photon_scene_trace_set_terminal(
                &result,
                tir ? RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_TIR_DEFERRED
                    : RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_TRACE_ERROR,
                RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_MEDIUM);
            break;
        }

        branch_pdf = 1.0 - photon_scene_trace_clamp(
                                   fresnel * profile.fresnelScale,
                                   0.0,
                                   1.0);
        throughput_after = RuntimeCausticLensTransport3D_ApplyInterfaceTransmissionProfile(
            throughput_before,
            fresnel,
            &profile);
        if (!RuntimeCausticPhotonPathWeight3D_ApplyThroughputRatio(
                state.transportWeight,
                state.throughput,
                throughput_after,
                &state.transportWeight)) {
            photon_scene_trace_set_terminal(
                &result,
                RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_TRACE_ERROR,
                RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_MEDIUM);
            break;
        }
        state.depth = depth;
        state.position = hit.position;
        state.direction = outgoing;
        state.throughput = throughput_after;
        state.pathPdf *= branch_pdf > 1.0e-12 ? branch_pdf : 1.0;

        memset(&dielectric, 0, sizeof(dielectric));
        dielectric.photonId = sample->photonId;
        dielectric.depth = depth;
        dielectric.sceneObjectIndex = hit.sceneObjectIndex;
        dielectric.primitiveIndex = hit.primitiveIndex;
        dielectric.triangleIndex = hit.triangleIndex;
        dielectric.position = hit.position;
        dielectric.normal = interface_normal;
        dielectric.incidentDirection = ray.direction;
        dielectric.refractedDirection = outgoing;
        dielectric.selectedDirection = outgoing;
        dielectric.throughputBefore = throughput_before;
        dielectric.throughputAfter = throughput_after;
        dielectric.etaFrom = eta_from;
        dielectric.etaTo = eta_to;
        dielectric.fresnel = fresnel;
        dielectric.branchPdf = branch_pdf;
        dielectric.distanceInMedium = distance_in_medium;
        dielectric.selectedBranch = RUNTIME_CAUSTIC_PHOTON_BRANCH_REFRACTED;
        if (!photon_scene_trace_append_dielectric(trace, &dielectric)) {
            photon_scene_trace_set_terminal(
                &result,
                RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_MAX_DEPTH,
                RUNTIME_CAUSTIC_PHOTON_REJECT_MAX_DEPTH);
            break;
        }
        hit_event->dielectric = dielectric;
        hit_event->termination =
            entering ? RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_NONE
                     : RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_DIELECTRIC_EXIT;

        memset(&event, 0, sizeof(event));
        event.photonId = sample->photonId;
        event.depth = depth;
        event.kind = RUNTIME_CAUSTIC_PHOTON_EVENT_DIELECTRIC;
        event.sceneObjectIndex = hit.sceneObjectIndex;
        event.primitiveIndex = hit.primitiveIndex;
        event.triangleIndex = hit.triangleIndex;
        event.position = hit.position;
        event.normal = interface_normal;
        event.incidentDirection = ray.direction;
        event.outgoingDirection = outgoing;
        event.throughput = throughput_after;
        event.pathPdf = state.pathPdf;
        if (!photon_scene_trace_append_event(trace, &event)) {
            photon_scene_trace_set_terminal(
                &result,
                RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_MAX_DEPTH,
                RUNTIME_CAUSTIC_PHOTON_REJECT_MAX_DEPTH);
            break;
        }
        trace->debug.refractedBranchCount++;

        if (photon_scene_trace_luma(state.transportWeight) <=
            active->minTransportWeightLuma) {
            photon_scene_trace_set_terminal(
                &result,
                RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_OPAQUE_SURFACE,
                RUNTIME_CAUSTIC_PHOTON_REJECT_BELOW_FLUX_THRESHOLD);
            break;
        }

        if (!entering) {
            trace->finalState = state;
            trace->finalState.active = true;
            trace->finalState.terminated = false;
            trace->finalState.rejectReason = RUNTIME_CAUSTIC_PHOTON_REJECT_NONE;
            trace->postExitOrigin = hit.position;
            trace->postExitDirection = outgoing;
            trace->insideDistance = distance_in_medium;
            trace->debug.eventCount = trace->eventCount;
            trace->debug.rejectedFlux = vec3(fmax(sample->flux.x - throughput_after.x, 0.0),
                                             fmax(sample->flux.y - throughput_after.y, 0.0),
                                             fmax(sample->flux.z - throughput_after.z, 0.0));
            trace->valid = true;
            result.readback.succeeded = true;
            result.readback.termination =
                RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_DIELECTRIC_EXIT;
            break;
        }

        trace->finalState = state;
        ray = RuntimeRay3D_MakeOffset(hit.position,
                                      interface_normal,
                                      outgoing,
                                      active->rayOffset);
    }

    if (!result.readback.succeeded &&
        result.readback.termination == RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_NONE) {
        photon_scene_trace_set_terminal(&result,
                                        RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_MAX_DEPTH,
                                        RUNTIME_CAUSTIC_PHOTON_REJECT_MAX_DEPTH);
    }
    RuntimeRay3DTraceContext_SnapshotRouteStats(&ray_context,
                                                &result.readback.routeStats);
    result.readback.usedSharedSceneAccelerationRoute =
        result.readback.routeStats.tlasTraceCalls > 0u;
    trace->debug.eventCount = trace->eventCount;
    *out_trace = result;
    return result.readback.succeeded;
}

bool RuntimeCausticPhotonSceneTrace3D_TraceDeterministicBsdfHit(
    const RuntimeScene3D* scene,
    const RuntimeCausticPhotonSample3D* sample,
    const RuntimeCausticPhotonSceneBsdfSample3D* bsdf_sample,
    const RuntimeCausticPhotonSceneTraceSettings3D* settings,
    RuntimeCausticPhotonSceneTrace3D* out_trace) {
    RuntimeCausticPhotonSceneTraceSettings3D defaults;
    const RuntimeCausticPhotonSceneTraceSettings3D* active = settings;
    RuntimeCausticPhotonSceneTrace3D result;
    RuntimeCausticPhotonTrace3D* trace = &result.trace;
    RuntimeCausticPhotonPathState3D state;
    RuntimeCausticPhotonEvent3D event;
    RuntimeRay3DTraceContext ray_context;
    RuntimeCausticPhotonSceneHitEvent3D* hit_event;
    RuntimeMaterialPayload3D material;
    HitInfo3D hit;
    Ray3D ray;
    bool found;
    bool terminal;
    double incident_cosine;

    if (out_trace) memset(out_trace, 0, sizeof(*out_trace));
    if (!scene || !sample || !bsdf_sample || !out_trace ||
        !isfinite(bsdf_sample->lobeUnitSample) ||
        !isfinite(bsdf_sample->directionSample.unitU) ||
        !isfinite(bsdf_sample->directionSample.unitV)) {
        return false;
    }
    if (!active) {
        RuntimeCausticPhotonSceneTrace3D_DefaultSettings(&defaults);
        active = &defaults;
    }

    memset(&result, 0, sizeof(result));
    result.readback.attempted = true;
    RuntimeCausticPhotonMediumStack3D_Init(&result.initialMediumStack);
    result.finalMediumStack = result.initialMediumStack;
    trace->sample = *sample;
    RuntimeCausticPhotonTrace3D_InitPathState(sample, &state);
    trace->initialState = state;
    trace->finalState = state;
    trace->debug.photonId = sample->photonId;
    trace->debug.emittedFlux = sample->flux;

    memset(&event, 0, sizeof(event));
    event.photonId = sample->photonId;
    event.kind = RUNTIME_CAUSTIC_PHOTON_EVENT_EMISSION;
    event.sceneObjectIndex = -1;
    event.primitiveIndex = -1;
    event.triangleIndex = -1;
    event.position = sample->position;
    event.incidentDirection = state.direction;
    event.outgoingDirection = state.direction;
    event.throughput = state.throughput;
    event.pathPdf = state.pathPdf;
    if (!photon_scene_trace_append_event(trace, &event)) return false;

    RuntimeRay3DTraceContext_Init(&ray_context);
    RuntimeRay3DTraceContext_SetTraceRoute(&ray_context, active->traceRoute);
    RuntimeRay3DTraceContext_SetSceneAccelerationTraceFirstHit(
        &ray_context,
        (RuntimeRay3DSceneAccelerationTraceFirstHitFn)
            RuntimeSceneAcceleration3D_TraceFirstHit);
    ray = RuntimeRay3D_Make(state.position, state.direction);
    HitInfo3D_Reset(&hit);
    result.readback.intersectionCount = 1u;
    found = RuntimeRay3D_TraceSceneFirstHitWithContext(&ray_context,
                                                       scene,
                                                       &ray,
                                                       active->tMin,
                                                       active->tMax,
                                                       &hit);
    if (!found) {
        photon_scene_trace_set_terminal(&result,
                                        RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_ESCAPED,
                                        RUNTIME_CAUSTIC_PHOTON_REJECT_ESCAPED_SCENE);
        goto finish;
    }

    RuntimeMaterialPayload3D_Reset(&material);
    if (!active->materialResolver ||
        !active->materialResolver(&hit,
                                  &material,
                                  active->materialResolverUserData) ||
        !material.valid) {
        result.readback.materialResolveFailureCount = 1u;
        photon_scene_trace_set_terminal(
            &result,
            RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_MATERIAL_UNRESOLVED,
            RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_MEDIUM);
        goto finish;
    }
    result.readback.materialResolveCount = 1u;
    hit_event = photon_scene_trace_append_hit(&result, &hit, &material, 1u);
    if (!hit_event) {
        photon_scene_trace_set_terminal(&result,
                                        RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_MAX_DEPTH,
                                        RUNTIME_CAUSTIC_PHOTON_REJECT_MAX_DEPTH);
        goto finish;
    }
    hit_event->bsdfSampleStream.valid = true;
    hit_event->bsdfSampleStream.depth = 1u;
    hit_event->bsdfSampleStream.bsdfSample = *bsdf_sample;

    incident_cosine = fabs(vec3_dot(vec3_scale(state.direction, -1.0),
                                   vec3_normalize(hit.normal)));
    if (!RuntimeCausticPhotonBsdfPolicy3D_Build(&material,
                                                incident_cosine,
                                                state.throughput,
                                                &hit_event->bsdfPolicy) ||
        !RuntimeCausticPhotonBsdfPolicy3D_Select(&hit_event->bsdfPolicy,
                                                 bsdf_sample->lobeUnitSample,
                                                 &hit_event->bsdfSelection)) {
        hit_event->termination =
            RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_DIRECTION_INVALID;
        photon_scene_trace_set_terminal(
            &result,
            RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_DIRECTION_INVALID,
            RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_MEDIUM);
        goto finish;
    }

    terminal = hit_event->bsdfSelection.termination !=
               RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_NONE;
    if (!terminal &&
        !RuntimeCausticPhotonBsdfDirection3D_Sample(
            hit_event->bsdfSelection.lobe,
            &material,
            state.direction,
            hit.normal,
            &bsdf_sample->directionSample,
            &hit_event->bsdfDirection)) {
        hit_event->termination = hit_event->bsdfDirection.totalInternalReflection
                                     ? RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_TIR_DEFERRED
                                     : RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_DIRECTION_INVALID;
        photon_scene_trace_set_terminal(
            &result,
            hit_event->termination,
            RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_MEDIUM);
        goto finish;
    }

    if (!RuntimeCausticPhotonPathWeight3D_ApplyThroughputRatio(
            state.transportWeight,
            state.throughput,
            hit_event->bsdfSelection.throughputAfter,
            &state.transportWeight)) {
        photon_scene_trace_set_terminal(
            &result,
            RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_TRACE_ERROR,
            RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_MEDIUM);
        goto finish;
    }
    state.depth = 1u;
    state.position = hit.position;
    state.throughput = hit_event->bsdfSelection.throughputAfter;
    state.pathPdf *= hit_event->bsdfSelection.branchPdf;
    if (!terminal) {
        state.direction = hit_event->bsdfDirection.outgoingDirection;
        state.pathPdf *= hit_event->bsdfDirection.angularPdf;
    }

    memset(&event, 0, sizeof(event));
    event.photonId = sample->photonId;
    event.depth = 1u;
    event.kind = terminal ? RUNTIME_CAUSTIC_PHOTON_EVENT_TERMINATED
                          : RUNTIME_CAUSTIC_PHOTON_EVENT_SURFACE;
    event.sceneObjectIndex = hit.sceneObjectIndex;
    event.primitiveIndex = hit.primitiveIndex;
    event.triangleIndex = hit.triangleIndex;
    event.position = hit.position;
    event.normal = hit.normal;
    event.incidentDirection = ray.direction;
    event.outgoingDirection = terminal ? vec3(0.0, 0.0, 0.0) : state.direction;
    event.throughput = state.throughput;
    event.pathPdf = state.pathPdf;
    if (!photon_scene_trace_append_event(trace, &event)) {
        photon_scene_trace_set_terminal(&result,
                                        RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_MAX_DEPTH,
                                        RUNTIME_CAUSTIC_PHOTON_REJECT_MAX_DEPTH);
        goto finish;
    }

    if (hit_event->bsdfSelection.termination ==
        RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_EMISSIVE) {
        hit_event->termination = RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_EMISSIVE;
        state.active = false;
        state.terminated = true;
    } else if (hit_event->bsdfSelection.termination ==
               RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_ABSORBED) {
        hit_event->termination = RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_ABSORBED;
        state.active = false;
        state.terminated = true;
        state.rejectReason = RUNTIME_CAUSTIC_PHOTON_REJECT_BELOW_FLUX_THRESHOLD;
    } else {
        hit_event->termination = RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_EVENT_READY;
        state.active = true;
        state.terminated = false;
        state.rejectReason = RUNTIME_CAUSTIC_PHOTON_REJECT_NONE;
    }
    trace->finalState = state;
    trace->postExitOrigin = hit.position;
    trace->postExitDirection = event.outgoingDirection;
    trace->valid = true;
    result.readback.succeeded = true;
    result.readback.termination = hit_event->termination;

finish:
    RuntimeRay3DTraceContext_SnapshotRouteStats(&ray_context,
                                                &result.readback.routeStats);
    result.readback.usedSharedSceneAccelerationRoute =
        result.readback.routeStats.tlasTraceCalls > 0u;
    trace->debug.eventCount = trace->eventCount;
    *out_trace = result;
    return result.readback.succeeded;
}

bool RuntimeCausticPhotonSceneTrace3D_TraceSeededBsdfHit(
    const RuntimeScene3D* scene,
    const RuntimeCausticPhotonSample3D* sample,
    uint32_t depth,
    const RuntimeCausticPhotonSceneTraceSettings3D* settings,
    RuntimeCausticPhotonSceneTrace3D* out_trace) {
    RuntimeCausticPhotonBsdfSampleStream3D stream;
    bool succeeded;

    if (out_trace) memset(out_trace, 0, sizeof(*out_trace));
    if (!out_trace ||
        !RuntimeCausticPhotonBsdfSampling3D_Generate(sample, depth, &stream)) {
        return false;
    }
    succeeded = RuntimeCausticPhotonSceneTrace3D_TraceDeterministicBsdfHit(
        scene,
        sample,
        &stream.bsdfSample,
        settings,
        out_trace);
    if (out_trace->readback.hitEventCount > 0u) {
        out_trace->hitEvents[0].bsdfSampleStream = stream;
        out_trace->hitEvents[0].usedSeededBsdfSamples = true;
    }
    return succeeded;
}

bool RuntimeCausticPhotonSceneTrace3D_TraceSeededBsdfHitWithRoulette(
    const RuntimeScene3D* scene,
    const RuntimeCausticPhotonSample3D* sample,
    uint32_t depth,
    const RuntimePathDepthPolicy3D* roulette_policy,
    const RuntimeCausticPhotonSceneTraceSettings3D* settings,
    RuntimeCausticPhotonSceneTrace3D* out_trace) {
    RuntimeCausticPhotonSceneHitEvent3D* hit_event;
    RuntimeCausticPhotonEvent3D* event;
    Vec3 physical_throughput_before_roulette;

    if (!RuntimeCausticPhotonSceneTrace3D_TraceSeededBsdfHit(
            scene, sample, depth, settings, out_trace)) {
        return false;
    }
    if (!out_trace || out_trace->readback.hitEventCount == 0u ||
        out_trace->trace.eventCount == 0u) {
        return false;
    }
    hit_event = &out_trace->hitEvents[0];
    if (hit_event->termination !=
        RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_EVENT_READY) {
        return true;
    }
    if (!RuntimeCausticPhotonBsdfSampling3D_EvaluateRoulette(
            roulette_policy,
            depth,
            out_trace->trace.finalState.transportWeight,
            hit_event->bsdfSampleStream.rouletteUnitSample,
            &hit_event->roulette)) {
        return false;
    }

    event = &out_trace->trace.events[out_trace->trace.eventCount - 1u];
    physical_throughput_before_roulette =
        out_trace->trace.finalState.throughput;
    out_trace->trace.finalState.pathPdf *= hit_event->roulette.branchPdf;
    out_trace->trace.finalState.transportWeight =
        hit_event->roulette.throughputAfter;
    if (hit_event->roulette.terminated) {
        out_trace->trace.finalState.throughput = vec3(0.0, 0.0, 0.0);
    } else if (hit_event->roulette.evaluated) {
        out_trace->trace.finalState.throughput = vec3_scale(
            physical_throughput_before_roulette,
            1.0 / hit_event->roulette.survivalProbability);
    }
    event->pathPdf = out_trace->trace.finalState.pathPdf;
    event->throughput = out_trace->trace.finalState.throughput;
    if (!hit_event->roulette.terminated) {
        return true;
    }

    event->kind = RUNTIME_CAUSTIC_PHOTON_EVENT_TERMINATED;
    out_trace->trace.finalState.active = false;
    out_trace->trace.finalState.terminated = true;
    out_trace->trace.finalState.rejectReason =
        RUNTIME_CAUSTIC_PHOTON_REJECT_RUSSIAN_ROULETTE;
    out_trace->trace.debug.rejectedPhotonCount = 1u;
    out_trace->trace.debug.lastRejectReason =
        RUNTIME_CAUSTIC_PHOTON_REJECT_RUSSIAN_ROULETTE;
    out_trace->trace.debug.rejectedFlux = physical_throughput_before_roulette;
    hit_event->termination =
        RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_ROULETTE_TERMINATED;
    out_trace->readback.termination = hit_event->termination;
    return true;
}

bool RuntimeCausticPhotonSceneTrace3D_CompareDescriptorOracle(
    const RuntimeCausticPhotonTrace3D* descriptor_trace,
    const RuntimeCausticPhotonSceneTrace3D* scene_trace,
    double direction_tolerance,
    double flux_tolerance,
    RuntimeCausticPhotonSceneTraceParity3D* out_parity) {
    RuntimeCausticPhotonSceneTraceParity3D parity;
    const RuntimeCausticPhotonTrace3D* general = NULL;
    uint32_t event_count = 0u;

    memset(&parity, 0, sizeof(parity));
    if (out_parity) *out_parity = parity;
    if (!descriptor_trace || !scene_trace || !out_parity) return false;
    general = &scene_trace->trace;
    parity.compared = true;
    direction_tolerance = fmax(direction_tolerance, 0.0);
    flux_tolerance = fmax(flux_tolerance, 0.0);
    event_count = descriptor_trace->dielectricEventCount;

    parity.identityMatches =
        event_count == general->dielectricEventCount && event_count > 0u;
    parity.directionMatches = parity.identityMatches;
    parity.branchMatches = parity.identityMatches;
    parity.triangleIdentityPreserved = parity.identityMatches;
    for (uint32_t i = 0u; i < event_count && i < general->dielectricEventCount; ++i) {
        const RuntimeCausticPhotonDielectricEvent3D* expected =
            &descriptor_trace->dielectricEvents[i];
        const RuntimeCausticPhotonDielectricEvent3D* actual =
            &general->dielectricEvents[i];
        double direction_error = photon_scene_trace_vec_error(
            expected->selectedDirection,
            actual->selectedDirection);
        if (expected->sceneObjectIndex != actual->sceneObjectIndex ||
            expected->primitiveIndex != actual->primitiveIndex) {
            parity.identityMatches = false;
        }
        if (actual->triangleIndex < 0) parity.triangleIdentityPreserved = false;
        if (expected->selectedBranch != actual->selectedBranch) {
            parity.branchMatches = false;
        }
        if (direction_error > parity.maxDirectionError) {
            parity.maxDirectionError = direction_error;
        }
        if (direction_error > direction_tolerance) parity.directionMatches = false;
    }
    parity.maxFluxError = photon_scene_trace_vec_error(
        descriptor_trace->finalState.throughput,
        general->finalState.throughput);
    parity.terminationMatches =
        descriptor_trace->finalState.terminated == general->finalState.terminated &&
        descriptor_trace->finalState.rejectReason == general->finalState.rejectReason;
    parity.matches = parity.identityMatches && parity.directionMatches &&
                     parity.branchMatches && parity.terminationMatches &&
                     parity.triangleIdentityPreserved &&
                     parity.maxFluxError <= flux_tolerance;
    if (!parity.identityMatches) {
        snprintf(parity.mismatchReason, sizeof(parity.mismatchReason), "identity");
    } else if (!parity.triangleIdentityPreserved) {
        snprintf(parity.mismatchReason, sizeof(parity.mismatchReason), "triangle_identity");
    } else if (!parity.directionMatches) {
        snprintf(parity.mismatchReason, sizeof(parity.mismatchReason), "direction");
    } else if (!parity.branchMatches) {
        snprintf(parity.mismatchReason, sizeof(parity.mismatchReason), "branch");
    } else if (!parity.terminationMatches) {
        snprintf(parity.mismatchReason, sizeof(parity.mismatchReason), "termination");
    } else if (parity.maxFluxError > flux_tolerance) {
        snprintf(parity.mismatchReason, sizeof(parity.mismatchReason), "flux");
    }
    *out_parity = parity;
    return parity.matches;
}
