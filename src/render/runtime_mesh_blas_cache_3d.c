#include "render/runtime_mesh_blas_cache_3d.h"

#include "math/vec3.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_triangle_bvh_3d.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct RuntimeMeshBLASCache3DEntry {
    bool valid;
    char asset_id[64];
    char path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX];
    long long mtime_sec;
    long long mtime_nsec;
    long long file_size;
    size_t vertex_count;
    size_t source_triangle_count;
    RuntimeTriangleMesh3D local_mesh;
} RuntimeMeshBLASCache3DEntry;

static RuntimeMeshBLASCache3DEntry
    gRuntimeMeshBLASCache3D[RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS];
static RuntimeSceneAcceleration3DDiagnostics gRuntimeMeshBLASCache3DDiagnostics;
static char gRuntimeMeshBLASCache3DLastDiagnostics[1024] = "ok";

static void runtime_mesh_blas_cache_3d_set_diag(const char* message) {
    snprintf(gRuntimeMeshBLASCache3DLastDiagnostics,
             sizeof(gRuntimeMeshBLASCache3DLastDiagnostics),
             "%s",
             (message && message[0]) ? message : "ok");
}

static void runtime_mesh_blas_cache_3d_stamp_path(const char* path,
                                                  long long* out_mtime_sec,
                                                  long long* out_mtime_nsec,
                                                  long long* out_file_size) {
    struct stat st;
    if (out_mtime_sec) *out_mtime_sec = 0;
    if (out_mtime_nsec) *out_mtime_nsec = 0;
    if (out_file_size) *out_file_size = 0;
    if (!path || !path[0] || stat(path, &st) != 0) return;
    if (out_mtime_sec) *out_mtime_sec = (long long)st.st_mtime;
#if defined(__APPLE__)
    if (out_mtime_nsec) *out_mtime_nsec = (long long)st.st_mtimespec.tv_nsec;
#elif defined(st_mtim)
    if (out_mtime_nsec) *out_mtime_nsec = (long long)st.st_mtim.tv_nsec;
#else
    if (out_mtime_nsec) *out_mtime_nsec = 0;
#endif
    if (out_file_size) *out_file_size = (long long)st.st_size;
}

static bool runtime_mesh_blas_cache_3d_entry_matches(
    const RuntimeMeshBLASCache3DEntry* entry,
    const RayTracingRuntimeMeshAsset* asset,
    long long mtime_sec,
    long long mtime_nsec,
    long long file_size) {
    return entry && entry->valid && asset &&
           strcmp(entry->asset_id, asset->asset_id) == 0 &&
           strcmp(entry->path, asset->path) == 0 &&
           entry->mtime_sec == mtime_sec &&
           entry->mtime_nsec == mtime_nsec &&
           entry->file_size == file_size &&
           entry->vertex_count == asset->document.vertex_count &&
           entry->source_triangle_count == asset->document.triangle_count;
}

static int runtime_mesh_blas_cache_3d_find(
    const RayTracingRuntimeMeshAsset* asset,
    long long mtime_sec,
    long long mtime_nsec,
    long long file_size) {
    for (int i = 0; i < RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS; ++i) {
        if (runtime_mesh_blas_cache_3d_entry_matches(&gRuntimeMeshBLASCache3D[i],
                                                     asset,
                                                     mtime_sec,
                                                     mtime_nsec,
                                                     file_size)) {
            return i;
        }
    }
    return -1;
}

static int runtime_mesh_blas_cache_3d_slot_for(const RayTracingRuntimeMeshAsset* asset) {
    int free_slot = -1;
    if (!asset) return -1;
    for (int i = 0; i < RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS; ++i) {
        RuntimeMeshBLASCache3DEntry* entry = &gRuntimeMeshBLASCache3D[i];
        if (!entry->valid) {
            if (free_slot < 0) free_slot = i;
            continue;
        }
        if (strcmp(entry->asset_id, asset->asset_id) == 0 &&
            strcmp(entry->path, asset->path) == 0) {
            RuntimeTriangleMesh3D_Free(&entry->local_mesh);
            memset(entry, 0, sizeof(*entry));
            gRuntimeMeshBLASCache3DDiagnostics.blasCacheInvalidations += 1u;
            return i;
        }
    }
    if (free_slot >= 0) return free_slot;

    RuntimeTriangleMesh3D_Free(&gRuntimeMeshBLASCache3D[0].local_mesh);
    memset(&gRuntimeMeshBLASCache3D[0], 0, sizeof(gRuntimeMeshBLASCache3D[0]));
    gRuntimeMeshBLASCache3DDiagnostics.blasCacheInvalidations += 1u;
    return 0;
}

