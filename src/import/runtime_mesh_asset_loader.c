#include "import/runtime_mesh_asset_loader_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include <json-c/json.h>

#include "config/config_manager.h"
#include "core_io.h"
#include "core_scene.h"
#include "procedural/procedural_surface_binding.h"
#include "procedural/procedural_surface_field_graph.h"
#include "procedural/procedural_solid_material_binding.h"
#include "import/runtime_mesh_asset_loader_authored_material.h"

static RayTracingRuntimeMeshAssetSet g_last_runtime_mesh_assets;
static bool g_last_runtime_mesh_asset_scene_stamp_valid = false;
static char g_last_runtime_mesh_asset_scene_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
static long long g_last_runtime_mesh_asset_scene_mtime_sec = 0;
static long long g_last_runtime_mesh_asset_scene_mtime_nsec = 0;
static long long g_last_runtime_mesh_asset_scene_file_size = 0;

static bool ray_tracing_runtime_mesh_asset_resolve_path_with_hint(const char* runtime_scene_path,
                                                                  const char* asset_id,
                                                                  const char* explicit_runtime_path,
                                                                  char* out_path,
                                                                  size_t out_path_size,
                                                                  char* out_diagnostics,
                                                                  size_t out_diagnostics_size);

static bool runtime_mesh_asset_resolve_relative_to_file(
    const char* owner_path,
    const char* referenced_path,
    char* out_path,
    size_t out_path_size) {
    char base_dir[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    if (!owner_path || !referenced_path || !referenced_path[0] || !out_path ||
        out_path_size == 0u) {
        return false;
    }
    if (referenced_path[0] == '/') {
        return snprintf(out_path, out_path_size, "%s", referenced_path) <
               (int)out_path_size;
    }
    return core_scene_dirname(owner_path, base_dir, sizeof(base_dir)).code ==
               CORE_OK &&
           core_scene_resolve_path(base_dir, referenced_path, out_path,
                                   out_path_size).code == CORE_OK;
}

static bool runtime_mesh_asset_paths_identify_same_file(const char* a,
                                                        const char* b) {
    struct stat a_stat;
    struct stat b_stat;
    if (!a || !b || stat(a, &a_stat) != 0 || stat(b, &b_stat) != 0) {
        return false;
    }
    return a_stat.st_dev == b_stat.st_dev && a_stat.st_ino == b_stat.st_ino;
}

static bool runtime_mesh_asset_capture_dependency(
    const char* path,
    RayTracingRuntimeMeshAssetFileDependency* dependency) {
    if (!path || !path[0] || !dependency) return false;
    memset(dependency, 0, sizeof(*dependency));
    if (snprintf(dependency->path, sizeof(dependency->path), "%s", path) >=
        (int)sizeof(dependency->path)) {
        return false;
    }
    dependency->stamp_valid =
        runtime_mesh_asset_stat_path(path,
                                     &dependency->mtime_sec,
                                     &dependency->mtime_nsec,
                                     &dependency->size_bytes);
    return dependency->stamp_valid;
}

static bool runtime_mesh_asset_dependency_matches(
    const RayTracingRuntimeMeshAssetFileDependency* dependency) {
    return dependency && dependency->stamp_valid &&
           runtime_mesh_asset_stamp_matches_path(dependency->path,
                                                 dependency->mtime_sec,
                                                 dependency->mtime_nsec,
                                                 dependency->size_bytes);
}

static bool runtime_mesh_asset_load_procedural_surface_ref(
    const char* runtime_scene_path,
    json_object* object,
    RayTracingRuntimeMeshAsset* asset,
    char* out_diagnostics,
    size_t out_diagnostics_size) {
    json_object* reference = NULL;
    const char* manifest_reference = NULL;
    char manifest_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    char recipe_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    char field_graph_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    char binding_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    char mesh_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    char material_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    ProceduralSurfaceDerivedAssetManifest manifest;
    ProceduralSurfaceDerivedAssetMaterial material;
    ProceduralSurfaceDerivedAssetReport derived_report;
    ProceduralSurfaceRecipeV1 recipe;
    ProceduralSurfaceRecipeReport recipe_report;
    char recipe_digest[PROCEDURAL_SURFACE_RECIPE_DIGEST_CAPACITY] = {0};
    char field_graph_digest[PROCEDURAL_SURFACE_FIELD_GRAPH_DIGEST_CAPACITY] = {0};
    char binding_digest[PROCEDURAL_SURFACE_BINDING_DIGEST_CAPACITY] = {0};

    if (!object || !asset) return false;
    if (!json_object_object_get_ex(object, "procedural_surface_ref", &reference)) {
        return true;
    }
    if (!json_object_is_type(reference, json_type_object)) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size,
                                "procedural_surface_ref must be an object");
        return false;
    }
    manifest_reference =
        runtime_mesh_asset_string_field(reference, "manifest_path");
    if (!manifest_reference ||
        !runtime_mesh_asset_resolve_relative_to_file(
            runtime_scene_path, manifest_reference, manifest_path,
            sizeof(manifest_path))) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size,
                                "procedural surface manifest path is invalid");
        return false;
    }
    memset(&manifest, 0, sizeof(manifest));
    ProceduralSurfaceDerivedAssetMaterial_Init(&material);
    if (!ProceduralSurfaceDerivedAssetManifest_LoadJsonFile(
            manifest_path, &manifest, &derived_report)) {
        char message[256] = {0};
        snprintf(message, sizeof(message), "procedural surface manifest invalid: %s",
                 derived_report.message);
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, message);
        return false;
    }
    if (strcmp(manifest.asset_id, asset->asset_id) != 0 ||
        strcmp(manifest.source_asset_id,
               asset->document.contract.source_asset_id) != 0) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size,
                                "procedural surface asset identity mismatch");
        return false;
    }
    if (!runtime_mesh_asset_resolve_relative_to_file(
            manifest_path, manifest.recipe_path, recipe_path,
            sizeof(recipe_path)) ||
        !runtime_mesh_asset_resolve_relative_to_file(
            manifest_path, manifest.mesh_path, mesh_path, sizeof(mesh_path)) ||
        !runtime_mesh_asset_resolve_relative_to_file(
            manifest_path, manifest.material_path, material_path,
            sizeof(material_path))) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size,
                                "procedural surface referenced path is invalid");
        return false;
    }
    if (!runtime_mesh_asset_paths_identify_same_file(mesh_path, asset->path)) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size,
                                "procedural surface mesh path mismatch");
        return false;
    }
    if (!ProceduralSurfaceRecipeV1_LoadJsonFile(
            recipe_path, &recipe, &recipe_report) ||
        !ProceduralSurfaceRecipeV1_Digest(
            &recipe, recipe_digest, &recipe_report) ||
        strcmp(recipe_digest, manifest.recipe_digest_sha256) != 0) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size,
                                "procedural surface recipe reference is stale");
        return false;
    }
    if (manifest.schema_version >= 2u) {
        ProceduralSurfaceFieldGraphV1 field_graph;
        ProceduralSurfaceFieldGraphReport field_graph_report;
        ProceduralSurfaceBindingV1 binding;
        ProceduralSurfaceBindingReport binding_report;
        if (!runtime_mesh_asset_resolve_relative_to_file(
                manifest_path, manifest.field_graph_path, field_graph_path,
                sizeof(field_graph_path)) ||
            !runtime_mesh_asset_resolve_relative_to_file(
                manifest_path, manifest.binding_path, binding_path,
                sizeof(binding_path)) ||
            !ProceduralSurfaceFieldGraphV1_LoadJsonFile(
                field_graph_path, &field_graph, &field_graph_report) ||
            !ProceduralSurfaceFieldGraphV1_Digest(
                &field_graph, field_graph_digest, &field_graph_report) ||
            strcmp(field_graph_digest,
                   manifest.field_graph_digest_sha256) != 0 ||
            !ProceduralSurfaceBindingV1_LoadJsonFile(
                binding_path, &binding, &binding_report) ||
            !ProceduralSurfaceBindingV1_Validate(
                &binding, &field_graph, &binding_report) ||
            !ProceduralSurfaceBindingV1_Digest(
                &binding, binding_digest, &binding_report) ||
            strcmp(binding_digest, manifest.binding_digest_sha256) != 0) {
            runtime_mesh_asset_diag(
                out_diagnostics, out_diagnostics_size,
                "procedural surface graph or binding reference is stale");
            return false;
        }
    }
    if (!ProceduralSurfaceDerivedAssetMaterial_LoadJsonFile(
            material_path, &manifest, asset->document.vertex_count,
            asset->document.triangle_count, &material, &derived_report)) {
        char message[256] = {0};
        snprintf(message, sizeof(message), "procedural surface material invalid: %s",
                 derived_report.message);
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, message);
        return false;
    }
    for (size_t i = 0u; i < asset->document.triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle* triangle =
            &asset->document.triangles[i];
        if (material.triangle_indices[(i * 3u) + 0u] != triangle->a ||
            material.triangle_indices[(i * 3u) + 1u] != triangle->b ||
            material.triangle_indices[(i * 3u) + 2u] != triangle->c) {
            ProceduralSurfaceDerivedAssetMaterial_Free(&material);
            runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size,
                                    "procedural surface material topology mismatch");
            return false;
        }
    }
    if (asset->procedural_surface_valid) {
        bool same = strcmp(asset->procedural_manifest.cache_identity_sha256,
                           manifest.cache_identity_sha256) == 0;
        ProceduralSurfaceDerivedAssetMaterial_Free(&material);
        if (!same) {
            runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size,
                                    "procedural surface instance identity mismatch");
        }
        return same;
    }
    if (!runtime_mesh_asset_capture_dependency(
            manifest_path, &asset->procedural_manifest_dependency) ||
        !runtime_mesh_asset_capture_dependency(
            recipe_path, &asset->procedural_recipe_dependency) ||
        (manifest.schema_version >= 2u &&
         (!runtime_mesh_asset_capture_dependency(
              field_graph_path,
              &asset->procedural_field_graph_dependency) ||
          !runtime_mesh_asset_capture_dependency(
              binding_path, &asset->procedural_binding_dependency))) ||
        !runtime_mesh_asset_capture_dependency(
            material_path, &asset->procedural_material_dependency)) {
        ProceduralSurfaceDerivedAssetMaterial_Free(&material);
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size,
                                "procedural surface dependency stat failed");
        return false;
    }
    asset->procedural_surface_valid = true;
    snprintf(asset->procedural_manifest_path,
             sizeof(asset->procedural_manifest_path), "%s", manifest_path);
    asset->procedural_manifest = manifest;
    asset->procedural_material = material;
    runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

