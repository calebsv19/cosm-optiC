#include "app/agent_render_request_internal.h"
#include "render/runtime_spectral_profile.h"
#include "app/ray_tracing_sha256.h"

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
static bool integer(json_object *obj, const char *key, int lo, int hi, int *out) {
    json_object *v = json_object_object_get(obj, key);
    if (!json_object_is_type(v, json_type_int)) return false;
    int64_t n = json_object_get_int64(v);
    if (n < lo || n > hi) return false;
    *out = (int)n;
    return true;
}
static bool spectrum_pairs(json_object *a, double pairs[][2], int cap, int *count) {
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
/* Generated emitter spectra use the same piecewise-linear preparation, launch
 * and direct-light quadrature as measured tables. */
static bool emitter_model(json_object *obj, RuntimeSpectralLamp *lamp) {
    const char *kind;
    if (!RayTracingJsonGetString(obj, "type", &kind)) return false;
    double watts;
    if (!number(json_object_object_get(obj, "visible_watts"), 0, 1e12, &watts)) return false;
    double temperature = 0, center[4], sigma[4], weight[4];
    int peaks = 0;
    if (!strcmp(kind, "blackbody")) {
        lamp->spectrum_model="blackbody";
        if (!ONLY(obj, "type", "visible_watts", "kelvin") ||
            !number(json_object_object_get(obj, "kelvin"), 500, 100000, &temperature))
            return false;
    } else if (!strcmp(kind, "gaussian_mixture")) {
        lamp->spectrum_model="gaussian_mixture";
        json_object *a = json_object_object_get(obj, "peaks");
        if (!ONLY(obj, "type", "visible_watts", "peaks") || !json_object_is_type(a, json_type_array) ||
            json_object_array_length(a) < 1 || json_object_array_length(a) > 4)
            return false;
        peaks = (int)json_object_array_length(a);
        for (int j = 0; j < peaks; ++j) {
            json_object *v = json_object_array_get_idx(a, j);
            if (!json_object_is_type(v, json_type_array) || json_object_array_length(v) != 3 ||
                !number(json_object_array_get_idx(v, 0), 380, 780, &center[j]) ||
                !number(json_object_array_get_idx(v, 1), 0.01, 400, &sigma[j]) ||
                !number(json_object_array_get_idx(v, 2), 0, 1e12, &weight[j]))
                return false;
        }
    } else if (!strcmp(kind, "rgb_approximation")) {
        lamp->spectrum_model="rgb_approximation";
        Vec3 rgb;
        if (!ONLY(obj, "type", "visible_watts", "linear_rgb") ||
            !vector(json_object_object_get(obj, "linear_rgb"), &rgb) ||
            fmin(rgb.x, fmin(rgb.y, rgb.z)) < 0 || fmax(rgb.x, fmax(rgb.y, rgb.z)) > 1)
            return false;
        peaks = 3;
        center[0] = 610;
        center[1] = 545;
        center[2] = 455;
        weight[0] = rgb.x;
        weight[1] = rgb.y;
        weight[2] = rgb.z;
        sigma[0] = sigma[1] = sigma[2] = 20;
    } else
        return false;
    return RuntimeSpectralLamp_PrepareModel(lamp, watts, temperature, peaks, center, sigma, weight);
}
static bool bounded_curve(json_object *v, RuntimeSpectralCurve *curve) {
    if (!spectrum_pairs(v, curve->points, SPECTRAL_MAP_KNOTS, &curve->count) || curve->count < 2 ||
        curve->points[0][0] != 380 || curve->points[curve->count - 1][0] != 780)
        return false;
    for (int i = 0; i < curve->count; ++i)
        if (curve->points[i][1] > 1) return false;
    return true;
}
static bool optics(json_object *s, RuntimeSpectralPencil *out) {
    json_object *v;
    out->exterior_ior = 1;
    out->display_sensor = -1;
    if (json_object_object_get_ex(s, "exterior_ior", &v) && !number(v, 1, 3, &out->exterior_ior))
        return false;
    if (json_object_object_get_ex(s, "glass_roughness", &v) && !number(v, 0, 1, &out->glass_roughness))
        return false;
    if (json_object_object_get_ex(s, "receiver_reflectance", &v) && !bounded_curve(v, &out->reflectance))
        return false;
    if (json_object_object_get_ex(s, "receiver_texture", &v)) {
        const char *type;
        if (!RayTracingJsonGetString(s, "receiver_texture", &type) ||
            strcmp(type, "scene_rgb_approximation"))
            return false;
        out->use_scene_texture = true;
    }
    if (json_object_object_get_ex(s, "sensors", &v)) {
        if (!json_object_is_type(v, json_type_array) || json_object_array_length(v) < 1 ||
            json_object_array_length(v) > 4)
            return false;
        out->sensor_count = (int)json_object_array_length(v);
        for (int i = 0; i < out->sensor_count; ++i) {
            json_object *sensor = json_object_array_get_idx(v, i);
            const char *kind;
            if (!ONLY(sensor, "type", "response") || !RayTracingJsonGetString(sensor, "type", &kind) ||
                !bounded_curve(json_object_object_get(sensor, "response"), &out->sensors[i].response))
                return false;
            out->sensors[i].quantum_efficiency = !strcmp(kind, "quantum_efficiency");
            if (!out->sensors[i].quantum_efficiency && strcmp(kind, "radiometric")) return false;
        }
    }
    return !json_object_object_get_ex(s, "display_sensor", &v) ||
           integer(s, "display_sensor", 0, out->sensor_count - 1, &out->display_sensor);
}
static bool parse_map(json_object *obj, RuntimeSpectralMapConfig *m) {
    if (!ONLY(obj, "enabled", "photons_per_source", "bins", "seed", "absorption_per_m", "sources"))
        return false;
    json_object *on = json_object_object_get(obj, "enabled"),
                *lamps = json_object_object_get(obj, "sources");
    int seed;
    if (!json_object_is_type(on, json_type_boolean) ||
        !integer(obj, "photons_per_source", 1, 262144, &m->samples) ||
        !integer(obj, "bins", 1, SPECTRAL_MAP_BINS, &m->bins) || !integer(obj, "seed", 0, INT_MAX, &seed) ||
        !json_object_is_type(lamps, json_type_array) || json_object_array_length(lamps) < 1 ||
        json_object_array_length(lamps) > SPECTRAL_MAP_LIGHTS)
        return false;
    json_object *absorption = json_object_object_get(obj, "absorption_per_m");
    if (json_object_is_type(absorption, json_type_array)) {
        if (!spectrum_pairs(absorption, m->absorption_curve, SPECTRAL_MAP_KNOTS, &m->absorption_knots) ||
            m->absorption_knots < 2 || m->absorption_curve[0][0] != 380 ||
            m->absorption_curve[m->absorption_knots - 1][0] != 780)
            return false;
    } else if (!number(absorption, 0, 1e12, &m->absorption_per_m))
        return false;
    m->enabled = json_object_get_boolean(on);
    m->seed = (unsigned)seed;
    m->source_count = (int)json_object_array_length(lamps);
    for (int i = 0; i < m->source_count; ++i) {
        json_object *lamp = json_object_array_get_idx(lamps, i);
        RuntimeSpectralLamp *l = &m->sources[i];
        double pairs[SPECTRAL_MAP_KNOTS][2];
        if (!ONLY(lamp, "origin_m", "direction", "aperture_m", "lines", "continuum", "spectrum",
                  "angular_radius_deg") ||
            !vector(json_object_object_get(lamp, "origin_m"), &l->origin) ||
            !vector(json_object_object_get(lamp, "direction"), &l->direction) ||
            vec3_length(l->direction) <= 1e-9 ||
            !number(json_object_object_get(lamp, "aperture_m"), 1e-8, 1e4, &l->aperture_m))
            return false;
        l->direction = vec3_normalize(l->direction);
        json_object *angle, *model;
        if (json_object_object_get_ex(lamp, "angular_radius_deg", &angle)) {
            if (!number(angle, 0, 45, &l->angular_radius_rad)) return false;
            l->angular_radius_rad *= M_PI / 180;
        }
        if (json_object_object_get_ex(lamp, "spectrum", &model)) {
            if (json_object_object_get_ex(lamp, "lines", NULL) ||
                json_object_object_get_ex(lamp, "continuum", NULL) || !emitter_model(model, l))
                return false;
            continue;
        }
        if (!spectrum_pairs(json_object_object_get(lamp, "lines"), pairs, RUNTIME_SPECTRAL_PENCIL_MAX_LINES,
                            &l->nlines))
            return false;

        for (int j = 0; j < l->nlines; ++j)
            l->lines[j] = (RuntimeSpectralLine){pairs[j][0], pairs[j][1]};
        if (!spectrum_pairs(json_object_object_get(lamp, "continuum"), pairs, SPECTRAL_MAP_KNOTS,
                            &l->nknots) ||
            l->nknots == 1 || (!l->nlines && !l->nknots))
            return false;
        for (int j = 0; j < l->nknots; ++j)
            l->knots[j] = (RuntimeSpectralKnot){pairs[j][0], pairs[j][1]};
    }
    const char *json = json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN);
    return json && ray_tracing_sha256_bytes(json, strlen(json), m->config_hash);
}

