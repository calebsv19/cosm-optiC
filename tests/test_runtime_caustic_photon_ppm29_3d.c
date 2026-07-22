#include "test_runtime_caustic_photon_ppm29_3d.h"

#include <math.h>
#include <string.h>

#include "render/runtime_caustic_photon_emission_proposal_3d.h"
#include "render/runtime_caustic_photon_emit_3d.h"
#include "render/runtime_caustic_photon_map_3d.h"
#include "render/runtime_caustic_photon_receiver_bsdf_3d.h"
#include "test_support.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static RuntimeCausticPhotonMapRecord3D ppm29_record(uint64_t id,
                                                     double x,
                                                     double path_pdf) {
    RuntimeCausticPhotonMapRecord3D record;
    memset(&record, 0, sizeof(record));
    record.photonId = id;
    record.position = vec3(x, 0.0, 0.0);
    record.normal = vec3(0.0, 1.0, 0.0);
    record.incidentDirection = vec3(0.0, -1.0, 0.0);
    record.flux = vec3(2.0, 1.0, 0.5);
    record.pathPdf = path_pdf;
    record.queryRadius = 0.2;
    return record;
}

static RuntimeCausticPhotonMapQuery3D ppm29_query(void) {
    RuntimeCausticPhotonMapQuery3D query;
    RuntimeCausticPhotonMap3D_DefaultQuery(&query);
    query.position = vec3(0.0, 0.0, 0.0);
    query.normal = vec3(0.0, 1.0, 0.0);
    query.radius = 0.1;
    query.requireReceiverIdentity = false;
    query.estimator.estimator = RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_RADIUS;
    query.estimator.minimumEffectiveSamples = 1u;
    return query;
}

static int test_ppm29_guided_proposal_contract(void) {
    RuntimeCausticPhotonSample3D sample;
    RuntimeCausticLensShape3D lens;
    RuntimeCausticPhotonEmissionProposalReadback3D readback;
    double expected_proposal_pdf;

    memset(&sample, 0, sizeof(sample));
    memset(&lens, 0, sizeof(lens));
    sample.photonId = 29u;
    sample.position = vec3(0.0, 2.0, 0.0);
    sample.direction = vec3(1.0, 0.0, 0.0);
    sample.proposalDirection = sample.direction;
    sample.flux = vec3(4.0, 2.0, 1.0);
    sample.sourceSelectionPdf = 0.5;
    sample.positionPdf = 2.0;
    sample.directionPdf = 1.0 / (4.0 * M_PI);
    sample.baseDirectionPdf = sample.directionPdf;
    lens.center = vec3(0.0, 0.0, 0.0);
    lens.radius = 0.25;
    lens.boundsMin = vec3(-0.25, -0.25, -0.25);
    lens.boundsMax = vec3(0.25, 0.25, 0.25);

    assert_true("ppm29_proposal_labels",
                RuntimeCausticPhotonEmissionProposalMode3D_FromLabel("unbiased") ==
                        RUNTIME_CAUSTIC_PHOTON_EMISSION_UNBIASED &&
                    RuntimeCausticPhotonEmissionProposalMode3D_FromLabel("lens_guided") ==
                        RUNTIME_CAUSTIC_PHOTON_EMISSION_LENS_GUIDED &&
                    strcmp(RuntimeCausticPhotonEmissionProposalMode3D_Label(
                               RUNTIME_CAUSTIC_PHOTON_EMISSION_UNBIASED),
                           "unbiased") == 0);
    assert_true("ppm29_guided_proposal_applies",
                RuntimeCausticPhotonEmissionProposal3D_ApplyLensGuidance(
                    &sample, NULL, &lens, &readback));
    expected_proposal_pdf = sample.sourceSelectionPdf * sample.positionPdf *
                            sample.directionPdf;
    assert_true("ppm29_guided_proposal_readback",
                readback.attempted && readback.applied &&
                    sample.guidingChangedSample && sample.guidingPdfFluxCorrected &&
                    sample.fluxPdfCompensated && sample.directionPdf > 0.0 &&
                    sample.emissionFluxCorrection > 0.0);
    assert_close("ppm29_guided_combined_pdf_once",
                 sample.proposalPdf,
                 expected_proposal_pdf,
                 1e-12);
    assert_close("ppm29_guided_likelihood_ratio",
                 sample.emissionFluxCorrection,
                 sample.baseDirectionPdf / sample.directionPdf,
                 1e-12);
    assert_close("ppm29_guided_flux_correction_once",
                 sample.flux.x,
                 4.0 * sample.emissionFluxCorrection,
                 1e-12);
    return 0;
}

