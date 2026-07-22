#include "render/runtime_caustic_photon_emission_proposal_3d.h"
#include "render/runtime_light_radiometry_3d.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static uint32_t proposal_hash(uint64_t value) {
    value ^= value >> 30;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31;
    return (uint32_t)value;
}

static double proposal_unit(uint32_t value) {
    return ((double)value + 0.5) / 4294967296.0;
}

static Vec3 proposal_cross(Vec3 a, Vec3 b) {
    return vec3(a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x);
}

static double proposal_lens_radius(const RuntimeCausticLensShape3D* lens) {
    if (!lens) return 0.0;
    if (lens->radius > 1.0e-6) return lens->radius;
    return 0.5 * fmax(lens->boundsMax.x - lens->boundsMin.x,
                      lens->boundsMax.y - lens->boundsMin.y);
}

static double proposal_lens_direction_pdf(
    Vec3 origin,
    Vec3 direction,
    const RuntimeCausticLensShape3D* lens) {
    Vec3 normal;
    Vec3 center_delta;
    Vec3 hit;
    Vec3 radial;
    double radius;
    double denominator;
    double distance;
    double radial_squared;
    double area_pdf;
    double cosine;

    radius = proposal_lens_radius(lens);
    if (!(radius > 1.0e-6) ||
        !(vec3_length(direction) > 1.0e-9)) return 0.0;
    direction = vec3_normalize(direction);
    normal = vec3_sub(origin, lens->center);
    if (!(vec3_length(normal) > 1.0e-9)) return 0.0;
    normal = vec3_normalize(normal);
    center_delta = vec3_sub(lens->center, origin);
    denominator = vec3_dot(direction, normal);
    if (fabs(denominator) <= 1.0e-12) return 0.0;
    distance = vec3_dot(center_delta, normal) / denominator;
    if (!(distance > 1.0e-9)) return 0.0;
    hit = vec3_add(origin, vec3_scale(direction, distance));
    radial = vec3_sub(hit, lens->center);
    radial = vec3_sub(radial, vec3_scale(normal, vec3_dot(radial, normal)));
    radial_squared = vec3_dot(radial, radial);
    if (radial_squared > radius * radius * (1.0 + 1.0e-9)) return 0.0;
    cosine = fabs(vec3_dot(normal, vec3_scale(direction, -1.0)));
    if (!(cosine > 1.0e-12)) return 0.0;
    area_pdf = 1.0 / (M_PI * radius * radius);
    return area_pdf * distance * distance / cosine;
}

RuntimeCausticPhotonEmissionProposalMode3D
RuntimeCausticPhotonEmissionProposalMode3D_FromLabel(const char* label) {
    if (label && (strcmp(label, "unbiased") == 0 ||
                  strcmp(label, "source_unbiased") == 0)) {
        return RUNTIME_CAUSTIC_PHOTON_EMISSION_UNBIASED;
    }
    return RUNTIME_CAUSTIC_PHOTON_EMISSION_LENS_GUIDED;
}

const char* RuntimeCausticPhotonEmissionProposalMode3D_Label(
    RuntimeCausticPhotonEmissionProposalMode3D mode) {
    if (mode == RUNTIME_CAUSTIC_PHOTON_EMISSION_UNBIASED) return "unbiased";
    return "lens_guided";
}

static void proposal_lens_disk_samples(
    const RuntimeCausticPhotonSample3D* sample,
    double* out_radial_unit,
    double* out_azimuth_unit) {
    if (!out_radial_unit || !out_azimuth_unit) return;
    if (!sample) {
        *out_radial_unit = 0.5;
        *out_azimuth_unit = 0.5;
        return;
    }
    *out_radial_unit = proposal_unit(proposal_hash(sample->photonId));
    *out_azimuth_unit = proposal_unit(proposal_hash(
        sample->photonId ^ UINT64_C(0x9e3779b97f4a7c15)));
}

