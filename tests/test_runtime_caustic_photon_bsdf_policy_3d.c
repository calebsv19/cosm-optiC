#include "test_runtime_caustic_photon_bsdf_policy_3d.h"

#include <math.h>
#include <string.h>

#include "render/runtime_caustic_photon_bsdf_policy_3d.h"
#include "test_support.h"
#include "render/runtime_water_material_3d.h"
#include "render/runtime_caustic_photon_medium_stack_3d.h"

static RuntimeMaterialPayload3D photon_bsdf_material(void) {
    RuntimeMaterialPayload3D material;
    RuntimeMaterialPayload3D_Reset(&material);
    material.valid = true;
    material.baseColorR = 0.8;
    material.baseColorG = 0.6;
    material.baseColorB = 0.4;
    material.opticalIor = 1.5;
    material.bsdf.ior = 1.5;
    material.bsdf.diffuseWeight = 1.0;
    material.bsdf.roughness = 0.5;
    return material;
}

static int test_runtime_caustic_photon_bsdf_policy_labels_and_invalid(void) {
    RuntimeMaterialPayload3D material = photon_bsdf_material();
    RuntimeCausticPhotonBsdfPolicy3D policy;
    assert_true("runtime_caustic_photon_bsdf_lobe_labels",
                strcmp(RuntimeCausticPhotonBsdfLobe3D_Label(
                           RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_TRANSMISSION),
                       "transmission") == 0);
    assert_true("runtime_caustic_photon_bsdf_termination_labels",
                strcmp(RuntimeCausticPhotonBsdfTermination3D_Label(
                           RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_ABSORBED),
                       "absorbed") == 0);
    assert_true("runtime_caustic_photon_bsdf_invalid_material",
                !RuntimeCausticPhotonBsdfPolicy3D_Build(
                    NULL,
                    1.0,
                    vec3(1.0, 1.0, 1.0),
                    &policy) &&
                    policy.termination ==
                        RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_INVALID_MATERIAL);
    material.transparency = NAN;
    assert_true("runtime_caustic_photon_bsdf_nonfinite_material",
                !RuntimeCausticPhotonBsdfPolicy3D_Build(
                    &material,
                    1.0,
                    vec3(1.0, 1.0, 1.0),
                    &policy));
    material = photon_bsdf_material();
    assert_true("runtime_caustic_photon_bsdf_negative_throughput",
                !RuntimeCausticPhotonBsdfPolicy3D_Build(
                    &material,
                    1.0,
                    vec3(1.0, -0.1, 1.0),
                    &policy));
    return 0;
}

static int test_runtime_caustic_photon_bsdf_policy_diffuse_ledger(void) {
    RuntimeMaterialPayload3D material = photon_bsdf_material();
    RuntimeCausticPhotonBsdfPolicy3D policy;
    RuntimeCausticPhotonBsdfSelection3D selection;
    Vec3 parent = vec3(2.0, 1.0, 0.5);

    assert_true("runtime_caustic_photon_bsdf_diffuse_build",
                RuntimeCausticPhotonBsdfPolicy3D_Build(
                    &material, 1.0, parent, &policy));
    assert_true("runtime_caustic_photon_bsdf_diffuse_candidate",
                policy.candidateCount == 1u &&
                    policy.candidates[0].lobe ==
                        RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_DIFFUSE);
    assert_close("runtime_caustic_photon_bsdf_diffuse_pdf",
                 policy.candidates[0].selectionPdf,
                 1.0,
                 1.0e-12);
    assert_close("runtime_caustic_photon_bsdf_diffuse_expected_r",
                 policy.expectedScatteredThroughput.x,
                 1.6,
                 1.0e-12);
    assert_close("runtime_caustic_photon_bsdf_diffuse_absorbed_b",
                 policy.expectedAbsorbedThroughput.z,
                 0.3,
                 1.0e-12);
    assert_true("runtime_caustic_photon_bsdf_diffuse_select",
                RuntimeCausticPhotonBsdfPolicy3D_Select(&policy, 0.5, &selection) &&
                    selection.lobe == RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_DIFFUSE &&
                    selection.termination == RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_NONE);
    assert_close("runtime_caustic_photon_bsdf_diffuse_selected_r",
                 selection.throughputAfter.x,
                 1.6,
                 1.0e-12);
    return 0;
}

