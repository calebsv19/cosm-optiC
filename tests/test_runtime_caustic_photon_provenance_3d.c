#include "test_runtime_caustic_photon_provenance_3d.h"

#include <string.h>

#include "render/runtime_caustic_beam_map_3d.h"
#include "render/runtime_caustic_photon_map_3d.h"
#include "render/runtime_caustic_photon_provenance_3d.h"
#include "test_support.h"

static RuntimeCausticPhotonMapRecord3D provenance_surface_record(int triangle) {
    RuntimeCausticPhotonMapRecord3D record;
    memset(&record, 0, sizeof(record));
    record.photonId = 27u;
    record.position = vec3(0.0, 0.0, 0.0);
    record.normal = vec3(0.0, 0.0, 1.0);
    record.incidentDirection = vec3(0.0, 0.0, -1.0);
    record.flux = vec3(1.0, 1.0, 1.0);
    record.pathPdf = 1.0;
    record.queryRadius = 0.25;
    record.sceneObjectIndex = 4;
    record.primitiveIndex = 2;
    record.triangleIndex = triangle;
    record.materialId = 3;
    record.provenance.originalMediumId = 0;
    record.provenance.segmentStage =
        RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_RECEIVER;
    record.provenance.guidingChangedSample = true;
    record.provenance.guidingPdfFluxCorrected = false;
    return record;
}

static int test_photon_surface_provenance_and_receiver_patch(void) {
    RuntimeCausticPhotonMap3D map;
    RuntimeCausticPhotonMapRecord3D record = provenance_surface_record(0);
    RuntimeCausticPhotonMapQuery3D query;
    RuntimeCausticPhotonMapQueryResult3D result;

    RuntimeCausticPhotonMap3D_Init(&map);
    assert_true("ppm27a_surface_allocate",
                RuntimeCausticPhotonMap3D_Allocate(&map, 2u));
    assert_true("ppm27a_direct_history_missing_is_currently_retained",
                RuntimeCausticPhotonMap3D_StoreRecord(&map, &record));
    assert_true("ppm27a_surface_provenance_survives_storage",
                !map.records[0].provenance.priorSpecularOrTransmission &&
                    map.records[0].provenance.guidingChangedSample &&
                    !map.records[0].provenance.guidingPdfFluxCorrected &&
                    map.records[0].provenance.segmentStage ==
                        RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_RECEIVER);

    RuntimeCausticPhotonMap3D_DefaultQuery(&query);
    query.position = record.position;
    query.normal = record.normal;
    query.radius = record.queryRadius;
    query.requireReceiverIdentity = true;
    query.sceneObjectIndex = record.sceneObjectIndex;
    query.primitiveIndex = record.primitiveIndex;
    query.triangleIndex = 1;
    query.materialId = record.materialId;
    query.estimator.minimumEffectiveSamples = 1u;
    assert_true("ppm28_flipped_diagonal_receiver_patch_retained",
                RuntimeCausticPhotonMap3D_Query(&map, &query, &result) &&
                    result.receiverRejectCount == 0u);
    RuntimeCausticPhotonMap3D_Free(&map);
    return 0;
}

static int test_photon_beam_map_is_camera_independent(void) {
    RuntimeCausticBeamMap3D map;
    RuntimeCausticPhotonVolumeBeamSegment3D beam;
    RuntimeCausticBeamMapQuery3D query;
    RuntimeCausticBeamMapQueryResult3D forward;
    RuntimeCausticBeamMapQueryResult3D reverse;
    RuntimeCausticBeamMapQueryResult3D perpendicular;

    memset(&beam, 0, sizeof(beam));
    beam.photonId = 28u;
    beam.start = vec3(0.0, 0.0, 0.0);
    beam.end = vec3(0.0, 0.0, 2.0);
    beam.direction = vec3(0.0, 0.0, 1.0);
    beam.flux = vec3(1.0, 1.0, 1.0);
    beam.radiusStart = 0.2;
    beam.radiusEnd = 0.2;
    beam.transmittance = 1.0;
    beam.densityWeight = 1.0;
    beam.mediumId = 0;
    beam.provenance.originalMediumId = 0;
    beam.provenance.segmentStage =
        RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_LENS;

    RuntimeCausticBeamMap3D_Init(&map);
    assert_true("ppm27a_beam_allocate",
                RuntimeCausticBeamMap3D_Allocate(&map, 1u));
    assert_true("ppm27a_beam_store",
                RuntimeCausticBeamMap3D_StoreSegment(&map, &beam));
    assert_true("ppm27a_beam_original_medium_survives_storage",
                map.segments[0].mediumId == 0 &&
                    map.segments[0].provenance.originalMediumId == 0);

    RuntimeCausticBeamMap3D_DefaultQuery(&query);
    query.position = vec3(0.0, 0.0, 1.0);
    query.radius = 0.2;
    query.requireMediumId = true;
    query.mediumId = 0;
    query.direction = vec3(0.0, 0.0, 1.0);
    assert_true("ppm27a_beam_forward_hit",
                RuntimeCausticBeamMap3D_Query(&map, &query, &forward));
    query.direction = vec3(0.0, 0.0, -1.0);
    assert_true("ppm30_beam_irradiance_reverse_camera_independent",
                RuntimeCausticBeamMap3D_Query(&map, &query, &reverse) &&
                    reverse.contributingCount == forward.contributingCount &&
                    reverse.physicalFlux.x == forward.physicalFlux.x);
    query.direction = vec3(1.0, 0.0, 0.0);
    assert_true("ppm30_beam_irradiance_perpendicular_camera_independent",
                RuntimeCausticBeamMap3D_Query(&map, &query, &perpendicular) &&
                    perpendicular.directionRejectCount == 0u &&
                    perpendicular.physicalFlux.x == forward.physicalFlux.x);
    RuntimeCausticBeamMap3D_Free(&map);
    return 0;
}

static int test_ppm27a_stable_labels(void) {
    assert_true("ppm27a_stage_source_label",
                strcmp(RuntimeCausticPhotonSegmentStage3D_Label(
                           RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_SOURCE_TO_LENS),
                       "source_to_lens") == 0);
    assert_true("ppm27a_stage_interior_label",
                strcmp(RuntimeCausticPhotonSegmentStage3D_Label(
                           RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_LENS_INTERIOR),
                       "lens_interior") == 0);
    assert_true("ppm27a_stage_post_lens_label",
                strcmp(RuntimeCausticPhotonSegmentStage3D_Label(
                           RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_LENS),
                       "post_lens") == 0);
    assert_true("ppm27a_stage_post_receiver_label",
                strcmp(RuntimeCausticPhotonSegmentStage3D_Label(
                           RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_RECEIVER),
                       "post_receiver") == 0);
    return 0;
}

int run_test_runtime_caustic_photon_provenance_3d_tests(void) {
    int failures = 0;
    failures += test_photon_surface_provenance_and_receiver_patch();
    failures += test_photon_beam_map_is_camera_independent();
    failures += test_ppm27a_stable_labels();
    return failures;
}
