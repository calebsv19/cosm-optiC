#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "ui/menu_worker_export.h"

#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "config/config_manager.h"

static bool file_exists(const char *path) {
    FILE *f = NULL;
    if (!path || !path[0]) return false;
    f = fopen(path, "r");
    if (!f) return false;
    fclose(f);
    return true;
}

static bool dir_exists(const char *path) {
    DIR *d = NULL;
    if (!path || !path[0]) return false;
    d = opendir(path);
    if (!d) return false;
    closedir(d);
    return true;
}

static void set_status(RayTracingMenuWorkerExportStatus *status,
                       bool ok,
                       const char *message) {
    if (!status) return;
    status->ok = ok;
    snprintf(status->message, sizeof(status->message), "%s", message ? message : "");
}

static bool copy_string(char *out, size_t out_size, const char *src) {
    if (!out || out_size == 0 || !src) return false;
    if (snprintf(out, out_size, "%s", src) >= (int)out_size) return false;
    return true;
}

static bool join_path(const char *a, const char *b, char *out, size_t out_size) {
    if (!a || !a[0] || !b || !b[0] || !out || out_size == 0) return false;
    if (snprintf(out, out_size, "%s/%s", a, b) >= (int)out_size) return false;
    return true;
}

static bool dirname_of(const char *path, char *out, size_t out_size) {
    const char *slash = NULL;
    size_t len = 0;
    if (!path || !path[0] || !out || out_size == 0) return false;
    slash = strrchr(path, '/');
    if (!slash || slash == path) return false;
    len = (size_t)(slash - path);
    if (len >= out_size) return false;
    memcpy(out, path, len);
    out[len] = '\0';
    return true;
}

