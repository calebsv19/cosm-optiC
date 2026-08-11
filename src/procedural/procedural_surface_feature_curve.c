#include "procedural/procedural_surface_feature_curve.h"

#include "app/ray_tracing_sha256.h"

#include <json-c/json.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

static double dot(ProceduralSurfaceFeatureVec3 a,
                  ProceduralSurfaceFeatureVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static ProceduralSurfaceFeatureVec3 sub(ProceduralSurfaceFeatureVec3 a,
                                        ProceduralSurfaceFeatureVec3 b) {
    return (ProceduralSurfaceFeatureVec3){
        a.x - b.x, a.y - b.y, a.z - b.z};
}

static ProceduralSurfaceFeatureVec3 cross(ProceduralSurfaceFeatureVec3 a,
                                          ProceduralSurfaceFeatureVec3 b) {
    return (ProceduralSurfaceFeatureVec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

static double length(ProceduralSurfaceFeatureVec3 value) {
    return sqrt(dot(value, value));
}

static bool normalize(ProceduralSurfaceFeatureVec3 value,
                      ProceduralSurfaceFeatureVec3 *out) {
    double magnitude;
    if (!out) return false;
    magnitude = length(value);
    if (!isfinite(magnitude) || magnitude <= 1e-12) return false;
    *out = (ProceduralSurfaceFeatureVec3){
        value.x / magnitude, value.y / magnitude, value.z / magnitude};
    return true;
}

static bool finite_vector(ProceduralSurfaceFeatureVec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool unit(ProceduralSurfaceFeatureVec3 value) {
    return finite_vector(value) && fabs(length(value) - 1.0) <= 1e-6;
}

static double clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static bool sha256_text(const char *text) {
    size_t index;
    if (!text || strlen(text) != 64u) return false;
    for (index = 0u; index < 64u; ++index) {
        if (!((text[index] >= '0' && text[index] <= '9') ||
              (text[index] >= 'a' && text[index] <= 'f'))) return false;
    }
    return true;
}

static bool barycentric_valid(const double value[3]) {
    return value && isfinite(value[0]) && isfinite(value[1]) &&
        isfinite(value[2]) && value[0] >= 0.0 && value[1] >= 0.0 &&
        value[2] >= 0.0 &&
        fabs(value[0] + value[1] + value[2] - 1.0) <= 1e-6;
}

bool ProceduralSurfaceFeatureCurveFieldV1_Validate(
    const ProceduralSurfaceFeatureCurveFieldV1 *field) {
    size_t index;
    if (!field || !sha256_text(field->source_mesh_digest_sha256) ||
        !sha256_text(field->authoring_digest_sha256) ||
        field->segment_count == 0u ||
        field->segment_count > PROCEDURAL_SURFACE_FEATURE_CURVE_MAX_SEGMENTS ||
        !isfinite(field->normal_compatibility_cosine) ||
        field->normal_compatibility_cosine < -1.0 ||
        field->normal_compatibility_cosine > 1.0) return false;
    for (index = 0u; index < field->segment_count; ++index) {
        const ProceduralSurfaceFeatureCurveSegmentV1 *segment =
            &field->segments[index];
        ProceduralSurfaceFeatureVec3 chord = sub(segment->end, segment->start);
        size_t earlier;
        if (!segment->curve_id || !segment->segment_id ||
            !barycentric_valid(segment->barycentric_start) ||
            !barycentric_valid(segment->barycentric_end) ||
            !finite_vector(segment->start) || !finite_vector(segment->end) ||
            !unit(segment->normal_start) || !unit(segment->normal_end) ||
            !unit(segment->tangent) || length(chord) <= 1e-9 ||
            fabs(dot(segment->normal_start, segment->tangent)) > 1e-6 ||
            fabs(dot(segment->normal_end, segment->tangent)) > 0.35 ||
            !isfinite(segment->width_start) || segment->width_start <= 0.0 ||
            !isfinite(segment->width_end) || segment->width_end <= 0.0 ||
            !isfinite(segment->depth_start) || segment->depth_start <= 0.0 ||
            !isfinite(segment->depth_end) || segment->depth_end <= 0.0 ||
            !isfinite(segment->edge_softness) ||
            segment->edge_softness <= 0.0 || segment->edge_softness > 1.0 ||
            !isfinite(segment->rim_width) || segment->rim_width <= 0.0 ||
            segment->rim_width >= 1.0) return false;
        for (earlier = 0u; earlier < index; ++earlier) {
            if (field->segments[earlier].segment_id == segment->segment_id)
                return false;
        }
    }
    return true;
}

static size_t grid_cell(const ProceduralSurfaceFeatureCurveFieldV1 *field,
                        double x, double y) {
    double sx = (x - field->grid_min_x) /
        (field->grid_max_x - field->grid_min_x);
    double sy = (y - field->grid_min_y) /
        (field->grid_max_y - field->grid_min_y);
    int ix = (int)floor(sx * PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_DIMENSION);
    int iy = (int)floor(sy * PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_DIMENSION);
    if (ix < 0) ix = 0;
    if (iy < 0) iy = 0;
    if (ix >= (int)PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_DIMENSION)
        ix = (int)PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_DIMENSION - 1;
    if (iy >= (int)PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_DIMENSION)
        iy = (int)PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_DIMENSION - 1;
    return (size_t)iy * PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_DIMENSION +
        (size_t)ix;
}

bool ProceduralSurfaceFeatureCurveFieldV1_BuildIndex(
    ProceduralSurfaceFeatureCurveFieldV1 *field) {
    size_t index;
    if (!ProceduralSurfaceFeatureCurveFieldV1_Validate(field)) return false;
    field->grid_min_x = field->grid_min_y = INFINITY;
    field->grid_max_x = field->grid_max_y = -INFINITY;
    for (index = 0u; index < field->segment_count; ++index) {
        const ProceduralSurfaceFeatureCurveSegmentV1 *segment =
            &field->segments[index];
        double extent = fmax(segment->width_start, segment->width_end);
        field->grid_min_x = fmin(field->grid_min_x,
            fmin(segment->start.x, segment->end.x) - extent);
        field->grid_min_y = fmin(field->grid_min_y,
            fmin(segment->start.y, segment->end.y) - extent);
        field->grid_max_x = fmax(field->grid_max_x,
            fmax(segment->start.x, segment->end.x) + extent);
        field->grid_max_y = fmax(field->grid_max_y,
            fmax(segment->start.y, segment->end.y) + extent);
    }
    if (field->grid_max_x <= field->grid_min_x)
        field->grid_max_x = field->grid_min_x + 1.0;
    if (field->grid_max_y <= field->grid_min_y)
        field->grid_max_y = field->grid_min_y + 1.0;
    memset(field->grid_counts, 0, sizeof(field->grid_counts));
    field->grid_index_count = 0u;
    for (index = 0u; index < field->segment_count; ++index) {
        const ProceduralSurfaceFeatureCurveSegmentV1 *segment =
            &field->segments[index];
        double extent = fmax(segment->width_start, segment->width_end);
        size_t low = grid_cell(field,
            fmin(segment->start.x, segment->end.x) - extent,
            fmin(segment->start.y, segment->end.y) - extent);
        size_t high = grid_cell(field,
            fmax(segment->start.x, segment->end.x) + extent,
            fmax(segment->start.y, segment->end.y) + extent);
        size_t minimum_x = low % PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_DIMENSION;
        size_t minimum_y = low / PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_DIMENSION;
        size_t maximum_x = high % PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_DIMENSION;
        size_t maximum_y = high / PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_DIMENSION;
        size_t y;
        for (y = minimum_y; y <= maximum_y; ++y) {
            size_t x;
            for (x = minimum_x; x <= maximum_x; ++x) {
                size_t cell = y * PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_DIMENSION + x;
                if (field->grid_counts[cell] >=
                        PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_CELL_CAPACITY ||
                    field->grid_index_count >=
                        sizeof(field->grid_indices) /
                        sizeof(field->grid_indices[0])) return false;
                field->grid_indices[
                    cell * PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_CELL_CAPACITY +
                    field->grid_counts[cell]++] = (uint16_t)index;
                ++field->grid_index_count;
            }
        }
    }
    return true;
}

bool ProceduralSurfaceFeatureCurveFieldV1_CanonicalJson(
    const ProceduralSurfaceFeatureCurveFieldV1 *field,
    char *out, size_t capacity) {
    size_t used = 0u;
    size_t index;
    int wrote;
    if (!out || capacity == 0u ||
        !ProceduralSurfaceFeatureCurveFieldV1_Validate(field)) return false;
#define APPEND(...) do { \
    wrote = snprintf(out + used, capacity - used, __VA_ARGS__); \
    if (wrote < 0 || (size_t)wrote >= capacity - used) return false; \
    used += (size_t)wrote; \
} while (0)
    APPEND("{\"schema\":\"surface_feature_curve_field_v1\",\"schema_version\":1,\"source_mesh_digest_sha256\":\"%s\",\"authoring_digest_sha256\":\"%s\",\"seed\":%llu,\"normal_compatibility_cosine\":%.17g,\"segments\":[",
        field->source_mesh_digest_sha256, field->authoring_digest_sha256,
        (unsigned long long)field->seed,
        field->normal_compatibility_cosine);
    for (index = 0u; index < field->segment_count; ++index) {
        const ProceduralSurfaceFeatureCurveSegmentV1 *s =
            &field->segments[index];
        APPEND("%s{\"curve_id\":%u,\"segment_id\":%u,\"parent_curve_id\":%u,\"source_triangle\":%u,\"barycentric_start\":[%.17g,%.17g,%.17g],\"barycentric_end\":[%.17g,%.17g,%.17g],\"start\":[%.17g,%.17g,%.17g],\"end\":[%.17g,%.17g,%.17g],\"normal_start\":[%.17g,%.17g,%.17g],\"normal_end\":[%.17g,%.17g,%.17g],\"tangent\":[%.17g,%.17g,%.17g],\"width_start\":%.17g,\"width_end\":%.17g,\"depth_start\":%.17g,\"depth_end\":%.17g,\"edge_softness\":%.17g,\"rim_width\":%.17g}",
            index ? "," : "", s->curve_id, s->segment_id,
            s->parent_curve_id, s->source_triangle,
            s->barycentric_start[0], s->barycentric_start[1],
            s->barycentric_start[2], s->barycentric_end[0],
            s->barycentric_end[1], s->barycentric_end[2],
            s->start.x, s->start.y, s->start.z,
            s->end.x, s->end.y, s->end.z,
            s->normal_start.x, s->normal_start.y, s->normal_start.z,
            s->normal_end.x, s->normal_end.y, s->normal_end.z,
            s->tangent.x, s->tangent.y, s->tangent.z,
            s->width_start, s->width_end, s->depth_start, s->depth_end,
            s->edge_softness, s->rim_width);
    }
    APPEND("]}");
#undef APPEND
    return true;
}

bool ProceduralSurfaceFeatureCurveFieldV1_Digest(
    const ProceduralSurfaceFeatureCurveFieldV1 *field,
    char out_digest[PROCEDURAL_SURFACE_FEATURE_CURVE_DIGEST_CAPACITY]) {
    char canonical[PROCEDURAL_SURFACE_FEATURE_CURVE_CANONICAL_CAPACITY];
    return out_digest &&
        ProceduralSurfaceFeatureCurveFieldV1_CanonicalJson(
            field, canonical, sizeof(canonical)) &&
        ray_tracing_sha256_bytes(canonical, strlen(canonical), out_digest);
}

static bool json_vector(json_object *object, const char *key,
                        ProceduralSurfaceFeatureVec3 *out) {
    json_object *array = NULL;
    if (!out || !json_object_object_get_ex(object, key, &array) ||
        !json_object_is_type(array, json_type_array) ||
        json_object_array_length(array) != 3u) return false;
    out->x = json_object_get_double(json_object_array_get_idx(array, 0u));
    out->y = json_object_get_double(json_object_array_get_idx(array, 1u));
    out->z = json_object_get_double(json_object_array_get_idx(array, 2u));
    return finite_vector(*out);
}

static bool json_triplet(json_object *object, const char *key, double out[3]) {
    json_object *array = NULL;
    size_t index;
    if (!out || !json_object_object_get_ex(object, key, &array) ||
        !json_object_is_type(array, json_type_array) ||
        json_object_array_length(array) != 3u) return false;
    for (index = 0u; index < 3u; ++index)
        out[index] = json_object_get_double(
            json_object_array_get_idx(array, index));
    return barycentric_valid(out);
}

static bool json_uint(json_object *object, const char *key, uint32_t *out) {
    json_object *value = NULL;
    int64_t number;
    if (!out || !json_object_object_get_ex(object, key, &value) ||
        !json_object_is_type(value, json_type_int)) return false;
    number = json_object_get_int64(value);
    if (number < 0 || number > UINT32_MAX) return false;
    *out = (uint32_t)number;
    return true;
}

static bool json_double_value(json_object *object, const char *key,
                              double *out) {
    json_object *value = NULL;
    if (!out || !json_object_object_get_ex(object, key, &value) ||
        !(json_object_is_type(value, json_type_double) ||
          json_object_is_type(value, json_type_int))) return false;
    *out = json_object_get_double(value);
    return isfinite(*out);
}

bool ProceduralSurfaceFeatureCurveFieldV1_LoadJsonFile(
    const char *path, ProceduralSurfaceFeatureCurveFieldV1 *out_field) {
    ProceduralSurfaceFeatureCurveFieldV1 field;
    json_object *root = NULL;
    json_object *value = NULL;
    json_object *segments = NULL;
    const char *text;
    size_t index;
    if (!path || !out_field) return false;
    memset(&field, 0, sizeof(field));
    root = json_object_from_file(path);
    if (!root || !json_object_is_type(root, json_type_object) ||
        !json_object_object_get_ex(root, "schema", &value) ||
        strcmp(json_object_get_string(value),
               "surface_feature_curve_field_v1") != 0 ||
        !json_object_object_get_ex(root, "schema_version", &value) ||
        json_object_get_int(value) != 1 ||
        !json_object_object_get_ex(root, "source_mesh_digest_sha256", &value) ||
        !(text = json_object_get_string(value)) ||
        strlen(text) >= sizeof(field.source_mesh_digest_sha256)) goto fail;
    snprintf(field.source_mesh_digest_sha256,
             sizeof(field.source_mesh_digest_sha256), "%s", text);
    if (!json_object_object_get_ex(root, "authoring_digest_sha256", &value) ||
        !(text = json_object_get_string(value)) ||
        strlen(text) >= sizeof(field.authoring_digest_sha256)) goto fail;
    snprintf(field.authoring_digest_sha256,
             sizeof(field.authoring_digest_sha256), "%s", text);
    if (!json_object_object_get_ex(root, "seed", &value) ||
        !json_object_is_type(value, json_type_int)) goto fail;
    field.seed = (uint64_t)json_object_get_int64(value);
    if (!json_double_value(root, "normal_compatibility_cosine",
                           &field.normal_compatibility_cosine) ||
        !json_object_object_get_ex(root, "segments", &segments) ||
        !json_object_is_type(segments, json_type_array)) goto fail;
    field.segment_count = json_object_array_length(segments);
    if (field.segment_count > PROCEDURAL_SURFACE_FEATURE_CURVE_MAX_SEGMENTS)
        goto fail;
    for (index = 0u; index < field.segment_count; ++index) {
        json_object *item = json_object_array_get_idx(segments, index);
        ProceduralSurfaceFeatureCurveSegmentV1 *segment =
            &field.segments[index];
        if (!item ||
            !json_uint(item, "curve_id", &segment->curve_id) ||
            !json_uint(item, "segment_id", &segment->segment_id) ||
            !json_uint(item, "parent_curve_id", &segment->parent_curve_id) ||
            !json_uint(item, "source_triangle", &segment->source_triangle) ||
            !json_triplet(item, "barycentric_start",
                          segment->barycentric_start) ||
            !json_triplet(item, "barycentric_end",
                          segment->barycentric_end) ||
            !json_vector(item, "start", &segment->start) ||
            !json_vector(item, "end", &segment->end) ||
            !json_vector(item, "normal_start", &segment->normal_start) ||
            !json_vector(item, "normal_end", &segment->normal_end) ||
            !json_vector(item, "tangent", &segment->tangent) ||
            !json_double_value(item, "width_start", &segment->width_start) ||
            !json_double_value(item, "width_end", &segment->width_end) ||
            !json_double_value(item, "depth_start", &segment->depth_start) ||
            !json_double_value(item, "depth_end", &segment->depth_end) ||
            !json_double_value(item, "edge_softness",
                               &segment->edge_softness) ||
            !json_double_value(item, "rim_width", &segment->rim_width))
            goto fail;
    }
    if (!ProceduralSurfaceFeatureCurveFieldV1_Validate(&field) ||
        !ProceduralSurfaceFeatureCurveFieldV1_BuildIndex(&field)) goto fail;
    *out_field = field;
    json_object_put(root);
    return true;
fail:
    if (root) json_object_put(root);
    return false;
}

bool ProceduralSurfaceFeatureCurveFieldV1_Sample(
    const ProceduralSurfaceFeatureCurveFieldV1 *field,
    ProceduralSurfaceFeatureVec3 position,
    ProceduralSurfaceFeatureVec3 normal,
    ProceduralSurfaceFeatureCurveSampleV1 *out_sample) {
    size_t cell;
    size_t candidate_count;
    size_t candidate;
    double best = 0.0;
    if (!out_sample || !unit(normal) ||
        !ProceduralSurfaceFeatureCurveFieldV1_Validate(field) ||
        field->grid_index_count == 0u) return false;
    memset(out_sample, 0, sizeof(*out_sample));
    cell = grid_cell(field, position.x, position.y);
    candidate_count = field->grid_counts[cell];
    for (candidate = 0u; candidate < candidate_count; ++candidate) {
        const ProceduralSurfaceFeatureCurveSegmentV1 *segment =
            &field->segments[field->grid_indices[
                cell * PROCEDURAL_SURFACE_FEATURE_CURVE_GRID_CELL_CAPACITY +
                candidate]];
        ProceduralSurfaceFeatureVec3 chord = sub(segment->end, segment->start);
        ProceduralSurfaceFeatureVec3 from_start = sub(position, segment->start);
        ProceduralSurfaceFeatureVec3 center;
        ProceduralSurfaceFeatureVec3 segment_normal;
        ProceduralSurfaceFeatureVec3 binormal;
        ProceduralSurfaceFeatureVec3 offset;
        double chord_length_squared = dot(chord, chord);
        double raw_t = dot(from_start, chord) / chord_length_squared;
        double t = clamp01(raw_t);
        double width;
        double depth;
        double lateral;
        double endpoint_distance;
        double planar_distance;
        double ratio;
        double edge;
        double coverage;
        center = (ProceduralSurfaceFeatureVec3){
            segment->start.x + chord.x * t,
            segment->start.y + chord.y * t,
            segment->start.z + chord.z * t};
        if (!normalize((ProceduralSurfaceFeatureVec3){
                segment->normal_start.x * (1.0 - t) +
                    segment->normal_end.x * t,
                segment->normal_start.y * (1.0 - t) +
                    segment->normal_end.y * t,
                segment->normal_start.z * (1.0 - t) +
                    segment->normal_end.z * t}, &segment_normal) ||
            dot(normal, segment_normal) < field->normal_compatibility_cosine ||
            !normalize(cross(segment_normal, segment->tangent), &binormal))
            continue;
        width = segment->width_start * (1.0 - t) + segment->width_end * t;
        depth = segment->depth_start * (1.0 - t) + segment->depth_end * t;
        offset = sub(position, center);
        lateral = dot(offset, binormal);
        endpoint_distance = (raw_t < 0.0 || raw_t > 1.0)
            ? dot(offset, segment->tangent) : 0.0;
        planar_distance = sqrt(lateral * lateral +
                               endpoint_distance * endpoint_distance);
        ratio = planar_distance / width;
        if (ratio > 1.0) continue;
        ++out_sample->candidates_considered;
        edge = clamp01((1.0 - ratio) / segment->edge_softness);
        coverage = edge * edge * (3.0 - 2.0 * edge);
        if (coverage > best) {
            best = coverage;
            out_sample->coverage = coverage;
            out_sample->interior = clamp01(
                (1.0 - ratio - segment->rim_width) /
                fmax(1.0 - segment->rim_width, 1e-9));
            out_sample->rim = clamp01(
                coverage - out_sample->interior);
            out_sample->signed_depth =
                -depth * (1.0 - ratio * ratio) * coverage;
            out_sample->signed_lateral_distance = lateral;
            out_sample->depth_slope =
                2.0 * depth * lateral / (width * width);
            out_sample->curve_id = segment->curve_id;
            out_sample->segment_id = segment->segment_id;
            out_sample->direction = segment->tangent;
        }
    }
    return true;
}

bool ProceduralSurfaceFeatureCurveSampleV1_ApplyShadingNormal(
    const ProceduralSurfaceFeatureCurveSampleV1 *sample,
    ProceduralSurfaceFeatureVec3 geometric_normal,
    double strength,
    ProceduralSurfaceFeatureVec3 *out_shading_normal) {
    ProceduralSurfaceFeatureVec3 binormal;
    ProceduralSurfaceFeatureVec3 perturbed;
    if (!sample || !out_shading_normal || !unit(geometric_normal) ||
        !unit(sample->direction) || !isfinite(strength) ||
        strength < 0.0 || strength > 1.0 ||
        !isfinite(sample->depth_slope) ||
        !normalize(cross(geometric_normal, sample->direction), &binormal))
        return false;
    perturbed = (ProceduralSurfaceFeatureVec3){
        geometric_normal.x - binormal.x * sample->depth_slope * strength,
        geometric_normal.y - binormal.y * sample->depth_slope * strength,
        geometric_normal.z - binormal.z * sample->depth_slope * strength};
    return normalize(perturbed, out_shading_normal);
}
