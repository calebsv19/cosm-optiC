#include "procedural/procedural_imported_surface_inset_internal.h"

#include "app/ray_tracing_sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void report_set(
    ProceduralImportedSurfaceInsetReport *report,
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

static bool recompute_normals(CoreMeshAssetRuntimeDocument *document) {
    if (!document || !document->vertices || !document->triangles) return false;
    for (size_t i = 0u; i < document->vertex_count; ++i)
        document->vertices[i].normal = (CoreObjectVec3){0.0, 0.0, 0.0};
    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *triangle = &document->triangles[i];
        const CoreObjectVec3 a = document->vertices[triangle->a].position;
        const CoreObjectVec3 b = document->vertices[triangle->b].position;
        const CoreObjectVec3 c = document->vertices[triangle->c].position;
        const CoreObjectVec3 normal =
            vec_cross(vec_sub(b, a), vec_sub(c, a));
        document->vertices[triangle->a].normal =
            vec_add(document->vertices[triangle->a].normal, normal);
        document->vertices[triangle->b].normal =
            vec_add(document->vertices[triangle->b].normal, normal);
        document->vertices[triangle->c].normal =
            vec_add(document->vertices[triangle->c].normal, normal);
    }
    for (size_t i = 0u; i < document->vertex_count; ++i) {
        if (!vec_normalize(document->vertices[i].normal,
                           &document->vertices[i].normal)) {
            return false;
        }
    }
    document->vertex_normal_count = document->vertex_count;
    document->normal_provenance =
        CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_GENERATED_SMOOTH;
    return true;
}

static bool config_digest(
    const ProceduralImportedSurfaceInsetConfig *config,
    char out_digest[PROCEDURAL_IMPORTED_SURFACE_INSET_DIGEST_CAPACITY]) {
    char canonical[512];
    const int count = snprintf(
        canonical, sizeof(canonical),
        "psg21_inset_config_v2|%.17g|%.17g|%.17g|%.17g|%.17g|%zu|%zu|%zu|%zu|%.17g|%d|%d",
        config->selection_threshold,
        config->depth_units,
        config->depth_variation,
        config->maximum_depth_to_bounds_diagonal_ratio,
        config->minimum_triangle_area2,
        config->max_vertices,
        config->max_triangles,
        config->max_adaptive_refinement_passes,
        config->minimum_selected_component_triangles,
        config->target_boundary_edge_length_units,
        config->refine_transition_band ? 1 : 0,
        config->adaptive_refinement_enabled ? 1 : 0);
    return count > 0 && (size_t)count < sizeof(canonical) &&
        ray_tracing_sha256_bytes(canonical, (size_t)count, out_digest);
}

