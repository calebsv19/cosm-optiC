#include "editor/scene_editor_document.h"

#include <errno.h>
#include <fcntl.h>
#include <json-c/json.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config/config_manager.h"
#include "editor/object_editor_motion.h"
#include "import/runtime_scene_bridge.h"
#include "import/runtime_scene_motion_bridge.h"

#define SCENE_EDITOR_DOCUMENT_HISTORY_LIMIT 32

typedef struct SceneEditorDocumentState {
    json_object* root;
    char path[PATH_MAX];
    char* baseline_bytes;
    size_t baseline_size;
    char* undo[SCENE_EDITOR_DOCUMENT_HISTORY_LIMIT];
    int undo_count;
    char* redo[SCENE_EDITOR_DOCUMENT_HISTORY_LIMIT];
    int redo_count;
    char* pending_before;
    unsigned long long revision;
    bool dirty;
} SceneEditorDocumentState;

static SceneEditorDocumentState s_document;

static bool document_parent_path(const char* path, char* out, size_t out_size);

static void document_diag(char* out, size_t size, const char* text) {
    if (!out || size == 0u) return;
    snprintf(out, size, "%s", text ? text : "unknown error");
}

static char* document_copy_bytes(const char* bytes, size_t size) {
    char* copy = (char*)malloc(size + 1u);
    if (!copy) return NULL;
    if (size > 0u) memcpy(copy, bytes, size);
    copy[size] = '\0';
    return copy;
}

static bool document_read_file(const char* path, char** out_bytes, size_t* out_size) {
    FILE* file = NULL;
    long length = 0;
    char* bytes = NULL;
    if (!path || !out_bytes || !out_size) return false;
    *out_bytes = NULL;
    *out_size = 0u;
    file = fopen(path, "rb");
    if (!file) return false;
    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }
    bytes = (char*)malloc((size_t)length + 1u);
    if (!bytes) {
        fclose(file);
        return false;
    }
    if (length > 0 && fread(bytes, 1u, (size_t)length, file) != (size_t)length) {
        free(bytes);
        fclose(file);
        return false;
    }
    bytes[length] = '\0';
    fclose(file);
    *out_bytes = bytes;
    *out_size = (size_t)length;
    return true;
}

static json_object* document_parse_valid(const char* bytes,
                                         char* diagnostics,
                                         size_t diagnostics_size) {
    json_tokener* tokener = NULL;
    json_object* root = NULL;
    RuntimeSceneBridgePreflight summary = {0};
    if (!bytes) return NULL;
    tokener = json_tokener_new();
    if (!tokener) {
        document_diag(diagnostics, diagnostics_size, "out of memory");
        return NULL;
    }
    root = json_tokener_parse_ex(tokener, bytes, (int)strlen(bytes));
    if (!root || json_tokener_get_error(tokener) != json_tokener_success ||
        !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        json_tokener_free(tokener);
        document_diag(diagnostics, diagnostics_size, "runtime scene is not valid JSON");
        return NULL;
    }
    json_tokener_free(tokener);
    if (!runtime_scene_bridge_preflight_json(bytes, &summary)) {
        json_object_put(root);
        document_diag(diagnostics, diagnostics_size, summary.diagnostics);
        return NULL;
    }
    return root;
}

static char* document_serialize_root(json_object* root, size_t* out_size) {
    const char* text = NULL;
    char* copy = NULL;
    size_t size = 0u;
    if (!root) return NULL;
    text = json_object_to_json_string_ext(root,
                                          JSON_C_TO_STRING_PRETTY |
                                              JSON_C_TO_STRING_NOSLASHESCAPE);
    if (!text) return NULL;
    size = strlen(text);
    copy = (char*)malloc(size + 2u);
    if (!copy) return NULL;
    memcpy(copy, text, size);
    copy[size++] = '\n';
    copy[size] = '\0';
    if (out_size) *out_size = size;
    return copy;
}

static void document_clear_stack(char** stack, int* count) {
    if (!stack || !count) return;
    for (int i = 0; i < *count; ++i) free(stack[i]);
    *count = 0;
}

static bool document_push(char** stack, int* count, char* owned_snapshot) {
    if (!stack || !count || !owned_snapshot) return false;
    if (*count == SCENE_EDITOR_DOCUMENT_HISTORY_LIMIT) {
        free(stack[0]);
        memmove(&stack[0], &stack[1], sizeof(stack[0]) * (SCENE_EDITOR_DOCUMENT_HISTORY_LIMIT - 1));
        *count -= 1;
    }
    stack[(*count)++] = owned_snapshot;
    return true;
}

