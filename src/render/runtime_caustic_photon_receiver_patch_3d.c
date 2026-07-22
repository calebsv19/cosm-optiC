#include "render/runtime_caustic_photon_receiver_patch_3d.h"

static bool receiver_known_mismatch(int lhs, int rhs) {
    return lhs >= 0 && rhs >= 0 && lhs != rhs;
}

RuntimeCausticPhotonReceiverMatchReason3D
RuntimeCausticPhotonReceiverPatch3D_Match(
    RuntimeCausticPhotonReceiverIdentity3D record,
    RuntimeCausticPhotonReceiverIdentity3D query,
    RuntimeCausticPhotonReceiverDomain3D domain) {
    if (receiver_known_mismatch(record.sceneObjectIndex, query.sceneObjectIndex)) {
        return RUNTIME_CAUSTIC_PHOTON_RECEIVER_OBJECT_MISMATCH;
    }
    if (receiver_known_mismatch(record.materialId, query.materialId)) {
        return RUNTIME_CAUSTIC_PHOTON_RECEIVER_MATERIAL_MISMATCH;
    }
    if (domain == RUNTIME_CAUSTIC_PHOTON_RECEIVER_EXACT_TRIANGLE_DIAGNOSTIC &&
        (receiver_known_mismatch(record.primitiveIndex, query.primitiveIndex) ||
         receiver_known_mismatch(record.triangleIndex, query.triangleIndex))) {
        return RUNTIME_CAUSTIC_PHOTON_RECEIVER_EXACT_TRIANGLE_MISMATCH;
    }
    return RUNTIME_CAUSTIC_PHOTON_RECEIVER_MATCH;
}

const char* RuntimeCausticPhotonReceiverDomain3D_Label(
    RuntimeCausticPhotonReceiverDomain3D domain) {
    return domain == RUNTIME_CAUSTIC_PHOTON_RECEIVER_EXACT_TRIANGLE_DIAGNOSTIC
               ? "exact_triangle_diagnostic"
               : "receiver_patch";
}
