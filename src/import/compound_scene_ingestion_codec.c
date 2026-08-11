#include "import/compound_scene_ingestion_codec.h"

#include "app/ray_tracing_request_utils.h"

#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_diag(char* out, size_t n, const char* text) {
    if (out && n) snprintf(out, n, "%s", text);
}

void ray_compound_scene_ingestion_file_init(RayCompoundSceneIngestionFile* file) {
    if (!file) return;
    memset(file, 0, sizeof(*file));
    ray_compound_scene_handoff_init(&file->handoff);
    ray_compound_scene_static_room_init(&file->room);
}

void ray_compound_scene_ingestion_file_free(RayCompoundSceneIngestionFile* file) {
    if (!file) return;
    ray_compound_scene_handoff_free(&file->handoff);
    memset(file, 0, sizeof(*file));
}

static bool copy_json_string(json_object* object, const char* key,
                             char* output, size_t output_size) {
    const char* value = NULL;
    return RayTracingJsonGetString(object, key, &value) &&
           RayTracingCopyString(output, output_size, value);
}

bool ray_compound_scene_ingestion_file_read(const char* path,
                                            RayCompoundSceneIngestionFile* output,
                                            char* diagnostics,
                                            size_t diagnostics_size) {
    RayCompoundSceneIngestionFile candidate;
    char* text = NULL;
    char directory[PATH_MAX] = {0};
    json_object* root = NULL;
    json_object* bodies = NULL;
    json_object* room = NULL;
    const char* schema = NULL;
    int tick = 0;
    bool ok = false;
    ray_compound_scene_ingestion_file_init(&candidate);
    set_diag(diagnostics, diagnostics_size, "compound ingestion file invalid");
    if (!path || !path[0] || !output || !RayTracingReadTextFile(path, &text)) goto done;
    root = json_tokener_parse(text);
    free(text);
    text = NULL;
    if (!root || !json_object_is_type(root, json_type_object) ||
        !RayTracingJsonGetString(root, "schema", &schema) ||
        strcmp(schema, RAY_COMPOUND_SCENE_INGESTION_SCHEMA) ||
        !RayTracingJsonGetInt(root, "tick", &tick) || tick < 0 ||
        !copy_json_string(root, "handoff_path", candidate.handoff_path,
                          sizeof(candidate.handoff_path)) ||
        !copy_json_string(root, "room_path", candidate.room_path,
                          sizeof(candidate.room_path)) ||
        !json_object_object_get_ex(root, "bodies", &bodies) ||
        !json_object_is_type(bodies, json_type_array) ||
        json_object_array_length(bodies) != RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT ||
        !json_object_object_get_ex(root, "room", &room) ||
        !json_object_is_type(room, json_type_array) ||
        json_object_array_length(room) != RAY_COMPOUND_SCENE_STATIC_ROOM_SURFACE_COUNT) {
        set_diag(diagnostics, diagnostics_size, "compound ingestion file schema is invalid");
        goto done;
    }
    RayTracingDirnameOf(path, directory, sizeof(directory));
    if (!RayTracingResolveRequestInputPath(directory, candidate.handoff_path,
                                           candidate.handoff_path,
                                           sizeof(candidate.handoff_path)) ||
        !RayTracingResolveRequestInputPath(directory, candidate.room_path,
                                           candidate.room_path, sizeof(candidate.room_path)) ||
        !ray_compound_scene_handoff_read(candidate.handoff_path, &candidate.handoff, NULL) ||
        !ray_compound_scene_static_room_read(candidate.room_path, &candidate.room, NULL)) {
        set_diag(diagnostics, diagnostics_size, "compound ingestion source path or provenance is invalid");
        goto done;
    }
    ray_compound_scene_ingestion_descriptor_init(&candidate.descriptor,
                                                 &candidate.handoff, &candidate.room);
    candidate.descriptor.tick = (uint64_t)tick;
    for (size_t i = 0; i < RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT; ++i) {
        json_object* body = json_object_array_get_idx(bodies, (int)i);
        int body_id = -1;
        RayCompoundSceneRendererBinding* binding = NULL;
        if (!body || !json_object_is_type(body, json_type_object) ||
            !RayTracingJsonGetInt(body, "body_id", &body_id) ||
            !(binding = (RayCompoundSceneRendererBinding*)
                ray_compound_scene_binding_manifest_find_body(&candidate.descriptor.bindings,
                                                              body_id)) ||
            !copy_json_string(body, "object_id", binding->object_id,
                              sizeof(binding->object_id)) ||
            !copy_json_string(body, "mesh_asset_id", binding->mesh_asset_id,
                              sizeof(binding->mesh_asset_id))) {
            set_diag(diagnostics, diagnostics_size, "compound ingestion body binding is invalid");
            goto done;
        }
    }
    for (size_t i = 0; i < RAY_COMPOUND_SCENE_STATIC_ROOM_SURFACE_COUNT; ++i) {
        json_object* role = json_object_array_get_idx(room, (int)i);
        if (!role || !json_object_is_type(role, json_type_object) ||
            !copy_json_string(role, "material_id", candidate.descriptor.room_material_ids[i],
                              sizeof(candidate.descriptor.room_material_ids[i])) ||
            (candidate.descriptor.room_visible[i] &&
             !copy_json_string(role, "object_id", candidate.descriptor.room_object_ids[i],
                               sizeof(candidate.descriptor.room_object_ids[i])))) {
            set_diag(diagnostics, diagnostics_size, "compound ingestion room binding is invalid");
            goto done;
        }
    }
    if (!ray_compound_scene_ingestion_descriptor_validate(&candidate.descriptor,
                                                           &candidate.handoff,
                                                           &candidate.room)) {
        set_diag(diagnostics, diagnostics_size, "compound ingestion descriptor validation failed");
        goto done;
    }
    *output = candidate;
    memset(&candidate, 0, sizeof(candidate));
    set_diag(diagnostics, diagnostics_size, "ok");
    ok = true;
done:
    if (root) json_object_put(root);
    free(text);
    ray_compound_scene_ingestion_file_free(&candidate);
    return ok;
}