static bool document_apply_current(char* diagnostics, size_t diagnostics_size) {
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeMotionTrack3DSummary motion_summary = {0};
    char* serialized = document_serialize_root(s_document.root, NULL);
    char parent[PATH_MAX] = {0};
    char candidate_path[PATH_MAX] = {0};
    int candidate_fd = -1;
    bool ok = false;
    if (!serialized) {
        document_diag(diagnostics, diagnostics_size, "failed to serialize runtime scene");
        return false;
    }
    if (!document_parent_path(s_document.path, parent, sizeof(parent)) ||
        snprintf(candidate_path,
                 sizeof(candidate_path),
                 "%s/.scene-editor-apply-XXXXXX",
                 parent) >= (int)sizeof(candidate_path)) {
        document_diag(diagnostics, diagnostics_size, "runtime scene preview path is too long");
        goto done;
    }
    candidate_fd = mkstemp(candidate_path);
    if (candidate_fd < 0) {
        document_diag(diagnostics, diagnostics_size, "failed to create runtime scene preview candidate");
        goto done;
    }
    for (size_t written = 0u, size = strlen(serialized); written < size;) {
        ssize_t count = write(candidate_fd, serialized + written, size - written);
        if (count <= 0) {
            document_diag(diagnostics, diagnostics_size, "failed to write runtime scene preview candidate");
            goto done;
        }
        written += (size_t)count;
    }
    if (close(candidate_fd) != 0) {
        candidate_fd = -1;
        document_diag(diagnostics, diagnostics_size, "failed to close runtime scene preview candidate");
        goto done;
    }
    candidate_fd = -1;
    if (!runtime_scene_bridge_apply_file_defer_mesh_assets(candidate_path, &summary)) {
        document_diag(diagnostics, diagnostics_size, summary.diagnostics);
        goto done;
    }
    runtime_scene_motion_bridge_get_last_summary(&motion_summary);
    ObjectEditorMotionHydrateFromRuntimeSummary(&motion_summary);
    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    snprintf(animSettings.runtimeScenePath,
             sizeof(animSettings.runtimeScenePath),
             "%s",
             s_document.path);
    document_diag(diagnostics, diagnostics_size, "ok");
    ok = true;
done:
    if (candidate_fd >= 0) close(candidate_fd);
    if (candidate_path[0]) unlink(candidate_path);
    free(serialized);
    return ok;
}

static bool document_apply_saved_file(char* diagnostics, size_t diagnostics_size) {
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeMotionTrack3DSummary motion_summary = {0};
    if (!runtime_scene_bridge_apply_file_defer_mesh_assets(s_document.path, &summary)) {
        document_diag(diagnostics, diagnostics_size, summary.diagnostics);
        return false;
    }
    runtime_scene_motion_bridge_get_last_summary(&motion_summary);
    ObjectEditorMotionHydrateFromRuntimeSummary(&motion_summary);
    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    snprintf(animSettings.runtimeScenePath,
             sizeof(animSettings.runtimeScenePath),
             "%s",
             s_document.path);
    document_diag(diagnostics, diagnostics_size, "ok");
    return true;
}

static bool document_begin_command(char* diagnostics, size_t diagnostics_size) {
    char* before = document_serialize_root(s_document.root, NULL);
    if (s_document.pending_before) {
        document_diag(diagnostics, diagnostics_size, "another document command is pending");
        return false;
    }
    if (!before) {
        document_diag(diagnostics, diagnostics_size, "failed to snapshot undo state");
        return false;
    }
    s_document.pending_before = before;
    return true;
}

static bool document_commit_command_history(void) {
    char* before = s_document.pending_before;
    if (!before) return false;
    if (!document_push(s_document.undo, &s_document.undo_count, before)) return false;
    s_document.pending_before = NULL;
    document_clear_stack(s_document.redo, &s_document.redo_count);
    return true;
}

static void document_rollback_command(void) {
    json_object* restored = NULL;
    char* snapshot = s_document.pending_before;
    if (!snapshot) return;
    restored = json_tokener_parse(snapshot);
    free(snapshot);
    s_document.pending_before = NULL;
    if (!restored) return;
    json_object_put(s_document.root);
    s_document.root = restored;
}

