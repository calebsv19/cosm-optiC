#include "render/runtime_disney_v2_transport_internal_3d.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static RuntimeDisneyV2_3DDominantLobe runtime_disney_v2_transport_3d_choose_lobe(
    const RuntimePrincipledBSDF3D* principled,
    const RuntimeNative3DSamplingContext* sampling,
    const HitInfo3D* hit,
    int depth,
    double* out_diffuse_probability,
    double* out_specular_probability,
    double* out_transmission_probability) {
    double diffuse = 1.0;
    double specular = 0.0;
    double transmission = 0.0;
    double total = 0.0;
    double u = 0.5;
    double v = 0.5;
    uint32_t seed = runtime_disney_v2_transport_3d_seed_from_hit(hit, sampling);

    if (principled && principled->valid) {
        transmission = runtime_disney_v2_transport_3d_clamp01(principled->transmissionWeight);
        diffuse = runtime_disney_v2_transport_3d_clamp01(
            principled->diffuseWeight * (1.0 - transmission));
        specular = runtime_disney_v2_transport_3d_clamp01(principled->specularWeight);
    }
    total = diffuse + specular + transmission;
    if (!(total > 1e-9)) {
        diffuse = 1.0;
        specular = 0.0;
        transmission = 0.0;
        total = 1.0;
    }
    diffuse /= total;
    specular /= total;
    transmission /= total;

    if (out_diffuse_probability) *out_diffuse_probability = diffuse;
    if (out_specular_probability) *out_specular_probability = specular;
    if (out_transmission_probability) *out_transmission_probability = transmission;

    RuntimeNative3DSampling_Stratified2D(sampling,
                                         seed ^ (0x74dcb303U + (uint32_t)(depth * 97)),
                                         1,
                                         0,
                                         (uint32_t)(4096 + depth),
                                         &u,
                                         &v);
    (void)v;
    u = runtime_disney_v2_transport_3d_clamp01(u);
    if (u <= diffuse) return RUNTIME_DISNEY_V2_3D_LOBE_DIFFUSE;
    if (u <= diffuse + specular) return RUNTIME_DISNEY_V2_3D_LOBE_SPECULAR;
    return RUNTIME_DISNEY_V2_3D_LOBE_TRANSMISSION;
}

static Vec3 runtime_disney_v2_transport_3d_sample_diffuse_direction(
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling,
    int depth,
    double* out_pdf,
    double* out_cos_theta) {
    Vec3 tangent = vec3(1.0, 0.0, 0.0);
    Vec3 bitangent = vec3(0.0, 0.0, 1.0);
    Vec3 normal = hit ? hit->normal : vec3(0.0, 1.0, 0.0);
    double u = 0.5;
    double v = 0.5;
    double phi = 0.0;
    double radius = 0.0;
    double local_x = 0.0;
    double local_y = 0.0;
    double local_z = 1.0;
    Vec3 direction = normal;
    uint32_t seed = runtime_disney_v2_transport_3d_seed_from_hit(hit, sampling);

    runtime_disney_v2_transport_3d_build_basis(normal, &tangent, &bitangent);
    RuntimeNative3DSampling_Stratified2D(sampling,
                                         seed ^ (0x2c1b3c6dU + (uint32_t)(depth * 131)),
                                         1,
                                         0,
                                         (uint32_t)(8192 + depth),
                                         &u,
                                         &v);
    phi = 2.0 * M_PI * u;
    radius = sqrt(runtime_disney_v2_transport_3d_clamp01(v));
    local_x = radius * cos(phi);
    local_y = radius * sin(phi);
    local_z = sqrt(fmax(0.0, 1.0 - v));
    direction = vec3_normalize(vec3_add(vec3_add(vec3_scale(tangent, local_x),
                                                vec3_scale(bitangent, local_y)),
                                       vec3_scale(normal, local_z)));
    if (out_cos_theta) {
        *out_cos_theta = runtime_disney_v2_transport_3d_clamp01(vec3_dot(normal, direction));
    }
    if (out_pdf) {
        *out_pdf = fmax((out_cos_theta ? *out_cos_theta : local_z) / M_PI, 1e-9);
    }
    return direction;
}

