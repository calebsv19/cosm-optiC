#include "import/runtime_mesh_asset_loader_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "core_io.h"
#include "import/runtime_mesh_asset_pack.h"

typedef struct RuntimeMeshAssetCacheEntry {
    bool valid;
    char asset_id[64];
    char path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX];
    long long mtime_sec;
    long long mtime_nsec;
    long long file_size;
    CoreMeshAssetRuntimeDocument document;
} RuntimeMeshAssetCacheEntry;

static RuntimeMeshAssetCacheEntry
    g_runtime_mesh_asset_cache[RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS];
static unsigned long long g_runtime_mesh_asset_cache_hits = 0u;
static unsigned long long g_runtime_mesh_asset_cache_misses = 0u;
static unsigned long long g_runtime_mesh_asset_cache_invalidations = 0u;
RayTracingRuntimeMeshAssetTimingStats g_runtime_mesh_asset_timing;

RayTracingRuntimeMeshAssetPersistentCacheMode runtime_mesh_asset_pack_cache_mode(void) {
    const char* mode = getenv("RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_MODE");
    if (!mode || !mode[0]) {
        return RAY_TRACING_RUNTIME_MESH_ASSET_PERSISTENT_CACHE_READ_WRITE;
    }
    if (strcmp(mode, "0") == 0 ||
        strcmp(mode, "off") == 0 ||
        strcmp(mode, "disabled") == 0 ||
        strcmp(mode, "disable") == 0) {
        return RAY_TRACING_RUNTIME_MESH_ASSET_PERSISTENT_CACHE_DISABLED;
    }
    if (strcmp(mode, "read_only") == 0 ||
        strcmp(mode, "readonly") == 0 ||
        strcmp(mode, "ro") == 0) {
        return RAY_TRACING_RUNTIME_MESH_ASSET_PERSISTENT_CACHE_READ_ONLY;
    }
    if (strcmp(mode, "refresh") == 0 ||
        strcmp(mode, "rebuild") == 0 ||
        strcmp(mode, "force") == 0) {
        return RAY_TRACING_RUNTIME_MESH_ASSET_PERSISTENT_CACHE_REFRESH;
    }
    return RAY_TRACING_RUNTIME_MESH_ASSET_PERSISTENT_CACHE_READ_WRITE;
}

static bool runtime_mesh_asset_pack_cache_reads_enabled(
    RayTracingRuntimeMeshAssetPersistentCacheMode mode) {
    return mode == RAY_TRACING_RUNTIME_MESH_ASSET_PERSISTENT_CACHE_READ_WRITE ||
           mode == RAY_TRACING_RUNTIME_MESH_ASSET_PERSISTENT_CACHE_READ_ONLY;
}

static bool runtime_mesh_asset_pack_cache_writes_enabled(
    RayTracingRuntimeMeshAssetPersistentCacheMode mode) {
    return mode == RAY_TRACING_RUNTIME_MESH_ASSET_PERSISTENT_CACHE_READ_WRITE ||
           mode == RAY_TRACING_RUNTIME_MESH_ASSET_PERSISTENT_CACHE_REFRESH;
}

static const char* runtime_mesh_asset_pack_cache_root(void) {
    const char* root = getenv("RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_ROOT");
    if (root && root[0]) return root;
    root = getenv("TMPDIR");
    if (root && root[0]) return root;
    return "/private/tmp";
}

