#include "render/runtime_caustic_photon_path_population_3d.h"

#include <math.h>
#include <string.h>

enum {
    PHOTON_PATH_POPULATION_MAX_CANDIDATES =
        RUNTIME_CAUSTIC_PHOTON_TRACE_MAX_DIELECTRIC_EVENTS
};

static double photon_path_population_luma(Vec3 value) {
    return 0.2126 * value.x + 0.7152 * value.y + 0.0722 * value.z;
}

static bool photon_path_population_vec_finite(Vec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool photon_path_population_transparent(
    const RuntimeMaterialPayload3D* material) {
    return material && (material->transparency > 1.0e-9 || material->thinWalled);
}

static bool photon_path_population_surface_valid(
    const RuntimeCausticPhotonMapRecord3D* record) {
    return record && photon_path_population_vec_finite(record->position) &&
           photon_path_population_vec_finite(record->normal) &&
           photon_path_population_vec_finite(record->incidentDirection) &&
           photon_path_population_vec_finite(record->flux) &&
           record->pathPdf > 1.0e-12 && record->queryRadius > 1.0e-9 &&
           photon_path_population_luma(record->flux) > 1.0e-12;
}

static bool photon_path_population_beam_valid(
    const RuntimeCausticPhotonVolumeBeamSegment3D* segment) {
    Vec3 axis;
    if (!segment || !photon_path_population_vec_finite(segment->start) ||
        !photon_path_population_vec_finite(segment->end) ||
        !photon_path_population_vec_finite(segment->direction) ||
        !photon_path_population_vec_finite(segment->flux)) {
        return false;
    }
    axis = vec3_sub(segment->end, segment->start);
    return vec3_dot(axis, axis) > 1.0e-12 &&
           segment->transmittance > 1.0e-12 &&
           segment->densityWeight > 1.0e-12 &&
           photon_path_population_luma(segment->flux) > 1.0e-12;
}

static void photon_path_population_terminal_energy(
    const RuntimeCausticPhotonSceneTrace3D* path,
    RuntimeCausticPhotonPathPopulationReadback3D* readback) {
    Vec3 terminal_flux;
    if (!path || !readback) return;
    terminal_flux = path->trace.debug.rejectedFlux;
    switch (path->readback.termination) {
        case RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_ESCAPED:
            readback->escapedFlux = terminal_flux;
            break;
        case RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_ABSORBED:
            readback->absorbedFlux = terminal_flux;
            break;
        case RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_ROULETTE_TERMINATED:
            readback->rouletteTerminatedFlux = terminal_flux;
            break;
        case RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_MAX_DEPTH:
        case RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_BSDF_EVENT_READY:
            readback->activeFlux = path->trace.finalState.throughput;
            break;
        default:
            readback->rejectedFlux = terminal_flux;
            break;
    }
}

void RuntimeCausticPhotonPathPopulation3D_DefaultSettings(
    RuntimeCausticPhotonPathPopulationSettings3D* settings) {
    if (!settings) return;
    memset(settings, 0, sizeof(*settings));
    settings->storeDiffuseSurfaces = true;
    settings->storeTraversedBeams = true;
    settings->surfaceQueryRadius = 0.10;
    settings->beamRadiusStart = 0.04;
    settings->beamRadiusEnd = 0.08;
    settings->beamTransmittance = 1.0;
    settings->beamDensityWeight = 1.0;
}

const char* RuntimeCausticPhotonPathPopulationTermination3D_Label(
    RuntimeCausticPhotonPathPopulationTermination3D termination) {
    switch (termination) {
        case RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_COMPLETE:
            return "complete";
        case RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_INVALID_TRACE:
            return "invalid_trace";
        case RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_INVALID_TARGETS:
            return "invalid_targets";
        case RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_NO_DIFFUSE_RECEIVER:
            return "no_diffuse_receiver";
        case RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_INVALID_CANDIDATE:
            return "invalid_candidate";
        case RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_CAPACITY_REJECTED:
            return "capacity_rejected";
        case RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_STORE_REJECTED:
            return "store_rejected";
        case RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_NONE:
        default:
            return "none";
    }
}

bool RuntimeCausticPhotonPathPopulation3D_PopulateMaps(
    const RuntimeCausticPhotonSceneTrace3D* path,
    const RuntimeCausticPhotonPathPopulationSettings3D* settings,
    RuntimeCausticPhotonMap3D* surface_map,
    RuntimeCausticBeamMap3D* beam_map,
    RuntimeCausticPhotonPathPopulationReadback3D* out_readback) {
    RuntimeCausticPhotonPathPopulationSettings3D defaults;
    const RuntimeCausticPhotonPathPopulationSettings3D* active = settings;
    RuntimeCausticPhotonPathPopulationReadback3D readback;
    RuntimeCausticPhotonMapRecord3D surfaces[PHOTON_PATH_POPULATION_MAX_CANDIDATES];
    RuntimeCausticPhotonVolumeBeamSegment3D beams[
        PHOTON_PATH_POPULATION_MAX_CANDIDATES];
    uint32_t last_receiver_index = 0u;
    bool has_receiver = false;

    memset(&readback, 0, sizeof(readback));
    if (out_readback) *out_readback = readback;
    if (!path || !out_readback) return false;
    readback.attempted = true;
    readback.emittedFlux = path->trace.debug.emittedFlux;
    photon_path_population_terminal_energy(path, &readback);
    if (!active) {
        RuntimeCausticPhotonPathPopulation3D_DefaultSettings(&defaults);
        active = &defaults;
    }
    if (!path->trace.valid || !path->readback.succeeded ||
        path->readback.hitEventCount > PHOTON_PATH_POPULATION_MAX_CANDIDATES) {
        readback.termination = RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_INVALID_TRACE;
        *out_readback = readback;
        return false;
    }
    if ((!active->storeDiffuseSurfaces && !active->storeTraversedBeams) ||
        (active->storeDiffuseSurfaces && !surface_map) ||
        (active->storeTraversedBeams && !beam_map)) {
        readback.termination = RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_INVALID_TARGETS;
        *out_readback = readback;
        return false;
    }

    memset(surfaces, 0, sizeof(surfaces));
    memset(beams, 0, sizeof(beams));
    for (uint32_t i = 0u; i < path->readback.hitEventCount; ++i) {
        const RuntimeCausticPhotonSceneHitEvent3D* hit = &path->hitEvents[i];
        if (photon_path_population_transparent(&hit->material)) {
            readback.transparentHitCount++;
        }
        if (hit->bsdfSelection.termination !=
                RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_NONE ||
            hit->roulette.terminated) {
            readback.terminalHitCount++;
        }
        if (hit->bsdfSelection.selected &&
            hit->bsdfSelection.lobe == RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_DIFFUSE &&
            !photon_path_population_transparent(&hit->material) &&
            hit->bsdfDirection.valid) {
            RuntimeCausticPhotonMapRecord3D* record =
                &surfaces[readback.diffuseReceiverCount++];
            record->photonId = path->trace.sample.photonId;
            record->depth = hit->depth;
            record->position = hit->hit.position;
            record->normal = hit->hit.normal;
            record->incidentDirection =
                vec3_normalize(vec3_sub(hit->hit.position, hit->pathStart));
            record->flux = hit->bsdfSelection.throughputBefore;
            record->pathPdf = hit->pathPdfBefore;
            record->queryRadius = active->surfaceQueryRadius;
            record->sceneObjectIndex = hit->hit.sceneObjectIndex;
            record->primitiveIndex = hit->hit.primitiveIndex;
            record->triangleIndex = hit->hit.triangleIndex;
            last_receiver_index = i;
            has_receiver = true;
        }
    }
    if (!has_receiver) {
        readback.storedSurfaceFlux = vec3(0.0, 0.0, 0.0);
        readback.termination =
            RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_NO_DIFFUSE_RECEIVER;
        *out_readback = readback;
        return false;
    }

    if (active->storeTraversedBeams) {
        for (uint32_t i = 0u; i <= last_receiver_index; ++i) {
            const RuntimeCausticPhotonSceneHitEvent3D* hit = &path->hitEvents[i];
            RuntimeCausticPhotonVolumeBeamSegment3D* beam =
                &beams[readback.beamCandidateCount++];
            beam->photonId = path->trace.sample.photonId;
            beam->depth = hit->depth;
            beam->start = hit->pathStart;
            beam->end = hit->hit.position;
            beam->direction = vec3_normalize(vec3_sub(beam->end, beam->start));
            beam->flux = hit->bsdfSelection.throughputBefore;
            beam->radiusStart = active->beamRadiusStart;
            beam->radiusEnd = active->beamRadiusEnd;
            beam->transmittance = active->beamTransmittance;
            beam->densityWeight = active->beamDensityWeight;
            beam->mediumId = active->beamMediumId;
        }
    }

    for (uint32_t i = 0u; i < readback.diffuseReceiverCount; ++i) {
        if (!photon_path_population_surface_valid(&surfaces[i])) {
            readback.termination =
                RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_INVALID_CANDIDATE;
            *out_readback = readback;
            return false;
        }
    }
    for (uint32_t i = 0u; i < readback.beamCandidateCount; ++i) {
        if (!photon_path_population_beam_valid(&beams[i])) {
            readback.termination =
                RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_INVALID_CANDIDATE;
            *out_readback = readback;
            return false;
        }
    }
    if ((active->storeDiffuseSurfaces &&
         (!RuntimeCausticPhotonMap3D_IsAllocated(surface_map) ||
          surface_map->recordCount > surface_map->recordCapacity ||
          surface_map->recordCapacity - surface_map->recordCount <
              readback.diffuseReceiverCount)) ||
        (active->storeTraversedBeams &&
         (!RuntimeCausticBeamMap3D_IsAllocated(beam_map) ||
          beam_map->segmentCount > beam_map->segmentCapacity ||
          beam_map->segmentCapacity - beam_map->segmentCount <
              readback.beamCandidateCount))) {
        readback.termination =
            RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_CAPACITY_REJECTED;
        *out_readback = readback;
        return false;
    }
    readback.preflightAccepted = true;

    if (active->storeDiffuseSurfaces) {
        for (uint32_t i = 0u; i < readback.diffuseReceiverCount; ++i) {
            if (!RuntimeCausticPhotonMap3D_StoreRecord(surface_map, &surfaces[i])) {
                readback.termination =
                    RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_STORE_REJECTED;
                *out_readback = readback;
                return false;
            }
            readback.storedSurfaceCount++;
            readback.storedSurfaceFlux =
                vec3_add(readback.storedSurfaceFlux, surfaces[i].flux);
        }
    } else {
        readback.storedSurfaceFlux = vec3(0.0, 0.0, 0.0);
    }
    if (active->storeTraversedBeams) {
        for (uint32_t i = 0u; i < readback.beamCandidateCount; ++i) {
            if (!RuntimeCausticBeamMap3D_StoreSegment(beam_map, &beams[i])) {
                readback.termination =
                    RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_STORE_REJECTED;
                *out_readback = readback;
                return false;
            }
            readback.storedBeamCount++;
            readback.storedBeamFlux =
                vec3_add(readback.storedBeamFlux, beams[i].flux);
        }
    } else {
        readback.storedBeamFlux = vec3(0.0, 0.0, 0.0);
    }
    readback.succeeded = true;
    readback.termination = RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_COMPLETE;
    *out_readback = readback;
    return true;
}

void RuntimeCausticPhotonPathPopulationBatch3D_Accumulate(
    RuntimeCausticPhotonPathPopulationBatch3D* batch,
    const RuntimeCausticPhotonPathPopulationReadback3D* path_readback) {
    if (!batch || !path_readback || !path_readback->attempted) return;
    batch->pathCount++;
    if (path_readback->succeeded) {
        batch->succeededPathCount++;
    } else {
        batch->rejectedPathCount++;
    }
    batch->storedSurfaceCount += path_readback->storedSurfaceCount;
    batch->storedBeamCount += path_readback->storedBeamCount;
    batch->emittedFlux = vec3_add(batch->emittedFlux, path_readback->emittedFlux);
    batch->activeFlux = vec3_add(batch->activeFlux, path_readback->activeFlux);
    batch->storedSurfaceFlux =
        vec3_add(batch->storedSurfaceFlux, path_readback->storedSurfaceFlux);
    batch->storedBeamFlux =
        vec3_add(batch->storedBeamFlux, path_readback->storedBeamFlux);
    batch->escapedFlux = vec3_add(batch->escapedFlux, path_readback->escapedFlux);
    batch->absorbedFlux = vec3_add(batch->absorbedFlux, path_readback->absorbedFlux);
    batch->rouletteTerminatedFlux =
        vec3_add(batch->rouletteTerminatedFlux,
                 path_readback->rouletteTerminatedFlux);
    batch->rejectedFlux =
        vec3_add(batch->rejectedFlux, path_readback->rejectedFlux);
}