static json_object* document_object_for_scene_index(int scene_object_index,
                                                     char* diagnostics,
                                                     size_t diagnostics_size) {
    char object_id[64] = {0};
    json_object* objects = NULL;
    if (!s_document.root ||
        !runtime_scene_bridge_get_last_object_id_for_scene_index(scene_object_index,
                                                                  object_id,
                                                                  sizeof(object_id))) {
        document_diag(diagnostics, diagnostics_size, "selected object has no stable runtime ID");
        return NULL;
    }
    if (!json_object_object_get_ex(s_document.root, "objects", &objects) ||
        !json_object_is_type(objects, json_type_array)) {
        document_diag(diagnostics, diagnostics_size, "runtime scene has no objects array");
        return NULL;
    }
    for (size_t i = 0; i < json_object_array_length(objects); ++i) {
        json_object* object = json_object_array_get_idx(objects, (int)i);
        json_object* id = NULL;
        if (object && json_object_object_get_ex(object, "object_id", &id) &&
            json_object_is_type(id, json_type_string) &&
            strcmp(json_object_get_string(id), object_id) == 0) {
            return object;
        }
    }
    document_diag(diagnostics, diagnostics_size, "selected runtime object is missing from document");
    return NULL;
}

static bool document_vec3_get(json_object* parent, const char* key, double out[3]) {
    static const char* axis[3] = {"x", "y", "z"};
    json_object* value = NULL;
    if (!parent || !json_object_object_get_ex(parent, key, &value) ||
        !json_object_is_type(value, json_type_object)) return false;
    for (int i = 0; i < 3; ++i) {
        json_object* component = NULL;
        if (!json_object_object_get_ex(value, axis[i], &component)) return false;
        out[i] = json_object_get_double(component);
        if (!isfinite(out[i])) return false;
    }
    return true;
}

static json_object* document_vec3_new(const double value[3]) {
    json_object* vector = json_object_new_object();
    if (!vector) return NULL;
    json_object_object_add(vector, "x", json_object_new_double(value[0]));
    json_object_object_add(vector, "y", json_object_new_double(value[1]));
    json_object_object_add(vector, "z", json_object_new_double(value[2]));
    return vector;
}

static bool document_finish_command(char* diagnostics, size_t diagnostics_size) {
    if (!document_apply_current(diagnostics, diagnostics_size)) {
        document_rollback_command();
        (void)document_apply_current(NULL, 0u);
        return false;
    }
    if (!document_commit_command_history()) {
        document_rollback_command();
        (void)document_apply_current(NULL, 0u);
        document_diag(diagnostics, diagnostics_size, "failed to commit document history");
        return false;
    }
    s_document.dirty = true;
    s_document.revision += 1u;
    return true;
}

bool SceneEditorDocumentOpen(const char* path, char* diagnostics, size_t diagnostics_size) {
    char* bytes = NULL;
    size_t size = 0u;
    json_object* root = NULL;
    if (!path || !path[0] || strlen(path) >= sizeof(s_document.path)) {
        document_diag(diagnostics, diagnostics_size, "invalid runtime scene path");
        return false;
    }
    if (!document_read_file(path, &bytes, &size)) {
        document_diag(diagnostics, diagnostics_size, "failed to read runtime scene");
        return false;
    }
    root = document_parse_valid(bytes, diagnostics, diagnostics_size);
    if (!root) {
        free(bytes);
        return false;
    }
    SceneEditorDocumentClose();
    s_document.root = root;
    s_document.baseline_bytes = bytes;
    s_document.baseline_size = size;
    snprintf(s_document.path, sizeof(s_document.path), "%s", path);
    s_document.revision = 1u;
    s_document.dirty = false;
    document_diag(diagnostics, diagnostics_size, "ok");
    return true;
}

bool SceneEditorDocumentOpenActive(char* diagnostics, size_t diagnostics_size) {
    if (animSettings.sceneSource != SCENE_SOURCE_RUNTIME_SCENE ||
        !animSettings.runtimeScenePath[0]) {
        SceneEditorDocumentClose();
        document_diag(diagnostics, diagnostics_size, "runtime scene source is not active");
        return false;
    }
    return SceneEditorDocumentOpen(animSettings.runtimeScenePath, diagnostics, diagnostics_size);
}

