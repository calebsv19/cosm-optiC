#include "app/agent_render_request_internal.h"
#include "spectral_profile_json_fields.h"

bool agent_render_request_parse_spectral(json_object *root, const char *dir, RuntimeSpectralPencil *out) {
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
        !ONLY(r, "integrator_3d", "width", "height", "temporal_frames", "denoise_enabled",
              "use_tiled_renderer", "frame_count"))
        return false;

    /* The common RGB parser accepts partial vectors and coerces dimensions.
     * The calibration request requires finite camera coordinates and a bounded film. */
    /* Accept an explicit request for the same single, unfiltered pass as omission. */
    const char *off_fields[] = {"denoise_enabled", "use_tiled_renderer"};
    for (size_t i = 0; i < 2; ++i) {
        json_object *v;
        if (json_object_object_get_ex(r, off_fields[i], &v) &&
            (!json_object_is_type(v, json_type_boolean) || json_object_get_boolean(v))) return false;
    }
    int ignored_count;
    const char *single_fields[] = {"frame_count", "temporal_frames"};
    for (size_t i = 0; i < 2; ++i) {
        if (json_object_object_get_ex(r, single_fields[i], NULL) &&
            !integer(r, single_fields[i], 1, 1, &ignored_count)) return false;
    }
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
    return RuntimeSpectralProfile_ParseFilm(s,dir,
        json_object_get_int(json_object_object_get(r,"width")),
        json_object_get_int(json_object_object_get(r,"height")),out);
}
