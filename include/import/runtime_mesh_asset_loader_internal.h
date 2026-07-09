#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include <time.h>

#include <json-c/json.h>

#include "import/runtime_mesh_asset_loader.h"

extern RayTracingRuntimeMeshAssetTimingStats g_runtime_mesh_asset_timing;

double runtime_mesh_asset_elapsed_ms_since(const struct timespec* start_time);
void runtime_mesh_asset_diag(char* out_diagnostics,
                             size_t out_diagnostics_size,
                             const char* message);
bool runtime_mesh_asset_copy_id(char* out_id,
                                size_t out_id_size,
                                const char* id,
                                char* out_diagnostics,
                                size_t out_diagnostics_size);
bool runtime_mesh_asset_read_text(const char* path,
                                  char** out_text,
                                  char* out_diagnostics,
                                  size_t out_diagnostics_size);
const char* runtime_mesh_asset_string_field(json_object* obj, const char* key);
const char* runtime_mesh_asset_object_runtime_path(json_object* object);
bool runtime_mesh_asset_try_asset_root(const char* root,
                                       const char* asset_id,
                                       char* out_path,
                                       size_t out_path_size,
                                       char* out_diagnostics,
                                       size_t out_diagnostics_size);
bool runtime_mesh_asset_is_authoring_helper_object_type(const char* object_type);
void runtime_mesh_asset_probe_preview(const char* runtime_mesh_path,
                                      RayTracingRuntimeMeshPreviewInfo* out_preview);
double runtime_mesh_asset_number_field_or(json_object* obj,
                                          const char* key,
                                          double fallback);
void runtime_mesh_asset_read_vec3_or(json_object* obj,
                                     double fallback_x,
                                     double fallback_y,
                                     double fallback_z,
                                     double* out_x,
                                     double* out_y,
                                     double* out_z);
void runtime_mesh_asset_read_transform(json_object* object,
                                       double world_scale,
                                       RayTracingRuntimeMeshAssetInstance* instance);
int runtime_mesh_asset_find_asset_index(const RayTracingRuntimeMeshAssetSet* set,
                                        const char* asset_id);
void runtime_mesh_asset_file_stamp(const struct stat* st,
                                   long long* out_mtime_sec,
                                   long long* out_mtime_nsec,
                                   long long* out_file_size);
bool runtime_mesh_asset_stat_path(const char* path,
                                  long long* out_mtime_sec,
                                  long long* out_mtime_nsec,
                                  long long* out_file_size);
bool runtime_mesh_asset_stamp_matches_path(const char* path,
                                           long long expected_mtime_sec,
                                           long long expected_mtime_nsec,
                                           long long expected_file_size);
RayTracingRuntimeMeshAssetPersistentCacheMode runtime_mesh_asset_pack_cache_mode(void);
bool runtime_mesh_asset_load_document_cached(const char* resolved_path,
                                             const char* asset_id,
                                             CoreMeshAssetRuntimeDocument* out_document,
                                             char* out_diagnostics,
                                             size_t out_diagnostics_size);