static int test_runtime_caustic_photon_bsdf_policy_mixed_fresnel_ledgers(void) {
    RuntimeMaterialPayload3D material = photon_bsdf_material();
    RuntimeCausticPhotonBsdfPolicy3D policy;
    RuntimeCausticPhotonBsdfSelection3D diffuse;
    RuntimeCausticPhotonBsdfSelection3D glossy;
    RuntimeCausticPhotonBsdfSelection3D transmission;
    Vec3 parent = vec3(2.0, 1.0, 0.5);

    material.transparency = 0.4;
    material.bsdf.diffuseWeight = 0.5;
    material.bsdf.specWeight = 0.5;
    material.bsdf.roughness = 0.2;
    assert_true("runtime_caustic_photon_bsdf_mixed_build",
                RuntimeCausticPhotonBsdfPolicy3D_Build(
                    &material, 1.0, parent, &policy));
    assert_true("runtime_caustic_photon_bsdf_mixed_candidates",
                policy.candidateCount == 3u &&
                    policy.candidates[0].lobe == RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_DIFFUSE &&
                    policy.candidates[1].lobe == RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_GLOSSY &&
                    policy.candidates[2].lobe == RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_TRANSMISSION);
    assert_close("runtime_caustic_photon_bsdf_mixed_fresnel",
                 policy.fresnelReflectance,
                 0.04,
                 1.0e-12);
    assert_close("runtime_caustic_photon_bsdf_mixed_diffuse_pdf",
                 policy.candidates[0].selectionPdf,
                 0.3,
                 1.0e-12);
    assert_close("runtime_caustic_photon_bsdf_mixed_glossy_pdf",
                 policy.candidates[1].selectionPdf,
                 0.316,
                 1.0e-12);
    assert_close("runtime_caustic_photon_bsdf_mixed_transmission_pdf",
                 policy.candidates[2].selectionPdf,
                 0.384,
                 1.0e-12);
    assert_close("runtime_caustic_photon_bsdf_mixed_pdf_sum",
                 policy.selectionPdfSum,
                 1.0,
                 1.0e-12);
    assert_close("runtime_caustic_photon_bsdf_mixed_expected_r",
                 policy.expectedScatteredThroughput.x,
                 1.7264,
                 1.0e-12);
    assert_close("runtime_caustic_photon_bsdf_mixed_absorbed_g",
                 policy.expectedAbsorbedThroughput.y,
                 0.2736,
                 1.0e-12);

    assert_true("runtime_caustic_photon_bsdf_mixed_select_diffuse",
                RuntimeCausticPhotonBsdfPolicy3D_Select(&policy, 0.1, &diffuse) &&
                    diffuse.lobe == RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_DIFFUSE);
    assert_true("runtime_caustic_photon_bsdf_mixed_select_glossy",
                RuntimeCausticPhotonBsdfPolicy3D_Select(&policy, 0.4, &glossy) &&
                    glossy.lobe == RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_GLOSSY);
    assert_true("runtime_caustic_photon_bsdf_mixed_select_transmission",
                RuntimeCausticPhotonBsdfPolicy3D_Select(&policy, 0.9, &transmission) &&
                    transmission.lobe == RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_TRANSMISSION);
    assert_close("runtime_caustic_photon_bsdf_mixed_glossy_throughput",
                 glossy.throughputAfter.x,
                 2.0,
                 1.0e-12);
    assert_close("runtime_caustic_photon_bsdf_mixed_transmission_throughput",
                 transmission.throughputAfter.z,
                 0.2,
                 1.0e-12);
    return 0;
}

static int test_runtime_caustic_photon_bsdf_policy_specular_emissive_absorbed(void) {
    RuntimeMaterialPayload3D material = photon_bsdf_material();
    RuntimeCausticPhotonBsdfPolicy3D policy;
    RuntimeCausticPhotonBsdfSelection3D selection;

    material.bsdf.diffuseWeight = 0.0;
    material.bsdf.specWeight = 1.0;
    material.bsdf.roughness = 0.01;
    assert_true("runtime_caustic_photon_bsdf_specular_build",
                RuntimeCausticPhotonBsdfPolicy3D_Build(
                    &material, 1.0, vec3(1.0, 1.0, 1.0), &policy) &&
                    policy.candidates[0].lobe ==
                        RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_SPECULAR);

    material.emissive = 3.0;
    assert_true("runtime_caustic_photon_bsdf_emissive_build",
                RuntimeCausticPhotonBsdfPolicy3D_Build(
                    &material, 1.0, vec3(1.0, 1.0, 1.0), &policy) &&
                    policy.candidateCount == 1u &&
                    policy.candidates[0].lobe ==
                        RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_EMISSIVE);
    assert_true("runtime_caustic_photon_bsdf_emissive_termination",
                RuntimeCausticPhotonBsdfPolicy3D_Select(&policy, 0.0, &selection) &&
                    selection.termination ==
                        RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_EMISSIVE &&
                    selection.throughputAfter.x == 0.0);

    material = photon_bsdf_material();
    material.baseColorR = 0.0;
    material.baseColorG = 0.0;
    material.baseColorB = 0.0;
    assert_true("runtime_caustic_photon_bsdf_black_build",
                RuntimeCausticPhotonBsdfPolicy3D_Build(
                    &material, 1.0, vec3(1.0, 1.0, 1.0), &policy));
    assert_true("runtime_caustic_photon_bsdf_black_absorbed",
                RuntimeCausticPhotonBsdfPolicy3D_Select(&policy, 0.5, &selection) &&
                    selection.termination ==
                        RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_ABSORBED &&
                    selection.throughputAfter.x == 0.0);
    return 0;
}