static bool runtime_mesh_asset_load_procedural_solid_material_ref(
    const char* runtime_scene_path,
    json_object* object,
    RayTracingRuntimeMeshAsset* asset,
    char* out_diagnostics,
    size_t out_diagnostics_size) {
    json_object* reference = NULL;
    const char* binding_reference = NULL;
    char binding_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    ProceduralSolidMaterialBindingV1 binding;
    ProceduralSolidMaterialBindingReport report = {0};
    char digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY] = {0};
    if (!object || !asset) return false;
    if (!json_object_object_get_ex(
            object, "procedural_solid_material_ref", &reference)) {
        if (asset->procedural_solid_material_reference_observed) {
            runtime_mesh_asset_diag(
                out_diagnostics, out_diagnostics_size,
                "procedural solid material references must be consistent "
                "across instances of one mesh asset");
            return false;
        }
        asset->procedural_solid_material_reference_absent = true;
        return true;
    }
    if (asset->procedural_solid_material_reference_absent) {
        runtime_mesh_asset_diag(
            out_diagnostics, out_diagnostics_size,
            "procedural solid material references must be consistent "
            "across instances of one mesh asset");
        return false;
    }
    asset->procedural_solid_material_reference_observed = true;
    if (!json_object_is_type(reference, json_type_object)) {
        runtime_mesh_asset_diag(
            out_diagnostics, out_diagnostics_size,
            "procedural_solid_material_ref must be an object");
        return false;
    }
    binding_reference =
        runtime_mesh_asset_string_field(reference, "binding_path");
    if (!binding_reference ||
        !runtime_mesh_asset_resolve_relative_to_file(
            runtime_scene_path, binding_reference, binding_path,
            sizeof(binding_path)) ||
        !ProceduralSolidMaterialBindingV1_LoadJsonFile(
            binding_path, &binding, &report) ||
        !ProceduralSolidMaterialBindingV1_Validate(
            &binding, &asset->document, &report) ||
        !ProceduralSolidMaterialBindingV1_Digest(
            &binding, digest, &report)) {
        char message[320] = {0};
        snprintf(
            message, sizeof(message),
            "procedural solid material binding invalid: %s",
            report.message[0] ? report.message : "path resolution failed");
        runtime_mesh_asset_diag(
            out_diagnostics, out_diagnostics_size, message);
        return false;
    }
    if (asset->procedural_solid_material_valid) {
        char existing_digest[
            PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY] = {0};
        if (!ProceduralSolidMaterialBindingV1_Digest(
                &asset->procedural_solid_material_binding,
                existing_digest, &report) ||
            strcmp(existing_digest, digest) != 0) {
            runtime_mesh_asset_diag(
                out_diagnostics, out_diagnostics_size,
                "procedural solid material instance identity mismatch");
            return false;
        }
        return true;
    }
    if (!runtime_mesh_asset_capture_dependency(
            binding_path,
            &asset->procedural_solid_material_binding_dependency)) {
        runtime_mesh_asset_diag(
            out_diagnostics, out_diagnostics_size,
            "procedural solid material dependency stat failed");
        return false;
    }
    asset->procedural_solid_material_valid = true;
    snprintf(
        asset->procedural_solid_material_binding_path,
        sizeof(asset->procedural_solid_material_binding_path), "%s",
        binding_path);
    asset->procedural_solid_material_binding = binding;
    return true;
}

