#include "import/compound_scene_handoff_import.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct RayCompoundSceneReader {
    const unsigned char* bytes;
    size_t size;
    size_t offset;
    bool ok;
} RayCompoundSceneReader;

static void set_failure(RayCompoundSceneImportFailure* failure,
                        RayCompoundSceneImportFailure value) {
    if (failure)
        *failure = value;
}

static uint64_t hash_bytes(uint64_t hash, const void* data, size_t size) {
    const unsigned char* bytes = data;
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t hash_u64(uint64_t hash, uint64_t value) {
    unsigned char bytes[8];
    for (size_t i = 0; i < sizeof(bytes); ++i)
        bytes[i] = (unsigned char)(value >> (8u * i));
    return hash_bytes(hash, bytes, sizeof(bytes));
}

static uint64_t hash_double(uint64_t hash, double value) {
    uint64_t bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    return hash_u64(hash, bits);
}

static uint64_t hash_string(uint64_t hash, const char* value) {
    return hash_bytes(hash, value, strlen(value) + 1u);
}

static bool finite_vec3(RayCompoundSceneVec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool unit_quat(RayCompoundSceneQuat value) {
    if (!isfinite(value.w) || !isfinite(value.x) || !isfinite(value.y) ||
        !isfinite(value.z))
        return false;
    const double norm_squared = value.w * value.w + value.x * value.x +
        value.y * value.y + value.z * value.z;
    return fabs(norm_squared - 1.0) <= 1e-6;
}

static bool lowercase_sha256(const char* value) {
    if (!value || strlen(value) != 64u)
        return false;
    for (size_t i = 0; i < 64u; ++i) {
        if (!((value[i] >= '0' && value[i] <= '9') ||
              (value[i] >= 'a' && value[i] <= 'f')))
            return false;
    }
    return true;
}

static bool safe_reference(const char* value) {
    if (!value || !value[0] || value[0] == '/' || strstr(value, "..") ||
        strchr(value, '\\'))
        return false;
    for (const unsigned char* p = (const unsigned char*)value; *p; ++p) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '_' || *p == '-' ||
              *p == '.' || *p == '/'))
            return false;
    }
    return true;
}

static double determinant(RayCompoundSceneMat3 value) {
    return value.m[0][0] *
            (value.m[1][1] * value.m[2][2] -
             value.m[1][2] * value.m[2][1]) -
        value.m[0][1] *
            (value.m[1][0] * value.m[2][2] -
             value.m[1][2] * value.m[2][0]) +
        value.m[0][2] *
            (value.m[1][0] * value.m[2][1] -
             value.m[1][1] * value.m[2][0]);
}

static bool orthonormal_frame(RayCompoundSceneMat3 value) {
    if (!isfinite(determinant(value)) ||
        fabs(determinant(value) - 1.0) > 1e-8)
        return false;
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            double dot = 0.0;
            for (size_t row = 0; row < 3; ++row) {
                if (!isfinite(value.m[row][i]) ||
                    !isfinite(value.m[row][j]))
                    return false;
                dot += value.m[row][i] * value.m[row][j];
            }
            if (fabs(dot - (i == j ? 1.0 : 0.0)) > 1e-8)
                return false;
        }
    }
    return true;
}

void ray_compound_scene_handoff_init(RayCompoundSceneHandoff* handoff) {
    if (handoff)
        memset(handoff, 0, sizeof(*handoff));
}

void ray_compound_scene_handoff_free(RayCompoundSceneHandoff* handoff) {
    if (!handoff)
        return;
    free(handoff->frames);
    ray_compound_scene_handoff_init(handoff);
}

uint64_t ray_compound_scene_source_binding_digest(
    const RayCompoundSceneSourceBinding* binding) {
    if (!binding)
        return 0;
    uint64_t hash = UINT64_C(1469598103934665603);
    hash = hash_string(hash, binding->schema);
    hash = hash_u64(hash, binding->fixture_digest);
    hash = hash_u64(hash, binding->body_index);
    hash = hash_u64(hash, (uint64_t)binding->body_id);
    hash = hash_string(hash, binding->source_asset_id);
    hash = hash_string(hash, binding->source_sha256);
    hash = hash_string(hash, binding->representation_role);
    hash = hash_double(hash, binding->source_center_m.x);
    hash = hash_double(hash, binding->source_center_m.y);
    hash = hash_double(hash, binding->source_center_m.z);
    for (size_t row = 0; row < 3; ++row)
        for (size_t column = 0; column < 3; ++column)
            hash = hash_double(hash,
                binding->principal_to_source.m[row][column]);
    return hash;
}

