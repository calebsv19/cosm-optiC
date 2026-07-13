#include "import/runtime_mesh_asset_pack.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const unsigned char kRuntimeMeshAssetPackMagic[8] = {
    'R', 'T', 'M', 'P', 'K', '1', '3', '\0'
};
static const uint32_t kRuntimeMeshAssetPackLegacyVersion = 1u;
static const uint32_t kRuntimeMeshAssetPackLegacyCacheVersion = 2u;
static const uint32_t kRuntimeMeshAssetPackVersion = 3u;
static const uint32_t kRuntimeMeshAssetPackCacheVersion = 4u;
static const uint32_t kRuntimeMeshAssetPackCacheSchemaVersion = 2u;
static const uint32_t kRuntimeMeshAssetPackEndianMarker = 0x01020304u;
static const uint64_t kRuntimeMeshAssetPackFnvOffset = 1469598103934665603ull;
static const uint64_t kRuntimeMeshAssetPackFnvPrime = 1099511628211ull;

static void runtime_mesh_asset_pack_diag(char* out_diagnostics,
                                         size_t out_diagnostics_size,
                                         const char* message) {
    if (!out_diagnostics || out_diagnostics_size == 0u || !message) return;
    snprintf(out_diagnostics, out_diagnostics_size, "%s", message);
}

static bool runtime_mesh_asset_pack_write_exact(FILE* f, const void* data, size_t size) {
    return f && data && fwrite(data, 1u, size, f) == size;
}

static bool runtime_mesh_asset_pack_read_exact(FILE* f, void* data, size_t size) {
    return f && data && fread(data, 1u, size, f) == size;
}

static bool runtime_mesh_asset_pack_write_u32(FILE* f, uint32_t value) {
    return runtime_mesh_asset_pack_write_exact(f, &value, sizeof(value));
}

static bool runtime_mesh_asset_pack_read_u32(FILE* f, uint32_t* out_value) {
    return runtime_mesh_asset_pack_read_exact(f, out_value, sizeof(*out_value));
}

static bool runtime_mesh_asset_pack_write_u64(FILE* f, uint64_t value) {
    return runtime_mesh_asset_pack_write_exact(f, &value, sizeof(value));
}

static bool runtime_mesh_asset_pack_read_u64(FILE* f, uint64_t* out_value) {
    return runtime_mesh_asset_pack_read_exact(f, out_value, sizeof(*out_value));
}

static bool runtime_mesh_asset_pack_write_i64(FILE* f, int64_t value) {
    return runtime_mesh_asset_pack_write_exact(f, &value, sizeof(value));
}

static bool runtime_mesh_asset_pack_read_i64(FILE* f, int64_t* out_value) {
    return runtime_mesh_asset_pack_read_exact(f, out_value, sizeof(*out_value));
}

static bool runtime_mesh_asset_pack_write_double(FILE* f, double value) {
    return runtime_mesh_asset_pack_write_exact(f, &value, sizeof(value));
}

static bool runtime_mesh_asset_pack_read_double(FILE* f, double* out_value) {
    return runtime_mesh_asset_pack_read_exact(f, out_value, sizeof(*out_value));
}

static bool runtime_mesh_asset_pack_write_string64(FILE* f, const char text[64]) {
    char buffer[64] = {0};
    if (text) {
        snprintf(buffer, sizeof(buffer), "%s", text);
    }
    return runtime_mesh_asset_pack_write_exact(f, buffer, sizeof(buffer));
}

static bool runtime_mesh_asset_pack_read_string64(FILE* f, char text[64]) {
    if (!runtime_mesh_asset_pack_read_exact(f, text, 64u)) return false;
    text[63] = '\0';
    return true;
}

static bool runtime_mesh_asset_pack_write_path(FILE* f, const char* text) {
    char buffer[4096] = {0};
    if (text) {
        snprintf(buffer, sizeof(buffer), "%s", text);
    }
    return runtime_mesh_asset_pack_write_exact(f, buffer, sizeof(buffer));
}