void SceneEditorDocumentClose(void) {
    if (s_document.root) json_object_put(s_document.root);
    free(s_document.baseline_bytes);
    free(s_document.pending_before);
    document_clear_stack(s_document.undo, &s_document.undo_count);
    document_clear_stack(s_document.redo, &s_document.redo_count);
    memset(&s_document, 0, sizeof(s_document));
}

bool SceneEditorDocumentIsOpen(void) { return s_document.root != NULL; }
bool SceneEditorDocumentIsDirty(void) { return s_document.dirty; }
const char* SceneEditorDocumentPath(void) { return s_document.root ? s_document.path : ""; }
unsigned long long SceneEditorDocumentRevision(void) { return s_document.revision; }

bool SceneEditorDocumentGetTransformForSceneIndex(int scene_object_index,
                                                  SceneEditorDocumentTransform* out_transform,
                                                  char* diagnostics,
                                                  size_t diagnostics_size) {
    json_object* object = NULL;
    json_object* transform = NULL;
    if (!out_transform) return false;
    object = document_object_for_scene_index(scene_object_index, diagnostics, diagnostics_size);
    if (!object || !json_object_object_get_ex(object, "transform", &transform) ||
        !json_object_is_type(transform, json_type_object) ||
        !document_vec3_get(transform, "position", out_transform->position) ||
        !document_vec3_get(transform, "rotation", out_transform->rotation_degrees) ||
        !document_vec3_get(transform, "scale", out_transform->scale)) {
        document_diag(diagnostics, diagnostics_size, "selected object has no editable XYZ transform");
        return false;
    }
    document_diag(diagnostics, diagnostics_size, "ok");
    return true;
}

bool SceneEditorDocumentSetTransformForSceneIndex(int scene_object_index,
                                                  const SceneEditorDocumentTransform* transform_value,
                                                  char* diagnostics,
                                                  size_t diagnostics_size) {
    json_object* object = NULL;
    json_object* transform = NULL;
    if (!transform_value) return false;
    for (int i = 0; i < 3; ++i) {
        if (!isfinite(transform_value->position[i]) ||
            !isfinite(transform_value->rotation_degrees[i]) ||
            !isfinite(transform_value->scale[i]) || transform_value->scale[i] <= 0.0) {
            document_diag(diagnostics, diagnostics_size, "position and rotation must be finite; scale must be positive");
            return false;
        }
    }
    object = document_object_for_scene_index(scene_object_index, diagnostics, diagnostics_size);
    if (!object) return false;
    if (!document_begin_command(diagnostics, diagnostics_size)) return false;
    if (!json_object_object_get_ex(object, "transform", &transform) ||
        !json_object_is_type(transform, json_type_object)) {
        transform = json_object_new_object();
        json_object_object_add(object, "transform", transform);
    }
    json_object_object_add(transform, "position", document_vec3_new(transform_value->position));
    json_object_object_add(transform, "rotation", document_vec3_new(transform_value->rotation_degrees));
    json_object_object_add(transform, "scale", document_vec3_new(transform_value->scale));
    return document_finish_command(diagnostics, diagnostics_size);
}

bool SceneEditorDocumentSetManagedShadingForSceneIndex(int scene_object_index,
                                                       const char* mode,
                                                       double crease_angle_degrees,
                                                       char* diagnostics,
                                                       size_t diagnostics_size) {
    json_object* object = NULL;
    json_object* extensions = NULL;
    json_object* ray = NULL;
    json_object* managed = NULL;
    json_object* shading = NULL;
    if (!mode || (strcmp(mode, "inherit") != 0 && strcmp(mode, "flat") != 0 &&
                  strcmp(mode, "smooth") != 0 && strcmp(mode, "crease_aware") != 0) ||
        !isfinite(crease_angle_degrees) || crease_angle_degrees <= 0.0 || crease_angle_degrees > 180.0) {
        document_diag(diagnostics, diagnostics_size, "invalid managed shading policy");
        return false;
    }
    object = document_object_for_scene_index(scene_object_index, diagnostics, diagnostics_size);
    if (!object || !json_object_object_get_ex(object, "extensions", &extensions) ||
        !json_object_object_get_ex(extensions, "ray_tracing", &ray) ||
        !json_object_object_get_ex(ray, "managed_mesh", &managed)) {
        document_diag(diagnostics, diagnostics_size, "selected object is not a managed mesh");
        return false;
    }
    if (!document_begin_command(diagnostics, diagnostics_size)) return false;
    if (strcmp(mode, "inherit") == 0) {
        shading = json_object_new_string("inherit");
    } else {
        shading = json_object_new_object();
        json_object_object_add(shading, "mode", json_object_new_string(mode));
        json_object_object_add(shading,
                               "crease_angle_degrees",
                               json_object_new_double(crease_angle_degrees));
    }
    json_object_object_add(managed, "shading", shading);
    return document_finish_command(diagnostics, diagnostics_size);
}