static Vec3 runtime_disney_v2_transport_3d_sample_specular_direction(
    const HitInfo3D* hit,
    Vec3 incoming_dir,
    const RuntimeNative3DSamplingContext* sampling,
    const RuntimePrincipledBSDF3D* principled,
    int depth,
    double* out_pdf,
    double* out_cos_theta) {
    Vec3 tangent = vec3(1.0, 0.0, 0.0);
    Vec3 bitangent = vec3(0.0, 0.0, 1.0);
    Vec3 normal = hit ? hit->normal : vec3(0.0, 1.0, 0.0);
    Vec3 reflection = runtime_disney_v2_transport_3d_reflect(vec3_normalize(incoming_dir),
                                                             normal);
    Vec3 jitter = reflection;
    double u = 0.5;
    double v = 0.5;
    double phi = 0.0;
    double radius = 0.0;
    double roughness = principled ? principled->roughness : 0.5;
    double blend = 0.0;
    uint32_t seed = runtime_disney_v2_transport_3d_seed_from_hit(hit, sampling);

    runtime_disney_v2_transport_3d_build_basis(normal, &tangent, &bitangent);
    RuntimeNative3DSampling_Stratified2D(sampling,
                                         seed ^ (0xa136aaadU + (uint32_t)(depth * 193)),
                                         1,
                                         0,
                                         (uint32_t)(12288 + depth),
                                         &u,
                                         &v);
    phi = 2.0 * M_PI * u;
    radius = sqrt(runtime_disney_v2_transport_3d_clamp01(v));
    jitter = vec3_normalize(vec3_add(vec3_add(vec3_scale(tangent, radius * cos(phi)),
                                             vec3_scale(bitangent, radius * sin(phi))),
                                    vec3_scale(reflection, 1.0)));
    blend = runtime_disney_v2_transport_3d_clamp(roughness * 0.35, 0.0, 0.35);
    reflection = vec3_normalize(vec3_add(vec3_scale(reflection, 1.0 - blend),
                                         vec3_scale(jitter, blend)));
    if (vec3_dot(reflection, normal) <= 1e-6) {
        reflection = normal;
    }
    if (out_cos_theta) {
        *out_cos_theta = runtime_disney_v2_transport_3d_clamp01(vec3_dot(normal, reflection));
    }
    if (out_pdf) {
        *out_pdf = fmax(RuntimePrincipledBSDF3D_GGXHalfVectorPdf(
                            principled,
                            sqrt(runtime_disney_v2_transport_3d_clamp01(
                                out_cos_theta ? *out_cos_theta : vec3_dot(normal, reflection))),
                            runtime_disney_v2_transport_3d_clamp01(
                                fabs(vec3_dot(reflection, vec3_scale(vec3_normalize(incoming_dir),
                                                                     -1.0))))),
                        1e-6);
    }
    return reflection;
}

