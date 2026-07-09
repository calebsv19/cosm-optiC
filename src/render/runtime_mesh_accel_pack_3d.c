#include "render/runtime_mesh_accel_pack_3d.h"

#include "render/runtime_triangle_bvh_3d.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const unsigned char kRuntimeMeshAccelPack3DMagic[8] = {
    'R', 'T', 'M', 'B', 'V', 'H', '3', '\0'
};
static const uint32_t kRuntimeMeshAccelPack3DVersion = 1u;
static const uint32_t kRuntimeMeshAccelPack3DEndianMarker = 0x01020304u;
static const uint64_t kRuntimeMeshAccelPack3DFnvOffset = 1469598103934665603ull;
static const uint64_t kRuntimeMeshAccelPack3DFnvPrime = 1099511628211ull;

static void runtime_mesh_accel_pack_3d_diag(char* out_diagnostics,
                                            size_t out_diagnostics_size,
                                            const char* message) {
    if (!out_diagnostics || out_diagnostics_size == 0u || !message) return;
    snprintf(out_diagnostics, out_diagnostics_size, "%s", message);
}

static bool runtime_mesh_accel_pack_3d_write_exact(FILE* file,
                                                   const void* data,
                                                   size_t size) {
    return file && data && fwrite(data, 1u, size, file) == size;
}

static bool runtime_mesh_accel_pack_3d_read_exact(FILE* file, void* data, size_t size) {
    return file && data && fread(data, 1u, size, file) == size;
}

static bool runtime_mesh_accel_pack_3d_write_u32(FILE* file, uint32_t value) {
    return runtime_mesh_accel_pack_3d_write_exact(file, &value, sizeof(value));
}

static bool runtime_mesh_accel_pack_3d_read_u32(FILE* file, uint32_t* out_value) {
    return runtime_mesh_accel_pack_3d_read_exact(file, out_value, sizeof(*out_value));
}

static bool runtime_mesh_accel_pack_3d_write_u64(FILE* file, uint64_t value) {
    return runtime_mesh_accel_pack_3d_write_exact(file, &value, sizeof(value));
}

static bool runtime_mesh_accel_pack_3d_read_u64(FILE* file, uint64_t* out_value) {
    return runtime_mesh_accel_pack_3d_read_exact(file, out_value, sizeof(*out_value));
}

static bool runtime_mesh_accel_pack_3d_write_i64(FILE* file, int64_t value) {
    return runtime_mesh_accel_pack_3d_write_exact(file, &value, sizeof(value));
}

static bool runtime_mesh_accel_pack_3d_read_i64(FILE* file, int64_t* out_value) {
    return runtime_mesh_accel_pack_3d_read_exact(file, out_value, sizeof(*out_value));
}

static bool runtime_mesh_accel_pack_3d_write_double(FILE* file, double value) {
    return runtime_mesh_accel_pack_3d_write_exact(file, &value, sizeof(value));
}

static bool runtime_mesh_accel_pack_3d_read_double(FILE* file, double* out_value) {
    return runtime_mesh_accel_pack_3d_read_exact(file, out_value, sizeof(*out_value));
}

static bool runtime_mesh_accel_pack_3d_write_path(FILE* file, const char* text) {
    char buffer[4096] = {0};
    if (text) snprintf(buffer, sizeof(buffer), "%s", text);
    return runtime_mesh_accel_pack_3d_write_exact(file, buffer, sizeof(buffer));
}

static bool runtime_mesh_accel_pack_3d_read_path(FILE* file, char text[4096]) {
    if (!runtime_mesh_accel_pack_3d_read_exact(file, text, 4096u)) return false;
    text[4095] = '\0';
    return true;
}

static bool runtime_mesh_accel_pack_3d_write_vec3(FILE* file, Vec3 value) {
    return runtime_mesh_accel_pack_3d_write_double(file, value.x) &&
           runtime_mesh_accel_pack_3d_write_double(file, value.y) &&
           runtime_mesh_accel_pack_3d_write_double(file, value.z);
}

