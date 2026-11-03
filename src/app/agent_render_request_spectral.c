#include "app/agent_render_request_internal.h"
#include "render/runtime_spectral_profile.h"

/* Capability allowlists are specific to the v2 calibration request. */
static bool keys(json_object *obj, const char *const allowed[]) {
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

static bool number(json_object *value, double min, double max, double *out) {
    if (!json_object_is_type(value, json_type_double) && !json_object_is_type(value, json_type_int))
        return false;
    double n = json_object_get_double(value);
    if (!isfinite(n) || n < min || n > max) return false;
    *out = n;
    return true;
}
static bool vector(json_object *a, Vec3 *out) {
    return json_object_is_type(a, json_type_array) && json_object_array_length(a) == 3 &&
           number(json_object_array_get_idx(a, 0), -1e6, 1e6, &out->x) &&
           number(json_object_array_get_idx(a, 1), -1e6, 1e6, &out->y) &&
           number(json_object_array_get_idx(a, 2), -1e6, 1e6, &out->z);
}
static bool object_id(json_object *obj, const char *key, char out[64]) {
    json_object *v = json_object_object_get(obj, key);
    const char *s = NULL;
    return RayTracingJsonGetString(obj, key, &s) && *s &&
           strlen(s) == (size_t)json_object_get_string_len(v) && RayTracingCopyString(out, 64, s);
}
bool agent_render_request_parse_spectral(json_object *root, RuntimeSpectralPencil *out) {
    json_object *s = json_object_object_get(root, "spectral");
    json_object *r = json_object_object_get(root, "render");
    json_object *inspection = json_object_object_get(root, "inspection");
    json_object *progress = json_object_object_get(root, "progress");
    if (!ONLY(root, "schema_version", "scene", "spectral", "render", "inspection", "progress", "output",
              "run_id") ||
        !ONLY(json_object_object_get(root, "scene"), "runtime_scene_path", "camera_id") ||
        !ONLY(json_object_object_get(root, "output"), "root", "overwrite") ||
        (progress && !ONLY(progress, "summary_path", "progress_path")) ||
        (inspection && !ONLY(inspection, "camera_position", "camera_look_at", "camera_zoom")) ||
        !ONLY(r, "integrator_3d", "width", "height") ||
        !ONLY(s, "glass_id", "receiver_id", "glass", "constant_ior", "origin_m", "direction",
              "receiver_radius_m", "exposure", "lines"))
        return false;

    /* The common RGB parser accepts partial vectors and coerces dimensions.
     * The calibration request requires finite camera coordinates and a bounded film. */
    const char *dims[] = {"width", "height"};
    for (size_t i = 0; i < 2; ++i) {
        json_object *v = json_object_object_get(r, dims[i]);
        if (!json_object_is_type(v, json_type_int) || json_object_get_int64(v) < 1 ||
            json_object_get_int64(v) > 4096)
            return false;
    }
    const char *vectors[] = {"camera_position", "camera_look_at"};
    double ignored;
    for (size_t i = 0; inspection && i < 2; ++i) {
        json_object *v = NULL;
        if (!json_object_object_get_ex(inspection, vectors[i], &v)) continue;
        if (!ONLY(v, "x", "y", "z") || !number(json_object_object_get(v, "x"), -1e6, 1e6, &ignored) ||
            !number(json_object_object_get(v, "y"), -1e6, 1e6, &ignored) ||
            !number(json_object_object_get(v, "z"), -1e6, 1e6, &ignored))
            return false;
    }
    json_object *zoom = NULL;
    if (inspection && json_object_object_get_ex(inspection, "camera_zoom", &zoom) &&
        !number(zoom, 0.01, 1000, &ignored))
        return false;
    const char *glass = NULL;
    if (!object_id(s, "glass_id", out->glass_id) || !object_id(s, "receiver_id", out->receiver_id) ||
        !strcmp(out->glass_id, out->receiver_id) || !RayTracingJsonGetString(s, "glass", &glass) ||
        !vector(json_object_object_get(s, "origin_m"), &out->origin_m) ||
        !vector(json_object_object_get(s, "direction"), &out->direction) ||
        vec3_length(out->direction) < 1e-12 ||
        !number(json_object_object_get(s, "receiver_radius_m"), 1e-8, 1e4, &out->receiver_radius_m) ||
        !number(json_object_object_get(s, "exposure"), 1e-20, 1e12, &out->exposure))
        return false;
    out->direction = vec3_normalize(out->direction);
    out->use_n_bk7 = strcmp(glass, "schott_n_bk7") == 0;
    json_object *ior = json_object_object_get(s, "constant_ior");
    if (out->use_n_bk7 ? ior != NULL
                       : (strcmp(glass, "constant") || !number(ior, 1, 5, &out->constant_ior)))
        return false;
    json_object *a = json_object_object_get(s, "lines");
    if (!json_object_is_type(a, json_type_array)) return false;
    size_t count = json_object_array_length(a);
    if (!count || count > RUNTIME_SPECTRAL_PENCIL_MAX_LINES) return false;
    for (size_t i = 0; i < count; ++i) {
        RuntimeSpectralPencilLine *line = &out->lines[i];
        json_object *l = json_object_array_get_idx(a, i);
        if (!ONLY(l, "vacuum_nm", "watts", "absorption_per_m") ||
            !number(json_object_object_get(l, "vacuum_nm"), RUNTIME_SPECTRAL_MIN_NM,
                    RUNTIME_SPECTRAL_MAX_NM, &line->vacuum_nm) ||
            !number(json_object_object_get(l, "watts"), 0, 1e12, &line->watts) ||
            !number(json_object_object_get(l, "absorption_per_m"), 0, 1e12, &line->absorption_per_m))
            return false;
        if (i && line->vacuum_nm <= out->lines[i - 1].vacuum_nm) return false;
    }
    out->count = (int)count;
    return true;
}