bool runtime_disney_v2_transport_3d_sample_vertex(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload,
    const RuntimePrincipledBSDF3D* principled,
    const RuntimeNative3DSamplingContext* sampling,
    Vec3 incoming_dir,
    int depth,
    bool has_emissive_area_direct,
    const RuntimeEmissiveDirect3DResult* emissive_area_direct,
    RuntimeDisneyV2Transport3DVertexSample* out_sample) {
    RuntimeDisneyV2Transport3DVertexSample sample = {0};
    RuntimeDisneyV2_3DTransmissionSample transmission_sample = {0};
    Vec3 normal = hit ? hit->normal : vec3(0.0, 1.0, 0.0);
    Vec3 light_dir = normal;
    double light_distance = 0.0;
    double diffuse_probability = 1.0;
    double specular_probability = 0.0;
    double transmission_probability = 0.0;
    double finite_direct_bsdf_pdf = 0.0;
    double area_direct_bsdf_pdf = 0.0;

    if (!hit || !out_sample) return false;
    memset(out_sample, 0, sizeof(*out_sample));

    sample.lobe = runtime_disney_v2_transport_3d_choose_lobe(principled,
                                                             sampling,
                                                             hit,
                                                             depth,
                                                             &diffuse_probability,
                                                             &specular_probability,
                                                             &transmission_probability);
    sample.policyLobe = runtime_disney_v2_transport_3d_policy_lobe(sample.lobe);
    if (scene && scene->hasLight) {
        light_dir = vec3_sub(scene->light.position, hit->position);
        light_distance = vec3_length(light_dir);
        if (light_distance > 1e-9) {
            light_dir = vec3_scale(light_dir, 1.0 / light_distance);
        } else {
            light_dir = normal;
        }
    }

    if (sample.lobe == RUNTIME_DISNEY_V2_3D_LOBE_TRANSMISSION) {
        Vec3 view_dir = vec3_scale(vec3_normalize(incoming_dir), -1.0);
        if (RuntimeDisneyV2_3D_SampleTransmission(payload,
                                                  principled,
                                                  hit,
                                                  view_dir,
                                                  transmission_probability,
                                                  &transmission_sample)) {
            sample.direction = transmission_sample.direction;
            sample.pdf = transmission_sample.pdf;
            sample.cosTheta = 1.0;
            sample.throughputR = transmission_sample.throughputR;
            sample.throughputG = transmission_sample.throughputG;
            sample.throughputB = transmission_sample.throughputB;
        } else {
            sample.direction = light_dir;
            sample.cosTheta = runtime_disney_v2_transport_3d_clamp01(
                fabs(vec3_dot(normal, sample.direction)));
            sample.pdf = fmax(transmission_probability, 1e-6);
            sample.throughputR = (principled ? principled->baseColorR : 1.0) *
                                 fmax(transmission_probability, 1e-6);
            sample.throughputG = (principled ? principled->baseColorG : 1.0) *
                                 fmax(transmission_probability, 1e-6);
            sample.throughputB = (principled ? principled->baseColorB : 1.0) *
                                 fmax(transmission_probability, 1e-6);
        }
    } else if (sample.lobe == RUNTIME_DISNEY_V2_3D_LOBE_SPECULAR) {
        sample.direction = runtime_disney_v2_transport_3d_sample_specular_direction(hit,
                                                                                    incoming_dir,
                                                                                    sampling,
                                                                                    principled,
                                                                                    depth,
                                                                                    &sample.pdf,
                                                                                    &sample.cosTheta);
        if (scene && scene->hasLight && fabs(vec3_dot(light_dir, normal)) > 1e-6) {
            sample.direction = light_dir;
            sample.cosTheta =
                runtime_disney_v2_transport_3d_clamp01(fabs(vec3_dot(normal, sample.direction)));
        }
        sample.pdf = fmax(sample.pdf, 1e-6);
        sample.throughputR = runtime_disney_v2_transport_3d_clamp(
            (principled ? principled->specularF0R : 0.04) *
                fmax(specular_probability, 1e-6) *
                fmax(sample.cosTheta, 0.15) / sample.pdf,
            0.0,
            2.0);
        sample.throughputG = runtime_disney_v2_transport_3d_clamp(
            (principled ? principled->specularF0G : 0.04) *
                fmax(specular_probability, 1e-6) *
                fmax(sample.cosTheta, 0.15) / sample.pdf,
            0.0,
            2.0);
        sample.throughputB = runtime_disney_v2_transport_3d_clamp(
            (principled ? principled->specularF0B : 0.04) *
                fmax(specular_probability, 1e-6) *
                fmax(sample.cosTheta, 0.15) / sample.pdf,
            0.0,
            2.0);
    } else {
        sample.lobe = RUNTIME_DISNEY_V2_3D_LOBE_DIFFUSE;
        sample.policyLobe = RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_DIFFUSE;
        sample.direction = runtime_disney_v2_transport_3d_sample_diffuse_direction(hit,
                                                                                   sampling,
                                                                                   depth,
                                                                                   &sample.pdf,
                                                                                   &sample.cosTheta);
        if (scene && scene->hasLight && fabs(vec3_dot(light_dir, normal)) > 1e-6) {
            sample.direction = light_dir;
            sample.cosTheta =
                runtime_disney_v2_transport_3d_clamp01(fabs(vec3_dot(normal, sample.direction)));
            sample.pdf = fmax(sample.cosTheta / M_PI, 1e-9);
        }
        sample.throughputR = runtime_disney_v2_transport_3d_clamp(
            (principled ? principled->baseColorR : 1.0) *
                fmax(diffuse_probability, 1e-6) * sample.cosTheta /
                fmax(sample.pdf * M_PI, 1e-6),
            0.0,
            2.0);
        sample.throughputG = runtime_disney_v2_transport_3d_clamp(
            (principled ? principled->baseColorG : 1.0) *
                fmax(diffuse_probability, 1e-6) * sample.cosTheta /
                fmax(sample.pdf * M_PI, 1e-6),
            0.0,
            2.0);
        sample.throughputB = runtime_disney_v2_transport_3d_clamp(
            (principled ? principled->baseColorB : 1.0) *
                fmax(diffuse_probability, 1e-6) * sample.cosTheta /
                fmax(sample.pdf * M_PI, 1e-6),
            0.0,
            2.0);
    }

    if (scene && scene->hasLight) {
        finite_direct_bsdf_pdf =
            RuntimeDisneyV2_3D_EstimateDirectBsdfPdf(principled,
                                                     hit,
                                                     incoming_dir,
                                                     light_dir,
                                                     transmission_probability > 1e-9);
    }
    if (has_emissive_area_direct &&
        emissive_area_direct &&
        emissive_area_direct->lightPdf > 0.0 &&
        vec3_length(emissive_area_direct->sampleDirection) > 1e-9) {
        area_direct_bsdf_pdf =
            RuntimeDisneyV2_3D_EstimateDirectBsdfPdf(principled,
                                                     hit,
                                                     incoming_dir,
                                                     emissive_area_direct->sampleDirection,
                                                     transmission_probability > 1e-9);
    }
    sample.finiteLightMis =
        runtime_disney_v2_transport_3d_make_mis_branch(
            RuntimeDisneyV2_3D_EstimateFiniteLightPdfForHit(scene, hit),
            finite_direct_bsdf_pdf);
    sample.emissiveAreaMis =
        runtime_disney_v2_transport_3d_make_mis_branch(
            has_emissive_area_direct && emissive_area_direct
                ? emissive_area_direct->lightPdf
                : 0.0,
            area_direct_bsdf_pdf);
    sample.lightPdf =
        RuntimeDisneyV2_3D_EstimateDirectLightPdfForHitWithAreaPdf(
            scene,
            hit,
            has_emissive_area_direct,
            emissive_area_direct ? emissive_area_direct->lightPdf : 0.0);
    sample.directBsdfPdf = fmax(finite_direct_bsdf_pdf, area_direct_bsdf_pdf);
    runtime_disney_v2_transport_3d_balance_mis(sample.lightPdf,
                                               sample.directBsdfPdf,
                                               &sample.misWeightLight,
                                               &sample.misWeightBsdf);
    *out_sample = sample;
    return sample.pdf > 1e-9 &&
           runtime_disney_v2_transport_3d_peak(sample.throughputR,
                                               sample.throughputG,
                                               sample.throughputB) > 1e-12 &&
           vec3_length(sample.direction) > 0.0;
}