static bool runtime_mesh_accel_pack_3d_read_vec3(FILE* file, Vec3* out_value) {
    return out_value &&
           runtime_mesh_accel_pack_3d_read_double(file, &out_value->x) &&
           runtime_mesh_accel_pack_3d_read_double(file, &out_value->y) &&
           runtime_mesh_accel_pack_3d_read_double(file, &out_value->z);
}

static uint64_t runtime_mesh_accel_pack_3d_hash_path(const char* source_path) {
    uint64_t hash = kRuntimeMeshAccelPack3DFnvOffset;
    const unsigned char* p = (const unsigned char*)source_path;
    while (p && *p) {
        hash ^= (uint64_t)(*p++);
        hash *= kRuntimeMeshAccelPack3DFnvPrime;
    }
    return hash;
}

static bool runtime_mesh_accel_pack_3d_write_key(FILE* file,
                                                 const RuntimeMeshAccelPack3DKey* key) {
    const RayTracingRuntimeMeshAssetPackSourceKey* source = key ? &key->source_key : NULL;
    return source &&
           runtime_mesh_accel_pack_3d_write_path(file, source->source_path) &&
           runtime_mesh_accel_pack_3d_write_i64(file, source->source_mtime_sec) &&
           runtime_mesh_accel_pack_3d_write_i64(file, source->source_mtime_nsec) &&
           runtime_mesh_accel_pack_3d_write_i64(file, source->source_size_bytes) &&
           runtime_mesh_accel_pack_3d_write_u64(file, source->source_checksum) &&
           runtime_mesh_accel_pack_3d_write_u32(file, source->core_mesh_asset_schema_version) &&
           runtime_mesh_accel_pack_3d_write_u32(file, source->ray_tracing_cache_schema_version) &&
           runtime_mesh_accel_pack_3d_write_u32(file, source->pointer_size_bytes) &&
           runtime_mesh_accel_pack_3d_write_u32(file, key->accel_cache_schema_version) &&
           runtime_mesh_accel_pack_3d_write_u32(file, key->blas_builder_version) &&
           runtime_mesh_accel_pack_3d_write_u32(file, key->triangle_layout_version) &&
           runtime_mesh_accel_pack_3d_write_u32(file, key->bvh_layout_version) &&
           runtime_mesh_accel_pack_3d_write_u32(file, key->bvh_builder_policy_version) &&
           runtime_mesh_accel_pack_3d_write_u32(file, key->acceleration_policy) &&
           runtime_mesh_accel_pack_3d_write_u32(file, key->pointer_size_bytes) &&
           runtime_mesh_accel_pack_3d_write_u32(file, key->source_triangle_count);
}

static bool runtime_mesh_accel_pack_3d_read_key(FILE* file,
                                                RuntimeMeshAccelPack3DKey* out_key) {
    RayTracingRuntimeMeshAssetPackSourceKey* source = out_key ? &out_key->source_key : NULL;
    if (!out_key || !source) return false;
    memset(out_key, 0, sizeof(*out_key));
    return runtime_mesh_accel_pack_3d_read_path(file, source->source_path) &&
           runtime_mesh_accel_pack_3d_read_i64(file, &source->source_mtime_sec) &&
           runtime_mesh_accel_pack_3d_read_i64(file, &source->source_mtime_nsec) &&
           runtime_mesh_accel_pack_3d_read_i64(file, &source->source_size_bytes) &&
           runtime_mesh_accel_pack_3d_read_u64(file, &source->source_checksum) &&
           runtime_mesh_accel_pack_3d_read_u32(file, &source->core_mesh_asset_schema_version) &&
           runtime_mesh_accel_pack_3d_read_u32(file, &source->ray_tracing_cache_schema_version) &&
           runtime_mesh_accel_pack_3d_read_u32(file, &source->pointer_size_bytes) &&
           runtime_mesh_accel_pack_3d_read_u32(file, &out_key->accel_cache_schema_version) &&
           runtime_mesh_accel_pack_3d_read_u32(file, &out_key->blas_builder_version) &&
           runtime_mesh_accel_pack_3d_read_u32(file, &out_key->triangle_layout_version) &&
           runtime_mesh_accel_pack_3d_read_u32(file, &out_key->bvh_layout_version) &&
           runtime_mesh_accel_pack_3d_read_u32(file, &out_key->bvh_builder_policy_version) &&
           runtime_mesh_accel_pack_3d_read_u32(file, &out_key->acceleration_policy) &&
           runtime_mesh_accel_pack_3d_read_u32(file, &out_key->pointer_size_bytes) &&
           runtime_mesh_accel_pack_3d_read_u32(file, &out_key->source_triangle_count);
}

