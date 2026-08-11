#include "procedural_imported_surface_strands_internal.h"

#include "app/ray_tracing_sha256.h"
#include "procedural/procedural_solid_mesh.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void report_set(
    ProceduralImportedSurfaceStrandReport *report,
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
    const ProceduralImportedSurfaceStrandConfig *config,
    char out_digest[PROCEDURAL_IMPORTED_SURFACE_STRANDS_DIGEST_CAPACITY]) {
    char canonical[640];
    const int count = snprintf(
        canonical, sizeof(canonical),
        "psg23a_strand_config_v1|%.17g|%.17g|%.17g|%.17g|%.17g|%.17g|%.17g|%.17g|%.17g|%.17g|%.17g|%zu|%zu|%zu|%zu|%zu",
        config->selection_threshold,
        config->strand_length_units,
        config->root_radius_units,
        config->tip_radius_units,
        config->root_penetration_units,
        config->length_variation,
        config->bend_strength,
        config->curl_strength,
        config->clearance_factor,
        config->maximum_length_to_bounds_diagonal_ratio,
        config->minimum_triangle_area2,
        config->max_strands,
        config->curve_segment_count,
        config->radial_segments,
        config->max_vertices,
        config->max_triangles);
    return count > 0 && (size_t)count < sizeof(canonical) &&
        ray_tracing_sha256_bytes(canonical, (size_t)count, out_digest);
}

static bool strand_data_digest(
    const ProceduralImportedSurfaceStrandAsset *asset,
    char out_digest[PROCEDURAL_IMPORTED_SURFACE_STRANDS_DIGEST_CAPACITY]) {
    char *canonical;
    size_t capacity;
    size_t length = 0u;
    const size_t point_count =
        asset ? asset->strand_count * asset->points_per_strand : 0u;
    if (!asset || !out_digest || asset->strand_count == 0u ||
        point_count > (SIZE_MAX - 128u) / 160u) return false;
    capacity = 128u + point_count * 160u + asset->strand_count * 160u;
    canonical = malloc(capacity);
    if (!canonical) return false;
    {
        const int count = snprintf(
            canonical, capacity, "psg23a_strand_data_v1|%zu|%zu|",
            asset->strand_count, asset->points_per_strand);
        if (count < 0 || (size_t)count >= capacity) goto fail;
        length = (size_t)count;
    }
    for (size_t strand = 0u; strand < asset->strand_count; ++strand) {
        const CoreObjectVec3 bary = asset->root_barycentrics[strand];
        const CoreObjectVec3 normal = asset->root_normals[strand];
        const CoreObjectVec3 tangent = asset->root_tangents[strand];
        const int count = snprintf(
            canonical + length, capacity - length,
            "%zu|%zu|%.17g|%.17g|%.17g|%.17g|%.17g|%.17g|%.17g|%.17g|%.17g|",
            strand, asset->source_triangle_indices[strand],
            bary.x, bary.y, bary.z,
            normal.x, normal.y, normal.z,
            tangent.x, tangent.y, tangent.z);
        if (count < 0 || (size_t)count >= capacity - length) goto fail;
        length += (size_t)count;
        for (size_t point = 0u;
             point < asset->points_per_strand; ++point) {
            const size_t index = strand * asset->points_per_strand + point;
            const CoreObjectVec3 p = asset->points[index];
            const int point_count_written = snprintf(
                canonical + length, capacity - length,
                "%zu|%.17g|%.17g|%.17g|%.17g|",
                point, p.x, p.y, p.z, asset->radii[index]);
            if (point_count_written < 0 ||
                (size_t)point_count_written >= capacity - length) goto fail;
            length += (size_t)point_count_written;
        }
    }
    {
        const bool ok =
            ray_tracing_sha256_bytes(canonical, length, out_digest);
        free(canonical);
        return ok;
    }
fail:
    free(canonical);
    return false;
}

