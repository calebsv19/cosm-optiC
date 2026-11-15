#ifndef SPECTRAL_PROFILE_JSON_FIELDS_H
#define SPECTRAL_PROFILE_JSON_FIELDS_H
#include "render/runtime_spectral_scene_profile.h"
#include "app/ray_tracing_request_utils.h"
#include <limits.h>
#include <math.h>
#include <string.h>
static inline bool keys(json_object *obj, const char *const allowed[]) {
    if (!json_object_is_type(obj, json_type_object)) return false;
    json_object_object_foreach(obj, key, value) {
        (void)value;
        size_t i = 0;
        while (allowed[i] && strcmp(key, allowed[i]))
            ++i;
        if (!allowed[i]) return false;
    }
    return true;
}
#define ONLY(obj, ...) keys(obj, (const char *const[]){__VA_ARGS__, NULL})

static inline bool number(json_object *value, double min, double max, double *out) {
    if (!json_object_is_type(value, json_type_double) && !json_object_is_type(value, json_type_int))
        return false;
    double n = json_object_get_double(value);
    if (!isfinite(n) || n < min || n > max) return false;
    *out = n;
    return true;
}
static inline bool vector(json_object *a, Vec3 *out) {
    return json_object_is_type(a, json_type_array) && json_object_array_length(a) == 3 &&
           number(json_object_array_get_idx(a, 0), -1e6, 1e6, &out->x) &&
           number(json_object_array_get_idx(a, 1), -1e6, 1e6, &out->y) &&
           number(json_object_array_get_idx(a, 2), -1e6, 1e6, &out->z);
}
static inline bool object_id(json_object *obj, const char *key, char out[64]) {
    json_object *v = json_object_object_get(obj, key);
    const char *s = NULL;
    return RayTracingJsonGetString(obj, key, &s) && *s &&
           strlen(s) == (size_t)json_object_get_string_len(v) && RayTracingCopyString(out, 64, s);
}
static inline bool integer(json_object *obj, const char *key, int lo, int hi, int *out) {
    json_object *v = json_object_object_get(obj, key);
    if (!json_object_is_type(v, json_type_int)) return false;
    int64_t n = json_object_get_int64(v);
    if (n < lo || n > hi) return false;
    *out = (int)n;
    return true;
}
static inline bool spectrum_pairs(json_object *a, double pairs[][2], int cap, int *count) {
    if (!json_object_is_type(a, json_type_array) || json_object_array_length(a) > (size_t)cap) return false;
    *count = (int)json_object_array_length(a);
    for (int i = 0; i < *count; ++i) {
        json_object *v = json_object_array_get_idx(a, i);
        if (!json_object_is_type(v, json_type_array) || json_object_array_length(v) != 2 ||
            !number(json_object_array_get_idx(v, 0), 380, 780, &pairs[i][0]) ||
            !number(json_object_array_get_idx(v, 1), 0, 1e12, &pairs[i][1]) ||
            (i && pairs[i][0] <= pairs[i - 1][0]))
            return false;
    }
    return true;
}
static inline bool bounded_curve(json_object *v, RuntimeSpectralCurve *curve) {
    if (!spectrum_pairs(v, curve->points, SPECTRAL_MAP_KNOTS, &curve->count) || curve->count < 2 ||
        curve->points[0][0] != 380 || curve->points[curve->count - 1][0] != 780)
        return false;
    for (int i = 0; i < curve->count; ++i)
        if (curve->points[i][1] > 1) return false;
    return true;
}

#endif