static bool runtime_mesh_accel_pack_3d_key_matches(
    const RuntimeMeshAccelPack3DKey* actual,
    const RuntimeMeshAccelPack3DKey* expected) {
    if (!actual || !expected) return false;
    return strcmp(actual->source_key.source_path, expected->source_key.source_path) == 0 &&
           actual->source_key.source_mtime_sec == expected->source_key.source_mtime_sec &&
           actual->source_key.source_mtime_nsec == expected->source_key.source_mtime_nsec &&
           actual->source_key.source_size_bytes == expected->source_key.source_size_bytes &&
           actual->source_key.source_checksum == expected->source_key.source_checksum &&
           actual->source_key.core_mesh_asset_schema_version ==
               expected->source_key.core_mesh_asset_schema_version &&
           actual->source_key.ray_tracing_cache_schema_version ==
               expected->source_key.ray_tracing_cache_schema_version &&
           actual->source_key.pointer_size_bytes == expected->source_key.pointer_size_bytes &&
           actual->accel_cache_schema_version == expected->accel_cache_schema_version &&
           actual->blas_builder_version == expected->blas_builder_version &&
           actual->triangle_layout_version == expected->triangle_layout_version &&
           actual->bvh_layout_version == expected->bvh_layout_version &&
           actual->bvh_builder_policy_version == expected->bvh_builder_policy_version &&
           actual->acceleration_policy == expected->acceleration_policy &&
           actual->pointer_size_bytes == expected->pointer_size_bytes &&
           actual->source_triangle_count == expected->source_triangle_count;
}

static bool runtime_mesh_accel_pack_3d_write_triangle(FILE* file,
                                                      const RuntimeTriangle3D* triangle) {
    uint32_t two_sided = triangle && triangle->twoSided ? 1u : 0u;
    return triangle &&
           runtime_mesh_accel_pack_3d_write_vec3(file, triangle->p0) &&
           runtime_mesh_accel_pack_3d_write_vec3(file, triangle->p1) &&
           runtime_mesh_accel_pack_3d_write_vec3(file, triangle->p2) &&
           runtime_mesh_accel_pack_3d_write_vec3(file, triangle->normal) &&
           runtime_mesh_accel_pack_3d_write_u32(file, two_sided) &&
           runtime_mesh_accel_pack_3d_write_exact(file,
                                                  &triangle->localTriangleIndex,
                                                  sizeof(triangle->localTriangleIndex));
}

static bool runtime_mesh_accel_pack_3d_read_triangle(FILE* file,
                                                     RuntimeTriangle3D* triangle,
                                                     int source_triangle_count) {
    uint32_t two_sided = 0u;
    if (!triangle) return false;
    memset(triangle, 0, sizeof(*triangle));
    if (!runtime_mesh_accel_pack_3d_read_vec3(file, &triangle->p0) ||
        !runtime_mesh_accel_pack_3d_read_vec3(file, &triangle->p1) ||
        !runtime_mesh_accel_pack_3d_read_vec3(file, &triangle->p2) ||
        !runtime_mesh_accel_pack_3d_read_vec3(file, &triangle->normal) ||
        !runtime_mesh_accel_pack_3d_read_u32(file, &two_sided) ||
        !runtime_mesh_accel_pack_3d_read_exact(file,
                                               &triangle->localTriangleIndex,
                                               sizeof(triangle->localTriangleIndex))) {
        return false;
    }
    if (two_sided > 1u ||
        triangle->localTriangleIndex < 0 ||
        triangle->localTriangleIndex >= source_triangle_count) {
        return false;
    }
    triangle->twoSided = two_sided != 0u;
    triangle->primitiveIndex = -1;
    triangle->sceneObjectIndex = -1;
    return true;
}