static void runtime_mesh_asset_clear_last_scene_stamp(void) {
    g_last_runtime_mesh_asset_scene_stamp_valid = false;
    g_last_runtime_mesh_asset_scene_path[0] = '\0';
    g_last_runtime_mesh_asset_scene_mtime_sec = 0;
    g_last_runtime_mesh_asset_scene_mtime_nsec = 0;
    g_last_runtime_mesh_asset_scene_file_size = 0;
}

static void runtime_mesh_asset_capture_last_scene_stamp(const char* runtime_scene_path) {
    long long mtime_sec = 0;
    long long mtime_nsec = 0;
    long long file_size = 0;
    runtime_mesh_asset_clear_last_scene_stamp();
    if (!runtime_scene_path || !runtime_scene_path[0]) return;
    if (!runtime_mesh_asset_stat_path(runtime_scene_path, &mtime_sec, &mtime_nsec, &file_size)) {
        return;
    }
    snprintf(g_last_runtime_mesh_asset_scene_path,
             sizeof(g_last_runtime_mesh_asset_scene_path),
             "%s",
             runtime_scene_path);
    g_last_runtime_mesh_asset_scene_mtime_sec = mtime_sec;
    g_last_runtime_mesh_asset_scene_mtime_nsec = mtime_nsec;
    g_last_runtime_mesh_asset_scene_file_size = file_size;
    g_last_runtime_mesh_asset_scene_stamp_valid = true;
}


static bool runtime_mesh_asset_append_instance(RayTracingRuntimeMeshAssetSet* set,
                                               const char* object_id,
                                               const char* asset_id,
                                               int asset_index,
                                               int scene_object_index,
                                               json_object* object,
                                               double world_scale,
                                               char* out_diagnostics,
                                               size_t out_diagnostics_size) {
    RayTracingRuntimeMeshAssetInstance* instance = NULL;
    if (!set || !object_id || !object_id[0] || !asset_id || !asset_id[0] || asset_index < 0) {
        runtime_mesh_asset_diag(out_diagnostics,
                                out_diagnostics_size,
                                "mesh asset instance is invalid");
        return false;
    }
    if (set->instance_count >= RAY_TRACING_RUNTIME_MESH_ASSET_MAX_INSTANCES) {
        runtime_mesh_asset_diag(out_diagnostics,
                                out_diagnostics_size,
                                "too many mesh asset instances");
        return false;
    }
    instance = &set->instances[set->instance_count++];
    memset(instance, 0, sizeof(*instance));
    if (!runtime_mesh_asset_copy_id(instance->object_id,
                                    sizeof(instance->object_id),
                                    object_id,
                                    out_diagnostics,
                                    out_diagnostics_size) ||
        !runtime_mesh_asset_copy_id(instance->asset_id,
                                    sizeof(instance->asset_id),
                                    asset_id,
                                    out_diagnostics,
                                    out_diagnostics_size)) {
        set->instance_count -= 1;
        memset(instance, 0, sizeof(*instance));
        return false;
    }
    instance->asset_index = asset_index;
    instance->scene_object_index = scene_object_index;
    runtime_mesh_asset_read_transform(object, world_scale, instance);
    return true;
}