static json_object* document_get_or_add_object(json_object* parent, const char* key) {
    json_object* value = NULL;
    if (!parent || !key) return NULL;
    if (json_object_object_get_ex(parent, key, &value) &&
        json_object_is_type(value, json_type_object)) {
        return value;
    }
    value = json_object_new_object();
    if (value) json_object_object_add(parent, key, value);
    return value;
}

bool SceneEditorDocumentSetMaterialIdForSceneIndex(int scene_object_index,
                                                   int material_id,
                                                   char* diagnostics,
                                                   size_t diagnostics_size) {
    json_object* object = document_object_for_scene_index(scene_object_index,
                                                           diagnostics,
                                                           diagnostics_size);
    json_object* object_id = NULL;
    json_object* extensions = NULL;
    json_object* ray = NULL;
    json_object* authoring = NULL;
    json_object* object_materials = NULL;
    json_object* row = NULL;
    if (!object || material_id < 0 ||
        !json_object_object_get_ex(object, "object_id", &object_id)) {
        document_diag(diagnostics, diagnostics_size, "invalid material assignment");
        return false;
    }
    if (!document_begin_command(diagnostics, diagnostics_size)) return false;
    extensions = document_get_or_add_object(s_document.root, "extensions");
    ray = document_get_or_add_object(extensions, "ray_tracing");
    authoring = document_get_or_add_object(ray, "authoring");
    if (!extensions || !ray || !authoring) {
        document_rollback_command();
        document_diag(diagnostics, diagnostics_size, "failed to allocate material authoring state");
        return false;
    }
    if (!json_object_object_get_ex(authoring, "object_materials", &object_materials) ||
        !json_object_is_type(object_materials, json_type_array)) {
        object_materials = json_object_new_array();
        json_object_object_add(authoring, "object_materials", object_materials);
    }
    for (size_t i = 0; i < json_object_array_length(object_materials); ++i) {
        json_object* candidate = json_object_array_get_idx(object_materials, (int)i);
        json_object* candidate_id = NULL;
        if (candidate && json_object_object_get_ex(candidate, "object_id", &candidate_id) &&
            strcmp(json_object_get_string(candidate_id), json_object_get_string(object_id)) == 0) {
            row = candidate;
            break;
        }
    }
    if (!row) {
        row = json_object_new_object();
        json_object_object_add(row,
                               "object_id",
                               json_object_new_string(json_object_get_string(object_id)));
        json_object_array_add(object_materials, row);
    }
    json_object_object_add(row, "material_id", json_object_new_int(material_id));
    return document_finish_command(diagnostics, diagnostics_size);
}

static bool document_make_unique_id(json_object* objects,
                                    const char* source_id,
                                    char* out_id,
                                    size_t out_id_size) {
    for (int suffix = 2; suffix < 10000; ++suffix) {
        bool used = false;
        snprintf(out_id, out_id_size, "%.48s_copy_%d", source_id, suffix);
        for (size_t i = 0; i < json_object_array_length(objects); ++i) {
            json_object* id = NULL;
            json_object* object = json_object_array_get_idx(objects, (int)i);
            if (object && json_object_object_get_ex(object, "object_id", &id) &&
                strcmp(json_object_get_string(id), out_id) == 0) {
                used = true;
                break;
            }
        }
        if (!used) return true;
    }
    return false;
}

