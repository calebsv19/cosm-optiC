#include "procedural/procedural_surface_material.h"
#include "app/ray_tracing_sha256.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void material_report(ProceduralSurfaceMaterialReport *report,
                            ProceduralSurfaceMaterialStatus status,
                            const char *field,
                            const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = status;
    snprintf(report->field, sizeof(report->field), "%s", field ? field : "");
    snprintf(report->message, sizeof(report->message), "%s", message ? message : "");
}

static double clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static double lerp(double a, double b, double t) {
    return a + ((b - a) * clamp01(t));
}

static double smoothstep(double edge0, double edge1, double value) {
    double t;
    if (!(edge1 > edge0)) return value >= edge1 ? 1.0 : 0.0;
    t = clamp01((value - edge0) / (edge1 - edge0));
    return t * t * (3.0 - (2.0 * t));
}

static bool field_valid(const ProceduralSurfaceFieldOutput *field) {
    return field &&
           isfinite(field->height) &&
           isfinite(field->macro_variation) &&
           isfinite(field->micro_variation) &&
           isfinite(field->rock_mask) &&
           isfinite(field->roughness) &&
           isfinite(field->snow_precursor) &&
           field->height >= -1.0 && field->height <= 1.0 &&
           field->macro_variation >= -1.0 && field->macro_variation <= 1.0 &&
           field->micro_variation >= -1.0 && field->micro_variation <= 1.0 &&
           field->rock_mask >= 0.0 && field->rock_mask <= 1.0 &&
           field->roughness >= 0.0 && field->roughness <= 1.0 &&
           field->snow_precursor >= 0.0 && field->snow_precursor <= 1.0;
}

static bool point_finite(ProceduralSurfaceFieldPoint3D point) {
    return isfinite(point.x) && isfinite(point.y) && isfinite(point.z);
}