bool RuntimeMeshAccelPack3D_CachePathForSource(const char* cache_root,
                                               const char* source_path,
                                               char* out_path,
                                               size_t out_path_size) {
    uint64_t hash = 0u;
    if (out_path && out_path_size > 0u) out_path[0] = '\0';
    if (!cache_root || !cache_root[0] || !source_path || !source_path[0] ||
        !out_path || out_path_size == 0u) {
        return false;
    }
    hash = runtime_mesh_accel_pack_3d_hash_path(source_path);
    if (snprintf(out_path,
                 out_path_size,
                 "%s/%016llx.rtmaccel",
                 cache_root,
                 (unsigned long long)hash) >= (int)out_path_size) {
        out_path[0] = '\0';
        return false;
    }
    return true;
}

bool RuntimeMeshAccelPack3D_WriteCacheFile(const char* path,
                                           const RuntimeMeshAccelPack3DKey* key,
                                           const RuntimeTriangleMesh3D* mesh,
                                           char* out_diagnostics,
                                           size_t out_diagnostics_size) {
    char temp_path[4096] = {0};
    FILE* file = NULL;
    uint32_t triangle_count = 0u;
    uint32_t source_triangle_count = 0u;
    if (!path || !path[0] || !key || !mesh || mesh->triangleCount < 0 ||
        !RuntimeTriangleMesh3D_HasReadyBVH(mesh)) {
        runtime_mesh_accel_pack_3d_diag(out_diagnostics,
                                        out_diagnostics_size,
                                        "mesh accel cache write input invalid");
        return false;
    }
    if (snprintf(temp_path, sizeof(temp_path), "%s.tmp.%ld", path, (long)getpid()) >=
        (int)sizeof(temp_path)) {
        runtime_mesh_accel_pack_3d_diag(out_diagnostics,
                                        out_diagnostics_size,
                                        "mesh accel cache temp path too long");
        return false;
    }
    file = fopen(temp_path, "wb");
    if (!file) {
        runtime_mesh_accel_pack_3d_diag(out_diagnostics,
                                        out_diagnostics_size,
                                        "mesh accel cache temp open failed");
        return false;
    }
    triangle_count = (uint32_t)mesh->triangleCount;
    source_triangle_count = key->source_triangle_count;
    if (!runtime_mesh_accel_pack_3d_write_exact(file,
                                                kRuntimeMeshAccelPack3DMagic,
                                                sizeof(kRuntimeMeshAccelPack3DMagic)) ||
        !runtime_mesh_accel_pack_3d_write_u32(file, kRuntimeMeshAccelPack3DVersion) ||
        !runtime_mesh_accel_pack_3d_write_u32(file, kRuntimeMeshAccelPack3DEndianMarker) ||
        !runtime_mesh_accel_pack_3d_write_key(file, key) ||
        !runtime_mesh_accel_pack_3d_write_u32(file, triangle_count) ||
        !runtime_mesh_accel_pack_3d_write_u32(file, source_triangle_count)) {
        fclose(file);
        unlink(temp_path);
        runtime_mesh_accel_pack_3d_diag(out_diagnostics,
                                        out_diagnostics_size,
                                        "mesh accel cache header write failed");
        return false;
    }
    for (int i = 0; i < mesh->triangleCount; ++i) {
        if (!runtime_mesh_accel_pack_3d_write_triangle(file, &mesh->triangles[i])) {
            fclose(file);
            unlink(temp_path);
            runtime_mesh_accel_pack_3d_diag(out_diagnostics,
                                            out_diagnostics_size,
                                            "mesh accel cache triangle write failed");
            return false;
        }
    }
    if (!RuntimeTriangleMesh3D_WriteBVHCachePayload(file, mesh)) {
        fclose(file);
        unlink(temp_path);
        runtime_mesh_accel_pack_3d_diag(out_diagnostics,
                                        out_diagnostics_size,
                                        "mesh accel cache bvh write failed");
        return false;
    }
    if (fclose(file) != 0) {
        unlink(temp_path);
        runtime_mesh_accel_pack_3d_diag(out_diagnostics,
                                        out_diagnostics_size,
                                        "mesh accel cache close failed");
        return false;
    }
    if (rename(temp_path, path) != 0) {
        unlink(temp_path);
        runtime_mesh_accel_pack_3d_diag(out_diagnostics,
                                        out_diagnostics_size,
                                        "mesh accel cache rename failed");
        return false;
    }
    runtime_mesh_accel_pack_3d_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

bool RuntimeMeshAccelPack3D_ReadCacheFile(const char* path,
                                          const RuntimeMeshAccelPack3DKey* expected_key,
                                          RuntimeTriangleMesh3D* out_mesh,
                                          char* out_diagnostics,
                                          size_t out_diagnostics_size) {
    FILE* file = NULL;
    unsigned char magic[8] = {0};
    uint32_t version = 0u;
    uint32_t endian = 0u;
    uint32_t triangle_count = 0u;
    uint32_t source_triangle_count = 0u;
    RuntimeMeshAccelPack3DKey actual_key;
    if (!path || !expected_key || !out_mesh) {
        runtime_mesh_accel_pack_3d_diag(out_diagnostics,
                                        out_diagnostics_size,
                                        "mesh accel cache read input invalid");
        return false;
    }
    RuntimeTriangleMesh3D_Init(out_mesh);
    file = fopen(path, "rb");
    if (!file) {
        runtime_mesh_accel_pack_3d_diag(out_diagnostics,
                                        out_diagnostics_size,
                                        "mesh accel cache open failed");
        return false;
    }
    if (!runtime_mesh_accel_pack_3d_read_exact(file, magic, sizeof(magic)) ||
        memcmp(magic, kRuntimeMeshAccelPack3DMagic, sizeof(magic)) != 0 ||
        !runtime_mesh_accel_pack_3d_read_u32(file, &version) ||
        !runtime_mesh_accel_pack_3d_read_u32(file, &endian) ||
        version != kRuntimeMeshAccelPack3DVersion ||
        endian != kRuntimeMeshAccelPack3DEndianMarker ||
        !runtime_mesh_accel_pack_3d_read_key(file, &actual_key) ||
        !runtime_mesh_accel_pack_3d_key_matches(&actual_key, expected_key) ||
        !runtime_mesh_accel_pack_3d_read_u32(file, &triangle_count) ||
        !runtime_mesh_accel_pack_3d_read_u32(file, &source_triangle_count) ||
        triangle_count == 0u ||
        source_triangle_count == 0u) {
        fclose(file);
        runtime_mesh_accel_pack_3d_diag(out_diagnostics,
                                        out_diagnostics_size,
                                        "mesh accel cache header invalid");
        return false;
    }
    out_mesh->triangles = (RuntimeTriangle3D*)calloc(triangle_count,
                                                     sizeof(*out_mesh->triangles));
    if (!out_mesh->triangles) {
        fclose(file);
        runtime_mesh_accel_pack_3d_diag(out_diagnostics,
                                        out_diagnostics_size,
                                        "mesh accel cache triangle allocation failed");
        return false;
    }
    out_mesh->triangleCount = (int)triangle_count;
    out_mesh->triangleCapacity = (int)triangle_count;
    out_mesh->bvhDirty = true;
    for (uint32_t i = 0u; i < triangle_count; ++i) {
        if (!runtime_mesh_accel_pack_3d_read_triangle(file,
                                                      &out_mesh->triangles[i],
                                                      (int)source_triangle_count)) {
            fclose(file);
            RuntimeTriangleMesh3D_Free(out_mesh);
            runtime_mesh_accel_pack_3d_diag(out_diagnostics,
                                            out_diagnostics_size,
                                            "mesh accel cache triangle invalid");
            return false;
        }
    }
    if (!RuntimeTriangleMesh3D_ReadBVHCachePayload(file,
                                                   out_mesh,
                                                   out_mesh->triangleCount,
                                                   out_diagnostics,
                                                   out_diagnostics_size)) {
        fclose(file);
        RuntimeTriangleMesh3D_Free(out_mesh);
        return false;
    }
    fclose(file);
    runtime_mesh_accel_pack_3d_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}
