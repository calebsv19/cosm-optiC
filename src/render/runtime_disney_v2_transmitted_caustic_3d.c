#include "render/runtime_disney_v2_transmitted_caustic_3d.h"

#include <math.h>

#include "render/runtime_caustic_photon_direct_consumer_3d.h"

static double transmitted_caustic_clamp01(double value) {
    if (!isfinite(value) || value <= 0.0) return 0.0;
    if (value >= 1.0) return 1.0;
    return value;
}

void RuntimeDisneyV2TransmittedCaustic3D_AccumulateResolvedSample(
    RuntimeDisneyV2_3DResult* result,
    bool query_hit,
    Vec3 radiance,
    double throughput_r,
    double throughput_g,
    double throughput_b) {
    double weighted_r = 0.0;
    double weighted_g = 0.0;
    double weighted_b = 0.0;

    if (!result) return;
    result->primaryTransmissionCausticDirectMapSampled = true;
    result->primaryTransmissionCausticLookupCount += 1;
    if (!query_hit) return;

    weighted_r = radiance.x * transmitted_caustic_clamp01(throughput_r);
    weighted_g = radiance.y * transmitted_caustic_clamp01(throughput_g);
    weighted_b = radiance.z * transmitted_caustic_clamp01(throughput_b);
    if (!(fmax(weighted_r, fmax(weighted_g, weighted_b)) > 0.0)) return;

    result->primaryTransmissionCausticRadianceR += weighted_r;
    result->primaryTransmissionCausticRadianceG += weighted_g;
    result->primaryTransmissionCausticRadianceB += weighted_b;
    result->primaryTransmissionCausticContributingSampleCount += 1;
}

bool RuntimeDisneyV2TransmittedCaustic3D_SampleDirectMap(
    RuntimeDisneyV2_3DResult* result,
    const HitInfo3D* receiver_hit,
    double throughput_r,
    double throughput_g,
    double throughput_b) {
    Vec3 radiance = vec3(0.0, 0.0, 0.0);
    bool query_hit = false;

    if (!result || !receiver_hit ||
        !RuntimeCausticPhotonDirectConsumer3D_Active()) {
        return false;
    }
    query_hit = RuntimeCausticPhotonDirectConsumer3D_SampleSurface(
        receiver_hit, &radiance, NULL);
    RuntimeDisneyV2TransmittedCaustic3D_AccumulateResolvedSample(
        result,
        query_hit,
        radiance,
        throughput_r,
        throughput_g,
        throughput_b);
    return true;
}

void RuntimeDisneyV2TransmittedCaustic3D_Finalize(
    RuntimeDisneyV2_3DResult* result,
    int transmission_contributing_sample_count) {
    double denominator = 0.0;

    if (!result || !result->primaryTransmissionCausticDirectMapSampled ||
        transmission_contributing_sample_count <= 0) {
        return;
    }
    denominator = (double)transmission_contributing_sample_count;
    result->primaryTransmissionCausticRadianceR /= denominator;
    result->primaryTransmissionCausticRadianceG /= denominator;
    result->primaryTransmissionCausticRadianceB /= denominator;
}