static bool digest_valid(const char *digest) {
    size_t length;
    if (!digest) return false;
    length = strlen(digest);
    if (length != 64u) return false;
    for (size_t i = 0u; i < length; ++i) {
        const char c = digest[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

static bool sample_valid(const ProceduralSurfaceMaterialSample *sample) {
    const double *values = (const double *)sample;
    if (!sample) return false;
    for (size_t i = 0u; i < sizeof(*sample) / sizeof(double); ++i) {
        if (!isfinite(values[i])) return false;
    }
    return sample->stone_color_r >= 0.0 && sample->stone_color_r <= 1.0 &&
           sample->stone_color_g >= 0.0 && sample->stone_color_g <= 1.0 &&
           sample->stone_color_b >= 0.0 && sample->stone_color_b <= 1.0 &&
           sample->stone_roughness >= 0.0 && sample->stone_roughness <= 1.0 &&
           sample->upward_slope >= 0.0 && sample->upward_slope <= 1.0 &&
           sample->snow_elevation_weight >= 0.0 &&
           sample->snow_elevation_weight <= 1.0 &&
           sample->snow_slope_weight >= 0.0 &&
           sample->snow_slope_weight <= 1.0 &&
           sample->snow_breakup_weight >= 0.0 &&
           sample->snow_breakup_weight <= 1.0 &&
           sample->snow_likelihood >= 0.0 && sample->snow_likelihood <= 1.0 &&
           sample->final_color_r >= 0.0 && sample->final_color_r <= 1.0 &&
           sample->final_color_g >= 0.0 && sample->final_color_g <= 1.0 &&
           sample->final_color_b >= 0.0 && sample->final_color_b <= 1.0 &&
           sample->final_roughness >= 0.0 && sample->final_roughness <= 1.0;
}

bool ProceduralSurfaceMaterial_Evaluate(
    const ProceduralSurfaceRecipeV1 *recipe,
    const ProceduralSurfaceFieldOutput *retained_field,
    ProceduralSurfaceFieldPoint3D displaced_position,
    ProceduralSurfaceFieldPoint3D geometry_normal,
    ProceduralSurfaceMaterialSample *out_sample,
    ProceduralSurfaceMaterialReport *report) {
    ProceduralSurfaceRecipeReport recipe_report;
    ProceduralSurfaceMaterialSample sample;
    double normal_length;
    double macro01;
    double micro01;
    double stone_tone;
    double elevation_band;
    double slope_band;
    static const double snow_color[3] = {0.88, 0.92, 0.95};

    if (!recipe || !retained_field || !out_sample) {
        material_report(report, PROCEDURAL_SURFACE_MATERIAL_STATUS_NULL_ARGUMENT,
                        "arguments", "recipe, retained field, and output are required");
        return false;
    }
    if (!ProceduralSurfaceRecipeV1_Validate(recipe, &recipe_report)) {
        material_report(report, PROCEDURAL_SURFACE_MATERIAL_STATUS_RECIPE,
                        recipe_report.field, recipe_report.message);
        return false;
    }
    if (!field_valid(retained_field)) {
        material_report(report, PROCEDURAL_SURFACE_MATERIAL_STATUS_FIELD,
                        "retained_field", "retained PSG-1 field output is invalid");
        return false;
    }
    if (!point_finite(displaced_position)) {
        material_report(report, PROCEDURAL_SURFACE_MATERIAL_STATUS_POSITION,
                        "displaced_position", "displaced position must be finite");
        return false;
    }
    if (!point_finite(geometry_normal)) {
        material_report(report, PROCEDURAL_SURFACE_MATERIAL_STATUS_NORMAL,
                        "geometry_normal", "geometry normal must be finite");
        return false;
    }
    normal_length = sqrt((geometry_normal.x * geometry_normal.x) +
                         (geometry_normal.y * geometry_normal.y) +
                         (geometry_normal.z * geometry_normal.z));
    if (normal_length < 0.999 || normal_length > 1.001) {
        material_report(report, PROCEDURAL_SURFACE_MATERIAL_STATUS_NORMAL,
                        "geometry_normal", "geometry normal must be unit length");
        return false;
    }

    memset(&sample, 0, sizeof(sample));
    macro01 = clamp01((retained_field->macro_variation + 1.0) * 0.5);
    micro01 = clamp01((retained_field->micro_variation + 1.0) * 0.5);
    stone_tone = clamp01(0.34 +
                         (0.28 * retained_field->rock_mask) +
                         (0.16 * macro01) -
                         (0.08 * micro01));
    sample.stone_color_r = lerp(0.20, 0.48, stone_tone);
    sample.stone_color_g = lerp(0.19, 0.44, stone_tone);
    sample.stone_color_b = lerp(0.17, 0.38, stone_tone);
    sample.stone_roughness = clamp01(
        0.52 + (0.38 * retained_field->roughness) +
        (0.08 * fabs(retained_field->micro_variation)));
    sample.elevation_units = displaced_position.z;
    sample.upward_slope = clamp01(geometry_normal.z / normal_length);

    elevation_band = fmax(recipe->micro_feature_size_units,
                          recipe->base_feature_size_units * 0.2);
    slope_band = 0.15;
    sample.snow_elevation_weight = smoothstep(
        recipe->snow_elevation_threshold_units - elevation_band,
        recipe->snow_elevation_threshold_units + elevation_band,
        sample.elevation_units);
    sample.snow_slope_weight = smoothstep(
        recipe->snow_slope_threshold - slope_band,
        recipe->snow_slope_threshold + slope_band,
        sample.upward_slope);
    sample.snow_breakup_weight = clamp01(
        0.35 + (0.65 * retained_field->snow_precursor));
    sample.snow_likelihood = clamp01(
        sample.snow_elevation_weight *
        sample.snow_slope_weight *
        sample.snow_breakup_weight);
    sample.final_color_r = lerp(sample.stone_color_r, snow_color[0],
                                sample.snow_likelihood);
    sample.final_color_g = lerp(sample.stone_color_g, snow_color[1],
                                sample.snow_likelihood);
    sample.final_color_b = lerp(sample.stone_color_b, snow_color[2],
                                sample.snow_likelihood);
    sample.final_roughness = lerp(sample.stone_roughness, 0.72,
                                  sample.snow_likelihood);
    if (!sample_valid(&sample)) {
        material_report(report, PROCEDURAL_SURFACE_MATERIAL_STATUS_SAMPLE,
                        "sample", "coupled material sample is invalid");
        return false;
    }
    *out_sample = sample;
    material_report(report, PROCEDURAL_SURFACE_MATERIAL_STATUS_OK, "", "");
    return true;
}

static bool appendf(char *output,
                    size_t capacity,
                    size_t *used,
                    const char *format,
                    ...) {
    int count;
    va_list args;
    if (!output || !used || *used >= capacity) return false;
    va_start(args, format);
    count = vsnprintf(output + *used, capacity - *used, format, args);
    va_end(args);
    if (count < 0 || (size_t)count >= capacity - *used) return false;
    *used += (size_t)count;
    return true;
}

bool ProceduralSurfaceMaterial_CanonicalSummary(
    const char *recipe_digest,
    const char *shell_digest,
    const char *const *sample_ids,
    const ProceduralSurfaceMaterialSample *samples,
    size_t sample_count,
    char *out_summary,
    size_t out_capacity,
    ProceduralSurfaceMaterialReport *report) {
    size_t used = 0u;
    if (!recipe_digest || !shell_digest || !sample_ids || !samples ||
        !out_summary || out_capacity == 0u) {
        material_report(report, PROCEDURAL_SURFACE_MATERIAL_STATUS_NULL_ARGUMENT,
                        "summary", "summary arguments are required");
        return false;
    }
    if (!digest_valid(recipe_digest) || !digest_valid(shell_digest)) {
        material_report(report, PROCEDURAL_SURFACE_MATERIAL_STATUS_IDENTITY,
                        "identity", "recipe and shell digests must be lowercase SHA-256");
        return false;
    }
    out_summary[0] = '\0';
    if (!appendf(out_summary, out_capacity, &used,
                 "{\"schema\":\"ray_tracing.procedural_surface_material_summary\","
                 "\"schema_version\":1,\"coordinate_space\":\"object\","
                 "\"recipe_digest_sha256\":\"%s\","
                 "\"shell_digest_sha256\":\"%s\",\"samples\":[",
                 recipe_digest, shell_digest)) {
        goto capacity_failure;
    }
    for (size_t i = 0u; i < sample_count; ++i) {
        const ProceduralSurfaceMaterialSample *s = &samples[i];
        if (!sample_ids[i] || !sample_ids[i][0] || !sample_valid(s)) {
            material_report(report, PROCEDURAL_SURFACE_MATERIAL_STATUS_SAMPLE,
                            "samples", "sample id or values are invalid");
            return false;
        }
        if (!appendf(
                out_summary, out_capacity, &used,
                "%s{\"id\":\"%s\",\"stone_color\":[%.17g,%.17g,%.17g],"
                "\"stone_roughness\":%.17g,\"elevation_units\":%.17g,"
                "\"upward_slope\":%.17g,\"snow_elevation_weight\":%.17g,"
                "\"snow_slope_weight\":%.17g,\"snow_breakup_weight\":%.17g,"
                "\"snow_likelihood\":%.17g,\"final_color\":[%.17g,%.17g,%.17g],"
                "\"final_roughness\":%.17g}",
                i == 0u ? "" : ",", sample_ids[i],
                s->stone_color_r, s->stone_color_g, s->stone_color_b,
                s->stone_roughness, s->elevation_units, s->upward_slope,
                s->snow_elevation_weight, s->snow_slope_weight,
                s->snow_breakup_weight, s->snow_likelihood,
                s->final_color_r, s->final_color_g, s->final_color_b,
                s->final_roughness)) {
            goto capacity_failure;
        }
    }
    if (!appendf(out_summary, out_capacity, &used, "]}")) goto capacity_failure;
    material_report(report, PROCEDURAL_SURFACE_MATERIAL_STATUS_OK, "", "");
    return true;

capacity_failure:
    out_summary[0] = '\0';
    material_report(report, PROCEDURAL_SURFACE_MATERIAL_STATUS_SUMMARY,
                    "summary", "canonical summary capacity exceeded");
    return false;
}

bool ProceduralSurfaceMaterial_SummaryDigest(
    const char *recipe_digest,
    const char *shell_digest,
    const char *const *sample_ids,
    const ProceduralSurfaceMaterialSample *samples,
    size_t sample_count,
    char out_digest[PROCEDURAL_SURFACE_MATERIAL_DIGEST_CAPACITY],
    ProceduralSurfaceMaterialReport *report) {
    const size_t base_capacity = 512u;
    const size_t per_sample_capacity = 768u;
    size_t summary_capacity;
    char *summary;
    if (!out_digest) {
        material_report(report, PROCEDURAL_SURFACE_MATERIAL_STATUS_NULL_ARGUMENT,
                        "digest", "digest output is required");
        return false;
    }
    if (sample_count > (SIZE_MAX - base_capacity) / per_sample_capacity) {
        material_report(report, PROCEDURAL_SURFACE_MATERIAL_STATUS_SUMMARY,
                        "digest", "material digest capacity overflow");
        return false;
    }
    summary_capacity = base_capacity + sample_count * per_sample_capacity;
    summary = calloc(summary_capacity, 1u);
    if (!summary) {
        material_report(report, PROCEDURAL_SURFACE_MATERIAL_STATUS_SUMMARY,
                        "digest", "unable to allocate material digest summary");
        return false;
    }
    if (!ProceduralSurfaceMaterial_CanonicalSummary(
            recipe_digest, shell_digest, sample_ids, samples, sample_count,
            summary, summary_capacity, report)) {
        free(summary);
        return false;
    }
    if (!ray_tracing_sha256_bytes(summary, strlen(summary), out_digest)) {
        free(summary);
        material_report(report, PROCEDURAL_SURFACE_MATERIAL_STATUS_SUMMARY,
                        "digest", "SHA-256 generation failed");
        return false;
    }
    free(summary);
    material_report(report, PROCEDURAL_SURFACE_MATERIAL_STATUS_OK, "", "");
    return true;
}

const char *ProceduralSurfaceMaterialStatus_Name(
    ProceduralSurfaceMaterialStatus status) {
    switch (status) {
        case PROCEDURAL_SURFACE_MATERIAL_STATUS_OK: return "ok";
        case PROCEDURAL_SURFACE_MATERIAL_STATUS_NULL_ARGUMENT: return "null_argument";
        case PROCEDURAL_SURFACE_MATERIAL_STATUS_RECIPE: return "recipe";
        case PROCEDURAL_SURFACE_MATERIAL_STATUS_FIELD: return "field";
        case PROCEDURAL_SURFACE_MATERIAL_STATUS_POSITION: return "position";
        case PROCEDURAL_SURFACE_MATERIAL_STATUS_NORMAL: return "normal";
        case PROCEDURAL_SURFACE_MATERIAL_STATUS_IDENTITY: return "identity";
        case PROCEDURAL_SURFACE_MATERIAL_STATUS_SAMPLE: return "sample";
        case PROCEDURAL_SURFACE_MATERIAL_STATUS_SUMMARY: return "summary";
        default: return "unknown";
    }
}