bool SceneEditorDocumentDuplicateForSceneIndex(int scene_object_index,
                                               int* out_new_scene_object_index,
                                               char* diagnostics,
                                               size_t diagnostics_size) {
    json_object* source = document_object_for_scene_index(scene_object_index, diagnostics, diagnostics_size);
    json_object* objects = NULL;
    json_object* id = NULL;
    json_object* duplicate = NULL;
    char unique_id[64] = {0};
    if (!source || !json_object_object_get_ex(s_document.root, "objects", &objects) ||
        !json_object_object_get_ex(source, "object_id", &id) ||
        !document_make_unique_id(objects, json_object_get_string(id), unique_id, sizeof(unique_id))) {
        document_diag(diagnostics, diagnostics_size, "failed to allocate duplicate object ID");
        return false;
    }
    duplicate = json_tokener_parse(json_object_to_json_string_ext(source, JSON_C_TO_STRING_PLAIN));
    if (!duplicate || !document_begin_command(diagnostics, diagnostics_size)) {
        if (duplicate) json_object_put(duplicate);
        return false;
    }
    json_object_object_add(duplicate, "object_id", json_object_new_string(unique_id));
    json_object_object_add(duplicate, "display_name", json_object_new_string(unique_id));
    json_object_array_add(objects, duplicate);
    if (!document_finish_command(diagnostics, diagnostics_size)) return false;
    if (out_new_scene_object_index) *out_new_scene_object_index = sceneSettings.objectCount - 1;
    return true;
}

bool SceneEditorDocumentRemoveForSceneIndex(int scene_object_index,
                                            char* diagnostics,
                                            size_t diagnostics_size) {
    json_object* object = document_object_for_scene_index(scene_object_index, diagnostics, diagnostics_size);
    json_object* objects = NULL;
    if (!object || !json_object_object_get_ex(s_document.root, "objects", &objects)) return false;
    for (size_t i = 0; i < json_object_array_length(objects); ++i) {
        if (json_object_array_get_idx(objects, (int)i) == object) {
            if (!document_begin_command(diagnostics, diagnostics_size)) return false;
            json_object_array_del_idx(objects, (int)i, 1);
            return document_finish_command(diagnostics, diagnostics_size);
        }
    }
    document_diag(diagnostics, diagnostics_size, "selected object is missing from document");
    return false;
}

bool SceneEditorDocumentRenameForSceneIndex(int scene_object_index,
                                            const char* display_name,
                                            char* diagnostics,
                                            size_t diagnostics_size) {
    json_object* object = NULL;
    size_t length = display_name ? strlen(display_name) : 0u;
    if (length == 0u || length > 96u) {
        document_diag(diagnostics, diagnostics_size, "display name must contain 1 to 96 characters");
        return false;
    }
    object = document_object_for_scene_index(scene_object_index, diagnostics, diagnostics_size);
    if (!object || !document_begin_command(diagnostics, diagnostics_size)) return false;
    json_object_object_add(object, "display_name", json_object_new_string(display_name));
    return document_finish_command(diagnostics, diagnostics_size);
}

bool SceneEditorDocumentObjectLabel(int scene_object_index, char* label, size_t size) {
    json_object* object = document_object_for_scene_index(scene_object_index, NULL, 0);
    json_object* value = NULL;
    if (!label || !size) return false;
    label[0] = '\0';
    if (!object) return false;
    if (!json_object_object_get_ex(object, "display_name", &value))
        (void)json_object_object_get_ex(object, "object_id", &value);
    if (!value || !json_object_is_type(value, json_type_string)) return false;
    snprintf(label, size, "%s", json_object_get_string(value));
    return true;
}

bool SceneEditorDocumentCanUndo(void) { return s_document.undo_count > 0; }
bool SceneEditorDocumentCanRedo(void) { return s_document.redo_count > 0; }

static bool document_history_move(char** from,
                                  int* from_count,
                                  char** to,
                                  int* to_count,
                                  char* diagnostics,
                                  size_t diagnostics_size) {
    char* current = NULL;
    char* target = NULL;
    json_object* root = NULL;
    json_object* previous_root = NULL;
    if (*from_count <= 0) return false;
    current = document_serialize_root(s_document.root, NULL);
    if (!current) return false;
    target = from[--(*from_count)];
    root = document_parse_valid(target, diagnostics, diagnostics_size);
    if (!root) {
        from[(*from_count)++] = target;
        free(current);
        return false;
    }
    previous_root = s_document.root;
    s_document.root = root;
    if (!document_apply_current(diagnostics, diagnostics_size)) {
        s_document.root = previous_root;
        json_object_put(root);
        from[(*from_count)++] = target;
        free(current);
        (void)document_apply_current(NULL, 0u);
        return false;
    }
    if (!document_push(to, to_count, current)) {
        s_document.root = previous_root;
        json_object_put(root);
        from[(*from_count)++] = target;
        free(current);
        (void)document_apply_current(NULL, 0u);
        return false;
    }
    json_object_put(previous_root);
    free(target);
    s_document.dirty = true;
    s_document.revision += 1u;
    return true;
}

