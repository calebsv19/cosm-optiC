#include "render/runtime_caustic_photon_path_population_3d.h"

#include <math.h>
#include <string.h>

#include "render/runtime_caustic_photon_volume_beam_estimator_3d.h"

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

static bool photon_path_population_is_specular_or_transmission(
    RuntimeCausticPhotonBsdfLobe3D lobe) {
    return lobe == RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_TRANSMISSION ||
           lobe == RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_SPECULAR ||
           lobe == RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_GLOSSY;
}

static bool photon_path_population_is_caustic_event(
    const RuntimeCausticPhotonSceneHitEvent3D* hit) {
    bool eta_contrast;
    if (!hit || !hit->bsdfSelection.selected) return false;
    eta_contrast =
        fabs(hit->material.opticalIor - 1.0) > 1.0e-6 ||
        fabs(hit->mediumTransition.topBefore.ior -
             hit->mediumTransition.topAfter.ior) > 1.0e-6 ||
        fabs(hit->mediumTransition.topBefore.ior -
             hit->mediumTransition.boundary.ior) > 1.0e-6 ||
        fabs(hit->mediumTransition.topAfter.ior -
             hit->mediumTransition.boundary.ior) > 1.0e-6;
    if (hit->bsdfSelection.lobe == RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_SPECULAR ||
        hit->bsdfSelection.lobe == RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_GLOSSY) {
        return !photon_path_population_transparent(&hit->material) || eta_contrast;
    }
    if (hit->bsdfSelection.lobe != RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_TRANSMISSION ||
        !hit->mediumTransition.succeeded) {
        return false;
    }
    return eta_contrast;
}

static RuntimeCausticPhotonSurfacePathClass3D
photon_path_population_surface_path_class(
    const RuntimeCausticPhotonSceneTrace3D* path,
    uint32_t receiver_hit_index) {
    int first_entry_index = -1;
    int last_exit_index = -1;
    uint32_t entry_count = 0u;
    uint32_t exit_count = 0u;

    if (!path || receiver_hit_index > path->readback.hitEventCount) {
        return RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_UNCLASSIFIED;
    }
    for (uint32_t i = 0u; i < receiver_hit_index; ++i) {
        const RuntimeCausticPhotonSceneHitEvent3D* hit = &path->hitEvents[i];
        if (!hit->mediumTransition.succeeded ||
            !hit->mediumTransition.stackChanged) {
            continue;
        }
        if (hit->mediumTransition.reason ==
            RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_ENTER_PUSHED) {
            if (first_entry_index < 0) first_entry_index = (int)i;
            entry_count++;
        } else if (hit->mediumTransition.reason ==
                   RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_EXIT_POPPED) {
            last_exit_index = (int)i;
            exit_count++;
        }
    }
    if (first_entry_index < 0 || last_exit_index < 0 || entry_count == 0u ||
        entry_count != exit_count) {
        return RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_UNCLASSIFIED;
    }
    return last_exit_index == first_entry_index + 1 &&
                   receiver_hit_index == (uint32_t)last_exit_index + 1u
               ? RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_DIRECT_TWO_INTERFACE
               : RUNTIME_CAUSTIC_PHOTON_SURFACE_PATH_MULTIPATH;
}

static RuntimeCausticPhotonPathProvenance3D
photon_path_population_provenance(
    const RuntimeCausticPhotonSceneTrace3D* path,
    bool prior_specular_or_transmission,
    uint32_t entry_count,
    uint32_t exit_count,
    int original_medium_id,
    RuntimeCausticPhotonSegmentStage3D stage) {
    RuntimeCausticPhotonPathProvenance3D provenance;
    memset(&provenance, 0, sizeof(provenance));
    provenance.priorSpecularOrTransmission = prior_specular_or_transmission;
    provenance.dielectricEntryCount = entry_count;
    provenance.dielectricExitCount = exit_count;
    provenance.firstDielectricSceneObjectIndex = -1;
    provenance.originalMediumId = original_medium_id;
    provenance.segmentStage = stage;
    if (path) {
        for (uint32_t i = 0u; i < path->readback.hitEventCount; ++i) {
            const RuntimeCausticPhotonSceneHitEvent3D* hit =
                &path->hitEvents[i];
            if (hit->mediumTransition.succeeded &&
                hit->mediumTransition.stackChanged &&
                hit->mediumTransition.reason ==
                    RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_ENTER_PUSHED) {
                provenance.firstDielectricSceneObjectIndex =
                    hit->hit.sceneObjectIndex;
                break;
            }
        }
        provenance.emittedProposalDirection = path->trace.sample.proposalDirection;
        provenance.emittedProposalPdf = path->trace.sample.proposalPdf;
        provenance.emittedSourceSelectionPdf =
            path->trace.sample.sourceSelectionPdf;
        provenance.emittedPositionPdf = path->trace.sample.positionPdf;
        provenance.emittedDirectionPdf = path->trace.sample.directionPdf;
        provenance.emittedFluxCorrection =
            path->trace.sample.emissionFluxCorrection;
        provenance.emittedFluxPdfCompensated =
            path->trace.sample.fluxPdfCompensated;
        provenance.guidedDirection = path->trace.sample.direction;
        provenance.guidingChangedSample =
            path->trace.sample.guidingChangedSample;
        provenance.guidingPdfFluxCorrected =
            path->trace.sample.guidingPdfFluxCorrected;
    }
    return provenance;
}

