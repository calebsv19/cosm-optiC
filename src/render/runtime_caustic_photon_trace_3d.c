#include "render/runtime_caustic_photon_trace_3d.h"

#include <math.h>
#include <string.h>

#include "render/runtime_caustic_photon_path_weight_3d.h"

static double photon_trace_clamp(double value, double min_value, double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double photon_trace_saturate(double value) {
    return photon_trace_clamp(value, 0.0, 1.0);
}

static double photon_trace_luma(Vec3 value) {
    return 0.2126 * value.x + 0.7152 * value.y + 0.0722 * value.z;
}

static Vec3 photon_trace_vec3_max(Vec3 value, double min_value) {
    return vec3(fmax(value.x, min_value), fmax(value.y, min_value), fmax(value.z, min_value));
}

static Vec3 photon_trace_normalize_or_zero(Vec3 value) {
    if (!(vec3_length(value) > 1.0e-12)) return vec3(0.0, 0.0, 0.0);
    return vec3_normalize(value);
}

static void photon_trace_store_reject(
    const RuntimeCausticPhotonSample3D* sample,
    RuntimeCausticPhotonRejectReason3D reason,
    RuntimeCausticPhotonTrace3D* out_trace) {
    RuntimeCausticPhotonPathState3D state;
    if (!out_trace) return;
    memset(out_trace, 0, sizeof(*out_trace));
    RuntimeCausticPhotonTrace3D_InitPathState(sample, &state);
    state.active = false;
    state.terminated = true;
    state.rejectReason = reason;
    if (sample) {
        out_trace->sample = *sample;
        out_trace->debug.photonId = sample->photonId;
        out_trace->debug.emittedFlux = sample->flux;
        out_trace->debug.rejectedFlux = sample->flux;
    }
    out_trace->initialState = state;
    out_trace->finalState = state;
    out_trace->debug.rejectedPhotonCount = 1u;
    out_trace->debug.lastRejectReason = reason;
}

static bool photon_trace_append_event(RuntimeCausticPhotonTrace3D* trace,
                                      const RuntimeCausticPhotonEvent3D* event) {
    if (!trace || !event ||
        trace->eventCount >= RUNTIME_CAUSTIC_PHOTON_TRACE_MAX_EVENTS) {
        return false;
    }
    trace->events[trace->eventCount++] = *event;
    return true;
}

static bool photon_trace_append_dielectric(
    RuntimeCausticPhotonTrace3D* trace,
    const RuntimeCausticPhotonDielectricEvent3D* event) {
    if (!trace || !event ||
        trace->dielectricEventCount >= RUNTIME_CAUSTIC_PHOTON_TRACE_MAX_DIELECTRIC_EVENTS) {
        return false;
    }
    trace->dielectricEvents[trace->dielectricEventCount++] = *event;
    return true;
}

static RuntimeCausticPhotonBranch3D photon_trace_branch_from_lens_event(
    const RuntimeCausticLensInterfaceEvent3D* event) {
    if (!event) return RUNTIME_CAUSTIC_PHOTON_BRANCH_NONE;
    if (event->totalInternalReflection) {
        return RUNTIME_CAUSTIC_PHOTON_BRANCH_TOTAL_INTERNAL_REFLECTION;
    }
    if (event->refracted) return RUNTIME_CAUSTIC_PHOTON_BRANCH_REFRACTED;
    if (event->reflected) return RUNTIME_CAUSTIC_PHOTON_BRANCH_REFLECTED;
    return RUNTIME_CAUSTIC_PHOTON_BRANCH_ABSORBED;
}

static double photon_trace_branch_pdf(
    const RuntimeCausticLensInterfaceEvent3D* event,
    const RuntimeCausticLensTraversalProfile3D* profile) {
    double scaled_fresnel = 1.0;
    double branch_pdf = 1.0;
    if (!event) return 0.0;
    if (event->totalInternalReflection) return 1.0;
    scaled_fresnel = photon_trace_saturate(event->fresnel *
                                           (profile ? profile->fresnelScale : 1.0));
    if (event->refracted) {
        branch_pdf = 1.0 - scaled_fresnel;
    } else if (event->reflected) {
        branch_pdf = scaled_fresnel;
    } else {
        branch_pdf = 0.0;
    }
    return photon_trace_saturate(branch_pdf);
}

void RuntimeCausticPhotonTrace3D_DefaultSettings(
    RuntimeCausticPhotonTraceSettings3D* settings) {
    if (!settings) return;
    memset(settings, 0, sizeof(*settings));
    settings->maxDepth = RUNTIME_CAUSTIC_PHOTON_TRACE_MAX_DIELECTRIC_EVENTS;
    settings->minTransportWeightLuma = 0.0;
}

void RuntimeCausticPhotonTrace3D_InitPathState(
    const RuntimeCausticPhotonSample3D* sample,
    RuntimeCausticPhotonPathState3D* out_state) {
    if (!out_state) return;
    memset(out_state, 0, sizeof(*out_state));
    out_state->pathPdf = 1.0;
    out_state->rejectReason = RUNTIME_CAUSTIC_PHOTON_REJECT_NONE;
    if (!sample) return;
    out_state->photonId = sample->photonId;
    out_state->position = sample->position;
    out_state->direction = photon_trace_normalize_or_zero(sample->direction);
    out_state->transportWeight = vec3(1.0, 1.0, 1.0);
    out_state->throughput = sample->flux;
    out_state->pathPdf = sample->emissionPdf > 1.0e-12 ? sample->emissionPdf : 1.0;
    out_state->active = true;
}

bool RuntimeCausticPhotonTrace3D_FromLensPath(
    const RuntimeCausticLensPath3D* lens_path,
    const RuntimeCausticPhotonSample3D* sample,
    const RuntimeCausticPhotonTraceSettings3D* settings,
    RuntimeCausticPhotonTrace3D* out_trace) {
    RuntimeCausticPhotonTraceSettings3D default_settings;
    const RuntimeCausticPhotonTraceSettings3D* active_settings = settings;
    RuntimeCausticPhotonTrace3D trace;
    RuntimeCausticPhotonEvent3D event;
    RuntimeCausticPhotonDielectricEvent3D dielectric_event;
    RuntimeCausticPhotonPathState3D current;
    Vec3 current_flux = vec3(0.0, 0.0, 0.0);
    Vec3 current_weight = vec3(1.0, 1.0, 1.0);
    double current_pdf = 1.0;

    if (out_trace) memset(out_trace, 0, sizeof(*out_trace));
    if (!lens_path || !lens_path->valid || !sample || !out_trace) return false;
    if (!active_settings) {
        RuntimeCausticPhotonTrace3D_DefaultSettings(&default_settings);
        active_settings = &default_settings;
    }
    if (lens_path->interfaceEventCount > active_settings->maxDepth ||
        lens_path->interfaceEventCount > RUNTIME_CAUSTIC_PHOTON_TRACE_MAX_DIELECTRIC_EVENTS) {
        RuntimeCausticPhotonTrace3D_InitPathState(sample, &current);
        current.terminated = true;
        current.active = false;
        current.rejectReason = RUNTIME_CAUSTIC_PHOTON_REJECT_MAX_DEPTH;
        out_trace->sample = *sample;
        out_trace->initialState = current;
        out_trace->finalState = current;
        out_trace->debug.photonId = sample->photonId;
        out_trace->debug.emittedFlux = sample->flux;
        out_trace->debug.rejectedFlux = sample->flux;
        out_trace->debug.rejectedPhotonCount = 1u;
        out_trace->debug.lastRejectReason = RUNTIME_CAUSTIC_PHOTON_REJECT_MAX_DEPTH;
        return false;
    }

    memset(&trace, 0, sizeof(trace));
    trace.sample = *sample;
    RuntimeCausticPhotonTrace3D_InitPathState(sample, &trace.initialState);
    current = trace.initialState;
    current_flux = current.throughput;
    current_pdf = current.pathPdf;

    memset(&event, 0, sizeof(event));
    event.photonId = sample->photonId;
    event.kind = RUNTIME_CAUSTIC_PHOTON_EVENT_EMISSION;
    event.sceneObjectIndex = lens_path->sceneObjectIndex;
    event.primitiveIndex = lens_path->primitiveIndex;
    event.triangleIndex = -1;
    event.position = sample->position;
    event.incidentDirection = sample->direction;
    event.outgoingDirection = sample->direction;
    event.throughput = current_flux;
    event.pathPdf = current_pdf;
    if (!photon_trace_append_event(&trace, &event)) return false;

    for (uint32_t i = 0; i < lens_path->interfaceEventCount; ++i) {
        const RuntimeCausticLensInterfaceEvent3D* lens_event = &lens_path->events[i];
        Vec3 throughput_before = current_flux;
        Vec3 throughput_after = current_flux;
        RuntimeCausticPhotonBranch3D branch =
            photon_trace_branch_from_lens_event(lens_event);
        double branch_pdf = photon_trace_branch_pdf(lens_event, &lens_path->traversalProfile);

        if (lens_event->distanceInMedium > 0.0) {
            throughput_before = RuntimeCausticLensTransport3D_ApplyAbsorptionTintProfile(
                throughput_before,
                lens_event->distanceInMedium,
                &lens_path->traversalProfile);
        }
        if (branch == RUNTIME_CAUSTIC_PHOTON_BRANCH_REFRACTED) {
            throughput_after = RuntimeCausticLensTransport3D_ApplyInterfaceTransmissionProfile(
                throughput_before,
                lens_event->fresnel,
                &lens_path->traversalProfile);
            trace.debug.refractedBranchCount++;
        } else if (branch == RUNTIME_CAUSTIC_PHOTON_BRANCH_REFLECTED) {
            throughput_after = vec3_scale(throughput_before,
                                          photon_trace_saturate(lens_event->fresnel));
            trace.debug.reflectedBranchCount++;
        } else if (branch == RUNTIME_CAUSTIC_PHOTON_BRANCH_TOTAL_INTERNAL_REFLECTION) {
            throughput_after = throughput_before;
            trace.debug.totalInternalReflectionCount++;
        } else {
            throughput_after = vec3(0.0, 0.0, 0.0);
            trace.debug.absorbedBranchCount++;
        }
        if (!RuntimeCausticPhotonPathWeight3D_ApplyThroughputRatio(
                current_weight,
                current_flux,
                throughput_after,
                &current_weight)) {
            return false;
        }
        current_pdf *= branch_pdf > 1.0e-12 ? branch_pdf : 1.0;

        memset(&dielectric_event, 0, sizeof(dielectric_event));
        dielectric_event.photonId = sample->photonId;
        dielectric_event.depth = i + 1u;
        dielectric_event.sceneObjectIndex = lens_path->sceneObjectIndex;
        dielectric_event.primitiveIndex = lens_path->primitiveIndex;
        dielectric_event.triangleIndex = -1;
        dielectric_event.position = lens_event->position;
        dielectric_event.normal = lens_event->normal;
        dielectric_event.incidentDirection = lens_event->incidentDirection;
        dielectric_event.selectedDirection = lens_event->outgoingDirection;
        dielectric_event.refractedDirection =
            branch == RUNTIME_CAUSTIC_PHOTON_BRANCH_REFRACTED
                ? lens_event->outgoingDirection
                : vec3(0.0, 0.0, 0.0);
        dielectric_event.reflectedDirection =
            branch == RUNTIME_CAUSTIC_PHOTON_BRANCH_REFLECTED
                ? lens_event->outgoingDirection
                : vec3(0.0, 0.0, 0.0);
        dielectric_event.throughputBefore = throughput_before;
        dielectric_event.throughputAfter = throughput_after;
        dielectric_event.etaFrom = lens_event->etaFrom;
        dielectric_event.etaTo = lens_event->etaTo;
        dielectric_event.fresnel = lens_event->fresnel;
        dielectric_event.branchPdf = branch_pdf;
        dielectric_event.distanceInMedium = lens_event->distanceInMedium;
        dielectric_event.selectedBranch = branch;
        dielectric_event.totalInternalReflection = lens_event->totalInternalReflection;
        if (!photon_trace_append_dielectric(&trace, &dielectric_event)) return false;

        memset(&event, 0, sizeof(event));
        event.photonId = sample->photonId;
        event.depth = dielectric_event.depth;
        event.kind = RUNTIME_CAUSTIC_PHOTON_EVENT_DIELECTRIC;
        event.sceneObjectIndex = lens_path->sceneObjectIndex;
        event.primitiveIndex = lens_path->primitiveIndex;
        event.triangleIndex = -1;
        event.position = lens_event->position;
        event.normal = lens_event->normal;
        event.incidentDirection = lens_event->incidentDirection;
        event.outgoingDirection = lens_event->outgoingDirection;
        event.throughput = throughput_after;
        event.pathPdf = current_pdf;
        if (!photon_trace_append_event(&trace, &event)) return false;

        current_flux = throughput_after;
    }

    current.depth = lens_path->interfaceEventCount;
    current.position = lens_path->postExitOrigin;
    current.direction = photon_trace_normalize_or_zero(lens_path->postExitDirection);
    current.transportWeight = current_weight;
    current.throughput = current_flux;
    current.pathPdf = current_pdf;
    current.active = true;
    current.terminated = photon_trace_luma(current_weight) <=
                         active_settings->minTransportWeightLuma;
    current.rejectReason = current.terminated
                               ? RUNTIME_CAUSTIC_PHOTON_REJECT_BELOW_FLUX_THRESHOLD
                               : RUNTIME_CAUSTIC_PHOTON_REJECT_NONE;
    trace.finalState = current;
    trace.postExitOrigin = lens_path->postExitOrigin;
    trace.postExitDirection = current.direction;
    trace.insideDistance = lens_path->insideDistance;
    trace.receiverPlaneT = lens_path->receiverPlaneT;
    trace.receiverCrossing = lens_path->receiverCrossing;
    trace.debug.photonId = sample->photonId;
    trace.debug.eventCount = trace.eventCount;
    trace.debug.emittedFlux = sample->flux;
    trace.debug.rejectedFlux =
        photon_trace_vec3_max(vec3_sub(sample->flux, current_flux), 0.0);
    if (current.terminated) {
        trace.debug.rejectedPhotonCount = 1u;
        trace.debug.lastRejectReason = current.rejectReason;
    }
    trace.valid = !current.terminated;
    *out_trace = trace;
    return trace.valid;
}

bool RuntimeCausticPhotonTrace3D_TraceMeshDielectricPath(
    const RuntimeCausticLensPath3D* mesh_dielectric_path,
    const RuntimeCausticPhotonSample3D* emitted_sample,
    const RuntimeCausticPhotonTraceSettings3D* settings,
    RuntimeCausticPhotonTrace3D* out_trace) {
    RuntimeCausticPhotonSample3D path_sample;
    Vec3 incident;

    if (out_trace) memset(out_trace, 0, sizeof(*out_trace));
    if (!emitted_sample || !out_trace) return false;
    if (!mesh_dielectric_path || !mesh_dielectric_path->valid ||
        mesh_dielectric_path->shapeKind != RUNTIME_CAUSTIC_LENS_SHAPE_MESH_DIELECTRIC ||
        mesh_dielectric_path->interfaceEventCount == 0u) {
        photon_trace_store_reject(emitted_sample,
                                  RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_MEDIUM,
                                  out_trace);
        return false;
    }

    path_sample = *emitted_sample;
    path_sample.position = mesh_dielectric_path->lightSamplePosition;
    incident = vec3_sub(mesh_dielectric_path->targetPosition,
                        mesh_dielectric_path->lightSamplePosition);
    path_sample.direction = photon_trace_normalize_or_zero(incident);
    if (!(vec3_length(path_sample.direction) > 1.0e-12)) {
        photon_trace_store_reject(emitted_sample,
                                  RUNTIME_CAUSTIC_PHOTON_REJECT_ESCAPED_SCENE,
                                  out_trace);
        return false;
    }

    return RuntimeCausticPhotonTrace3D_FromLensPath(mesh_dielectric_path,
                                                   &path_sample,
                                                   settings,
                                                   out_trace);
}

bool RuntimeCausticPhotonTrace3D_TraceSphereLens(
    const RuntimeCausticSphereLens3DDescriptor* sphere,
    const RuntimeCausticSphereLens3DLight* light,
    const RuntimeCausticSphereLens3DSample* sample,
    const RuntimeCausticPhotonTraceSettings3D* settings,
    uint64_t photon_id,
    uint32_t rng_seed,
    int scene_object_index,
    int primitive_index,
    RuntimeCausticPhotonTrace3D* out_trace) {
    RuntimeCausticLensPath3D lens_path;
    RuntimeCausticPhotonSample3D photon_sample;
    double sample_weight = sample ? fmax(sample->sampleWeight, 0.0) : 1.0;

    if (out_trace) memset(out_trace, 0, sizeof(*out_trace));
    if (!sphere || !light || !out_trace) return false;
    if (!RuntimeCausticLensTransport3D_SolveSpherePath(sphere,
                                                       light,
                                                       sample,
                                                       scene_object_index,
                                                       primitive_index,
                                                       &lens_path)) {
        return false;
    }
    memset(&photon_sample, 0, sizeof(photon_sample));
    photon_sample.photonId = photon_id;
    photon_sample.sampleIndex = photon_id;
    photon_sample.rngSeed = rng_seed;
    photon_sample.lightIndex = -1;
    photon_sample.wavelengthBucket = 0;
    photon_sample.position = lens_path.lightSamplePosition;
    photon_sample.direction =
        photon_trace_normalize_or_zero(vec3_sub(lens_path.targetPosition,
                                                lens_path.lightSamplePosition));
    photon_sample.flux = vec3(light->color.x * light->intensity * sample_weight,
                              light->color.y * light->intensity * sample_weight,
                              light->color.z * light->intensity * sample_weight);
    photon_sample.emissionPdf = lens_path.pathPdf > 1.0e-12 ? lens_path.pathPdf : 1.0;
    return RuntimeCausticPhotonTrace3D_FromLensPath(&lens_path,
                                                   &photon_sample,
                                                   settings,
                                                   out_trace);
}
