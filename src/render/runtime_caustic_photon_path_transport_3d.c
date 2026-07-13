#include "render/runtime_caustic_photon_path_transport_3d.h"

#include <math.h>
#include <string.h>

#include "render/runtime_scene_accel_3d.h"

static double photon_path_transport_luma(Vec3 value) {
    return 0.2126 * value.x + 0.7152 * value.y + 0.0722 * value.z;
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

static void photon_path_transport_observe_medium(
    RuntimeCausticPhotonMediumStack3D* stack,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* material,
    const RuntimeCausticPhotonBsdfDirection3D* direction,
    RuntimeCausticPhotonSceneHitEvent3D* hit_event,
    RuntimeCausticPhotonSceneTraceReadback3D* readback) {
    RuntimeCausticPhotonMediumEntry3D boundary;
    bool succeeded;
    if (!stack || !hit || !material || !direction || !hit_event || !readback ||
        material->thinWalled) {
        return;
    }
    memset(&boundary, 0, sizeof(boundary));
    RuntimeCausticPhotonMediumEntry3D_FromMaterial(
        material, hit->sceneObjectIndex, 0.0, &boundary);
    succeeded = RuntimeCausticPhotonMediumStack3D_ObserveBoundary(
        stack,
        &boundary,
        direction->dielectric.entering,
        direction->totalInternalReflection,
        &hit_event->mediumTransition);
    readback->mediumTransitionCount++;
    if (!succeeded) readback->mediumTransitionFailureCount++;
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
    double ior;
    memset(&event, 0, sizeof(event));
    if (!sample || !hit || !material || !selection || !direction || !out_event) {
        if (out_event) *out_event = event;
        return;
    }
    ior = material->opticalIor > 1.0 ? material->opticalIor : material->bsdf.ior;
    if (!(ior > 1.0)) ior = 1.0;
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
    event.etaFrom = direction->dielectric.entering ? 1.0 : ior;
    event.etaTo = direction->dielectric.entering ? ior : 1.0;
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
    RuntimeCausticPhotonMediumStack3D_Init(&medium_stack);
    result.initialMediumStack = medium_stack;
    result.finalMediumStack = medium_stack;
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
        geometric_normal = photon_path_transport_geometric_normal(scene, &hit);
        hit.normal = geometric_normal;
        hit_event->hit.normal = geometric_normal;
        incident_cosine = fabs(vec3_dot(vec3_scale(state.direction, -1.0),
                                       geometric_normal));

        memset(&dielectric_probe, 0, sizeof(dielectric_probe));
        if (active->continueTotalInternalReflection &&
            material.transparency > 1.0e-12 &&
            RuntimeDielectricTransport3D_Resolve(
                &material, geometric_normal, state.direction, &dielectric_probe) &&
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
            !RuntimeCausticPhotonBsdfDirection3D_Sample(
                hit_event->bsdfSelection.lobe,
                &material,
                state.direction,
                geometric_normal,
                &stream.bsdfSample.directionSample,
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
            photon_path_transport_observe_medium(&medium_stack,
                                                 &hit,
                                                 &material,
                                                 &hit_event->bsdfDirection,
                                                 hit_event,
                                                 &result.readback);
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
                state.throughput,
                stream.rouletteUnitSample,
                &hit_event->roulette)) {
            hard_failure = true;
            break;
        }
        if (hit_event->roulette.valid) {
            state.pathPdf *= hit_event->roulette.branchPdf;
            state.throughput = hit_event->roulette.throughputAfter;
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
                hit_event->roulette.terminatedThroughput,
                true);
            break;
        }
        if (photon_path_transport_luma(state.throughput) <=
            active->sceneTrace.minFluxLuma) {
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
    trace->debug.eventCount = trace->eventCount;
    *out_trace = result;
    return result.readback.succeeded;
}
