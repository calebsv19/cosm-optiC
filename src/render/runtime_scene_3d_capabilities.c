#include "render/runtime_scene_3d.h"

#include "render/runtime_material_payload_3d.h"
#include "render/runtime_volume_3d_integrate.h"

void RuntimeScene3D_RefreshCapabilities(RuntimeScene3D* scene) {
    bool seen[MAX_OBJECTS] = {0};
    RuntimeScene3DCapabilities capabilities = {0};
    RuntimeScene3DMaterialFlags flags = {0};

    if (!scene) return;

    capabilities.valid = true;
    for (int i = 0; i < scene->primitiveCount; ++i) {
        int scene_object_index = scene->primitives[i].source.sceneObjectIndex;
        if (scene_object_index >= 0 && scene_object_index < MAX_OBJECTS) {
            seen[scene_object_index] = true;
        } else {
            capabilities.hasUnresolvedSurfaces = true;
        }
    }
    for (int i = 0; i < scene->triangleMesh.triangleCount; ++i) {
        int scene_object_index = scene->triangleMesh.triangles[i].sceneObjectIndex;
        if (scene_object_index >= 0 && scene_object_index < MAX_OBJECTS) {
            seen[scene_object_index] = true;
        } else {
            capabilities.hasUnresolvedSurfaces = true;
        }
    }

    for (int i = 0; i < MAX_OBJECTS; ++i) {
        RuntimeMaterialPayload3D payload = {0};
        if (!seen[i]) continue;
        if (!RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(i, &payload) ||
            !payload.valid) {
            capabilities.hasUnresolvedSurfaces = true;
            continue;
        }
        if (payload.transparency > 1e-6) {
            capabilities.hasTransparentSurfaces = true;
        }
        if (payload.emissive > 1e-6) {
            capabilities.hasEmissiveSurfaces = true;
        }
        if (payload.bsdf.specWeight > 1e-6 || payload.bsdf.reflectivity > 1e-6) {
            capabilities.hasSpecularSurfaces = true;
        }
        if (payload.transparency > 1e-6) {
            capabilities.hasTransmissionSurfaces = true;
        }
    }

    capabilities.hasLightingExtinctionVolume =
        RuntimeVolume3D_HasActiveExtinction(&scene->volume);
    capabilities.hasLightingScatterVolume = capabilities.hasLightingExtinctionVolume;
    capabilities.canUseOpaqueNoVolumeVisibilityFastPath =
        capabilities.valid &&
        !capabilities.hasTransparentSurfaces &&
        !capabilities.hasUnresolvedSurfaces &&
        !capabilities.hasLightingExtinctionVolume;
    capabilities.canSkipEmissionSupport =
        !capabilities.hasEmissiveSurfaces &&
        !capabilities.hasUnresolvedSurfaces;
    capabilities.canSkipTransparencySupport =
        !capabilities.hasTransparentSurfaces &&
        !capabilities.hasTransmissionSurfaces;
    capabilities.canSkipVolumeScatter =
        !capabilities.hasLightingExtinctionVolume &&
        !capabilities.hasLightingScatterVolume;

    flags.valid = capabilities.valid;
    flags.hasTransparentSurfaces = capabilities.hasTransparentSurfaces;
    flags.hasEmissiveSurfaces = capabilities.hasEmissiveSurfaces;
    flags.hasUnresolvedSurfaces = capabilities.hasUnresolvedSurfaces;
    if (RuntimeEmissiveLightSet3D_BuildForScene(&scene->emissiveLightSet, scene)) {
        capabilities.emissiveLightSetValid = scene->emissiveLightSet.valid;
        capabilities.emissiveLightCandidateCount = scene->emissiveLightSet.candidateCount;
        capabilities.emissiveLightTotalWeight = scene->emissiveLightSet.totalWeight;
    } else {
        capabilities.emissiveLightSetValid = false;
        capabilities.emissiveLightCandidateCount = 0;
        capabilities.emissiveLightTotalWeight = 0.0;
    }
    scene->capabilities = capabilities;
    scene->materialFlags = flags;
}

void RuntimeScene3D_RefreshMaterialFlags(RuntimeScene3D* scene) {
    RuntimeScene3D_RefreshCapabilities(scene);
}
