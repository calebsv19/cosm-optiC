#include "procedural/procedural_solid_local_remesh.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void local_report(
    ProceduralSolidMeshReport *report,
    ProceduralSolidMeshStatus status,
    const char *field,
    const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = status;
    snprintf(report->field, sizeof(report->field), "%s", field);
    snprintf(report->message, sizeof(report->message), "%s", message);
}

void ProceduralSolidLocalRemeshConfig_Init(
    ProceduralSolidLocalRemeshConfig *config) {
    if (!config) return;
    memset(config, 0, sizeof(*config));
    ProceduralSolidMeshConfig_Init(&config->mesh);
    ProceduralSolidFeatureConfig_Init(&config->feature);
    config->base_cells = 24u;
    config->maximum_cells = 96u;
    config->maximum_passes = 3u;
    config->refinement_factor = 2u;
    config->surface_band_cells = 0.75;
    config->maximum_active_cell_ratio = 0.70;
    config->maximum_relative_volume_delta = 0.045;
    config->maximum_bounds_delta_units = 0.08;
    config->require_topology_convergence = true;
}

static bool local_config_valid(
    const ProceduralSolidLocalRemeshConfig *config) {
    return config && config->base_cells >= 4u &&
        config->maximum_cells >= config->base_cells &&
        config->maximum_cells <= 512u &&
        config->maximum_passes >= 2u &&
        config->maximum_passes <=
            PROCEDURAL_SOLID_LOCAL_REMESH_MAX_PASSES &&
        config->refinement_factor >= 2u &&
        config->refinement_factor <= 4u &&
        config->base_cells % config->refinement_factor == 0u &&
        isfinite(config->surface_band_cells) &&
        config->surface_band_cells >= 0.25 &&
        config->surface_band_cells <= 4.0 &&
        isfinite(config->maximum_active_cell_ratio) &&
        config->maximum_active_cell_ratio > 0.0 &&
        config->maximum_active_cell_ratio < 1.0 &&
        isfinite(config->maximum_relative_volume_delta) &&
        config->maximum_relative_volume_delta >= 0.0 &&
        isfinite(config->maximum_bounds_delta_units) &&
        config->maximum_bounds_delta_units >= 0.0;
}

static size_t grid_id(
    size_t x,
    size_t y,
    size_t z,
    size_t cells) {
    const size_t samples = cells + 1u;
    return x + samples * (y + samples * z);
}

static size_t cell_id(
    size_t x,
    size_t y,
    size_t z,
    size_t cells) {
    return x + cells * (y + cells * z);
}

static CoreObjectVec3 grid_position(
    const ProceduralSolidMeshConfig *mesh,
    size_t cells,
    size_t x,
    size_t y,
    size_t z) {
    return (CoreObjectVec3){
        mesh->bounds_min.x +
            (mesh->bounds_max.x - mesh->bounds_min.x) *
                (double)x / (double)cells,
        mesh->bounds_min.y +
            (mesh->bounds_max.y - mesh->bounds_min.y) *
                (double)y / (double)cells,
        mesh->bounds_min.z +
            (mesh->bounds_max.z - mesh->bounds_min.z) *
                (double)z / (double)cells};
}

static bool evaluate_point(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    CoreObjectVec3 point,
    double *out_value,
    ProceduralSolidMeshReport *report) {
    ProceduralSolidGraphReport graph_report;
    ProceduralSolidSample sample;
    if (!ProceduralSolidGraphV1_Evaluate(
            graph, sources, point, &sample, &graph_report)) {
        local_report(report, PROCEDURAL_SOLID_MESH_STATUS_FIELD,
                     graph_report.field, graph_report.message);
        return false;
    }
    *out_value = sample.signed_distance;
    if (fabs(*out_value) <= 1.0e-12) *out_value = 1.0e-12;
    return true;
}