static RuntimeCausticPhotonRecordRejectReason3D
photon_path_population_reject_reason(
    RuntimeCausticPhotonPathPopulationTermination3D termination) {
    switch (termination) {
        case RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_INVALID_TRACE:
            return RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_TRACE;
        case RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_INVALID_TARGETS:
            return RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_TARGETS;
        case RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_NO_DIFFUSE_RECEIVER:
            return RUNTIME_CAUSTIC_PHOTON_REJECT_NO_DIFFUSE_RECEIVER;
        case RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_NO_CAUSTIC_HISTORY:
            return RUNTIME_CAUSTIC_PHOTON_REJECT_NO_CAUSTIC_HISTORY;
        case RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_REFLECTION_ONLY_SURFACE_REJECTED:
            return RUNTIME_CAUSTIC_PHOTON_REJECT_REFLECTION_ONLY_SURFACE;
        case RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_UNRECONCILED_DIELECTRIC_SURFACE_REJECTED:
            return RUNTIME_CAUSTIC_PHOTON_REJECT_UNRECONCILED_DIELECTRIC_SURFACE;
        case RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_INVALID_CANDIDATE:
            return RUNTIME_CAUSTIC_PHOTON_REJECT_INVALID_CANDIDATE;
        case RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_CAPACITY_REJECTED:
            return RUNTIME_CAUSTIC_PHOTON_REJECT_CAPACITY;
        case RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_STORE_REJECTED:
        default:
            return RUNTIME_CAUSTIC_PHOTON_REJECT_STORE;
    }
}