static Vec3 runtime_mesh_blas_cache_3d_vertex_position(
    const CoreMeshAssetRuntimeVertex* vertex) {
    if (!vertex) return vec3(0.0, 0.0, 0.0);
    return vec3(vertex->position.x, vertex->position.y, vertex->position.z);
}

static bool runtime_mesh_blas_cache_3d_build_local_mesh(
    const RayTracingRuntimeMeshAsset* asset,
    RuntimeTriangleMesh3D* out_mesh) {
    const CoreMeshAssetRuntimeDocument* document = NULL;
    RuntimeTriangle3D* triangles = NULL;
    int appended = 0;

    if (!asset || !out_mesh) {
        runtime_mesh_blas_cache_3d_set_diag("BLAS build failed: invalid input");
        return false;
    }
    document = &asset->document;
    RuntimeTriangleMesh3D_Init(out_mesh);
    if (document->triangle_count == 0u || !document->triangles ||
        document->vertex_count == 0u || !document->vertices) {
        return true;
    }
    if (document->triangle_count > (size_t)INT_MAX) {
        runtime_mesh_blas_cache_3d_set_diag("BLAS build failed: triangle count exceeds int range");
        return false;
    }

    triangles = (RuntimeTriangle3D*)calloc(document->triangle_count, sizeof(*triangles));
    if (!triangles) {
        runtime_mesh_blas_cache_3d_set_diag("BLAS build failed: triangle allocation failed");
        return false;
    }

    for (size_t i = 0; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle* src = &document->triangles[i];
        RuntimeTriangle3D* dst = NULL;
        Vec3 p0;
        Vec3 p1;
        Vec3 p2;
        Vec3 normal;
        double normal_len = 0.0;

        if (src->a >= document->vertex_count ||
            src->b >= document->vertex_count ||
            src->c >= document->vertex_count) {
            continue;
        }
        p0 = runtime_mesh_blas_cache_3d_vertex_position(&document->vertices[src->a]);
        p1 = runtime_mesh_blas_cache_3d_vertex_position(&document->vertices[src->b]);
        p2 = runtime_mesh_blas_cache_3d_vertex_position(&document->vertices[src->c]);
        normal = vec3_cross(vec3_sub(p1, p0), vec3_sub(p2, p0));
        normal_len = vec3_length(normal);
        if (normal_len <= 1e-18) continue;
        normal = vec3_scale(normal, 1.0 / normal_len);

        dst = &triangles[appended++];
        dst->p0 = p0;
        dst->p1 = p1;
        dst->p2 = p2;
        dst->normal = normal;
        dst->twoSided = false;
        dst->primitiveIndex = -1;
        dst->sceneObjectIndex = -1;
        dst->localTriangleIndex = (int)i;
    }

    if (appended <= 0) {
        free(triangles);
        return true;
    }
    out_mesh->triangles = triangles;
    out_mesh->triangleCount = appended;
    out_mesh->triangleCapacity = appended;
    out_mesh->bvhDirty = true;
    if (!RuntimeTriangleMesh3D_BuildBVH(out_mesh)) {
        runtime_mesh_blas_cache_3d_set_diag(RuntimeTriangleMesh3D_BVHLastDiagnostics());
        RuntimeTriangleMesh3D_Free(out_mesh);
        return false;
    }
    return true;
}

static uint64_t runtime_mesh_blas_cache_3d_cached_asset_count(void) {
    uint64_t count = 0u;
    for (int i = 0; i < RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS; ++i) {
        if (gRuntimeMeshBLASCache3D[i].valid) count += 1u;
    }
    return count;
}

