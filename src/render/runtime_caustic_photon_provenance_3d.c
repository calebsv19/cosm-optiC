#include "render/runtime_caustic_photon_provenance_3d.h"

#include <string.h>

const char* RuntimeCausticPhotonSegmentStage3D_Label(
    RuntimeCausticPhotonSegmentStage3D stage) {
    switch (stage) {
        case RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_SOURCE_TO_LENS:
            return "source_to_lens";
        case RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_LENS_INTERIOR:
            return "lens_interior";
        case RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_LENS:
            return "post_lens";
        case RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_RECEIVER:
            return "post_receiver";
        case RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_COUNT:
        default:
            return "unknown";
    }
}

const char* RuntimeCausticPhotonSurfacePathClass3D_Label(
    RuntimeCausticPhotonSurfacePathClass3D path_class) {
    switch (path_class) {
        case RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_DIRECT_TWO_INTERFACE:
            return "direct_two_interface";
        case RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_MULTIPATH:
            return "multipath";
        case RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_UNCLASSIFIED:
        default:
            return "unclassified";
    }
}

const char* RuntimeCausticPhotonSurfacePathFilter3D_Label(
    RuntimeCausticPhotonSurfacePathFilter3D filter) {
    switch (filter) {
        case RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_DIRECT_TWO_INTERFACE:
            return "direct_two_interface";
        case RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_MULTIPATH:
            return "multipath";
        case RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_UNCLASSIFIED:
            return "unclassified";
        case RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_ALL:
        default:
            return "all";
    }
}

RuntimeCausticPhotonSurfacePathFilter3D
RuntimeCausticPhotonSurfacePathFilter3D_FromLabel(const char* label) {
    if (label && (strcmp(label, "direct_two_interface") == 0 ||
                  strcmp(label, "direct") == 0)) {
        return RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_DIRECT_TWO_INTERFACE;
    }
    if (label && strcmp(label, "multipath") == 0) {
        return RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_MULTIPATH;
    }
    if (label && strcmp(label, "unclassified") == 0) {
        return RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_UNCLASSIFIED;
    }
    return RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_ALL;
}

const char* RuntimeCausticPhotonSurfaceRetentionReason3D_Label(
    RuntimeCausticPhotonSurfaceRetentionReason3D reason) {
    switch (reason) {
        case RUNTIME_CAUSTIC_PHOTON_SURFACE_RETAIN_RECONCILED_TRANSMISSION:
            return "reconciled_dielectric_transmission";
        case RUNTIME_CAUSTIC_PHOTON_SURFACE_RETAIN_WITH_SPECULAR_HISTORY:
            return "with_prior_specular_or_transmission";
        case RUNTIME_CAUSTIC_PHOTON_SURFACE_RETAIN_WITHOUT_SPECULAR_HISTORY:
            return "without_prior_specular_or_transmission";
        case RUNTIME_CAUSTIC_PHOTON_SURFACE_RETAIN_REASON_COUNT:
        default:
            return "unknown";
    }
}

const char* RuntimeCausticPhotonRecordRejectReason3D_Label(
    RuntimeCausticPhotonRecordRejectReason3D reason) {
    switch (reason) {
        case RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_TRACE:
            return "invalid_trace";
        case RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_TARGETS:
            return "invalid_targets";
        case RUNTIME_CAUSTIC_PHOTON_REJECT_NO_DIFFUSE_RECEIVER:
            return "no_diffuse_receiver";
        case RUNTIME_CAUSTIC_PHOTON_REJECT_NO_CAUSTIC_HISTORY:
            return "no_prior_specular_or_transmission";
        case RUNTIME_CAUSTIC_PHOTON_REJECT_REFLECTION_ONLY_SURFACE:
            return "reflection_only_surface";
        case RUNTIME_CAUSTIC_PHOTON_REJECT_UNRECONCILED_DIELECTRIC_SURFACE:
            return "unreconciled_dielectric_surface";
        case RUNTIME_CAUSTIC_PHOTON_REJECT_POST_FIRST_DIFFUSE_RECEIVER:
            return "post_first_diffuse_receiver";
        case RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_CANDIDATE:
            return "invalid_candidate";
        case RUNTIME_CAUSTIC_PHOTON_REJECT_CAPACITY:
            return "capacity_rejected";
        case RUNTIME_CAUSTIC_PHOTON_REJECT_STORE:
            return "store_rejected";
        case RUNTIME_CAUSTIC_PHOTON_RECORD_REJECT_REASON_COUNT:
        default:
            return "unknown";
    }
}
