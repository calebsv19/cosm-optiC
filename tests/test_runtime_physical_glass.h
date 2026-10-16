#pragma once
#include "test_runtime_glass_microsurface.h"
#include "render/runtime_physical_glass_3d.h"
#include "test_runtime_path_termination.h"
#include "test_runtime_glass_path_termination.h"
static void test_runtime_physical_glass(void) {
    test_runtime_path_termination();
    test_runtime_glass_path_termination();
    test_runtime_glass_microsurface();
    assert_close("physical_fresnel_normal", RuntimePhysicalGlass3D_Fresnel(1, 1, 1.5), .04, 1e-14);
    assert_close("physical_fresnel_tir", RuntimePhysicalGlass3D_Fresnel(.5, 1.5, 1), 1, 1e-14);
    RuntimeMaterialPayload3D p = {0};
    p.valid = p.hasGlassTransportOverride = true;
    p.opticalIor = 1;
    p.transparency = 1;
    p.bsdf.reflectivity = .8; /* physical interface never takes an artistic floor */
    const double rough[] = {0, .0001, .08, .3, 1};
    for (int r = 0; r < 5; ++r)
        for (int a = 0; a < 3; ++a) {
            double c = a == 0 ? 1 : a == 1 ? .5 : .001;
            Vec3 incoming = vec3(sqrt(1 - c * c), 0, -c);
            p.bsdf.roughness = rough[r];
            RuntimePhysicalGlass3DSample s;
            assert_true("physical_equal_media_sample",
                        RuntimePhysicalGlass3D_Sample(&p, vec3(0, 0, 1), incoming, 1.52, 1.52, .31,
                                                      .72, .4, &s));
            assert_true("physical_equal_media_delta", s.delta && !s.reflected);
            assert_close("physical_equal_media_direction",
                         vec3_length(vec3_sub(s.direction, incoming)), 0, 1e-12);
            assert_close("physical_equal_media_energy", s.weight, 1, 1e-12);
            assert_close("physical_equal_media_fresnel", s.interface.fresnel, 0, 1e-12);
        }
    /* White-furnace energy in the radiance invariant L/eta^2. Invalid masked
       microfacets contribute zero; they must not be resampled/renormalized. */
    for (int r = 0; r < 5; ++r) {
        p.bsdf.roughness = rough[r];
        double energy = 0;
        unsigned state = 0x632be59b;
        for (int i = 0; i < 20000; ++i) {
            double random[3];
            for (int j = 0; j < 3; ++j) {
                state = 1664525u * state + 1013904223u;
                random[j] = (state + .5) / 4294967296.;
            }
            RuntimePhysicalGlass3DSample s;
            if (RuntimePhysicalGlass3D_SampleSingleScatter(&p, vec3(0, 0, 1), vec3(.6, 0, -.8), 1, 1.5,
                                              random[0], random[1], random[2], &s)) {
                assert_true("physical_sample_finite_pdf", isfinite(s.pdf) && s.pdf > 0);
                if (rough[r] >= .08) {
                    double fc, pdf;
                    assert_true("physical_evaluate",
                                RuntimePhysicalGlass3D_EvaluateSingleScatter(&p, vec3(0, 0, 1), vec3(.6, 0, -.8),
                                                                s.direction, 1, 1.5, &fc, &pdf));
                    assert_close("physical_sample_pdf_consistent", s.pdf / pdf, 1, 1e-7);
                    assert_close("physical_sample_energy_consistent", s.weight * pdf, fc,
                                 fmax(1e-8, fc * 1e-7));
                }
                energy += s.weight * (s.reflected ? 1 : 2.25);
            }
        }
        energy /= 20000;
        assert_true("physical_furnace_no_gain", energy <= 1.001 && energy > .4);
    }
    p.bsdf.roughness = 0;
    RuntimePhysicalGlass3DSample snell, tir;
    assert_true("physical_snell_sample",
                RuntimePhysicalGlass3D_Sample(&p, vec3(0, 0, 1), vec3(.6, 0, -.8), 1, 1.5, .2, .3,
                                              .9, &snell));
    assert_close("physical_snell_tangent", snell.direction.x, .4, 1e-12);
    assert_close("physical_snell_normal", snell.direction.z, -sqrt(.84), 1e-12);
    assert_true("physical_tir_sample",
                RuntimePhysicalGlass3D_Sample(&p, vec3(0, 0, 1), vec3(.8, 0, -.6), 1.5, 1, .2, .3,
                                              .99, &tir));
    assert_true("physical_tir_reflected", tir.reflected && tir.interface.totalInternalReflection);
    assert_close("physical_tir_energy", tir.weight, 1, 1e-12);
    /* The null-interface limit also holds away from air, while the boundary
       remains in the stack and its volume absorption still applies. */
    RuntimeCameraMedium3D nested;
    RuntimeCameraMedium3D_Init(&nested);
    RuntimeMaterialPayload3D outer = p;
    outer.opticalIor = outer.bsdf.ior = 1.52;
    outer.absorptionDistance = 2;
    outer.materialId = MATERIAL_PRESET_TRANSPARENT;
    outer.hasGlassAbsorptionColorOverride = true;
    outer.glassAbsorptionColorR = exp(-1);
    outer.glassAbsorptionColorG = outer.glassAbsorptionColorB = 1;
    HitInfo3D boundary = {0};
    boundary.normal = vec3(0, 0, 1);
    boundary.sceneObjectIndex = 2;
    RuntimeDielectricTransport3D crossing;
    assert_true(
        "physical_nested_enter",
        RuntimeCameraDielectric3D_Resolve(&nested, &outer, &boundary, vec3(0, 0, -1), &crossing));
    assert_true("physical_nested_push",
                RuntimeCameraMedium3D_Transmit(&nested, &outer, &boundary, &crossing));
    boundary.sceneObjectIndex = 3;
    assert_true(
        "physical_nested_equal_resolve",
        RuntimeCameraDielectric3D_Resolve(&nested, &outer, &boundary, vec3(0, 0, -1), &crossing));
    assert_close("physical_nested_equal_fresnel", crossing.fresnel, 0, 1e-12);
    assert_true("physical_nested_equal_push",
                RuntimeCameraMedium3D_Transmit(&nested, &outer, &boundary, &crossing));
    assert_true("physical_nested_boundary_retained", nested.count == 3);
    Vec3 nested_attenuation;
    assert_true("physical_nested_absorption",
                RuntimeCausticPhotonMediumEntry3D_SegmentTransmittance(
                    RuntimeCausticPhotonMediumStack3D_Top(&nested), 2, &nested_attenuation));
    assert_close("physical_nested_absorption_retained", nested_attenuation.x, exp(-1), 1e-12);
    assert_true(
        "physical_nested_equal_exit",
        RuntimeCameraDielectric3D_Resolve(&nested, &outer, &boundary, vec3(0, 0, 1), &crossing));
    assert_true("physical_nested_equal_pop",
                RuntimeCameraMedium3D_Transmit(&nested, &outer, &boundary, &crossing));
    assert_true("physical_nested_stack_restored", nested.count == 2);
    RuntimeCameraMedium3D medium;
    RuntimeCameraMedium3D_Init(&medium);
    RuntimeCausticPhotonMediumEntry3D entry = {0};
    p.materialId = MATERIAL_PRESET_TRANSPARENT;
    p.sceneObjectIndex = 0;
    p.absorptionDistance = 2;
    p.hasGlassAbsorptionColorOverride = true;
    p.glassAbsorptionColorR = exp(-1);
    p.glassAbsorptionColorG = 1;
    p.glassAbsorptionColorB = 1;
    assert_true("physical_absorbing_entry",
                RuntimeCausticPhotonMediumEntry3D_FromMaterial(&p, 0, 0, &entry));
    Vec3 attenuation;
    assert_true("physical_segment",
                RuntimeCausticPhotonMediumEntry3D_SegmentTransmittance(&entry, 2, &attenuation));
    assert_close("physical_beer_lambert", attenuation.x, exp(-1), 1e-12);

    /* A floor reflection offset can begin just inside a touching solid after
       the camera segment already entered and exited it. The new segment must
       classify its origin, while duplicate exits inside a segment still fail. */
    for (int matched = 0; matched < 2; ++matched) {
        RuntimeCameraMedium3D segment;
        RuntimeCameraMedium3D_Init(&segment);
        RuntimeMaterialPayload3D solid = outer;
        solid.opticalIor = solid.bsdf.ior = matched ? 1 : 1.5;
        boundary.sceneObjectIndex = 7;
        assert_true("segment_initial_entry", RuntimeCameraDielectric3D_Resolve(
            &segment, &solid, &boundary, vec3(0,0,-1), &crossing));
        assert_true("segment_initial_push", RuntimeCameraMedium3D_Transmit(
            &segment, &solid, &boundary, &crossing));
        assert_true("segment_initial_exit", RuntimeCameraDielectric3D_Resolve(
            &segment, &solid, &boundary, vec3(0,0,1), &crossing));
        assert_true("segment_initial_pop", RuntimeCameraMedium3D_Transmit(
            &segment, &solid, &boundary, &crossing));
        assert_true("segment_duplicate_exit_refused", !RuntimeCameraDielectric3D_Resolve(
            &segment, &solid, &boundary, vec3(0,0,1), &crossing));
        assert_true("segment_offset_origin_classified", RuntimeCameraDielectric3D_ResolveSegmentStart(
            &segment, &solid, &boundary, vec3(0,0,1), &crossing));
        assert_close("segment_incident_medium", crossing.etaFrom, solid.opticalIor, 1e-12);
        assert_close("segment_destination_medium", crossing.etaTo, 1, 1e-12);
        assert_true("segment_history_preserved", segment.pushCount==2 && segment.popCount==1);
        Vec3 attenuation;
        assert_true("segment_absorption", RuntimeCausticPhotonMediumEntry3D_SegmentTransmittance(
            RuntimeCausticPhotonMediumStack3D_Top(&segment), 2, &attenuation));
        assert_close("segment_absorption_retained", attenuation.x, exp(-1), 1e-12);
        assert_true("segment_offset_exit_pop", RuntimeCameraMedium3D_Transmit(
            &segment, &solid, &boundary, &crossing));
        assert_true("segment_balanced_history", segment.count==1 && segment.pushCount==2 && segment.popCount==2);
        assert_true("segment_second_duplicate_refused", !RuntimeCameraDielectric3D_Resolve(
            &segment, &solid, &boundary, vec3(0,0,1), &crossing));
    }
    boundary.sceneObjectIndex=99;
    assert_true("segment_known_nested_exit_refused", !RuntimeCameraDielectric3D_ResolveSegmentStart(
        &nested, &outer, &boundary, vec3(0,0,1), &crossing));
}