static bool provenance_digest(
    const ProceduralImportedSurfaceStrandProvenance *provenance,
    char out_digest[PROCEDURAL_IMPORTED_SURFACE_STRANDS_DIGEST_CAPACITY]) {
    char *canonical;
    size_t capacity;
    size_t length = 0u;
    if (!provenance || !out_digest ||
        provenance->triangle_count > (SIZE_MAX - 64u) / 96u) return false;
    capacity = 64u + provenance->triangle_count * 96u;
    canonical = malloc(capacity);
    if (!canonical) return false;
    {
        const int count = snprintf(
            canonical, capacity, "psg23a_strand_provenance_v1|%zu|",
            provenance->triangle_count);
        if (count < 0 || (size_t)count >= capacity) goto fail;
        length = (size_t)count;
    }
    for (size_t i = 0u; i < provenance->triangle_count; ++i) {
        const int count = snprintf(
            canonical + length, capacity - length, "%zu|%zu|%zu|%u|",
            provenance->source_triangle_indices[i],
            provenance->strand_indices[i],
            provenance->segment_indices[i],
            (unsigned int)provenance->roles[i]);
        if (count < 0 || (size_t)count >= capacity - length) goto fail;
        length += (size_t)count;
    }
    {
        const bool ok =
            ray_tracing_sha256_bytes(canonical, length, out_digest);
        free(canonical);
        return ok;
    }
fail:
    free(canonical);
    return false;
}

void ProceduralImportedSurfaceStrandConfig_Init(
    ProceduralImportedSurfaceStrandConfig *config) {
    if (!config) return;
    *config = (ProceduralImportedSurfaceStrandConfig){
        .selection_threshold = 0.58,
        .strand_length_units = 0.28,
        .root_radius_units = 0.018,
        .tip_radius_units = 0.006,
        .root_penetration_units = 0.012,
        .length_variation = 0.30,
        .bend_strength = 0.22,
        .curl_strength = 0.12,
        .clearance_factor = 1.10,
        .maximum_length_to_bounds_diagonal_ratio = 0.18,
        .minimum_triangle_area2 = 1.0e-12,
        .max_strands = 24u,
        .curve_segment_count = 8u,
        .radial_segments = 8u,
        .max_vertices = 200000u,
        .max_triangles = 400000u};
}

void ProceduralImportedSurfaceStrandAsset_Init(
    ProceduralImportedSurfaceStrandAsset *asset) {
    if (asset) memset(asset, 0, sizeof(*asset));
}

void ProceduralImportedSurfaceStrandAsset_Free(
    ProceduralImportedSurfaceStrandAsset *asset) {
    if (!asset) return;
    free(asset->points);
    free(asset->radii);
    free(asset->source_triangle_indices);
    free(asset->root_barycentrics);
    free(asset->root_normals);
    free(asset->root_tangents);
    memset(asset, 0, sizeof(*asset));
}

void ProceduralImportedSurfaceStrandProvenance_Init(
    ProceduralImportedSurfaceStrandProvenance *provenance) {
    if (provenance) memset(provenance, 0, sizeof(*provenance));
}

void ProceduralImportedSurfaceStrandProvenance_Free(
    ProceduralImportedSurfaceStrandProvenance *provenance) {
    if (!provenance) return;
    free(provenance->source_triangle_indices);
    free(provenance->strand_indices);
    free(provenance->segment_indices);
    free(provenance->roles);
    memset(provenance, 0, sizeof(*provenance));
}

const char *ProceduralImportedSurfaceStrandRole_Name(
    ProceduralImportedSurfaceStrandRole role) {
    switch (role) {
        case PROCEDURAL_IMPORTED_SURFACE_STRAND_ROLE_ROOT_CAP:
            return "root_cap";
        case PROCEDURAL_IMPORTED_SURFACE_STRAND_ROLE_SHAFT:
            return "strand_shaft";
        case PROCEDURAL_IMPORTED_SURFACE_STRAND_ROLE_TIP_CAP:
            return "tip_cap";
    }
    return "unknown";
}

