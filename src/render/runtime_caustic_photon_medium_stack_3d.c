#include "render/runtime_caustic_photon_medium_stack_3d.h"

#include <math.h>
#include <string.h>

static double photon_medium_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static RuntimeCausticPhotonMediumEntry3D photon_medium_air(void) {
    RuntimeCausticPhotonMediumEntry3D entry;
    memset(&entry, 0, sizeof(entry));
    entry.valid = true;
    entry.isAir = true;
    entry.mediumId = 0;
    entry.sceneObjectIndex = -1;
    entry.materialId = -1;
    entry.ior = 1.0;
    entry.absorptionColor = vec3(1.0, 1.0, 1.0);
    return entry;
}

static bool photon_medium_entry_valid(
    const RuntimeCausticPhotonMediumEntry3D* entry) {
    return entry && entry->valid && isfinite(entry->ior) && entry->ior >= 1.0 &&
           isfinite(entry->absorptionColor.x) &&
           isfinite(entry->absorptionColor.y) &&
           isfinite(entry->absorptionColor.z) &&
           isfinite(entry->absorptionDistance) &&
           entry->absorptionDistance >= 0.0 && isfinite(entry->density) &&
           entry->density >= 0.0;
}

static bool photon_medium_entries_match(
    const RuntimeCausticPhotonMediumEntry3D* active,
    const RuntimeCausticPhotonMediumEntry3D* boundary) {
    if (!active || !boundary || active->isAir || boundary->isAir) return false;
    if (active->sceneObjectIndex != boundary->sceneObjectIndex) return false;
    if (active->materialId >= 0 && boundary->materialId >= 0 &&
        active->materialId != boundary->materialId) {
        return false;
    }
    return active->mediumId == boundary->mediumId;
}

static void photon_medium_transition_begin(
    const RuntimeCausticPhotonMediumStack3D* stack,
    const RuntimeCausticPhotonMediumEntry3D* boundary,
    bool entering,
    bool total_internal_reflection,
    RuntimeCausticPhotonMediumTransition3D* transition) {
    memset(transition, 0, sizeof(*transition));
    transition->attempted = true;
    transition->entering = entering;
    transition->totalInternalReflection = total_internal_reflection;
    if (boundary) transition->boundary = *boundary;
    transition->depthBefore = RuntimeCausticPhotonMediumStack3D_Depth(stack);
    if (RuntimeCausticPhotonMediumStack3D_Top(stack)) {
        transition->topBefore = *RuntimeCausticPhotonMediumStack3D_Top(stack);
    }
}

static bool photon_medium_transition_finish(
    const RuntimeCausticPhotonMediumStack3D* stack,
    RuntimeCausticPhotonMediumTransitionReason3D reason,
    bool succeeded,
    bool changed,
    RuntimeCausticPhotonMediumTransition3D* transition) {
    transition->reason = reason;
    transition->succeeded = succeeded;
    transition->stackChanged = changed;
    transition->depthAfter = RuntimeCausticPhotonMediumStack3D_Depth(stack);
    if (RuntimeCausticPhotonMediumStack3D_Top(stack)) {
        transition->topAfter = *RuntimeCausticPhotonMediumStack3D_Top(stack);
    }
    return succeeded;
}

void RuntimeCausticPhotonMediumStack3D_Init(
    RuntimeCausticPhotonMediumStack3D* stack) {
    if (!stack) return;
    memset(stack, 0, sizeof(*stack));
    stack->entries[0] = photon_medium_air();
    stack->count = 1u;
}

