#include "render/runtime_caustic_photon_lifecycle_3d.h"

#include <string.h>

static uint64_t lifecycle_hash_bytes(uint64_t hash, const void* data, size_t size) {
    const unsigned char* bytes = data;
    size_t i;
    for (i = 0u; i < size; ++i) {
        hash ^= (uint64_t)bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t lifecycle_hash_vec3(uint64_t hash, Vec3 value) {
    hash = lifecycle_hash_bytes(hash, &value.x, sizeof(value.x));
    hash = lifecycle_hash_bytes(hash, &value.y, sizeof(value.y));
    return lifecycle_hash_bytes(hash, &value.z, sizeof(value.z));
}

void RuntimeCausticPhotonMapLifecycle3D_Init(RuntimeCausticPhotonMapLifecycleState3D* state) {
    if (state) memset(state, 0, sizeof(*state));
}

void RuntimeCausticPhotonMapLifecycle3D_BuildInputFromScene(
    const RuntimeScene3D* scene,
    int sample_budget,
    uint32_t emission_seed,
    int max_path_depth,
    double surface_query_radius,
    double volume_query_radius,
    bool volume_query_enabled,
    bool persistent_map_ownership_enabled,
    RuntimeCausticPhotonMapLifecycleInput3D* out_input) {
    const uint64_t seed = UINT64_C(1469598103934665603);
    uint64_t geometry = seed;
    uint64_t light = seed;
    uint64_t material = seed;
    uint64_t volume = seed;
    int i;

    if (!out_input) return;
    memset(out_input, 0, sizeof(*out_input));
    if (!scene) return;
    for (i = 0; i < scene->triangleMesh.triangleCount; ++i) {
        const RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[i];
        geometry = lifecycle_hash_vec3(geometry, triangle->p0);
        geometry = lifecycle_hash_vec3(geometry, triangle->p1);
        geometry = lifecycle_hash_vec3(geometry, triangle->p2);
        geometry = lifecycle_hash_bytes(geometry, &triangle->sceneObjectIndex,
                                        sizeof(triangle->sceneObjectIndex));
    }
    for (i = 0; i < scene->lightSet.lightCount; ++i) {
        const RuntimeLightSource3D* source = &scene->lightSet.lights[i];
        light = lifecycle_hash_vec3(light, source->position);
        light = lifecycle_hash_vec3(light, source->color);
        light = lifecycle_hash_bytes(light, &source->intensity, sizeof(source->intensity));
        light = lifecycle_hash_bytes(light, &source->radius, sizeof(source->radius));
        light = lifecycle_hash_bytes(light, &source->enabled, sizeof(source->enabled));
    }
    material = lifecycle_hash_bytes(material, &scene->materialFlags,
                                    sizeof(scene->materialFlags));
    for (i = 0; i < MAX_OBJECTS; ++i) {
        material = lifecycle_hash_bytes(material, &scene->objectMaterialSummaries[i],
                                        sizeof(scene->objectMaterialSummaries[i]));
    }
    volume = lifecycle_hash_bytes(volume, &scene->volume.grid, sizeof(scene->volume.grid));
    if (scene->volume.channels.density && scene->volume.grid.cellCount > 0u) {
        volume = lifecycle_hash_bytes(volume, scene->volume.channels.density,
                                      (size_t)scene->volume.grid.cellCount *
                                          sizeof(*scene->volume.channels.density));
    }
    out_input->geometryKey = geometry;
    out_input->lightKey = light;
    out_input->materialKey = material;
    out_input->volumeKey = volume;
    out_input->budgetKey = ((uint64_t)(unsigned int)sample_budget << 32u) |
                         (uint32_t)max_path_depth;
    out_input->budgetKey = lifecycle_hash_bytes(out_input->budgetKey,
                                                &emission_seed,
                                                sizeof(emission_seed));
    out_input->budgetKey = lifecycle_hash_bytes(out_input->budgetKey,
                                                &surface_query_radius,
                                                sizeof(surface_query_radius));
    out_input->budgetKey = lifecycle_hash_bytes(out_input->budgetKey,
                                                &volume_query_radius,
                                                sizeof(volume_query_radius));
    out_input->budgetKey = lifecycle_hash_bytes(out_input->budgetKey,
                                                &volume_query_enabled,
                                                sizeof(volume_query_enabled));
    out_input->persistentMapOwnershipEnabled = persistent_map_ownership_enabled;
}

const char* RuntimeCausticPhotonMapRebuildReason3D_Label(
    RuntimeCausticPhotonMapRebuildReason3D reason) {
    switch (reason) {
        case RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_FIRST_BUILD: return "first_build";
        case RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_PER_FRAME_POLICY: return "per_frame_policy";
        case RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_GEOMETRY_CHANGED: return "geometry_changed";
        case RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_LIGHT_CHANGED: return "light_changed";
        case RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_MATERIAL_CHANGED: return "material_changed";
        case RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_VOLUME_CHANGED: return "volume_changed";
        case RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_BUDGET_CHANGED: return "budget_changed";
        case RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_EXPLICIT_REQUEST: return "explicit_request";
        case RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_NONE:
        default: return "none";
    }
}

RuntimeCausticPhotonBudgetTier3D RuntimeCausticPhotonBudgetTier3D_FromBudget(
    int sample_budget,
    int max_path_depth) {
    if (sample_budget <= 64 && max_path_depth <= 4) {
        return RUNTIME_CAUSTIC_PHOTON_BUDGET_PREVIEW;
    }
    if (sample_budget <= 512 && max_path_depth <= 8) {
        return RUNTIME_CAUSTIC_PHOTON_BUDGET_INSPECTION;
    }
    return RUNTIME_CAUSTIC_PHOTON_BUDGET_FINAL;
}

const char* RuntimeCausticPhotonBudgetTier3D_Label(
    RuntimeCausticPhotonBudgetTier3D tier) {
    switch (tier) {
        case RUNTIME_CAUSTIC_PHOTON_BUDGET_PREVIEW: return "preview";
        case RUNTIME_CAUSTIC_PHOTON_BUDGET_INSPECTION: return "inspection";
        case RUNTIME_CAUSTIC_PHOTON_BUDGET_FINAL: return "final";
        default: return "preview";
    }
}

void RuntimeCausticPhotonMapLifecycle3D_Evaluate(
    const RuntimeCausticPhotonMapLifecycleInput3D* input,
    RuntimeCausticPhotonMapLifecycleState3D* io_state,
    RuntimeCausticPhotonMapLifecycleReadback3D* out_readback) {
    RuntimeCausticPhotonMapLifecycleReadback3D readback;

    memset(&readback, 0, sizeof(readback));
    if (out_readback) *out_readback = readback;
    if (!input || !io_state || !out_readback) return;
    readback.evaluated = true;
    readback.persistentMapOwnershipEnabled = input->persistentMapOwnershipEnabled;
    readback.geometryKey = input->geometryKey;
    readback.lightKey = input->lightKey;
    readback.materialKey = input->materialKey;
    readback.volumeKey = input->volumeKey;
    readback.budgetKey = input->budgetKey;
    if (!input->persistentMapOwnershipEnabled) {
        readback.rebuilt = true;
        readback.rebuildReason = RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_PER_FRAME_POLICY;
    } else if (!io_state->valid) {
        readback.rebuilt = true;
        readback.rebuildReason = RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_FIRST_BUILD;
    } else if (input->explicitRebuildRequested) {
        readback.rebuilt = true;
        readback.rebuildReason = RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_EXPLICIT_REQUEST;
    } else if (input->geometryKey != io_state->input.geometryKey) {
        readback.rebuilt = true;
        readback.rebuildReason = RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_GEOMETRY_CHANGED;
    } else if (input->lightKey != io_state->input.lightKey) {
        readback.rebuilt = true;
        readback.rebuildReason = RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_LIGHT_CHANGED;
    } else if (input->materialKey != io_state->input.materialKey) {
        readback.rebuilt = true;
        readback.rebuildReason = RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_MATERIAL_CHANGED;
    } else if (input->volumeKey != io_state->input.volumeKey) {
        readback.rebuilt = true;
        readback.rebuildReason = RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_VOLUME_CHANGED;
    } else if (input->budgetKey != io_state->input.budgetKey) {
        readback.rebuilt = true;
        readback.rebuildReason = RUNTIME_CAUSTIC_PHOTON_MAP_REBUILD_BUDGET_CHANGED;
    } else {
        readback.reused = true;
    }
    io_state->valid = true;
    io_state->input = *input;
    *out_readback = readback;
}
