#include "render/runtime_caustic_photon_bsdf_policy_3d.h"

#include <math.h>
#include <string.h>

static double photon_bsdf_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static Vec3 photon_bsdf_clamp_vec01(Vec3 value) {
    return vec3(photon_bsdf_clamp01(value.x),
                photon_bsdf_clamp01(value.y),
                photon_bsdf_clamp01(value.z));
}

static Vec3 photon_bsdf_mul(Vec3 a, Vec3 b) {
    return vec3(a.x * b.x, a.y * b.y, a.z * b.z);
}

static Vec3 photon_bsdf_sub_nonnegative(Vec3 a, Vec3 b) {
    return vec3(fmax(0.0, a.x - b.x),
                fmax(0.0, a.y - b.y),
                fmax(0.0, a.z - b.z));
}

static double photon_bsdf_peak(Vec3 value) {
    return fmax(value.x, fmax(value.y, value.z));
}

static bool photon_bsdf_valid_input(const RuntimeMaterialPayload3D* material,
                                    double incident_cosine,
                                    Vec3 parent_throughput) {
    if (!material || !material->valid || !isfinite(incident_cosine) ||
        !isfinite(parent_throughput.x) || !isfinite(parent_throughput.y) ||
        !isfinite(parent_throughput.z) || parent_throughput.x < 0.0 ||
        parent_throughput.y < 0.0 || parent_throughput.z < 0.0) {
        return false;
    }
    return isfinite(material->baseColorR) && isfinite(material->baseColorG) &&
           isfinite(material->baseColorB) && isfinite(material->emissive) &&
           isfinite(material->transparency) && isfinite(material->opticalIor) &&
           isfinite(material->bsdf.emissive) &&
           isfinite(material->bsdf.diffuseWeight) &&
           isfinite(material->bsdf.specWeight) &&
           isfinite(material->bsdf.reflectivity) &&
           isfinite(material->bsdf.roughness) && isfinite(material->bsdf.ior);
}

static double photon_bsdf_fresnel(double incident_cosine,
                                  double optical_ior,
                                  double authored_reflectivity) {
    double ior = fmin(fmax(optical_ior, 1.0), 4.0);
    double ratio = (ior - 1.0) / (ior + 1.0);
    double f0 = fmax(ratio * ratio, photon_bsdf_clamp01(authored_reflectivity));
    double one_minus_cosine = 1.0 - photon_bsdf_clamp01(fabs(incident_cosine));
    double schlick = one_minus_cosine * one_minus_cosine;
    schlick *= schlick * one_minus_cosine;
    return photon_bsdf_clamp01(f0 + (1.0 - f0) * schlick);
}

static bool photon_bsdf_append(RuntimeCausticPhotonBsdfPolicy3D* policy,
                               RuntimeCausticPhotonBsdfLobe3D lobe,
                               double raw_weight,
                               Vec3 response_tint,
                               bool terminal) {
    RuntimeCausticPhotonBsdfCandidate3D* candidate = NULL;
    if (!policy || raw_weight <= 1.0e-12 ||
        policy->candidateCount >= RUNTIME_CAUSTIC_PHOTON_BSDF_MAX_CANDIDATES) {
        return raw_weight <= 1.0e-12;
    }
    candidate = &policy->candidates[policy->candidateCount++];
    memset(candidate, 0, sizeof(*candidate));
    candidate->lobe = lobe;
    candidate->rawWeight = raw_weight;
    candidate->responseTint = photon_bsdf_clamp_vec01(response_tint);
    candidate->terminal = terminal;
    policy->rawWeightSum += raw_weight;
    return true;
}

const char* RuntimeCausticPhotonBsdfLobe3D_Label(
    RuntimeCausticPhotonBsdfLobe3D lobe) {
    switch (lobe) {
        case RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_DIFFUSE: return "diffuse";
        case RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_GLOSSY: return "glossy";
        case RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_SPECULAR: return "specular";
        case RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_TRANSMISSION: return "transmission";
        case RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_EMISSIVE: return "emissive";
        case RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_NONE:
        default: return "none";
    }
}

const char* RuntimeCausticPhotonBsdfTermination3D_Label(
    RuntimeCausticPhotonBsdfTermination3D termination) {
    switch (termination) {
        case RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_INVALID_MATERIAL:
            return "invalid_material";
        case RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_EMISSIVE: return "emissive";
        case RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_ABSORBED: return "absorbed";
        case RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_NONE:
        default: return "none";
    }
}

