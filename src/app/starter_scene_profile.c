#include "app/starter_scene_profile.h"

#include <json-c/json.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static void set_error(char* error, size_t error_size, const char* format, ...) {
    va_list args;
    if (!error || error_size == 0u) return;
    va_start(args, format);
    (void)vsnprintf(error, error_size, format, args);
    va_end(args);
}

static bool copy_json_string(struct json_object* root,
                             const char* key,
                             char* out,
                             size_t out_size,
                             bool required,
                             char* error,
                             size_t error_size) {
    struct json_object* value = NULL;
    const char* text = NULL;
    if (!json_object_object_get_ex(root, key, &value)) {
        if (!required) return true;
        set_error(error, error_size, "missing required field: %s", key);
        return false;
    }
    if (!json_object_is_type(value, json_type_string)) {
        set_error(error, error_size, "field must be a string: %s", key);
        return false;
    }
    text = json_object_get_string(value);
    if (!text || (required && !text[0]) || snprintf(out, out_size, "%s", text) >= (int)out_size) {
        set_error(error, error_size, "field is empty or too long: %s", key);
        return false;
    }
    return true;
}

static bool read_json_bool(struct json_object* root,
                           const char* key,
                           bool* out,
                           char* error,
                           size_t error_size) {
    struct json_object* value = NULL;
    if (!json_object_object_get_ex(root, key, &value) ||
        !json_object_is_type(value, json_type_boolean)) {
        set_error(error, error_size, "field must be a boolean: %s", key);
        return false;
    }
    *out = json_object_get_boolean(value) != 0;
    return true;
}

static bool read_json_int(struct json_object* root,
                          const char* key,
                          int* out,
                          char* error,
                          size_t error_size) {
    struct json_object* value = NULL;
    if (!json_object_object_get_ex(root, key, &value) ||
        !json_object_is_type(value, json_type_int)) {
        set_error(error, error_size, "field must be an integer: %s", key);
        return false;
    }
    *out = json_object_get_int(value);
    return true;
}

static bool is_safe_relative_path(const char* path) {
    const char* segment = path;
    if (!path || !path[0] || path[0] == '/') return false;
    while (*segment) {
        const char* end = strchr(segment, '/');
        const size_t length = end ? (size_t)(end - segment) : strlen(segment);
        if (length == 0u || (length == 1u && segment[0] == '.') ||
            (length == 2u && segment[0] == '.' && segment[1] == '.')) {
            return false;
        }
        if (!end) break;
        segment = end + 1;
    }
    return true;
}

static bool compose_path(const char* root,
                         const char* relative,
                         char* out,
                         size_t out_size) {
    return root && root[0] && relative && relative[0] && out && out_size > 0u &&
           snprintf(out, out_size, "%s/%s", root, relative) < (int)out_size;
}

static bool path_is_file(const char* path) {
    struct stat info;
    return path && stat(path, &info) == 0 && S_ISREG(info.st_mode);
}