static bool runtime_mesh_asset_load_unique_asset(const char* runtime_scene_path,
                                                 const char* asset_id,
                                                 const char* explicit_runtime_path,
                                                 RayTracingRuntimeMeshAssetSet* set,
                                                 int* out_asset_index,
                                                 bool* out_skipped,
                                                 char* out_resolved_path,
                                                 size_t out_resolved_path_size,
                                                 size_t* out_file_size_bytes,
                                                 size_t max_asset_file_bytes,
                                                 char* out_diagnostics,
                                                 size_t out_diagnostics_size) {
    RayTracingRuntimeMeshAsset* asset = NULL;
    char resolved_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    int existing = runtime_mesh_asset_find_asset_index(set, asset_id);
    struct timespec resolve_start = {0};

    if (out_asset_index) *out_asset_index = -1;
    if (out_skipped) *out_skipped = false;
    if (out_resolved_path && out_resolved_path_size > 0u) out_resolved_path[0] = '\0';
    if (out_file_size_bytes) *out_file_size_bytes = 0u;
    if (existing >= 0) {
        if (out_asset_index) *out_asset_index = existing;
        return true;
    }
    if (!set || !asset_id || !asset_id[0]) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "mesh asset id missing");
        return false;
    }
    if (set->asset_count >= RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "too many mesh assets");
        return false;
    }
    (void)clock_gettime(CLOCK_MONOTONIC, &resolve_start);
    if (!ray_tracing_runtime_mesh_asset_resolve_path_with_hint(runtime_scene_path,
                                                               asset_id,
                                                               explicit_runtime_path,
                                                               resolved_path,
                                                               sizeof(resolved_path),
                                                               out_diagnostics,
                                                               out_diagnostics_size)) {
        g_runtime_mesh_asset_timing.sidecar_path_resolution_ms +=
            runtime_mesh_asset_elapsed_ms_since(&resolve_start);
        return false;
    }
    g_runtime_mesh_asset_timing.sidecar_path_resolution_ms +=
        runtime_mesh_asset_elapsed_ms_since(&resolve_start);
    if (max_asset_file_bytes > 0u) {
        struct stat st;
        if (stat(resolved_path, &st) != 0) {
            runtime_mesh_asset_diag(out_diagnostics,
                                    out_diagnostics_size,
                                    "mesh asset runtime file stat failed");
            return false;
        }
        if (out_file_size_bytes) *out_file_size_bytes = (size_t)st.st_size;
        if (st.st_size > (off_t)max_asset_file_bytes) {
            if (out_skipped) *out_skipped = true;
            if (out_resolved_path && out_resolved_path_size > 0u) {
                snprintf(out_resolved_path, out_resolved_path_size, "%s", resolved_path);
            }
            runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "ok");
            return true;
        }
    }
    if (out_resolved_path && out_resolved_path_size > 0u) {
        snprintf(out_resolved_path, out_resolved_path_size, "%s", resolved_path);
    }

    asset = &set->assets[set->asset_count];
    memset(asset, 0, sizeof(*asset));
    core_mesh_asset_runtime_document_init(&asset->document);
    if (!runtime_mesh_asset_copy_id(asset->asset_id,
                                    sizeof(asset->asset_id),
                                    asset_id,
                                    out_diagnostics,
                                    out_diagnostics_size)) {
        core_mesh_asset_runtime_document_free(&asset->document);
        return false;
    }
    snprintf(asset->path, sizeof(asset->path), "%s", resolved_path);
    asset->file_stamp_valid =
        runtime_mesh_asset_stat_path(resolved_path,
                                     &asset->file_mtime_sec,
                                     &asset->file_mtime_nsec,
                                     &asset->file_size_bytes);
    runtime_mesh_asset_probe_preview(resolved_path, &asset->preview);
    if (!runtime_mesh_asset_load_document_cached(resolved_path,
                                                asset_id,
                                                &asset->document,
                                                out_diagnostics,
                                                out_diagnostics_size)) {
        char message[256] = {0};
        snprintf(message,
                 sizeof(message),
                 "mesh asset '%s' invalid: %s",
                 asset_id,
                 (out_diagnostics && out_diagnostics[0]) ? out_diagnostics : "validation failed");
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, message);
        core_mesh_asset_runtime_document_free(&asset->document);
        memset(asset, 0, sizeof(*asset));
        return false;
    }
    if (strcmp(asset->document.contract.asset_id, asset_id) != 0) {
        char message[256] = {0};
        snprintf(message, sizeof(message), "mesh asset '%s' file asset_id mismatch", asset_id);
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, message);
        core_mesh_asset_runtime_document_free(&asset->document);
        memset(asset, 0, sizeof(*asset));
        return false;
    }
    if (out_asset_index) *out_asset_index = set->asset_count;
    set->asset_count += 1;
    return true;
}

void ray_tracing_runtime_mesh_asset_set_init(RayTracingRuntimeMeshAssetSet* set) {
    if (!set) return;
    memset(set, 0, sizeof(*set));
}

void ray_tracing_runtime_mesh_asset_set_free(RayTracingRuntimeMeshAssetSet* set) {
    int i = 0;
    if (!set) return;
    for (i = 0; i < set->asset_count; ++i) {
        core_mesh_asset_runtime_document_free(&set->assets[i].document);
        ProceduralSurfaceDerivedAssetMaterial_Free(
            &set->assets[i].procedural_material);
        free(set->assets[i].procedural_solid_composed_triangle_materials);
        ProceduralSolidMaterialRuntimeProgramV1_Free(
            &set->assets[i].procedural_solid_material_runtime_program);
        ProceduralImportedSurfaceRegionV1_Free(
            &set->assets[i].procedural_imported_surface_region);
        for (size_t selector = 0u;
             selector < set->assets[i].procedural_named_surface_selector_count;
             ++selector) {
            ProceduralImportedSurfaceRegionV1_Free(
                &set->assets[i].procedural_named_surface_selectors[selector]);
        }
    }
    memset(set, 0, sizeof(*set));
}

