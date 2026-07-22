#include "render/runtime_caustic_photon_volume_beam_estimator_3d.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double volume_beam_clamp(double value, double minimum, double maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static bool volume_beam_vec_finite(Vec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool volume_beam_has_energy(Vec3 value) {
    return value.x > 0.0 || value.y > 0.0 || value.z > 0.0;
}

static double volume_beam_luma(Vec3 value) {
    return 0.2126 * value.x + 0.7152 * value.y + 0.0722 * value.z;
}

static Vec3 volume_beam_component_multiply(Vec3 left, Vec3 right) {
    return vec3(left.x * right.x, left.y * right.y, left.z * right.z);
}

static Vec3 volume_beam_component_sqrt(Vec3 value) {
    return vec3(sqrt(fmax(0.0, value.x)),
                sqrt(fmax(0.0, value.y)),
                sqrt(fmax(0.0, value.z)));
}

void RuntimeCausticPhotonVolumeBeamEstimator3D_DefaultSettings(
    RuntimeCausticPhotonVolumeBeamEstimatorSettings3D* settings) {
    if (!settings) return;
    memset(settings, 0, sizeof(*settings));
    settings->queryRadius = 0.10;
    settings->scatteringCoefficient = 1.0;
    settings->extinctionCoefficient = 1.0;
    settings->phaseAnisotropy = 0.55;
    settings->requireMediumId = true;
    settings->mediumId = 0;
    settings->requireSegmentStage = true;
    settings->segmentStage = RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_LENS;
}

const char* RuntimeCausticPhotonVolumeBeamEligibility3D_Label(
    RuntimeCausticPhotonVolumeBeamEligibility3D eligibility) {
    switch (eligibility) {
        case RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_ELIGIBLE:
            return "eligible";
        case RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_STAGE:
            return "stage_rejected";
        case RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_MEDIUM:
            return "medium_rejected";
        case RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_OUTSIDE_VOLUME:
            return "outside_volume";
        case RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_INVALID:
        default:
            return "invalid";
    }
}

double RuntimeCausticPhotonVolumeBeamEstimator3D_CompactKernel(
    double distance,
    double radius) {
    double unit_distance;
    if (!isfinite(distance) || !isfinite(radius) || distance < 0.0 ||
        !(radius > 1.0e-12) || distance >= radius) {
        return 0.0;
    }
    unit_distance = distance / radius;
    return (2.0 / (M_PI * radius * radius)) *
           (1.0 - unit_distance * unit_distance);
}

double RuntimeCausticPhotonVolumeBeamEstimator3D_HenyeyGreensteinPhase(
    double cosine,
    double anisotropy) {
    const double g = volume_beam_clamp(anisotropy, -0.95, 0.95);
    const double g2 = g * g;
    const double clamped_cosine = volume_beam_clamp(cosine, -1.0, 1.0);
    const double denominator = 1.0 + g2 - 2.0 * g * clamped_cosine;
    if (!(denominator > 1.0e-12)) return 1.0 / (4.0 * M_PI);
    return (1.0 - g2) / (4.0 * M_PI * pow(denominator, 1.5));
}

RuntimeCausticPhotonVolumeBeamEligibility3D
RuntimeCausticPhotonVolumeBeamEstimator3D_SegmentEligibility(
    const RuntimeCausticPhotonVolumeBeamSegment3D* segment,
    const RuntimeCausticPhotonVolumeBeamEstimatorSettings3D* settings) {
    Vec3 axis;
    if (!segment || !settings || !volume_beam_vec_finite(segment->start) ||
        !volume_beam_vec_finite(segment->end) ||
        !volume_beam_vec_finite(segment->direction) ||
        !volume_beam_vec_finite(segment->flux)) {
        return RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_INVALID;
    }
    axis = vec3_sub(segment->end, segment->start);
    if (!(vec3_dot(axis, axis) > 1.0e-12) ||
        !volume_beam_has_energy(segment->flux)) {
        return RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_INVALID;
    }
    if (settings->requireSegmentStage &&
        segment->provenance.segmentStage != settings->segmentStage) {
        return RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_STAGE;
    }
    if (settings->requireMediumId && segment->mediumId != settings->mediumId) {
        return RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_MEDIUM;
    }
    return RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_ELIGIBLE;
}

