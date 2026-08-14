#include "import/compound_scene_static_room_import.h"

#include <stdio.h>
#include <string.h>

static void set_failure(RayCompoundSceneStaticRoomImportFailure* failure,
                        RayCompoundSceneStaticRoomImportFailure value) {
    if (failure) *failure = value;
}

static bool next_line(const char** cursor, char* line, size_t line_size) {
    if (!cursor || !*cursor || !line || line_size < 2u) return false;
    const char* newline = strchr(*cursor, '\n');
    if (!newline || newline == *cursor ||
        (size_t)(newline - *cursor) >= line_size) return false;
    const size_t size = (size_t)(newline - *cursor);
    memcpy(line, *cursor, size);
    line[size] = '\0';
    *cursor = newline + 1;
    return true;
}

bool ray_compound_scene_static_room_z_up_v2_parse(
    const char* text, RayCompoundSceneStaticRoom* output,
    RayCompoundSceneStaticRoomImportFailure* failure) {
    RayCompoundSceneStaticRoom candidate;
    const char* cursor = text;
    char line[4096];
    unsigned long long request = 0, spec = 0, result = 0, handoff = 0;
    unsigned long long set_digest = 0, digest = 0;
    set_failure(failure, RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_NONE);
    if (!text || !output) {
        set_failure(failure, RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_INPUT);
        return false;
    }
    ray_compound_scene_static_room_init(&candidate);
    if (!next_line(&cursor, line, sizeof(line)) ||
        strcmp(line, RAY_COMPOUND_SCENE_STATIC_ROOM_Z_UP_SCHEMA) ||
        !next_line(&cursor, line, sizeof(line)) ||
        sscanf(line, "version %u", &candidate.schema_version) != 1 ||
        !next_line(&cursor, line, sizeof(line)) ||
        sscanf(line, "coordinate_system %63s", candidate.coordinate_system) != 1 ||
        !next_line(&cursor, line, sizeof(line)) ||
        sscanf(line, "provenance %llx %llx %llx %llx",
               &request, &spec, &result, &handoff) != 4 ||
        !next_line(&cursor, line, sizeof(line)) ||
        sscanf(line, "surface_count %u", &candidate.surface_count) != 1 ||
        candidate.surface_count != RAY_COMPOUND_SCENE_STATIC_ROOM_SURFACE_COUNT) {
        set_failure(failure, RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_ENVELOPE);
        return false;
    }
    snprintf(candidate.schema, sizeof(candidate.schema), "%s",
             RAY_COMPOUND_SCENE_STATIC_ROOM_Z_UP_SCHEMA);
    snprintf(candidate.artifact_id, sizeof(candidate.artifact_id), "%s",
             "native_z_up_pair_room_static_room_v2");
    snprintf(candidate.room_id, sizeof(candidate.room_id), "%s",
             "native_z_up_pair_room_2_dynamic_6_static_v2");
    candidate.provenance.pair_request_digest = request;
    candidate.provenance.room_spec_digest = spec;
    candidate.provenance.pair_room_result_digest = result;
    candidate.provenance.transform_fixture_digest = handoff;
    for (size_t index = 0; index < candidate.surface_count; ++index) {
        RayCompoundSceneStaticRoomSurface* surface = &candidate.surfaces[index];
        int role = -1;
        unsigned bit = 0;
        unsigned long long surface_digest = 0;
        surface->schema_version = 2u;
        surface->collision_box_orientation.w = 1.0;
        if (!next_line(&cursor, line, sizeof(line)) ||
            sscanf(line,
                "surface %d %31s %d %u "
                "%la %la %la %la %la %la %la %la %la "
                "%la %la %la %la %la %la %la %la %la "
                "%la %la %la %la %llx",
                &role, surface->surface_id, &surface->body_id, &bit,
                &surface->collision_box_center_m.x,
                &surface->collision_box_center_m.y,
                &surface->collision_box_center_m.z,
                &surface->collision_box_half_extent_m.x,
                &surface->collision_box_half_extent_m.y,
                &surface->collision_box_half_extent_m.z,
                &surface->interior_plane_origin_m.x,
                &surface->interior_plane_origin_m.y,
                &surface->interior_plane_origin_m.z,
                &surface->inward_normal.x, &surface->inward_normal.y,
                &surface->inward_normal.z, &surface->tangent_u.x,
                &surface->tangent_u.y, &surface->tangent_u.z,
                &surface->tangent_v.x, &surface->tangent_v.y,
                &surface->tangent_v.z, &surface->half_extent_u_m,
                &surface->half_extent_v_m, &surface->restitution,
                &surface->friction, &surface_digest) != 27) {
            set_failure(failure, RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_ENVELOPE);
            return false;
        }
        surface->role = (RayCompoundSceneStaticRoomRole)role;
        surface->contact_mask_bit = (uint8_t)bit;
        surface->surface_digest = surface_digest;
    }
    if (!next_line(&cursor, line, sizeof(line)) ||
        sscanf(line, "surface_set_digest %llx", &set_digest) != 1 ||
        !next_line(&cursor, line, sizeof(line)) ||
        sscanf(line, "digest %llx", &digest) != 1 ||
        !next_line(&cursor, line, sizeof(line)) || strcmp(line, "end") ||
        *cursor != '\0') {
        set_failure(failure, RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_ENVELOPE);
        return false;
    }
    candidate.surface_set_digest = set_digest;
    candidate.artifact_digest = digest;
    if (!ray_compound_scene_static_room_validate(&candidate)) {
        set_failure(failure, RAY_COMPOUND_SCENE_STATIC_ROOM_IMPORT_PROVENANCE);
        return false;
    }
    *output = candidate;
    return true;
}
