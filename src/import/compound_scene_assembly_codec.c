#include "import/compound_scene_assembly_codec.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FNV_OFFSET UINT64_C(1469598103934665603)
#define FNV_PRIME UINT64_C(1099511628211)

typedef struct TextWriter {
    char* data;
    size_t capacity;
    size_t size;
    bool ok;
} TextWriter;

static void fail(RayCompoundSceneAssemblyCodecFailure* output,
                 RayCompoundSceneAssemblyCodecFailure value) {
    if (output) *output = value;
}

static uint64_t hash_bytes(uint64_t hash, const void* data, size_t size) {
    const unsigned char* bytes = data;
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

static uint64_t hash_string(uint64_t hash, const char* value) {
    return hash_bytes(hash, value, strlen(value) + 1u);
}

static uint64_t hash_u64(uint64_t hash, uint64_t value) {
    unsigned char bytes[8];
    for (size_t i = 0; i < sizeof(bytes); ++i)
        bytes[i] = (unsigned char)(value >> (8u * i));
    return hash_bytes(hash, bytes, sizeof(bytes));
}

static uint64_t hash_double(uint64_t hash, double value) {
    uint64_t bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    return hash_u64(hash, bits);
}

static bool token(const char* value, size_t capacity) {
    if (!value || !value[0] || !memchr(value, '\0', capacity)) return false;
    for (const unsigned char* p = (const unsigned char*)value; *p; ++p)
        if (*p == '|' || *p == '\n' || *p == '\r') return false;
    return true;
}

static bool sha256(const char* value) {
    if (!value || strlen(value) != 64u) return false;
    for (size_t i = 0; i < 64u; ++i)
        if (!((value[i] >= '0' && value[i] <= '9') ||
              (value[i] >= 'a' && value[i] <= 'f'))) return false;
    return true;
}

static bool finite_vec(RayCompoundSceneVec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static double minimum_plane_distance(
    const RayCompoundSceneObjectRecord* object,
    const RayCompoundSceneStaticSurfaceRecord* surface) {
    const RayCompoundSceneVec3 corner = {
        surface->normal.x >= 0.0 ? object->bounds_min.x : object->bounds_max.x,
        surface->normal.y >= 0.0 ? object->bounds_min.y : object->bounds_max.y,
        surface->normal.z >= 0.0 ? object->bounds_min.z : object->bounds_max.z};
    return (corner.x - surface->origin_m.x) * surface->normal.x +
        (corner.y - surface->origin_m.y) * surface->normal.y +
        (corner.z - surface->origin_m.z) * surface->normal.z;
}

const char* ray_compound_scene_static_authority_name(
    RayCompoundSceneStaticAuthority authority) {
    switch (authority) {
        case RAY_COMPOUND_SCENE_STATIC_AUTHORITY_RENDERER_SET_DRESSING:
            return "renderer_set_dressing";
        case RAY_COMPOUND_SCENE_STATIC_AUTHORITY_SIMULATION_COLLISION_SURFACE:
            return "simulation_collision_surface";
        case RAY_COMPOUND_SCENE_STATIC_AUTHORITY_UNKNOWN: return "unknown";
    }
    return "unknown";
}

static RayCompoundSceneStaticAuthority authority_from_name(const char* name) {
    if (!strcmp(name, "renderer_set_dressing"))
        return RAY_COMPOUND_SCENE_STATIC_AUTHORITY_RENDERER_SET_DRESSING;
    if (!strcmp(name, "simulation_collision_surface"))
        return RAY_COMPOUND_SCENE_STATIC_AUTHORITY_SIMULATION_COLLISION_SURFACE;
    return RAY_COMPOUND_SCENE_STATIC_AUTHORITY_UNKNOWN;
}

uint64_t ray_compound_scene_assembly_archive_digest(
    const RayCompoundSceneAssemblyArchive* archive) {
    if (!archive) return 0u;
    uint64_t hash = FNV_OFFSET;
    hash = hash_string(hash, archive->schema);
    hash = hash_u64(hash, archive->schema_version);
    hash = hash_u64(hash, archive->handoff_digest);
    hash = hash_u64(hash, archive->fixture_digest);
    hash = hash_double(hash, archive->fixed_dt_s);
    for (size_t i = 0; i < 2u; ++i) {
        const RayCompoundSceneExternalAssetReference* a = &archive->assets[i];
        hash = hash_u64(hash, (uint64_t)a->body_id);
        hash = hash_string(hash, a->object_id);
        hash = hash_string(hash, a->mesh_asset_id);
        hash = hash_string(hash, a->runtime_path);
        hash = hash_string(hash, a->runtime_sha256);
        hash = hash_string(hash, a->source_asset_id);
        hash = hash_string(hash, a->source_sha256);
        hash = hash_u64(hash, a->source_binding_digest);
    }
    hash = hash_u64(hash, archive->static_count);
    for (size_t i = 0; i < archive->static_count; ++i) {
        const RayCompoundSceneStaticSurfaceRecord* s = &archive->statics[i];
        hash = hash_string(hash, s->object_id);
        hash = hash_string(hash, s->geometry_id);
        hash = hash_string(hash, s->material_id);
        hash = hash_u64(hash, (uint64_t)s->authority);
        hash = hash_double(hash, s->origin_m.x);
        hash = hash_double(hash, s->origin_m.y);
        hash = hash_double(hash, s->origin_m.z);
        hash = hash_double(hash, s->normal.x);
        hash = hash_double(hash, s->normal.y);
        hash = hash_double(hash, s->normal.z);
        hash = hash_double(hash, s->half_extent_u_m);
        hash = hash_double(hash, s->half_extent_v_m);
        hash = hash_u64(hash, s->collision_surface_digest);
        hash = hash_double(hash, s->minimum_clearance_m);
    }
    hash = hash_u64(hash, archive->frame_count);
    for (size_t i = 0; i < archive->frame_count; ++i) {
        const RayCompoundSceneAssemblyFrameRecord* frame = &archive->frames[i];
        hash = hash_u64(hash, frame->tick);
        hash = hash_u64(hash, frame->assembly_digest);
        for (size_t b = 0; b < 2u; ++b) {
            const RayCompoundSceneObjectRecord* o = &frame->simulated[b];
            hash = hash_u64(hash, (uint64_t)o->body_id);
            hash = hash_string(hash, o->object_id);
            hash = hash_string(hash, o->geometry_id);
            hash = hash_string(hash, o->source_asset_id);
            hash = hash_string(hash, o->source_sha256);
            hash = hash_u64(hash, o->source_binding_digest);
            hash = hash_u64(hash, o->source_tick);
            hash = hash_u64(hash, o->vertex_count);
            hash = hash_double(hash, o->bounds_min.x);
            hash = hash_double(hash, o->bounds_min.y);
            hash = hash_double(hash, o->bounds_min.z);
            hash = hash_double(hash, o->bounds_max.x);
            hash = hash_double(hash, o->bounds_max.y);
            hash = hash_double(hash, o->bounds_max.z);
            hash = hash_u64(hash, o->geometry_digest);
        }
    }
    return hash;
}

bool ray_compound_scene_assembly_archive_validate(
    const RayCompoundSceneAssemblyArchive* archive) {
    if (!archive || !archive->valid ||
        strcmp(archive->schema, RAY_COMPOUND_SCENE_ASSEMBLY_ARCHIVE_SCHEMA) ||
        archive->schema_version != RAY_COMPOUND_SCENE_ASSEMBLY_ARCHIVE_VERSION ||
        !archive->handoff_digest || !archive->fixture_digest ||
        !isfinite(archive->fixed_dt_s) || archive->fixed_dt_s <= 0.0 ||
        archive->static_count > RAY_COMPOUND_SCENE_ASSEMBLY_MAX_STATIC_COUNT ||
        !archive->frame_count ||
        archive->frame_count > RAY_COMPOUND_SCENE_ASSEMBLY_ARCHIVE_MAX_FRAMES)
        return false;
    for (size_t i = 0; i < 2u; ++i) {
        const RayCompoundSceneExternalAssetReference* asset = &archive->assets[i];
        if (asset->body_id <= 0 || !token(asset->object_id, sizeof(asset->object_id)) ||
            !token(asset->mesh_asset_id, sizeof(asset->mesh_asset_id)) ||
            !token(asset->runtime_path, sizeof(asset->runtime_path)) ||
            asset->runtime_path[0] == '/' || strstr(asset->runtime_path, "..") ||
            !sha256(asset->runtime_sha256) ||
            !token(asset->source_asset_id, sizeof(asset->source_asset_id)) ||
            !sha256(asset->source_sha256) || !asset->source_binding_digest ||
            (i && asset->body_id == archive->assets[0].body_id)) return false;
    }
    for (size_t i = 0; i < archive->static_count; ++i) {
        const RayCompoundSceneStaticSurfaceRecord* surface = &archive->statics[i];
        const double norm = surface->normal.x * surface->normal.x +
            surface->normal.y * surface->normal.y + surface->normal.z * surface->normal.z;
        if (!token(surface->object_id, sizeof(surface->object_id)) ||
            !token(surface->geometry_id, sizeof(surface->geometry_id)) ||
            !token(surface->material_id, sizeof(surface->material_id)) ||
            !finite_vec(surface->origin_m) || !finite_vec(surface->normal) ||
            fabs(norm - 1.0) > 1e-9 || !isfinite(surface->half_extent_u_m) ||
            !isfinite(surface->half_extent_v_m) || surface->half_extent_u_m <= 0.0 ||
            surface->half_extent_v_m <= 0.0 ||
            !isfinite(surface->minimum_clearance_m) ||
            surface->minimum_clearance_m < -1e-9 ||
            (surface->authority == RAY_COMPOUND_SCENE_STATIC_AUTHORITY_RENDERER_SET_DRESSING &&
             surface->collision_surface_digest) ||
            (surface->authority == RAY_COMPOUND_SCENE_STATIC_AUTHORITY_SIMULATION_COLLISION_SURFACE &&
             !surface->collision_surface_digest) ||
            surface->authority == RAY_COMPOUND_SCENE_STATIC_AUTHORITY_UNKNOWN) return false;
        for (size_t j = 0; j < i; ++j)
            if (!strcmp(surface->object_id, archive->statics[j].object_id)) return false;
    }
    for (size_t f = 0; f < archive->frame_count; ++f) {
        const RayCompoundSceneAssemblyFrameRecord* frame = &archive->frames[f];
        if (!frame->assembly_digest || (f && frame->tick <= archive->frames[f - 1u].tick))
            return false;
        for (size_t b = 0; b < 2u; ++b) {
            const RayCompoundSceneObjectRecord* object = &frame->simulated[b];
            const RayCompoundSceneExternalAssetReference* asset = &archive->assets[b];
            if (object->membership != RAY_COMPOUND_SCENE_MEMBERSHIP_SIMULATED ||
                object->body_id != asset->body_id || strcmp(object->object_id, asset->object_id) ||
                strcmp(object->geometry_id, asset->mesh_asset_id) ||
                strcmp(object->source_asset_id, asset->source_asset_id) ||
                strcmp(object->source_sha256, asset->source_sha256) ||
                object->source_binding_digest != asset->source_binding_digest ||
                object->source_tick != frame->tick || !object->vertex_count ||
                !object->geometry_digest || !finite_vec(object->bounds_min) ||
                !finite_vec(object->bounds_max)) return false;
            for (size_t s = 0; s < archive->static_count; ++s)
                if (minimum_plane_distance(object, &archive->statics[s]) < -1e-9)
                    return false;
        }
    }
    for (size_t s = 0; s < archive->static_count; ++s) {
        double measured = INFINITY;
        for (size_t f = 0; f < archive->frame_count; ++f)
            for (size_t b = 0; b < 2u; ++b)
                measured = fmin(measured, minimum_plane_distance(
                    &archive->frames[f].simulated[b], &archive->statics[s]));
        if (fabs(measured - archive->statics[s].minimum_clearance_m) > 1e-12)
            return false;
    }
    return archive->archive_digest == ray_compound_scene_assembly_archive_digest(archive);
}

bool ray_compound_scene_assembly_archive_build(
    const RayCompoundSceneHandoff* handoff,
    const RayCompoundSceneExternalAssetReference assets[2],
    const RayCompoundSceneStaticSurfaceRecord* statics, size_t static_count,
    const RayCompoundSceneAssembly* assemblies, size_t frame_count,
    RayCompoundSceneAssemblyArchive* output,
    RayCompoundSceneAssemblyCodecFailure* failure) {
    fail(failure, RAY_COMPOUND_SCENE_ASSEMBLY_CODEC_FAILURE_NONE);
    if (!handoff || !assets || !assemblies || !output ||
        (static_count && !statics)) {
        fail(failure, RAY_COMPOUND_SCENE_ASSEMBLY_CODEC_FAILURE_INPUT); return false;
    }
    if (static_count > RAY_COMPOUND_SCENE_ASSEMBLY_MAX_STATIC_COUNT || !frame_count ||
        frame_count > RAY_COMPOUND_SCENE_ASSEMBLY_ARCHIVE_MAX_FRAMES) {
        fail(failure, RAY_COMPOUND_SCENE_ASSEMBLY_CODEC_FAILURE_CAPACITY); return false;
    }
    if (!ray_compound_scene_handoff_validate(handoff)) {
        fail(failure, RAY_COMPOUND_SCENE_ASSEMBLY_CODEC_FAILURE_PROVENANCE); return false;
    }
    RayCompoundSceneAssemblyArchive candidate = {0};
    candidate.valid = true;
    snprintf(candidate.schema, sizeof(candidate.schema), "%s",
             RAY_COMPOUND_SCENE_ASSEMBLY_ARCHIVE_SCHEMA);
    candidate.schema_version = RAY_COMPOUND_SCENE_ASSEMBLY_ARCHIVE_VERSION;
    candidate.handoff_digest = handoff->handoff_digest;
    candidate.fixture_digest = handoff->fixture_digest;
    candidate.fixed_dt_s = handoff->fixed_dt_s;
    memcpy(candidate.assets, assets, sizeof(candidate.assets));
    candidate.static_count = static_count;
    if (static_count) memcpy(candidate.statics, statics, static_count * sizeof(*statics));
    candidate.frame_count = frame_count;
    for (size_t f = 0; f < frame_count; ++f) {
        const RayCompoundSceneAssembly* assembly = &assemblies[f];
        if (!ray_compound_scene_assembly_validate(assembly) ||
            assembly->handoff_digest != handoff->handoff_digest ||
            assembly->static_count != static_count) {
            fail(failure, RAY_COMPOUND_SCENE_ASSEMBLY_CODEC_FAILURE_PROVENANCE); return false;
        }
        candidate.frames[f].tick = assembly->tick;
        candidate.frames[f].assembly_digest = assembly->assembly_digest;
        memcpy(candidate.frames[f].simulated, assembly->objects,
               sizeof(candidate.frames[f].simulated));
        for (size_t s = 0; s < static_count; ++s)
            if (strcmp(candidate.statics[s].object_id,
                       assembly->objects[2u + s].object_id)) {
                fail(failure, RAY_COMPOUND_SCENE_ASSEMBLY_CODEC_FAILURE_PROVENANCE); return false;
            }
    }
    for (size_t s = 0; s < static_count; ++s) {
        double minimum = INFINITY;
        for (size_t f = 0; f < frame_count; ++f)
            for (size_t b = 0; b < 2u; ++b)
                minimum = fmin(minimum, minimum_plane_distance(
                    &candidate.frames[f].simulated[b], &candidate.statics[s]));
        candidate.statics[s].minimum_clearance_m = minimum;
    }
    candidate.archive_digest = ray_compound_scene_assembly_archive_digest(&candidate);
    if (!ray_compound_scene_assembly_archive_validate(&candidate)) {
        fail(failure, RAY_COMPOUND_SCENE_ASSEMBLY_CODEC_FAILURE_PROVENANCE); return false;
    }
    *output = candidate;
    return true;
}

static void put(TextWriter* writer, const char* format, ...) {
    if (!writer->ok) return;
    va_list args;
    va_start(args, format);
    const int count = vsnprintf(writer->data + writer->size,
        writer->capacity - writer->size, format, args);
    va_end(args);
    if (count < 0 || (size_t)count >= writer->capacity - writer->size) {
        writer->ok = false; return;
    }
    writer->size += (size_t)count;
}

bool ray_compound_scene_assembly_archive_format(
    const RayCompoundSceneAssemblyArchive* archive, char* output,
    size_t output_size) {
    if (!output || !output_size || !ray_compound_scene_assembly_archive_validate(archive))
        return false;
    TextWriter w = {output, output_size, 0u, true};
    put(&w, "%s\nversion=%u\nhandoff=%016llx\nfixture=%016llx\ndt=%a\n",
        archive->schema, archive->schema_version,
        (unsigned long long)archive->handoff_digest,
        (unsigned long long)archive->fixture_digest, archive->fixed_dt_s);
    for (size_t i = 0; i < 2u; ++i) {
        const RayCompoundSceneExternalAssetReference* a = &archive->assets[i];
        put(&w, "asset=%d|%s|%s|%s|%s|%s|%s|%016llx\n", a->body_id,
            a->object_id, a->mesh_asset_id, a->runtime_path, a->runtime_sha256,
            a->source_asset_id, a->source_sha256,
            (unsigned long long)a->source_binding_digest);
    }
    put(&w, "statics=%zu\n", archive->static_count);
    for (size_t i = 0; i < archive->static_count; ++i) {
        const RayCompoundSceneStaticSurfaceRecord* s = &archive->statics[i];
        put(&w, "static=%s|%s|%s|%s|%a|%a|%a|%a|%a|%a|%a|%a|%016llx|%a\n",
            s->object_id, s->geometry_id, s->material_id,
            ray_compound_scene_static_authority_name(s->authority),
            s->origin_m.x, s->origin_m.y, s->origin_m.z,
            s->normal.x, s->normal.y, s->normal.z,
            s->half_extent_u_m, s->half_extent_v_m,
            (unsigned long long)s->collision_surface_digest,
            s->minimum_clearance_m);
    }
    put(&w, "frames=%zu\n", archive->frame_count);
    for (size_t f = 0; f < archive->frame_count; ++f) {
        const RayCompoundSceneAssemblyFrameRecord* frame = &archive->frames[f];
        put(&w, "frame=%llu|%016llx\n", (unsigned long long)frame->tick,
            (unsigned long long)frame->assembly_digest);
        for (size_t b = 0; b < 2u; ++b) {
            const RayCompoundSceneObjectRecord* o = &frame->simulated[b];
            put(&w, "body=%d|%s|%s|%s|%s|%016llx|%llu|%zu|%a|%a|%a|%a|%a|%a|%016llx\n",
                o->body_id, o->object_id, o->geometry_id, o->source_asset_id,
                o->source_sha256, (unsigned long long)o->source_binding_digest,
                (unsigned long long)o->source_tick, o->vertex_count,
                o->bounds_min.x, o->bounds_min.y, o->bounds_min.z,
                o->bounds_max.x, o->bounds_max.y, o->bounds_max.z,
                (unsigned long long)o->geometry_digest);
        }
    }
    put(&w, "archive_digest=%016llx\n", (unsigned long long)archive->archive_digest);
    return w.ok;
}

static char* next_line(char** cursor) {
    if (!cursor || !*cursor || !**cursor) return NULL;
    char* line = *cursor;
    char* end = strchr(line, '\n');
    if (!end) return NULL;
    *end = '\0';
    *cursor = end + 1;
    return line;
}

bool ray_compound_scene_assembly_archive_parse(
    const char* text, RayCompoundSceneAssemblyArchive* output,
    RayCompoundSceneAssemblyCodecFailure* failure) {
    fail(failure, RAY_COMPOUND_SCENE_ASSEMBLY_CODEC_FAILURE_NONE);
    if (!text || !output) { fail(failure, RAY_COMPOUND_SCENE_ASSEMBLY_CODEC_FAILURE_INPUT); return false; }
    const size_t size = strlen(text);
    if (!size || size >= RAY_COMPOUND_SCENE_ASSEMBLY_ARCHIVE_TEXT_CAPACITY) {
        fail(failure, RAY_COMPOUND_SCENE_ASSEMBLY_CODEC_FAILURE_CAPACITY); return false;
    }
    char* copy = malloc(size + 1u);
    char* canonical = malloc(RAY_COMPOUND_SCENE_ASSEMBLY_ARCHIVE_TEXT_CAPACITY);
    if (!copy || !canonical) { free(copy); free(canonical); fail(failure, RAY_COMPOUND_SCENE_ASSEMBLY_CODEC_FAILURE_CAPACITY); return false; }
    memcpy(copy, text, size + 1u);
    char* cursor = copy;
    char* line = next_line(&cursor);
    RayCompoundSceneAssemblyArchive a = {0};
    unsigned version = 0;
    unsigned long long u64 = 0;
    bool ok = line && !strcmp(line, RAY_COMPOUND_SCENE_ASSEMBLY_ARCHIVE_SCHEMA);
    snprintf(a.schema, sizeof(a.schema), "%s", RAY_COMPOUND_SCENE_ASSEMBLY_ARCHIVE_SCHEMA);
    line = ok ? next_line(&cursor) : NULL; ok = line && sscanf(line, "version=%u", &version) == 1; a.schema_version = version;
    line = ok ? next_line(&cursor) : NULL; ok = line && sscanf(line, "handoff=%llx", &u64) == 1; a.handoff_digest = (uint64_t)u64;
    line = ok ? next_line(&cursor) : NULL; ok = line && sscanf(line, "fixture=%llx", &u64) == 1; a.fixture_digest = (uint64_t)u64;
    line = ok ? next_line(&cursor) : NULL; ok = line && sscanf(line, "dt=%la", &a.fixed_dt_s) == 1;
    for (size_t i = 0; ok && i < 2u; ++i) {
        RayCompoundSceneExternalAssetReference* x = &a.assets[i];
        line = next_line(&cursor);
        ok = line && sscanf(line, "asset=%d|%63[^|]|%63[^|]|%191[^|]|%64[^|]|%63[^|]|%64[^|]|%llx",
            &x->body_id, x->object_id, x->mesh_asset_id, x->runtime_path,
            x->runtime_sha256, x->source_asset_id, x->source_sha256, &u64) == 8;
        x->source_binding_digest = (uint64_t)u64;
    }
    line = ok ? next_line(&cursor) : NULL; ok = line && sscanf(line, "statics=%zu", &a.static_count) == 1 && a.static_count <= RAY_COMPOUND_SCENE_ASSEMBLY_MAX_STATIC_COUNT;
    for (size_t i = 0; ok && i < a.static_count; ++i) {
        RayCompoundSceneStaticSurfaceRecord* s = &a.statics[i]; char authority[48] = {0};
        line = next_line(&cursor);
        ok = line && sscanf(line, "static=%63[^|]|%63[^|]|%63[^|]|%47[^|]|%la|%la|%la|%la|%la|%la|%la|%la|%llx|%la",
            s->object_id, s->geometry_id, s->material_id, authority,
            &s->origin_m.x, &s->origin_m.y, &s->origin_m.z,
            &s->normal.x, &s->normal.y, &s->normal.z,
            &s->half_extent_u_m, &s->half_extent_v_m, &u64,
            &s->minimum_clearance_m) == 14;
        s->authority = authority_from_name(authority); s->collision_surface_digest = (uint64_t)u64;
    }
    line = ok ? next_line(&cursor) : NULL; ok = line && sscanf(line, "frames=%zu", &a.frame_count) == 1 && a.frame_count <= RAY_COMPOUND_SCENE_ASSEMBLY_ARCHIVE_MAX_FRAMES;
    for (size_t f = 0; ok && f < a.frame_count; ++f) {
        RayCompoundSceneAssemblyFrameRecord* frame = &a.frames[f];
        unsigned long long tick_value = 0, assembly_value = 0;
        line = next_line(&cursor);
        ok = line && sscanf(line, "frame=%llu|%llx", &tick_value,
            &assembly_value) == 2;
        frame->tick = (uint64_t)tick_value;
        frame->assembly_digest = (uint64_t)assembly_value;
        for (size_t b = 0; ok && b < 2u; ++b) {
            RayCompoundSceneObjectRecord* o = &frame->simulated[b]; unsigned long long binding = 0, tick = 0, geom = 0;
            line = next_line(&cursor);
            ok = line && sscanf(line, "body=%d|%63[^|]|%63[^|]|%63[^|]|%64[^|]|%llx|%llu|%zu|%la|%la|%la|%la|%la|%la|%llx",
                &o->body_id, o->object_id, o->geometry_id, o->source_asset_id,
                o->source_sha256, &binding, &tick, &o->vertex_count,
                &o->bounds_min.x, &o->bounds_min.y, &o->bounds_min.z,
                &o->bounds_max.x, &o->bounds_max.y, &o->bounds_max.z, &geom) == 15;
            o->membership = RAY_COMPOUND_SCENE_MEMBERSHIP_SIMULATED;
            o->source_binding_digest = (uint64_t)binding; o->source_tick = (uint64_t)tick; o->geometry_digest = (uint64_t)geom;
        }
    }
    line = ok ? next_line(&cursor) : NULL; ok = line && sscanf(line, "archive_digest=%llx", &u64) == 1 && !*cursor; a.archive_digest = (uint64_t)u64; a.valid = true;
    ok = ok && ray_compound_scene_assembly_archive_validate(&a) &&
        ray_compound_scene_assembly_archive_format(&a, canonical,
            RAY_COMPOUND_SCENE_ASSEMBLY_ARCHIVE_TEXT_CAPACITY) && !strcmp(text, canonical);
    free(copy); free(canonical);
    if (!ok) { fail(failure, RAY_COMPOUND_SCENE_ASSEMBLY_CODEC_FAILURE_CODEC); return false; }
    *output = a; return true;
}

bool ray_compound_scene_assembly_archive_write(
    const RayCompoundSceneAssemblyArchive* archive, const char* path) {
    if (!path) return false;
    char* text = malloc(RAY_COMPOUND_SCENE_ASSEMBLY_ARCHIVE_TEXT_CAPACITY);
    if (!text || !ray_compound_scene_assembly_archive_format(archive, text,
            RAY_COMPOUND_SCENE_ASSEMBLY_ARCHIVE_TEXT_CAPACITY)) { free(text); return false; }
    FILE* file = fopen(path, "wb");
    const size_t size = strlen(text);
    bool ok = false;
    if (file) {
        ok = fwrite(text, 1, size, file) == size;
        if (fclose(file) != 0) ok = false;
    }
    free(text); return ok;
}

bool ray_compound_scene_assembly_archive_read(
    const char* path, RayCompoundSceneAssemblyArchive* output,
    RayCompoundSceneAssemblyCodecFailure* failure) {
    if (!path || !output) { fail(failure, RAY_COMPOUND_SCENE_ASSEMBLY_CODEC_FAILURE_INPUT); return false; }
    FILE* file = fopen(path, "rb");
    if (!file) { fail(failure, RAY_COMPOUND_SCENE_ASSEMBLY_CODEC_FAILURE_IO); return false; }
    char* text = malloc(RAY_COMPOUND_SCENE_ASSEMBLY_ARCHIVE_TEXT_CAPACITY);
    if (!text) { fclose(file); fail(failure, RAY_COMPOUND_SCENE_ASSEMBLY_CODEC_FAILURE_CAPACITY); return false; }
    const size_t size = fread(text, 1, RAY_COMPOUND_SCENE_ASSEMBLY_ARCHIVE_TEXT_CAPACITY - 1u, file);
    const bool read_ok = !ferror(file) && feof(file);
    const bool close_ok = fclose(file) == 0;
    const bool io_ok = read_ok && close_ok;
    text[size] = '\0';
    const bool ok = io_ok && ray_compound_scene_assembly_archive_parse(text, output, failure);
    free(text); if (!io_ok) fail(failure, RAY_COMPOUND_SCENE_ASSEMBLY_CODEC_FAILURE_IO); return ok;
}

bool ray_compound_scene_assembly_archive_replay_exact(
    const RayCompoundSceneAssemblyArchive* archive, uint64_t tick,
    RayCompoundSceneAssemblyFrameRecord* output) {
    if (!output || !ray_compound_scene_assembly_archive_validate(archive)) return false;
    for (size_t i = 0; i < archive->frame_count; ++i)
        if (archive->frames[i].tick == tick) { *output = archive->frames[i]; return true; }
    return false;
}

double ray_compound_scene_assembly_clearance_floor_z(
    const RayCompoundSceneAssembly* assemblies, size_t frame_count,
    double clearance_m) {
    if (!assemblies || !frame_count || !isfinite(clearance_m) || clearance_m < 0.0)
        return NAN;
    double minimum = INFINITY;
    for (size_t f = 0; f < frame_count; ++f) {
        if (!ray_compound_scene_assembly_validate(&assemblies[f])) return NAN;
        for (size_t b = 0; b < 2u; ++b)
            minimum = fmin(minimum, assemblies[f].objects[b].bounds_min.z);
    }
    return minimum - clearance_m;
}