bool RuntimeCausticPhotonBsdfPolicy3D_Build(
    const RuntimeMaterialPayload3D* material,
    double incident_cosine,
    Vec3 parent_throughput,
    RuntimeCausticPhotonBsdfPolicy3D* out_policy) {
    RuntimeCausticPhotonBsdfPolicy3D policy;
    Vec3 base_tint;
    Vec3 white = vec3(1.0, 1.0, 1.0);
    double transparency;
    double surface_weight;
    double diffuse_weight;
    double specular_weight;
    double bsdf_weight_sum;
    double reflection_weight;
    double transmission_weight;
    double emissive;

    if (!out_policy) return false;
    memset(&policy, 0, sizeof(policy));
    policy.parentThroughput = parent_throughput;
    policy.incidentCosine = photon_bsdf_clamp01(fabs(incident_cosine));
    if (!photon_bsdf_valid_input(material, incident_cosine, parent_throughput)) {
        policy.termination = RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_INVALID_MATERIAL;
        *out_policy = policy;
        return false;
    }

    emissive = fmax(material->emissive, material->bsdf.emissive);
    if (emissive > 1.0e-12) {
        photon_bsdf_append(&policy,
                           RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_EMISSIVE,
                           1.0,
                           vec3(0.0, 0.0, 0.0),
                           true);
        policy.termination = RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_EMISSIVE;
    } else {
        transparency = photon_bsdf_clamp01(material->transparency);
        surface_weight = 1.0 - transparency;
        diffuse_weight = photon_bsdf_clamp01(material->bsdf.diffuseWeight);
        specular_weight = photon_bsdf_clamp01(material->bsdf.specWeight);
        bsdf_weight_sum = diffuse_weight + specular_weight;
        if (bsdf_weight_sum <= 1.0e-12) {
            diffuse_weight = 1.0;
            specular_weight = 0.0;
            bsdf_weight_sum = 1.0;
        }
        diffuse_weight = surface_weight * diffuse_weight / bsdf_weight_sum;
        specular_weight = surface_weight * specular_weight / bsdf_weight_sum;
        policy.fresnelReflectance = photon_bsdf_fresnel(
            policy.incidentCosine,
            material->opticalIor > 0.0 ? material->opticalIor : material->bsdf.ior,
            material->bsdf.reflectivity);
        reflection_weight = specular_weight + transparency * policy.fresnelReflectance;
        transmission_weight = transparency * (1.0 - policy.fresnelReflectance);
        base_tint = photon_bsdf_clamp_vec01(vec3(material->baseColorR,
                                                 material->baseColorG,
                                                 material->baseColorB));

        if (!photon_bsdf_append(&policy,
                                RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_DIFFUSE,
                                diffuse_weight,
                                base_tint,
                                false) ||
            !photon_bsdf_append(
                &policy,
                material->bsdf.roughness <= 0.05
                    ? RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_SPECULAR
                    : RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_GLOSSY,
                reflection_weight,
                white,
                false) ||
            !photon_bsdf_append(&policy,
                                RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_TRANSMISSION,
                                transmission_weight,
                                base_tint,
                                false)) {
            policy.termination = RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_INVALID_MATERIAL;
            *out_policy = policy;
            return false;
        }
    }

    if (policy.candidateCount == 0u || policy.rawWeightSum <= 1.0e-12) {
        policy.termination = RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_ABSORBED;
        *out_policy = policy;
        return false;
    }

    for (uint32_t i = 0u; i < policy.candidateCount; ++i) {
        RuntimeCausticPhotonBsdfCandidate3D* candidate = &policy.candidates[i];
        Vec3 weighted_tint;
        candidate->selectionPdf = candidate->rawWeight / policy.rawWeightSum;
        policy.selectionPdfSum += candidate->selectionPdf;
        weighted_tint = vec3_scale(candidate->responseTint, candidate->selectionPdf);
        candidate->expectedThroughput =
            photon_bsdf_mul(parent_throughput, weighted_tint);
        policy.expectedScatteredThroughput = vec3_add(
            policy.expectedScatteredThroughput,
            candidate->expectedThroughput);
    }
    policy.expectedAbsorbedThroughput = photon_bsdf_sub_nonnegative(
        parent_throughput,
        policy.expectedScatteredThroughput);
    policy.valid = true;
    *out_policy = policy;
    return true;
}

bool RuntimeCausticPhotonBsdfPolicy3D_Select(
    const RuntimeCausticPhotonBsdfPolicy3D* policy,
    double unit_sample,
    RuntimeCausticPhotonBsdfSelection3D* out_selection) {
    RuntimeCausticPhotonBsdfSelection3D selection;
    double sample;
    double cumulative = 0.0;

    if (!out_selection) return false;
    memset(&selection, 0, sizeof(selection));
    selection.attempted = true;
    selection.candidateIndex = UINT32_MAX;
    if (!policy || !policy->valid || policy->candidateCount == 0u) {
        selection.termination = policy ? policy->termination
                                       : RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_INVALID_MATERIAL;
        *out_selection = selection;
        return false;
    }

    sample = photon_bsdf_clamp01(unit_sample);
    if (sample >= 1.0) sample = nextafter(1.0, 0.0);
    selection.unitSample = sample;
    selection.throughputBefore = policy->parentThroughput;
    for (uint32_t i = 0u; i < policy->candidateCount; ++i) {
        const RuntimeCausticPhotonBsdfCandidate3D* candidate = &policy->candidates[i];
        cumulative += candidate->selectionPdf;
        if (sample < cumulative || i + 1u == policy->candidateCount) {
            double compensation = candidate->selectionPdf > 0.0
                                      ? candidate->rawWeight / candidate->selectionPdf
                                      : 0.0;
            selection.selected = true;
            selection.candidateIndex = i;
            selection.lobe = candidate->lobe;
            selection.branchPdf = candidate->selectionPdf;
            selection.expectedContribution = candidate->expectedThroughput;
            selection.throughputAfter = photon_bsdf_mul(
                policy->parentThroughput,
                vec3_scale(candidate->responseTint, compensation));
            if (candidate->terminal) {
                selection.termination = policy->termination;
            } else if (photon_bsdf_peak(selection.throughputAfter) <= 1.0e-12) {
                selection.termination = RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_ABSORBED;
            }
            *out_selection = selection;
            return true;
        }
    }

    selection.termination = RUNTIME_CAUSTIC_PHOTON_BSDF_TERMINATION_INVALID_MATERIAL;
    *out_selection = selection;
    return false;
}
