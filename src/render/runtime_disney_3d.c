#include "render/runtime_disney_3d.h"

#include <math.h>
#include <string.h>

#include "render/runtime_emission_transparency_3d.h"
#include "render/runtime_material_response_3d.h"

static double runtime_disney_3d_clamp(double value,
                                      double min_value,
                                      double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static void runtime_disney_3d_scale_rgb(double input_r,
                                        double input_g,
                                        double input_b,
                                        double scale,
                                        double* out_r,
                                        double* out_g,
                                        double* out_b) {
    if (out_r) *out_r = input_r * scale;
    if (out_g) *out_g = input_g * scale;
    if (out_b) *out_b = input_b * scale;
}

static void runtime_disney_3d_clamp_rgb(double input_r,
                                        double input_g,
                                        double input_b,
                                        double max_value,
                                        double* out_r,
                                        double* out_g,
                                        double* out_b) {
    if (out_r) *out_r = runtime_disney_3d_clamp(input_r, 0.0, max_value);
    if (out_g) *out_g = runtime_disney_3d_clamp(input_g, 0.0, max_value);
    if (out_b) *out_b = runtime_disney_3d_clamp(input_b, 0.0, max_value);
}

static Vec3 runtime_disney_3d_default_view_dir(const HitInfo3D* hit) {
    if (!hit) return vec3(0.0, 0.0, 1.0);
    return vec3_normalize(hit->normal);
}

static bool runtime_disney_3d_resolve_light_dir(const RuntimeScene3D* scene,
                                                const HitInfo3D* hit,
                                                Vec3* out_light_dir) {
    Vec3 to_light = vec3(0.0, 0.0, 0.0);
    double light_distance = 0.0;

    if (!scene || !hit || !out_light_dir || !scene->hasLight) return false;

    to_light = vec3_sub(scene->light.position, hit->position);
    light_distance = vec3_length(to_light);
    if (!(light_distance > 1e-9)) {
        return false;
    }

    *out_light_dir = vec3_scale(to_light, 1.0 / light_distance);
    return true;
}

static void runtime_disney_3d_copy_emission_result(
    const RuntimeEmissionTransparency3DResult* source,
    RuntimeDisney3DResult* out_result) {
    if (!source || !out_result) return;

    out_result->hit = source->hit;
    out_result->visible = source->visible;
    out_result->payloadResolved = source->payloadResolved;
    out_result->primaryRay = source->primaryRay;
    out_result->hitInfo = source->hitInfo;
    out_result->payload = source->payload;
    out_result->directRadiance = source->directRadiance;
    out_result->directRadianceR = source->directRadianceR;
    out_result->directRadianceG = source->directRadianceG;
    out_result->directRadianceB = source->directRadianceB;
    out_result->bounceRadiance = source->bounceRadiance;
    out_result->bounceRadianceR = source->bounceRadianceR;
    out_result->bounceRadianceG = source->bounceRadianceG;
    out_result->bounceRadianceB = source->bounceRadianceB;
    out_result->radiance = source->radiance;
    out_result->radianceR = source->radianceR;
    out_result->radianceG = source->radianceG;
    out_result->radianceB = source->radianceB;
    out_result->secondaryRayCount = source->secondaryRayCount;
    out_result->secondaryHitCount = source->secondaryHitCount;
    out_result->secondaryContributingHitCount = source->secondaryContributingHitCount;
}

static double runtime_disney_3d_resolve_bsdf_signal(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload,
    Vec3 view_dir) {
    Vec3 light_dir = vec3(0.0, 0.0, 0.0);
    double bsdf_eval = 0.0;

    if (!scene || !hit || !payload || !payload->valid) {
        return 1.0;
    }
    if (!runtime_disney_3d_resolve_light_dir(scene, hit, &light_dir)) {
        return 1.0;
    }

    bsdf_eval = MaterialBSDFEvaluateCos3(&payload->bsdf,
                                         hit->normal.x,
                                         hit->normal.y,
                                         hit->normal.z,
                                         light_dir.x,
                                         light_dir.y,
                                         light_dir.z,
                                         view_dir.x,
                                         view_dir.y,
                                         view_dir.z);
    return runtime_disney_3d_clamp(bsdf_eval / 0.25, 0.1, 2.5);
}

static void runtime_disney_3d_resolve_support_components(
    const RuntimeEmissionTransparency3DResult* emission_result,
    double* out_emission_direct,
    double* out_emission_direct_r,
    double* out_emission_direct_g,
    double* out_emission_direct_b,
    double* out_emission_bounce,
    double* out_emission_bounce_r,
    double* out_emission_bounce_g,
    double* out_emission_bounce_b,
    double* out_transmission_direct,
    double* out_transmission_direct_r,
    double* out_transmission_direct_g,
    double* out_transmission_direct_b,
    double* out_transmission_bounce,
    double* out_transmission_bounce_r,
    double* out_transmission_bounce_g,
    double* out_transmission_bounce_b) {
    if (!out_emission_direct || !out_emission_direct_r || !out_emission_direct_g ||
        !out_emission_direct_b || !out_emission_bounce || !out_emission_bounce_r ||
        !out_emission_bounce_g || !out_emission_bounce_b || !out_transmission_direct ||
        !out_transmission_direct_r || !out_transmission_direct_g ||
        !out_transmission_direct_b || !out_transmission_bounce ||
        !out_transmission_bounce_r || !out_transmission_bounce_g ||
        !out_transmission_bounce_b) {
        return;
    }

    *out_emission_direct = 0.0;
    *out_emission_direct_r = 0.0;
    *out_emission_direct_g = 0.0;
    *out_emission_direct_b = 0.0;
    *out_emission_bounce = 0.0;
    *out_emission_bounce_r = 0.0;
    *out_emission_bounce_g = 0.0;
    *out_emission_bounce_b = 0.0;
    *out_transmission_direct = 0.0;
    *out_transmission_direct_r = 0.0;
    *out_transmission_direct_g = 0.0;
    *out_transmission_direct_b = 0.0;
    *out_transmission_bounce = 0.0;
    *out_transmission_bounce_r = 0.0;
    *out_transmission_bounce_g = 0.0;
    *out_transmission_bounce_b = 0.0;
    if (!emission_result) return;

    *out_emission_direct = fmax(0.0, emission_result->emissiveDirectRadiance);
    *out_emission_direct_r = fmax(0.0, emission_result->emissiveDirectRadianceR);
    *out_emission_direct_g = fmax(0.0, emission_result->emissiveDirectRadianceG);
    *out_emission_direct_b = fmax(0.0, emission_result->emissiveDirectRadianceB);
    *out_emission_bounce = fmax(0.0, emission_result->emissiveBounceRadiance);
    *out_emission_bounce_r = fmax(0.0, emission_result->emissiveBounceRadianceR);
    *out_emission_bounce_g = fmax(0.0, emission_result->emissiveBounceRadianceG);
    *out_emission_bounce_b = fmax(0.0, emission_result->emissiveBounceRadianceB);
    *out_transmission_direct = fmax(0.0, emission_result->transmittedDirectRadiance);
    *out_transmission_direct_r = fmax(0.0, emission_result->transmittedDirectRadianceR);
    *out_transmission_direct_g = fmax(0.0, emission_result->transmittedDirectRadianceG);
    *out_transmission_direct_b = fmax(0.0, emission_result->transmittedDirectRadianceB);
    *out_transmission_bounce = fmax(0.0, emission_result->transmittedBounceRadiance);
    *out_transmission_bounce_r = fmax(0.0, emission_result->transmittedBounceRadianceR);
    *out_transmission_bounce_g = fmax(0.0, emission_result->transmittedBounceRadianceG);
    *out_transmission_bounce_b = fmax(0.0, emission_result->transmittedBounceRadianceB);
}

static void runtime_disney_3d_apply_combiner(
    const RuntimeScene3D* scene,
    const RuntimeMaterialResponse3DResult* material_result,
    const RuntimeEmissionTransparency3DResult* emission_result,
    Vec3 view_dir,
    RuntimeDisney3DResult* out_result) {
    double reflectivity = 0.0;
    double albedo = 1.0;
    double diffuse_weight = 1.0;
    double spec_weight = 0.0;
    double roughness = 1.0;
    double diffuse_energy = 1.0;
    double specular_focus = 1.0;
    double diffuse_roughness_weight = 1.0;
    double bsdf_signal = 1.0;
    double emission_direct = 0.0;
    double emission_direct_r = 0.0;
    double emission_direct_g = 0.0;
    double emission_direct_b = 0.0;
    double emission_bounce = 0.0;
    double emission_bounce_r = 0.0;
    double emission_bounce_g = 0.0;
    double emission_bounce_b = 0.0;
    double transmission_direct = 0.0;
    double transmission_direct_r = 0.0;
    double transmission_direct_g = 0.0;
    double transmission_direct_b = 0.0;
    double transmission_bounce = 0.0;
    double transmission_bounce_r = 0.0;
    double transmission_bounce_g = 0.0;
    double transmission_bounce_b = 0.0;
    double direct_limit = 1.0;
    double bounce_limit = 1.0;
    double total_limit = 1.0;
    double view_facing = 1.0;
    double f0 = 0.04;
    double direct_total = 0.0;
    double bounce_total = 0.0;
    double direct_scale = 1.0;
    double diffuse_scale = 1.0;
    double specular_scale = 1.0;
    double direct_total_r = 0.0;
    double direct_total_g = 0.0;
    double direct_total_b = 0.0;
    double bounce_total_r = 0.0;
    double bounce_total_g = 0.0;
    double bounce_total_b = 0.0;

    if (!scene || !material_result || !emission_result || !out_result) return;
    if (!out_result->payloadResolved || !out_result->payload.valid) {
        out_result->baseRadiance = emission_result->radiance;
        out_result->baseRadianceR = emission_result->radianceR;
        out_result->baseRadianceG = emission_result->radianceG;
        out_result->baseRadianceB = emission_result->radianceB;
        out_result->radiance = emission_result->radiance;
        out_result->radianceR = emission_result->radianceR;
        out_result->radianceG = emission_result->radianceG;
        out_result->radianceB = emission_result->radianceB;
        return;
    }

    reflectivity = runtime_disney_3d_clamp(out_result->payload.bsdf.reflectivity, 0.0, 1.0);
    albedo = runtime_disney_3d_clamp(out_result->payload.bsdf.albedo, 0.0, 1.0);
    diffuse_weight = runtime_disney_3d_clamp(out_result->payload.bsdf.diffuseWeight, 0.0, 1.0);
    spec_weight = runtime_disney_3d_clamp(out_result->payload.bsdf.specWeight, 0.0, 1.0);
    roughness = runtime_disney_3d_clamp(out_result->payload.bsdf.roughness, 0.0, 1.0);
    view_facing = runtime_disney_3d_clamp(vec3_dot(out_result->hitInfo.normal,
                                                   vec3_normalize(view_dir)),
                                          0.0,
                                          1.0);
    f0 = runtime_disney_3d_clamp(0.04 + (reflectivity * 0.96), 0.04, 1.0);
    bsdf_signal = runtime_disney_3d_resolve_bsdf_signal(scene,
                                                        &out_result->hitInfo,
                                                        &out_result->payload,
                                                        view_dir);

    out_result->fresnelWeight = FresnelSchlick(view_facing, f0);
    out_result->roughnessWeight =
        runtime_disney_3d_clamp(1.0 - (0.65 * roughness), 0.35, 1.0);
    diffuse_energy =
        runtime_disney_3d_clamp(1.0 - (0.45 * spec_weight) -
                                    (0.35 * out_result->fresnelWeight),
                                0.25,
                                1.0);
    specular_focus =
        runtime_disney_3d_clamp(1.0 + ((1.0 - roughness) * 0.35), 1.0, 1.35);
    diffuse_roughness_weight =
        runtime_disney_3d_clamp(0.65 + (0.35 * roughness), 0.65, 1.0);
    runtime_disney_3d_resolve_support_components(emission_result,
                                                 &emission_direct,
                                                 &emission_direct_r,
                                                 &emission_direct_g,
                                                 &emission_direct_b,
                                                 &emission_bounce,
                                                 &emission_bounce_r,
                                                 &emission_bounce_g,
                                                 &emission_bounce_b,
                                                 &transmission_direct,
                                                 &transmission_direct_r,
                                                 &transmission_direct_g,
                                                 &transmission_direct_b,
                                                 &transmission_bounce,
                                                 &transmission_bounce_r,
                                                 &transmission_bounce_g,
                                                 &transmission_bounce_b);
    direct_scale =
        runtime_disney_3d_clamp((0.45 + (0.55 * albedo)) * (0.75 + (0.25 * diffuse_weight)),
                                0.25,
                                1.15) *
        diffuse_energy *
        diffuse_roughness_weight;
    diffuse_scale =
        runtime_disney_3d_clamp(diffuse_weight * (0.75 + (0.25 * albedo)),
                                0.05,
                                1.15) *
        diffuse_energy *
        diffuse_roughness_weight;
    specular_scale =
        runtime_disney_3d_clamp(spec_weight * (0.6 + reflectivity), 0.0, 1.5) *
        specular_focus *
        out_result->roughnessWeight *
        out_result->fresnelWeight *
        bsdf_signal;

    out_result->baseRadiance =
        material_result->directRadiance * direct_scale;
    runtime_disney_3d_scale_rgb(material_result->directRadianceR,
                                material_result->directRadianceG,
                                material_result->directRadianceB,
                                direct_scale,
                                &out_result->baseRadianceR,
                                &out_result->baseRadianceG,
                                &out_result->baseRadianceB);
    out_result->diffuseRadiance =
        material_result->bounceRadiance * diffuse_scale;
    runtime_disney_3d_scale_rgb(material_result->bounceRadianceR,
                                material_result->bounceRadianceG,
                                material_result->bounceRadianceB,
                                diffuse_scale,
                                &out_result->diffuseRadianceR,
                                &out_result->diffuseRadianceG,
                                &out_result->diffuseRadianceB);
    out_result->specularRadiance =
        material_result->directRadiance * specular_scale;
    runtime_disney_3d_scale_rgb(material_result->directRadianceR,
                                material_result->directRadianceG,
                                material_result->directRadianceB,
                                specular_scale,
                                &out_result->specularRadianceR,
                                &out_result->specularRadianceG,
                                &out_result->specularRadianceB);
    out_result->emissionRadiance = emission_direct + emission_bounce;
    out_result->emissionRadianceR = emission_direct_r + emission_bounce_r;
    out_result->emissionRadianceG = emission_direct_g + emission_bounce_g;
    out_result->emissionRadianceB = emission_direct_b + emission_bounce_b;
    out_result->transmissionRadiance = transmission_direct + transmission_bounce;
    out_result->transmissionRadianceR = transmission_direct_r + transmission_bounce_r;
    out_result->transmissionRadianceG = transmission_direct_g + transmission_bounce_g;
    out_result->transmissionRadianceB = transmission_direct_b + transmission_bounce_b;

    direct_total = out_result->baseRadiance +
                   out_result->specularRadiance +
                   emission_direct +
                   transmission_direct;
    bounce_total = out_result->diffuseRadiance +
                   emission_bounce +
                   transmission_bounce;
    direct_limit = fmax(scene->light.intensity * 1.35,
                        (emission_result->directRadiance * 1.6) + 0.1);
    bounce_limit = fmax(scene->light.intensity * 0.8,
                        (emission_result->bounceRadiance * 1.8) + 0.1);
    total_limit = fmax(scene->light.intensity * 2.0,
                       (emission_result->radiance * 2.25) + 0.1);
    direct_total_r = out_result->baseRadianceR +
                     out_result->specularRadianceR +
                     emission_direct_r +
                     transmission_direct_r;
    direct_total_g = out_result->baseRadianceG +
                     out_result->specularRadianceG +
                     emission_direct_g +
                     transmission_direct_g;
    direct_total_b = out_result->baseRadianceB +
                     out_result->specularRadianceB +
                     emission_direct_b +
                     transmission_direct_b;
    bounce_total_r = out_result->diffuseRadianceR +
                     emission_bounce_r +
                     transmission_bounce_r;
    bounce_total_g = out_result->diffuseRadianceG +
                     emission_bounce_g +
                     transmission_bounce_g;
    bounce_total_b = out_result->diffuseRadianceB +
                     emission_bounce_b +
                     transmission_bounce_b;

    out_result->directRadiance = runtime_disney_3d_clamp(direct_total, 0.0, direct_limit);
    runtime_disney_3d_clamp_rgb(direct_total_r,
                                direct_total_g,
                                direct_total_b,
                                direct_limit,
                                &out_result->directRadianceR,
                                &out_result->directRadianceG,
                                &out_result->directRadianceB);
    out_result->bounceRadiance = runtime_disney_3d_clamp(bounce_total, 0.0, bounce_limit);
    runtime_disney_3d_clamp_rgb(bounce_total_r,
                                bounce_total_g,
                                bounce_total_b,
                                bounce_limit,
                                &out_result->bounceRadianceR,
                                &out_result->bounceRadianceG,
                                &out_result->bounceRadianceB);
    out_result->radiance = runtime_disney_3d_clamp(out_result->directRadiance +
                                                       out_result->bounceRadiance,
                                                   0.0,
                                                   total_limit);
    runtime_disney_3d_clamp_rgb(out_result->directRadianceR + out_result->bounceRadianceR,
                                out_result->directRadianceG + out_result->bounceRadianceG,
                                out_result->directRadianceB + out_result->bounceRadianceB,
                                total_limit,
                                &out_result->radianceR,
                                &out_result->radianceG,
                                &out_result->radianceB);
}

bool RuntimeDisney3D_ShadeHit(const RuntimeScene3D* scene,
                              const HitInfo3D* hit,
                              const RuntimeNative3DSamplingContext* sampling,
                              RuntimeDisney3DResult* out_result) {
    RuntimeDisney3DResult result = {0};
    RuntimeMaterialResponse3DResult material_result = {0};
    RuntimeEmissionTransparency3DResult emission_result = {0};
    Vec3 view_dir = vec3(0.0, 0.0, 0.0);

    if (!scene || !hit || !out_result) return false;
    if (!RuntimeMaterialResponse3D_ShadeHit(scene, hit, sampling, &material_result)) {
        *out_result = result;
        return false;
    }
    if (!RuntimeEmissionTransparency3D_ShadeHit(scene, hit, sampling, &emission_result)) {
        *out_result = result;
        return false;
    }

    runtime_disney_3d_copy_emission_result(&emission_result, &result);
    view_dir = runtime_disney_3d_default_view_dir(hit);
    runtime_disney_3d_apply_combiner(scene,
                                     &material_result,
                                     &emission_result,
                                     view_dir,
                                     &result);
    *out_result = result;
    return true;
}

bool RuntimeDisney3D_ShadePixel(const RuntimeScene3D* scene,
                                const RuntimeCameraProjector3D* projector,
                                double pixel_x,
                                double pixel_y,
                                const RuntimeNative3DSamplingContext* sampling,
                                RuntimeDisney3DResult* out_result) {
    RuntimeDisney3DResult result = {0};
    RuntimeMaterialResponse3DResult material_result = {0};
    RuntimeEmissionTransparency3DResult emission_result = {0};
    Vec3 view_dir = vec3(0.0, 0.0, 0.0);

    if (!scene || !projector || !out_result) return false;
    if (!RuntimeMaterialResponse3D_ShadePixel(scene,
                                              projector,
                                              pixel_x,
                                              pixel_y,
                                              sampling,
                                              &material_result)) {
        result.primaryRay = material_result.primaryRay;
        *out_result = result;
        return false;
    }
    if (!RuntimeEmissionTransparency3D_ShadePixel(scene,
                                                  projector,
                                                  pixel_x,
                                                  pixel_y,
                                                  sampling,
                                                  &emission_result)) {
        memset(&result, 0, sizeof(result));
        result.primaryRay = emission_result.primaryRay;
        *out_result = result;
        return false;
    }

    runtime_disney_3d_copy_emission_result(&emission_result, &result);
    view_dir = vec3_scale(result.primaryRay.direction, -1.0);
    runtime_disney_3d_apply_combiner(scene,
                                     &material_result,
                                     &emission_result,
                                     view_dir,
                                     &result);
    *out_result = result;
    return true;
}
