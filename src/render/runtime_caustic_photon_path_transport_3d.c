#include "render/runtime_caustic_photon_path_transport_3d.h"

#include <math.h>
#include <string.h>

#include "render/runtime_caustic_photon_path_weight_3d.h"
#include "render/runtime_scene_accel_3d.h"

static double photon_path_transport_luma(Vec3 value) {
    return 0.2126 * value.x + 0.7152 * value.y + 0.0722 * value.z;
}

static Vec3 photon_path_transport_multiply(Vec3 a, Vec3 b) {
    return vec3(a.x * b.x, a.y * b.y, a.z * b.z);
}

static bool photon_path_transport_apply_segment_attenuation(
    const RuntimeCausticPhotonMediumStack3D* stack,
    Vec3 path_start,
    Vec3 path_end,
    Vec3 throughput,
    RuntimeCausticPhotonSceneHitEvent3D* hit_event,
    RuntimeCausticPhotonSceneTraceReadback3D* readback,
    Vec3* out_throughput) {
    const RuntimeCausticPhotonMediumEntry3D* medium;
    Vec3 transmittance;
    if (!stack || !hit_event || !readback || !out_throughput) return false;
    medium = RuntimeCausticPhotonMediumStack3D_Top(stack);
    if (!medium) return false;
    hit_event->segmentMedium = *medium;
    hit_event->segmentDistance = vec3_length(vec3_sub(path_end, path_start));
    hit_event->throughputBeforeAttenuation = throughput;
    if (!RuntimeCausticPhotonMediumEntry3D_SegmentTransmittance(
            medium, hit_event->segmentDistance, &transmittance)) {
        return false;
    }
    hit_event->segmentTransmittance = transmittance;
    hit_event->throughputAfterAttenuation =
        photon_path_transport_multiply(throughput, transmittance);
    hit_event->segmentAbsorbedFlux =
        vec3_sub(throughput, hit_event->throughputAfterAttenuation);
    hit_event->segmentAttenuationApplied =
        !medium->isAir && medium->absorptionDistance > 1.0e-12 &&
        hit_event->segmentDistance > 1.0e-12;
    if (hit_event->segmentAttenuationApplied) {
        readback->attenuatedSegmentCount++;
        readback->attenuatedSegmentDistance += hit_event->segmentDistance;
        readback->mediumAbsorbedFlux =
            vec3_add(readback->mediumAbsorbedFlux,
                     hit_event->segmentAbsorbedFlux);
    }
    *out_throughput = hit_event->throughputAfterAttenuation;
    return true;
}

static Vec3 photon_path_transport_geometric_normal(const RuntimeScene3D* scene,
                                                   const HitInfo3D* hit) {
    if (scene && hit && hit->triangleIndex >= 0 &&
        hit->triangleIndex < scene->triangleMesh.triangleCount &&
        scene->triangleMesh.triangles) {
        Vec3 normal = scene->triangleMesh.triangles[hit->triangleIndex].normal;
        if (vec3_length(normal) > 1.0e-12) return vec3_normalize(normal);
    }
    return hit ? vec3_normalize(hit->normal) : vec3(0.0, 0.0, 0.0);
}

static bool photon_path_transport_append_event(
    RuntimeCausticPhotonTrace3D* trace,
    const RuntimeCausticPhotonEvent3D* event) {
    if (!trace || !event ||
        trace->eventCount >= RUNTIME_CAUSTIC_PHOTON_TRACE_MAX_EVENTS) {
        return false;
    }
    trace->events[trace->eventCount++] = *event;
    return true;
}

static RuntimeCausticPhotonSceneHitEvent3D* photon_path_transport_append_hit(
    RuntimeCausticPhotonSceneTrace3D* result,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* material,
    uint32_t depth) {
    RuntimeCausticPhotonSceneHitEvent3D* event;
    if (!result || !hit || !material ||
        result->readback.hitEventCount >=
            RUNTIME_CAUSTIC_PHOTON_TRACE_MAX_DIELECTRIC_EVENTS) {
        return NULL;
    }
    event = &result->hitEvents[result->readback.hitEventCount++];
    memset(event, 0, sizeof(*event));
    event->depth = depth;
    event->hit = *hit;
    event->material = *material;
    return event;
}