bool ray_tracing_runtime_mesh_asset_resolve_path(const char* runtime_scene_path,
                                                 const char* asset_id,
                                                 char* out_path,
                                                 size_t out_path_size,
                                                 char* out_diagnostics,
                                                 size_t out_diagnostics_size) {
    return ray_tracing_runtime_mesh_asset_resolve_path_with_hint(runtime_scene_path,
                                                                 asset_id,
                                                                 NULL,
                                                                 out_path,
                                                                 out_path_size,
                                                                 out_diagnostics,
                                                                 out_diagnostics_size);
}

static bool ray_tracing_runtime_mesh_asset_resolve_path_with_hint(const char* runtime_scene_path,
                                                                  const char* asset_id,
                                                                  const char* explicit_runtime_path,
                                                                  char* out_path,
                                                                  size_t out_path_size,
                                                                  char* out_diagnostics,
                                                                  size_t out_diagnostics_size) {
    char base_dir[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    char relative_path[256] = {0};
    CoreResult resolve_result = core_result_ok();

    if (out_path && out_path_size > 0u) out_path[0] = '\0';
    if (!runtime_scene_path || !runtime_scene_path[0] || !asset_id || !asset_id[0] ||
        !out_path || out_path_size == 0u) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "mesh asset resolve input missing");
        return false;
    }
    if (core_scene_dirname(runtime_scene_path, base_dir, sizeof(base_dir)).code != CORE_OK) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "runtime scene directory invalid");
        return false;
    }

    if (explicit_runtime_path && explicit_runtime_path[0]) {
        char resolved_hint[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
        const char* use_hint = explicit_runtime_path;
        if (explicit_runtime_path[0] != '/') {
            resolve_result = core_scene_resolve_path(base_dir,
                                                     explicit_runtime_path,
                                                     resolved_hint,
                                                     sizeof(resolved_hint));
            if (resolve_result.code == CORE_OK && resolved_hint[0]) {
                use_hint = resolved_hint;
            }
        }
        if (core_io_path_exists(use_hint)) {
            snprintf(out_path, out_path_size, "%s", use_hint);
            runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "ok");
            return true;
        }
    }

    snprintf(relative_path, sizeof(relative_path), "assets/mesh_assets/%s.runtime.json", asset_id);
    resolve_result = core_scene_resolve_path(base_dir, relative_path, out_path, out_path_size);
    if (resolve_result.code == CORE_OK && out_path[0] && core_io_path_exists(out_path)) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "ok");
        return true;
    }

    snprintf(relative_path, sizeof(relative_path), "mesh_assets/%s.runtime.json", asset_id);
    resolve_result = core_scene_resolve_path(base_dir, relative_path, out_path, out_path_size);
    if (resolve_result.code == CORE_OK && out_path[0] && core_io_path_exists(out_path)) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "ok");
        return true;
    }

    {
        const char* config_mesh_asset_root = animSettings.meshAssetRoot;
        const char* mesh_asset_root = getenv("RAY_TRACING_MESH_ASSET_ROOT");
        const char* input_root = getenv("RAY_TRACING_INPUT_ROOT");

        if (runtime_mesh_asset_try_asset_root(config_mesh_asset_root,
                                              asset_id,
                                              out_path,
                                              out_path_size,
                                              out_diagnostics,
                                              out_diagnostics_size)) {
            return true;
        }
        if (runtime_mesh_asset_try_asset_root(mesh_asset_root,
                                              asset_id,
                                              out_path,
                                              out_path_size,
                                              out_diagnostics,
                                              out_diagnostics_size)) {
            return true;
        }
        if (runtime_mesh_asset_try_asset_root(input_root,
                                              asset_id,
                                              out_path,
                                              out_path_size,
                                              out_diagnostics,
                                              out_diagnostics_size)) {
            return true;
        }
    }

    {
        char message[256] = {0};
        snprintf(message, sizeof(message), "mesh asset '%s' runtime file not found", asset_id);
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, message);
    }
    if (out_path && out_path_size > 0u) out_path[0] = '\0';
    return false;
}

