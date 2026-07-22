#include "render/runtime_caustic_photon_bsdf_sampling_3d.h"

#include <math.h>
#include <string.h>

static uint64_t photon_bsdf_sampling_mix64(uint64_t value) {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

static uint64_t photon_bsdf_sampling_key(const RuntimeCausticPhotonSample3D* photon,
                                         uint32_t depth) {
    uint64_t mixed;
    if (!photon) return 0u;
    mixed = photon_bsdf_sampling_mix64(photon->photonId);
    mixed ^= photon_bsdf_sampling_mix64(photon->sampleIndex ^ 0x632be59bd9b4e019ULL);
    mixed ^= photon_bsdf_sampling_mix64((uint64_t)photon->rngSeed << 32);
    mixed ^= photon_bsdf_sampling_mix64((uint64_t)depth ^ 0xa511e9b3ULL);
    mixed = photon_bsdf_sampling_mix64(mixed);
    return mixed;
}

static double photon_bsdf_sampling_unit(uint64_t key, uint64_t dimension_salt) {
    const uint64_t bits = photon_bsdf_sampling_mix64(key ^ dimension_salt) >> 11;
    return (double)bits * (1.0 / 9007199254740992.0);
}

bool RuntimeCausticPhotonBsdfSampling3D_Generate(
    const RuntimeCausticPhotonSample3D* photon,
    uint32_t depth,
    RuntimeCausticPhotonBsdfSampleStream3D* out_stream) {
    RuntimeCausticPhotonBsdfSampleStream3D stream;
    uint64_t key;

    if (!out_stream) return false;
    memset(&stream, 0, sizeof(stream));
    *out_stream = stream;
    if (!photon || depth == 0u) return false;

    stream.depth = depth;
    key = photon_bsdf_sampling_key(photon, depth);
    stream.baseSeed = (uint32_t)(key ^ (key >> 32));
    stream.bsdfSample.lobeUnitSample =
        photon_bsdf_sampling_unit(key, 0x243f6a8885a308d3ULL);
    stream.bsdfSample.directionSample.unitU =
        photon_bsdf_sampling_unit(key, 0x13198a2e03707344ULL);
    stream.bsdfSample.directionSample.unitV =
        photon_bsdf_sampling_unit(key, 0xa4093822299f31d0ULL);
    stream.rouletteUnitSample =
        photon_bsdf_sampling_unit(key, 0x082efa98ec4e6c89ULL);
    stream.valid = isfinite(stream.bsdfSample.lobeUnitSample) &&
                   isfinite(stream.bsdfSample.directionSample.unitU) &&
                   isfinite(stream.bsdfSample.directionSample.unitV) &&
                   isfinite(stream.rouletteUnitSample);
    *out_stream = stream;
    return stream.valid;
}

bool RuntimeCausticPhotonBsdfSampling3D_EvaluateRoulette(
    const RuntimePathDepthPolicy3D* policy,
    uint32_t depth,
    Vec3 transport_weight,
    double unit_sample,
    RuntimeCausticPhotonRoulette3D* out_roulette) {
    RuntimeCausticPhotonRoulette3D roulette;
    double survival;

    if (!out_roulette) return false;
    memset(&roulette, 0, sizeof(roulette));
    *out_roulette = roulette;
    if (depth == 0u || !isfinite(unit_sample) || unit_sample < 0.0 ||
        unit_sample >= 1.0 || !isfinite(transport_weight.x) ||
        transport_weight.x < 0.0 || !isfinite(transport_weight.y) ||
        transport_weight.y < 0.0 || !isfinite(transport_weight.z) ||
        transport_weight.z < 0.0) {
        return false;
    }

    roulette.depth = depth;
    roulette.unitSample = unit_sample;
    roulette.throughputBefore = transport_weight;
    roulette.throughputLuma =
        0.2126 * transport_weight.x + 0.7152 * transport_weight.y +
        0.0722 * transport_weight.z;
    survival = RuntimePathDepthPolicy3D_SurvivalProbability(
        policy, (int)depth, roulette.throughputLuma);
    roulette.survivalProbability = survival;
    roulette.evaluated = survival < 1.0;
    roulette.terminated =
        roulette.evaluated && RuntimePathDepthPolicy3D_ShouldTerminate(
                                  policy,
                                  (int)depth,
                                  roulette.throughputLuma,
                                  unit_sample,
                                  NULL);
    roulette.expectedThroughput = transport_weight;
    if (!roulette.evaluated) {
        roulette.branchPdf = 1.0;
        roulette.throughputAfter = transport_weight;
    } else if (roulette.terminated) {
        roulette.branchPdf = 1.0 - survival;
        roulette.terminatedThroughput = transport_weight;
        roulette.throughputAfter = vec3(0.0, 0.0, 0.0);
    } else {
        roulette.branchPdf = survival;
        roulette.throughputAfter = vec3_scale(transport_weight, 1.0 / survival);
    }
    roulette.valid = true;
    *out_roulette = roulette;
    return true;
}