static bool photon_path_transport_append_dielectric(
    RuntimeCausticPhotonTrace3D* trace,
    const RuntimeCausticPhotonDielectricEvent3D* event) {
    if (!trace || !event ||
        trace->dielectricEventCount >=
            RUNTIME_CAUSTIC_PHOTON_TRACE_MAX_DIELECTRIC_EVENTS) {
        return false;
    }
    trace->dielectricEvents[trace->dielectricEventCount++] = *event;
    return true;
}

static void photon_path_transport_mark_terminal(
    RuntimeCausticPhotonSceneTrace3D* result,
    RuntimeCausticPhotonSceneTermination3D termination,
    RuntimeCausticPhotonRejectReason3D reject_reason,
    Vec3 rejected_throughput,
    bool succeeded);

static bool photon_path_transport_observe_medium(
    RuntimeCausticPhotonMediumStack3D* stack,
    const RuntimeCausticPhotonMediumEntry3D* boundary,
    bool entering,
    bool total_internal_reflection,
    uint32_t depth,
    RuntimeCausticPhotonMediumFailurePolicy3D failure_policy,
    RuntimeCausticPhotonSceneHitEvent3D* hit_event,
    RuntimeCausticPhotonSceneTraceReadback3D* readback) {
    bool succeeded;
    if (!stack || !boundary || !hit_event || !readback) return false;
    succeeded = RuntimeCausticPhotonMediumStack3D_ObserveBoundary(
        stack,
        boundary,
        entering,
        total_internal_reflection,
        &hit_event->mediumTransition);
    readback->mediumTransitionCount++;
    if (!succeeded) {
        readback->mediumTransitionFailureCount++;
        readback->terminatedByMediumFailurePolicy = true;
        readback->mediumFailureDepth = depth;
        readback->mediumFailurePolicy = failure_policy;
        readback->mediumFailureReason = hit_event->mediumTransition.reason;
    }
    return succeeded;
}

static void photon_path_transport_mark_medium_failure(
    RuntimeCausticPhotonSceneTrace3D* result,
    RuntimeCausticPhotonPathState3D* state,
    RuntimeCausticPhotonSceneHitEvent3D* hit_event) {
    if (!result || !state || !hit_event) return;
    hit_event->termination =
        RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_MEDIUM_TRANSITION_REJECTED;
    result->trace.finalState = *state;
    photon_path_transport_mark_terminal(
        result,
        hit_event->termination,
        RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_MEDIUM,
        state->throughput,
        false);
}

static void photon_path_transport_mark_terminal(
    RuntimeCausticPhotonSceneTrace3D* result,
    RuntimeCausticPhotonSceneTermination3D termination,
    RuntimeCausticPhotonRejectReason3D reject_reason,
    Vec3 rejected_throughput,
    bool succeeded) {
    RuntimeCausticPhotonTrace3D* trace;
    if (!result) return;
    trace = &result->trace;
    trace->finalState.active = false;
    trace->finalState.terminated = true;
    trace->finalState.rejectReason = reject_reason;
    trace->debug.lastRejectReason = reject_reason;
    trace->debug.rejectedFlux = rejected_throughput;
    if (reject_reason != RUNTIME_CAUSTIC_PHOTON_REJECT_NONE) {
        trace->debug.rejectedPhotonCount = 1u;
    }
    result->readback.termination = termination;
    result->readback.succeeded = succeeded;
    trace->valid = succeeded;
}