static bool ray_tracing_runtime_mesh_assets_load_scene_file_with_options(
    const char* runtime_scene_path,
    size_t max_asset_file_bytes,
    RayTracingRuntimeMeshAssetSet* out_set,
    char* out_diagnostics,
    size_t out_diagnostics_size) {
    char* scene_text = NULL;
    json_object* root = NULL;
    json_object* objects = NULL;
    json_object* world_scale_obj = NULL;
    double world_scale = 1.0;
    int object_count = 0;
    int i = 0;
    int runtime_object_index = 0;
    struct timespec total_start = {0};
    struct timespec stage_start = {0};

    ray_tracing_runtime_mesh_assets_timing_reset();
    g_runtime_mesh_asset_timing.asset_persistent_cache_mode =
        runtime_mesh_asset_pack_cache_mode();
    (void)clock_gettime(CLOCK_MONOTONIC, &total_start);
    if (!out_set) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "mesh asset output missing");
        return false;
    }
    ray_tracing_runtime_mesh_asset_set_init(out_set);
    runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "ok");
    (void)clock_gettime(CLOCK_MONOTONIC, &stage_start);
    if (!runtime_mesh_asset_read_text(runtime_scene_path,
                                      &scene_text,
                                      out_diagnostics,
                                      out_diagnostics_size)) {
        return false;
    }
    g_runtime_mesh_asset_timing.scene_read_ms +=
        runtime_mesh_asset_elapsed_ms_since(&stage_start);

    (void)clock_gettime(CLOCK_MONOTONIC, &stage_start);
    root = json_tokener_parse(scene_text);
    free(scene_text);
    scene_text = NULL;
    g_runtime_mesh_asset_timing.scene_parse_ms +=
        runtime_mesh_asset_elapsed_ms_since(&stage_start);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "invalid runtime scene json");
        return false;
    }

    if (!json_object_object_get_ex(root, "objects", &objects) ||
        !json_object_is_type(objects, json_type_array)) {
        json_object_put(root);
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "runtime scene objects missing");
        return false;
    }

    object_count = (int)json_object_array_length(objects);
    if (json_object_object_get_ex(root, "world_scale", &world_scale_obj)) {
        world_scale = json_object_get_double(world_scale_obj);
    }
    for (i = 0; i < object_count; ++i) {
        json_object* object = json_object_array_get_idx(objects, (size_t)i);
        json_object* geometry_ref = NULL;
        const char* object_type = NULL;
        const char* object_id = NULL;
        const char* geometry_kind = NULL;
        const char* asset_id = NULL;
        const char* explicit_runtime_path = NULL;
        int asset_index = -1;
        bool asset_skipped = false;
        char resolved_asset_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
        size_t asset_file_size_bytes = 0u;

        if (!object || !json_object_is_type(object, json_type_object)) continue;
        object_type = runtime_mesh_asset_string_field(object, "object_type");
        if (runtime_mesh_asset_is_authoring_helper_object_type(object_type)) continue;
        if (!object_type || strcmp(object_type, "mesh_asset_instance") != 0) {
            runtime_object_index += 1;
            continue;
        }

        object_id = runtime_mesh_asset_string_field(object, "object_id");
        if (!object_id || !object_id[0]) {
            json_object_put(root);
            ray_tracing_runtime_mesh_asset_set_free(out_set);
            runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "mesh asset object_id missing");
            return false;
        }
        if (!json_object_object_get_ex(object, "geometry_ref", &geometry_ref) ||
            !json_object_is_type(geometry_ref, json_type_object)) {
            json_object_put(root);
            ray_tracing_runtime_mesh_asset_set_free(out_set);
            runtime_mesh_asset_diag(out_diagnostics,
                                    out_diagnostics_size,
                                    "mesh_asset_instance geometry_ref missing");
            return false;
        }
        geometry_kind = runtime_mesh_asset_string_field(geometry_ref, "kind");
        asset_id = runtime_mesh_asset_string_field(geometry_ref, "id");
        if (!geometry_kind || strcmp(geometry_kind, "mesh_asset") != 0) {
            json_object_put(root);
            ray_tracing_runtime_mesh_asset_set_free(out_set);
            runtime_mesh_asset_diag(out_diagnostics,
                                    out_diagnostics_size,
                                    "mesh_asset_instance geometry_ref.kind must be mesh_asset");
            return false;
        }
        if (!asset_id || !asset_id[0]) {
            json_object_put(root);
            ray_tracing_runtime_mesh_asset_set_free(out_set);
            runtime_mesh_asset_diag(out_diagnostics,
                                    out_diagnostics_size,
                                    "mesh_asset_instance geometry_ref.id missing");
            return false;
        }
        explicit_runtime_path = runtime_mesh_asset_object_runtime_path(object);

        if (!runtime_mesh_asset_load_unique_asset(runtime_scene_path,
                                                 asset_id,
                                                 explicit_runtime_path,
                                                 out_set,
                                                 &asset_index,
                                                 &asset_skipped,
                                                 resolved_asset_path,
                                                 sizeof(resolved_asset_path),
                                                 &asset_file_size_bytes,
                                                 max_asset_file_bytes,
                                                 out_diagnostics,
                                                 out_diagnostics_size)) {
            json_object_put(root);
            ray_tracing_runtime_mesh_asset_set_free(out_set);
            return false;
        }
        if (!asset_skipped &&
            !runtime_mesh_asset_load_procedural_surface_ref(
                runtime_scene_path, object, &out_set->assets[asset_index],
                out_diagnostics, out_diagnostics_size)) {
            json_object_put(root);
            ray_tracing_runtime_mesh_asset_set_free(out_set);
            return false;
        }
        if (!asset_skipped &&
            !runtime_mesh_asset_load_procedural_solid_material_ref(
                runtime_scene_path, object, &out_set->assets[asset_index],
                out_diagnostics, out_diagnostics_size)) {
            json_object_put(root);
            ray_tracing_runtime_mesh_asset_set_free(out_set);
            return false;
        }
        if (!asset_skipped &&
            !runtime_mesh_asset_load_procedural_solid_authored_material_ref(
                runtime_scene_path, object, &out_set->assets[asset_index],
                out_diagnostics, out_diagnostics_size)) {
            json_object_put(root);
            ray_tracing_runtime_mesh_asset_set_free(out_set);
            return false;
        }
        if (!asset_skipped &&
            !runtime_mesh_asset_append_instance(out_set,
                                                object_id,
                                                asset_id,
                                                asset_index,
                                                runtime_object_index,
                                                object,
                                                world_scale,
                                                out_diagnostics,
                                                out_diagnostics_size)) {
            json_object_put(root);
            ray_tracing_runtime_mesh_asset_set_free(out_set);
            return false;
        }
        if (asset_skipped) {
            if (out_set->skipped_instance_count <
                RAY_TRACING_RUNTIME_MESH_ASSET_MAX_INSTANCES) {
                RayTracingRuntimeMeshAssetSkippedInstance* skipped =
                    &out_set->skipped_instances[out_set->skipped_instance_count];
                memset(skipped, 0, sizeof(*skipped));
                runtime_mesh_asset_copy_id(skipped->object_id,
                                           sizeof(skipped->object_id),
                                           object_id,
                                           out_diagnostics,
                                           out_diagnostics_size);
                runtime_mesh_asset_copy_id(skipped->asset_id,
                                           sizeof(skipped->asset_id),
                                           asset_id,
                                           out_diagnostics,
                                           out_diagnostics_size);
                snprintf(skipped->path,
                         sizeof(skipped->path),
                         "%s",
                         resolved_asset_path);
                skipped->scene_object_index = runtime_object_index;
                skipped->file_size_bytes = asset_file_size_bytes;
                skipped->max_file_size_bytes = max_asset_file_bytes;
                runtime_mesh_asset_probe_preview(resolved_asset_path, &skipped->preview);
                memset(&skipped->preview_instance, 0, sizeof(skipped->preview_instance));
                runtime_mesh_asset_copy_id(skipped->preview_instance.object_id,
                                           sizeof(skipped->preview_instance.object_id),
                                           object_id,
                                           out_diagnostics,
                                           out_diagnostics_size);
                runtime_mesh_asset_copy_id(skipped->preview_instance.asset_id,
                                           sizeof(skipped->preview_instance.asset_id),
                                           asset_id,
                                           out_diagnostics,
                                           out_diagnostics_size);
                skipped->preview_instance.asset_index = -1;
                skipped->preview_instance.scene_object_index = runtime_object_index;
                runtime_mesh_asset_read_transform(object,
                                                  world_scale,
                                                  &skipped->preview_instance);
                out_set->skipped_instance_count += 1;
            }
        }
        runtime_object_index += 1;
    }

    json_object_put(root);
    g_runtime_mesh_asset_timing.loaded_assets = out_set->asset_count;
    g_runtime_mesh_asset_timing.loaded_instances = out_set->instance_count;
    for (i = 0; i < out_set->asset_count; ++i) {
        const RayTracingRuntimeMeshAsset* asset = &out_set->assets[i];
        struct stat st;
        if (stat(asset->path, &st) == 0 && st.st_size > 0) {
            g_runtime_mesh_asset_timing.loaded_asset_bytes +=
                (unsigned long long)st.st_size;
        }
        g_runtime_mesh_asset_timing.loaded_vertices +=
            (unsigned long long)asset->document.vertex_count;
        g_runtime_mesh_asset_timing.loaded_triangles +=
            (unsigned long long)asset->document.triangle_count;
        if (asset->procedural_surface_valid) {
            g_runtime_mesh_asset_timing.procedural_surface_assets += 1;
            g_runtime_mesh_asset_timing.procedural_surface_vertices +=
                (unsigned long long)asset->procedural_material.vertex_count;
            if (!g_runtime_mesh_asset_timing
                     .procedural_surface_cache_identity_sha256[0]) {
                snprintf(
                    g_runtime_mesh_asset_timing
                        .procedural_surface_cache_identity_sha256,
                    sizeof(g_runtime_mesh_asset_timing
                               .procedural_surface_cache_identity_sha256),
                    "%s",
                    asset->procedural_manifest.cache_identity_sha256);
                snprintf(
                    g_runtime_mesh_asset_timing
                        .procedural_surface_cage_digest_sha256,
                    sizeof(g_runtime_mesh_asset_timing
                               .procedural_surface_cage_digest_sha256),
                    "%s", asset->procedural_manifest.cage_digest_sha256);
                snprintf(
                    g_runtime_mesh_asset_timing
                        .procedural_surface_shell_digest_sha256,
                    sizeof(g_runtime_mesh_asset_timing
                               .procedural_surface_shell_digest_sha256),
                    "%s", asset->procedural_manifest.shell_digest_sha256);
                snprintf(
                    g_runtime_mesh_asset_timing
                        .procedural_surface_material_digest_sha256,
                    sizeof(g_runtime_mesh_asset_timing
                               .procedural_surface_material_digest_sha256),
                    "%s", asset->procedural_manifest.material_digest_sha256);
                snprintf(
                    g_runtime_mesh_asset_timing
                        .procedural_surface_collision_owner,
                    sizeof(g_runtime_mesh_asset_timing
                               .procedural_surface_collision_owner),
                    "%s", asset->procedural_manifest.collision_owner);
            }
        }
        if (asset->procedural_solid_material_valid) {
            char digest[
                PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY] = {0};
            ProceduralSolidMaterialBindingReport report;
            g_runtime_mesh_asset_timing.procedural_solid_material_assets += 1;
            g_runtime_mesh_asset_timing.procedural_solid_material_regions +=
                (unsigned long long)
                    asset->procedural_solid_material_binding.region_count;
            if (ProceduralSolidMaterialBindingV1_Digest(
                    &asset->procedural_solid_material_binding,
                    digest, &report) &&
                !g_runtime_mesh_asset_timing
                     .procedural_solid_material_binding_digest_sha256[0]) {
                snprintf(
                    g_runtime_mesh_asset_timing
                        .procedural_solid_material_binding_digest_sha256,
                    sizeof(g_runtime_mesh_asset_timing
                               .procedural_solid_material_binding_digest_sha256),
                    "%s", digest);
                snprintf(
                    g_runtime_mesh_asset_timing
                        .procedural_solid_material_mesh_digest_sha256,
                    sizeof(g_runtime_mesh_asset_timing
                               .procedural_solid_material_mesh_digest_sha256),
                    "%s",
                    asset->procedural_solid_material_binding
                        .mesh_digest_sha256);
                snprintf(
                    g_runtime_mesh_asset_timing
                        .procedural_solid_material_region_digest_sha256,
                    sizeof(g_runtime_mesh_asset_timing
                               .procedural_solid_material_region_digest_sha256),
                    "%s",
                    asset->procedural_solid_material_binding
                        .region_digest_sha256);
            }
        }
        if (asset->procedural_solid_authored_material_valid) {
            char digest[
                PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY] = {0};
            ProceduralSolidAuthoredBindingReport report;
            g_runtime_mesh_asset_timing
                .procedural_solid_authored_material_assets += 1;
            g_runtime_mesh_asset_timing
                .procedural_solid_authored_material_regions +=
                    (unsigned long long)
                        asset->procedural_solid_authored_binding
                            .assignment_count;
            if (ProceduralSolidAuthoredMaterialBindingV1_Digest(
                    &asset->procedural_solid_authored_binding,
                    digest, &report) &&
                !g_runtime_mesh_asset_timing
                     .procedural_solid_authored_binding_digest_sha256[0]) {
                snprintf(
                    g_runtime_mesh_asset_timing
                        .procedural_solid_authored_binding_digest_sha256,
                    sizeof(g_runtime_mesh_asset_timing
                               .procedural_solid_authored_binding_digest_sha256),
                    "%s", digest);
            }
        }
    }
    g_runtime_mesh_asset_timing.total_ms += runtime_mesh_asset_elapsed_ms_since(&total_start);
    runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

