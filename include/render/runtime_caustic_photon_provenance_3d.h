#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_PROVENANCE_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_PROVENANCE_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "math/vec3.h"

typedef enum {
    RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_SOURCE_TO_LENS = 0,
    RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_LENS_INTERIOR,
    RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_LENS,
    RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_RECEIVER,
    RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_COUNT
} RuntimeCausticPhotonSegmentStage3D;

typedef enum {
    RUNTIME_CAUSTIC_PHOTON_SURFACE_RETAIN_RECONCILED_TRANSMISSION = 0,
    RUNTIME_CAUSTIC_PHOTON_SURFACE_RETAIN_WITH_SPECULAR_HISTORY,
    RUNTIME_CAUSTIC_PHOTON_SURFACE_RETAIN_WITHOUT_SPECULAR_HISTORY,
    RUNTIME_CAUSTIC_PHOTON_SURFACE_RETAIN_REASON_COUNT
} RuntimeCausticPhotonSurfaceRetentionReason3D;

typedef enum {
    RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_UNCLASSIFIED = 0,
    RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_DIRECT_TWO_INTERFACE,
    RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_MULTIPATH
} RuntimeCausticPhotonSurfacePathClass3D;

typedef enum {
    RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_ALL = 0,
    RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_DIRECT_TWO_INTERFACE,
    RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_MULTIPATH,
    RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_FILTER_UNCLASSIFIED
} RuntimeCausticPhotonSurfacePathFilter3D;

typedef enum {
    RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_TRACE = 0,
    RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_TARGETS,
    RUNTIME_CAUSTIC_PHOTON_REJECT_NO_DIFFUSE_RECEIVER,
    RUNTIME_CAUSTIC_PHOTON_REJECT_NO_CAUSTIC_HISTORY,
    RUNTIME_CAUSTIC_PHOTON_REJECT_REFLECTION_ONLY_SURFACE,
    RUNTIME_CAUSTIC_PHOTON_REJECT_UNRECONCILED_DIELECTRIC_SURFACE,
    RUNTIME_CAUSTIC_PHOTON_REJECT_POST_FIRST_DIFFUSE_RECEIVER,
    RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_CANDIDATE,
    RUNTIME_CAUSTIC_PHOTON_REJECT_CAPACITY,
    RUNTIME_CAUSTIC_PHOTON_REJECT_STORE,
    RUNTIME_CAUSTIC_PHOTON_RECORD_REJECT_REASON_COUNT
} RuntimeCausticPhotonRecordRejectReason3D;

typedef struct {
    bool priorSpecularOrTransmission;
    uint32_t dielectricEntryCount;
    uint32_t dielectricExitCount;
    int firstDielectricSceneObjectIndex;
    int originalMediumId;
    RuntimeCausticPhotonSegmentStage3D segmentStage;
    RuntimeCausticPhotonSurfacePathClass3D surfacePathClass;
    Vec3 emittedProposalDirection;
    double emittedProposalPdf;
    double emittedSourceSelectionPdf;
    double emittedPositionPdf;
    double emittedDirectionPdf;
    double emittedFluxCorrection;
    bool emittedFluxPdfCompensated;
    Vec3 guidedDirection;
    bool guidingChangedSample;
    bool guidingPdfFluxCorrected;
} RuntimeCausticPhotonPathProvenance3D;

typedef struct {
    uint64_t surfaceRetained[
        RUNTIME_CAUSTIC_PHOTON_SURFACE_RETAIN_REASON_COUNT];
    uint64_t surfaceRejected[RUNTIME_CAUSTIC_PHOTON_RECORD_REJECT_REASON_COUNT];
    uint64_t beamRetained[RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_COUNT];
    uint64_t beamRejected[RUNTIME_CAUSTIC_PHOTON_RECORD_REJECT_REASON_COUNT];
} RuntimeCausticPhotonRetentionReadback3D;

const char* RuntimeCausticPhotonSegmentStage3D_Label(
    RuntimeCausticPhotonSegmentStage3D stage);
const char* RuntimeCausticPhotonSurfaceRetentionReason3D_Label(
    RuntimeCausticPhotonSurfaceRetentionReason3D reason);
const char* RuntimeCausticPhotonSurfacePathClass3D_Label(
    RuntimeCausticPhotonSurfacePathClass3D path_class);
const char* RuntimeCausticPhotonSurfacePathFilter3D_Label(
    RuntimeCausticPhotonSurfacePathFilter3D filter);
RuntimeCausticPhotonSurfacePathFilter3D
RuntimeCausticPhotonSurfacePathFilter3D_FromLabel(const char* label);
const char* RuntimeCausticPhotonRecordRejectReason3D_Label(
    RuntimeCausticPhotonRecordRejectReason3D reason);

#endif
