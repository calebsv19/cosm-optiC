#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <json-c/json.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app/scene_project_render_request.h"
#include "test_support.h"

static bool make_dir(const char *path) {
    return mkdir(path, 0700) == 0;
}

static void remove_project_fixture(const char *root, bool has_request, const char *request_relpath) {
    char path[PATH_MAX];
    if (has_request && request_relpath) {
        snprintf(path, sizeof(path), "%s/%s", root, request_relpath);
        unlink(path);
    }
    snprintf(path, sizeof(path), "%s/ray_tracing", root);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/scene_runtime.json", root);
    unlink(path);
    snprintf(path, sizeof(path), "%s/scene_project.json", root);
    unlink(path);
    rmdir(root);
}

static int test_absent_request_creates_canonical_project_sidecar(void) {
    char root_template[] = "/tmp/ray_scene_project_request_absent_XXXXXX";
    char *root = mkdtemp(root_template);
    char path[PATH_MAX];
    char resolved_path[PATH_MAX];
    char error[256];
    RayTracingSceneProjectRenderRequest request;
    RayTracingSceneProjectRenderRequest readback;
    json_object *json;
    json_object *simulation;
    json_object *project;

    assert_true("project_request_absent_tmpdir", root != NULL);
    if (!root) return 0;
    snprintf(path, sizeof(path), "%s/scene_runtime.json", root);
    assert_true("project_request_absent_runtime", write_text_file(path, "{}\n"));
    snprintf(path, sizeof(path), "%s/scene_project.json", root);
    assert_true("project_request_absent_manifest",
                write_text_file(path,
                    "{\"schema\":\"codework_scene_project_v1\","
                    "\"active\":{\"render_request\":\"ray_tracing/render_request.json\","
                    "\"physics_cache\":\"physics_sim/active_cache_manifest.json\"}}\n"));

    snprintf(path, sizeof(path), "%s/scene_runtime.json", root);
    memset(&request, 0, sizeof(request));
    assert_true("project_request_absent_resolve",
                ray_tracing_scene_project_render_request_resolve(
                    path,
                    NULL,
                    &request,
                    error,
                    sizeof(error)));
    assert_true("project_request_absent_project_backed", request.project_backed);
    assert_true("project_request_absent_project_owned", request.project_owned);
    assert_true("project_request_absent_not_existing", !request.request_exists);
    assert_true("project_request_absent_canonical_relpath",
                strcmp(request.request_relpath, "ray_tracing/render_request.json") == 0);
    assert_true("project_request_absent_write",
                ray_tracing_scene_project_render_request_write(&request, 4, 7, 3, error, sizeof(error)));
    assert_true("project_request_absent_written", request.request_exists && path_exists(request.request_path));

    json = json_object_from_file(request.request_path);
    assert_true("project_request_absent_json", json != NULL);
    simulation = json ? json_object_object_get(json, "simulation_frames") : NULL;
    project = json ? json_object_object_get(json, "scene_project") : NULL;
    assert_true("project_request_absent_window",
                simulation && json_object_get_int(json_object_object_get(simulation, "start")) == 4 &&
                json_object_get_int(json_object_object_get(simulation, "count")) == 7 &&
                json_object_get_int(json_object_object_get(simulation, "stride")) == 3);
    assert_true("project_request_absent_portable_scene",
                project && strcmp(json_object_get_string(json_object_object_get(project, "runtime_scene")),
                                  "scene_runtime.json") == 0);
    if (json) json_object_put(json);

    snprintf(path, sizeof(path), "%s/scene_runtime.json", root);
    memset(&readback, 0, sizeof(readback));
    assert_true("project_request_absent_readback_resolve",
                ray_tracing_scene_project_render_request_resolve(path, NULL, &readback, error, sizeof(error)));
    assert_true("project_request_absent_readback_window",
                readback.simulation_start_frame == 4 &&
                readback.simulation_frame_count == 7 &&
                readback.simulation_frame_stride == 3);
    assert_true("project_request_absent_readback_cache",
                strcmp(readback.physics_cache_relpath,
                       "physics_sim/active_cache_manifest.json") == 0);
    assert_true("project_request_absent_readback_output",
                strcmp(readback.output_root_relpath, "ray_tracing/frames_temp") == 0);
    assert_true("project_request_absent_readback_paths",
                realpath(path, resolved_path) != NULL &&
                strcmp(readback.runtime_scene_path, resolved_path) == 0 &&
                strstr(readback.request_path, "/ray_tracing/render_request.json") != NULL);

    remove_project_fixture(root, true, "ray_tracing/render_request.json");
    return 0;
}