static int test_clear_dielectric_no_diffuse_and_single_bulk_absorption(void) {
    RuntimeMaterialPayload3D material = photon_bsdf_material();
    RuntimeCausticPhotonBsdfPolicy3D policy;
    RuntimeCausticPhotonBsdfSelection3D selection;
    RuntimeCausticPhotonMediumEntry3D medium;
    Vec3 transmittance;
    material.hasGlassTransportOverride = true;
    material.transparency = 0.97;
    material.absorptionDistance = 2.0;
    material.bsdf.roughness = 0.006;
    assert_true("clear_glass_policy", RuntimeCausticPhotonBsdfPolicy3D_Build(
        &material, 1.0, vec3(1.0, 1.0, 1.0), &policy));
    assert_true("clear_glass_no_diffuse", policy.candidateCount == 2u &&
        policy.candidates[0].lobe == RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_SPECULAR &&
        policy.candidates[1].lobe == RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_TRANSMISSION);
    assert_close("clear_glass_normal_fresnel", policy.candidates[0].rawWeight, 0.04, 1e-12);
    assert_close("clear_glass_authored_transmission", policy.candidates[1].rawWeight, 0.9312, 1e-12);
    assert_close("clear_glass_energy_ledger", policy.expectedScatteredThroughput.x, 0.9712, 1e-12);
    assert_close("clear_glass_interface_loss", policy.expectedAbsorbedThroughput.x, 0.0288, 1e-12);
    assert_true("clear_glass_transmission_select", RuntimeCausticPhotonBsdfPolicy3D_Select(&policy, 0.9, &selection));
    assert_close("clear_glass_no_interface_bulk_tint", selection.throughputAfter.x, 0.9712, 1e-12);
    /* The same tint is still applied over actual medium distance. */
    assert_true("clear_glass_medium", RuntimeCausticPhotonMediumEntry3D_FromMaterial(
        &material, 0, 1.0, &medium));
    assert_true("clear_glass_segment", RuntimeCausticPhotonMediumEntry3D_SegmentTransmittance(
        &medium, 1.0, &transmittance));
    assert_close("clear_glass_beer_lambert", transmittance.x, sqrt(0.8), 1e-12);
    material.hasGlassInterfaceTintOverride = true;
    material.glassInterfaceTintR = 0.5; material.glassInterfaceTintG = 1.0; material.glassInterfaceTintB = 1.0;
    assert_true("clear_glass_explicit_interface_tint", RuntimeCausticPhotonBsdfPolicy3D_Build(
        &material, 1.0, vec3(1,1,1), &policy));
    assert_close("clear_glass_authored_tint_kept", policy.candidates[1].responseTint.x, 0.5, 1e-12);
    material = photon_bsdf_material();
    material.sceneObjectIndex = 0;
    RuntimeWaterMaterial3DOverride water = {0};
    water.ior = 1.333; water.transparency = 0.92; water.roughness = 0.008;
    RuntimeWaterMaterial3D_ClearAll();
    assert_true("clear_water_override", RuntimeWaterMaterial3D_Set(0, &water));
    assert_true("clear_water_payload", RuntimeWaterMaterial3D_ApplyToPayload(0, &material));
    assert_true("clear_water_policy", RuntimeCausticPhotonBsdfPolicy3D_Build(
        &material, 1.0, vec3(1,1,1), &policy));
    assert_true("clear_water_no_inherited_diffuse", policy.candidateCount == 2u &&
        policy.candidates[0].lobe != RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_DIFFUSE);
    assert_close("clear_water_ior_fresnel", policy.fresnelReflectance,
        pow((1.333-1)/(1.333+1), 2), 1e-12);
    assert_close("clear_water_no_bulk_tint_at_interface", policy.candidates[1].responseTint.x, 1.0, 1e-12);
    RuntimeWaterMaterial3D_ClearAll();
    return 0;
}

int run_test_runtime_caustic_photon_bsdf_policy_3d_tests(void) {
    int failures = 0;
    failures += test_clear_dielectric_no_diffuse_and_single_bulk_absorption();
    failures += test_runtime_caustic_photon_bsdf_policy_labels_and_invalid();
    failures += test_runtime_caustic_photon_bsdf_policy_diffuse_ledger();
    failures += test_runtime_caustic_photon_bsdf_policy_mixed_fresnel_ledgers();
    failures += test_runtime_caustic_photon_bsdf_policy_specular_emissive_absorbed();
    return failures;
}