static RuntimeCausticPhotonSample3D ppm29_mixture_sample(uint64_t sample_index) {
    RuntimeCausticPhotonSample3D sample;
    memset(&sample, 0, sizeof(sample));
    sample.photonId = sample_index + 100u;
    sample.sampleIndex = sample_index;
    sample.position = vec3(0.0, 2.0, 0.0);
    sample.direction = vec3(0.0, -1.0, 0.0);
    sample.proposalDirection = sample.direction;
    sample.flux = vec3(1.0, 1.0, 1.0);
    sample.sourceSelectionPdf = 1.0;
    sample.positionPdf = 1.0;
    sample.directionPdf = 1.0 / (4.0 * M_PI);
    sample.baseDirectionPdf = sample.directionPdf;
    return sample;
}

static RuntimeCausticLensShape3D ppm29_mixture_lens(double x) {
    RuntimeCausticLensShape3D lens;
    memset(&lens, 0, sizeof(lens));
    lens.center = vec3(x, 0.0, 0.0);
    lens.radius = 0.25;
    lens.boundsMin = vec3(x - 0.25, -0.25, -0.25);
    lens.boundsMax = vec3(x + 0.25, 0.25, 0.25);
    return lens;
}

static int test_ppm29_guided_mixture_selects_and_weights_separated_lenses(void) {
    RuntimeCausticLensShape3D lenses[2];
    RuntimeCausticPhotonSample3D left = ppm29_mixture_sample(0u);
    RuntimeCausticPhotonSample3D right = ppm29_mixture_sample(1u);
    RuntimeCausticPhotonEmissionProposalReadback3D left_readback;
    RuntimeCausticPhotonEmissionProposalReadback3D right_readback;

    lenses[0] = ppm29_mixture_lens(-0.75);
    lenses[1] = ppm29_mixture_lens(0.75);
    assert_true("ppm29_guided_mixture_left_applies",
                RuntimeCausticPhotonEmissionProposal3D_ApplyLensGuidanceMixture(
                    &left, NULL, lenses, 2u, &left_readback));
    assert_true("ppm29_guided_mixture_right_applies",
                RuntimeCausticPhotonEmissionProposal3D_ApplyLensGuidanceMixture(
                    &right, NULL, lenses, 2u, &right_readback));
    assert_true("ppm29_guided_mixture_balanced_selection",
                left_readback.lensCount == 2u &&
                    right_readback.lensCount == 2u &&
                    left_readback.selectedLensIndex == 0u &&
                    right_readback.selectedLensIndex == 1u &&
                    left.direction.x < 0.0 && right.direction.x > 0.0);
    assert_close("ppm29_guided_mixture_left_disjoint_pdf",
                 left_readback.mixtureDirectionPdf,
                 left_readback.selectedComponentDirectionPdf * 0.5,
                 1e-10);
    assert_close("ppm29_guided_mixture_right_disjoint_pdf",
                 right_readback.mixtureDirectionPdf,
                 right_readback.selectedComponentDirectionPdf * 0.5,
                 1e-10);
    assert_close("ppm29_guided_mixture_left_likelihood_ratio",
                 left.emissionFluxCorrection,
                 left.baseDirectionPdf / left.directionPdf,
                 1e-12);
    assert_close("ppm29_guided_mixture_right_likelihood_ratio",
                 right.emissionFluxCorrection,
                 right.baseDirectionPdf / right.directionPdf,
                 1e-12);
    return 0;
}

static int test_ppm29_guided_mixture_sums_overlapping_component_pdfs(void) {
    RuntimeCausticLensShape3D lenses[2];
    RuntimeCausticPhotonSample3D single = ppm29_mixture_sample(0u);
    RuntimeCausticPhotonSample3D mixture = single;
    RuntimeCausticPhotonEmissionProposalReadback3D single_readback;
    RuntimeCausticPhotonEmissionProposalReadback3D mixture_readback;

    lenses[0] = ppm29_mixture_lens(0.0);
    lenses[1] = lenses[0];
    assert_true("ppm29_guided_single_overlap_reference",
                RuntimeCausticPhotonEmissionProposal3D_ApplyLensGuidance(
                    &single, NULL, &lenses[0], &single_readback));
    assert_true("ppm29_guided_mixture_overlap_applies",
                RuntimeCausticPhotonEmissionProposal3D_ApplyLensGuidanceMixture(
                    &mixture, NULL, lenses, 2u, &mixture_readback));
    assert_close("ppm29_guided_mixture_overlap_pdf_not_halved",
                 mixture.directionPdf,
                 single.directionPdf,
                 1e-12);
    assert_close("ppm29_guided_mixture_overlap_correction_matches_single",
                 mixture.emissionFluxCorrection,
                 single.emissionFluxCorrection,
                 1e-12);
    assert_close("ppm29_guided_mixture_overlap_component_sum",
                 mixture_readback.mixtureDirectionPdf,
                 mixture_readback.selectedComponentDirectionPdf,
                 1e-12);
    return 0;
}