bool ray_tracing_runtime_mesh_assets_load_scene_file(const char* runtime_scene_path,
                                                     RayTracingRuntimeMeshAssetSet* out_set,
                                                     char* out_diagnostics,
                                                     size_t out_diagnostics_size) {
    return ray_tracing_runtime_mesh_assets_load_scene_file_with_options(runtime_scene_path,
                                                                        0u,
                                                                        out_set,
                                                                        out_diagnostics,
                                                                        out_diagnostics_size);
}

bool ray_tracing_runtime_mesh_assets_load_scene_file_preview_limited(
    const char* runtime_scene_path,
    size_t max_asset_file_bytes,
    RayTracingRuntimeMeshAssetSet* out_set,
    char* out_diagnostics,
    size_t out_diagnostics_size) {
    return ray_tracing_runtime_mesh_assets_load_scene_file_with_options(runtime_scene_path,
                                                                        max_asset_file_bytes,
                                                                        out_set,
                                                                        out_diagnostics,
                                                                        out_diagnostics_size);
}

void ray_tracing_runtime_mesh_assets_reset_last(void) {
    ray_tracing_runtime_mesh_asset_set_free(&g_last_runtime_mesh_assets);
    runtime_mesh_asset_clear_last_scene_stamp();
}

void ray_tracing_runtime_mesh_assets_take_last(RayTracingRuntimeMeshAssetSet* loaded) {
    ray_tracing_runtime_mesh_assets_take_last_for_scene(NULL, loaded);
}