bool RuntimeMeshBLASCache3D_PrepareAssetSet(
    const RayTracingRuntimeMeshAssetSet* mesh_assets) {
    uint64_t call_hits = 0u;
    uint64_t call_misses = 0u;
    bool had_assets = false;

    runtime_mesh_blas_cache_3d_set_diag("ok");
    gRuntimeMeshBLASCache3DDiagnostics.blasPrepareCalls += 1u;
    if (!mesh_assets || mesh_assets->asset_count <= 0) {
        gRuntimeMeshBLASCache3DDiagnostics.enabled = false;
        gRuntimeMeshBLASCache3DDiagnostics.reuseStatus =
            RUNTIME_SCENE_ACCEL_3D_REUSE_DISABLED;
        gRuntimeMeshBLASCache3DDiagnostics.blasCachedAssetCount =
            runtime_mesh_blas_cache_3d_cached_asset_count();
        return true;
    }

    had_assets = true;
    for (int i = 0; i < mesh_assets->asset_count; ++i) {
        const RayTracingRuntimeMeshAsset* asset = &mesh_assets->assets[i];
        long long mtime_sec = 0;
        long long mtime_nsec = 0;
        long long file_size = 0;
        int cached_index = -1;
        int slot = -1;
        RuntimeMeshBLASCache3DEntry* entry = NULL;
        RuntimeTriangleMesh3D local_mesh;

        runtime_mesh_blas_cache_3d_stamp_path(asset->path,
                                              &mtime_sec,
                                              &mtime_nsec,
                                              &file_size);
        cached_index = runtime_mesh_blas_cache_3d_find(asset,
                                                       mtime_sec,
                                                       mtime_nsec,
                                                       file_size);
        if (cached_index >= 0) {
            gRuntimeMeshBLASCache3DDiagnostics.blasCacheHits += 1u;
            call_hits += 1u;
            continue;
        }

        gRuntimeMeshBLASCache3DDiagnostics.blasCacheMisses += 1u;
        call_misses += 1u;
        slot = runtime_mesh_blas_cache_3d_slot_for(asset);
        if (slot < 0) {
            runtime_mesh_blas_cache_3d_set_diag("BLAS cache failed: no cache slot available");
            return false;
        }
        memset(&local_mesh, 0, sizeof(local_mesh));
        if (!runtime_mesh_blas_cache_3d_build_local_mesh(asset, &local_mesh)) {
            return false;
        }

        entry = &gRuntimeMeshBLASCache3D[slot];
        RuntimeTriangleMesh3D_Free(&entry->local_mesh);
        memset(entry, 0, sizeof(*entry));
        entry->valid = true;
        snprintf(entry->asset_id, sizeof(entry->asset_id), "%s", asset->asset_id);
        snprintf(entry->path, sizeof(entry->path), "%s", asset->path);
        entry->mtime_sec = mtime_sec;
        entry->mtime_nsec = mtime_nsec;
        entry->file_size = file_size;
        entry->vertex_count = asset->document.vertex_count;
        entry->source_triangle_count = asset->document.triangle_count;
        entry->local_mesh = local_mesh;
        gRuntimeMeshBLASCache3DDiagnostics.blasFullRebuilds += 1u;
    }

    gRuntimeMeshBLASCache3DDiagnostics.enabled = had_assets;
    gRuntimeMeshBLASCache3DDiagnostics.blasCachedAssetCount =
        runtime_mesh_blas_cache_3d_cached_asset_count();
    if (call_misses > 0u) {
        gRuntimeMeshBLASCache3DDiagnostics.reuseStatus =
            RUNTIME_SCENE_ACCEL_3D_REUSE_REBUILT;
    } else if (call_hits > 0u) {
        gRuntimeMeshBLASCache3DDiagnostics.reuseStatus =
            RUNTIME_SCENE_ACCEL_3D_REUSE_REUSED;
    } else {
        gRuntimeMeshBLASCache3DDiagnostics.reuseStatus =
            RUNTIME_SCENE_ACCEL_3D_REUSE_DISABLED;
    }
    return true;
}

void RuntimeMeshBLASCache3D_SnapshotDiagnostics(
    RuntimeSceneAcceleration3DDiagnostics* out_diagnostics) {
    if (!out_diagnostics) return;
    *out_diagnostics = gRuntimeMeshBLASCache3DDiagnostics;
    out_diagnostics->blasCachedAssetCount =
        runtime_mesh_blas_cache_3d_cached_asset_count();
}

void RuntimeMeshBLASCache3D_ResetForTests(void) {
    for (int i = 0; i < RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS; ++i) {
        RuntimeTriangleMesh3D_Free(&gRuntimeMeshBLASCache3D[i].local_mesh);
        memset(&gRuntimeMeshBLASCache3D[i], 0, sizeof(gRuntimeMeshBLASCache3D[i]));
    }
    gRuntimeMeshBLASCache3DDiagnostics =
        RuntimeSceneAcceleration3DDiagnostics_Disabled();
    runtime_mesh_blas_cache_3d_set_diag("ok");
}

const char* RuntimeMeshBLASCache3D_LastDiagnostics(void) {
    return gRuntimeMeshBLASCache3DLastDiagnostics;
}
