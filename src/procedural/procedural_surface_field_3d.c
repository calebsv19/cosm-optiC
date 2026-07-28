#include "procedural/procedural_surface_field_3d.h"

#include "app/ray_tracing_sha256.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define PROCEDURAL_SURFACE_FIELD_COORDINATE_LIMIT 1000000000.0
#define PROCEDURAL_SURFACE_FIELD_LATTICE_LIMIT 4503599627370495.0

static const uint64_t MACRO_DOMAIN = UINT64_C(0x6d6163726f5f7631);
static const uint64_t MICRO_DOMAIN = UINT64_C(0x6d6963726f5f7631);

static void report_set(ProceduralSurfaceFieldReport *report,
                       ProceduralSurfaceFieldStatus status,
                       const char *field,
                       const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = status;
    if (field) snprintf(report->field, sizeof(report->field), "%s", field);
    if (message) {
        snprintf(report->message, sizeof(report->message), "%s", message);
    }
}

static double clamp_value(double value, double minimum, double maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static double clamp01(double value) {
    return clamp_value(value, 0.0, 1.0);
}

static double lerp(double a, double b, double t) {
    return a + ((b - a) * t);
}

static double smoother_step(double t) {
    t = clamp01(t);
    return t * t * t * ((t * ((t * 6.0) - 15.0)) + 10.0);
}

static uint64_t mix_u64(uint64_t value) {
    value += UINT64_C(0x9e3779b97f4a7c15);
    value = (value ^ (value >> 30u)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27u)) * UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31u);
}

static uint64_t lattice_hash(uint64_t seed,
                             int64_t x,
                             int64_t y,
                             int64_t z,
                             uint64_t domain,
                             uint32_t octave) {
    uint64_t hash = mix_u64(seed ^ domain);
    hash = mix_u64(hash ^ mix_u64((uint64_t)x));
    hash = mix_u64(hash ^ mix_u64((uint64_t)y));
    hash = mix_u64(hash ^ mix_u64((uint64_t)z));
    return mix_u64(hash ^ mix_u64((uint64_t)octave));
}

static double lattice_signed_value(uint64_t seed,
                                   int64_t x,
                                   int64_t y,
                                   int64_t z,
                                   uint64_t domain,
                                   uint32_t octave) {
    const uint64_t hash = lattice_hash(seed, x, y, z, domain, octave);
    const double unit =
        (double)(hash >> 11u) * (1.0 / 9007199254740992.0);
    return (unit * 2.0) - 1.0;
}

static double value_noise_3d(uint64_t seed,
                             ProceduralSurfaceFieldPoint3D point,
                             uint64_t domain,
                             uint32_t octave) {
    const double floor_x = floor(point.x);
    const double floor_y = floor(point.y);
    const double floor_z = floor(point.z);
    const int64_t x0 = (int64_t)floor_x;
    const int64_t y0 = (int64_t)floor_y;
    const int64_t z0 = (int64_t)floor_z;
    const double tx = smoother_step(point.x - floor_x);
    const double ty = smoother_step(point.y - floor_y);
    const double tz = smoother_step(point.z - floor_z);
    double xy0;
    double xy1;

    const double x00 = lerp(
        lattice_signed_value(seed, x0, y0, z0, domain, octave),
        lattice_signed_value(seed, x0 + 1, y0, z0, domain, octave),
        tx);
    const double x10 = lerp(
        lattice_signed_value(seed, x0, y0 + 1, z0, domain, octave),
        lattice_signed_value(seed, x0 + 1, y0 + 1, z0, domain, octave),
        tx);
    const double x01 = lerp(
        lattice_signed_value(seed, x0, y0, z0 + 1, domain, octave),
        lattice_signed_value(seed, x0 + 1, y0, z0 + 1, domain, octave),
        tx);
    const double x11 = lerp(
        lattice_signed_value(seed, x0, y0 + 1, z0 + 1, domain, octave),
        lattice_signed_value(seed, x0 + 1, y0 + 1, z0 + 1, domain, octave),
        tx);

    xy0 = lerp(x00, x10, ty);
    xy1 = lerp(x01, x11, ty);
    return clamp_value(lerp(xy0, xy1, tz), -1.0, 1.0);
}