void ray_tracing_runtime_mesh_assets_take_last_for_scene(const char* runtime_scene_path,
                                                        RayTracingRuntimeMeshAssetSet* loaded) {
    ray_tracing_runtime_mesh_assets_reset_last();
    if (!loaded) return;
    g_last_runtime_mesh_assets = *loaded;
    memset(loaded, 0, sizeof(*loaded));
    runtime_mesh_asset_capture_last_scene_stamp(runtime_scene_path);
}

bool ray_tracing_runtime_mesh_assets_last_matches_scene_file(const char* runtime_scene_path) {
    if (!runtime_scene_path || !runtime_scene_path[0]) return false;
    if (!g_last_runtime_mesh_asset_scene_stamp_valid) return false;
    if (strcmp(g_last_runtime_mesh_asset_scene_path, runtime_scene_path) != 0) return false;
    if (!runtime_mesh_asset_stamp_matches_path(runtime_scene_path,
                                              g_last_runtime_mesh_asset_scene_mtime_sec,
                                              g_last_runtime_mesh_asset_scene_mtime_nsec,
                                              g_last_runtime_mesh_asset_scene_file_size)) {
        return false;
    }
    for (int i = 0; i < g_last_runtime_mesh_assets.asset_count; ++i) {
        const RayTracingRuntimeMeshAsset* asset = &g_last_runtime_mesh_assets.assets[i];
        if (!asset->file_stamp_valid) return false;
        if (!runtime_mesh_asset_stamp_matches_path(asset->path,
                                                  asset->file_mtime_sec,
                                                  asset->file_mtime_nsec,
                                                  asset->file_size_bytes)) {
            return false;
        }
        if (asset->procedural_surface_valid &&
            (!runtime_mesh_asset_dependency_matches(
                 &asset->procedural_manifest_dependency) ||
             !runtime_mesh_asset_dependency_matches(
                 &asset->procedural_recipe_dependency) ||
             (asset->procedural_manifest.schema_version >= 2u &&
              (!runtime_mesh_asset_dependency_matches(
                   &asset->procedural_field_graph_dependency) ||
               !runtime_mesh_asset_dependency_matches(
                   &asset->procedural_binding_dependency))) ||
             !runtime_mesh_asset_dependency_matches(
                 &asset->procedural_material_dependency))) {
            return false;
        }
        if (asset->procedural_solid_material_valid &&
            !runtime_mesh_asset_dependency_matches(
                &asset->procedural_solid_material_binding_dependency)) {
            return false;
        }
        if (!runtime_mesh_asset_procedural_solid_authored_dependencies_match(
                asset)) {
            return false;
        }
    }
    return true;
}

bool ray_tracing_runtime_mesh_assets_load_scene_file_to_last(const char* runtime_scene_path,
                                                            char* out_diagnostics,
                                                            size_t out_diagnostics_size) {
    RayTracingRuntimeMeshAssetSet loaded;
    ray_tracing_runtime_mesh_asset_set_init(&loaded);
    if (!ray_tracing_runtime_mesh_assets_load_scene_file(runtime_scene_path,
                                                        &loaded,
                                                        out_diagnostics,
                                                        out_diagnostics_size)) {
        return false;
    }
    ray_tracing_runtime_mesh_assets_take_last_for_scene(runtime_scene_path, &loaded);
    return true;
}

const RayTracingRuntimeMeshAssetSet* ray_tracing_runtime_mesh_assets_last(void) {
    return &g_last_runtime_mesh_assets;
}