bool ray_compound_scene_source_binding_validate(
    const RayCompoundSceneSourceBinding* binding) {
    if (!binding || strcmp(binding->schema,
            RAY_COMPOUND_SCENE_SOURCE_BINDING_SCHEMA) ||
        !binding->fixture_digest ||
        binding->body_index >= RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT ||
        binding->body_id <= 0 || !binding->source_asset_id[0] ||
        !lowercase_sha256(binding->source_sha256) ||
        (strcmp(binding->representation_role, "authored_reference") &&
         strcmp(binding->representation_role, "compiler_output")) ||
        !finite_vec3(binding->source_center_m) ||
        !orthonormal_frame(binding->principal_to_source))
        return false;
    return binding->binding_digest ==
        ray_compound_scene_source_binding_digest(binding);
}

uint64_t ray_compound_scene_handoff_digest(
    const RayCompoundSceneHandoff* handoff) {
    if (!handoff)
        return 0;
    uint64_t hash = UINT64_C(1469598103934665603);
    hash = hash_string(hash, handoff->schema);
    hash = hash_u64(hash, handoff->schema_version);
    hash = hash_string(hash, handoff->handoff_id);
    hash = hash_string(hash, handoff->fixture_reference);
    hash = hash_u64(hash, handoff->fixture_digest);
    hash = hash_u64(hash, handoff->seed);
    hash = hash_double(hash, handoff->fixed_dt_s);
    for (size_t body = 0; body < RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT;
         ++body)
        hash = hash_u64(hash, handoff->bindings[body].binding_digest);
    hash = hash_u64(hash, handoff->frame_count);
    if (!handoff->frames)
        return hash;
    for (size_t i = 0; i < handoff->frame_count; ++i) {
        hash = hash_u64(hash, handoff->frames[i].tick);
        for (size_t body = 0; body < RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT;
             ++body) {
            const RayCompoundSceneBodyTransform* transform =
                &handoff->frames[i].bodies[body];
            hash = hash_u64(hash, (uint64_t)transform->body_id);
            hash = hash_double(hash, transform->position_m.x);
            hash = hash_double(hash, transform->position_m.y);
            hash = hash_double(hash, transform->position_m.z);
            hash = hash_double(hash, transform->orientation.w);
            hash = hash_double(hash, transform->orientation.x);
            hash = hash_double(hash, transform->orientation.y);
            hash = hash_double(hash, transform->orientation.z);
        }
    }
    return hash;
}

bool ray_compound_scene_handoff_validate(
    const RayCompoundSceneHandoff* handoff) {
    if (!handoff || strcmp(handoff->schema,
            RAY_COMPOUND_SCENE_HANDOFF_SCHEMA) ||
        handoff->schema_version != RAY_COMPOUND_SCENE_HANDOFF_CODEC_VERSION ||
        strcmp(handoff->handoff_id, RAY_COMPOUND_SCENE_HANDOFF_ID) ||
        !safe_reference(handoff->fixture_reference) ||
        !handoff->fixture_digest || !handoff->seed ||
        !isfinite(handoff->fixed_dt_s) || handoff->fixed_dt_s <= 0.0 ||
        handoff->frame_count < 2 ||
        handoff->frame_count > RAY_COMPOUND_SCENE_HANDOFF_MAX_FRAMES ||
        !handoff->frames)
        return false;
    for (size_t body = 0; body < RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT;
         ++body) {
        const RayCompoundSceneSourceBinding* binding =
            &handoff->bindings[body];
        if (!ray_compound_scene_source_binding_validate(binding) ||
            binding->fixture_digest != handoff->fixture_digest ||
            binding->body_index != body ||
            (body && binding->body_id == handoff->bindings[0].body_id) ||
            (body && !strcmp(binding->source_asset_id,
                handoff->bindings[0].source_asset_id)))
            return false;
    }
    for (size_t i = 0; i < handoff->frame_count; ++i) {
        const RayCompoundSceneFrame* frame = &handoff->frames[i];
        if (frame->tick != i)
            return false;
        for (size_t body = 0; body < RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT;
             ++body) {
            const RayCompoundSceneBodyTransform* transform =
                &frame->bodies[body];
            if (transform->body_id != handoff->bindings[body].body_id ||
                !finite_vec3(transform->position_m) ||
                !unit_quat(transform->orientation))
                return false;
        }
    }
    return handoff->handoff_digest ==
        ray_compound_scene_handoff_digest(handoff);
}