bool agent_render_request_parse_spectral(json_object *root, RuntimeSpectralPencil *out) {
    json_object *s = json_object_object_get(root, "spectral");
    json_object *r = json_object_object_get(root, "render");
    json_object *inspection = json_object_object_get(root, "inspection");
    json_object *progress = json_object_object_get(root, "progress");
    json_object *map = NULL;
    bool mapping = json_object_object_get_ex(s, "surface_map", &map);
    if (!ONLY(root, "schema_version", "scene", "spectral", "render", "inspection", "progress", "output",
              "run_id") ||
        !ONLY(json_object_object_get(root, "scene"), "runtime_scene_path", "camera_id") ||
        !ONLY(json_object_object_get(root, "output"), "root", "overwrite") ||
        (progress && !ONLY(progress, "summary_path", "progress_path")) ||
        (inspection && !ONLY(inspection, "camera_position", "camera_look_at", "camera_zoom")) ||
        !ONLY(r, "integrator_3d", "width", "height") ||
        !(mapping ? ONLY(s, "glass_id", "receiver_id", "glass", "constant_ior", "receiver_radius_m",
                         "exposure", "surface_map", "exterior_ior", "glass_roughness",
                         "receiver_reflectance", "receiver_texture", "sensors", "display_sensor")
                  : ONLY(s, "glass_id", "receiver_id", "glass", "constant_ior", "origin_m", "direction",
                         "receiver_radius_m", "exposure", "lines")))
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
        (!mapping && (!vector(json_object_object_get(s, "origin_m"), &out->origin_m) ||
                      !vector(json_object_object_get(s, "direction"), &out->direction) ||
                      vec3_length(out->direction) < 1e-12)) ||
        !number(json_object_object_get(s, "receiver_radius_m"), 1e-8, 1e4, &out->receiver_radius_m) ||
        !number(json_object_object_get(s, "exposure"), 1e-20, 1e12, &out->exposure))
        return false;
    if (!mapping) out->direction = vec3_normalize(out->direction);
    out->use_n_bk7 = strcmp(glass, "schott_n_bk7") == 0;
    json_object *ior = json_object_object_get(s, "constant_ior");
    if (out->use_n_bk7 ? ior != NULL
                       : (strcmp(glass, "constant") || !number(ior, 1, 5, &out->constant_ior)))
        return false;
    if (mapping) return optics(s, out) && parse_map(map, &out->map);
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