static bool path_is_directory(const char* path) {
    struct stat info;
    return path && stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

void ray_tracing_starter_scene_profile_defaults(RayTracingStarterSceneProfile* profile) {
    if (!profile) return;
    memset(profile, 0, sizeof(*profile));
    (void)snprintf(profile->schema, sizeof(profile->schema), "%s",
                   RAY_TRACING_STARTER_SCENE_PROFILE_SCHEMA);
}

bool ray_tracing_starter_scene_profile_load(const char* path,
                                            RayTracingStarterSceneProfile* out_profile,
                                            char* error,
                                            size_t error_size) {
    struct json_object* root = NULL;
    struct json_object* value = NULL;
    RayTracingStarterSceneProfile profile;
    bool ok = false;
    if (error && error_size > 0u) error[0] = '\0';
    if (!path || !path[0] || !out_profile) {
        set_error(error, error_size, "profile path and output are required");
        return false;
    }
    root = json_object_from_file(path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        set_error(error, error_size, "unable to parse starter scene profile: %s", path);
        goto done;
    }
    ray_tracing_starter_scene_profile_defaults(&profile);
    if (!copy_json_string(root, "schema", profile.schema, sizeof(profile.schema), true,
                          error, error_size) ||
        strcmp(profile.schema, RAY_TRACING_STARTER_SCENE_PROFILE_SCHEMA) != 0) {
        if (!error || error_size == 0u || !error[0]) {
            set_error(error, error_size, "unsupported starter scene profile schema");
        }
        goto done;
    }
    if (!copy_json_string(root, "profile_id", profile.profileId, sizeof(profile.profileId), true,
                          error, error_size) ||
        !read_json_bool(root, "enabled", &profile.enabled, error, error_size) ||
        !copy_json_string(root, "template_directory", profile.templateDirectory,
                          sizeof(profile.templateDirectory), false, error, error_size) ||
        !copy_json_string(root, "working_directory", profile.workingDirectory,
                          sizeof(profile.workingDirectory), false, error, error_size) ||
        !copy_json_string(root, "runtime_scene", profile.runtimeSceneRelativePath,
                          sizeof(profile.runtimeSceneRelativePath), false, error, error_size) ||
        !read_json_int(root, "preferred_space_mode", &profile.preferredSpaceMode,
                       error, error_size) ||
        !read_json_int(root, "preferred_editor_mode", &profile.preferredEditorMode,
                       error, error_size) ||
        !read_json_bool(root, "open_scene_editor", &profile.openSceneEditor,
                        error, error_size)) {
        goto done;
    }
    if (json_object_object_get_ex(root, "notes", &value) &&
        !json_object_is_type(value, json_type_string)) {
        set_error(error, error_size, "field must be a string: notes");
        goto done;
    }
    *out_profile = profile;
    ok = true;
done:
    if (root) json_object_put(root);
    return ok;
}

bool ray_tracing_starter_scene_profile_plan(
    const RayTracingStarterSceneProfile* profile,
    const RayTracingStarterSceneProfileContext* context,
    RayTracingStarterSceneProfilePlan* out_plan,
    char* error,
    size_t error_size) {
    char template_directory[PATH_MAX];
    char working_directory[PATH_MAX];
    bool working_directory_exists = false;
    if (error && error_size > 0u) error[0] = '\0';
    if (!profile || !context || !out_plan) {
        set_error(error, error_size, "profile, context, and plan are required");
        return false;
    }
    memset(out_plan, 0, sizeof(*out_plan));
    out_plan->status = RAY_TRACING_STARTER_PROFILE_INVALID;
    if (strcmp(profile->schema, RAY_TRACING_STARTER_SCENE_PROFILE_SCHEMA) != 0 ||
        !profile->profileId[0]) {
        set_error(error, error_size, "profile schema or id is invalid");
        return false;
    }
    if (!profile->enabled) {
        out_plan->status = RAY_TRACING_STARTER_PROFILE_DISABLED;
        return true;
    }
    if (profile->preferredSpaceMode != 1 ||
        profile->preferredEditorMode < 0 || profile->preferredEditorMode > 3) {
        set_error(error, error_size,
                  "enabled profile must select 3D space and a supported editor mode");
        return false;
    }
    if (!context->programRoot[0] ||
        !is_safe_relative_path(profile->templateDirectory) ||
        !is_safe_relative_path(profile->workingDirectory) ||
        !is_safe_relative_path(profile->runtimeSceneRelativePath)) {
        set_error(error, error_size, "enabled profile paths must be safe relative paths");
        return false;
    }
    if (!compose_path(context->programRoot, profile->templateDirectory,
                      template_directory, sizeof(template_directory)) ||
        !compose_path(context->programRoot, profile->workingDirectory,
                      working_directory, sizeof(working_directory)) ||
        !compose_path(template_directory, profile->runtimeSceneRelativePath,
                      out_plan->templateRuntimeScenePath,
                      sizeof(out_plan->templateRuntimeScenePath)) ||
        !compose_path(working_directory, profile->runtimeSceneRelativePath,
                      out_plan->workingRuntimeScenePath,
                      sizeof(out_plan->workingRuntimeScenePath))) {
        set_error(error, error_size, "starter scene profile path is too long");
        return false;
    }
    if (context->activationMarkerExists) {
        out_plan->status = RAY_TRACING_STARTER_PROFILE_PRESERVE_AFTER_ACTIVATION;
        return true;
    }
    if (context->hasExplicitSceneSelection) {
        if (!context->activeRuntimeScenePath[0] ||
            strcmp(context->activeRuntimeScenePath, out_plan->workingRuntimeScenePath) != 0) {
            out_plan->status = RAY_TRACING_STARTER_PROFILE_PRESERVE_USER_SCENE;
            return true;
        }
    }
    working_directory_exists = path_is_directory(working_directory);
    if (path_is_file(out_plan->workingRuntimeScenePath)) {
        out_plan->status = RAY_TRACING_STARTER_PROFILE_READY_TO_ACTIVATE;
        out_plan->shouldActivateWorkingCopy = true;
        out_plan->shouldOpenSceneEditor = profile->openSceneEditor;
        return true;
    }
    if (working_directory_exists) {
        out_plan->status = RAY_TRACING_STARTER_PROFILE_WORKING_COPY_INVALID;
        return true;
    }
    if (!path_is_file(out_plan->templateRuntimeScenePath)) {
        out_plan->status = RAY_TRACING_STARTER_PROFILE_TEMPLATE_MISSING;
        return true;
    }
    out_plan->status = RAY_TRACING_STARTER_PROFILE_READY_TO_SEED;
    out_plan->shouldSeedWorkingCopy = true;
    return true;
}

const char* ray_tracing_starter_scene_profile_status_label(
    RayTracingStarterSceneProfileStatus status) {
    switch (status) {
        case RAY_TRACING_STARTER_PROFILE_DISABLED: return "disabled";
        case RAY_TRACING_STARTER_PROFILE_PRESERVE_USER_SCENE: return "preserve_user_scene";
        case RAY_TRACING_STARTER_PROFILE_PRESERVE_AFTER_ACTIVATION: return "preserve_after_activation";
        case RAY_TRACING_STARTER_PROFILE_TEMPLATE_MISSING: return "template_missing";
        case RAY_TRACING_STARTER_PROFILE_WORKING_COPY_INVALID: return "working_copy_invalid";
        case RAY_TRACING_STARTER_PROFILE_READY_TO_SEED: return "ready_to_seed";
        case RAY_TRACING_STARTER_PROFILE_READY_TO_ACTIVATE: return "ready_to_activate";
        case RAY_TRACING_STARTER_PROFILE_INVALID: return "invalid";
    }
    return "invalid";
}