static bool proposal_apply_lens_guidance(
    RuntimeCausticPhotonSample3D* sample,
    const RuntimeLightSource3D* source,
    const RuntimeCausticLensShape3D* lenses,
    uint32_t lens_count,
    RuntimeCausticPhotonEmissionProposalReadback3D* out_readback) {

    RuntimeCausticPhotonEmissionProposalReadback3D readback;
    Vec3 aperture_normal;
    Vec3 tangent;
    Vec3 bitangent;
    Vec3 reference;
    Vec3 target;
    Vec3 to_target;
    double distance;
    double cosine;
    double radius;
    double area_pdf;
    double component_direction_pdf;
    double mixture_direction_pdf = 0.0;
    double unbiased_pdf;
    double radial;
    double phi;
    double radial_unit;
    double azimuth_unit;
    uint32_t selected_lens_index;
    const RuntimeCausticLensShape3D* lens;

    memset(&readback, 0, sizeof(readback));
    readback.attempted = true;
    if (out_readback) *out_readback = readback;
    if (!sample || !lenses || lens_count == 0u) return false;
    selected_lens_index = (uint32_t)(sample->sampleIndex % lens_count);
    lens = &lenses[selected_lens_index];
    radius = proposal_lens_radius(lens);
    if (!(radius > 1.0e-6)) return false;
    aperture_normal = vec3_sub(sample->position, lens->center);
    if (!(vec3_length(aperture_normal) > 1.0e-9)) return false;
    aperture_normal = vec3_normalize(aperture_normal);
    reference = fabs(aperture_normal.z) < 0.9 ? vec3(0.0, 0.0, 1.0)
                                              : vec3(0.0, 1.0, 0.0);
    tangent = proposal_cross(reference, aperture_normal);
    if (!(vec3_length(tangent) > 1.0e-9)) return false;
    tangent = vec3_normalize(tangent);
    bitangent = vec3_normalize(proposal_cross(aperture_normal, tangent));
    proposal_lens_disk_samples(
        sample, &radial_unit, &azimuth_unit);
    radial = radius * sqrt(radial_unit);
    phi = 2.0 * M_PI * azimuth_unit;
    target = vec3_add(lens->center,
                      vec3_add(vec3_scale(tangent, radial * cos(phi)),
                               vec3_scale(bitangent, radial * sin(phi))));
    to_target = vec3_sub(target, sample->position);
    distance = vec3_length(to_target);
    if (!(distance > 1.0e-9)) return false;
    sample->direction = vec3_scale(to_target, 1.0 / distance);
    cosine = fabs(vec3_dot(aperture_normal, vec3_scale(sample->direction, -1.0)));
    area_pdf = 1.0 / (M_PI * radius * radius);
    component_direction_pdf =
        cosine > 1.0e-12 ? area_pdf * distance * distance / cosine : 0.0;
    for (uint32_t lens_index = 0u; lens_index < lens_count; ++lens_index) {
        mixture_direction_pdf += proposal_lens_direction_pdf(
            sample->position, sample->direction, &lenses[lens_index]);
    }
    mixture_direction_pdf /= (double)lens_count;
    unbiased_pdf = RuntimeLightRadiometry3D_DirectionPdf(source,
                                                         sample->direction);
    if (!(unbiased_pdf > 1.0e-12)) {
        unbiased_pdf = sample->baseDirectionPdf > 1.0e-12
                           ? sample->baseDirectionPdf
                           : 1.0 / (4.0 * M_PI);
    }
    if (!(component_direction_pdf > 1.0e-12) ||
        !(mixture_direction_pdf > 1.0e-12)) return false;
    sample->directionPdf = mixture_direction_pdf;
    sample->proposalPdf = sample->sourceSelectionPdf * sample->positionPdf *
                          sample->directionPdf;
    sample->emissionPdf = sample->proposalPdf;
    sample->emissionFluxCorrection = unbiased_pdf / mixture_direction_pdf;
    sample->flux = vec3_scale(sample->flux, sample->emissionFluxCorrection);
    sample->guidingChangedSample =
        vec3_length(vec3_sub(sample->direction, sample->proposalDirection)) > 1.0e-9;
    sample->guidingPdfFluxCorrected = true;
    sample->fluxPdfCompensated = true;
    readback.applied = true;
    readback.apertureRadius = radius;
    readback.apertureAreaPdf = area_pdf;
    readback.guidedDirectionPdf = mixture_direction_pdf;
    readback.unbiasedDirectionPdf = unbiased_pdf;
    readback.fluxCorrection = sample->emissionFluxCorrection;
    readback.lensCount = lens_count;
    readback.selectedLensIndex = selected_lens_index;
    readback.selectedComponentDirectionPdf = component_direction_pdf;
    readback.mixtureDirectionPdf = mixture_direction_pdf;
    if (out_readback) *out_readback = readback;
    return true;
}

bool RuntimeCausticPhotonEmissionProposal3D_ApplyLensGuidance(
    RuntimeCausticPhotonSample3D* sample,
    const RuntimeLightSource3D* source,
    const RuntimeCausticLensShape3D* lens,
    RuntimeCausticPhotonEmissionProposalReadback3D* out_readback) {
    return proposal_apply_lens_guidance(
        sample, source, lens, 1u, out_readback);
}

bool RuntimeCausticPhotonEmissionProposal3D_ApplyLensGuidanceMixture(
    RuntimeCausticPhotonSample3D* sample,
    const RuntimeLightSource3D* source,
    const RuntimeCausticLensShape3D* lenses,
    uint32_t lens_count,
    RuntimeCausticPhotonEmissionProposalReadback3D* out_readback) {
    return proposal_apply_lens_guidance(
        sample, source, lenses, lens_count, out_readback);
}