static double fbm_3d(const ProceduralSurfaceRecipeV1 *recipe,
                     ProceduralSurfaceFieldPoint3D point,
                     double feature_size,
                     uint64_t domain) {
    double amplitude = 1.0;
    double frequency = 1.0 / feature_size;
    double weighted_sum = 0.0;
    double weight_sum = 0.0;

    for (uint32_t octave = 0u; octave < recipe->octave_count; ++octave) {
        ProceduralSurfaceFieldPoint3D scaled = {
            .x = point.x * frequency,
            .y = point.y * frequency,
            .z = point.z * frequency
        };
        weighted_sum +=
            value_noise_3d(recipe->seed, scaled, domain, octave) * amplitude;
        weight_sum += amplitude;
        frequency *= recipe->lacunarity;
        amplitude *= recipe->persistence;
    }
    if (weight_sum <= 0.0) return 0.0;
    return clamp_value(weighted_sum / weight_sum, -1.0, 1.0);
}

static void evaluate_base(const ProceduralSurfaceRecipeV1 *recipe,
                          ProceduralSurfaceFieldPoint3D point,
                          ProceduralSurfaceFieldOutput *output) {
    const double macro =
        fbm_3d(recipe, point, recipe->base_feature_size_units, MACRO_DOMAIN);
    const double micro =
        fbm_3d(recipe, point, recipe->micro_feature_size_units, MICRO_DOMAIN);
    const double ridged = 1.0 - (2.0 * fabs(macro));
    const double shaped_macro =
        lerp(macro, ridged, recipe->ridge_valley_blend);
    const double height = clamp_value(
        lerp(micro, shaped_macro, recipe->macro_micro_mix), -1.0, 1.0);
    const double ridge_presence = clamp01(1.0 - fabs(macro));
    const double rock =
        clamp01((ridge_presence * 0.65) + (fabs(micro) * 0.35));

    memset(output, 0, sizeof(*output));
    output->height = height;
    output->macro_variation = macro;
    output->micro_variation = micro;
    output->rock_mask = rock;
    output->roughness =
        clamp01(0.3 + (rock * 0.5) + (fabs(micro) * 0.2));
}

static double normalized_height_at(
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfaceFieldPoint3D point) {
    ProceduralSurfaceFieldOutput output;
    evaluate_base(recipe, point, &output);
    return output.height;
}

static double smooth_range(double edge0, double edge1, double value) {
    if (!(edge1 > edge0)) return value >= edge1 ? 1.0 : 0.0;
    return smoother_step((value - edge0) / (edge1 - edge0));
}

static double snow_precursor(
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfaceFieldPoint3D point,
    const ProceduralSurfaceFieldOutput *base) {
    const double epsilon =
        fmin(recipe->micro_feature_size_units,
             recipe->target_edge_length_units) *
        0.25;
    ProceduralSurfaceFieldPoint3D low = point;
    ProceduralSurfaceFieldPoint3D high = point;
    double dx;
    double dy;
    double dz;
    double physical_slope;
    double elevation;
    double elevation_mask;
    double slope_mask;

    low.x -= epsilon;
    high.x += epsilon;
    dx = (normalized_height_at(recipe, high) -
          normalized_height_at(recipe, low)) /
         (2.0 * epsilon);
    low = point;
    high = point;
    low.y -= epsilon;
    high.y += epsilon;
    dy = (normalized_height_at(recipe, high) -
          normalized_height_at(recipe, low)) /
         (2.0 * epsilon);
    low = point;
    high = point;
    low.z -= epsilon;
    high.z += epsilon;
    dz = (normalized_height_at(recipe, high) -
          normalized_height_at(recipe, low)) /
         (2.0 * epsilon);

    physical_slope =
        sqrt((dx * dx) + (dy * dy) + (dz * dz)) *
        recipe->displacement_amplitude_units;
    elevation =
        point.z + (base->height * recipe->displacement_amplitude_units);
    elevation_mask = smooth_range(
        recipe->snow_elevation_threshold_units -
            (recipe->base_feature_size_units * 0.5),
        recipe->snow_elevation_threshold_units +
            (recipe->base_feature_size_units * 0.5),
        elevation);
    slope_mask = 1.0 - smooth_range(
        recipe->snow_slope_threshold * 0.5,
        recipe->snow_slope_threshold,
        physical_slope);
    return clamp01(
        elevation_mask * slope_mask * (0.75 + (0.25 * (1.0 - base->rock_mask))));
}

