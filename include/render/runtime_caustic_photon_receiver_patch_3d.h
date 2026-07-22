#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_RECEIVER_PATCH_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_RECEIVER_PATCH_3D_H

#include <stdbool.h>

typedef enum {
    RUNTIME_CAUSTIC_PHOTON_RECEIVER_PATCH = 0,
    RUNTIME_CAUSTIC_PHOTON_RECEIVER_EXACT_TRIANGLE_DIAGNOSTIC = 1
} RuntimeCausticPhotonReceiverDomain3D;

typedef enum {
    RUNTIME_CAUSTIC_PHOTON_RECEIVER_MATCH = 0,
    RUNTIME_CAUSTIC_PHOTON_RECEIVER_OBJECT_MISMATCH,
    RUNTIME_CAUSTIC_PHOTON_RECEIVER_MATERIAL_MISMATCH,
    RUNTIME_CAUSTIC_PHOTON_RECEIVER_EXACT_TRIANGLE_MISMATCH
} RuntimeCausticPhotonReceiverMatchReason3D;

typedef struct {
    int sceneObjectIndex;
    int materialId;
    int primitiveIndex;
    int triangleIndex;
} RuntimeCausticPhotonReceiverIdentity3D;

RuntimeCausticPhotonReceiverMatchReason3D
RuntimeCausticPhotonReceiverPatch3D_Match(
    RuntimeCausticPhotonReceiverIdentity3D record,
    RuntimeCausticPhotonReceiverIdentity3D query,
    RuntimeCausticPhotonReceiverDomain3D domain);
const char* RuntimeCausticPhotonReceiverDomain3D_Label(
    RuntimeCausticPhotonReceiverDomain3D domain);

#endif