static int test_ppm29_source_selection_monte_carlo_contract(void) {
    RuntimeLightSet3D lights;
    RuntimeLightSource3D light;
    RuntimeCausticPhotonEmissionSettings3D settings;
    RuntimeCausticPhotonEmissionBatch3D batch;
    RuntimeCausticPhotonEmissionDiagnostics3D diagnostics;

    RuntimeLightSet3D_Init(&lights);
    RuntimeLightSource3D_Init(&light);
    light.kind = RUNTIME_LIGHT_SOURCE_3D_KIND_POINT;
    light.enabled = true;
    light.color = vec3(1.0, 0.0, 0.0);
    light.intensity = 2.0;
    assert_true("ppm29_source_append_red",
                RuntimeLightSet3D_Append(&lights, &light, NULL));
    RuntimeLightSource3D_Init(&light);
    light.kind = RUNTIME_LIGHT_SOURCE_3D_KIND_POINT;
    light.enabled = true;
    light.color = vec3(0.0, 1.0, 0.0);
    light.intensity = 6.0;
    assert_true("ppm29_source_append_green",
                RuntimeLightSet3D_Append(&lights, &light, NULL));
    RuntimeCausticPhotonEmission3D_DefaultSettings(&settings);
    settings.sampleBudget = 16384u;
    settings.baseSeed = 29029u;
    RuntimeCausticPhotonEmission3D_InitBatch(&batch);
    assert_true("ppm29_source_batch_allocate",
                RuntimeCausticPhotonEmission3D_AllocateBatch(
                    &batch, settings.sampleBudget));
    assert_true("ppm29_source_emit",
                RuntimeCausticPhotonEmission3D_EmitFromLightSet(
                    &batch, &lights, &settings, &diagnostics));
    assert_true("ppm29_source_pdf_fields_present",
                batch.samples[0].sourceSelectionPdf > 0.0 &&
                    batch.samples[0].positionPdf == 1.0 &&
                    batch.samples[0].directionPdf > 0.0 &&
                    batch.samples[0].proposalPdf == batch.samples[0].emissionPdf &&
                    batch.samples[0].fluxPdfCompensated);
    assert_close("ppm29_source_selection_red_power_unbiased",
                 diagnostics.totalEmittedFlux.x,
                 2.0,
                 0.08);
    assert_close("ppm29_source_selection_green_power_unbiased",
                 diagnostics.totalEmittedFlux.y,
                 6.0,
                 0.08);
    assert_close("ppm29_source_selection_no_blue_power",
                 diagnostics.totalEmittedFlux.z,
                 0.0,
                 1e-12);
    RuntimeCausticPhotonEmission3D_FreeBatch(&batch);
    RuntimeLightSet3D_Free(&lights);
    return 0;
}

static int test_ppm29_map_does_not_redivide_path_pdf(void) {
    RuntimeCausticPhotonMap3D map_a;
    RuntimeCausticPhotonMap3D map_b;
    RuntimeCausticPhotonMapQuery3D query = ppm29_query();
    RuntimeCausticPhotonMapQueryResult3D a;
    RuntimeCausticPhotonMapQueryResult3D b;
    RuntimeCausticPhotonMapRecord3D record_a = ppm29_record(1u, 0.02, 0.5);
    RuntimeCausticPhotonMapRecord3D record_b = ppm29_record(2u, 0.02, 0.125);

    RuntimeCausticPhotonMap3D_Init(&map_a);
    RuntimeCausticPhotonMap3D_Init(&map_b);
    assert_true("ppm29_exact_once_allocate",
                RuntimeCausticPhotonMap3D_Allocate(&map_a, 1u) &&
                    RuntimeCausticPhotonMap3D_Allocate(&map_b, 1u));
    assert_true("ppm29_exact_once_store",
                RuntimeCausticPhotonMap3D_StoreRecord(&map_a, &record_a) &&
                    RuntimeCausticPhotonMap3D_StoreRecord(&map_b, &record_b));
    assert_true("ppm29_exact_once_query",
                RuntimeCausticPhotonMap3D_Query(&map_a, &query, &a) &&
                    RuntimeCausticPhotonMap3D_Query(&map_b, &query, &b));
    assert_close("ppm29_path_pdf_is_audit_not_second_divisor", a.flux.x, b.flux.x, 1e-12);
    assert_true("ppm29_stored_flux_compensated_readback",
                a.storedFluxAlreadyPdfCompensated && b.storedFluxAlreadyPdfCompensated);
    RuntimeCausticPhotonMap3D_Free(&map_a);
    RuntimeCausticPhotonMap3D_Free(&map_b);
    return 0;
}