static bool photon_path_transport_append_terminal_event(
    RuntimeCausticPhotonTrace3D* trace,
    const RuntimeCausticPhotonPathState3D* state,
    uint32_t depth) {
    RuntimeCausticPhotonEvent3D event;
    if (!trace || !state) return false;
    memset(&event, 0, sizeof(event));
    event.photonId = state->photonId;
    event.depth = depth;
    event.kind = RUNTIME_CAUSTIC_PHOTON_EVENT_TERMINATED;
    event.sceneObjectIndex = -1;
    event.primitiveIndex = -1;
    event.triangleIndex = -1;
    event.position = state->position;
    event.incidentDirection = state->direction;
    event.outgoingDirection = state->direction;
    event.throughput = state->throughput;
    event.pathPdf = state->pathPdf;
    return photon_path_transport_append_event(trace, &event);
}

static void photon_path_transport_fill_dielectric_event(
    const RuntimeCausticPhotonSample3D* sample,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* material,
    uint32_t depth,
    Vec3 incident,
    const RuntimeCausticPhotonBsdfSelection3D* selection,
    const RuntimeCausticPhotonBsdfDirection3D* direction,
    RuntimeCausticPhotonDielectricEvent3D* out_event) {
    RuntimeCausticPhotonDielectricEvent3D event;
    memset(&event, 0, sizeof(event));
    if (!sample || !hit || !material || !selection || !direction || !out_event) {
        if (out_event) *out_event = event;
        return;
    }
    event.photonId = sample->photonId;
    event.depth = depth;
    event.sceneObjectIndex = hit->sceneObjectIndex;
    event.primitiveIndex = hit->primitiveIndex;
    event.triangleIndex = hit->triangleIndex;
    event.position = hit->position;
    event.normal = hit->normal;
    event.incidentDirection = incident;
    event.reflectedDirection = direction->dielectric.reflectionDir;
    event.refractedDirection = direction->dielectric.refractionDir;
    event.selectedDirection = direction->outgoingDirection;
    event.throughputBefore = selection->throughputBefore;
    event.throughputAfter = selection->throughputAfter;
    event.etaFrom = direction->dielectric.etaFrom;
    event.etaTo = direction->dielectric.etaTo;
    event.fresnel = direction->dielectric.fresnel;
    event.branchPdf = selection->branchPdf;
    event.selectedBranch = direction->totalInternalReflection
                               ? RUNTIME_CAUSTIC_PHOTON_BRANCH_TOTAL_INTERNAL_REFLECTION
                               : RUNTIME_CAUSTIC_PHOTON_BRANCH_REFRACTED;
    event.totalInternalReflection = direction->totalInternalReflection;
    *out_event = event;
}

void RuntimeCausticPhotonPathTransport3D_DefaultSettings(
    RuntimeCausticPhotonPathTransportSettings3D* settings) {
    if (!settings) return;
    memset(settings, 0, sizeof(*settings));
    RuntimeCausticPhotonSceneTrace3D_DefaultSettings(&settings->sceneTrace);
    settings->depthPolicy = RuntimePathDepthPolicy3D_Resolve();
    settings->applyRoulette = true;
    settings->continueTotalInternalReflection = true;
    settings->mediumFailurePolicy =
        RUNTIME_CAUSTIC_PHOTON_MEDIUM_FAILURE_FAIL_CLOSED;
    RuntimeCausticPhotonMediumStack3D_Init(&settings->initialMediumStack);
}

