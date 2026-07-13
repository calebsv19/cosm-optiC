#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "app/scene_project_render_request.h"

#include <errno.h>
#include <json-c/json.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SCENE_PROJECT_RENDER_REQUEST_SCHEMA "ray_tracing_agent_render_request_v1"
#define SCENE_PROJECT_RENDER_REQUEST_DEFAULT "ray_tracing/render_request.json"
#define SCENE_PROJECT_PHYSICS_CACHE_DEFAULT "physics_sim/active_cache_manifest.json"
#define SCENE_PROJECT_OUTPUT_ROOT_DEFAULT "ray_tracing/frames_temp"

static void set_error(char *error, size_t error_size, const char *format, ...) {
    va_list args;
    if (!error || error_size == 0u) return;
    va_start(args, format);
    vsnprintf(error, error_size, format, args);
    va_end(args);
}

static bool copy_string(char *out, size_t out_size, const char *value) {
    if (!out || out_size == 0u || !value) return false;
    return snprintf(out, out_size, "%s", value) < (int)out_size;
}

static bool path_is_file(const char *path) {
    struct stat st;
    return path && path[0] && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static bool path_is_dir(const char *path) {
    struct stat st;
    return path && path[0] && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool join_path(const char *root, const char *relpath, char *out, size_t out_size) {
    if (!root || !root[0] || !relpath || !relpath[0] || !out || out_size == 0u) return false;
    return snprintf(out, out_size, "%s/%s", root, relpath) < (int)out_size;
}

static bool dirname_of(const char *path, char *out, size_t out_size) {
    const char *slash;
    size_t length;
    if (!path || !path[0] || !out || out_size == 0u) return false;
    slash = strrchr(path, '/');
    if (!slash) return copy_string(out, out_size, ".");
    length = slash == path ? 1u : (size_t)(slash - path);
    if (length >= out_size) return false;
    memcpy(out, path, length);
    out[length] = '\0';
    return true;
}

static bool portable_project_relpath(const char *path) {
    const char *cursor;
    if (!path || !path[0] || path[0] == '/') return false;
    cursor = path;
    while (*cursor) {
        const char *end = strchr(cursor, '/');
        size_t length = end ? (size_t)(end - cursor) : strlen(cursor);
        if (length == 0u || (length == 1u && cursor[0] == '.') ||
            (length == 2u && cursor[0] == '.' && cursor[1] == '.')) {
            return false;
        }
        if (!end) break;
        cursor = end + 1;
    }
    return true;
}

static bool path_is_within_root(const char *root, const char *path) {
    size_t root_length;
    if (!root || !path) return false;
    root_length = strlen(root);
    return strncmp(root, path, root_length) == 0 &&
           (path[root_length] == '\0' || path[root_length] == '/');
}

static bool existing_ancestor_is_within_root(const char *root, const char *path) {
    char cursor[PATH_MAX];
    char canonical[PATH_MAX];
    if (!root || !path || !dirname_of(path, cursor, sizeof(cursor))) return false;
    while (!path_is_dir(cursor)) {
        char parent[PATH_MAX];
        if (!dirname_of(cursor, parent, sizeof(parent)) || strcmp(parent, cursor) == 0) {
            return false;
        }
        if (!copy_string(cursor, sizeof(cursor), parent)) return false;
    }
    return realpath(cursor, canonical) && path_is_within_root(root, canonical);
}

static bool mkdir_parents(const char *path) {
    char work[PATH_MAX];
    if (!path || !path[0] || !copy_string(work, sizeof(work), path)) return false;
    for (char *cursor = work + 1; *cursor; ++cursor) {
        if (*cursor != '/') continue;
        *cursor = '\0';
        if (mkdir(work, 0700) != 0 && errno != EEXIST) return false;
        *cursor = '/';
    }
    return mkdir(work, 0700) == 0 || errno == EEXIST;
}

static const char *json_string_member(json_object *owner, const char *key) {
    json_object *value = NULL;
    if (!owner || !key || !json_object_object_get_ex(owner, key, &value) ||
        !json_object_is_type(value, json_type_string)) {
        return NULL;
    }
    return json_object_get_string(value);
}

static int json_int_member(json_object *owner, const char *key, int fallback) {
    json_object *value = NULL;
    if (!owner || !json_object_object_get_ex(owner, key, &value) ||
        !json_object_is_type(value, json_type_int)) {
        return fallback;
    }
    return json_object_get_int(value);
}

static json_object *json_object_member(json_object *owner, const char *key) {
    json_object *value = NULL;
    if (!owner || !json_object_object_get_ex(owner, key, &value) ||
        !json_object_is_type(value, json_type_object)) {
        return NULL;
    }
    return value;
}

static const char *manifest_render_request_relpath(json_object *manifest) {
    json_object *active = json_object_member(manifest, "active");
    json_object *ray = json_object_member(manifest, "ray_tracing");
    const char *value = json_string_member(manifest, "active_render_request");
    if (!value && active) value = json_string_member(active, "render_request");
    if (!value && ray) value = json_string_member(ray, "active_render_request");
    return value;
}

static const char *manifest_physics_cache_relpath(json_object *manifest) {
    json_object *active = json_object_member(manifest, "active");
    json_object *physics = json_object_member(manifest, "physics");
    const char *value = json_string_member(manifest, "active_cache");
    if (!value && active) value = json_string_member(active, "physics_cache");
    if (!value && physics) value = json_string_member(physics, "active_cache");
    return value;
}

static bool load_json_object(const char *path, json_object **out, char *error, size_t error_size) {
    json_object *root;
    if (!path || !out) return false;
    root = json_object_from_file(path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        set_error(error, error_size, "failed to parse JSON object: %s", path);
        return false;
    }
    *out = root;
    return true;
}

static bool resolve_request_file(const char *path,
                                 char *out,
                                 size_t out_size,
                                 bool require_exists) {
    char resolved[PATH_MAX];
    char parent[PATH_MAX];
    char parent_real[PATH_MAX];
    const char *basename;
    if (require_exists) {
        return realpath(path, resolved) && copy_string(out, out_size, resolved);
    }
    if (!dirname_of(path, parent, sizeof(parent)) || !realpath(parent, parent_real)) return false;
    basename = strrchr(path, '/');
    basename = basename ? basename + 1 : path;
    return join_path(parent_real, basename, out, out_size);
}

static bool request_relative_project_path(const char *request_relpath,
                                          const char *project_relpath,
                                          char *out,
                                          size_t out_size) {
    char request_dir[PATH_MAX];
    size_t depth = 0u;
    size_t used = 0u;
    if (!portable_project_relpath(request_relpath) ||
        !portable_project_relpath(project_relpath) ||
        !dirname_of(request_relpath, request_dir, sizeof(request_dir))) {
        return false;
    }
    if (strcmp(request_dir, ".") != 0) {
        depth = 1u;
        for (const char *cursor = request_dir; *cursor; ++cursor) {
            if (*cursor == '/') ++depth;
        }
    }
    out[0] = '\0';
    for (size_t i = 0u; i < depth; ++i) {
        int written = snprintf(out + used, out_size - used, "../");
        if (written < 0 || (size_t)written >= out_size - used) return false;
        used += (size_t)written;
    }
    return snprintf(out + used, out_size - used, "%s", project_relpath) <
           (int)(out_size - used);
}

static bool request_relative_output_path(const char *request_relpath,
                                         const char *project_output_relpath,
                                         char *out,
                                         size_t out_size) {
    char request_dir[PATH_MAX];
    size_t dir_length;
    if (!portable_project_relpath(request_relpath) ||
        !portable_project_relpath(project_output_relpath) ||
        !dirname_of(request_relpath, request_dir, sizeof(request_dir))) {
        return false;
    }
    dir_length = strcmp(request_dir, ".") == 0 ? 0u : strlen(request_dir);
    if (dir_length > 0u && strncmp(project_output_relpath, request_dir, dir_length) == 0 &&
        project_output_relpath[dir_length] == '/') {
        return copy_string(out, out_size, project_output_relpath + dir_length + 1u);
    }
    return request_relative_project_path(request_relpath, project_output_relpath, out, out_size);
}

static bool read_request_state(RayTracingSceneProjectRenderRequest *request,
                               char *error,
                               size_t error_size) {
    json_object *root = NULL;
    json_object *render;
    json_object *simulation;
    json_object *project;
    const char *schema;
    const char *value;
    if (!request->request_exists) return true;
    if (!load_json_object(request->request_path, &root, error, error_size)) return false;
    schema = json_string_member(root, "schema_version");
    if (!schema || strcmp(schema, SCENE_PROJECT_RENDER_REQUEST_SCHEMA) != 0) {
        set_error(error, error_size, "unsupported render request schema: %s", request->request_path);
        json_object_put(root);
        return false;
    }
    render = json_object_member(root, "render");
    simulation = json_object_member(root, "simulation_frames");
    project = json_object_member(root, "scene_project");
    request->simulation_start_frame = json_int_member(simulation, "start",
        json_int_member(render, "start_frame", 0));
    request->simulation_frame_count = json_int_member(simulation, "count",
        json_int_member(render, "frame_count", 1));
    request->simulation_frame_stride = json_int_member(simulation, "stride", 1);
    value = simulation ? json_string_member(simulation, "cache_manifest") : NULL;
    if (value && portable_project_relpath(value)) {
        copy_string(request->physics_cache_relpath, sizeof(request->physics_cache_relpath), value);
    }
    value = project ? json_string_member(project, "output_root") : NULL;
    if (value && portable_project_relpath(value)) {
        copy_string(request->output_root_relpath, sizeof(request->output_root_relpath), value);
    }
    json_object_put(root);
    if (request->simulation_start_frame < 0 || request->simulation_frame_count <= 0 ||
        request->simulation_frame_stride <= 0) {
        set_error(error, error_size, "invalid simulation frame window in %s", request->request_path);
        return false;
    }
    return true;
}

bool ray_tracing_scene_project_render_request_resolve(
    const char *runtime_scene_path,
    const char *explicit_request_path,
    RayTracingSceneProjectRenderRequest *out_request,
    char *error,
    size_t error_size) {
    RayTracingSceneProjectRenderRequest request;
    char runtime_real[PATH_MAX];
    char project_candidate[PATH_MAX];
    char manifest_path[PATH_MAX];
    char request_candidate[PATH_MAX];
    json_object *manifest = NULL;
    const char *render_relpath = NULL;
    const char *cache_relpath = NULL;

    if (error && error_size) error[0] = '\0';
    if (!runtime_scene_path || !runtime_scene_path[0] || !out_request ||
        !realpath(runtime_scene_path, runtime_real) || !path_is_file(runtime_real)) {
        set_error(error, error_size, "runtime scene does not exist: %s",
                  runtime_scene_path ? runtime_scene_path : "<null>");
        return false;
    }
    memset(&request, 0, sizeof(request));
    request.simulation_frame_count = 1;
    request.simulation_frame_stride = 1;
    copy_string(request.runtime_scene_path, sizeof(request.runtime_scene_path), runtime_real);
    if (!dirname_of(runtime_real, project_candidate, sizeof(project_candidate))) {
        set_error(error, error_size, "failed to resolve runtime scene directory");
        return false;
    }
    if (!join_path(project_candidate, "scene_project.json", manifest_path, sizeof(manifest_path))) {
        set_error(error, error_size, "scene project manifest path is too long");
        return false;
    }
    request.project_backed = path_is_file(manifest_path);
    if (request.project_backed) {
        char project_real[PATH_MAX];
        if (!realpath(project_candidate, project_real) ||
            !copy_string(request.project_root, sizeof(request.project_root), project_real) ||
            !load_json_object(manifest_path, &manifest, error, error_size)) {
            return false;
        }
        render_relpath = manifest_render_request_relpath(manifest);
        cache_relpath = manifest_physics_cache_relpath(manifest);
        if (!render_relpath) render_relpath = SCENE_PROJECT_RENDER_REQUEST_DEFAULT;
        if (!cache_relpath) cache_relpath = SCENE_PROJECT_PHYSICS_CACHE_DEFAULT;
        if (!portable_project_relpath(render_relpath)) {
            set_error(error, error_size, "unsafe scene-project render request path: %s", render_relpath);
            json_object_put(manifest);
            return false;
        }
        if (!portable_project_relpath(cache_relpath)) {
            set_error(error, error_size, "unsafe scene-project physics cache path: %s", cache_relpath);
            json_object_put(manifest);
            return false;
        }
        copy_string(request.request_relpath, sizeof(request.request_relpath), render_relpath);
        copy_string(request.physics_cache_relpath, sizeof(request.physics_cache_relpath), cache_relpath);
        copy_string(request.output_root_relpath,
                    sizeof(request.output_root_relpath),
                    SCENE_PROJECT_OUTPUT_ROOT_DEFAULT);
        json_object_put(manifest);
    }

    if (explicit_request_path && explicit_request_path[0]) {
        if (!resolve_request_file(explicit_request_path,
                                  request.request_path,
                                  sizeof(request.request_path),
                                  true)) {
            set_error(error, error_size, "explicit render request does not exist: %s", explicit_request_path);
            return false;
        }
        request.request_exists = true;
        request.project_owned = request.project_backed &&
            path_is_within_root(request.project_root, request.request_path);
        if (request.project_owned) {
            const char *relative = request.request_path + strlen(request.project_root) + 1u;
            copy_string(request.request_relpath, sizeof(request.request_relpath), relative);
        }
    } else if (request.project_backed) {
        if (!join_path(request.project_root,
                       request.request_relpath,
                       request_candidate,
                       sizeof(request_candidate))) {
            set_error(error, error_size, "project render request path is too long");
            return false;
        }
        request.request_exists = path_is_file(request_candidate);
        if (request.request_exists) {
            if (!resolve_request_file(request_candidate,
                                      request.request_path,
                                      sizeof(request.request_path),
                                      true)) {
                set_error(error, error_size, "failed to resolve project render request path");
                return false;
            }
        } else if (!copy_string(request.request_path,
                                sizeof(request.request_path),
                                request_candidate)) {
            set_error(error, error_size, "project render request path is too long");
            return false;
        }
        request.project_owned = path_is_within_root(request.project_root, request.request_path);
        if (!request.project_owned ||
            !existing_ancestor_is_within_root(request.project_root, request.request_path)) {
            set_error(error, error_size, "scene-project render request escapes project root");
            return false;
        }
    } else {
        char runtime_dir[PATH_MAX];
        const char *names[] = {"render_request.json", "ray_tracing_request.json"};
        if (!dirname_of(runtime_real, runtime_dir, sizeof(runtime_dir))) return false;
        for (size_t i = 0u; i < sizeof(names) / sizeof(names[0]); ++i) {
            if (join_path(runtime_dir, names[i], request_candidate, sizeof(request_candidate)) &&
                path_is_file(request_candidate) &&
                resolve_request_file(request_candidate,
                                     request.request_path,
                                     sizeof(request.request_path),
                                     true)) {
                request.request_exists = true;
                break;
            }
        }
    }
    if (!read_request_state(&request, error, error_size)) return false;
    *out_request = request;
    return true;
}

static void replace_object_member(json_object *owner, const char *key, json_object *value) {
    json_object_object_del(owner, key);
    json_object_object_add(owner, key, value);
}

bool ray_tracing_scene_project_render_request_write(
    RayTracingSceneProjectRenderRequest *request,
    int simulation_start_frame,
    int simulation_frame_count,
    int simulation_frame_stride,
    char *error,
    size_t error_size) {
    json_object *root = NULL;
    json_object *scene;
    json_object *render;
    json_object *output;
    json_object *progress;
    json_object *volume;
    json_object *simulation;
    json_object *project;
    char request_dir[PATH_MAX];
    char runtime_request_relpath[PATH_MAX];
    char output_request_relpath[PATH_MAX];
    char summary_request_relpath[PATH_MAX];
    char progress_request_relpath[PATH_MAX];
    char tmp_path[PATH_MAX];
    FILE *file;
    const char *json_text;
    bool write_ok;

    if (error && error_size) error[0] = '\0';
    if (!request || !request->project_backed || !request->project_owned ||
        !portable_project_relpath(request->request_relpath)) {
        set_error(error, error_size, "render request write requires a project-owned path");
        return false;
    }
    if (simulation_start_frame < 0 || simulation_frame_count <= 0 ||
        simulation_frame_stride <= 0) {
        set_error(error, error_size, "simulation frame window requires start>=0, count>0, stride>0");
        return false;
    }
    if (!request_relative_project_path(request->request_relpath,
                                       "scene_runtime.json",
                                       runtime_request_relpath,
                                       sizeof(runtime_request_relpath)) ||
        !request_relative_output_path(request->request_relpath,
                                      request->output_root_relpath,
                                      output_request_relpath,
                                      sizeof(output_request_relpath)) ||
        snprintf(summary_request_relpath,
                 sizeof(summary_request_relpath),
                 "%s/render_summary.json",
                 output_request_relpath) >= (int)sizeof(summary_request_relpath) ||
        snprintf(progress_request_relpath,
                 sizeof(progress_request_relpath),
                 "%s/render_progress.json",
                 output_request_relpath) >= (int)sizeof(progress_request_relpath)) {
        set_error(error, error_size, "failed to build portable request paths");
        return false;
    }
    if (request->request_exists) {
        if (!load_json_object(request->request_path, &root, error, error_size)) return false;
    } else {
        root = json_object_new_object();
    }
    replace_object_member(root, "schema_version", json_object_new_string(SCENE_PROJECT_RENDER_REQUEST_SCHEMA));
    if (!json_string_member(root, "run_id")) {
        json_object_object_add(root, "run_id", json_object_new_string("scene-project-render"));
    }

    scene = json_object_member(root, "scene");
    if (!scene) {
        scene = json_object_new_object();
        json_object_object_add(root, "scene", scene);
    }
    replace_object_member(scene, "runtime_scene_path", json_object_new_string(runtime_request_relpath));

    render = json_object_member(root, "render");
    if (!render) {
        render = json_object_new_object();
        json_object_object_add(root, "render", render);
        json_object_object_add(render, "width", json_object_new_int(160));
        json_object_object_add(render, "height", json_object_new_int(96));
        json_object_object_add(render, "temporal_frames", json_object_new_int(1));
        json_object_object_add(render, "integrator_3d", json_object_new_string("direct_light"));
    }
    replace_object_member(render, "start_frame", json_object_new_int(simulation_start_frame));
    replace_object_member(render, "frame_count", json_object_new_int(simulation_frame_count));

    volume = json_object_member(root, "volume");
    if (!volume) {
        volume = json_object_new_object();
        json_object_object_add(root, "volume", volume);
        json_object_object_add(volume, "enabled", json_object_new_boolean(false));
    }

    output = json_object_member(root, "output");
    if (!output) {
        output = json_object_new_object();
        json_object_object_add(root, "output", output);
    }
    replace_object_member(output, "root", json_object_new_string(output_request_relpath));
    {
        json_object *overwrite_value = NULL;
        if (!json_object_object_get_ex(output, "overwrite", &overwrite_value)) {
        json_object_object_add(output, "overwrite", json_object_new_boolean(false));
        }
    }

    progress = json_object_member(root, "progress");
    if (!progress) {
        progress = json_object_new_object();
        json_object_object_add(root, "progress", progress);
    }
    replace_object_member(progress, "summary_path", json_object_new_string(summary_request_relpath));
    replace_object_member(progress, "progress_path", json_object_new_string(progress_request_relpath));

    simulation = json_object_member(root, "simulation_frames");
    if (!simulation) {
        simulation = json_object_new_object();
        json_object_object_add(root, "simulation_frames", simulation);
    }
    replace_object_member(simulation, "cache_manifest",
                          json_object_new_string(request->physics_cache_relpath));
    replace_object_member(simulation, "start", json_object_new_int(simulation_start_frame));
    replace_object_member(simulation, "count", json_object_new_int(simulation_frame_count));
    replace_object_member(simulation, "stride", json_object_new_int(simulation_frame_stride));

    project = json_object_member(root, "scene_project");
    if (!project) {
        project = json_object_new_object();
        json_object_object_add(root, "scene_project", project);
    }
    replace_object_member(project, "project_root", json_object_new_string("."));
    replace_object_member(project, "runtime_scene", json_object_new_string("scene_runtime.json"));
    replace_object_member(project, "physics_cache",
                          json_object_new_string(request->physics_cache_relpath));
    replace_object_member(project, "render_request",
                          json_object_new_string(request->request_relpath));
    replace_object_member(project, "output_root",
                          json_object_new_string(request->output_root_relpath));

    if (!dirname_of(request->request_path, request_dir, sizeof(request_dir)) ||
        (!path_is_dir(request_dir) && !mkdir_parents(request_dir)) ||
        snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.%ld", request->request_path, (long)getpid()) >=
            (int)sizeof(tmp_path)) {
        set_error(error, error_size, "failed to prepare render request directory");
        json_object_put(root);
        return false;
    }
    file = fopen(tmp_path, "wb");
    if (!file) {
        set_error(error, error_size, "failed to open render request temp file: %s", tmp_path);
        json_object_put(root);
        return false;
    }
    json_text = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
    write_ok = fprintf(file, "%s\n", json_text) >= 0;
    if (fclose(file) != 0) write_ok = false;
    if (!write_ok || rename(tmp_path, request->request_path) != 0) {
        unlink(tmp_path);
        set_error(error, error_size, "failed to write render request: %s", request->request_path);
        json_object_put(root);
        return false;
    }
    json_object_put(root);
    request->request_exists = true;
    request->simulation_start_frame = simulation_start_frame;
    request->simulation_frame_count = simulation_frame_count;
    request->simulation_frame_stride = simulation_frame_stride;
    return true;
}
