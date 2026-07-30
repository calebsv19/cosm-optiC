#include "procedural_imported_surface_growth_internal.h"

#include "app/ray_tracing_sha256.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void report_set(
    ProceduralImportedSurfaceGrowthReport *report,
    bool ok,
    const char *field,
    const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->ok = ok;
    snprintf(report->field, sizeof(report->field), "%s", field ? field : "");
    snprintf(report->message, sizeof(report->message), "%s",
             message ? message : "");
}

static bool config_digest(
    const ProceduralImportedSurfaceGrowthConfig *config,
    char out_digest[PROCEDURAL_IMPORTED_SURFACE_GROWTH_DIGEST_CAPACITY]) {
    char canonical[512];
    const int count = snprintf(
        canonical, sizeof(canonical),
        "psg22_growth_config_v1|%.17g|%.17g|%.17g|%.17g|%.17g|%.17g|%.17g|%.17g|%.17g|%zu|%zu|%zu|%zu|%zu",
        config->selection_threshold,
        config->mound_radius_units,
        config->mound_height_units,
        config->attachment_depth_units,
        config->radius_variation,
        config->height_variation,
        config->clearance_factor,
        config->maximum_radius_to_bounds_diagonal_ratio,
        config->minimum_triangle_area2,
        config->max_growth_elements,
        config->radial_segments,
        config->latitude_segments,
        config->max_vertices,
        config->max_triangles);
    return count > 0 && (size_t)count < sizeof(canonical) &&
        ray_tracing_sha256_bytes(canonical, (size_t)count, out_digest);
}

static bool provenance_digest(
    const ProceduralImportedSurfaceGrowthProvenance *provenance,
    char out_digest[PROCEDURAL_IMPORTED_SURFACE_GROWTH_DIGEST_CAPACITY]) {
    char *canonical;
    size_t capacity;
    size_t length = 0u;
    if (!provenance || !out_digest ||
        provenance->triangle_count > (SIZE_MAX - 64u) / 72u) return false;
    capacity = 64u + provenance->triangle_count * 72u;
    canonical = malloc(capacity);
    if (!canonical) return false;
    {
        const int count = snprintf(
            canonical, capacity, "psg22_growth_provenance_v1|%zu|",
            provenance->triangle_count);
        if (count < 0 || (size_t)count >= capacity) {
            free(canonical);
            return false;
        }
        length = (size_t)count;
    }
    for (size_t i = 0u; i < provenance->triangle_count; ++i) {
        const int count = snprintf(
            canonical + length, capacity - length, "%zu|%zu|%u|",
            provenance->source_triangle_indices[i],
            provenance->growth_element_indices[i],
            (unsigned int)provenance->roles[i]);
        if (count < 0 || (size_t)count >= capacity - length) {
            free(canonical);
            return false;
        }
        length += (size_t)count;
    }
    {
        const bool ok =
            ray_tracing_sha256_bytes(canonical, length, out_digest);
        free(canonical);
        return ok;
    }
}

void ProceduralImportedSurfaceGrowthConfig_Init(
    ProceduralImportedSurfaceGrowthConfig *config) {
    if (!config) return;
    *config = (ProceduralImportedSurfaceGrowthConfig){
        .selection_threshold = 0.62,
        .mound_radius_units = 0.11,
        .mound_height_units = 0.060,
        .attachment_depth_units = 0.020,
        .radius_variation = 0.25,
        .height_variation = 0.35,
        .clearance_factor = 1.15,
        .maximum_radius_to_bounds_diagonal_ratio = 0.08,
        .minimum_triangle_area2 = 1.0e-12,
        .max_growth_elements = 12u,
        .radial_segments = 16u,
        .latitude_segments = 8u,
        .max_vertices = 100000u,
        .max_triangles = 200000u};
}

void ProceduralImportedSurfaceGrowthProvenance_Init(
    ProceduralImportedSurfaceGrowthProvenance *provenance) {
    if (provenance) memset(provenance, 0, sizeof(*provenance));
}

void ProceduralImportedSurfaceGrowthProvenance_Free(
    ProceduralImportedSurfaceGrowthProvenance *provenance) {
    if (!provenance) return;
    free(provenance->source_triangle_indices);
    free(provenance->growth_element_indices);
    free(provenance->roles);
    memset(provenance, 0, sizeof(*provenance));
}

const char *ProceduralImportedSurfaceGrowthRole_Name(
    ProceduralImportedSurfaceGrowthRole role) {
    switch (role) {
        case PROCEDURAL_IMPORTED_SURFACE_GROWTH_ROLE_EXPOSED_GROWTH:
            return "exposed_growth";
        case PROCEDURAL_IMPORTED_SURFACE_GROWTH_ROLE_ATTACHMENT_BASE:
            return "attachment_base";
    }
    return "unknown";
}

