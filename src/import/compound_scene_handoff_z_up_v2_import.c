#include "import/compound_scene_handoff_import.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_failure(RayCompoundSceneImportFailure* failure,
                        RayCompoundSceneImportFailure value) {
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

bool ray_compound_scene_handoff_z_up_v2_parse(
    const char* text, RayCompoundSceneHandoff* output,
    RayCompoundSceneImportFailure* failure) {
    RayCompoundSceneHandoff candidate;
    const char* cursor = text;
    char line[4096];
    unsigned long long descriptor = 0, request = 0, room_spec = 0;
    unsigned long long result = 0, seed = 0, digest = 0;
    set_failure(failure, RAY_COMPOUND_SCENE_IMPORT_FAILURE_NONE);
    if (!text || !output) {
        set_failure(failure, RAY_COMPOUND_SCENE_IMPORT_FAILURE_INPUT);
        return false;
    }
    ray_compound_scene_handoff_init(&candidate);
    if (!next_line(&cursor, line, sizeof(line)) ||
        strcmp(line, RAY_COMPOUND_SCENE_HANDOFF_Z_UP_SCHEMA) ||
        !next_line(&cursor, line, sizeof(line)) ||
        sscanf(line, "version %u", &candidate.schema_version) != 1 ||
        !next_line(&cursor, line, sizeof(line)) ||
        sscanf(line, "coordinate_system %63s", candidate.coordinate_system) != 1 ||
        !next_line(&cursor, line, sizeof(line)) ||
        sscanf(line, "provenance %llx %llx %llx %llx %llx %la",
               &descriptor, &request, &room_spec, &result, &seed,
               &candidate.fixed_dt_s) != 6) {
        set_failure(failure, RAY_COMPOUND_SCENE_IMPORT_FAILURE_ENVELOPE);
        return false;
    }
    snprintf(candidate.schema, sizeof(candidate.schema), "%s",
             RAY_COMPOUND_SCENE_HANDOFF_Z_UP_SCHEMA);
    snprintf(candidate.handoff_id, sizeof(candidate.handoff_id), "%s",
             "native_z_up_pair_room_handoff_v2");
    snprintf(candidate.fixture_reference, sizeof(candidate.fixture_reference),
             "%s", "native_z_up_descriptor_v2");
    candidate.descriptor_digest = descriptor;
    candidate.request_digest = request;
    candidate.room_spec_digest = room_spec;
    candidate.result_digest = result;
    candidate.seed = seed;

    for (size_t body = 0; body < RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT;
         ++body) {
        RayCompoundSceneSourceBinding* binding = &candidate.bindings[body];
        unsigned long long binding_descriptor = 0, geometry = 0;
        unsigned long long body_hash = 0, binding_digest = 0;
        if (!next_line(&cursor, line, sizeof(line)) ||
            sscanf(line,
                "source %63s %llx %zu %d %63s %64s %31s "
                "%la %la %la "
                "%la %la %la %la %la %la %la %la %la "
                "%llx %llx %llx",
                binding->schema, &binding_descriptor, &binding->body_index,
                &binding->body_id, binding->source_asset_id,
                binding->source_sha256, binding->representation_role,
                &binding->source_center_m.x, &binding->source_center_m.y,
                &binding->source_center_m.z,
                &binding->principal_to_source.m[0][0],
                &binding->principal_to_source.m[0][1],
                &binding->principal_to_source.m[0][2],
                &binding->principal_to_source.m[1][0],
                &binding->principal_to_source.m[1][1],
                &binding->principal_to_source.m[1][2],
                &binding->principal_to_source.m[2][0],
                &binding->principal_to_source.m[2][1],
                &binding->principal_to_source.m[2][2],
                &geometry, &body_hash, &binding_digest) != 22) {
            set_failure(failure, RAY_COMPOUND_SCENE_IMPORT_FAILURE_ENVELOPE);
            return false;
        }
        binding->descriptor_digest = binding_descriptor;
        binding->geometry_hash = geometry;
        binding->body_hash = body_hash;
        binding->binding_digest = binding_digest;
    }

    if (!next_line(&cursor, line, sizeof(line)) ||
        sscanf(line, "frame_count %zu", &candidate.frame_count) != 1 ||
        candidate.frame_count < 2u ||
        candidate.frame_count > RAY_COMPOUND_SCENE_HANDOFF_MAX_FRAMES) {
        set_failure(failure, RAY_COMPOUND_SCENE_IMPORT_FAILURE_ENVELOPE);
        return false;
    }
    candidate.frames = calloc(candidate.frame_count, sizeof(*candidate.frames));
    if (!candidate.frames) {
        set_failure(failure, RAY_COMPOUND_SCENE_IMPORT_FAILURE_ALLOCATION);
        return false;
    }
    for (size_t index = 0; index < candidate.frame_count; ++index) {
        RayCompoundSceneFrame* frame = &candidate.frames[index];
        unsigned long long tick = 0;
        RayCompoundSceneBodyTransform* a = &frame->bodies[0];
        RayCompoundSceneBodyTransform* b = &frame->bodies[1];
        if (!next_line(&cursor, line, sizeof(line)) ||
            sscanf(line,
                "frame %llu %d %la %la %la %la %la %la %la "
                "%d %la %la %la %la %la %la %la",
                &tick, &a->body_id, &a->position_m.x, &a->position_m.y,
                &a->position_m.z, &a->orientation.w, &a->orientation.x,
                &a->orientation.y, &a->orientation.z, &b->body_id,
                &b->position_m.x, &b->position_m.y, &b->position_m.z,
                &b->orientation.w, &b->orientation.x, &b->orientation.y,
                &b->orientation.z) != 17) {
            ray_compound_scene_handoff_free(&candidate);
            set_failure(failure, RAY_COMPOUND_SCENE_IMPORT_FAILURE_ENVELOPE);
            return false;
        }
        frame->tick = tick;
    }
    if (!next_line(&cursor, line, sizeof(line)) ||
        sscanf(line, "digest %llx", &digest) != 1 ||
        !next_line(&cursor, line, sizeof(line)) || strcmp(line, "end") ||
        *cursor != '\0') {
        ray_compound_scene_handoff_free(&candidate);
        set_failure(failure, RAY_COMPOUND_SCENE_IMPORT_FAILURE_ENVELOPE);
        return false;
    }
    candidate.handoff_digest = digest;
    if (!ray_compound_scene_handoff_validate(&candidate)) {
        ray_compound_scene_handoff_free(&candidate);
        set_failure(failure, RAY_COMPOUND_SCENE_IMPORT_FAILURE_PROVENANCE);
        return false;
    }
    ray_compound_scene_handoff_free(output);
    *output = candidate;
    return true;
}