bool SceneEditorDocumentUndo(char* diagnostics, size_t diagnostics_size) {
    return document_history_move(s_document.undo,
                                 &s_document.undo_count,
                                 s_document.redo,
                                 &s_document.redo_count,
                                 diagnostics,
                                 diagnostics_size);
}

bool SceneEditorDocumentRedo(char* diagnostics, size_t diagnostics_size) {
    return document_history_move(s_document.redo,
                                 &s_document.redo_count,
                                 s_document.undo,
                                 &s_document.undo_count,
                                 diagnostics,
                                 diagnostics_size);
}

static bool document_parent_path(const char* path, char* out, size_t out_size) {
    char* slash = NULL;
    if (!path || strlen(path) >= out_size) return false;
    snprintf(out, out_size, "%s", path);
    slash = strrchr(out, '/');
    if (!slash) return false;
    if (slash == out) slash[1] = '\0';
    else *slash = '\0';
    return true;
}

bool SceneEditorDocumentSave(char* diagnostics, size_t diagnostics_size) {
    char lock_path[PATH_MAX] = {0};
    char parent[PATH_MAX] = {0};
    char temp_path[PATH_MAX] = {0};
    char* disk = NULL;
    size_t disk_size = 0u;
    char* serialized = NULL;
    char* new_baseline = NULL;
    size_t serialized_size = 0u;
    int lock_fd = -1;
    int temp_fd = -1;
    int dir_fd = -1;
    bool ok = false;
    if (!s_document.root || !document_parent_path(s_document.path, parent, sizeof(parent))) {
        document_diag(diagnostics, diagnostics_size, "no runtime scene document is open");
        return false;
    }
    if (snprintf(lock_path, sizeof(lock_path), "%s/.%s.managed.lock", parent,
                 strrchr(s_document.path, '/') + 1) >= (int)sizeof(lock_path)) {
        document_diag(diagnostics, diagnostics_size, "runtime scene lock path is too long");
        return false;
    }
    lock_fd = open(lock_path, O_CREAT | O_RDWR, 0600);
    if (lock_fd < 0 || flock(lock_fd, LOCK_EX) != 0) {
        document_diag(diagnostics, diagnostics_size, "failed to acquire runtime scene lock");
        goto done;
    }
    if (!document_read_file(s_document.path, &disk, &disk_size) ||
        disk_size != s_document.baseline_size ||
        memcmp(disk, s_document.baseline_bytes, disk_size) != 0) {
        document_diag(diagnostics, diagnostics_size, "scene changed concurrently; reopen before saving");
        goto done;
    }
    serialized = document_serialize_root(s_document.root, &serialized_size);
    if (!serialized) {
        document_diag(diagnostics, diagnostics_size, "failed to serialize runtime scene");
        goto done;
    }
    new_baseline = document_copy_bytes(serialized, serialized_size);
    if (!new_baseline) {
        document_diag(diagnostics, diagnostics_size, "failed to allocate saved scene baseline");
        goto done;
    }
    if (snprintf(temp_path, sizeof(temp_path), "%s/.scene-editor-save-XXXXXX", parent) >=
        (int)sizeof(temp_path)) {
        document_diag(diagnostics, diagnostics_size, "runtime scene temporary path is too long");
        goto done;
    }
    temp_fd = mkstemp(temp_path);
    if (temp_fd < 0) {
        document_diag(diagnostics, diagnostics_size, "failed to create runtime scene temporary file");
        goto done;
    }
    for (size_t written = 0u; written < serialized_size;) {
        ssize_t count = write(temp_fd, serialized + written, serialized_size - written);
        if (count <= 0) {
            document_diag(diagnostics, diagnostics_size, "failed to write runtime scene temporary file");
            goto done;
        }
        written += (size_t)count;
    }
    if (fsync(temp_fd) != 0 || close(temp_fd) != 0) {
        temp_fd = -1;
        document_diag(diagnostics, diagnostics_size, "failed to sync runtime scene temporary file");
        goto done;
    }
    temp_fd = -1;
    if (rename(temp_path, s_document.path) != 0) {
        document_diag(diagnostics, diagnostics_size, "failed to commit runtime scene atomically");
        goto done;
    }
    temp_path[0] = '\0';
    free(s_document.baseline_bytes);
    s_document.baseline_bytes = new_baseline;
    new_baseline = NULL;
    s_document.baseline_size = serialized_size;
    s_document.dirty = false;
    dir_fd = open(parent, O_RDONLY);
    if (dir_fd < 0 || fsync(dir_fd) != 0) {
        document_diag(diagnostics, diagnostics_size, "runtime scene committed but directory sync failed");
        goto done;
    }
    document_diag(diagnostics, diagnostics_size, "ok");
    ok = true;
done:
    if (temp_fd >= 0) close(temp_fd);
    if (temp_path[0]) unlink(temp_path);
    if (dir_fd >= 0) close(dir_fd);
    free(disk);
    free(serialized);
    free(new_baseline);
    if (lock_fd >= 0) {
        (void)flock(lock_fd, LOCK_UN);
        close(lock_fd);
    }
    return ok;
}