bool RuntimeCausticPhotonVolumeBeamEstimator3D_ClipSegmentToBounds(
    const RuntimeCausticPhotonVolumeBeamSegment3D* segment,
    Vec3 bounds_min,
    Vec3 bounds_max,
    RuntimeCausticPhotonVolumeBeamSegment3D* out_segment) {
    Vec3 axis;
    double start_values[3];
    double axis_values[3];
    double min_values[3];
    double max_values[3];
    double t_min = 0.0;
    double t_max = 1.0;

    if (out_segment) memset(out_segment, 0, sizeof(*out_segment));
    if (!segment || !out_segment || !volume_beam_vec_finite(bounds_min) ||
        !volume_beam_vec_finite(bounds_max)) {
        return false;
    }
    axis = vec3_sub(segment->end, segment->start);
    if (!(vec3_dot(axis, axis) > 1.0e-12)) return false;
    start_values[0] = segment->start.x;
    start_values[1] = segment->start.y;
    start_values[2] = segment->start.z;
    axis_values[0] = axis.x;
    axis_values[1] = axis.y;
    axis_values[2] = axis.z;
    min_values[0] = bounds_min.x;
    min_values[1] = bounds_min.y;
    min_values[2] = bounds_min.z;
    max_values[0] = bounds_max.x;
    max_values[1] = bounds_max.y;
    max_values[2] = bounds_max.z;
    for (int i = 0; i < 3; ++i) {
        double entry;
        double exit;
        if (min_values[i] > max_values[i]) return false;
        if (fabs(axis_values[i]) <= 1.0e-12) {
            if (start_values[i] < min_values[i] ||
                start_values[i] > max_values[i]) {
                return false;
            }
            continue;
        }
        entry = (min_values[i] - start_values[i]) / axis_values[i];
        exit = (max_values[i] - start_values[i]) / axis_values[i];
        if (entry > exit) {
            const double swap = entry;
            entry = exit;
            exit = swap;
        }
        if (entry > t_min) t_min = entry;
        if (exit < t_max) t_max = exit;
        if (t_max <= t_min) return false;
    }
    *out_segment = *segment;
    out_segment->start = vec3_add(segment->start, vec3_scale(axis, t_min));
    out_segment->end = vec3_add(segment->start, vec3_scale(axis, t_max));
    out_segment->direction = vec3_normalize(axis);
    out_segment->radiusStart = segment->radiusStart +
        (segment->radiusEnd - segment->radiusStart) * t_min;
    out_segment->radiusEnd = segment->radiusStart +
        (segment->radiusEnd - segment->radiusStart) * t_max;
    return true;
}