bool RuntimeCausticPhotonMediumEntry3D_FromMaterial(
    const RuntimeMaterialPayload3D* material,
    int scene_object_index,
    double density,
    RuntimeCausticPhotonMediumEntry3D* out_entry) {
    RuntimeCausticPhotonMediumEntry3D entry;
    double ior;
    if (out_entry) memset(out_entry, 0, sizeof(*out_entry));
    if (!material || !material->valid || !out_entry || scene_object_index < 0 ||
        !isfinite(density) || density < 0.0) {
        return false;
    }
    ior = material->opticalIor >= 1.0 ? material->opticalIor : material->bsdf.ior;
    if (!isfinite(ior) || ior < 1.0 || !isfinite(material->baseColorR) ||
        !isfinite(material->baseColorG) || !isfinite(material->baseColorB) ||
        !isfinite(material->absorptionDistance) ||
        material->absorptionDistance < 0.0) {
        return false;
    }
    memset(&entry, 0, sizeof(entry));
    entry.valid = true;
    entry.mediumId = material->materialId >= 0 ? material->materialId
                                                : scene_object_index;
    entry.sceneObjectIndex = scene_object_index;
    entry.materialId = material->materialId;
    entry.ior = ior;
    entry.absorptionColor = vec3(
        photon_medium_clamp01(material->hasGlassAbsorptionColorOverride
                                  ? material->glassAbsorptionColorR
                                  : material->baseColorR),
        photon_medium_clamp01(material->hasGlassAbsorptionColorOverride
                                  ? material->glassAbsorptionColorG
                                  : material->baseColorG),
        photon_medium_clamp01(material->hasGlassAbsorptionColorOverride
                                  ? material->glassAbsorptionColorB
                                  : material->baseColorB));
    entry.absorptionDistance = material->absorptionDistance;
    entry.density = density;
    *out_entry = entry;
    return true;
}

bool RuntimeCausticPhotonMediumEntry3D_SegmentTransmittance(
    const RuntimeCausticPhotonMediumEntry3D* entry,
    double distance,
    Vec3* out_transmittance) {
    const double minimum_transmittance = 1.0e-12;
    double exponent;
    if (out_transmittance) *out_transmittance = vec3(0.0, 0.0, 0.0);
    if (!photon_medium_entry_valid(entry) || !out_transmittance ||
        !isfinite(distance) || distance < 0.0) {
        return false;
    }
    *out_transmittance = vec3(1.0, 1.0, 1.0);
    if (entry->isAir || distance <= 1.0e-12 ||
        entry->absorptionDistance <= 1.0e-12) {
        return true;
    }
    exponent = distance / entry->absorptionDistance;
    *out_transmittance = vec3(
        pow(fmax(entry->absorptionColor.x, minimum_transmittance), exponent),
        pow(fmax(entry->absorptionColor.y, minimum_transmittance), exponent),
        pow(fmax(entry->absorptionColor.z, minimum_transmittance), exponent));
    return true;
}

const RuntimeCausticPhotonMediumEntry3D* RuntimeCausticPhotonMediumStack3D_Top(
    const RuntimeCausticPhotonMediumStack3D* stack) {
    if (!stack || stack->count == 0u ||
        stack->count > RUNTIME_CAUSTIC_PHOTON_MEDIUM_STACK_CAPACITY) {
        return NULL;
    }
    return &stack->entries[stack->count - 1u];
}

uint32_t RuntimeCausticPhotonMediumStack3D_Depth(
    const RuntimeCausticPhotonMediumStack3D* stack) {
    if (!stack || stack->count == 0u) return 0u;
    return stack->count - 1u;
}

bool RuntimeCausticPhotonMediumStack3D_ResolveInterface(
    const RuntimeCausticPhotonMediumStack3D* stack,
    const RuntimeCausticPhotonMediumEntry3D* boundary,
    bool entering,
    double* out_eta_from,
    double* out_eta_to) {
    const RuntimeCausticPhotonMediumEntry3D* top;
    const RuntimeCausticPhotonMediumEntry3D* destination;
    if (out_eta_from) *out_eta_from = 0.0;
    if (out_eta_to) *out_eta_to = 0.0;
    if (!stack || !out_eta_from || !out_eta_to ||
        !photon_medium_entry_valid(boundary) || boundary->isAir) {
        return false;
    }
    top = RuntimeCausticPhotonMediumStack3D_Top(stack);
    if (!photon_medium_entry_valid(top)) return false;
    if (entering) {
        if (photon_medium_entries_match(top, boundary)) return false;
        *out_eta_from = top->ior;
        *out_eta_to = boundary->ior;
        return true;
    }
    if (stack->count <= 1u || !photon_medium_entries_match(top, boundary)) {
        return false;
    }
    destination = &stack->entries[stack->count - 2u];
    if (!photon_medium_entry_valid(destination)) return false;
    *out_eta_from = top->ior;
    *out_eta_to = destination->ior;
    return true;
}