bool ProceduralImportedSurfaceStrands_Compile(
    const CoreMeshAssetRuntimeDocument *source,
    const char *source_runtime_path,
    const ProceduralImportedSurfaceRegionV1 *region,
    const char *region_path,
    const ProceduralImportedSurfaceStrandConfig *config,
    const char *strand_asset_id,
    ProceduralImportedSurfaceStrandAsset *out_strands,
    CoreMeshAssetRuntimeDocument *out_tube_document,
    ProceduralImportedSurfaceStrandProvenance *out_provenance,
    ProceduralImportedSurfaceStrandReceipt *out_receipt,
    ProceduralImportedSurfaceStrandReport *report) {
    SurfaceStrandSelection selection = {0};
    ProceduralImportedSurfaceStrandAsset strands;
    CoreMeshAssetRuntimeDocument tubes;
    ProceduralImportedSurfaceStrandProvenance provenance;
    ProceduralImportedSurfaceStrandReceipt receipt = {0};
    ProceduralImportedSurfaceRegionReport region_report = {0};
    ProceduralSolidMeshConfig mesh_config;
    ProceduralSolidMeshSummary mesh_summary = {0};
    ProceduralSolidMeshReport mesh_report = {0};
    CoreObjectVec3 extent;
    double diagonal;
    size_t overlap_pairs = 0u;
    size_t self_pairs = 0u;
    char carrier_file_digest[
        PROCEDURAL_IMPORTED_SURFACE_STRANDS_DIGEST_CAPACITY] = {0};
    const size_t id_length = strand_asset_id ? strlen(strand_asset_id) : 0u;
    ProceduralImportedSurfaceStrandAsset_Init(&strands);
    core_mesh_asset_runtime_document_init(&tubes);
    ProceduralImportedSurfaceStrandProvenance_Init(&provenance);
    report_set(report, false, "arguments", "PSG-23A strand inputs are required");
    if (!source || !source_runtime_path || !region || !region_path || !config ||
        !strand_asset_id || !out_strands || !out_tube_document ||
        !out_provenance || !out_receipt || id_length == 0u ||
        id_length >= 64u || strcmp(source->contract.asset_id,
                                   strand_asset_id) == 0) return false;
    if (!(config->selection_threshold > 0.0 &&
          config->selection_threshold < 1.0) ||
        !(config->strand_length_units > 0.0) ||
        !(config->root_radius_units > 0.0) ||
        !(config->tip_radius_units > 0.0) ||
        config->tip_radius_units > config->root_radius_units ||
        !(config->root_penetration_units > 0.0) ||
        !(config->length_variation >= 0.0 &&
          config->length_variation <= 1.0) ||
        !(config->bend_strength >= 0.0 &&
          config->bend_strength <= 0.75) ||
        !(config->curl_strength >= 0.0 &&
          config->curl_strength <= 0.50) ||
        !(config->clearance_factor >= 1.0) ||
        !(config->maximum_length_to_bounds_diagonal_ratio > 0.0) ||
        !(config->minimum_triangle_area2 > 0.0) ||
        config->max_strands == 0u || config->max_strands > 256u ||
        config->curve_segment_count < 3u ||
        config->curve_segment_count > 64u ||
        config->radial_segments < 6u || config->radial_segments > 32u ||
        config->max_vertices == 0u || config->max_triangles == 0u) {
        report_set(report, false, "config",
                   "strand dimensions, curvature, tessellation, or budgets are invalid");
        return false;
    }
    extent = strand_vec_sub(
        source->contract.local_bounds.max,
        source->contract.local_bounds.min);
    diagonal = strand_vec_length(extent);
    if (!(diagonal > 0.0) ||
        config->strand_length_units >
            diagonal * config->maximum_length_to_bounds_diagonal_ratio ||
        config->root_radius_units >= config->strand_length_units * 0.25 ||
        config->root_penetration_units >= config->strand_length_units * 0.25) {
        report_set(report, false, "strand_scale",
                   "strand scale exceeds the bounded source-diagonal contract");
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
    if (!surface_strand_select(source, region, config, &selection) ||
        !surface_strand_build_asset(&selection, config, &strands) ||
        !surface_strand_validate(
            &strands, config, &overlap_pairs, &self_pairs)) {
        report_set(report, false, "strand_authoring",
                   "carrier produced invalid, overlapping, or self-intersecting strands");
        goto fail;
    }
    receipt.schema_version =
        PROCEDURAL_IMPORTED_SURFACE_STRANDS_SCHEMA_VERSION;
    snprintf(receipt.source_asset_id, sizeof(receipt.source_asset_id), "%s",
             source->contract.asset_id);
    snprintf(receipt.semantic_source_id,
             sizeof(receipt.semantic_source_id), "%s",
             source->contract.source_asset_id);
    snprintf(receipt.strand_asset_id, sizeof(receipt.strand_asset_id), "%s",
             strand_asset_id);
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
    receipt.rejected_clearance_candidate_count =
        selection.rejected_clearance_count;
    receipt.strand_count = strands.strand_count;
    receipt.control_point_count =
        strands.strand_count * strands.points_per_strand;
    receipt.minimum_root_clearance_units =
        selection.minimum_clearance_units;
    receipt.minimum_root_penetration_units =
        config->root_penetration_units;
    receipt.minimum_strand_length_units = DBL_MAX;
    for (size_t i = 0u; i < selection.count; ++i) {
        if (selection.roots[i].length < receipt.minimum_strand_length_units)
            receipt.minimum_strand_length_units = selection.roots[i].length;
        if (selection.roots[i].length > receipt.maximum_strand_length_units)
            receipt.maximum_strand_length_units = selection.roots[i].length;
    }
    receipt.inter_strand_overlap_pair_count = overlap_pairs;
    receipt.strand_self_intersection_pair_count = self_pairs;
    if (!config_digest(config, receipt.config_digest_sha256) ||
        !strand_data_digest(&strands, receipt.strand_data_digest_sha256) ||
        !surface_strand_build_tubes(
            source, &selection, config, strand_asset_id, &strands,
            &tubes, &provenance, &receipt)) {
        report_set(report, false, "strand_geometry",
                   "strand data or closed tube proof construction failed");
        goto fail;
    }
    ProceduralSolidMeshConfig_Init(&mesh_config);
    mesh_config.bounds_min = tubes.contract.local_bounds.min;
    mesh_config.bounds_max = tubes.contract.local_bounds.max;
    mesh_config.max_vertices = config->max_vertices;
    mesh_config.max_triangles = config->max_triangles;
    mesh_config.min_components = strands.strand_count;
    mesh_config.max_components = strands.strand_count;
    mesh_config.minimum_triangle_area2 = config->minimum_triangle_area2;
    mesh_config.require_closed_manifold = true;
    mesh_config.require_positive_volume = true;
    mesh_config.collision_authority =
        PROCEDURAL_SOLID_COLLISION_AUTHORITY_DERIVED_SHELL;
    if (!ProceduralSolidMesh_Reanalyze(
            &mesh_config, &tubes, &mesh_summary, &mesh_report) ||
        !provenance_digest(
            &provenance, receipt.provenance_digest_sha256)) {
        report_set(report, false, "strand_validation",
                   mesh_report.message[0] ? mesh_report.message :
                   "strand tube topology or provenance validation failed");
        goto fail;
    }
    snprintf(receipt.tube_mesh_digest_sha256,
             sizeof(receipt.tube_mesh_digest_sha256), "%s",
             mesh_summary.mesh_digest_sha256);
    receipt.tube_vertex_count = mesh_summary.vertex_count;
    receipt.tube_triangle_count = mesh_summary.triangle_count;
    receipt.boundary_edge_count = mesh_summary.boundary_edge_count;
    receipt.nonmanifold_edge_count = mesh_summary.nonmanifold_edge_count;
    receipt.connected_component_count =
        mesh_summary.connected_component_count;
    receipt.euler_characteristic = mesh_summary.euler_characteristic;
    receipt.signed_volume_units3 = mesh_summary.signed_volume_units3;
    receipt.source_mesh_immutable = true;
    receipt.exact_source_and_carrier_binding = true;
    receipt.root_triangle_mapping_retained = true;
    receipt.root_barycentrics_valid = true;
    receipt.root_attachment_verified =
        receipt.minimum_root_penetration_units > 0.0 &&
        receipt.root_cap_triangle_count > 0u;
    receipt.finite_continuous_strands = true;
    receipt.overlap_gate_passed = overlap_pairs == 0u;
    receipt.self_intersection_gate_passed = self_pairs == 0u;
    receipt.closed_valid_tube_shells =
        receipt.boundary_edge_count == 0u &&
        receipt.nonmanifold_edge_count == 0u &&
        receipt.connected_component_count == receipt.strand_count &&
        receipt.signed_volume_units3 > 0.0;
    receipt.replaceable_strand_asset = true;
    receipt.triangle_tube_proof_backend = true;
    if (!receipt.root_attachment_verified ||
        !receipt.finite_continuous_strands ||
        !receipt.overlap_gate_passed ||
        !receipt.self_intersection_gate_passed ||
        !receipt.closed_valid_tube_shells) {
        report_set(report, false, "acceptance",
                   "strand attachment, continuity, collision, or closure gate failed");
        goto fail;
    }
    surface_strand_selection_free(&selection);
    *out_strands = strands;
    *out_tube_document = tubes;
    *out_provenance = provenance;
    *out_receipt = receipt;
    report_set(report, true, "", "ok");
    return true;
fail:
    surface_strand_selection_free(&selection);
    ProceduralImportedSurfaceStrandAsset_Free(&strands);
    core_mesh_asset_runtime_document_free(&tubes);
    ProceduralImportedSurfaceStrandProvenance_Free(&provenance);
    return false;
}
