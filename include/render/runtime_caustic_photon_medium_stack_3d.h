#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_MEDIUM_STACK_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_MEDIUM_STACK_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_material_payload_3d.h"

enum {
    RUNTIME_CAUSTIC_PHOTON_MEDIUM_STACK_CAPACITY = 8
};

typedef enum {
    RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_NONE = 0,
    RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_ENTER_PUSHED,
    RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_EXIT_POPPED,
    RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_TIR_NO_CHANGE,
    RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_OVERFLOW,
    RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_UNDERFLOW,
    RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_MISMATCH,
    RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_INVALID_ENTRY
} RuntimeCausticPhotonMediumTransitionReason3D;

typedef struct {
    bool valid;
    bool isAir;
    int mediumId;
    int sceneObjectIndex;
    int materialId;
    double ior;
    Vec3 absorptionColor;
    double absorptionDistance;
    double density;
} RuntimeCausticPhotonMediumEntry3D;

typedef struct {
    bool attempted;
    bool succeeded;
    bool entering;
    bool totalInternalReflection;
    bool stackChanged;
    uint32_t depthBefore;
    uint32_t depthAfter;
    RuntimeCausticPhotonMediumTransitionReason3D reason;
    RuntimeCausticPhotonMediumEntry3D boundary;
    RuntimeCausticPhotonMediumEntry3D topBefore;
    RuntimeCausticPhotonMediumEntry3D topAfter;
} RuntimeCausticPhotonMediumTransition3D;

typedef struct {
    RuntimeCausticPhotonMediumEntry3D
        entries[RUNTIME_CAUSTIC_PHOTON_MEDIUM_STACK_CAPACITY];
    uint32_t count;
    uint32_t maxDepth;
    uint32_t pushCount;
    uint32_t popCount;
    uint32_t tirNoChangeCount;
    uint32_t overflowCount;
    uint32_t underflowCount;
    uint32_t mismatchCount;
    uint32_t invalidEntryCount;
} RuntimeCausticPhotonMediumStack3D;

void RuntimeCausticPhotonMediumStack3D_Init(
    RuntimeCausticPhotonMediumStack3D* stack);
bool RuntimeCausticPhotonMediumEntry3D_FromMaterial(
    const RuntimeMaterialPayload3D* material,
    int scene_object_index,
    double density,
    RuntimeCausticPhotonMediumEntry3D* out_entry);
bool RuntimeCausticPhotonMediumEntry3D_SegmentTransmittance(
    const RuntimeCausticPhotonMediumEntry3D* entry,
    double distance,
    Vec3* out_transmittance);
const RuntimeCausticPhotonMediumEntry3D* RuntimeCausticPhotonMediumStack3D_Top(
    const RuntimeCausticPhotonMediumStack3D* stack);
uint32_t RuntimeCausticPhotonMediumStack3D_Depth(
    const RuntimeCausticPhotonMediumStack3D* stack);
bool RuntimeCausticPhotonMediumStack3D_ResolveInterface(
    const RuntimeCausticPhotonMediumStack3D* stack,
    const RuntimeCausticPhotonMediumEntry3D* boundary,
    bool entering,
    double* out_eta_from,
    double* out_eta_to);
bool RuntimeCausticPhotonMediumStack3D_ObserveBoundary(
    RuntimeCausticPhotonMediumStack3D* stack,
    const RuntimeCausticPhotonMediumEntry3D* boundary,
    bool entering,
    bool total_internal_reflection,
    RuntimeCausticPhotonMediumTransition3D* out_transition);
const char* RuntimeCausticPhotonMediumTransitionReason3D_Label(
    RuntimeCausticPhotonMediumTransitionReason3D reason);

#endif