static bool point_is_safe(const ProceduralSurfaceRecipeV1 *recipe,
                          ProceduralSurfaceFieldPoint3D point) {
    const double maximum_coordinate =
        fmax(fabs(point.x), fmax(fabs(point.y), fabs(point.z)));
    const double minimum_feature =
        fmin(recipe->base_feature_size_units,
             recipe->micro_feature_size_units);
    double scaled_coordinate = maximum_coordinate / minimum_feature;

    if (maximum_coordinate > PROCEDURAL_SURFACE_FIELD_COORDINATE_LIMIT) {
        return false;
    }
    for (uint32_t octave = 0u; octave < recipe->octave_count; ++octave) {
        if (!isfinite(scaled_coordinate) ||
            scaled_coordinate > PROCEDURAL_SURFACE_FIELD_LATTICE_LIMIT) {
            return false;
        }
        scaled_coordinate *= recipe->lacunarity;
    }
    return true;
}

static bool output_is_finite(const ProceduralSurfaceFieldOutput *output) {
    return isfinite(output->height) &&
           isfinite(output->macro_variation) &&
           isfinite(output->micro_variation) &&
           isfinite(output->rock_mask) &&
           isfinite(output->roughness) &&
           isfinite(output->snow_precursor);
}

void ProceduralSurfaceFieldBudget_Init(
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfaceFieldBudget *budget) {
    if (!budget) return;
    memset(budget, 0, sizeof(*budget));
    if (recipe) {
        budget->max_evaluations = recipe->quality.max_field_evaluations;
    }
}

bool ProceduralSurfaceField3D_Evaluate(
    const ProceduralSurfaceRecipeV1 *recipe,
    ProceduralSurfaceFieldPoint3D point,
    ProceduralSurfaceFieldBudget *budget,
    ProceduralSurfaceFieldOutput *out_output,
    ProceduralSurfaceFieldReport *report) {
    ProceduralSurfaceRecipeReport recipe_report;
    ProceduralSurfaceFieldOutput evaluated;

    report_set(report, PROCEDURAL_SURFACE_FIELD_STATUS_OK, "", "ok");
    if (!recipe || !budget || !out_output) {
        report_set(report,
                   PROCEDURAL_SURFACE_FIELD_STATUS_NULL_ARGUMENT,
                   "",
                   "recipe, budget, and output are required");
        return false;
    }
    if (!ProceduralSurfaceRecipeV1_Validate(recipe, &recipe_report)) {
        report_set(report,
                   PROCEDURAL_SURFACE_FIELD_STATUS_RECIPE,
                   recipe_report.field,
                   recipe_report.message);
        return false;
    }
    if (!isfinite(point.x) || !isfinite(point.y) || !isfinite(point.z)) {
        report_set(report,
                   PROCEDURAL_SURFACE_FIELD_STATUS_NON_FINITE_POINT,
                   "point",
                   "point coordinates must be finite");
        return false;
    }
    if (!point_is_safe(recipe, point)) {
        report_set(report,
                   PROCEDURAL_SURFACE_FIELD_STATUS_COORDINATE_RANGE,
                   "point",
                   "point exceeds the deterministic coordinate range");
        return false;
    }
    if (budget->max_evaluations !=
            (uint64_t)recipe->quality.max_field_evaluations ||
        budget->evaluations >= budget->max_evaluations) {
        report_set(report,
                   PROCEDURAL_SURFACE_FIELD_STATUS_BUDGET,
                   "quality_budgets.max_field_evaluations",
                   "field evaluation budget is invalid or exhausted");
        return false;
    }

    evaluate_base(recipe, point, &evaluated);
    evaluated.snow_precursor =
        snow_precursor(recipe, point, &evaluated);
    if (!output_is_finite(&evaluated)) {
        report_set(report,
                   PROCEDURAL_SURFACE_FIELD_STATUS_NON_FINITE_OUTPUT,
                   "output",
                   "field evaluation produced a non-finite output");
        return false;
    }

    *out_output = evaluated;
    budget->evaluations += 1u;
    return true;
}