static bool runtime_mesh_asset_pack_cache_dir(char* out_dir, size_t out_dir_size) {
    const char* root = runtime_mesh_asset_pack_cache_root();
    if (!out_dir || out_dir_size == 0u || !root || !root[0]) return false;
    if (snprintf(out_dir,
                 out_dir_size,
                 "%s/ray_tracing_runtime_mesh_asset_pack_cache",
                 root) >= (int)out_dir_size) {
        out_dir[0] = '\0';
        return false;
    }
    if (mkdir(out_dir, 0777) != 0) {
        struct stat st;
        if (stat(out_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
            out_dir[0] = '\0';
            return false;
        }
    }
    return true;
}

static void runtime_mesh_asset_pack_source_key(const char* resolved_path,
                                               long long mtime_sec,
                                               long long mtime_nsec,
                                               long long file_size,
                                               unsigned long long checksum,
                                               RayTracingRuntimeMeshAssetPackSourceKey* out_key) {
    if (!out_key) return;
    memset(out_key, 0, sizeof(*out_key));
    if (resolved_path) {
        snprintf(out_key->source_path, sizeof(out_key->source_path), "%s", resolved_path);
    }
    out_key->source_mtime_sec = (int64_t)mtime_sec;
    out_key->source_mtime_nsec = (int64_t)mtime_nsec;
    out_key->source_size_bytes = (int64_t)file_size;
    out_key->source_checksum = (uint64_t)checksum;
    out_key->core_mesh_asset_schema_version = CORE_MESH_ASSET_SCHEMA_VERSION_1;
    out_key->ray_tracing_cache_schema_version = 2u;
    out_key->pointer_size_bytes = (uint32_t)sizeof(void*);
}

static bool runtime_mesh_asset_pack_cache_path(const char* resolved_path,
                                               char* out_path,
                                               size_t out_path_size) {
    char cache_dir[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    if (out_path && out_path_size > 0u) out_path[0] = '\0';
    if (!resolved_path || !resolved_path[0] || !out_path || out_path_size == 0u) {
        return false;
    }
    if (!runtime_mesh_asset_pack_cache_dir(cache_dir, sizeof(cache_dir))) {
        return false;
    }
    return ray_tracing_runtime_mesh_asset_pack_cache_path_for_source(cache_dir,
                                                                     resolved_path,
                                                                     out_path,
                                                                     out_path_size);
}


static bool runtime_mesh_asset_document_copy(const CoreMeshAssetRuntimeDocument* src,
                                             CoreMeshAssetRuntimeDocument* dst,
                                             char* out_diagnostics,
                                             size_t out_diagnostics_size) {
    CoreResult result = core_result_ok();
    if (!src || !dst) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "mesh asset copy input missing");
        return false;
    }

    core_mesh_asset_runtime_document_init(dst);
    dst->contract = src->contract;

    result = core_mesh_asset_runtime_document_set_vertex_count(dst, src->vertex_count);
    if (result.code != CORE_OK) goto fail;
    dst->vertex_normal_count = src->vertex_normal_count;
    dst->normal_provenance = src->normal_provenance;
    result = core_mesh_asset_runtime_document_set_triangle_count(dst, src->triangle_count);
    if (result.code != CORE_OK) goto fail;
    result = core_mesh_asset_runtime_document_set_surface_group_count(dst,
                                                                      src->surface_group_count);
    if (result.code != CORE_OK) goto fail;

    if (src->vertex_count > 0u && src->vertices) {
        memcpy(dst->vertices, src->vertices, sizeof(*src->vertices) * src->vertex_count);
    }
    if (src->triangle_count > 0u && src->triangles) {
        memcpy(dst->triangles, src->triangles, sizeof(*src->triangles) * src->triangle_count);
    }
    if (src->surface_group_count > 0u && src->surface_groups) {
        memcpy(dst->surface_groups,
               src->surface_groups,
               sizeof(*src->surface_groups) * src->surface_group_count);
    }
    return true;

fail:
    runtime_mesh_asset_diag(out_diagnostics,
                            out_diagnostics_size,
                            result.message ? result.message : "mesh asset copy failed");
    core_mesh_asset_runtime_document_free(dst);
    return false;
}

static bool runtime_mesh_asset_cache_entry_matches(const RuntimeMeshAssetCacheEntry* entry,
                                                   const char* asset_id,
                                                   const char* path,
                                                   long long mtime_sec,
                                                   long long mtime_nsec,
                                                   long long file_size) {
    return entry && entry->valid &&
           strcmp(entry->asset_id, asset_id) == 0 &&
           strcmp(entry->path, path) == 0 &&
           entry->mtime_sec == mtime_sec &&
           entry->mtime_nsec == mtime_nsec &&
           entry->file_size == file_size;
}

static int runtime_mesh_asset_cache_find(const char* asset_id,
                                         const char* path,
                                         long long mtime_sec,
                                         long long mtime_nsec,
                                         long long file_size) {
    for (int i = 0; i < RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS; ++i) {
        if (runtime_mesh_asset_cache_entry_matches(&g_runtime_mesh_asset_cache[i],
                                                   asset_id,
                                                   path,
                                                   mtime_sec,
                                                   mtime_nsec,
                                                   file_size)) {
            return i;
        }
    }
    return -1;
}

static int runtime_mesh_asset_cache_slot_for(const char* asset_id, const char* path) {
    int free_slot = -1;
    for (int i = 0; i < RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS; ++i) {
        RuntimeMeshAssetCacheEntry* entry = &g_runtime_mesh_asset_cache[i];
        if (!entry->valid) {
            if (free_slot < 0) free_slot = i;
            continue;
        }
        if (strcmp(entry->asset_id, asset_id) == 0 && strcmp(entry->path, path) == 0) {
            core_mesh_asset_runtime_document_free(&entry->document);
            memset(entry, 0, sizeof(*entry));
            g_runtime_mesh_asset_cache_invalidations += 1u;
            return i;
        }
    }
    return free_slot;
}