static int test_ppm29_incident_hemisphere_contract(void) {
    RuntimeCausticPhotonMap3D map;
    RuntimeCausticPhotonMapQuery3D query = ppm29_query();
    RuntimeCausticPhotonMapQueryResult3D result;
    RuntimeCausticPhotonMapRecord3D record = ppm29_record(3u, 0.01, 1.0);
    record.incidentDirection = vec3(0.0, 1.0, 0.0);
    RuntimeCausticPhotonMap3D_Init(&map);
    assert_true("ppm29_hemisphere_allocate",
                RuntimeCausticPhotonMap3D_Allocate(&map, 1u));
    assert_true("ppm29_hemisphere_store",
                RuntimeCausticPhotonMap3D_StoreRecord(&map, &record));
    assert_true("ppm29_reverse_incident_rejected",
                !RuntimeCausticPhotonMap3D_Query(&map, &query, &result) &&
                    result.incidentHemisphereRejectCount == 1u &&
                    result.effectiveSampleCount == 0u);
    RuntimeCausticPhotonMap3D_Free(&map);
    return 0;
}

static RuntimeMaterialPayload3D ppm29_material(double r,
                                                double g,
                                                double b,
                                                double roughness) {
    RuntimeMaterialPayload3D material;
    memset(&material, 0, sizeof(material));
    material.valid = true;
    material.baseColorR = r;
    material.baseColorG = g;
    material.baseColorB = b;
    material.bsdf.diffuseWeight = 1.0;
    material.bsdf.roughness = roughness;
    return material;
}

static int test_ppm29_receiver_bsdf_controls(void) {
    RuntimeCausticPhotonMapQueryResult3D query;
    RuntimeMaterialPayload3D black = ppm29_material(0.0, 0.0, 0.0, 0.2);
    RuntimeMaterialPayload3D red = ppm29_material(1.0, 0.0, 0.0, 0.2);
    RuntimeMaterialPayload3D white_rough = ppm29_material(1.0, 1.0, 1.0, 0.9);
    RuntimeMaterialPayload3D white_smooth = ppm29_material(1.0, 1.0, 1.0, 0.1);
    RuntimeCausticPhotonReceiverBsdfReadback3D readback;
    Vec3 radiance;
    Vec3 rough_radiance;

    memset(&query, 0, sizeof(query));
    query.hit = true;
    query.physicalFlux = vec3(2.0, 2.0, 2.0);
    query.meanIncidentCosine = 1.0;
    assert_true("ppm29_black_receiver_absorbs",
                !RuntimeCausticPhotonReceiverBsdf3D_EvaluateDiffuse(
                    &black, &query, &radiance, &readback) &&
                    radiance.x == 0.0 && radiance.y == 0.0 && radiance.z == 0.0);
    assert_true("ppm29_red_receiver_colors_flux",
                RuntimeCausticPhotonReceiverBsdf3D_EvaluateDiffuse(
                    &red, &query, &radiance, &readback) &&
                    radiance.x > 0.0 && radiance.y == 0.0 && radiance.z == 0.0);
    assert_close("ppm29_lambertian_normalization_once",
                 radiance.x,
                 2.0 / M_PI,
                 1e-12);
    assert_true("ppm29_white_rough_receiver",
                RuntimeCausticPhotonReceiverBsdf3D_EvaluateDiffuse(
                    &white_rough, &query, &rough_radiance, &readback) &&
                    readback.roughness == 0.9);
    assert_true("ppm29_diffuse_roughness_control_is_invariant",
                RuntimeCausticPhotonReceiverBsdf3D_EvaluateDiffuse(
                    &white_smooth, &query, &radiance, &readback));
    assert_close("ppm29_lambertian_roughness_invariance",
                 radiance.x,
                 rough_radiance.x,
                 1e-12);
    return 0;
}

int run_test_runtime_caustic_photon_ppm29_3d_tests(void) {
    int failures = 0;
    failures += test_ppm29_guided_proposal_contract();
    failures += test_ppm29_guided_mixture_selects_and_weights_separated_lenses();
    failures += test_ppm29_guided_mixture_sums_overlapping_component_pdfs();
    failures += test_ppm29_source_selection_monte_carlo_contract();
    failures += test_ppm29_map_does_not_redivide_path_pdf();
    failures += test_ppm29_incident_hemisphere_contract();
    failures += test_ppm29_receiver_bsdf_controls();
    return failures;
}