static bool build_active_mask(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    const ProceduralSolidLocalRemeshConfig *config,
    uint32_t fine_cells,
    uint8_t **out_mask,
    ProceduralSolidLocalRemeshPass *pass,
    ProceduralSolidMeshReport *report) {
    static const size_t corners[8][3] = {
        {0u, 0u, 0u}, {1u, 0u, 0u}, {1u, 1u, 0u}, {0u, 1u, 0u},
        {0u, 0u, 1u}, {1u, 0u, 1u}, {1u, 1u, 1u}, {0u, 1u, 1u}};
    const size_t factor = config->refinement_factor;
    const size_t coarse_cells = fine_cells / factor;
    const size_t coarse_sample_count =
        (coarse_cells + 1u) * (coarse_cells + 1u) *
        (coarse_cells + 1u);
    const size_t coarse_cell_count =
        coarse_cells * coarse_cells * coarse_cells;
    const size_t fine_cell_count =
        (size_t)fine_cells * (size_t)fine_cells * (size_t)fine_cells;
    const double dx =
        (config->mesh.bounds_max.x - config->mesh.bounds_min.x) /
        (double)coarse_cells;
    const double dy =
        (config->mesh.bounds_max.y - config->mesh.bounds_min.y) /
        (double)coarse_cells;
    const double dz =
        (config->mesh.bounds_max.z - config->mesh.bounds_min.z) /
        (double)coarse_cells;
    const double band =
        config->surface_band_cells * fmax(dx, fmax(dy, dz));
    double *values = NULL;
    uint8_t *candidate = NULL;
    uint8_t *expanded = NULL;
    uint8_t *fine_mask = NULL;
    values = malloc(coarse_sample_count * sizeof(*values));
    candidate = calloc(coarse_cell_count, sizeof(*candidate));
    expanded = calloc(coarse_cell_count, sizeof(*expanded));
    fine_mask = calloc(fine_cell_count, sizeof(*fine_mask));
    if (!values || !candidate || !expanded || !fine_mask) {
        free(values);
        free(candidate);
        free(expanded);
        free(fine_mask);
        local_report(report, PROCEDURAL_SOLID_MESH_STATUS_ALLOCATION,
                     "local_mask", "local remesh mask allocation failed");
        return false;
    }
    for (size_t z = 0u; z <= coarse_cells; ++z) {
        for (size_t y = 0u; y <= coarse_cells; ++y) {
            for (size_t x = 0u; x <= coarse_cells; ++x) {
                if (!evaluate_point(
                        graph, sources,
                        grid_position(
                            &config->mesh, coarse_cells, x, y, z),
                        &values[grid_id(
                            x, y, z, coarse_cells)],
                        report)) {
                    free(values);
                    free(candidate);
                    free(expanded);
                    free(fine_mask);
                    return false;
                }
            }
        }
    }
    for (size_t z = 0u; z < coarse_cells; ++z) {
        for (size_t y = 0u; y < coarse_cells; ++y) {
            for (size_t x = 0u; x < coarse_cells; ++x) {
                double minimum = HUGE_VAL;
                double maximum = -HUGE_VAL;
                double minimum_absolute = HUGE_VAL;
                for (size_t corner = 0u; corner < 8u; ++corner) {
                    const double value = values[grid_id(
                        x + corners[corner][0],
                        y + corners[corner][1],
                        z + corners[corner][2],
                        coarse_cells)];
                    minimum = fmin(minimum, value);
                    maximum = fmax(maximum, value);
                    minimum_absolute = fmin(minimum_absolute, fabs(value));
                }
                if ((minimum < 0.0 && maximum > 0.0) ||
                    minimum_absolute <= band) {
                    candidate[cell_id(x, y, z, coarse_cells)] = 1u;
                }
            }
        }
    }
    /*
     * One deterministic closure ring moves every active/inactive interface
     * away from the sampled zero band.  The extracted fine lattice therefore
     * needs no surface-bearing transition cell: inactive neighbors are
     * certified empty by the same signed-distance band and final topology
     * validation remains the fail-closed authority.
     */
    memcpy(expanded, candidate, coarse_cell_count);
    for (size_t ring = 0u; ring < 1u; ++ring) {
        memcpy(candidate, expanded, coarse_cell_count);
        for (size_t z = 0u; z < coarse_cells; ++z) {
            for (size_t y = 0u; y < coarse_cells; ++y) {
                for (size_t x = 0u; x < coarse_cells; ++x) {
                    if (!candidate[cell_id(x, y, z, coarse_cells)]) {
                        continue;
                    }
                    for (int dz_ring = -1; dz_ring <= 1; ++dz_ring) {
                        for (int dy_ring = -1; dy_ring <= 1; ++dy_ring) {
                            for (int dx_ring = -1;
                                 dx_ring <= 1; ++dx_ring) {
                                const long long nx =
                                    (long long)x + dx_ring;
                                const long long ny =
                                    (long long)y + dy_ring;
                                const long long nz =
                                    (long long)z + dz_ring;
                                if (nx >= 0 && ny >= 0 && nz >= 0 &&
                                    (size_t)nx < coarse_cells &&
                                    (size_t)ny < coarse_cells &&
                                    (size_t)nz < coarse_cells) {
                                    expanded[cell_id(
                                        (size_t)nx, (size_t)ny,
                                        (size_t)nz, coarse_cells)] = 1u;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    for (size_t z = 0u; z < coarse_cells; ++z) {
        for (size_t y = 0u; y < coarse_cells; ++y) {
            for (size_t x = 0u; x < coarse_cells; ++x) {
                if (!expanded[cell_id(x, y, z, coarse_cells)]) continue;
                for (size_t fz = 0u; fz < factor; ++fz) {
                    for (size_t fy = 0u; fy < factor; ++fy) {
                        for (size_t fx = 0u; fx < factor; ++fx) {
                            fine_mask[cell_id(
                                x * factor + fx, y * factor + fy,
                                z * factor + fz, fine_cells)] = 1u;
                        }
                    }
                }
            }
        }
    }
    pass->coarse_cells = (uint32_t)coarse_cells;
    pass->fine_cells = fine_cells;
    pass->coarse_sample_count = coarse_sample_count;
    pass->total_fine_cell_count = fine_cell_count;
    for (size_t i = 0u; i < fine_cell_count; ++i) {
        if (fine_mask[i]) ++pass->active_fine_cell_count;
    }
    pass->active_cell_ratio =
        (double)pass->active_fine_cell_count / (double)fine_cell_count;
    for (size_t z = 0u; z < coarse_cells; ++z) {
        for (size_t y = 0u; y < coarse_cells; ++y) {
            for (size_t x = 0u; x < coarse_cells; ++x) {
                static const int neighbors[3][3] = {
                    {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
                const bool active =
                    expanded[cell_id(x, y, z, coarse_cells)] != 0u;
                for (size_t axis = 0u; axis < 3u; ++axis) {
                    const size_t nx = x + (size_t)neighbors[axis][0];
                    const size_t ny = y + (size_t)neighbors[axis][1];
                    const size_t nz = z + (size_t)neighbors[axis][2];
                    if (nx >= coarse_cells || ny >= coarse_cells ||
                        nz >= coarse_cells) {
                        continue;
                    }
                    if (active !=
                        (expanded[cell_id(
                            nx, ny, nz, coarse_cells)] != 0u)) {
                        ++pass->inactive_interface_face_count;
                    }
                }
            }
        }
    }
    free(values);
    free(candidate);
    free(expanded);
    *out_mask = fine_mask;
    return true;
}

static double bounds_delta(
    const ProceduralSolidMeshSummary *a,
    const ProceduralSolidMeshSummary *b) {
    double result = 0.0;
#define DELTA(FIELD) result = fmax(result, fabs(a->FIELD - b->FIELD))
    DELTA(bounds_min.x);
    DELTA(bounds_min.y);
    DELTA(bounds_min.z);
    DELTA(bounds_max.x);
    DELTA(bounds_max.y);
    DELTA(bounds_max.z);
#undef DELTA
    return result;
}

bool ProceduralSolidLocalRemesh_Compile(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    const ProceduralSolidLocalRemeshConfig *config,
    const char *derived_asset_id,
    CoreMeshAssetRuntimeDocument *out_document,
    ProceduralSolidLocalRemeshSummary *out_summary,
    ProceduralSolidMeshReport *report) {
    ProceduralSolidLocalRemeshSummary summary;
    ProceduralSolidMeshSummary previous_mesh;
    CoreMeshAssetRuntimeDocument previous_document;
    bool have_previous = false;
    uint32_t fine_cells;
    if (!graph || !derived_asset_id || !derived_asset_id[0] ||
        !out_document || !out_summary || !local_config_valid(config)) {
        local_report(report, PROCEDURAL_SOLID_MESH_STATUS_CONFIG,
                     "local_remesh_config",
                     "local remesh config is invalid");
        return false;
    }
    memset(&summary, 0, sizeof(summary));
    memset(&previous_mesh, 0, sizeof(previous_mesh));
    core_mesh_asset_runtime_document_init(&previous_document);
    fine_cells = config->base_cells;
    for (size_t pass_index = 0u;
         pass_index < config->maximum_passes &&
         fine_cells <= config->maximum_cells;
         ++pass_index) {
        ProceduralSolidLocalRemeshPass *pass =
            &summary.passes[pass_index];
        ProceduralSolidMeshConfig mesh_config = config->mesh;
        CoreMeshAssetRuntimeDocument document;
        uint8_t *active_mask = NULL;
        core_mesh_asset_runtime_document_init(&document);
        if (!build_active_mask(
                graph, sources, config, fine_cells, &active_mask,
                pass, report)) {
            core_mesh_asset_runtime_document_free(&previous_document);
            return false;
        }
        if (pass->active_fine_cell_count == 0u ||
            pass->active_cell_ratio > config->maximum_active_cell_ratio) {
            free(active_mask);
            core_mesh_asset_runtime_document_free(&previous_document);
            local_report(
                report, PROCEDURAL_SOLID_MESH_STATUS_CAPACITY,
                "local_active_ratio",
                "local refinement exceeded its active-cell ratio budget");
            return false;
        }
        mesh_config.cells_x = fine_cells;
        mesh_config.cells_y = fine_cells;
        mesh_config.cells_z = fine_cells;
        mesh_config.active_cell_mask = active_mask;
        mesh_config.active_cell_mask_count =
            pass->total_fine_cell_count;
        if (!ProceduralSolidMesh_Compile(
                graph, sources, &mesh_config, derived_asset_id,
                &document, &pass->mesh, report)) {
            free(active_mask);
            core_mesh_asset_runtime_document_free(&previous_document);
            return false;
        }
        if (!ProceduralSolidFeature_Optimize(
                graph, sources, &config->feature, &mesh_config,
                &document, &pass->mesh, &pass->feature, report) ||
            !ProceduralSolidRegions_Assign(
                graph, sources, &mesh_config, &document, &pass->mesh,
                &pass->regions, report)) {
            free(active_mask);
            core_mesh_asset_runtime_document_free(&document);
            core_mesh_asset_runtime_document_free(&previous_document);
            return false;
        }
        free(active_mask);
        pass->transition_surface_crossing_count =
            pass->mesh.boundary_edge_count;
        if (have_previous) {
            const double denominator =
                fmax(fabs(pass->mesh.signed_volume_units3), 1.0e-12);
            pass->relative_volume_delta =
                fabs(pass->mesh.signed_volume_units3 -
                     previous_mesh.signed_volume_units3) /
                denominator;
            pass->bounds_delta_units =
                bounds_delta(&pass->mesh, &previous_mesh);
            pass->topology_matches_previous =
                pass->mesh.euler_characteristic ==
                    previous_mesh.euler_characteristic &&
                pass->mesh.connected_component_count ==
                    previous_mesh.connected_component_count &&
                pass->mesh.boundary_edge_count == 0u &&
                pass->mesh.nonmanifold_edge_count == 0u;
            pass->converged =
                (!config->require_topology_convergence ||
                 pass->topology_matches_previous) &&
                pass->relative_volume_delta <=
                    config->maximum_relative_volume_delta &&
                pass->bounds_delta_units <=
                    config->maximum_bounds_delta_units &&
                pass->transition_surface_crossing_count == 0u &&
                pass->feature.measurable_improvement;
        }
        ++summary.pass_count;
        core_mesh_asset_runtime_document_free(&previous_document);
        previous_document = document;
        previous_mesh = pass->mesh;
        have_previous = true;
        if (pass->converged) {
            summary.converged = true;
            summary.selected_pass = pass_index;
            summary.selected_mesh = pass->mesh;
            summary.selected_feature = pass->feature;
            summary.selected_regions = pass->regions;
            break;
        }
        if (fine_cells > config->maximum_cells / 2u) break;
        fine_cells *= 2u;
    }
    if (!summary.converged) {
        char message[256];
        const ProceduralSolidLocalRemeshPass *last =
            summary.pass_count
                ? &summary.passes[summary.pass_count - 1u] : NULL;
        core_mesh_asset_runtime_document_free(&previous_document);
        snprintf(
            message, sizeof(message),
            "local remesh did not converge passes=%zu topology=%d "
            "volume_delta=%.6g bounds_delta=%.6g transition=%zu "
            "feature=%d",
            summary.pass_count,
            last ? (int)last->topology_matches_previous : 0,
            last ? last->relative_volume_delta : 0.0,
            last ? last->bounds_delta_units : 0.0,
            last ? last->transition_surface_crossing_count : 0u,
            last ? (int)last->feature.measurable_improvement : 0);
        local_report(
            report, PROCEDURAL_SOLID_MESH_STATUS_TOPOLOGY,
            "local_convergence", message);
        return false;
    }
    *out_document = previous_document;
    *out_summary = summary;
    return true;
}