static const char *basename_of(const char *path) {
    const char *slash = NULL;
    if (!path || !path[0]) return "scene";
    slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static bool has_export_script(const char *program_root) {
    char script[PATH_MAX];
    return join_path(program_root,
                     "tools/export_worker_queue_fixture.py",
                     script,
                     sizeof(script)) &&
           file_exists(script);
}

static bool resolve_program_root_from_candidate(const char *candidate,
                                                char *out,
                                                size_t out_size) {
    char resolved[PATH_MAX];
    if (!candidate || !candidate[0]) return false;
    if (!has_export_script(candidate)) return false;
    if (realpath(candidate, resolved)) {
        return copy_string(out, out_size, resolved);
    }
    return copy_string(out, out_size, candidate);
}

static bool find_program_root(char *out, size_t out_size) {
    char cwd[PATH_MAX];
    char cursor[PATH_MAX];
    char candidate[PATH_MAX];
    const char *home = getenv("HOME");

    if (!out || out_size == 0) return false;
    if (getcwd(cwd, sizeof(cwd))) {
        if (!copy_string(cursor, sizeof(cursor), cwd)) return false;
        while (cursor[0]) {
            if (resolve_program_root_from_candidate(cursor, out, out_size)) {
                return true;
            }
            if (join_path(cursor, "ray_tracing", candidate, sizeof(candidate)) &&
                resolve_program_root_from_candidate(candidate, out, out_size)) {
                return true;
            }
            char *slash = strrchr(cursor, '/');
            if (!slash) break;
            if (slash == cursor) {
                cursor[1] = '\0';
                break;
            }
            *slash = '\0';
        }
    }

    if (home && home[0] &&
        snprintf(candidate,
                 sizeof(candidate),
                 "%s/Desktop/CodeWork/ray_tracing",
                 home) < (int)sizeof(candidate) &&
        resolve_program_root_from_candidate(candidate, out, out_size)) {
        return true;
    }
    return false;
}

static bool resolve_render_request(const char *scene_runtime_path,
                                   char *out,
                                   size_t out_size) {
    char scene_dir[PATH_MAX];
    char candidate[PATH_MAX];
    const char *env_path = getenv("RAY_TRACING_WORKER_RENDER_REQUEST");

    if (!out || out_size == 0) return false;
    if (env_path && env_path[0] && file_exists(env_path)) {
        return copy_string(out, out_size, env_path);
    }
    if (!dirname_of(scene_runtime_path, scene_dir, sizeof(scene_dir))) {
        return false;
    }
    if (join_path(scene_dir, "render_request.json", candidate, sizeof(candidate)) &&
        file_exists(candidate)) {
        return copy_string(out, out_size, candidate);
    }
    if (join_path(scene_dir, "ray_tracing_request.json", candidate, sizeof(candidate)) &&
        file_exists(candidate)) {
        return copy_string(out, out_size, candidate);
    }
    return false;
}

static bool resolve_mesh_asset_root(const char *scene_runtime_path,
                                    char *out,
                                    size_t out_size) {
    char scene_dir[PATH_MAX];
    char candidate[PATH_MAX];

    if (!out || out_size == 0) return false;
    out[0] = '\0';
    if (animSettings.meshAssetRoot[0] && dir_exists(animSettings.meshAssetRoot)) {
        return copy_string(out, out_size, animSettings.meshAssetRoot);
    }
    if (!dirname_of(scene_runtime_path, scene_dir, sizeof(scene_dir))) {
        return false;
    }
    if (join_path(scene_dir, "assets/mesh_assets", candidate, sizeof(candidate)) &&
        dir_exists(candidate)) {
        return copy_string(out, out_size, candidate);
    }
    if (join_path(scene_dir, "mesh_assets", candidate, sizeof(candidate)) &&
        dir_exists(candidate)) {
        return copy_string(out, out_size, candidate);
    }
    return false;
}

static void sanitize_item_slug(const char *name, char *out, size_t out_size) {
    size_t n = 0;
    if (!out || out_size == 0) return;
    if (!name || !name[0]) name = "scene";
    for (const char *p = name; *p && n + 1 < out_size; ++p) {
        unsigned char ch = (unsigned char)*p;
        if (isalnum(ch)) {
            out[n++] = (char)tolower(ch);
        } else if (ch == '-' || ch == '_') {
            out[n++] = (char)ch;
        } else if (ch == '.') {
            break;
        } else if (n > 0 && out[n - 1] != '-') {
            out[n++] = '-';
        }
    }
    while (n > 0 && out[n - 1] == '-') {
        --n;
    }
    if (n == 0 && out_size > 1) {
        out[n++] = 's';
    }
    out[n] = '\0';
}

static bool make_export_ids(const char *scene_runtime_path,
                            char *item_name,
                            size_t item_name_size,
                            char *job_id,
                            size_t job_id_size) {
    char slug[64];
    time_t now = time(NULL);
    struct tm tm_value;
    char stamp[32];
    int written = 0;
    if (!item_name || item_name_size == 0 || !job_id || job_id_size == 0) {
        return false;
    }
    sanitize_item_slug(basename_of(scene_runtime_path), slug, sizeof(slug));
#if defined(_POSIX_THREAD_SAFE_FUNCTIONS) || !defined(_WIN32)
    if (!gmtime_r(&now, &tm_value)) return false;
#else
    {
        struct tm *tmp = gmtime(&now);
        if (!tmp) return false;
        tm_value = *tmp;
    }
#endif
    if (strftime(stamp, sizeof(stamp), "%Y%m%dT%H%M%SZ", &tm_value) == 0) {
        return false;
    }
    written = snprintf(item_name,
                       item_name_size,
                       "ray-menu-scene-only-%s-%s",
                       slug,
                       stamp);
    if (written < 0 || written >= (int)item_name_size) return false;
    written = snprintf(job_id,
                       job_id_size,
                       "ray-tracing--menu-scene-only-%s--%s--rtbundle",
                       slug,
                       stamp);
    return written >= 0 && written < (int)job_id_size;
}

static bool shell_quote(const char *src, char *out, size_t out_size) {
    size_t n = 0;
    if (!src || !out || out_size < 3) return false;
    out[n++] = '\'';
    for (const char *p = src; *p; ++p) {
        if (*p == '\'') {
            const char *escape = "'\\''";
            const size_t escape_len = 4;
            if (n + escape_len >= out_size) return false;
            memcpy(out + n, escape, escape_len);
            n += escape_len;
        } else {
            if (n + 1 >= out_size) return false;
            out[n++] = *p;
        }
    }
    if (n + 2 > out_size) return false;
    out[n++] = '\'';
    out[n] = '\0';
    return true;
}

static bool append_arg(char *cmd,
                       size_t cmd_size,
                       const char *flag,
                       const char *value) {
    char quoted[PATH_MAX + 8];
    size_t len = 0;
    if (!cmd || !flag || !value || !shell_quote(value, quoted, sizeof(quoted))) {
        return false;
    }
    len = strlen(cmd);
    return snprintf(cmd + len, cmd_size - len, " %s %s", flag, quoted) < (int)(cmd_size - len);
}

static bool run_export_command(const char *program_root,
                               const char *scene_runtime,
                               const char *render_request,
                               const char *mesh_asset_root,
                               const char *output_root,
                               const char *item_name,
                               const char *job_id) {
    char cmd[8192];
    char python_q[PATH_MAX + 8];
    char script[PATH_MAX];
    const char *python = getenv("RAY_TRACING_WORKER_EXPORT_PYTHON");
    int rc = 0;
    if (!python || !python[0]) python = "python3";
    if (!shell_quote(python, python_q, sizeof(python_q))) return false;
    if (!join_path(program_root,
                   "tools/export_worker_queue_fixture.py",
                   script,
                   sizeof(script))) {
        return false;
    }
    snprintf(cmd, sizeof(cmd), "%s", python_q);
    if (!append_arg(cmd, sizeof(cmd), "", script)) return false;
    if (!append_arg(cmd, sizeof(cmd), "--scene-runtime", scene_runtime)) return false;
    if (!append_arg(cmd, sizeof(cmd), "--render-request", render_request)) return false;
    if (mesh_asset_root && mesh_asset_root[0] &&
        !append_arg(cmd, sizeof(cmd), "--mesh-asset-root", mesh_asset_root)) {
        return false;
    }
    if (!append_arg(cmd, sizeof(cmd), "--output-root", output_root)) return false;
    if (!append_arg(cmd, sizeof(cmd), "--item-name", item_name)) return false;
    if (!append_arg(cmd, sizeof(cmd), "--job-id", job_id)) return false;
    rc = system(cmd);
    return rc != -1 && WIFEXITED(rc) && WEXITSTATUS(rc) == 0;
}

bool ray_tracing_menu_worker_export_scene_only(RayTracingMenuWorkerExportStatus *status) {
    char program_root[PATH_MAX];
    char render_request[PATH_MAX];
    char mesh_asset_root[PATH_MAX];
    char output_root[PATH_MAX];
    char item_name[128];
    char job_id[192];

    if (status) {
        memset(status, 0, sizeof(*status));
    }
    if (animation_config_scene_source_clamp(animSettings.sceneSource) != SCENE_SOURCE_RUNTIME_SCENE ||
        animSettings.runtimeScenePath[0] == '\0') {
        set_status(status, false, "Select a runtime scene first");
        return false;
    }
    if (!find_program_root(program_root, sizeof(program_root))) {
        set_status(status, false, "Worker exporter not found");
        return false;
    }
    if (!resolve_render_request(animSettings.runtimeScenePath, render_request, sizeof(render_request))) {
        set_status(status, false, "Missing render request sidecar");
        return false;
    }
    if (!join_path(program_root,
                   "visual_artifacts/worker_queue_exports",
                   output_root,
                   sizeof(output_root))) {
        set_status(status, false, "Export path too long");
        return false;
    }
    if (!make_export_ids(animSettings.runtimeScenePath,
                         item_name,
                         sizeof(item_name),
                         job_id,
                         sizeof(job_id))) {
        set_status(status, false, "Export id failed");
        return false;
    }
    (void)resolve_mesh_asset_root(animSettings.runtimeScenePath,
                                  mesh_asset_root,
                                  sizeof(mesh_asset_root));

    if (!run_export_command(program_root,
                            animSettings.runtimeScenePath,
                            render_request,
                            mesh_asset_root,
                            output_root,
                            item_name,
                            job_id)) {
        set_status(status, false, "Worker export failed");
        return false;
    }

    if (status) {
        status->ok = true;
        snprintf(status->message, sizeof(status->message), "Worker export ready");
        snprintf(status->item_name, sizeof(status->item_name), "%s", item_name);
        snprintf(status->output_root, sizeof(status->output_root), "%s", output_root);
    }
    return true;
}