static bool sample_id_is_valid(const char *sample_id) {
    size_t length = 0u;
    if (!sample_id || sample_id[0] == '\0') return false;
    for (const unsigned char *cursor =
             (const unsigned char *)sample_id;
         *cursor != '\0';
         ++cursor) {
        if (!(isalnum(*cursor) || *cursor == '_' || *cursor == '-' ||
              *cursor == '.')) {
            return false;
        }
        length += 1u;
        if (length > 63u) return false;
    }
    return true;
}

static bool append_text(char *output,
                        size_t capacity,
                        size_t *length,
                        const char *format,
                        ...) {
    va_list arguments;
    int written;
    if (!output || !length || *length >= capacity) return false;
    va_start(arguments, format);
    written = vsnprintf(
        output + *length, capacity - *length, format, arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written >= capacity - *length) return false;
    *length += (size_t)written;
    return true;
}

bool ProceduralSurfaceField3D_CanonicalSummary(
    const char *recipe_digest,
    const char *const *sample_ids,
    const ProceduralSurfaceFieldOutput *outputs,
    size_t sample_count,
    char *out_summary,
    size_t out_capacity,
    ProceduralSurfaceFieldReport *report) {
    char summary[PROCEDURAL_SURFACE_FIELD_SUMMARY_CAPACITY];
    size_t length = 0u;

    report_set(report, PROCEDURAL_SURFACE_FIELD_STATUS_OK, "", "ok");
    if (!recipe_digest || !sample_ids || !outputs || !out_summary ||
        out_capacity == 0u) {
        report_set(report,
                   PROCEDURAL_SURFACE_FIELD_STATUS_NULL_ARGUMENT,
                   "",
                   "summary inputs and output are required");
        return false;
    }
    if (strlen(recipe_digest) != 64u ||
        !ray_tracing_sha256_is_valid_hex(recipe_digest) ||
        sample_count == 0u || sample_count > 1024u) {
        report_set(report,
                   PROCEDURAL_SURFACE_FIELD_STATUS_SUMMARY,
                   "summary",
                   "summary digest or sample count is invalid");
        return false;
    }
    if (!append_text(
            summary,
            sizeof(summary),
            &length,
            "{\"schema\":\"ray_tracing.procedural_surface_field_summary\","
            "\"schema_version\":1,\"recipe_digest_sha256\":\"%s\","
            "\"samples\":[",
            recipe_digest)) {
        goto capacity_failure;
    }
    for (size_t i = 0u; i < sample_count; ++i) {
        if (!sample_id_is_valid(sample_ids[i])) {
            report_set(report,
                       PROCEDURAL_SURFACE_FIELD_STATUS_SAMPLE_ID,
                       "sample_id",
                       "sample ids must use 1-63 safe identifier characters");
            return false;
        }
        if (!output_is_finite(&outputs[i])) {
            report_set(report,
                       PROCEDURAL_SURFACE_FIELD_STATUS_NON_FINITE_OUTPUT,
                       "output",
                       "summary outputs must be finite");
            return false;
        }
        if (!append_text(
                summary,
                sizeof(summary),
                &length,
                "%s{\"sample_id\":\"%s\",\"height\":%.17g,"
                "\"macro_variation\":%.17g,\"micro_variation\":%.17g,"
                "\"rock_mask\":%.17g,\"roughness\":%.17g,"
                "\"snow_precursor\":%.17g}",
                i == 0u ? "" : ",",
                sample_ids[i],
                outputs[i].height,
                outputs[i].macro_variation,
                outputs[i].micro_variation,
                outputs[i].rock_mask,
                outputs[i].roughness,
                outputs[i].snow_precursor)) {
            goto capacity_failure;
        }
    }
    if (!append_text(summary, sizeof(summary), &length, "]}") ||
        length + 1u > out_capacity) {
        goto capacity_failure;
    }
    memcpy(out_summary, summary, length + 1u);
    return true;

capacity_failure:
    report_set(report,
               PROCEDURAL_SURFACE_FIELD_STATUS_SUMMARY,
               "summary",
               "canonical field summary exceeds output capacity");
    return false;
}