bool RuntimeCausticPhotonVolumeBeamEstimator3D_Evaluate(
    const RuntimeCausticPhotonVolumeBeamEstimatorSettings3D* settings,
    const RuntimeCausticPhotonVolumeBeamEstimatorInput3D* input,
    RuntimeCausticPhotonVolumeBeamEstimatorReadback3D* out_readback) {
    RuntimeCausticPhotonVolumeBeamEstimatorReadback3D readback;
    Vec3 beam_direction;
    Vec3 view_direction;
    double sigma_s;
    double sigma_t;
    double density;

    memset(&readback, 0, sizeof(readback));
    if (out_readback) *out_readback = readback;
    if (!settings || !input || !out_readback ||
        !volume_beam_vec_finite(input->beamFluxDensity) ||
        !volume_beam_vec_finite(input->beamDirection) ||
        !volume_beam_vec_finite(input->viewToCameraDirection) ||
        !volume_beam_has_energy(input->beamFluxDensity) ||
        !(input->mediumDensity > 0.0) || !(input->stepLength > 0.0)) {
        return false;
    }
    beam_direction = vec3_normalize(input->beamDirection);
    view_direction = vec3_normalize(input->viewToCameraDirection);
    if (!(vec3_length(beam_direction) > 1.0e-12) ||
        !(vec3_length(view_direction) > 1.0e-12)) {
        return false;
    }
    sigma_s = fmax(0.0, settings->scatteringCoefficient);
    sigma_t = fmax(sigma_s, settings->extinctionCoefficient);
    density = fmax(0.0, input->mediumDensity);
    readback.evaluated = true;
    readback.phaseCosine = volume_beam_clamp(
        vec3_dot(beam_direction, view_direction), -1.0, 1.0);
    readback.phaseValue =
        RuntimeCausticPhotonVolumeBeamEstimator3D_HenyeyGreensteinPhase(
            readback.phaseCosine, settings->phaseAnisotropy);
    readback.beamTransmittance = exp(
        -sigma_t * density * fmax(0.0, input->beamDistance));
    readback.scatterProbability =
        1.0 - exp(-sigma_s * density * input->stepLength);
    readback.cameraTransmittance = volume_beam_clamp(
        input->cameraTransmittance, 0.0, 1.0);
    readback.integrationWeight = readback.beamTransmittance *
        readback.scatterProbability * readback.phaseValue *
        readback.cameraTransmittance;
    readback.radiance = vec3_scale(input->beamFluxDensity,
                                   readback.integrationWeight);
    readback.contributed = readback.integrationWeight > 0.0 &&
                           volume_beam_has_energy(readback.radiance);
    *out_readback = readback;
    return readback.contributed;
}