static bool runtime_mesh_asset_pack_read_path(FILE* f, char text[4096]) {
    if (!runtime_mesh_asset_pack_read_exact(f, text, 4096u)) return false;
    text[4095] = '\0';
    return true;
}

static bool runtime_mesh_asset_pack_write_vec3(FILE* f, CoreObjectVec3 vec) {
    return runtime_mesh_asset_pack_write_double(f, vec.x) &&
           runtime_mesh_asset_pack_write_double(f, vec.y) &&
           runtime_mesh_asset_pack_write_double(f, vec.z);
}

static bool runtime_mesh_asset_pack_read_vec3(FILE* f, CoreObjectVec3* out_vec) {
    return out_vec &&
           runtime_mesh_asset_pack_read_double(f, &out_vec->x) &&
           runtime_mesh_asset_pack_read_double(f, &out_vec->y) &&
           runtime_mesh_asset_pack_read_double(f, &out_vec->z);
}

static bool runtime_mesh_asset_pack_count_to_u32(size_t count,
                                                 uint32_t* out_count,
                                                 char* out_diagnostics,
                                                 size_t out_diagnostics_size) {
    if (!out_count) return false;
    if (count > UINT32_MAX) {
        runtime_mesh_asset_pack_diag(out_diagnostics,
                                     out_diagnostics_size,
                                     "mesh asset pack count exceeds v1 limit");
        return false;
    }
    *out_count = (uint32_t)count;
    return true;
}