bool ProceduralSurfaceField3D_SummaryDigest(
    const char *recipe_digest,
    const char *const *sample_ids,
    const ProceduralSurfaceFieldOutput *outputs,
    size_t sample_count,
    char out_digest[PROCEDURAL_SURFACE_FIELD_DIGEST_CAPACITY],
    ProceduralSurfaceFieldReport *report) {
    char summary[PROCEDURAL_SURFACE_FIELD_SUMMARY_CAPACITY];
    char digest[PROCEDURAL_SURFACE_FIELD_DIGEST_CAPACITY];
    if (!out_digest) {
        report_set(report,
                   PROCEDURAL_SURFACE_FIELD_STATUS_NULL_ARGUMENT,
                   "digest",
                   "digest output is required");
        return false;
    }
    if (!ProceduralSurfaceField3D_CanonicalSummary(
            recipe_digest,
            sample_ids,
            outputs,
            sample_count,
            summary,
            sizeof(summary),
            report)) {
        return false;
    }
    if (!ray_tracing_sha256_bytes(summary, strlen(summary), digest)) {
        report_set(report,
                   PROCEDURAL_SURFACE_FIELD_STATUS_SUMMARY,
                   "digest",
                   "unable to hash canonical field summary");
        return false;
    }
    memcpy(out_digest, digest, sizeof(digest));
    return true;
}

const char *ProceduralSurfaceFieldStatus_Name(
    ProceduralSurfaceFieldStatus status) {
    switch (status) {
        case PROCEDURAL_SURFACE_FIELD_STATUS_OK: return "ok";
        case PROCEDURAL_SURFACE_FIELD_STATUS_NULL_ARGUMENT:
            return "null_argument";
        case PROCEDURAL_SURFACE_FIELD_STATUS_RECIPE: return "recipe";
        case PROCEDURAL_SURFACE_FIELD_STATUS_NON_FINITE_POINT:
            return "non_finite_point";
        case PROCEDURAL_SURFACE_FIELD_STATUS_COORDINATE_RANGE:
            return "coordinate_range";
        case PROCEDURAL_SURFACE_FIELD_STATUS_BUDGET: return "budget";
        case PROCEDURAL_SURFACE_FIELD_STATUS_NON_FINITE_OUTPUT:
            return "non_finite_output";
        case PROCEDURAL_SURFACE_FIELD_STATUS_SAMPLE_ID: return "sample_id";
        case PROCEDURAL_SURFACE_FIELD_STATUS_SUMMARY: return "summary";
    }
    return "unknown";
}