bool RuntimeCausticPhotonPathTransport3D_Trace(
    const RuntimeScene3D* scene,
    const RuntimeCausticPhotonSample3D* sample,
    const RuntimeCausticPhotonPathTransportSettings3D* settings,
    RuntimeCausticPhotonSceneTrace3D* out_trace) {
    RuntimeCausticPhotonPathTransportSettings3D defaults;
    const RuntimeCausticPhotonPathTransportSettings3D* active = settings;
    RuntimeCausticPhotonSceneTrace3D result;
    RuntimeCausticPhotonTrace3D* trace = &result.trace;
    RuntimeCausticPhotonPathState3D state;
    RuntimeCausticPhotonMediumStack3D medium_stack;
    RuntimeCausticPhotonEvent3D event;
    RuntimeRay3DTraceContext ray_context;
    Vec3 previous_normal = vec3(0.0, 0.0, 0.0);
    bool hard_failure = false;

    if (out_trace) memset(out_trace, 0, sizeof(*out_trace));
    if (!scene || !sample || !out_trace) return false;
    if (!active) {
        RuntimeCausticPhotonPathTransport3D_DefaultSettings(&defaults);
        active = &defaults;
    }
    if (!active->sceneTrace.materialResolver || active->sceneTrace.maxDepth == 0u) {
        return false;
    }

    memset(&result, 0, sizeof(result));
    result.readback.attempted = true;
    if (active->hasInitialMediumStack) {
        medium_stack = active->initialMediumStack;
        if (!RuntimeCausticPhotonMediumStack3D_Top(&medium_stack)) return false;
    } else {
        RuntimeCausticPhotonMediumStack3D_Init(&medium_stack);
    }
    result.initialMediumStack = medium_stack;
    result.finalMediumStack = medium_stack;
    trace->sample = *sample;
    RuntimeCausticPhotonTrace3D_InitPathState(sample, &state);
    trace->initialState = state;
    trace->finalState = state;
    trace->debug.photonId = sample->photonId;
    trace->debug.emittedFlux = sample->flux;
    trace->provenance.originalMediumId =
        RuntimeCausticPhotonMediumStack3D_Top(&medium_stack)->mediumId;
    trace->provenance.segmentStage =
        RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_SOURCE_TO_LENS;
    trace->provenance.emittedProposalDirection = sample->proposalDirection;
    trace->provenance.emittedProposalPdf = sample->proposalPdf;
    trace->provenance.emittedSourceSelectionPdf = sample->sourceSelectionPdf;
    trace->provenance.emittedPositionPdf = sample->positionPdf;
    trace->provenance.emittedDirectionPdf = sample->directionPdf;
    trace->provenance.emittedFluxCorrection = sample->emissionFluxCorrection;
    trace->provenance.emittedFluxPdfCompensated = sample->fluxPdfCompensated;
    trace->provenance.guidedDirection = sample->direction;
    trace->provenance.guidingChangedSample = sample->guidingChangedSample;
    trace->provenance.guidingPdfFluxCorrected =
        sample->guidingPdfFluxCorrected;

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
    if (!photon_path_transport_append_event(trace, &event)) return false;

    RuntimeRay3DTraceContext_Init(&ray_context);
    RuntimeRay3DTraceContext_SetTraceRoute(&ray_context,
                                           active->sceneTrace.traceRoute);
    RuntimeRay3DTraceContext_SetSceneAccelerationTraceFirstHit(
        &ray_context,
        (RuntimeRay3DSceneAccelerationTraceFirstHitFn)
            RuntimeSceneAcceleration3D_TraceFirstHit);

    for (uint32_t depth = 1u; depth <= active->sceneTrace.maxDepth; ++depth) {
        RuntimeCausticPhotonSceneHitEvent3D* hit_event;
        RuntimeCausticPhotonBsdfSampleStream3D stream;
        RuntimeMaterialPayload3D material;
        RuntimeCausticPhotonMediumEntry3D medium_boundary;
        RuntimeDielectricTransport3D dielectric_probe;
        HitInfo3D hit;
        Ray3D ray;
        Vec3 path_start = state.position;
        Vec3 throughput_before = state.throughput;
        double path_pdf_before = state.pathPdf;
        Vec3 geometric_normal;
        bool found;
        bool terminal;
        bool exact_tir = false;
        bool entering = false;
        bool medium_interface_resolved = false;
        double eta_from = 0.0;
        double eta_to = 0.0;
        double incident_cosine;

        ray = depth == 1u
                  ? RuntimeRay3D_Make(state.position, state.direction)
                  : RuntimeRay3D_MakeOffset(state.position,
                                            previous_normal,
                                            state.direction,
                                            active->sceneTrace.rayOffset);
        HitInfo3D_Reset(&hit);
        result.readback.intersectionCount++;
        found = RuntimeRay3D_TraceSceneFirstHitWithContext(
            &ray_context,
            scene,
            &ray,
            active->sceneTrace.tMin,
            active->sceneTrace.tMax,
            &hit);
        if (!found) {
            state.depth = depth - 1u;
            trace->finalState = state;
            photon_path_transport_mark_terminal(
                &result,
                RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_ESCAPED,
                RUNTIME_CAUSTIC_PHOTON_REJECT_ESCAPED_SCENE,
                state.throughput,
                true);
            photon_path_transport_append_terminal_event(trace, &state, depth);
            break;
        }

        RuntimeMaterialPayload3D_Reset(&material);
        if (!active->sceneTrace.materialResolver(
                &hit, &material, active->sceneTrace.materialResolverUserData) ||
            !material.valid) {
            result.readback.materialResolveFailureCount++;
            trace->finalState = state;
            photon_path_transport_mark_terminal(
                &result,
                RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_MATERIAL_UNRESOLVED,
                RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_MEDIUM,
                state.throughput,
                false);
            hard_failure = true;
            break;
        }
        result.readback.materialResolveCount++;
        hit_event = photon_path_transport_append_hit(&result, &hit, &material, depth);
        if (!hit_event ||
            !RuntimeCausticPhotonBsdfSampling3D_Generate(sample, depth, &stream)) {
            trace->finalState = state;
            photon_path_transport_mark_terminal(
                &result,
                RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_TRACE_ERROR,
                RUNTIME_CAUSTIC_PHOTON_REJECT_MAX_DEPTH,
                state.throughput,
                false);
            hard_failure = true;
            break;
        }
        hit_event->bsdfSampleStream = stream;
        hit_event->usedSeededBsdfSamples = true;
        hit_event->pathStart = path_start;
        hit_event->pathPdfBefore = path_pdf_before;
        if (!photon_path_transport_apply_segment_attenuation(
                &medium_stack,
                path_start,
                hit.position,
                state.throughput,
                hit_event,
                &result.readback,
                &state.throughput)) {
            trace->finalState = state;
            photon_path_transport_mark_terminal(
                &result,
                RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_TRACE_ERROR,
                RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_MEDIUM,
                state.throughput,
                false);
            hard_failure = true;
            break;
        }
        state.transportWeight = photon_path_transport_multiply(
            state.transportWeight, hit_event->segmentTransmittance);
        throughput_before = state.throughput;
        geometric_normal = photon_path_transport_geometric_normal(scene, &hit);
        hit.normal = geometric_normal;
        hit_event->hit.normal = geometric_normal;
        incident_cosine = fabs(vec3_dot(vec3_scale(state.direction, -1.0),
                                       geometric_normal));

        memset(&medium_boundary, 0, sizeof(medium_boundary));
        entering = vec3_dot(state.direction, geometric_normal) < 0.0;
        if (material.transparency > 1.0e-12 && !material.thinWalled &&
            RuntimeCausticPhotonMediumEntry3D_FromMaterial(
                &material, hit.sceneObjectIndex, 0.0, &medium_boundary)) {
            medium_interface_resolved =
                RuntimeCausticPhotonMediumStack3D_ResolveInterface(
                    &medium_stack,
                    &medium_boundary,
                    entering,
                    &eta_from,
                    &eta_to);
        }

        memset(&dielectric_probe, 0, sizeof(dielectric_probe));
        if (active->continueTotalInternalReflection &&
            material.transparency > 1.0e-12 &&
            !material.thinWalled && medium_interface_resolved &&
            RuntimeDielectricTransport3D_ResolveInterface(&material,
                                                          geometric_normal,
                                                          state.direction,
                                                          eta_from,
                                                          eta_to,
                                                          &dielectric_probe) &&
            dielectric_probe.totalInternalReflection) {
            exact_tir = true;
        }

        if (!RuntimeCausticPhotonBsdfPolicy3D_Build(
                &material, incident_cosine, state.throughput, &hit_event->bsdfPolicy)) {
            hit_event->termination =
                RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_DIRECTION_INVALID;
            trace->finalState = state;
            photon_path_transport_mark_terminal(
                &result,
                hit_event->termination,
                RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_MEDIUM,
                state.throughput,
                false);
            hard_failure = true;
            break;
        }

        if (exact_tir) {
            memset(&hit_event->bsdfSelection, 0, sizeof(hit_event->bsdfSelection));
            hit_event->bsdfSelection.attempted = true;
            hit_event->bsdfSelection.selected = true;
            hit_event->bsdfSelection.lobe =
                RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_TRANSMISSION;
            hit_event->bsdfSelection.unitSample = stream.bsdfSample.lobeUnitSample;
            hit_event->bsdfSelection.branchPdf = 1.0;
            hit_event->bsdfSelection.throughputBefore = state.throughput;
            hit_event->bsdfSelection.throughputAfter = state.throughput;
            memset(&hit_event->bsdfDirection, 0, sizeof(hit_event->bsdfDirection));
            hit_event->bsdfDirection.attempted = true;
            hit_event->bsdfDirection.valid = true;
            hit_event->bsdfDirection.totalInternalReflection = true;
            hit_event->bsdfDirection.lobe =
                RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_TRANSMISSION;
            hit_event->bsdfDirection.outgoingDirection =
                dielectric_probe.reflectionDir;
            hit_event->bsdfDirection.angularPdf = 1.0;
            hit_event->bsdfDirection.cosine = fabs(vec3_dot(
                dielectric_probe.orientedNormal, dielectric_probe.reflectionDir));
            hit_event->bsdfDirection.dielectric = dielectric_probe;
        } else if (!RuntimeCausticPhotonBsdfPolicy3D_Select(
                       &hit_event->bsdfPolicy,
                       stream.bsdfSample.lobeUnitSample,
                       &hit_event->bsdfSelection)) {
            hit_event->termination =
                RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_DIRECTION_INVALID;
            trace->finalState = state;
            photon_path_transport_mark_terminal(
                &result,
                hit_event->termination,
                RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_MEDIUM,
                state.throughput,
                false);
            hard_failure = true;
            break;
        }

        terminal = hit_event->bsdfSelection.termination !=
                   RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_NONE;
        if (!terminal && !exact_tir &&
            hit_event->bsdfSelection.lobe ==
                RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_TRANSMISSION &&
            !material.thinWalled && !medium_interface_resolved) {
            photon_path_transport_observe_medium(
                &medium_stack,
                &medium_boundary,
                entering,
                false,
                depth,
                active->mediumFailurePolicy,
                hit_event,
                &result.readback);
            photon_path_transport_mark_medium_failure(
                &result, &state, hit_event);
            hard_failure = true;
            break;
        }
        if (!terminal && !exact_tir &&
            !RuntimeCausticPhotonBsdfDirection3D_SampleInterface(
                 hit_event->bsdfSelection.lobe,
                 &material,
                 state.direction,
                 geometric_normal,
                 &stream.bsdfSample.directionSample,
                 hit_event->bsdfSelection.lobe ==
                             RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_TRANSMISSION &&
                         !material.thinWalled
                     ? eta_from
                     : 0.0,
                 hit_event->bsdfSelection.lobe ==
                             RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_TRANSMISSION &&
                         !material.thinWalled
                     ? eta_to
                     : 0.0,
                 &hit_event->bsdfDirection)) {
            hit_event->termination =
                hit_event->bsdfDirection.totalInternalReflection
                    ? RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_TIR_DEFERRED
                    : RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_DIRECTION_INVALID;
            trace->finalState = state;
            photon_path_transport_mark_terminal(
                &result,
                hit_event->termination,
                RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_MEDIUM,
                state.throughput,
                false);
            hard_failure = true;
            break;
        }

        if (!terminal &&
            hit_event->bsdfSelection.lobe ==
                RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_TRANSMISSION &&
            !material.thinWalled &&
            !photon_path_transport_observe_medium(
                &medium_stack,
                &medium_boundary,
                hit_event->bsdfDirection.dielectric.entering,
                hit_event->bsdfDirection.totalInternalReflection,
                depth,
                active->mediumFailurePolicy,
                hit_event,
                &result.readback)) {
            photon_path_transport_mark_medium_failure(
                &result, &state, hit_event);
            hard_failure = true;
            break;
        }

        if (!RuntimeCausticPhotonPathWeight3D_ApplyThroughputRatio(
                state.transportWeight,
                state.throughput,
                hit_event->bsdfSelection.throughputAfter,
                &state.transportWeight)) {
            hard_failure = true;
            break;
        }
        state.depth = depth;
        state.position = hit.position;
        state.throughput = hit_event->bsdfSelection.throughputAfter;
        state.pathPdf *= hit_event->bsdfSelection.branchPdf;
        if (!terminal) {
            state.direction = hit_event->bsdfDirection.outgoingDirection;
            state.pathPdf *= hit_event->bsdfDirection.angularPdf;
        }

        if (hit_event->bsdfSelection.lobe ==
                RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_TRANSMISSION &&
            hit_event->bsdfDirection.valid) {
            photon_path_transport_fill_dielectric_event(
                sample,
                &hit,
                &material,
                depth,
                ray.direction,
                &hit_event->bsdfSelection,
                &hit_event->bsdfDirection,
                &hit_event->dielectric);
            if (!photon_path_transport_append_dielectric(trace,
                                                          &hit_event->dielectric)) {
                hard_failure = true;
                break;
            }
            if (hit_event->bsdfDirection.totalInternalReflection) {
                trace->debug.totalInternalReflectionCount++;
                trace->debug.reflectedBranchCount++;
            } else {
                trace->debug.refractedBranchCount++;
            }
        } else if (hit_event->bsdfSelection.lobe ==
                       RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_SPECULAR ||
                   hit_event->bsdfSelection.lobe ==
                       RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_GLOSSY) {
            trace->debug.reflectedBranchCount++;
        }

        if (!terminal && active->applyRoulette &&
            !RuntimeCausticPhotonBsdfSampling3D_EvaluateRoulette(
                &active->depthPolicy,
                depth,
                state.transportWeight,
                stream.rouletteUnitSample,
                &hit_event->roulette)) {
            hard_failure = true;
            break;
        }
        if (hit_event->roulette.valid) {
            state.pathPdf *= hit_event->roulette.branchPdf;
            if (hit_event->roulette.terminated) {
                state.throughput = vec3(0.0, 0.0, 0.0);
            } else if (hit_event->roulette.evaluated) {
                state.throughput = vec3_scale(
                    state.throughput,
                    1.0 / hit_event->roulette.survivalProbability);
            }
            state.transportWeight = hit_event->roulette.throughputAfter;
        }
        hit_event->pathPdfAfter = state.pathPdf;

        memset(&event, 0, sizeof(event));
        event.photonId = sample->photonId;
        event.depth = depth;
        event.kind = terminal || hit_event->roulette.terminated
                         ? RUNTIME_CAUSTIC_PHOTON_EVENT_TERMINATED
                         : RUNTIME_CAUSTIC_PHOTON_EVENT_SURFACE;
        event.sceneObjectIndex = hit.sceneObjectIndex;
        event.primitiveIndex = hit.primitiveIndex;
        event.triangleIndex = hit.triangleIndex;
        event.position = hit.position;
        event.normal = geometric_normal;
        event.incidentDirection = ray.direction;
        event.outgoingDirection = terminal ? vec3(0.0, 0.0, 0.0)
                                           : state.direction;
        event.throughput = state.throughput;
        event.pathPdf = state.pathPdf;
        if (!photon_path_transport_append_event(trace, &event)) {
            hard_failure = true;
            break;
        }
        trace->finalState = state;
        trace->postExitOrigin = hit.position;
        trace->postExitDirection = event.outgoingDirection;
        previous_normal = geometric_normal;

        if (hit_event->bsdfSelection.termination ==
            RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_EMISSIVE) {
            hit_event->termination =
                RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_EMISSIVE;
            photon_path_transport_mark_terminal(&result,
                                                hit_event->termination,
                                                RUNTIME_CAUSTIC_PHOTON_REJECT_NONE,
                                                throughput_before,
                                                true);
            break;
        }
        if (hit_event->bsdfSelection.termination ==
            RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_ABSORBED) {
            hit_event->termination =
                RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_ABSORBED;
            trace->debug.absorbedBranchCount++;
            photon_path_transport_mark_terminal(
                &result,
                hit_event->termination,
                RUNTIME_CAUSTIC_PHOTON_REJECT_BELOW_FLUX_THRESHOLD,
                throughput_before,
                true);
            break;
        }
        if (hit_event->roulette.terminated) {
            hit_event->termination =
                RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_ROULETTE_TERMINATED;
            photon_path_transport_mark_terminal(
                &result,
                hit_event->termination,
                RUNTIME_CAUSTIC_PHOTON_REJECT_RUSSIAN_ROULETTE,
                hit_event->bsdfSelection.throughputAfter,
                true);
            break;
        }
        if (photon_path_transport_luma(state.transportWeight) <=
            active->sceneTrace.minTransportWeightLuma) {
            hit_event->termination =
                RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_ABSORBED;
            photon_path_transport_mark_terminal(
                &result,
                hit_event->termination,
                RUNTIME_CAUSTIC_PHOTON_REJECT_BELOW_FLUX_THRESHOLD,
                state.throughput,
                true);
            break;
        }
        if (depth == active->sceneTrace.maxDepth) {
            hit_event->termination =
                RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_MAX_DEPTH;
            trace->events[trace->eventCount - 1u].kind =
                RUNTIME_CAUSTIC_PHOTON_EVENT_TERMINATED;
            photon_path_transport_mark_terminal(
                &result,
                hit_event->termination,
                RUNTIME_CAUSTIC_PHOTON_REJECT_MAX_DEPTH,
                state.throughput,
                true);
            break;
        }
        hit_event->termination =
            RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_EVENT_READY;
        state.active = true;
        state.terminated = false;
        state.rejectReason = RUNTIME_CAUSTIC_PHOTON_REJECT_NONE;
        trace->finalState = state;
    }

    if (hard_failure && result.readback.termination ==
                            RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_NONE) {
        trace->finalState = state;
        photon_path_transport_mark_terminal(
            &result,
            RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_TRACE_ERROR,
            RUNTIME_CAUSTIC_PHOTON_REJECT_MAX_DEPTH,
            state.throughput,
            false);
    }
    RuntimeRay3DTraceContext_SnapshotRouteStats(&ray_context,
                                                &result.readback.routeStats);
    result.readback.usedSharedSceneAccelerationRoute =
        result.readback.routeStats.tlasTraceCalls > 0u;
    result.finalMediumStack = medium_stack;
    for (uint32_t i = 0u; i < result.readback.hitEventCount; ++i) {
        const RuntimeCausticPhotonSceneHitEvent3D* hit_event =
            &result.hitEvents[i];
        if (hit_event->mediumTransition.succeeded &&
            hit_event->mediumTransition.stackChanged) {
            if (hit_event->mediumTransition.reason ==
                RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_ENTER_PUSHED) {
                trace->provenance.dielectricEntryCount++;
            } else if (hit_event->mediumTransition.reason ==
                       RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_EXIT_POPPED) {
                trace->provenance.dielectricExitCount++;
            }
        }
        if (hit_event->bsdfSelection.selected &&
            (hit_event->bsdfSelection.lobe ==
                 RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_TRANSMISSION ||
             hit_event->bsdfSelection.lobe ==
                 RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_SPECULAR ||
             hit_event->bsdfSelection.lobe ==
                 RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_GLOSSY)) {
            trace->provenance.priorSpecularOrTransmission = true;
        }
    }
    if (trace->provenance.dielectricExitCount > 0u) {
        trace->provenance.segmentStage =
            RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_LENS;
    } else if (RuntimeCausticPhotonMediumStack3D_Depth(&medium_stack) > 1u) {
        trace->provenance.segmentStage =
            RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_LENS_INTERIOR;
    }
    trace->debug.eventCount = trace->eventCount;
    *out_trace = result;
    return result.readback.succeeded;
}