bool ProceduralImportedSurfaceGrowth_Compile(
    const CoreMeshAssetRuntimeDocument *source,
    const char *source_runtime_path,
    const ProceduralImportedSurfaceRegionV1 *region,
    const char *region_path,
    const ProceduralImportedSurfaceGrowthConfig *config,
    const char *growth_asset_id,
    CoreMeshAssetRuntimeDocument *out_document,
    ProceduralImportedSurfaceGrowthProvenance *out_provenance,
    ProceduralImportedSurfaceGrowthReceipt *out_receipt,
    ProceduralImportedSurfaceGrowthReport *report) {
    SurfaceGrowthSelection selection = {0};
    CoreMeshAssetRuntimeDocument document;
    ProceduralImportedSurfaceGrowthProvenance provenance;
    ProceduralImportedSurfaceGrowthReceipt receipt = {0};
    ProceduralImportedSurfaceRegionReport region_report = {0};
    ProceduralSolidMeshConfig mesh_config;
    ProceduralSolidMeshSummary mesh_summary = {0};
    ProceduralSolidMeshReport mesh_report = {0};
    CoreObjectVec3 extent;
    double diagonal;
    size_t overlap_pairs = 0u;
    size_t intersection_pairs = 0u;
    double minimum_clearance = 0.0;
    char carrier_file_digest[
        PROCEDURAL_IMPORTED_SURFACE_GROWTH_DIGEST_CAPACITY] = {0};
    const size_t growth_id_length =
        growth_asset_id ? strlen(growth_asset_id) : 0u;
    core_mesh_asset_runtime_document_init(&document);
    ProceduralImportedSurfaceGrowthProvenance_Init(&provenance);
    report_set(report, false, "arguments", "PSG-22 growth inputs are required");
    if (!source || !source_runtime_path || !region || !region_path || !config ||
        !growth_asset_id || !out_document || !out_provenance || !out_receipt ||
        growth_id_length == 0u || growth_id_length >= 64u ||
        strcmp(source->contract.asset_id, growth_asset_id) == 0) return false;
    if (!(config->selection_threshold > 0.0 &&
          config->selection_threshold < 1.0) ||
        !(config->mound_radius_units > 0.0) ||
        !(config->mound_height_units > 0.0) ||
        !(config->attachment_depth_units > 0.0) ||
        !(config->radius_variation >= 0.0 &&
          config->radius_variation <= 1.0) ||
        !(config->height_variation >= 0.0 &&
          config->height_variation <= 1.0) ||
        !(config->clearance_factor >= 1.0) ||
        !(config->maximum_radius_to_bounds_diagonal_ratio > 0.0) ||
        !(config->minimum_triangle_area2 > 0.0) ||
        config->max_growth_elements == 0u ||
        config->max_growth_elements > 128u ||
        config->radial_segments < 8u || config->radial_segments > 64u ||
        config->latitude_segments < 4u || config->latitude_segments > 32u ||
        config->max_vertices == 0u || config->max_triangles == 0u) {
        report_set(report, false, "config",
                   "growth dimensions, variation, tessellation, or budgets are invalid");
        return false;
    }
    extent = growth_vec_sub(
        source->contract.local_bounds.max,
        source->contract.local_bounds.min);
    diagonal = growth_vec_length(extent);
    if (!(diagonal > 0.0) ||
        config->mound_radius_units >
            diagonal * config->maximum_radius_to_bounds_diagonal_ratio ||
        config->mound_height_units >
            diagonal * config->maximum_radius_to_bounds_diagonal_ratio) {
        report_set(report, false, "growth_scale",
                   "growth scale exceeds the bounded source-diagonal ratio");
        return false;
    }
    if (!source->contract.topology_closed_volume ||
        !source->contract.topology_manifold_expected ||
        !ProceduralImportedSurfaceRegionV1_ValidateForMesh(
            region, source, source_runtime_path, &region_report) ||
        !ray_tracing_sha256_file(region_path, carrier_file_digest)) {
        report_set(report, false, "source_identity",
                   region_report.message[0] ? region_report.message :
                   "source and carrier identity must reproduce exactly");
        return false;
    }
    if (!surface_growth_select(source, region, config, &selection)) {
        report_set(report, false, "growth_selection",
                   "carrier produced no separated growth candidates");
        return false;
    }
    if (!surface_growth_validate_separation(
            &selection, &overlap_pairs, &intersection_pairs,
            &minimum_clearance)) {
        report_set(report, false, "growth_overlap",
                   "growth elements overlap or self-intersect");
        goto fail;
    }
    receipt.schema_version =
        PROCEDURAL_IMPORTED_SURFACE_GROWTH_SCHEMA_VERSION;
    snprintf(receipt.source_asset_id, sizeof(receipt.source_asset_id), "%s",
             source->contract.asset_id);
    snprintf(receipt.semantic_source_id,
             sizeof(receipt.semantic_source_id), "%s",
             source->contract.source_asset_id);
    snprintf(receipt.growth_asset_id, sizeof(receipt.growth_asset_id), "%s",
             growth_asset_id);
    snprintf(receipt.region_id, sizeof(receipt.region_id), "%s",
             region->region_id);
    snprintf(receipt.source_mesh_digest_sha256,
             sizeof(receipt.source_mesh_digest_sha256), "%s",
             region->source_mesh_digest_sha256);
    snprintf(receipt.source_file_digest_sha256,
             sizeof(receipt.source_file_digest_sha256), "%s",
             region->source_file_digest_sha256);
    snprintf(receipt.carrier_value_digest_sha256,
             sizeof(receipt.carrier_value_digest_sha256), "%s",
             region->value_digest_sha256);
    snprintf(receipt.carrier_file_digest_sha256,
             sizeof(receipt.carrier_file_digest_sha256), "%s",
             carrier_file_digest);
    receipt.source_vertex_count = source->vertex_count;
    receipt.source_triangle_count = source->triangle_count;
    receipt.candidate_triangle_count = selection.candidate_count;
    receipt.growth_element_count = selection.count;
    receipt.rejected_clearance_candidate_count =
        selection.rejected_clearance_count;
    receipt.inter_element_overlap_pair_count = overlap_pairs;
    receipt.self_intersection_pair_count = intersection_pairs;
    receipt.minimum_inter_element_clearance_units = minimum_clearance;
    receipt.minimum_attachment_depth_units = DBL_MAX;
    for (size_t i = 0u; i < selection.count; ++i) {
        if (selection.elements[i].attachment_depth <
            receipt.minimum_attachment_depth_units)
            receipt.minimum_attachment_depth_units =
                selection.elements[i].attachment_depth;
        if (selection.elements[i].height >
            receipt.maximum_growth_height_units)
            receipt.maximum_growth_height_units =
                selection.elements[i].height;
    }
    if (!config_digest(config, receipt.config_digest_sha256) ||
        !surface_growth_build_geometry(
            source, &selection, config, growth_asset_id,
            &document, &provenance, &receipt)) {
        report_set(report, false, "growth_geometry",
                   "closed growth geometry construction failed");
        goto fail;
    }
    ProceduralSolidMeshConfig_Init(&mesh_config);
    mesh_config.bounds_min = document.contract.local_bounds.min;
    mesh_config.bounds_max = document.contract.local_bounds.max;
    mesh_config.max_vertices = config->max_vertices;
    mesh_config.max_triangles = config->max_triangles;
    mesh_config.min_components = selection.count;
    mesh_config.max_components = selection.count;
    mesh_config.minimum_triangle_area2 = config->minimum_triangle_area2;
    mesh_config.require_closed_manifold = true;
    mesh_config.require_positive_volume = true;
    mesh_config.collision_authority =
        PROCEDURAL_SOLID_COLLISION_AUTHORITY_DERIVED_SHELL;
    if (!ProceduralSolidMesh_Reanalyze(
            &mesh_config, &document, &mesh_summary, &mesh_report) ||
        !provenance_digest(
            &provenance, receipt.provenance_digest_sha256)) {
        report_set(report, false, "growth_validation",
                   mesh_report.message[0] ? mesh_report.message :
                   "growth topology or provenance validation failed");
        goto fail;
    }
    snprintf(receipt.growth_mesh_digest_sha256,
             sizeof(receipt.growth_mesh_digest_sha256), "%s",
             mesh_summary.mesh_digest_sha256);
    receipt.growth_vertex_count = mesh_summary.vertex_count;
    receipt.growth_triangle_count = mesh_summary.triangle_count;
    receipt.unique_edge_count = mesh_summary.unique_edge_count;
    receipt.boundary_edge_count = mesh_summary.boundary_edge_count;
    receipt.nonmanifold_edge_count = mesh_summary.nonmanifold_edge_count;
    receipt.connected_component_count =
        mesh_summary.connected_component_count;
    receipt.euler_characteristic = mesh_summary.euler_characteristic;
    receipt.signed_volume_units3 = mesh_summary.signed_volume_units3;
    receipt.source_mesh_immutable = true;
    receipt.exact_source_and_carrier_binding = true;
    receipt.source_triangle_mapping_retained = true;
    receipt.attachment_penetration_verified =
        receipt.minimum_attachment_depth_units > 0.0 &&
        receipt.attachment_base_triangle_count > 0u;
    receipt.overlap_gate_passed = overlap_pairs == 0u;
    receipt.self_intersection_gate_passed = intersection_pairs == 0u;
    receipt.closed_valid_growth_shells =
        receipt.boundary_edge_count == 0u &&
        receipt.nonmanifold_edge_count == 0u &&
        receipt.connected_component_count == receipt.growth_element_count &&
        receipt.signed_volume_units3 > 0.0;
    receipt.replaceable_attached_geometry = true;
    if (!receipt.attachment_penetration_verified ||
        !receipt.overlap_gate_passed ||
        !receipt.self_intersection_gate_passed ||
        !receipt.closed_valid_growth_shells) {
        report_set(report, false, "acceptance",
                   "growth attachment, overlap, intersection, or closure gate failed");
        goto fail;
    }
    surface_growth_selection_free(&selection);
    *out_document = document;
    *out_provenance = provenance;
    *out_receipt = receipt;
    report_set(report, true, "", "ok");
    return true;
fail:
    surface_growth_selection_free(&selection);
    core_mesh_asset_runtime_document_free(&document);
    ProceduralImportedSurfaceGrowthProvenance_Free(&provenance);
    return false;
}