static int runtime_mesh_asset_pack_find_surface_group(
    const CoreMeshAssetRuntimeDocument* document,
    const char* group_id) {
    if (!document || !group_id || !group_id[0]) return -1;
    for (size_t i = 0u; i < document->surface_group_count; ++i) {
        if (strcmp(document->surface_groups[i].group_id, group_id) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static uint64_t runtime_mesh_asset_pack_hash_bytes(uint64_t hash,
                                                   const void* data,
                                                   size_t size) {
    const unsigned char* bytes = (const unsigned char*)data;
    if (!bytes) return hash;
    for (size_t i = 0u; i < size; ++i) {
        hash ^= (uint64_t)bytes[i];
        hash *= kRuntimeMeshAssetPackFnvPrime;
    }
    return hash;
}

static bool runtime_mesh_asset_pack_write_document_file(
    const char* path,
    const RayTracingRuntimeMeshAssetPackSourceKey* source_key,
    const CoreMeshAssetRuntimeDocument* document,
    char* out_diagnostics,
    size_t out_diagnostics_size,
    bool atomic_write) {
    FILE* f = NULL;
    char temp_path[4096] = {0};
    const char* write_path = path;
    uint32_t format_version = source_key ? kRuntimeMeshAssetPackCacheVersion
                                         : kRuntimeMeshAssetPackVersion;
    uint32_t vertex_count = 0u;
    uint32_t normal_count = 0u;
    uint32_t triangle_count = 0u;
    uint32_t surface_group_count = 0u;
    CoreResult validate_result = core_result_ok();

    runtime_mesh_asset_pack_diag(out_diagnostics, out_diagnostics_size, "ok");
    if (!path || !path[0] || !document) {
        runtime_mesh_asset_pack_diag(out_diagnostics, out_diagnostics_size, "mesh asset pack input missing");
        return false;
    }
    if (!core_is_little_endian()) {
        runtime_mesh_asset_pack_diag(out_diagnostics,
                                     out_diagnostics_size,
                                     "mesh asset pack v1 requires little-endian host");
        return false;
    }
    if (source_key) {
        if (!source_key->source_path[0] ||
            source_key->core_mesh_asset_schema_version != CORE_MESH_ASSET_SCHEMA_VERSION_1 ||
            source_key->ray_tracing_cache_schema_version !=
                kRuntimeMeshAssetPackCacheSchemaVersion ||
            source_key->pointer_size_bytes != (uint32_t)sizeof(void*)) {
            runtime_mesh_asset_pack_diag(out_diagnostics,
                                         out_diagnostics_size,
                                         "mesh asset cache source key invalid");
            return false;
        }
    }
    validate_result = core_mesh_asset_runtime_document_validate(document);
    if (validate_result.code != CORE_OK) {
        runtime_mesh_asset_pack_diag(out_diagnostics,
                                     out_diagnostics_size,
                                     validate_result.message ? validate_result.message : "mesh asset invalid");
        return false;
    }
    if (!runtime_mesh_asset_pack_count_to_u32(document->vertex_count,
                                             &vertex_count,
                                             out_diagnostics,
                                             out_diagnostics_size) ||
        !runtime_mesh_asset_pack_count_to_u32(document->vertex_normal_count,
                                             &normal_count,
                                             out_diagnostics,
                                             out_diagnostics_size) ||
        !runtime_mesh_asset_pack_count_to_u32(document->triangle_count,
                                             &triangle_count,
                                             out_diagnostics,
                                             out_diagnostics_size) ||
        !runtime_mesh_asset_pack_count_to_u32(document->surface_group_count,
                                             &surface_group_count,
                                             out_diagnostics,
                                             out_diagnostics_size)) {
        return false;
    }

    if (atomic_write) {
        if (snprintf(temp_path,
                     sizeof(temp_path),
                     "%s.tmp.%ld",
                     path,
                     (long)getpid()) >= (int)sizeof(temp_path)) {
            runtime_mesh_asset_pack_diag(out_diagnostics,
                                         out_diagnostics_size,
                                         "mesh asset pack temp path too long");
            return false;
        }
        write_path = temp_path;
    }

    f = fopen(write_path, "wb");
    if (!f) {
        runtime_mesh_asset_pack_diag(out_diagnostics, out_diagnostics_size, "mesh asset pack open failed");
        return false;
    }

    if (!runtime_mesh_asset_pack_write_exact(f,
                                             kRuntimeMeshAssetPackMagic,
                                             sizeof(kRuntimeMeshAssetPackMagic)) ||
        !runtime_mesh_asset_pack_write_u32(f, format_version) ||
        !runtime_mesh_asset_pack_write_u32(f, kRuntimeMeshAssetPackEndianMarker)) {
        fclose(f);
        if (atomic_write) unlink(write_path);
        runtime_mesh_asset_pack_diag(out_diagnostics, out_diagnostics_size, "mesh asset pack write failed");
        return false;
    }
    if (source_key &&
        (!runtime_mesh_asset_pack_write_u32(f, source_key->core_mesh_asset_schema_version) ||
         !runtime_mesh_asset_pack_write_u32(f, source_key->ray_tracing_cache_schema_version) ||
         !runtime_mesh_asset_pack_write_u32(f, source_key->pointer_size_bytes) ||
         !runtime_mesh_asset_pack_write_i64(f, source_key->source_mtime_sec) ||
         !runtime_mesh_asset_pack_write_i64(f, source_key->source_mtime_nsec) ||
         !runtime_mesh_asset_pack_write_i64(f, source_key->source_size_bytes) ||
         !runtime_mesh_asset_pack_write_u64(f, source_key->source_checksum) ||
         !runtime_mesh_asset_pack_write_path(f, source_key->source_path))) {
        fclose(f);
        if (atomic_write) unlink(write_path);
        runtime_mesh_asset_pack_diag(out_diagnostics, out_diagnostics_size, "mesh asset cache key write failed");
        return false;
    }
    if (!runtime_mesh_asset_pack_write_u32(f, vertex_count) ||
        !runtime_mesh_asset_pack_write_u32(f, normal_count) ||
        !runtime_mesh_asset_pack_write_u32(f, (uint32_t)document->normal_provenance) ||
        !runtime_mesh_asset_pack_write_u32(f, triangle_count) ||
        !runtime_mesh_asset_pack_write_u32(f, surface_group_count) ||
        !runtime_mesh_asset_pack_write_string64(f, document->contract.asset_id) ||
        !runtime_mesh_asset_pack_write_string64(f, document->contract.source_asset_id) ||
        !runtime_mesh_asset_pack_write_u32(f, (uint32_t)document->contract.asset_type) ||
        !runtime_mesh_asset_pack_write_u32(f, document->contract.topology_closed_volume ? 1u : 0u) ||
        !runtime_mesh_asset_pack_write_u32(f,
                                           document->contract.topology_manifold_expected ? 1u : 0u) ||
        !runtime_mesh_asset_pack_write_vec3(f, document->contract.local_bounds.min) ||
        !runtime_mesh_asset_pack_write_vec3(f, document->contract.local_bounds.max) ||
        !runtime_mesh_asset_pack_write_vec3(f, document->contract.pivot.origin) ||
        !runtime_mesh_asset_pack_write_vec3(f, document->contract.pivot.axis_u) ||
        !runtime_mesh_asset_pack_write_vec3(f, document->contract.pivot.axis_v) ||
        !runtime_mesh_asset_pack_write_vec3(f, document->contract.pivot.normal)) {
        fclose(f);
        if (atomic_write) unlink(write_path);
        runtime_mesh_asset_pack_diag(out_diagnostics, out_diagnostics_size, "mesh asset pack write failed");
        return false;
    }

    for (size_t i = 0u; i < document->surface_group_count; ++i) {
        const CoreMeshAssetSurfaceGroup* group = &document->surface_groups[i];
        uint32_t triangle_start = 0u;
        uint32_t group_triangle_count = 0u;
        if (!runtime_mesh_asset_pack_count_to_u32(group->triangle_start,
                                                 &triangle_start,
                                                 out_diagnostics,
                                                 out_diagnostics_size) ||
            !runtime_mesh_asset_pack_count_to_u32(group->triangle_count,
                                                 &group_triangle_count,
                                                 out_diagnostics,
                                                 out_diagnostics_size) ||
            !runtime_mesh_asset_pack_write_string64(f, group->group_id) ||
            !runtime_mesh_asset_pack_write_u32(f, triangle_start) ||
            !runtime_mesh_asset_pack_write_u32(f, group_triangle_count)) {
            fclose(f);
            if (atomic_write) unlink(write_path);
            runtime_mesh_asset_pack_diag(out_diagnostics, out_diagnostics_size, "mesh asset pack group write failed");
            return false;
        }
    }

    for (size_t i = 0u; i < document->vertex_count; ++i) {
        if (!runtime_mesh_asset_pack_write_vec3(f, document->vertices[i].position)) {
            fclose(f);
            if (atomic_write) unlink(write_path);
            runtime_mesh_asset_pack_diag(out_diagnostics, out_diagnostics_size, "mesh asset pack vertex write failed");
            return false;
        }
    }

    for (size_t i = 0u; i < document->vertex_normal_count; ++i) {
        if (!runtime_mesh_asset_pack_write_vec3(f, document->vertices[i].normal)) {
            fclose(f);
            if (atomic_write) unlink(write_path);
            runtime_mesh_asset_pack_diag(out_diagnostics,
                                         out_diagnostics_size,
                                         "mesh asset pack normal write failed");
            return false;
        }
    }

    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle* triangle = &document->triangles[i];
        uint32_t a = 0u;
        uint32_t b = 0u;
        uint32_t c = 0u;
        int group_index = runtime_mesh_asset_pack_find_surface_group(document,
                                                                     triangle->surface_group_id);
        if (group_index < 0 ||
            !runtime_mesh_asset_pack_count_to_u32(triangle->a,
                                                 &a,
                                                 out_diagnostics,
                                                 out_diagnostics_size) ||
            !runtime_mesh_asset_pack_count_to_u32(triangle->b,
                                                 &b,
                                                 out_diagnostics,
                                                 out_diagnostics_size) ||
            !runtime_mesh_asset_pack_count_to_u32(triangle->c,
                                                 &c,
                                                 out_diagnostics,
                                                 out_diagnostics_size) ||
            !runtime_mesh_asset_pack_write_u32(f, a) ||
            !runtime_mesh_asset_pack_write_u32(f, b) ||
            !runtime_mesh_asset_pack_write_u32(f, c) ||
            !runtime_mesh_asset_pack_write_u32(f, (uint32_t)group_index)) {
            fclose(f);
            if (atomic_write) unlink(write_path);
            runtime_mesh_asset_pack_diag(out_diagnostics,
                                         out_diagnostics_size,
                                         "mesh asset pack triangle write failed");
            return false;
        }
    }

    if (fclose(f) != 0) {
        if (atomic_write) unlink(write_path);
        runtime_mesh_asset_pack_diag(out_diagnostics, out_diagnostics_size, "mesh asset pack close failed");
        return false;
    }
    if (atomic_write && rename(write_path, path) != 0) {
        unlink(write_path);
        runtime_mesh_asset_pack_diag(out_diagnostics,
                                     out_diagnostics_size,
                                     "mesh asset pack rename failed");
        return false;
    }
    return true;
}

static bool runtime_mesh_asset_pack_source_key_matches(
    const RayTracingRuntimeMeshAssetPackSourceKey* actual,
    const RayTracingRuntimeMeshAssetPackSourceKey* expected) {
    if (!actual || !expected) return false;
    return strcmp(actual->source_path, expected->source_path) == 0 &&
           actual->source_mtime_sec == expected->source_mtime_sec &&
           actual->source_mtime_nsec == expected->source_mtime_nsec &&
           actual->source_size_bytes == expected->source_size_bytes &&
           (expected->source_checksum == 0u ||
            actual->source_checksum == expected->source_checksum) &&
           actual->core_mesh_asset_schema_version == expected->core_mesh_asset_schema_version &&
           actual->ray_tracing_cache_schema_version == expected->ray_tracing_cache_schema_version &&
           actual->pointer_size_bytes == expected->pointer_size_bytes;
}

static bool runtime_mesh_asset_pack_read_document_file(
    const char* path,
    const RayTracingRuntimeMeshAssetPackSourceKey* expected_source_key,
    CoreMeshAssetRuntimeDocument* out_document,
    char* out_diagnostics,
    size_t out_diagnostics_size,
    bool require_cache_key) {
    FILE* f = NULL;
    unsigned char magic[8] = {0};
    uint32_t version = 0u;
    uint32_t endian_marker = 0u;
    uint32_t vertex_count = 0u;
    uint32_t normal_count = 0u;
    uint32_t normal_provenance = 0u;
    uint32_t triangle_count = 0u;
    uint32_t surface_group_count = 0u;
    uint32_t asset_type = 0u;
    uint32_t topology_closed_volume = 0u;
    uint32_t topology_manifold_expected = 0u;
    RayTracingRuntimeMeshAssetPackSourceKey actual_source_key;
    CoreResult result = core_result_ok();
    CoreMeshAssetRuntimeDocument document;

    runtime_mesh_asset_pack_diag(out_diagnostics, out_diagnostics_size, "ok");
    if (!path || !path[0] || !out_document) {
        runtime_mesh_asset_pack_diag(out_diagnostics, out_diagnostics_size, "mesh asset pack input missing");
        return false;
    }
    if (!core_is_little_endian()) {
        runtime_mesh_asset_pack_diag(out_diagnostics,
                                     out_diagnostics_size,
                                     "mesh asset pack v1 requires little-endian host");
        return false;
    }

    f = fopen(path, "rb");
    if (!f) {
        runtime_mesh_asset_pack_diag(out_diagnostics, out_diagnostics_size, "mesh asset pack open failed");
        return false;
    }
    if (!runtime_mesh_asset_pack_read_exact(f, magic, sizeof(magic)) ||
        memcmp(magic, kRuntimeMeshAssetPackMagic, sizeof(magic)) != 0 ||
        !runtime_mesh_asset_pack_read_u32(f, &version) ||
        !runtime_mesh_asset_pack_read_u32(f, &endian_marker) ||
        (version != kRuntimeMeshAssetPackLegacyVersion &&
         version != kRuntimeMeshAssetPackLegacyCacheVersion &&
         version != kRuntimeMeshAssetPackVersion &&
         version != kRuntimeMeshAssetPackCacheVersion) ||
        endian_marker != kRuntimeMeshAssetPackEndianMarker ||
        (require_cache_key && version != kRuntimeMeshAssetPackLegacyCacheVersion &&
         version != kRuntimeMeshAssetPackCacheVersion)) {
        fclose(f);
        runtime_mesh_asset_pack_diag(out_diagnostics, out_diagnostics_size, "mesh asset pack header invalid");
        return false;
    }
    memset(&actual_source_key, 0, sizeof(actual_source_key));
    if (version == kRuntimeMeshAssetPackLegacyCacheVersion ||
        version == kRuntimeMeshAssetPackCacheVersion) {
        if (!runtime_mesh_asset_pack_read_u32(f, &actual_source_key.core_mesh_asset_schema_version) ||
            !runtime_mesh_asset_pack_read_u32(f, &actual_source_key.ray_tracing_cache_schema_version) ||
            !runtime_mesh_asset_pack_read_u32(f, &actual_source_key.pointer_size_bytes) ||
            !runtime_mesh_asset_pack_read_i64(f, &actual_source_key.source_mtime_sec) ||
            !runtime_mesh_asset_pack_read_i64(f, &actual_source_key.source_mtime_nsec) ||
            !runtime_mesh_asset_pack_read_i64(f, &actual_source_key.source_size_bytes) ||
            !runtime_mesh_asset_pack_read_u64(f, &actual_source_key.source_checksum) ||
            !runtime_mesh_asset_pack_read_path(f, actual_source_key.source_path)) {
            fclose(f);
            runtime_mesh_asset_pack_diag(out_diagnostics, out_diagnostics_size, "mesh asset cache key invalid");
            return false;
        }
        if (expected_source_key &&
            !runtime_mesh_asset_pack_source_key_matches(&actual_source_key, expected_source_key)) {
            fclose(f);
            runtime_mesh_asset_pack_diag(out_diagnostics, out_diagnostics_size, "mesh asset cache stale");
            return false;
        }
    } else if (require_cache_key) {
        fclose(f);
        runtime_mesh_asset_pack_diag(out_diagnostics, out_diagnostics_size, "mesh asset cache key missing");
        return false;
    }
    if (!runtime_mesh_asset_pack_read_u32(f, &vertex_count) ||
        ((version == kRuntimeMeshAssetPackVersion ||
          version == kRuntimeMeshAssetPackCacheVersion) &&
         (!runtime_mesh_asset_pack_read_u32(f, &normal_count) ||
          !runtime_mesh_asset_pack_read_u32(f, &normal_provenance))) ||
        !runtime_mesh_asset_pack_read_u32(f, &triangle_count) ||
        !runtime_mesh_asset_pack_read_u32(f, &surface_group_count)) {
        fclose(f);
        runtime_mesh_asset_pack_diag(out_diagnostics, out_diagnostics_size, "mesh asset pack header invalid");
        return false;
    }

    core_mesh_asset_runtime_document_init(&document);
    if (!runtime_mesh_asset_pack_read_string64(f, document.contract.asset_id) ||
        !runtime_mesh_asset_pack_read_string64(f, document.contract.source_asset_id) ||
        !runtime_mesh_asset_pack_read_u32(f, &asset_type) ||
        !runtime_mesh_asset_pack_read_u32(f, &topology_closed_volume) ||
        !runtime_mesh_asset_pack_read_u32(f, &topology_manifold_expected) ||
        !runtime_mesh_asset_pack_read_vec3(f, &document.contract.local_bounds.min) ||
        !runtime_mesh_asset_pack_read_vec3(f, &document.contract.local_bounds.max) ||
        !runtime_mesh_asset_pack_read_vec3(f, &document.contract.pivot.origin) ||
        !runtime_mesh_asset_pack_read_vec3(f, &document.contract.pivot.axis_u) ||
        !runtime_mesh_asset_pack_read_vec3(f, &document.contract.pivot.axis_v) ||
        !runtime_mesh_asset_pack_read_vec3(f, &document.contract.pivot.normal)) {
        fclose(f);
        core_mesh_asset_runtime_document_free(&document);
        runtime_mesh_asset_pack_diag(out_diagnostics, out_diagnostics_size, "mesh asset pack contract invalid");
        return false;
    }
    document.contract.asset_type = (CoreMeshAssetType)asset_type;
    document.contract.topology_closed_volume = topology_closed_volume != 0u;
    document.contract.topology_manifold_expected = topology_manifold_expected != 0u;

    result = core_mesh_asset_runtime_document_set_surface_group_count(&document,
                                                                      (size_t)surface_group_count);
    if (result.code != CORE_OK) goto fail;
    for (size_t i = 0u; i < document.surface_group_count; ++i) {
        uint32_t triangle_start = 0u;
        uint32_t group_triangle_count = 0u;
        if (!runtime_mesh_asset_pack_read_string64(f, document.surface_groups[i].group_id) ||
            !runtime_mesh_asset_pack_read_u32(f, &triangle_start) ||
            !runtime_mesh_asset_pack_read_u32(f, &group_triangle_count)) {
            fclose(f);
            core_mesh_asset_runtime_document_free(&document);
            runtime_mesh_asset_pack_diag(out_diagnostics, out_diagnostics_size, "mesh asset pack group invalid");
            return false;
        }
        document.surface_groups[i].triangle_start = (size_t)triangle_start;
        document.surface_groups[i].triangle_count = (size_t)group_triangle_count;
    }

    result = core_mesh_asset_runtime_document_set_vertex_count(&document, (size_t)vertex_count);
    if (result.code != CORE_OK) goto fail;
    for (size_t i = 0u; i < document.vertex_count; ++i) {
        if (!runtime_mesh_asset_pack_read_vec3(f, &document.vertices[i].position)) {
            fclose(f);
            core_mesh_asset_runtime_document_free(&document);
            runtime_mesh_asset_pack_diag(out_diagnostics, out_diagnostics_size, "mesh asset pack vertex invalid");
            return false;
        }
    }
    if (normal_count > 0u) {
        if (normal_count != vertex_count) {
            result = (CoreResult){CORE_ERR_INVALID_ARG,
                                  "mesh asset pack normal count invalid"};
            goto fail;
        }
        document.vertex_normal_count = (size_t)normal_count;
        document.normal_provenance =
            (CoreMeshAssetRuntimeNormalProvenance)normal_provenance;
        for (size_t i = 0u; i < document.vertex_normal_count; ++i) {
            if (!runtime_mesh_asset_pack_read_vec3(f, &document.vertices[i].normal)) {
                fclose(f);
                core_mesh_asset_runtime_document_free(&document);
                runtime_mesh_asset_pack_diag(out_diagnostics,
                                             out_diagnostics_size,
                                             "mesh asset pack normal invalid");
                return false;
            }
        }
    }

    result = core_mesh_asset_runtime_document_set_triangle_count(&document, (size_t)triangle_count);
    if (result.code != CORE_OK) goto fail;
    for (size_t i = 0u; i < document.triangle_count; ++i) {
        uint32_t a = 0u;
        uint32_t b = 0u;
        uint32_t c = 0u;
        uint32_t group_index = 0u;
        if (!runtime_mesh_asset_pack_read_u32(f, &a) ||
            !runtime_mesh_asset_pack_read_u32(f, &b) ||
            !runtime_mesh_asset_pack_read_u32(f, &c) ||
            !runtime_mesh_asset_pack_read_u32(f, &group_index) ||
            group_index >= surface_group_count) {
            fclose(f);
            core_mesh_asset_runtime_document_free(&document);
            runtime_mesh_asset_pack_diag(out_diagnostics, out_diagnostics_size, "mesh asset pack triangle invalid");
            return false;
        }
        document.triangles[i].a = (size_t)a;
        document.triangles[i].b = (size_t)b;
        document.triangles[i].c = (size_t)c;
        snprintf(document.triangles[i].surface_group_id,
                 sizeof(document.triangles[i].surface_group_id),
                 "%s",
                 document.surface_groups[group_index].group_id);
    }

    if (fclose(f) != 0) {
        core_mesh_asset_runtime_document_free(&document);
        runtime_mesh_asset_pack_diag(out_diagnostics, out_diagnostics_size, "mesh asset pack close failed");
        return false;
    }
    result = core_mesh_asset_runtime_document_validate(&document);
    if (result.code != CORE_OK) goto fail_closed;
    *out_document = document;
    return true;

fail:
    fclose(f);
fail_closed:
    runtime_mesh_asset_pack_diag(out_diagnostics,
                                 out_diagnostics_size,
                                 result.message ? result.message : "mesh asset pack read failed");
    core_mesh_asset_runtime_document_free(&document);
    return false;
}

bool ray_tracing_runtime_mesh_asset_pack_write_file(
    const char* path,
    const CoreMeshAssetRuntimeDocument* document,
    char* out_diagnostics,
    size_t out_diagnostics_size) {
    return runtime_mesh_asset_pack_write_document_file(path,
                                                       NULL,
                                                       document,
                                                       out_diagnostics,
                                                       out_diagnostics_size,
                                                       false);
}

bool ray_tracing_runtime_mesh_asset_pack_read_file(
    const char* path,
    CoreMeshAssetRuntimeDocument* out_document,
    char* out_diagnostics,
    size_t out_diagnostics_size) {
    return runtime_mesh_asset_pack_read_document_file(path,
                                                      NULL,
                                                      out_document,
                                                      out_diagnostics,
                                                      out_diagnostics_size,
                                                      false);
}

bool ray_tracing_runtime_mesh_asset_pack_cache_path_for_source(
    const char* cache_root,
    const char* source_path,
    char* out_path,
    size_t out_path_size) {
    uint64_t hash = kRuntimeMeshAssetPackFnvOffset;
    if (out_path && out_path_size > 0u) out_path[0] = '\0';
    if (!cache_root || !cache_root[0] || !source_path || !source_path[0] ||
        !out_path || out_path_size == 0u) {
        return false;
    }
    hash = runtime_mesh_asset_pack_hash_bytes(hash, source_path, strlen(source_path));
    return snprintf(out_path,
                    out_path_size,
                    "%s/%016llx.rtmeshpack",
                    cache_root,
                    (unsigned long long)hash) > 0;
}

bool ray_tracing_runtime_mesh_asset_pack_checksum_file(const char* path,
                                                       uint64_t* out_checksum) {
    FILE* f = NULL;
    uint64_t hash = kRuntimeMeshAssetPackFnvOffset;
    unsigned char buffer[65536];
    size_t nread = 0u;
    if (out_checksum) *out_checksum = 0u;
    if (!path || !path[0] || !out_checksum) return false;
    f = fopen(path, "rb");
    if (!f) return false;
    while ((nread = fread(buffer, 1u, sizeof(buffer), f)) > 0u) {
        hash = runtime_mesh_asset_pack_hash_bytes(hash, buffer, nread);
    }
    if (ferror(f)) {
        fclose(f);
        return false;
    }
    fclose(f);
    *out_checksum = hash;
    return true;
}

bool ray_tracing_runtime_mesh_asset_pack_write_cache_file(
    const char* path,
    const RayTracingRuntimeMeshAssetPackSourceKey* source_key,
    const CoreMeshAssetRuntimeDocument* document,
    char* out_diagnostics,
    size_t out_diagnostics_size) {
    return runtime_mesh_asset_pack_write_document_file(path,
                                                       source_key,
                                                       document,
                                                       out_diagnostics,
                                                       out_diagnostics_size,
                                                       true);
}

bool ray_tracing_runtime_mesh_asset_pack_read_cache_file(
    const char* path,
    const RayTracingRuntimeMeshAssetPackSourceKey* expected_source_key,
    CoreMeshAssetRuntimeDocument* out_document,
    char* out_diagnostics,
    size_t out_diagnostics_size) {
    return runtime_mesh_asset_pack_read_document_file(path,
                                                      expected_source_key,
                                                      out_document,
                                                      out_diagnostics,
                                                      out_diagnostics_size,
                                                      true);
}