bool runtime_mesh_asset_load_document_cached(const char* resolved_path,
                                                    const char* asset_id,
                                                    CoreMeshAssetRuntimeDocument* out_document,
                                                    char* out_diagnostics,
                                                    size_t out_diagnostics_size) {
    struct stat st;
    long long mtime_sec = 0;
    long long mtime_nsec = 0;
    long long file_size = 0;
    int cached_index = -1;
    int cache_slot = -1;
    RuntimeMeshAssetCacheEntry* entry = NULL;
    CoreResult load_result = core_result_ok();
    struct timespec load_start = {0};
    struct timespec copy_start = {0};
    bool copy_ok = false;
    char pack_cache_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    bool pack_cache_path_ok = false;
    RayTracingRuntimeMeshAssetPersistentCacheMode pack_cache_mode =
        (RayTracingRuntimeMeshAssetPersistentCacheMode)
            g_runtime_mesh_asset_timing.asset_persistent_cache_mode;
    bool pack_cache_can_read = runtime_mesh_asset_pack_cache_reads_enabled(pack_cache_mode);
    bool pack_cache_can_write = runtime_mesh_asset_pack_cache_writes_enabled(pack_cache_mode);

    (void)clock_gettime(CLOCK_MONOTONIC, &load_start);
    g_runtime_mesh_asset_timing.asset_load_calls += 1;
    if (!resolved_path || !asset_id || !out_document) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "mesh asset cache input missing");
        return false;
    }
    if (stat(resolved_path, &st) != 0) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "mesh asset runtime file stat failed");
        return false;
    }
    runtime_mesh_asset_file_stamp(&st, &mtime_sec, &mtime_nsec, &file_size);

    cached_index = runtime_mesh_asset_cache_find(asset_id,
                                                resolved_path,
                                                mtime_sec,
                                                mtime_nsec,
                                                file_size);
    if (cached_index >= 0) {
        g_runtime_mesh_asset_cache_hits += 1u;
        g_runtime_mesh_asset_timing.asset_cache_hits += 1;
        (void)clock_gettime(CLOCK_MONOTONIC, &copy_start);
        copy_ok = runtime_mesh_asset_document_copy(&g_runtime_mesh_asset_cache[cached_index].document,
                                                   out_document,
                                                   out_diagnostics,
                                                   out_diagnostics_size);
        g_runtime_mesh_asset_timing.asset_document_copy_ms +=
            runtime_mesh_asset_elapsed_ms_since(&copy_start);
        g_runtime_mesh_asset_timing.asset_load_total_ms +=
            runtime_mesh_asset_elapsed_ms_since(&load_start);
        return copy_ok;
    }

    g_runtime_mesh_asset_cache_misses += 1u;
    g_runtime_mesh_asset_timing.asset_cache_misses += 1;
    cache_slot = runtime_mesh_asset_cache_slot_for(asset_id, resolved_path);
    if (cache_slot < 0) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "mesh asset cache full");
        return false;
    }

    entry = &g_runtime_mesh_asset_cache[cache_slot];
    memset(entry, 0, sizeof(*entry));
    core_mesh_asset_runtime_document_init(&entry->document);

    if (pack_cache_mode == RAY_TRACING_RUNTIME_MESH_ASSET_PERSISTENT_CACHE_REFRESH) {
        g_runtime_mesh_asset_timing.asset_persistent_cache_refreshes += 1;
    }
    if (pack_cache_can_read || pack_cache_can_write) {
        pack_cache_path_ok = runtime_mesh_asset_pack_cache_path(resolved_path,
                                                                pack_cache_path,
                                                                sizeof(pack_cache_path));
    }
    if (pack_cache_can_read && pack_cache_path_ok && core_io_path_exists(pack_cache_path)) {
        RayTracingRuntimeMeshAssetPackSourceKey source_key;
        char pack_diagnostics[256] = {0};
        struct timespec pack_read_start = {0};
        runtime_mesh_asset_pack_source_key(resolved_path,
                                           mtime_sec,
                                           mtime_nsec,
                                           file_size,
                                           0u,
                                           &source_key);
        (void)clock_gettime(CLOCK_MONOTONIC, &pack_read_start);
        if (ray_tracing_runtime_mesh_asset_pack_read_cache_file(pack_cache_path,
                                                               &source_key,
                                                               &entry->document,
                                                               pack_diagnostics,
                                                               sizeof(pack_diagnostics))) {
            g_runtime_mesh_asset_timing.asset_persistent_cache_hits += 1;
            g_runtime_mesh_asset_timing.asset_persistent_cache_read_ms +=
                runtime_mesh_asset_elapsed_ms_since(&pack_read_start);
            goto document_loaded;
        }
        g_runtime_mesh_asset_timing.asset_persistent_cache_invalidations += 1;
        g_runtime_mesh_asset_timing.asset_persistent_cache_read_ms +=
            runtime_mesh_asset_elapsed_ms_since(&pack_read_start);
    }
    if (pack_cache_path_ok && (pack_cache_can_read || pack_cache_can_write)) {
        g_runtime_mesh_asset_timing.asset_persistent_cache_misses += 1;
    }

    {
        struct timespec document_load_start = {0};
        (void)clock_gettime(CLOCK_MONOTONIC, &document_load_start);
        load_result = core_mesh_asset_runtime_document_load_file(resolved_path, &entry->document);
        g_runtime_mesh_asset_timing.asset_runtime_document_load_ms +=
            runtime_mesh_asset_elapsed_ms_since(&document_load_start);
    }
    if (load_result.code != CORE_OK) {
        runtime_mesh_asset_diag(out_diagnostics,
                                out_diagnostics_size,
                                load_result.message ? load_result.message : "validation failed");
        core_mesh_asset_runtime_document_free(&entry->document);
        memset(entry, 0, sizeof(*entry));
        return false;
    }
    if (pack_cache_can_write && pack_cache_path_ok) {
        RayTracingRuntimeMeshAssetPackSourceKey source_key;
        uint64_t checksum = 0u;
        char pack_diagnostics[256] = {0};
        struct timespec pack_write_start = {0};
        (void)ray_tracing_runtime_mesh_asset_pack_checksum_file(resolved_path, &checksum);
        runtime_mesh_asset_pack_source_key(resolved_path,
                                           mtime_sec,
                                           mtime_nsec,
                                           file_size,
                                           checksum,
                                           &source_key);
        (void)clock_gettime(CLOCK_MONOTONIC, &pack_write_start);
        if (ray_tracing_runtime_mesh_asset_pack_write_cache_file(pack_cache_path,
                                                                &source_key,
                                                                &entry->document,
                                                                pack_diagnostics,
                                                                sizeof(pack_diagnostics))) {
            g_runtime_mesh_asset_timing.asset_persistent_cache_writes += 1;
        }
        g_runtime_mesh_asset_timing.asset_persistent_cache_write_ms +=
            runtime_mesh_asset_elapsed_ms_since(&pack_write_start);
    }