static int test_existing_request_round_trip_preserves_unknown_fields(void) {
    char root_template[] = "/tmp/ray_scene_project_request_existing_XXXXXX";
    char *root = mkdtemp(root_template);
    char path[PATH_MAX];
    char error[256];
    RayTracingSceneProjectRenderRequest request;
    json_object *json;
    json_object *custom;

    assert_true("project_request_existing_tmpdir", root != NULL);
    if (!root) return 0;
    snprintf(path, sizeof(path), "%s/ray_tracing", root);
    assert_true("project_request_existing_ray_dir", make_dir(path));
    snprintf(path, sizeof(path), "%s/scene_runtime.json", root);
    assert_true("project_request_existing_runtime", write_text_file(path, "{}\n"));
    snprintf(path, sizeof(path), "%s/scene_project.json", root);
    assert_true("project_request_existing_manifest",
                write_text_file(path,
                    "{\"active_render_request\":\"ray_tracing/render_request.json\","
                    "\"active_cache\":\"physics_sim/cache_manifest.json\"}\n"));
    snprintf(path, sizeof(path), "%s/ray_tracing/render_request.json", root);
    assert_true("project_request_existing_request",
                write_text_file(path,
                    "{\"schema_version\":\"ray_tracing_agent_render_request_v1\","
                    "\"run_id\":\"keep-run\",\"scene\":{\"runtime_scene_path\":\"../scene_runtime.json\"},"
                    "\"render\":{\"start_frame\":2,\"frame_count\":5,\"width\":320},"
                    "\"custom\":{\"keep\":\"yes\"}}\n"));
    snprintf(path, sizeof(path), "%s/scene_runtime.json", root);
    assert_true("project_request_existing_resolve",
                ray_tracing_scene_project_render_request_resolve(path, NULL, &request, error, sizeof(error)));
    assert_true("project_request_existing_legacy_window",
                request.simulation_start_frame == 2 && request.simulation_frame_count == 5 &&
                request.simulation_frame_stride == 1);
    assert_true("project_request_existing_write",
                ray_tracing_scene_project_render_request_write(&request, 6, 9, 2, error, sizeof(error)));
    json = json_object_from_file(request.request_path);
    custom = json ? json_object_object_get(json, "custom") : NULL;
    assert_true("project_request_existing_unknown_preserved",
                custom && strcmp(json_object_get_string(json_object_object_get(custom, "keep")), "yes") == 0);
    assert_true("project_request_existing_run_id_preserved",
                json && strcmp(json_object_get_string(json_object_object_get(json, "run_id")), "keep-run") == 0);
    assert_true("project_request_existing_render_field_preserved",
                json_object_get_int(json_object_object_get(json_object_object_get(json, "render"), "width")) == 320);
    if (json) json_object_put(json);
    remove_project_fixture(root, true, "ray_tracing/render_request.json");
    return 0;
}

static int test_unsafe_project_pointer_rejected_and_external_explicit_is_read_only(void) {
    char root_template[] = "/tmp/ray_scene_project_request_unsafe_XXXXXX";
    char external_template[] = "/tmp/ray_external_render_request_XXXXXX";
    char *root = mkdtemp(root_template);
    int external_fd = mkstemp(external_template);
    char path[PATH_MAX];
    char error[256];
    RayTracingSceneProjectRenderRequest request;

    assert_true("project_request_unsafe_tmpdir", root != NULL);
    assert_true("project_request_external_tmpfile", external_fd >= 0);
    if (!root || external_fd < 0) return 0;
    close(external_fd);
    assert_true("project_request_external_write",
                write_text_file(external_template,
                    "{\"schema_version\":\"ray_tracing_agent_render_request_v1\","
                    "\"render\":{\"start_frame\":0,\"frame_count\":1}}\n"));
    snprintf(path, sizeof(path), "%s/scene_runtime.json", root);
    assert_true("project_request_unsafe_runtime", write_text_file(path, "{}\n"));
    snprintf(path, sizeof(path), "%s/scene_project.json", root);
    assert_true("project_request_unsafe_manifest",
                write_text_file(path, "{\"active_render_request\":\"../escape.json\"}\n"));
    snprintf(path, sizeof(path), "%s/scene_runtime.json", root);
    assert_true("project_request_unsafe_rejected",
                !ray_tracing_scene_project_render_request_resolve(path, NULL, &request, error, sizeof(error)));
    assert_true("project_request_unsafe_diagnostic", strstr(error, "unsafe") != NULL);

    snprintf(path, sizeof(path), "%s/scene_project.json", root);
    assert_true("project_request_external_safe_manifest",
                write_text_file(path,
                    "{\"active_render_request\":\"ray_tracing/render_request.json\"}\n"));
    snprintf(path, sizeof(path), "%s/scene_runtime.json", root);
    assert_true("project_request_external_resolve",
                ray_tracing_scene_project_render_request_resolve(
                    path, external_template, &request, error, sizeof(error)));
    assert_true("project_request_external_compat_read", request.request_exists && !request.project_owned);
    assert_true("project_request_external_write_rejected",
                !ray_tracing_scene_project_render_request_write(&request, 0, 1, 1, error, sizeof(error)));

    unlink(external_template);
    remove_project_fixture(root, false, NULL);
    return 0;
}

int run_test_scene_project_render_request_tests(void) {
    int before = test_support_failures();
    test_absent_request_creates_canonical_project_sidecar();
    test_existing_request_round_trip_preserves_unknown_fields();
    test_unsafe_project_pointer_rejected_and_external_explicit_is_read_only();
    return test_support_failures() - before;
}
