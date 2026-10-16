#pragma once
#include "render/runtime_physical_glass_3d.h"
/* Two-sided energy and angular checks. Angular references are independent
 * 200k-sample runs of the authors' 2016 Smith dielectric tutorial, GGX,
 * uniform heights; tolerances include Monte Carlo error of both estimators. */
static void test_runtime_glass_microsurface(void) {
    RuntimeMaterialPayload3D p = {0};
    p.valid = p.hasGlassTransportOverride = true;
    p.transparency = 1;
    const double roughness[] = {.08, .2, .4, .7, 1};
    const double cosine[] = {1, .5, .1};
    unsigned rng = 901;
    for (int side = 0; side < 2; ++side)
        for (int r = 0; r < 5; ++r)
            for (int a = 0; a < 3; ++a) {
                double sum = 0, from = side ? 1.5 : 1, to = side ? 1 : 1.5;
                p.bsdf.roughness = roughness[r];
                Vec3 incoming = vec3(sqrt(1 - cosine[a] * cosine[a]), 0, -cosine[a]);
                for (int i = 0; i < 10000; ++i) {
                    double u[3];
                    for (int k = 0; k < 3; ++k) {
                        rng = 1664525u * rng + 1013904223u;
                        u[k] = (rng + .5) / 4294967296.;
                    }
                    RuntimePhysicalGlass3DSample s;
                    if (!RuntimePhysicalGlass3D_Sample(&p, vec3(0, 0, 1), incoming,
                                                       from, to, u[0], u[1], u[2], &s)) continue;
                    assert_true("microsurface_no_fake_pdf", !s.hasMarginalPdf && s.pdf == 0);
                    assert_true("microsurface_exit_side", s.reflected == (s.direction.z > 0));
                    assert_close("microsurface_unit_direction", vec3_length(s.direction), 1, 1e-12);
                    assert_true("microsurface_has_scattering", s.scatteringOrder > 0);
                    sum += s.weight * (s.reflected ? 1 : (to * to) / (from * from));
                }
                assert_close("microsurface_two_sided_energy", sum / 10000, 1, .003);
            }
    const double reference[][6] = {
        {0.20,1.00,1.500000000,0.040715000,-0.918645339,0.997743809},
        {0.20,1.00,0.666666687,0.042790000,-0.915199239,0.994876211},
        {0.20,0.50,1.500000000,0.090100000,-0.695203049,0.623739509},
        {0.20,0.50,0.666666687,0.995585000,0.499582643,0.260286865},
        {0.20,0.10,1.500000000,0.485165000,-0.292067808,0.270369881},
        {0.20,0.10,0.666666687,0.996540000,0.179860775,0.056559000},
        {0.70,1.00,1.500000000,0.030150000,-0.917159140,0.928252587},
        {0.70,1.00,0.666666687,0.230115000,-0.579157326,0.786958459},
        {0.70,0.50,1.500000000,0.049120000,-0.622678105,0.483016049},
        {0.70,0.50,0.666666687,0.760005000,0.344113707,0.353667205},
        {0.70,0.10,1.500000000,0.108580000,-0.454155598,0.359629537},
        {0.70,0.10,0.666666687,0.973800000,0.540795884,0.382427185},
        {1.00,1.00,1.500000000,0.015345000,-0.903740241,0.863106824},
        {1.00,1.00,0.666666687,0.407375000,-0.224026744,0.624118116},
        {1.00,0.50,1.500000000,0.027945000,-0.623062834,0.461273416},
        {1.00,0.50,0.666666687,0.657745000,0.223181346,0.374429870},
        {1.00,0.10,1.500000000,0.069645000,-0.611325984,0.512345576},
        {1.00,0.10,0.666666687,0.981945000,0.473459123,0.319274152}
    };
    for (unsigned k = 0; k < sizeof(reference)/sizeof(reference[0]); ++k) {
        const double *ref = reference[k];
        p.bsdf.roughness = ref[0];
        double reflection = 0, z = 0, z2 = 0;
        for (int i = 0; i < 40000; ++i) {
            double u[3];
            for (int j = 0; j < 3; ++j) {
                rng = 1664525u * rng + 1013904223u;
                u[j] = (rng + .5) / 4294967296.;
            }
            RuntimePhysicalGlass3DSample sample;
            assert_true("microsurface_reference_valid", RuntimePhysicalGlass3D_Sample(
                &p, vec3(0,0,1), vec3(-sqrt(1-ref[1]*ref[1]),0,-ref[1]),
                1,ref[2],u[0],u[1],u[2],&sample));
            reflection += sample.reflected;
            z += sample.direction.z;
            z2 += sample.direction.z * sample.direction.z;
        }
        assert_close("microsurface_reference_reflection",reflection/40000,ref[3],.012);
        assert_close("microsurface_reference_mean_z",z/40000,ref[4],.012);
        assert_close("microsurface_reference_mean_z2",z2/40000,ref[5],.012);
    }
    /* Equal radiance/eta^2 on both sides is a stationary angular field.
     * This rejects an energy-conserving sampler with the wrong distribution. */
    for (int r = 0; r < 3; ++r) {
        p.bsdf.roughness = r == 0 ? .2 : r == 1 ? .7 : 1;
        double flux[2] = {0}, moment[2] = {0};
        for (int side = 0; side < 2; ++side)
            for (int i = 0; i < 50000; ++i) {
                double u[5];
                for (int j = 0; j < 5; ++j) {
                    rng = 1664525u * rng + 1013904223u;
                    u[j] = (rng + .5) / 4294967296.;
                }
                double c = sqrt(u[0]), phi = 6.283185307179586 * u[1];
                Vec3 incoming = vec3(sqrt(1-c*c)*cos(phi),sqrt(1-c*c)*sin(phi),-c);
                RuntimePhysicalGlass3DSample sample;
                assert_true("microsurface_equilibrium_valid", RuntimePhysicalGlass3D_Sample(
                    &p,vec3(0,0,1),incoming,side?1.5:1,side?1:1.5,u[2],u[3],u[4],&sample));
                int destination = sample.reflected ? side : 1-side;
                double weight = side ? 2.25 : 1;
                flux[destination] += weight;
                moment[destination] += weight*fabs(sample.direction.z);
            }
        assert_close("microsurface_equilibrium_air",flux[0]/50000,1,.02);
        assert_close("microsurface_equilibrium_glass",flux[1]/50000,2.25,.02);
        for (int side = 0; side < 2; ++side)
            assert_close("microsurface_equilibrium_angle",moment[side]/flux[side],2./3,.01);
    }
    p.bsdf.roughness = 1;
    RuntimePhysicalGlass3DSample a, b;
    assert_true("microsurface_repeat_a", RuntimePhysicalGlass3D_Sample(
        &p, vec3(0,0,1), vec3(.6,0,-.8), 1, 1.5, .2,.3,.4,&a));
    assert_true("microsurface_repeat_b", RuntimePhysicalGlass3D_Sample(
        &p, vec3(0,0,1), vec3(.6,0,-.8), 1, 1.5, .2,.3,.4,&b));
    assert_close("microsurface_deterministic", vec3_length(vec3_sub(a.direction,b.direction)),0,0);
    assert_true("microsurface_nan_refusal", !RuntimePhysicalGlass3D_Sample(
        &p, vec3(0,0,1), vec3(0,0,-1), 1, 1.5, NAN,.3,.4,&a));
}