static bool provenance_digest(
    const ProceduralImportedSurfaceInsetProvenance *provenance,
    char out_digest[PROCEDURAL_IMPORTED_SURFACE_INSET_DIGEST_CAPACITY]) {
    char *canonical;
    size_t capacity;
    size_t length = 0u;
    if (!provenance || !out_digest ||
        provenance->triangle_count > (SIZE_MAX - 64u) / 48u) {
        return false;
    }
    capacity = 64u + provenance->triangle_count * 48u;
    canonical = malloc(capacity);
    if (!canonical) return false;
    {
        const int count = snprintf(
            canonical, capacity, "psg20_provenance_v1|%zu|",
            provenance->triangle_count);
        if (count < 0 || (size_t)count >= capacity) {
            free(canonical);
            return false;
        }
        length = (size_t)count;
    }
    for (size_t i = 0u; i < provenance->triangle_count; ++i) {
        const int count = snprintf(
            canonical + length, capacity - length, "%zu|%u|",
            provenance->source_triangle_indices[i],
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

static bool build_inset(
    const CoreMeshAssetRuntimeDocument *source,
    const RefinedMesh *refined,
    const bool *selected,
    InsetEdge *edges,
    size_t edge_count,
    size_t selected_count,
    size_t boundary_count,
    const ProceduralImportedSurfaceInsetConfig *config,
    const char *derived_asset_id,
    CoreMeshAssetRuntimeDocument *out_document,
    ProceduralImportedSurfaceInsetProvenance *out_provenance,
    ProceduralImportedSurfaceInsetReceipt *receipt) {
    CoreMeshAssetRuntimeDocument result;
    ProceduralImportedSurfaceInsetProvenance provenance;
    size_t *retained_map = NULL;
    size_t *floor_map = NULL;
    bool *retained_used = NULL;
    bool *floor_used = NULL;
    size_t retained_vertices = 0u;
    size_t floor_vertices = 0u;
    size_t retained_triangles = refined->triangle_count - selected_count;
    size_t output_triangle_count;
    size_t triangle_index = 0u;
    CoreResult core_result;
    core_mesh_asset_runtime_document_init(&result);
    ProceduralImportedSurfaceInsetProvenance_Init(&provenance);
    if (boundary_count > (SIZE_MAX - refined->triangle_count) / 2u)
        return false;
    output_triangle_count = refined->triangle_count + boundary_count * 2u;
    if (output_triangle_count > config->max_triangles) return false;
    retained_map = malloc(refined->vertex_count * sizeof(*retained_map));
    floor_map = malloc(refined->vertex_count * sizeof(*floor_map));
    retained_used = calloc(refined->vertex_count, sizeof(*retained_used));
    floor_used = calloc(refined->vertex_count, sizeof(*floor_used));
    if (!retained_map || !floor_map || !retained_used || !floor_used)
        goto fail;
    for (size_t i = 0u; i < refined->triangle_count; ++i) {
        const InsetTriangle *triangle = &refined->triangles[i];
        const size_t ids[3] = {triangle->a, triangle->b, triangle->c};
        for (size_t corner = 0u; corner < 3u; ++corner) {
            if (selected[i]) floor_used[ids[corner]] = true;
            else retained_used[ids[corner]] = true;
        }
    }
    for (size_t i = 0u; i < refined->vertex_count; ++i) {
        retained_map[i] = SIZE_MAX;
        floor_map[i] = SIZE_MAX;
        if (retained_used[i]) retained_map[i] = retained_vertices++;
    }
    for (size_t i = 0u; i < refined->vertex_count; ++i)
        if (floor_used[i]) floor_map[i] = retained_vertices + floor_vertices++;
    if (retained_vertices > config->max_vertices - floor_vertices)
        goto fail;
    core_result = core_mesh_asset_runtime_contract_set_asset_id(
        &result.contract, derived_asset_id);
    if (core_result.code != CORE_OK) goto fail;
    core_result = core_mesh_asset_runtime_contract_set_source_asset_id(
        &result.contract, source->contract.asset_id);
    if (core_result.code != CORE_OK ||
        core_mesh_asset_runtime_document_set_vertex_count(
            &result, retained_vertices + floor_vertices).code != CORE_OK ||
        core_mesh_asset_runtime_document_set_triangle_count(
            &result, output_triangle_count).code != CORE_OK ||
        core_mesh_asset_runtime_document_set_surface_group_count(
            &result, 3u).code != CORE_OK) {
        goto fail;
    }
    provenance.triangle_count = output_triangle_count;
    provenance.source_triangle_indices = calloc(
        output_triangle_count, sizeof(*provenance.source_triangle_indices));
    provenance.roles = calloc(
        output_triangle_count, sizeof(*provenance.roles));
    if (!provenance.source_triangle_indices || !provenance.roles) goto fail;
    for (size_t i = 0u; i < refined->vertex_count; ++i) {
        if (retained_used[i])
            result.vertices[retained_map[i]] = refined->vertices[i];
        if (floor_used[i]) {
            const double modulation =
                1.0 - config->depth_variation +
                config->depth_variation * refined->weights[i];
            const double depth = config->depth_units * modulation;
            result.vertices[floor_map[i]] = refined->vertices[i];
            result.vertices[floor_map[i]].position = vec_sub(
                refined->vertices[i].position,
                vec_scale(refined->vertices[i].normal, depth));
            if (receipt->minimum_inset_depth_units == 0.0 ||
                depth < receipt->minimum_inset_depth_units)
                receipt->minimum_inset_depth_units = depth;
            if (depth > receipt->maximum_inset_depth_units)
                receipt->maximum_inset_depth_units = depth;
        }
    }
#define OUTPUT_TRIANGLE(a_, b_, c_, group_, source_, role_) \
    do { \
        CoreMeshAssetRuntimeTriangle *output_triangle = \
            &result.triangles[triangle_index]; \
        output_triangle->a = (a_); \
        output_triangle->b = (b_); \
        output_triangle->c = (c_); \
        snprintf(output_triangle->surface_group_id, \
                 sizeof(output_triangle->surface_group_id), "%s", (group_)); \
        provenance.source_triangle_indices[triangle_index] = (source_); \
        provenance.roles[triangle_index] = (role_); \
        ++triangle_index; \
    } while (0)
    for (size_t i = 0u; i < refined->triangle_count; ++i) {
        if (!selected[i]) continue;
        const InsetTriangle *triangle = &refined->triangles[i];
        OUTPUT_TRIANGLE(
            floor_map[triangle->a],
            floor_map[triangle->b],
            floor_map[triangle->c],
            "inset_floor",
            triangle->source_triangle,
            PROCEDURAL_IMPORTED_SURFACE_INSET_ROLE_INSET_FLOOR);
    }
    for (size_t i = 0u; i < refined->triangle_count; ++i) {
        if (selected[i]) continue;
        const InsetTriangle *triangle = &refined->triangles[i];
        OUTPUT_TRIANGLE(
            retained_map[triangle->a],
            retained_map[triangle->b],
            retained_map[triangle->c],
            "retained_surface",
            triangle->source_triangle,
            PROCEDURAL_IMPORTED_SURFACE_INSET_ROLE_RETAINED_SURFACE);
    }
    for (size_t i = 0u; i < edge_count; ++i) {
        InsetEdge *edge = &edges[i];
        size_t selected_triangle;
        size_t a;
        size_t b;
        if (selected[edge->first_triangle] ==
            selected[edge->second_triangle]) {
            continue;
        }
        selected_triangle = selected[edge->first_triangle]
            ? edge->first_triangle : edge->second_triangle;
        if (!selected_directed_edge(
                &refined->triangles[selected_triangle],
                edge->lo, edge->hi, &a, &b) ||
            retained_map[a] == SIZE_MAX || retained_map[b] == SIZE_MAX ||
            floor_map[a] == SIZE_MAX || floor_map[b] == SIZE_MAX) {
            goto fail;
        }
        OUTPUT_TRIANGLE(
            retained_map[a], retained_map[b], floor_map[b],
            "transition_wall",
            refined->triangles[selected_triangle].source_triangle,
            PROCEDURAL_IMPORTED_SURFACE_INSET_ROLE_TRANSITION_WALL);
        OUTPUT_TRIANGLE(
            retained_map[a], floor_map[b], floor_map[a],
            "transition_wall",
            refined->triangles[selected_triangle].source_triangle,
            PROCEDURAL_IMPORTED_SURFACE_INSET_ROLE_TRANSITION_WALL);
    }
#undef OUTPUT_TRIANGLE
    if (triangle_index != output_triangle_count) goto fail;
    snprintf(result.surface_groups[0].group_id,
             sizeof(result.surface_groups[0].group_id),
             "inset_floor");
    result.surface_groups[0].triangle_start = 0u;
    result.surface_groups[0].triangle_count = selected_count;
    snprintf(result.surface_groups[1].group_id,
             sizeof(result.surface_groups[1].group_id),
             "retained_surface");
    result.surface_groups[1].triangle_start = selected_count;
    result.surface_groups[1].triangle_count = retained_triangles;
    snprintf(result.surface_groups[2].group_id,
             sizeof(result.surface_groups[2].group_id),
             "transition_wall");
    result.surface_groups[2].triangle_start =
        selected_count + retained_triangles;
    result.surface_groups[2].triangle_count = boundary_count * 2u;
    result.contract.asset_type = source->contract.asset_type;
    result.contract.pivot = source->contract.pivot;
    result.contract.topology_closed_volume = true;
    result.contract.topology_manifold_expected = true;
    if (!recompute_normals(&result)) goto fail;
    free(retained_map);
    free(floor_map);
    free(retained_used);
    free(floor_used);
    *out_document = result;
    *out_provenance = provenance;
    return true;
fail:
    free(retained_map);
    free(floor_map);
    free(retained_used);
    free(floor_used);
    core_mesh_asset_runtime_document_free(&result);
    ProceduralImportedSurfaceInsetProvenance_Free(&provenance);
    return false;
}

void ProceduralImportedSurfaceInsetConfig_Init(
    ProceduralImportedSurfaceInsetConfig *config) {
    if (!config) return;
    *config = (ProceduralImportedSurfaceInsetConfig){
        .selection_threshold = 0.55,
        .depth_units = 0.045,
        .depth_variation = 0.15,
        .maximum_depth_to_bounds_diagonal_ratio = 0.04,
        .minimum_triangle_area2 = 1.0e-12,
        .max_vertices = 1000000u,
        .max_triangles = 2000000u,
        .max_adaptive_refinement_passes = 4u,
        .minimum_selected_component_triangles = 8u,
        .target_boundary_edge_length_units = 0.0,
        .refine_transition_band = true,
        .adaptive_refinement_enabled = true};
}

void ProceduralImportedSurfaceInsetProvenance_Init(
    ProceduralImportedSurfaceInsetProvenance *provenance) {
    if (provenance) memset(provenance, 0, sizeof(*provenance));
}

void ProceduralImportedSurfaceInsetProvenance_Free(
    ProceduralImportedSurfaceInsetProvenance *provenance) {
    if (!provenance) return;
    free(provenance->source_triangle_indices);
    free(provenance->roles);
    memset(provenance, 0, sizeof(*provenance));
}

const char *ProceduralImportedSurfaceInsetRole_Name(
    ProceduralImportedSurfaceInsetRole role) {
    switch (role) {
        case PROCEDURAL_IMPORTED_SURFACE_INSET_ROLE_RETAINED_SURFACE:
            return "retained_surface";
        case PROCEDURAL_IMPORTED_SURFACE_INSET_ROLE_TRANSITION_WALL:
            return "transition_wall";
        case PROCEDURAL_IMPORTED_SURFACE_INSET_ROLE_INSET_FLOOR:
            return "inset_floor";
    }
    return "unknown";
}

bool ProceduralImportedSurfaceInset_Compile(
    const CoreMeshAssetRuntimeDocument *source,
    const char *source_runtime_path,
    const ProceduralImportedSurfaceRegionV1 *region,
    const char *region_path,
    const ProceduralImportedSurfaceInsetConfig *config,
    const char *derived_asset_id,
    CoreMeshAssetRuntimeDocument *out_document,
    ProceduralImportedSurfaceInsetProvenance *out_provenance,
    ProceduralImportedSurfaceInsetReceipt *out_receipt,
    ProceduralImportedSurfaceInsetReport *report) {
    RefinedMesh refined = {0};
    bool *selected = NULL;
    InsetEdge *edges = NULL;
    size_t edge_count = 0u;
    size_t selected_count = 0u;
    size_t discarded_candidate_count = 0u;
    size_t selected_component_count = 0u;
    size_t boundary_count = 0u;
    size_t boundary_loops = 0u;
    double maximum_boundary_edge = 0.0;
    InsetRefinementSummary refinement_summary = {0};
    CoreMeshAssetRuntimeDocument document;
    ProceduralImportedSurfaceInsetProvenance provenance;
    ProceduralImportedSurfaceInsetReceipt receipt = {0};
    ProceduralImportedSurfaceRegionReport region_report = {0};
    ProceduralSolidMeshConfig mesh_config;
    ProceduralSolidMeshSummary mesh_summary = {0};
    ProceduralSolidMeshReport mesh_report = {0};
    CoreObjectVec3 bounds_extent;
    double bounds_diagonal;
    const size_t derived_id_length =
        derived_asset_id ? strlen(derived_asset_id) : 0u;
    core_mesh_asset_runtime_document_init(&document);
    ProceduralImportedSurfaceInsetProvenance_Init(&provenance);
    report_set(report, false, "arguments", "PSG-20 inset inputs are required");
    if (!source || !source_runtime_path || !region || !region_path || !config ||
        !derived_asset_id || !out_document || !out_provenance || !out_receipt ||
        derived_id_length == 0u || derived_id_length >= 64u ||
        strcmp(source->contract.asset_id, derived_asset_id) == 0) {
        return false;
    }
    if (!(config->selection_threshold > 0.0 &&
          config->selection_threshold < 1.0) ||
        !(config->depth_units > 0.0) ||
        !(config->depth_variation >= 0.0 &&
          config->depth_variation <= 1.0) ||
        !(config->maximum_depth_to_bounds_diagonal_ratio > 0.0) ||
        !(config->minimum_triangle_area2 > 0.0) ||
        config->max_vertices < source->vertex_count ||
        config->max_triangles < source->triangle_count ||
        config->max_adaptive_refinement_passes == 0u ||
        config->max_adaptive_refinement_passes > 8u ||
        config->minimum_selected_component_triangles == 0u ||
        config->target_boundary_edge_length_units < 0.0) {
        report_set(report, false, "config",
                   "threshold, depth, variation, area, and budgets are invalid");
        return false;
    }
    bounds_extent = vec_sub(
        source->contract.local_bounds.max,
        source->contract.local_bounds.min);
    bounds_diagonal = vec_length(bounds_extent);
    if (!(bounds_diagonal > 0.0) ||
        config->depth_units >
            bounds_diagonal *
            config->maximum_depth_to_bounds_diagonal_ratio) {
        report_set(report, false, "depth_units",
                   "inset depth exceeds the bounded source-diagonal ratio");
        return false;
    }
    if (!source->contract.topology_closed_volume ||
        !source->contract.topology_manifold_expected ||
        !ProceduralImportedSurfaceRegionV1_ValidateForMesh(
            region, source, source_runtime_path, &region_report)) {
        report_set(report, false, "source_identity",
                   region_report.message[0] ? region_report.message :
                   "source must be a closed manifold exact carrier match");
        return false;
    }
    if (!refine_transition_band(
            source, region, config, &refined,
            &refinement_summary)) {
        report_set(report, false, "transition_refinement",
                   "localized conforming transition refinement failed");
        return false;
    }
    if (!classify_patch(
            &refined, config->selection_threshold, &selected,
            &edges, &edge_count, &selected_count,
            &discarded_candidate_count, &selected_component_count,
            &boundary_count, &boundary_loops, &maximum_boundary_edge,
            config->minimum_selected_component_triangles)) {
        report_set(report, false, "selected_patch",
                   "carrier threshold must produce valid selected components and loops");
        goto fail;
    }
    receipt.schema_version =
        PROCEDURAL_IMPORTED_SURFACE_INSET_SCHEMA_VERSION;
    snprintf(receipt.source_asset_id, sizeof(receipt.source_asset_id), "%s",
             source->contract.asset_id);
    snprintf(receipt.semantic_source_id,
             sizeof(receipt.semantic_source_id), "%s",
             source->contract.source_asset_id);
    snprintf(receipt.derived_asset_id, sizeof(receipt.derived_asset_id), "%s",
             derived_asset_id);
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
    if (!ray_tracing_sha256_file(
            region_path, receipt.carrier_file_digest_sha256) ||
        !config_digest(config, receipt.config_digest_sha256)) {
        report_set(report, false, "identity",
                   "carrier-file or inset-config digest failed");
        goto fail;
    }
    receipt.source_vertex_count = source->vertex_count;
    receipt.source_triangle_count = source->triangle_count;
    receipt.refined_vertex_count = refined.vertex_count;
    receipt.refined_triangle_count = refined.triangle_count;
    receipt.transition_source_triangle_count =
        refinement_summary.transition_source_triangle_count;
    receipt.adaptive_refinement_pass_count = refinement_summary.pass_count;
    receipt.discarded_candidate_triangle_count =
        discarded_candidate_count;
    receipt.selected_component_count = selected_component_count;
    receipt.selected_refined_triangle_count = selected_count;
    receipt.retained_triangle_count = refined.triangle_count - selected_count;
    receipt.transition_wall_triangle_count = boundary_count * 2u;
    receipt.inset_floor_triangle_count = selected_count;
    receipt.boundary_loop_count = boundary_loops;
    receipt.boundary_ring_edge_count = boundary_count;
    receipt.target_boundary_edge_length_units =
        refinement_summary.target_boundary_edge_length_units;
    receipt.initial_max_boundary_edge_length_units =
        refinement_summary.initial_max_boundary_edge_length_units;
    receipt.final_max_boundary_edge_length_units = maximum_boundary_edge;
    if (!build_inset(
            source, &refined, selected, edges, edge_count, selected_count,
            boundary_count, config, derived_asset_id, &document, &provenance,
            &receipt)) {
        report_set(report, false, "derived_shell",
                   "failed to build retained, wall, and floor topology");
        goto fail;
    }
    ProceduralSolidMeshConfig_Init(&mesh_config);
    mesh_config.max_vertices = config->max_vertices;
    mesh_config.max_triangles = config->max_triangles;
    mesh_config.min_components = 1u;
    mesh_config.max_components = 1u;
    mesh_config.minimum_triangle_area2 = config->minimum_triangle_area2;
    mesh_config.require_closed_manifold = true;
    mesh_config.require_positive_volume = true;
    if (!ProceduralSolidMesh_Reanalyze(
            &mesh_config, &document, &mesh_summary, &mesh_report) ||
        !provenance_digest(
            &provenance, receipt.provenance_digest_sha256)) {
        report_set(report, false, "validation",
                   mesh_report.message[0] ? mesh_report.message :
                   "derived topology or provenance digest failed");
        goto fail;
    }
    snprintf(receipt.derived_mesh_digest_sha256,
             sizeof(receipt.derived_mesh_digest_sha256), "%s",
             mesh_summary.mesh_digest_sha256);
    receipt.derived_vertex_count = document.vertex_count;
    receipt.derived_triangle_count = document.triangle_count;
    receipt.unique_edge_count = mesh_summary.unique_edge_count;
    receipt.boundary_edge_count = mesh_summary.boundary_edge_count;
    receipt.nonmanifold_edge_count = mesh_summary.nonmanifold_edge_count;
    receipt.connected_component_count =
        mesh_summary.connected_component_count;
    receipt.euler_characteristic = mesh_summary.euler_characteristic;
    receipt.signed_volume_units3 = mesh_summary.signed_volume_units3;
    receipt.transition_refinement_active =
        config->refine_transition_band &&
        refinement_summary.transition_source_triangle_count > 0u &&
        refined.triangle_count > source->triangle_count;
    receipt.adaptive_refinement_active =
        config->adaptive_refinement_enabled &&
        refinement_summary.pass_count > 1u;
    receipt.adaptive_refinement_converged =
        refinement_summary.converged &&
        maximum_boundary_edge <=
            refinement_summary.target_boundary_edge_length_units;
    receipt.source_mesh_immutable = true;
    receipt.exact_source_and_carrier_binding = true;
    receipt.source_triangle_mapping_retained = true;
    receipt.explicit_region_transition_topology =
        receipt.transition_wall_triangle_count > 0u &&
        receipt.inset_floor_triangle_count > 0u;
    receipt.replaceable_derived_geometry = true;
    receipt.closed_valid_shell =
        receipt.boundary_edge_count == 0u &&
        receipt.nonmanifold_edge_count == 0u &&
        receipt.connected_component_count == 1u &&
        receipt.euler_characteristic == 2 &&
        receipt.signed_volume_units3 > 0.0;
    if (!receipt.transition_refinement_active ||
        !receipt.adaptive_refinement_active ||
        !receipt.adaptive_refinement_converged ||
        receipt.selected_component_count == 0u ||
        receipt.boundary_loop_count < receipt.selected_component_count ||
        !(receipt.final_max_boundary_edge_length_units <
          receipt.initial_max_boundary_edge_length_units) ||
        !receipt.explicit_region_transition_topology ||
        !receipt.closed_valid_shell ||
        receipt.minimum_inset_depth_units <= 0.0 ||
        receipt.maximum_inset_depth_units < receipt.minimum_inset_depth_units ||
        strcmp(receipt.source_mesh_digest_sha256,
               receipt.derived_mesh_digest_sha256) == 0) {
        report_set(report, false, "acceptance",
                   "PSG-20 physical inset acceptance gates failed");
        goto fail;
    }
    free(selected);
    free(edges);
    refined_free(&refined);
    *out_document = document;
    *out_provenance = provenance;
    *out_receipt = receipt;
    report_set(report, true, "", "ok");
    return true;
fail:
    free(selected);
    free(edges);
    refined_free(&refined);
    core_mesh_asset_runtime_document_free(&document);
    ProceduralImportedSurfaceInsetProvenance_Free(&provenance);
    return false;
}