static void photon_path_population_mark_rejected(
    RuntimeCausticPhotonPathPopulationReadback3D* readback,
    RuntimeCausticPhotonPathPopulationTermination3D termination,
    bool surface_requested,
    bool beam_requested) {
    RuntimeCausticPhotonRecordRejectReason3D reason;
    if (!readback) return;
    reason = photon_path_population_reject_reason(termination);
    if (surface_requested) readback->retention.surfaceRejected[reason]++;
    if (beam_requested) readback->retention.beamRejected[reason]++;
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
    settings->requirePriorSpecularOrTransmission = true;
    settings->requireReconciledDielectricTransmissionForSurface = true;
    settings->surfaceQueryRadius = 0.10;
    settings->beamRadiusStart = 0.04;
    settings->beamRadiusEnd = 0.08;
    settings->beamTransmittance = 1.0;
    settings->beamDensityWeight = 1.0;
    settings->beamMediumId = 0;
    settings->requireBeamMediumId = true;
    settings->requireBeamStage = true;
    settings->beamStage = RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_LENS;
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
        case RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_NO_CAUSTIC_HISTORY:
            return "no_prior_specular_or_transmission";
        case RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_REFLECTION_ONLY_SURFACE_REJECTED:
            return "reflection_only_surface_rejected";
        case RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_UNRECONCILED_DIELECTRIC_SURFACE_REJECTED:
            return "unreconciled_dielectric_surface_rejected";
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
    bool saw_noncaustic_diffuse_receiver = false;
    bool saw_reflection_only_surface = false;
    bool saw_unreconciled_dielectric_surface = false;
    bool prior_specular_or_transmission = false;
    bool selected_first_diffuse_surface = false;
    uint32_t entry_count = 0u;
    uint32_t exit_count = 0u;

    memset(&readback, 0, sizeof(readback));
    if (out_readback) *out_readback = readback;
    if (!path || !out_readback) return false;
    readback.attempted = true;
    readback.emittedFlux = path->trace.debug.emittedFlux;
    readback.mediumAbsorbedFlux = path->readback.mediumAbsorbedFlux;
    photon_path_population_terminal_energy(path, &readback);
    if (!active) {
        RuntimeCausticPhotonPathPopulation3D_DefaultSettings(&defaults);
        active = &defaults;
    }
    if (!path->trace.valid || !path->readback.succeeded ||
        path->readback.hitEventCount > PHOTON_PATH_POPULATION_MAX_CANDIDATES) {
        readback.termination = RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_INVALID_TRACE;
        photon_path_population_mark_rejected(&readback,
                                             readback.termination,
                                             active ? active->storeDiffuseSurfaces : true,
                                             active ? active->storeTraversedBeams : true);
        *out_readback = readback;
        return false;
    }
    if ((!active->storeDiffuseSurfaces && !active->storeTraversedBeams) ||
        (active->storeDiffuseSurfaces && !surface_map) ||
        (active->storeTraversedBeams && !beam_map)) {
        readback.termination = RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_INVALID_TARGETS;
        photon_path_population_mark_rejected(&readback,
                                             readback.termination,
                                             active->storeDiffuseSurfaces,
                                             active->storeTraversedBeams);
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
            if (active->requirePriorSpecularOrTransmission &&
                !prior_specular_or_transmission) {
                saw_noncaustic_diffuse_receiver = true;
                continue;
            }
            has_receiver = true;
            last_receiver_index = i;
            if (active->storeDiffuseSurfaces &&
                active->requireReconciledDielectricTransmissionForSurface &&
                !(entry_count > 0u && entry_count == exit_count) &&
                !(active->allowActiveMediumSurfaceReceiver &&
                  entry_count == exit_count + 1u)) {
                if (entry_count == 0u && exit_count == 0u) {
                    if (!saw_reflection_only_surface) {
                        readback.retention.surfaceRejected[
                            RUNTIME_CAUSTIC_PHOTON_REJECT_REFLECTION_ONLY_SURFACE]++;
                    }
                    saw_reflection_only_surface = true;
                } else {
                    if (!saw_unreconciled_dielectric_surface) {
                        readback.retention.surfaceRejected[
                            RUNTIME_CAUSTIC_PHOTON_REJECT_UNRECONCILED_DIELECTRIC_SURFACE]++;
                    }
                    saw_unreconciled_dielectric_surface = true;
                }
                continue;
            }
            if (active->storeDiffuseSurfaces &&
                selected_first_diffuse_surface) {
                readback.retention.surfaceRejected[
                    RUNTIME_CAUSTIC_PHOTON_REJECT_POST_FIRST_DIFFUSE_RECEIVER]++;
                continue;
            }
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
            record->materialId = hit->material.materialId;
            record->provenance = photon_path_population_provenance(
                path,
                prior_specular_or_transmission,
                entry_count,
                exit_count,
                hit->segmentMedium.mediumId,
                RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_RECEIVER);
            record->provenance.surfacePathClass =
                photon_path_population_surface_path_class(path, i);
            if (active->storeDiffuseSurfaces) {
                selected_first_diffuse_surface = true;
            }
        }
        if (hit->mediumTransition.succeeded && hit->mediumTransition.stackChanged) {
            if (hit->mediumTransition.reason ==
                RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_ENTER_PUSHED) {
                entry_count++;
            } else if (hit->mediumTransition.reason ==
                       RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_EXIT_POPPED) {
                exit_count++;
            }
        }
        if (photon_path_population_is_caustic_event(hit)) {
            prior_specular_or_transmission = true;
        }
    }
    readback.dielectricEntryCount = entry_count;
    readback.dielectricExitCount = exit_count;
    if (!has_receiver) {
        readback.storedSurfaceFlux = vec3(0.0, 0.0, 0.0);
        readback.termination = saw_noncaustic_diffuse_receiver
                                   ? RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_NO_CAUSTIC_HISTORY
                                   : RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_NO_DIFFUSE_RECEIVER;
        photon_path_population_mark_rejected(&readback,
                                             readback.termination,
                                             active->storeDiffuseSurfaces,
                                             active->storeTraversedBeams);
        *out_readback = readback;
        return false;
    }
    if (active->storeDiffuseSurfaces && readback.diffuseReceiverCount == 0u &&
        !active->storeTraversedBeams) {
        readback.termination = saw_reflection_only_surface
                                   ? RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_REFLECTION_ONLY_SURFACE_REJECTED
                                   : RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_UNRECONCILED_DIELECTRIC_SURFACE_REJECTED;
        *out_readback = readback;
        return false;
    }

    if (active->storeTraversedBeams) {
        for (uint32_t i = 0u; i <= last_receiver_index; ++i) {
            const RuntimeCausticPhotonSceneHitEvent3D* hit = &path->hitEvents[i];
            RuntimeCausticPhotonVolumeBeamSegment3D candidate;
            RuntimeCausticPhotonVolumeBeamSegment3D clipped;
            RuntimeCausticPhotonVolumeBeamEstimatorSettings3D eligibility_settings;
            RuntimeCausticPhotonVolumeBeamEligibility3D eligibility;

            memset(&candidate, 0, sizeof(candidate));
            candidate.photonId = path->trace.sample.photonId;
            candidate.depth = hit->depth;
            candidate.start = hit->pathStart;
            candidate.end = hit->hit.position;
            candidate.direction = vec3_normalize(
                vec3_sub(candidate.end, candidate.start));
            candidate.flux = hit->bsdfSelection.throughputBefore;
            candidate.radiusStart = active->beamRadiusStart;
            candidate.radiusEnd = active->beamRadiusEnd;
            candidate.transmittance = active->beamTransmittance;
            candidate.densityWeight = active->beamDensityWeight;
            candidate.mediumId = hit->segmentMedium.mediumId;
            candidate.provenance = photon_path_population_provenance(
                path,
                false,
                0u,
                0u,
                hit->segmentMedium.mediumId,
                !hit->segmentMedium.isAir
                    ? RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_LENS_INTERIOR
                    : RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_SOURCE_TO_LENS);
            for (uint32_t j = 0u; j < i; ++j) {
                const RuntimeCausticPhotonSceneHitEvent3D* prior =
                    &path->hitEvents[j];
                if (prior->mediumTransition.succeeded &&
                    prior->mediumTransition.stackChanged) {
                    if (prior->mediumTransition.reason ==
                        RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_ENTER_PUSHED) {
                        candidate.provenance.dielectricEntryCount++;
                    } else if (prior->mediumTransition.reason ==
                               RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_EXIT_POPPED) {
                        candidate.provenance.dielectricExitCount++;
                    }
                }
                if (prior->bsdfSelection.selected &&
                    photon_path_population_is_specular_or_transmission(
                        prior->bsdfSelection.lobe)) {
                    candidate.provenance.priorSpecularOrTransmission = true;
                }
            }
            if (candidate.provenance.segmentStage !=
                    RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_LENS_INTERIOR &&
                candidate.provenance.dielectricExitCount > 0u) {
                candidate.provenance.segmentStage =
                    RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_LENS;
            }
            readback.beamExaminedCount++;
            RuntimeCausticPhotonVolumeBeamEstimator3D_DefaultSettings(
                &eligibility_settings);
            eligibility_settings.requireMediumId = active->requireBeamMediumId;
            eligibility_settings.mediumId = active->beamMediumId;
            eligibility_settings.requireSegmentStage = active->requireBeamStage;
            eligibility_settings.segmentStage = active->beamStage;
            eligibility =
                RuntimeCausticPhotonVolumeBeamEstimator3D_SegmentEligibility(
                    &candidate, &eligibility_settings);
            if (eligibility != RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_ELIGIBLE) {
                readback.beamEligibility[eligibility]++;
                continue;
            }
            if (active->clipBeamsToBounds) {
                if (!RuntimeCausticPhotonVolumeBeamEstimator3D_ClipSegmentToBounds(
                        &candidate,
                        active->beamBoundsMin,
                        active->beamBoundsMax,
                        &clipped)) {
                    readback.beamEligibility[
                        RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_OUTSIDE_VOLUME]++;
                    continue;
                }
                if (vec3_length(vec3_sub(clipped.start, candidate.start)) > 1.0e-9 ||
                    vec3_length(vec3_sub(clipped.end, candidate.end)) > 1.0e-9) {
                    readback.beamClippedCount++;
                }
                candidate = clipped;
            }
            readback.beamEligibility[
                RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_ELIGIBLE]++;
            beams[readback.beamCandidateCount++] = candidate;
        }
    }

    for (uint32_t i = 0u; i < readback.diffuseReceiverCount; ++i) {
        if (!photon_path_population_surface_valid(&surfaces[i])) {
            readback.termination =
                RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_INVALID_CANDIDATE;
            photon_path_population_mark_rejected(&readback,
                                                 readback.termination,
                                                 active->storeDiffuseSurfaces,
                                                 active->storeTraversedBeams);
            *out_readback = readback;
            return false;
        }
    }
    for (uint32_t i = 0u; i < readback.beamCandidateCount; ++i) {
        if (!photon_path_population_beam_valid(&beams[i])) {
            readback.termination =
                RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_INVALID_CANDIDATE;
            photon_path_population_mark_rejected(&readback,
                                                 readback.termination,
                                                 active->storeDiffuseSurfaces,
                                                 active->storeTraversedBeams);
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
        photon_path_population_mark_rejected(&readback,
                                             readback.termination,
                                             active->storeDiffuseSurfaces,
                                             active->storeTraversedBeams);
        *out_readback = readback;
        return false;
    }
    readback.preflightAccepted = true;

    if (active->storeDiffuseSurfaces) {
        for (uint32_t i = 0u; i < readback.diffuseReceiverCount; ++i) {
            if (!RuntimeCausticPhotonMap3D_StoreRecord(surface_map, &surfaces[i])) {
                readback.termination =
                    RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_STORE_REJECTED;
                photon_path_population_mark_rejected(&readback,
                                                     readback.termination,
                                                     true,
                                                     false);
                *out_readback = readback;
                return false;
            }
            readback.storedSurfaceCount++;
            readback.retention.surfaceRetained[
                surfaces[i].provenance.dielectricEntryCount > 0u &&
                        surfaces[i].provenance.dielectricEntryCount ==
                            surfaces[i].provenance.dielectricExitCount
                    ? RUNTIME_CAUSTIC_PHOTON_SURFACE_RETAIN_RECONCILED_TRANSMISSION
                    : surfaces[i].provenance.priorSpecularOrTransmission
                          ? RUNTIME_CAUSTIC_PHOTON_SURFACE_RETAIN_WITH_SPECULAR_HISTORY
                          : RUNTIME_CAUSTIC_PHOTON_SURFACE_RETAIN_WITHOUT_SPECULAR_HISTORY]++;
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
                photon_path_population_mark_rejected(&readback,
                                                     readback.termination,
                                                     false,
                                                     true);
                *out_readback = readback;
                return false;
            }
            readback.storedBeamCount++;
            readback.retention.beamRetained[beams[i].provenance.segmentStage]++;
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
    batch->dielectricEntryCount += path_readback->dielectricEntryCount;
    batch->dielectricExitCount += path_readback->dielectricExitCount;
    for (int i = 0; i < RUNTIME_CAUSTIC_PHOTON_SURFACE_RETAIN_REASON_COUNT; ++i) {
        batch->retention.surfaceRetained[i] +=
            path_readback->retention.surfaceRetained[i];
    }
    for (int i = 0; i < RUNTIME_CAUSTIC_PHOTON_RECORD_REJECT_REASON_COUNT; ++i) {
        batch->retention.surfaceRejected[i] +=
            path_readback->retention.surfaceRejected[i];
        batch->retention.beamRejected[i] +=
            path_readback->retention.beamRejected[i];
    }
    for (int i = 0; i < RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_COUNT; ++i) {
        batch->retention.beamRetained[i] +=
            path_readback->retention.beamRetained[i];
    }
    batch->beamExaminedCount += path_readback->beamExaminedCount;
    batch->beamClippedCount += path_readback->beamClippedCount;
    for (int i = 0;
         i <= RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_INVALID;
         ++i) {
        batch->beamEligibility[i] += path_readback->beamEligibility[i];
    }
    batch->emittedFlux = vec3_add(batch->emittedFlux, path_readback->emittedFlux);
    batch->activeFlux = vec3_add(batch->activeFlux, path_readback->activeFlux);
    batch->storedSurfaceFlux =
        vec3_add(batch->storedSurfaceFlux, path_readback->storedSurfaceFlux);
    batch->storedBeamFlux =
        vec3_add(batch->storedBeamFlux, path_readback->storedBeamFlux);
    batch->mediumAbsorbedFlux =
        vec3_add(batch->mediumAbsorbedFlux,
                 path_readback->mediumAbsorbedFlux);
    batch->escapedFlux = vec3_add(batch->escapedFlux, path_readback->escapedFlux);
    batch->absorbedFlux = vec3_add(batch->absorbedFlux, path_readback->absorbedFlux);
    batch->rouletteTerminatedFlux =
        vec3_add(batch->rouletteTerminatedFlux,
                 path_readback->rouletteTerminatedFlux);
    batch->rejectedFlux =
        vec3_add(batch->rejectedFlux, path_readback->rejectedFlux);
}
