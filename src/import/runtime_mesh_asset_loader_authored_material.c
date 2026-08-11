#include "import/runtime_mesh_asset_loader_authored_material.h"

#include "core_io.h"
#include "core_scene.h"
#include "procedural/procedural_solid_mesh.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static long long stat_mtime_nsec(const struct stat *status) {
    if (!status) return 0;
#if defined(__APPLE__)
    return (long long)status->st_mtimespec.tv_nsec;
#elif defined(__linux__)
    return (long long)status->st_mtim.tv_nsec;
#else
    return 0;
#endif
}

static void set_diag(char *out, size_t capacity, const char *message) {
    if (!out || capacity == 0u) return;
    snprintf(out, capacity, "%s", message ? message : "");
}

static const char *string_field(json_object *object, const char *key) {
    json_object *value = NULL;
    if (!object || !json_object_object_get_ex(object, key, &value) ||
        json_object_get_type(value) != json_type_string) return NULL;
    return json_object_get_string(value);
}

static bool resolve_relative(
    const char *owner_path,
    const char *reference,
    char *out,
    size_t capacity) {
    char directory[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    if (!owner_path || !reference || !out || capacity == 0u) return false;
    if (reference[0] == '/') {
        if (strlen(reference) >= capacity || !core_io_path_exists(reference)) {
            return false;
        }
        snprintf(out, capacity, "%s", reference);
        return true;
    }
    if (core_scene_dirname(owner_path, directory, sizeof(directory)).code !=
            CORE_OK ||
        core_scene_resolve_path(directory, reference, out, capacity).code !=
            CORE_OK ||
        !core_io_path_exists(out)) return false;
    return true;
}

static bool capture_dependency(
    const char *path,
    RayTracingRuntimeMeshAssetFileDependency *dependency) {
    struct stat status;
    if (!path || !dependency || stat(path, &status) != 0) return false;
    memset(dependency, 0, sizeof(*dependency));
    snprintf(dependency->path, sizeof(dependency->path), "%s", path);
    dependency->stamp_valid = true;
    dependency->mtime_sec = (long long)status.st_mtime;
    dependency->mtime_nsec = stat_mtime_nsec(&status);
    dependency->size_bytes = (long long)status.st_size;
    return true;
}

static bool dependency_matches(
    const RayTracingRuntimeMeshAssetFileDependency *dependency) {
    struct stat status;
    if (!dependency || !dependency->stamp_valid ||
        stat(dependency->path, &status) != 0) return false;
    return dependency->mtime_sec == (long long)status.st_mtime &&
           dependency->mtime_nsec == stat_mtime_nsec(&status) &&
           dependency->size_bytes == (long long)status.st_size;
}

static bool load_materials(
    const char *authored_binding_path,
    const ProceduralSolidAuthoredMaterialBindingV1 *binding,
    ProceduralSolidAuthoredMaterialV1 materials[PROCEDURAL_SOLID_REGION_MAX],
    RayTracingRuntimeMeshAssetFileDependency
        dependencies[PROCEDURAL_SOLID_REGION_MAX],
    char *out_diagnostics,
    size_t out_diagnostics_size) {
    for (size_t i = 0u; i < binding->assignment_count; ++i) {
        const ProceduralSolidAuthoredMaterialReferenceV1 *ref =
            &binding->assignments[i];
        ProceduralSolidAuthoredMaterialReport report;
        char material_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
        char digest[PROCEDURAL_SOLID_AUTHORED_MATERIAL_DIGEST_CAPACITY] = {0};
        if (!resolve_relative(
                authored_binding_path, ref->material_path,
                material_path, sizeof(material_path)) ||
            !ProceduralSolidAuthoredMaterialV1_LoadJsonFile(
                material_path, &materials[i], &report) ||
            !ProceduralSolidAuthoredMaterialV1_Digest(
                &materials[i], digest, &report) ||
            strcmp(materials[i].material_id, ref->material_id) != 0 ||
            strcmp(digest, ref->material_digest_sha256) != 0 ||
            !capture_dependency(material_path, &dependencies[i])) {
            char message[320] = {0};
            snprintf(message, sizeof(message),
                     "procedural solid authored material invalid: %s",
                     ref->material_path);
            set_diag(out_diagnostics, out_diagnostics_size, message);
            return false;
        }
    }
    return true;
}

static const char *region_kind_for_triangle(
    const RayTracingRuntimeMeshAsset *asset, size_t triangle_index) {
    const char *region_id;
    if (!asset || triangle_index >= asset->document.triangle_count)
        return "retained";
    region_id = asset->document.triangles[triangle_index].surface_group_id;
    for (size_t i = 0u;
         i < asset->procedural_solid_material_binding.region_count; ++i) {
        const ProceduralSolidRegionRecord *record =
            &asset->procedural_solid_material_binding.regions[i];
        if (strcmp(record->region_id, region_id) == 0)
            return ProceduralSolidRegionKind_Name(record->kind);
    }
    return "retained";
}

static void free_named_surface_selectors(RayTracingRuntimeMeshAsset *asset) {
    if (!asset) return;
    for (size_t i = 0u;
         i < asset->procedural_named_surface_selector_count; ++i) {
        ProceduralImportedSurfaceRegionV1_Free(
            &asset->procedural_named_surface_selectors[i]);
    }
    asset->procedural_named_surface_selector_count = 0u;
    memset(asset->procedural_named_surface_selector_names, 0,
           sizeof(asset->procedural_named_surface_selector_names));
    memset(asset->procedural_named_surface_selector_paths, 0,
           sizeof(asset->procedural_named_surface_selector_paths));
    memset(asset->procedural_named_surface_selector_dependencies, 0,
           sizeof(asset->procedural_named_surface_selector_dependencies));
}

static bool load_named_surface_selectors(
    const char *runtime_scene_path, json_object *reference,
    RayTracingRuntimeMeshAsset *asset, char *out_diagnostics,
    size_t out_diagnostics_size) {
    json_object *values = NULL;
    size_t count;
    if (!json_object_object_get_ex(
            reference, "named_surface_selectors", &values)) {
        if (asset->procedural_named_surface_selectors_observed) {
            set_diag(out_diagnostics, out_diagnostics_size,
                     "named surface selector references must be consistent "
                     "across instances of one mesh asset");
            return false;
        }
        asset->procedural_named_surface_selectors_absent = true;
        return true;
    }
    if (asset->procedural_named_surface_selectors_absent ||
        json_object_get_type(values) != json_type_array ||
        json_object_array_length(values) == 0u ||
        json_object_array_length(values) >
            RAY_TRACING_RUNTIME_MESH_ASSET_MAX_NAMED_SURFACE_SELECTORS) {
        set_diag(out_diagnostics, out_diagnostics_size,
                 "named_surface_selectors must be a bounded non-empty array");
        return false;
    }
    count = json_object_array_length(values);
    if (asset->procedural_named_surface_selectors_observed) {
        if (count != asset->procedural_named_surface_selector_count) {
            set_diag(out_diagnostics, out_diagnostics_size,
                     "named surface selector instance count mismatch");
            return false;
        }
        for (size_t i = 0u; i < count; ++i) {
            json_object *item = json_object_array_get_idx(values, i);
            const char *name = string_field(item, "name");
            const char *path = string_field(item, "surface_region_path");
            char resolved[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
            if (!name || !path || !resolve_relative(runtime_scene_path, path,
                                                    resolved, sizeof(resolved)) ||
                strcmp(name, asset->procedural_named_surface_selector_names[i]) != 0 ||
                strcmp(resolved, asset->procedural_named_surface_selector_paths[i]) != 0) {
                set_diag(out_diagnostics, out_diagnostics_size,
                         "named surface selector instance mismatch");
                return false;
            }
        }
        return true;
    }
    free_named_surface_selectors(asset);
    for (size_t i = 0u; i < count; ++i) {
        json_object *item = json_object_array_get_idx(values, i);
        const char *name = string_field(item, "name");
        const char *path = string_field(item, "surface_region_path");
        char resolved[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
        ProceduralImportedSurfaceRegionReport report = {0};
        if (!item || json_object_get_type(item) != json_type_object || !name ||
            !name[0] || strlen(name) >=
                PROCEDURAL_SOLID_MATERIAL_GRAPH_ID_CAPACITY || !path ||
            !resolve_relative(runtime_scene_path, path, resolved,
                              sizeof(resolved)) ||
            !ProceduralImportedSurfaceRegionV1_LoadJsonFile(
                resolved, &asset->document, asset->path,
                &asset->procedural_named_surface_selectors[i], &report) ||
            !capture_dependency(
                resolved,
                &asset->procedural_named_surface_selector_dependencies[i])) {
            set_diag(out_diagnostics, out_diagnostics_size,
                     report.message[0] ? report.message :
                     "named surface selector identity is stale");
            return false;
        }
        for (size_t prior = 0u; prior < i; ++prior) {
            if (strcmp(name, asset->procedural_named_surface_selector_names[prior]) == 0) {
                set_diag(out_diagnostics, out_diagnostics_size,
                         "named surface selector names must be unique");
                return false;
            }
        }
        snprintf(asset->procedural_named_surface_selector_names[i],
                 sizeof(asset->procedural_named_surface_selector_names[i]),
                 "%s", name);
        snprintf(asset->procedural_named_surface_selector_paths[i],
                 sizeof(asset->procedural_named_surface_selector_paths[i]),
                 "%s", resolved);
        asset->procedural_named_surface_selector_count = i + 1u;
    }
    asset->procedural_named_surface_selectors_observed = true;
    return true;
}

static bool load_material_graph(
    const char *runtime_scene_path,
    json_object *reference,
    RayTracingRuntimeMeshAsset *asset,
    const char *authored_binding_digest,
    char *out_diagnostics,
    size_t out_diagnostics_size) {
    const char *graph_reference = string_field(reference, "graph_path");
    const char *surface_region_reference =
        string_field(reference, "surface_region_path");
    const char *feature_field_reference =
        string_field(reference, "surface_feature_field_path");
    const char *feature_curve_field_reference =
        string_field(reference, "surface_feature_curve_field_path");
    const char *wood_grain_reference =
        string_field(reference, "wood_grain_field_path");
    const char *wood_grain_preset_digest =
        string_field(reference, "wood_grain_preset_digest_sha256");
    ProceduralSolidMaterialGraphV1 graph;
    ProceduralSolidMaterialGraphReport graph_report = {0};
    ProceduralSolidAuthoredMaterialV1
        materials[PROCEDURAL_SOLID_MATERIAL_GRAPH_MAX_LAYERS];
    RayTracingRuntimeMeshAssetFileDependency
        dependencies[PROCEDURAL_SOLID_MATERIAL_GRAPH_MAX_LAYERS];
    ProceduralSolidMaterialGeometryInputs *inputs = NULL;
    ProceduralSolidAuthoredMaterialSurfaceV1 *surfaces = NULL;
    const char **region_kinds = NULL;
    char graph_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    char graph_digest[PROCEDURAL_SOLID_MATERIAL_GRAPH_DIGEST_CAPACITY] = {0};
    char surface_region_path[
        RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    const ProceduralImportedSurfaceRegionV1 *surface_region = NULL;
    char feature_field_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    char feature_curve_field_path[
        RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    char wood_grain_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    char mesh_digest[PROCEDURAL_SURFACE_FEATURE_FIELD_DIGEST_CAPACITY] = {0};
    memset(materials, 0, sizeof(materials));
    memset(dependencies, 0, sizeof(dependencies));
    if (!graph_reference) {
        if (asset->procedural_solid_material_graph_observed) {
            set_diag(out_diagnostics, out_diagnostics_size,
                     "material graph references must be consistent across "
                     "instances of one mesh asset");
            return false;
        }
        asset->procedural_solid_material_graph_absent = true;
        return true;
    }
    if (asset->procedural_solid_material_graph_absent) {
        set_diag(out_diagnostics, out_diagnostics_size,
                 "material graph references must be consistent across "
                 "instances of one mesh asset");
        return false;
    }
    asset->procedural_solid_material_graph_observed = true;
    if (!resolve_relative(runtime_scene_path, graph_reference,
                          graph_path, sizeof(graph_path)) ||
        !ProceduralSolidMaterialGraphV1_LoadJsonFile(
            graph_path, &graph, &graph_report) ||
        strcmp(graph.authored_binding_id,
               asset->procedural_solid_authored_binding.binding_id) != 0 ||
        strcmp(graph.authored_binding_digest_sha256,
               authored_binding_digest) != 0 ||
        !ProceduralSolidMaterialGraphV1_Digest(
            &graph, graph_digest, &graph_report)) {
        set_diag(out_diagnostics, out_diagnostics_size,
                 "procedural solid material graph identity is stale");
        return false;
    }
    if (asset->procedural_solid_material_graph_valid) {
        char existing[PROCEDURAL_SOLID_MATERIAL_GRAPH_DIGEST_CAPACITY] = {0};
        if (!ProceduralSolidMaterialGraphV1_Digest(
                &asset->procedural_solid_material_graph,
                existing, &graph_report) ||
            strcmp(existing, graph_digest) != 0) {
            set_diag(out_diagnostics, out_diagnostics_size,
                     "procedural solid material graph instance mismatch");
            return false;
        }
        if ((surface_region_reference != NULL) !=
            asset->procedural_imported_surface_region_valid) {
            set_diag(out_diagnostics, out_diagnostics_size,
                     "imported surface region references must be consistent "
                     "across instances of one mesh asset");
            return false;
        }
        if (surface_region_reference &&
            (!resolve_relative(runtime_scene_path, surface_region_reference,
                               surface_region_path,
                               sizeof(surface_region_path)) ||
             strcmp(surface_region_path,
                    asset->procedural_imported_surface_region_path) != 0)) {
            set_diag(out_diagnostics, out_diagnostics_size,
                     "imported surface region instance mismatch");
            return false;
        }
        if ((feature_field_reference != NULL) !=
            asset->procedural_surface_feature_field_valid) {
            set_diag(out_diagnostics, out_diagnostics_size,
                     "surface feature field references must be consistent "
                     "across instances of one mesh asset");
            return false;
        }
        if (feature_field_reference &&
            (!resolve_relative(runtime_scene_path, feature_field_reference,
                               feature_field_path, sizeof(feature_field_path)) ||
             strcmp(feature_field_path,
                    asset->procedural_surface_feature_field_path) != 0)) {
            set_diag(out_diagnostics, out_diagnostics_size,
                     "surface feature field instance mismatch");
            return false;
        }
        if ((feature_curve_field_reference != NULL) !=
            asset->procedural_surface_feature_curve_field_valid) {
            set_diag(out_diagnostics, out_diagnostics_size,
                     "surface feature curve field references must be "
                     "consistent across instances of one mesh asset");
            return false;
        }
        if (feature_curve_field_reference &&
            (!resolve_relative(runtime_scene_path,
                               feature_curve_field_reference,
                               feature_curve_field_path,
                               sizeof(feature_curve_field_path)) ||
             strcmp(feature_curve_field_path,
                    asset->procedural_surface_feature_curve_field_path) != 0)) {
            set_diag(out_diagnostics, out_diagnostics_size,
                     "surface feature curve field instance mismatch");
            return false;
        }
        if ((wood_grain_reference != NULL) !=
            asset->procedural_surface_wood_grain_valid) {
            set_diag(out_diagnostics, out_diagnostics_size,
                     "wood grain references must be consistent across instances of one mesh asset");
            return false;
        }
        if (wood_grain_reference &&
            (!wood_grain_preset_digest ||
             !resolve_relative(runtime_scene_path, wood_grain_reference,
                               wood_grain_path, sizeof(wood_grain_path)) ||
             strcmp(wood_grain_path,
                    asset->procedural_surface_wood_grain_path) != 0 ||
             strcmp(wood_grain_preset_digest,
                    asset->procedural_surface_wood_grain.preset_digest_sha256) != 0)) {
            set_diag(out_diagnostics, out_diagnostics_size,
                     "wood grain field instance mismatch");
            return false;
        }
        return true;
    }
    if (surface_region_reference) {
        ProceduralImportedSurfaceRegionReport region_report = {0};
        if (asset->procedural_imported_surface_region_absent ||
            !resolve_relative(runtime_scene_path, surface_region_reference,
                              surface_region_path,
                              sizeof(surface_region_path)) ||
            !ProceduralImportedSurfaceRegionV1_LoadJsonFile(
                surface_region_path, &asset->document, asset->path,
                &asset->procedural_imported_surface_region, &region_report) ||
            !capture_dependency(
                surface_region_path,
                &asset->procedural_imported_surface_region_dependency)) {
            set_diag(out_diagnostics, out_diagnostics_size,
                     region_report.message[0]
                         ? region_report.message
                         : "imported surface region identity is stale");
            return false;
        }
        asset->procedural_imported_surface_region_observed = true;
        asset->procedural_imported_surface_region_valid = true;
        snprintf(asset->procedural_imported_surface_region_path,
                 sizeof(asset->procedural_imported_surface_region_path),
                 "%s", surface_region_path);
        surface_region = &asset->procedural_imported_surface_region;
    } else {
        if (asset->procedural_imported_surface_region_observed) {
            set_diag(out_diagnostics, out_diagnostics_size,
                     "imported surface region references must be consistent "
                     "across instances of one mesh asset");
            return false;
        }
        asset->procedural_imported_surface_region_absent = true;
    }
    if (!load_named_surface_selectors(
            runtime_scene_path, reference, asset, out_diagnostics,
            out_diagnostics_size)) goto fail;
    if (feature_field_reference) {
        if (asset->procedural_surface_feature_field_absent ||
            !resolve_relative(runtime_scene_path, feature_field_reference,
                              feature_field_path, sizeof(feature_field_path)) ||
            !ProceduralSolidMesh_Digest(&asset->document, mesh_digest) ||
            !ProceduralSurfaceFeatureFieldV1_LoadJsonFile(
                feature_field_path, &asset->procedural_surface_feature_field) ||
            strcmp(mesh_digest, asset->procedural_surface_feature_field.
                source_mesh_digest_sha256) != 0 ||
            !capture_dependency(feature_field_path,
                &asset->procedural_surface_feature_field_dependency)) {
            set_diag(out_diagnostics, out_diagnostics_size,
                     "surface feature field identity is stale");
            goto fail;
        }
        asset->procedural_surface_feature_field_observed = true;
        asset->procedural_surface_feature_field_valid = true;
        snprintf(asset->procedural_surface_feature_field_path,
                 sizeof(asset->procedural_surface_feature_field_path), "%s",
                 feature_field_path);
    } else {
        if (asset->procedural_surface_feature_field_observed) {
            set_diag(out_diagnostics, out_diagnostics_size,
                     "surface feature field references must be consistent "
                     "across instances of one mesh asset");
            goto fail;
        }
        asset->procedural_surface_feature_field_absent = true;
    }
    if (wood_grain_reference) {
        if (asset->procedural_surface_wood_grain_absent ||
            !wood_grain_preset_digest ||
            !resolve_relative(runtime_scene_path, wood_grain_reference,
                              wood_grain_path, sizeof(wood_grain_path)) ||
            !ProceduralSolidMesh_Digest(&asset->document, mesh_digest) ||
            !ProceduralSurfaceWoodGrainFieldV1_LoadJsonFile(
                wood_grain_path, &asset->procedural_surface_wood_grain) ||
            strcmp(mesh_digest, asset->procedural_surface_wood_grain.
                source_mesh_digest_sha256) != 0 ||
            strcmp(wood_grain_preset_digest, asset->procedural_surface_wood_grain.
                preset_digest_sha256) != 0 ||
            !capture_dependency(wood_grain_path,
                &asset->procedural_surface_wood_grain_dependency)) {
            set_diag(out_diagnostics, out_diagnostics_size,
                     "wood grain field identity is stale");
            goto fail;
        }
        asset->procedural_surface_wood_grain_observed = true;
        asset->procedural_surface_wood_grain_valid = true;
        snprintf(asset->procedural_surface_wood_grain_path,
                 sizeof(asset->procedural_surface_wood_grain_path), "%s",
                 wood_grain_path);
    } else {
        if (asset->procedural_surface_wood_grain_observed) {
            set_diag(out_diagnostics, out_diagnostics_size,
                     "wood grain references must be consistent across instances of one mesh asset");
            goto fail;
        }
        asset->procedural_surface_wood_grain_absent = true;
    }
    if (feature_curve_field_reference) {
        if (asset->procedural_surface_feature_curve_field_absent ||
            !resolve_relative(runtime_scene_path,
                              feature_curve_field_reference,
                              feature_curve_field_path,
                              sizeof(feature_curve_field_path)) ||
            !ProceduralSolidMesh_Digest(&asset->document, mesh_digest) ||
            !ProceduralSurfaceFeatureCurveFieldV1_LoadJsonFile(
                feature_curve_field_path,
                &asset->procedural_surface_feature_curve_field) ||
            strcmp(mesh_digest,
                   asset->procedural_surface_feature_curve_field.
                       source_mesh_digest_sha256) != 0 ||
            !capture_dependency(
                feature_curve_field_path,
                &asset->procedural_surface_feature_curve_field_dependency)) {
            set_diag(out_diagnostics, out_diagnostics_size,
                     "surface feature curve field identity is stale");
            goto fail;
        }
        asset->procedural_surface_feature_curve_field_observed = true;
        asset->procedural_surface_feature_curve_field_valid = true;
        snprintf(asset->procedural_surface_feature_curve_field_path,
                 sizeof(asset->procedural_surface_feature_curve_field_path),
                 "%s", feature_curve_field_path);
    } else {
        if (asset->procedural_surface_feature_curve_field_observed) {
            set_diag(out_diagnostics, out_diagnostics_size,
                     "surface feature curve field references must be "
                     "consistent across instances of one mesh asset");
            goto fail;
        }
        asset->procedural_surface_feature_curve_field_absent = true;
    }
    for (size_t i = 0u; i < graph.layer_count; ++i) {
        char material_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
        char digest[PROCEDURAL_SOLID_AUTHORED_MATERIAL_DIGEST_CAPACITY] = {0};
        ProceduralSolidAuthoredMaterialReport material_report;
        if (!graph.layers[i].material_path[0] ||
            strlen(graph.layers[i].material_digest_sha256) != 64u ||
            !resolve_relative(graph_path, graph.layers[i].material_path,
                              material_path, sizeof(material_path)) ||
            !ProceduralSolidAuthoredMaterialV1_LoadJsonFile(
                material_path, &materials[i], &material_report) ||
            strcmp(materials[i].material_id,
                   graph.layers[i].material_id) != 0 ||
            !ProceduralSolidAuthoredMaterialV1_Digest(
                &materials[i], digest, &material_report) ||
            strcmp(digest, graph.layers[i].material_digest_sha256) != 0 ||
            !capture_dependency(material_path, &dependencies[i])) {
            set_diag(out_diagnostics, out_diagnostics_size,
                     "procedural solid graph material reference is stale");
            return false;
        }
    }
    inputs = calloc(asset->document.triangle_count, sizeof(*inputs));
    surfaces = calloc(asset->document.triangle_count, sizeof(*surfaces));
    region_kinds = calloc(asset->document.triangle_count,
                          sizeof(*region_kinds));
    if ((!inputs || !surfaces || !region_kinds) &&
        asset->document.triangle_count > 0u) goto fail;
    for (size_t i = 0u; i < asset->document.triangle_count; ++i)
        region_kinds[i] = region_kind_for_triangle(asset, i);
    if (!ProceduralSolidMaterialGeometryInputs_Build(
            &asset->document, region_kinds, inputs,
            asset->document.triangle_count, &graph_report)) goto fail;
    for (size_t i = 0u; i < asset->document.triangle_count; ++i) {
        if (!ProceduralSolidMaterialGraphV1_Evaluate(
                &graph, &inputs[i], materials, graph.layer_count,
                &surfaces[i], &graph_report)) goto fail;
    }
    if (!ProceduralSolidMaterialRuntimeProgramV1_BuildWithImportedRegion(
            &graph, materials, graph.layer_count, &asset->document,
            region_kinds, surface_region,
            &asset->procedural_solid_material_runtime_program,
            &graph_report)) {
        goto fail;
    }
    for (size_t i = 0u;
         i < asset->procedural_named_surface_selector_count; ++i) {
        if (!ProceduralSolidMaterialRuntimeProgramV1_AttachNamedSelector(
                &asset->procedural_solid_material_runtime_program,
                asset->procedural_named_surface_selector_names[i],
                &asset->procedural_named_surface_selectors[i])) goto fail;
    }
    if (asset->procedural_surface_feature_field_valid &&
        !ProceduralSolidMaterialRuntimeProgramV1_AttachFeatureField(
            &asset->procedural_solid_material_runtime_program,
            &asset->procedural_surface_feature_field)) goto fail;
    if (asset->procedural_surface_feature_curve_field_valid &&
        !ProceduralSolidMaterialRuntimeProgramV1_AttachCurveField(
            &asset->procedural_solid_material_runtime_program,
            &asset->procedural_surface_feature_curve_field)) goto fail;
    if (asset->procedural_surface_wood_grain_valid &&
        !ProceduralSolidMaterialRuntimeProgramV1_AttachWoodGrain(
            &asset->procedural_solid_material_runtime_program,
            &asset->procedural_surface_wood_grain)) goto fail;
    if (!capture_dependency(
            graph_path, &asset->procedural_solid_material_graph_dependency))
        goto fail;
    asset->procedural_solid_material_graph_valid = true;
    snprintf(asset->procedural_solid_material_graph_path,
             sizeof(asset->procedural_solid_material_graph_path), "%s",
             graph_path);
    asset->procedural_solid_material_graph = graph;
    asset->procedural_solid_material_graph_material_count = graph.layer_count;
    memcpy(asset->procedural_solid_material_graph_materials, materials,
           sizeof(materials));
    memcpy(asset->procedural_solid_material_graph_material_dependencies,
           dependencies, sizeof(dependencies));
    asset->procedural_solid_composed_triangle_materials = surfaces;
    asset->procedural_solid_composed_triangle_material_count =
        asset->document.triangle_count;
    free(inputs);
    free(region_kinds);
    return true;
fail:
    ProceduralSolidMaterialRuntimeProgramV1_Free(
        &asset->procedural_solid_material_runtime_program);
    free_named_surface_selectors(asset);
    free(inputs);
    free(region_kinds);
    free(surfaces);
    set_diag(out_diagnostics, out_diagnostics_size,
             graph_report.message[0] ? graph_report.message
                                     : "material graph compile failed");
    return false;
}

bool runtime_mesh_asset_load_procedural_solid_authored_material_ref(
    const char *runtime_scene_path,
    json_object *object,
    RayTracingRuntimeMeshAsset *asset,
    char *out_diagnostics,
    size_t out_diagnostics_size) {
    json_object *reference = NULL;
    const char *authored_reference = NULL;
    char binding_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    char digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY] = {0};
    ProceduralSolidAuthoredMaterialBindingV1 binding;
    ProceduralSolidAuthoredBindingReport report;
    ProceduralSolidAuthoredMaterialV1
        materials[PROCEDURAL_SOLID_REGION_MAX];
    RayTracingRuntimeMeshAssetFileDependency
        dependencies[PROCEDURAL_SOLID_REGION_MAX];
    memset(materials, 0, sizeof(materials));
    memset(dependencies, 0, sizeof(dependencies));
    if (!runtime_scene_path || !object || !asset) return false;
    if (!json_object_object_get_ex(
            object, "procedural_solid_material_ref", &reference) ||
        json_object_get_type(reference) != json_type_object ||
        !(authored_reference =
              string_field(reference, "authored_binding_path"))) {
        if (asset->procedural_solid_authored_reference_observed) {
            set_diag(out_diagnostics, out_diagnostics_size,
                     "procedural solid authored references must be consistent "
                     "across instances of one mesh asset");
            return false;
        }
        asset->procedural_solid_authored_reference_absent = true;
        return true;
    }
    if (asset->procedural_solid_authored_reference_absent ||
        !asset->procedural_solid_material_valid) {
        set_diag(out_diagnostics, out_diagnostics_size,
                 "authored material reference requires one consistent "
                 "procedural solid region binding");
        return false;
    }
    asset->procedural_solid_authored_reference_observed = true;
    if (!resolve_relative(
            runtime_scene_path, authored_reference,
            binding_path, sizeof(binding_path)) ||
        !ProceduralSolidAuthoredMaterialBindingV1_LoadJsonFile(
            binding_path, &binding, &report) ||
        !ProceduralSolidAuthoredMaterialBindingV1_Validate(
            &binding, &asset->procedural_solid_material_binding, &report) ||
        !ProceduralSolidAuthoredMaterialBindingV1_Digest(
            &binding, digest, &report) ||
        !load_materials(
            binding_path, &binding, materials, dependencies,
            out_diagnostics, out_diagnostics_size)) {
        if (!out_diagnostics || !out_diagnostics[0]) {
            char message[320] = {0};
            snprintf(message, sizeof(message),
                     "procedural solid authored binding invalid: %s",
                     report.message[0] ? report.message
                                       : "path resolution failed");
            set_diag(out_diagnostics, out_diagnostics_size, message);
        }
        return false;
    }
    if (asset->procedural_solid_authored_material_valid) {
        char existing_digest[
            PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY] = {0};
        if (!ProceduralSolidAuthoredMaterialBindingV1_Digest(
                &asset->procedural_solid_authored_binding,
                existing_digest, &report) ||
            strcmp(existing_digest, digest) != 0) {
            set_diag(out_diagnostics, out_diagnostics_size,
                     "procedural solid authored instance identity mismatch");
            return false;
        }
        return load_material_graph(
            runtime_scene_path, reference, asset, digest,
            out_diagnostics, out_diagnostics_size);
    }
    if (!capture_dependency(
            binding_path,
            &asset->procedural_solid_authored_binding_dependency)) {
        set_diag(out_diagnostics, out_diagnostics_size,
                 "procedural solid authored dependency stat failed");
        return false;
    }
    asset->procedural_solid_authored_material_valid = true;
    snprintf(asset->procedural_solid_authored_binding_path,
             sizeof(asset->procedural_solid_authored_binding_path),
             "%s", binding_path);
    asset->procedural_solid_authored_binding = binding;
    asset->procedural_solid_authored_material_count =
        binding.assignment_count;
    memcpy(asset->procedural_solid_authored_materials, materials,
           sizeof(materials));
    memcpy(asset->procedural_solid_authored_material_dependencies,
           dependencies, sizeof(dependencies));
    if (!load_material_graph(
            runtime_scene_path, reference, asset, digest,
            out_diagnostics, out_diagnostics_size)) {
        asset->procedural_solid_authored_material_valid = false;
        return false;
    }
    set_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

bool runtime_mesh_asset_procedural_solid_authored_dependencies_match(
    const RayTracingRuntimeMeshAsset *asset) {
    if (!asset || !asset->procedural_solid_authored_material_valid) return true;
    if (!dependency_matches(
            &asset->procedural_solid_authored_binding_dependency)) return false;
    for (size_t i = 0u;
         i < asset->procedural_solid_authored_material_count; ++i) {
        if (!dependency_matches(
                &asset->procedural_solid_authored_material_dependencies[i])) {
            return false;
        }
    }
    if (asset->procedural_solid_material_graph_valid) {
        if (!dependency_matches(
                &asset->procedural_solid_material_graph_dependency))
            return false;
        for (size_t i = 0u;
             i < asset->procedural_solid_material_graph_material_count; ++i) {
            if (!dependency_matches(
                    &asset->procedural_solid_material_graph_material_dependencies[
                        i])) return false;
        }
    }
    if (asset->procedural_imported_surface_region_valid &&
        !dependency_matches(
            &asset->procedural_imported_surface_region_dependency))
        return false;
    for (size_t i = 0u;
         i < asset->procedural_named_surface_selector_count; ++i) {
        if (!dependency_matches(
                &asset->procedural_named_surface_selector_dependencies[i])) {
            return false;
        }
    }
    if (asset->procedural_surface_wood_grain_valid &&
        !dependency_matches(&asset->procedural_surface_wood_grain_dependency))
        return false;
    return true;
}

const ProceduralSolidAuthoredMaterialV1 *
runtime_mesh_asset_resolve_procedural_solid_authored_material(
    const RayTracingRuntimeMeshAsset *asset,
    const char *region_id) {
    if (!asset || !region_id ||
        !asset->procedural_solid_authored_material_valid) return NULL;
    for (size_t i = 0u;
         i < asset->procedural_solid_authored_material_count; ++i) {
        if (strcmp(
                asset->procedural_solid_authored_binding
                    .assignments[i].region_id,
                region_id) == 0) {
            return &asset->procedural_solid_authored_materials[i];
        }
    }
    return NULL;
}