bool SceneEditorDocumentMergeOverlayAndSave(const char* overlay_json,
                                            char* diagnostics,
                                            size_t diagnostics_size) {
    char* current = NULL;
    char* merged = NULL;
    json_object* merged_root = NULL;
    bool ok = false;
    if (!s_document.root || !overlay_json) return false;
    current = document_serialize_root(s_document.root, NULL);
    if (!current) return false;
    ok = runtime_scene_bridge_writeback_ray_overlay_json(current,
                                                         overlay_json,
                                                         &merged,
                                                         diagnostics,
                                                         diagnostics_size);
    free(current);
    if (!ok || !merged) {
        free(merged);
        return false;
    }
    merged_root = document_parse_valid(merged, diagnostics, diagnostics_size);
    free(merged);
    if (!merged_root) return false;
    json_object_put(s_document.root);
    s_document.root = merged_root;
    s_document.dirty = true;
    s_document.revision += 1u;
    if (!SceneEditorDocumentSave(diagnostics, diagnostics_size)) return false;
    return document_apply_saved_file(diagnostics, diagnostics_size);
}

bool SceneEditorDocumentAdoptCandidateAsCommand(const char* candidate_path,
                                                char* diagnostics,
                                                size_t diagnostics_size) {
    char* bytes = NULL;
    size_t size = 0u;
    json_object* root = NULL;
    char candidate_parent[PATH_MAX] = {0};
    char document_parent[PATH_MAX] = {0};
    bool was_dirty = s_document.dirty;
    unsigned long long previous_revision = s_document.revision;
    if (!s_document.root || !candidate_path ||
        !document_parent_path(candidate_path, candidate_parent, sizeof(candidate_parent)) ||
        !document_parent_path(s_document.path, document_parent, sizeof(document_parent)) ||
        strcmp(candidate_parent, document_parent) != 0 ||
        !document_read_file(candidate_path, &bytes, &size)) {
        document_diag(diagnostics, diagnostics_size, "managed candidate is unavailable or outside the scene directory");
        return false;
    }
    root = document_parse_valid(bytes, diagnostics, diagnostics_size);
    free(bytes);
    if (!root) {
        return false;
    }
    if (!document_begin_command(diagnostics, diagnostics_size)) {
        json_object_put(root);
        return false;
    }
    json_object_put(s_document.root);
    s_document.root = root;
    s_document.dirty = true;
    s_document.revision += 1u;
    if (!SceneEditorDocumentSave(diagnostics, diagnostics_size)) {
        document_rollback_command();
        s_document.dirty = was_dirty;
        s_document.revision = previous_revision;
        (void)document_apply_current(NULL, 0u);
        return false;
    }
    if (!document_commit_command_history()) {
        document_diag(diagnostics, diagnostics_size, "managed candidate saved but history commit failed");
        return false;
    }
    return document_apply_saved_file(diagnostics, diagnostics_size);
}

const char* SceneEditorDocumentUnitLabel(void) {
    json_object* unit=NULL;
    if (s_document.root && json_object_object_get_ex(s_document.root,"unit_system",&unit) &&
        json_object_is_type(unit,json_type_string)) {
        const char* label=json_object_get_string(unit);
        if (label && label[0] && strcmp(label,"unitless")!=0 && strcmp(label,"unknown")!=0 && strcmp(label,"scene_units")!=0)
            return label;
    }
    return "scene units";
}
double SceneEditorDocumentWorldScale(void) {
    json_object* scale=NULL;
    double value=1.0;
    if (s_document.root && json_object_object_get_ex(s_document.root,"world_scale",&scale))
        value=json_object_get_double(scale);
    return isfinite(value) && value>0 ? value : 1.0;
}