document_loaded:
    snprintf(entry->asset_id, sizeof(entry->asset_id), "%s", asset_id);
    snprintf(entry->path, sizeof(entry->path), "%s", resolved_path);
    entry->mtime_sec = mtime_sec;
    entry->mtime_nsec = mtime_nsec;
    entry->file_size = file_size;
    entry->valid = true;

    (void)clock_gettime(CLOCK_MONOTONIC, &copy_start);
    copy_ok = runtime_mesh_asset_document_copy(&entry->document,
                                               out_document,
                                               out_diagnostics,
                                               out_diagnostics_size);
    g_runtime_mesh_asset_timing.asset_document_copy_ms +=
        runtime_mesh_asset_elapsed_ms_since(&copy_start);
    g_runtime_mesh_asset_timing.asset_load_total_ms +=
        runtime_mesh_asset_elapsed_ms_since(&load_start);
    return copy_ok;
}

void ray_tracing_runtime_mesh_assets_reset_cache(void) {
    for (int i = 0; i < RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS; ++i) {
        if (g_runtime_mesh_asset_cache[i].valid) {
            core_mesh_asset_runtime_document_free(&g_runtime_mesh_asset_cache[i].document);
        }
        memset(&g_runtime_mesh_asset_cache[i], 0, sizeof(g_runtime_mesh_asset_cache[i]));
    }
    g_runtime_mesh_asset_cache_hits = 0u;
    g_runtime_mesh_asset_cache_misses = 0u;
    g_runtime_mesh_asset_cache_invalidations = 0u;
}

void ray_tracing_runtime_mesh_assets_cache_stats(unsigned long long* out_hits,
                                                unsigned long long* out_misses,
                                                unsigned long long* out_invalidations,
                                                int* out_cached_assets) {
    int cached_assets = 0;
    for (int i = 0; i < RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS; ++i) {
        if (g_runtime_mesh_asset_cache[i].valid) cached_assets += 1;
    }
    if (out_hits) *out_hits = g_runtime_mesh_asset_cache_hits;
    if (out_misses) *out_misses = g_runtime_mesh_asset_cache_misses;
    if (out_invalidations) *out_invalidations = g_runtime_mesh_asset_cache_invalidations;
    if (out_cached_assets) *out_cached_assets = cached_assets;
}

void ray_tracing_runtime_mesh_assets_timing_reset(void) {
    memset(&g_runtime_mesh_asset_timing, 0, sizeof(g_runtime_mesh_asset_timing));
}

void ray_tracing_runtime_mesh_assets_timing_snapshot(
    RayTracingRuntimeMeshAssetTimingStats* out_stats) {
    if (!out_stats) return;
    *out_stats = g_runtime_mesh_asset_timing;
}