static void reader_get(RayCompoundSceneReader* reader, void* output,
                       size_t size) {
    if (!reader || !reader->ok || !output || reader->offset > reader->size ||
        size > reader->size - reader->offset) {
        if (reader)
            reader->ok = false;
        return;
    }
    memcpy(output, reader->bytes + reader->offset, size);
    reader->offset += size;
}

static uint32_t reader_u32(RayCompoundSceneReader* reader) {
    unsigned char bytes[4] = {0};
    reader_get(reader, bytes, sizeof(bytes));
    uint32_t value = 0;
    for (size_t i = 0; i < sizeof(bytes); ++i)
        value |= (uint32_t)bytes[i] << (8u * i);
    return value;
}

static uint64_t reader_u64(RayCompoundSceneReader* reader) {
    unsigned char bytes[8] = {0};
    reader_get(reader, bytes, sizeof(bytes));
    uint64_t value = 0;
    for (size_t i = 0; i < sizeof(bytes); ++i)
        value |= (uint64_t)bytes[i] << (8u * i);
    return value;
}

static double reader_double(RayCompoundSceneReader* reader) {
    const uint64_t bits = reader_u64(reader);
    double value = 0.0;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool reader_int(RayCompoundSceneReader* reader, int* output) {
    const uint32_t raw = reader_u32(reader);
    if (!reader->ok || raw > INT_MAX)
        return false;
    *output = (int)raw;
    return true;
}

static void reader_string(RayCompoundSceneReader* reader, char* output,
                          size_t capacity) {
    const uint32_t size = reader_u32(reader);
    if (!reader->ok || !output || !capacity || size >= capacity ||
        reader->offset > reader->size || size > reader->size - reader->offset) {
        reader->ok = false;
        return;
    }
    reader_get(reader, output, size);
    output[size] = '\0';
}

static RayCompoundSceneVec3 reader_vec3(RayCompoundSceneReader* reader) {
    RayCompoundSceneVec3 value;
    value.x = reader_double(reader);
    value.y = reader_double(reader);
    value.z = reader_double(reader);
    return value;
}

static RayCompoundSceneQuat reader_quat(RayCompoundSceneReader* reader) {
    RayCompoundSceneQuat value;
    value.w = reader_double(reader);
    value.x = reader_double(reader);
    value.y = reader_double(reader);
    value.z = reader_double(reader);
    return value;
}

static RayCompoundSceneMat3 reader_mat3(RayCompoundSceneReader* reader) {
    RayCompoundSceneMat3 value;
    for (size_t row = 0; row < 3; ++row)
        for (size_t column = 0; column < 3; ++column)
            value.m[row][column] = reader_double(reader);
    return value;
}

static void decode_binding(RayCompoundSceneReader* reader,
                           RayCompoundSceneSourceBinding* binding) {
    reader_string(reader, binding->schema, sizeof(binding->schema));
    binding->fixture_digest = reader_u64(reader);
    const uint64_t body_index = reader_u64(reader);
    if (body_index > SIZE_MAX)
        reader->ok = false;
    binding->body_index = (size_t)body_index;
    if (!reader_int(reader, &binding->body_id))
        reader->ok = false;
    reader_string(reader, binding->source_asset_id,
        sizeof(binding->source_asset_id));
    reader_string(reader, binding->source_sha256,
        sizeof(binding->source_sha256));
    reader_string(reader, binding->representation_role,
        sizeof(binding->representation_role));
    binding->source_center_m = reader_vec3(reader);
    binding->principal_to_source = reader_mat3(reader);
    binding->binding_digest = reader_u64(reader);
}

static bool decode_payload(const unsigned char* payload, size_t payload_size,
                           RayCompoundSceneHandoff* output) {
    RayCompoundSceneReader reader = {
        .bytes = payload, .size = payload_size, .ok = true
    };
    RayCompoundSceneHandoff handoff;
    ray_compound_scene_handoff_init(&handoff);
    reader_string(&reader, handoff.schema, sizeof(handoff.schema));
    handoff.schema_version = reader_u32(&reader);
    reader_string(&reader, handoff.handoff_id, sizeof(handoff.handoff_id));
    reader_string(&reader, handoff.fixture_reference,
        sizeof(handoff.fixture_reference));
    handoff.fixture_digest = reader_u64(&reader);
    handoff.seed = reader_u64(&reader);
    handoff.fixed_dt_s = reader_double(&reader);
    for (size_t body = 0; body < RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT;
         ++body)
        decode_binding(&reader, &handoff.bindings[body]);
    const uint32_t frame_count = reader_u32(&reader);
    if (!reader.ok || frame_count < 2 ||
        frame_count > RAY_COMPOUND_SCENE_HANDOFF_MAX_FRAMES)
        reader.ok = false;
    handoff.frame_count = frame_count;
    if (reader.ok) {
        handoff.frames = calloc(handoff.frame_count, sizeof(*handoff.frames));
        if (!handoff.frames)
            reader.ok = false;
    }
    for (size_t i = 0; reader.ok && i < handoff.frame_count; ++i) {
        handoff.frames[i].tick = reader_u64(&reader);
        for (size_t body = 0; body < RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT;
             ++body) {
            RayCompoundSceneBodyTransform* transform =
                &handoff.frames[i].bodies[body];
            if (!reader_int(&reader, &transform->body_id))
                reader.ok = false;
            transform->position_m = reader_vec3(&reader);
            transform->orientation = reader_quat(&reader);
        }
    }
    handoff.handoff_digest = reader_u64(&reader);
    if (!reader.ok || reader.offset != reader.size ||
        !ray_compound_scene_handoff_validate(&handoff)) {
        ray_compound_scene_handoff_free(&handoff);
        return false;
    }
    *output = handoff;
    return true;
}

static int lowercase_hex_digit(char value) {
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    return -1;
}

bool ray_compound_scene_handoff_parse(
    const char* text,
    RayCompoundSceneHandoff* output,
    RayCompoundSceneImportFailure* failure) {
    set_failure(failure, RAY_COMPOUND_SCENE_IMPORT_FAILURE_NONE);
    if (!text || !output) {
        set_failure(failure, RAY_COMPOUND_SCENE_IMPORT_FAILURE_INPUT);
        return false;
    }
    char prefix[128];
    const int prefix_size = snprintf(prefix, sizeof(prefix),
        "%s\ncodec_version=%u\npayload_hex=",
        RAY_COMPOUND_SCENE_HANDOFF_SCHEMA,
        RAY_COMPOUND_SCENE_HANDOFF_CODEC_VERSION);
    if (prefix_size < 0 || (size_t)prefix_size >= sizeof(prefix) ||
        strncmp(text, prefix, (size_t)prefix_size)) {
        set_failure(failure, RAY_COMPOUND_SCENE_IMPORT_FAILURE_ENVELOPE);
        return false;
    }
    const char* hex_payload = text + prefix_size;
    const char* digest_line = strstr(hex_payload, "\npayload_digest=");
    const size_t suffix_size = strlen("\npayload_digest=") + 17u;
    if (!digest_line || ((size_t)(digest_line - hex_payload) & 1u) ||
        strlen(digest_line) != suffix_size ||
        digest_line[suffix_size - 1u] != '\n') {
        set_failure(failure, RAY_COMPOUND_SCENE_IMPORT_FAILURE_ENVELOPE);
        return false;
    }
    const size_t payload_size = (size_t)(digest_line - hex_payload) / 2u;
    if (!payload_size ||
        payload_size > RAY_COMPOUND_SCENE_HANDOFF_PAYLOAD_CAPACITY) {
        set_failure(failure, RAY_COMPOUND_SCENE_IMPORT_FAILURE_ENVELOPE);
        return false;
    }
    unsigned char* payload = malloc(payload_size);
    if (!payload) {
        set_failure(failure, RAY_COMPOUND_SCENE_IMPORT_FAILURE_ALLOCATION);
        return false;
    }
    bool hex_ok = true;
    for (size_t i = 0; i < payload_size; ++i) {
        const int high = lowercase_hex_digit(hex_payload[i * 2u]);
        const int low = lowercase_hex_digit(hex_payload[i * 2u + 1u]);
        if (high < 0 || low < 0) {
            hex_ok = false;
            break;
        }
        payload[i] = (unsigned char)((high << 4) | low);
    }
    uint64_t wanted_digest = 0;
    const char* digest_text = digest_line + strlen("\npayload_digest=");
    for (size_t i = 0; hex_ok && i < 16u; ++i) {
        const int digit = lowercase_hex_digit(digest_text[i]);
        if (digit < 0) {
            hex_ok = false;
            break;
        }
        wanted_digest = (wanted_digest << 4) | (uint64_t)digit;
    }
    if (!hex_ok ||
        hash_bytes(UINT64_C(1469598103934665603), payload, payload_size) !=
            wanted_digest) {
        free(payload);
        set_failure(failure, RAY_COMPOUND_SCENE_IMPORT_FAILURE_ENVELOPE);
        return false;
    }
    RayCompoundSceneHandoff parsed;
    ray_compound_scene_handoff_init(&parsed);
    const bool decoded = decode_payload(payload, payload_size, &parsed);
    free(payload);
    if (!decoded) {
        set_failure(failure, RAY_COMPOUND_SCENE_IMPORT_FAILURE_PROVENANCE);
        return false;
    }
    *output = parsed;
    return true;
}

bool ray_compound_scene_handoff_read(
    const char* path,
    RayCompoundSceneHandoff* output,
    RayCompoundSceneImportFailure* failure) {
    set_failure(failure, RAY_COMPOUND_SCENE_IMPORT_FAILURE_NONE);
    if (!path || !output) {
        set_failure(failure, RAY_COMPOUND_SCENE_IMPORT_FAILURE_INPUT);
        return false;
    }
    FILE* file = fopen(path, "rb");
    if (!file) {
        set_failure(failure, RAY_COMPOUND_SCENE_IMPORT_FAILURE_IO);
        return false;
    }
    char* text = malloc(RAY_COMPOUND_SCENE_HANDOFF_TEXT_CAPACITY);
    if (!text) {
        fclose(file);
        set_failure(failure, RAY_COMPOUND_SCENE_IMPORT_FAILURE_ALLOCATION);
        return false;
    }
    const size_t size = fread(text, 1,
        RAY_COMPOUND_SCENE_HANDOFF_TEXT_CAPACITY - 1u, file);
    const bool read_ok = !ferror(file) && feof(file);
    const bool close_ok = fclose(file) == 0;
    text[size] = '\0';
    if (!read_ok || !close_ok) {
        free(text);
        set_failure(failure, RAY_COMPOUND_SCENE_IMPORT_FAILURE_IO);
        return false;
    }
    const bool parsed = ray_compound_scene_handoff_parse(text, output, failure);
    free(text);
    return parsed;
}

bool ray_compound_scene_handoff_replay_exact(
    const RayCompoundSceneHandoff* handoff,
    uint64_t tick,
    RayCompoundSceneFrame* output) {
    if (!output || !ray_compound_scene_handoff_validate(handoff) ||
        tick >= handoff->frame_count || handoff->frames[tick].tick != tick)
        return false;
    *output = handoff->frames[tick];
    return true;
}

const char* ray_compound_scene_import_failure_name(
    RayCompoundSceneImportFailure failure) {
    switch (failure) {
        case RAY_COMPOUND_SCENE_IMPORT_FAILURE_NONE:
            return "none";
        case RAY_COMPOUND_SCENE_IMPORT_FAILURE_INPUT:
            return "input";
        case RAY_COMPOUND_SCENE_IMPORT_FAILURE_IO:
            return "io";
        case RAY_COMPOUND_SCENE_IMPORT_FAILURE_ALLOCATION:
            return "allocation";
        case RAY_COMPOUND_SCENE_IMPORT_FAILURE_ENVELOPE:
            return "envelope";
        case RAY_COMPOUND_SCENE_IMPORT_FAILURE_PROVENANCE:
            return "provenance";
    }
    return "unknown";
}
