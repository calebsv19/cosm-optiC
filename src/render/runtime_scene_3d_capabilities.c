#include "render/runtime_scene_3d.h"

#include <string.h>

#include "render/runtime_material_payload_3d.h"
#include "render/runtime_volume_3d_integrate.h"

void RuntimeScene3D_RefreshCapabilities(RuntimeScene3D* scene) {
    bool seen[MAX_OBJECTS] = {0};
    RuntimeScene3DCapabilities capabilities = {0};
    RuntimeScene3DMaterialFlags flags = {0};

    if (!scene) return;

    memset(scene->objectMaterialSummaries, 0, sizeof(scene->objectMaterialSummaries));
    scene->objectMaterialSummariesValid = false;
    capabilities.valid = true;
    for (int i = 0; i < scene->primitiveCount; ++i) {
        int scene_object_index = scene->primitives[i].source.sceneObjectIndex;
        if (scene_object_index >= 0 && scene_object_index < MAX_OBJECTS) {
            seen[scene_object_index] = true;
            scene->objectMaterialSummaries[scene_object_index].seen = true;
        } else {
            capabilities.hasUnresolvedSurfaces = true;
        }
    }
    for (int i = 0; i < scene->triangleMesh.triangleCount; ++i) {
        int scene_object_index = scene->triangleMesh.triangles[i].sceneObjectIndex;
        if (scene_object_index >= 0 && scene_object_index < MAX_OBJECTS) {
            seen[scene_object_index] = true;
            scene->objectMaterialSummaries[scene_object_index].seen = true;
        } else {
            capabilities.hasUnresolvedSurfaces = true;
        }
    }

    for (int i = 0; i < MAX_OBJECTS; ++i) {
        RuntimeMaterialPayload3D payload = {0};
        if (!seen[i]) continue;
        if (!RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(i, &payload) ||
            !payload.valid) {
            scene->objectMaterialSummaries[i].resolved = false;
            scene->objectMaterialSummaries[i].valid = false;
            capabilities.hasUnresolvedSurfaces = true;
            continue;
        }
        scene->objectMaterialSummaries[i].resolved = true;
        scene->objectMaterialSummaries[i].valid = payload.valid;
        scene->objectMaterialSummaries[i].transparency = payload.transparency;
        scene->objectMaterialSummaries[i].emissive = payload.emissive;
        scene->objectMaterialSummaries[i].specWeight = payload.bsdf.specWeight;
        scene->objectMaterialSummaries[i].reflectivity = payload.bsdf.reflectivity;
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
    RuntimeLightSet3D_RemoveOrigin(&scene->lightSet,
                                   RUNTIME_LIGHT_SOURCE_3D_ORIGIN_MATERIAL_EMITTER);
    if (RuntimeEmissiveLightSet3D_BuildForScene(&scene->emissiveLightSet, scene)) {
        capabilities.emissiveLightSetValid = scene->emissiveLightSet.valid;
        capabilities.emissiveLightCandidateCount = scene->emissiveLightSet.candidateCount;
        capabilities.emissiveLightTotalWeight = scene->emissiveLightSet.totalWeight;
        if (!RuntimeEmissiveLightSet3D_AppendRegistryEntries(
                &scene->emissiveLightSet,
                &scene->lightSet,
                RUNTIME_EMISSIVE_LIGHT_REGISTRY_MODE_ENABLE_SIMPLE_PROXIES)) {
            capabilities.emissiveLightSetValid = false;
        }
    } else {
        capabilities.emissiveLightSetValid = false;
        capabilities.emissiveLightCandidateCount = 0;
        capabilities.emissiveLightTotalWeight = 0.0;
    }
    scene->capabilities = capabilities;
    scene->materialFlags = flags;
    scene->objectMaterialSummariesValid = true;
}

void RuntimeScene3D_RefreshMaterialFlags(RuntimeScene3D* scene) {
    RuntimeScene3D_RefreshCapabilities(scene);
}