bool RuntimeCausticPhotonVolumeBeamEstimator3D_AnalyzePopulation(
    const RuntimeCausticPhotonVolumeBeamSegment3D* segments,
    uint64_t segment_count,
    const RuntimeCausticPhotonVolumeBeamEstimatorSettings3D* settings,
    Vec3 bounds_min,
    Vec3 bounds_max,
    RuntimeCausticPhotonVolumeBeamOracleReadback3D* out_readback) {
    RuntimeCausticPhotonVolumeBeamOracleReadback3D readback;
    Vec3 first_moment = vec3(0.0, 0.0, 0.0);
    Vec3 second_moment = vec3(0.0, 0.0, 0.0);
    Vec3 direction_moment = vec3(0.0, 0.0, 0.0);

    memset(&readback, 0, sizeof(readback));
    if (out_readback) *out_readback = readback;
    if (!out_readback || !segments || segment_count == 0u || !settings ||
        !volume_beam_vec_finite(bounds_min) ||
        !volume_beam_vec_finite(bounds_max)) {
        return false;
    }
    readback.attempted = true;
    readback.spatialBoundsMin = vec3(INFINITY, INFINITY, INFINITY);
    readback.spatialBoundsMax = vec3(-INFINITY, -INFINITY, -INFINITY);
    for (uint64_t i = 0u; i < segment_count; ++i) {
        const RuntimeCausticPhotonVolumeBeamSegment3D* source = &segments[i];
        RuntimeCausticPhotonVolumeBeamSegment3D clipped;
        const RuntimeCausticPhotonVolumeBeamEligibility3D eligibility =
            RuntimeCausticPhotonVolumeBeamEstimator3D_SegmentEligibility(
                source, settings);
        Vec3 axis;
        Vec3 midpoint;
        Vec3 mean_square_position;
        Vec3 transmitted_flux;
        double length;
        double scalar_weight;

        readback.segmentExaminedCount++;
        if (eligibility != RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_ELIGIBLE) {
            switch (eligibility) {
                case RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_STAGE:
                    readback.segmentRejectedStageCount++;
                    break;
                case RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_MEDIUM:
                    readback.segmentRejectedMediumCount++;
                    break;
                case RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_OUTSIDE_VOLUME:
                    readback.segmentRejectedOutsideCount++;
                    break;
                case RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_INVALID:
                default:
                    readback.segmentRejectedInvalidCount++;
                    break;
            }
            continue;
        }
        readback.segmentEligibleCount++;
        readback.eligiblePowerSum = vec3_add(readback.eligiblePowerSum,
                                             source->flux);
        if (!RuntimeCausticPhotonVolumeBeamEstimator3D_ClipSegmentToBounds(
                source, bounds_min, bounds_max, &clipped)) {
            readback.segmentRejectedOutsideCount++;
            continue;
        }
        axis = vec3_sub(clipped.end, clipped.start);
        length = vec3_length(axis);
        if (!(length > 1.0e-12)) {
            readback.segmentRejectedInvalidCount++;
            continue;
        }
        midpoint = vec3_scale(vec3_add(clipped.start, clipped.end), 0.5);
        mean_square_position = vec3(
            (clipped.start.x * clipped.start.x +
             clipped.start.x * clipped.end.x +
             clipped.end.x * clipped.end.x) / 3.0,
            (clipped.start.y * clipped.start.y +
             clipped.start.y * clipped.end.y +
             clipped.end.y * clipped.end.y) / 3.0,
            (clipped.start.z * clipped.start.z +
             clipped.start.z * clipped.end.z +
             clipped.end.z * clipped.end.z) / 3.0);
        transmitted_flux = vec3_scale(clipped.flux,
                                      fmax(0.0, clipped.transmittance));
        scalar_weight = fmax(0.0, volume_beam_luma(transmitted_flux)) * length;
        readback.segmentClippedCount++;
        readback.clippedLengthSum += length;
        readback.clippedPowerLength = vec3_add(
            readback.clippedPowerLength, vec3_scale(clipped.flux, length));
        readback.transmittedPowerLength = vec3_add(
            readback.transmittedPowerLength,
            vec3_scale(transmitted_flux, length));
        readback.scalarPowerLength +=
            fmax(0.0, volume_beam_luma(clipped.flux)) * length;
        readback.scalarTransmittedPowerLength += scalar_weight;
        first_moment = vec3_add(first_moment,
                                vec3_scale(midpoint, scalar_weight));
        second_moment = vec3_add(
            second_moment, vec3_scale(mean_square_position, scalar_weight));
        direction_moment = vec3_add(
            direction_moment,
            vec3_scale(vec3_normalize(axis), scalar_weight));
        readback.spatialBoundsMin.x = fmin(
            readback.spatialBoundsMin.x, fmin(clipped.start.x, clipped.end.x));
        readback.spatialBoundsMin.y = fmin(
            readback.spatialBoundsMin.y, fmin(clipped.start.y, clipped.end.y));
        readback.spatialBoundsMin.z = fmin(
            readback.spatialBoundsMin.z, fmin(clipped.start.z, clipped.end.z));
        readback.spatialBoundsMax.x = fmax(
            readback.spatialBoundsMax.x, fmax(clipped.start.x, clipped.end.x));
        readback.spatialBoundsMax.y = fmax(
            readback.spatialBoundsMax.y, fmax(clipped.start.y, clipped.end.y));
        readback.spatialBoundsMax.z = fmax(
            readback.spatialBoundsMax.z, fmax(clipped.start.z, clipped.end.z));
    }
    if (!(readback.scalarTransmittedPowerLength > 0.0) ||
        readback.segmentClippedCount == 0u) {
        readback.spatialBoundsMin = vec3(0.0, 0.0, 0.0);
        readback.spatialBoundsMax = vec3(0.0, 0.0, 0.0);
        *out_readback = readback;
        return false;
    }
    readback.powerLengthCentroid = vec3_scale(
        first_moment, 1.0 / readback.scalarTransmittedPowerLength);
    second_moment = vec3_scale(
        second_moment, 1.0 / readback.scalarTransmittedPowerLength);
    readback.powerLengthVariance = vec3_sub(
        second_moment,
        volume_beam_component_multiply(readback.powerLengthCentroid,
                                       readback.powerLengthCentroid));
    readback.powerLengthStdDev =
        volume_beam_component_sqrt(readback.powerLengthVariance);
    readback.powerLengthMeanDirection = vec3_normalize(direction_moment);
    readback.valid = true;
    *out_readback = readback;
    return true;
}
