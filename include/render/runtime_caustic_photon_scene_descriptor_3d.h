#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_SCENE_DESCRIPTOR_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_SCENE_DESCRIPTOR_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_caustic_lens_transport_3d.h"
#include "render/runtime_scene_3d.h"

typedef struct {
    bool valid;
    int sceneObjectIndex;
    int primitiveIndex;
    int triangleIndex;
    int triangleCount;
    RuntimeTriangle3D entryTriangle;
    RuntimeCausticLensShape3D shape;
    double areaScore;
} RuntimeCausticPhotonMeshDielectricDescriptor3D;

typedef struct {
    bool attempted;
    bool succeeded;
    uint64_t meshDielectricCandidateCount;
    int selectedDescriptorIndex;
    int descriptorCount;
    RuntimeCausticPhotonMeshDielectricDescriptor3D descriptors[MAX_OBJECTS];
} RuntimeCausticPhotonSceneDescriptorBatch3D;

void RuntimeCausticPhotonSceneDescriptor3D_InitBatch(
    RuntimeCausticPhotonSceneDescriptorBatch3D* batch);
bool RuntimeCausticPhotonSceneDescriptor3D_HarvestMeshDielectricBatch(
    const RuntimeScene3D* scene,
    RuntimeCausticPhotonSceneDescriptorBatch3D* out_batch);
const RuntimeCausticPhotonMeshDielectricDescriptor3D*
RuntimeCausticPhotonSceneDescriptor3D_SelectedMeshDielectric(
    const RuntimeCausticPhotonSceneDescriptorBatch3D* batch);

#endif