bool RuntimeCausticPhotonMediumStack3D_ObserveBoundary(
    RuntimeCausticPhotonMediumStack3D* stack,
    const RuntimeCausticPhotonMediumEntry3D* boundary,
    bool entering,
    bool total_internal_reflection,
    RuntimeCausticPhotonMediumTransition3D* out_transition) {
    RuntimeCausticPhotonMediumTransition3D transition;
    const RuntimeCausticPhotonMediumEntry3D* top;
    if (out_transition) memset(out_transition, 0, sizeof(*out_transition));
    if (!stack || !out_transition || stack->count == 0u ||
        stack->count > RUNTIME_CAUSTIC_PHOTON_MEDIUM_STACK_CAPACITY) {
        return false;
    }
    photon_medium_transition_begin(stack,
                                   boundary,
                                   entering,
                                   total_internal_reflection,
                                   &transition);
    if (!photon_medium_entry_valid(boundary) || boundary->isAir) {
        stack->invalidEntryCount++;
        photon_medium_transition_finish(stack,
                                        RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_INVALID_ENTRY,
                                        false,
                                        false,
                                        &transition);
        *out_transition = transition;
        return false;
    }
    if (total_internal_reflection) {
        stack->tirNoChangeCount++;
        photon_medium_transition_finish(
            stack,
            RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_TIR_NO_CHANGE,
            true,
            false,
            &transition);
        *out_transition = transition;
        return true;
    }
    top = RuntimeCausticPhotonMediumStack3D_Top(stack);
    if (entering) {
        if (photon_medium_entries_match(top, boundary)) {
            stack->mismatchCount++;
            photon_medium_transition_finish(
                stack,
                RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_MISMATCH,
                false,
                false,
                &transition);
            *out_transition = transition;
            return false;
        }
        if (stack->count >= RUNTIME_CAUSTIC_PHOTON_MEDIUM_STACK_CAPACITY) {
            stack->overflowCount++;
            photon_medium_transition_finish(
                stack,
                RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_OVERFLOW,
                false,
                false,
                &transition);
            *out_transition = transition;
            return false;
        }
        stack->entries[stack->count++] = *boundary;
        stack->pushCount++;
        if (RuntimeCausticPhotonMediumStack3D_Depth(stack) > stack->maxDepth) {
            stack->maxDepth = RuntimeCausticPhotonMediumStack3D_Depth(stack);
        }
        photon_medium_transition_finish(
            stack,
            RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_ENTER_PUSHED,
            true,
            true,
            &transition);
        *out_transition = transition;
        return true;
    }
    if (stack->count <= 1u) {
        stack->underflowCount++;
        photon_medium_transition_finish(
            stack,
            RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_UNDERFLOW,
            false,
            false,
            &transition);
        *out_transition = transition;
        return false;
    }
    if (!photon_medium_entries_match(top, boundary)) {
        stack->mismatchCount++;
        photon_medium_transition_finish(stack,
                                        RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_MISMATCH,
                                        false,
                                        false,
                                        &transition);
        *out_transition = transition;
        return false;
    }
    memset(&stack->entries[stack->count - 1u], 0, sizeof(stack->entries[0]));
    stack->count--;
    stack->popCount++;
    photon_medium_transition_finish(
        stack,
        RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_EXIT_POPPED,
        true,
        true,
        &transition);
    *out_transition = transition;
    return true;
}

const char* RuntimeCausticPhotonMediumTransitionReason3D_Label(
    RuntimeCausticPhotonMediumTransitionReason3D reason) {
    switch (reason) {
        case RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_ENTER_PUSHED:
            return "enter_pushed";
        case RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_EXIT_POPPED:
            return "exit_popped";
        case RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_TIR_NO_CHANGE:
            return "tir_no_change";
        case RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_OVERFLOW:
            return "overflow";
        case RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_UNDERFLOW:
            return "underflow";
        case RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_MISMATCH:
            return "mismatch";
        case RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_INVALID_ENTRY:
            return "invalid_entry";
        case RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_NONE:
        default:
            return "none";
    }
}

const char* RuntimeCausticPhotonMediumFailurePolicy3D_Label(
    RuntimeCausticPhotonMediumFailurePolicy3D policy) {
    switch (policy) {
        case RUNTIME_CAUSTIC_PHOTON_MEDIUM_FAILURE_FAIL_CLOSED:
        default:
            return "fail_closed";
    }
}
