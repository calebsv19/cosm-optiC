#include "procedural/procedural_solid_remesh.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void ProceduralSolidRemeshConfig_Init(ProceduralSolidRemeshConfig *config) {
    if (!config) return;
    memset(config, 0, sizeof(*config));
    ProceduralSolidMeshConfig_Init(&config->mesh);
    config->base_cells = 16u;
    config->maximum_cells = 64u;
    config->maximum_passes = 4u;
    config->requested_feature_size_units = 0.2;
    config->maximum_relative_volume_delta = 0.035;
    config->maximum_bounds_delta_units = 0.04;
    config->require_topology_convergence = true;
}

static double maximum_bounds_delta(
    const ProceduralSolidMeshSummary *a,
    const ProceduralSolidMeshSummary *b) {
    double delta = 0.0;
#define ACCUMULATE(FIELD) delta = fmax(delta, fabs(a->FIELD - b->FIELD))
    ACCUMULATE(bounds_min.x);
    ACCUMULATE(bounds_min.y);
    ACCUMULATE(bounds_min.z);
    ACCUMULATE(bounds_max.x);
    ACCUMULATE(bounds_max.y);
    ACCUMULATE(bounds_max.z);
#undef ACCUMULATE
    return delta;
}

static bool remesh_config_valid(const ProceduralSolidRemeshConfig *config) {
    return config && config->base_cells >= 2u &&
           config->maximum_cells >= config->base_cells &&
           config->maximum_cells <= 512u &&
           config->maximum_passes >= 2u &&
           config->maximum_passes <= PROCEDURAL_SOLID_REMESH_MAX_PASSES &&
           isfinite(config->requested_feature_size_units) &&
           config->requested_feature_size_units > 0.0 &&
           isfinite(config->maximum_relative_volume_delta) &&
           config->maximum_relative_volume_delta >= 0.0 &&
           isfinite(config->maximum_bounds_delta_units) &&
           config->maximum_bounds_delta_units >= 0.0;
}

bool ProceduralSolidRemesh_CompileAdaptive(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    const ProceduralSolidRemeshConfig *config,
    const char *derived_asset_id,
    CoreMeshAssetRuntimeDocument *out_document,
    ProceduralSolidRemeshSummary *out_summary,
    ProceduralSolidMeshReport *report) {
    ProceduralSolidRemeshSummary summary;
    ProceduralSolidMeshSummary previous_summary;
    CoreMeshAssetRuntimeDocument previous_document;
    bool have_previous = false;
    uint32_t cells;
    if (!graph || !config || !derived_asset_id || !out_document ||
        !out_summary || !remesh_config_valid(config)) {
        if (report) {
            memset(report, 0, sizeof(*report));
            report->status = PROCEDURAL_SOLID_MESH_STATUS_CONFIG;
            snprintf(report->field, sizeof(report->field), "remesh_config");
            snprintf(report->message, sizeof(report->message),
                     "adaptive remesh config is invalid");
        }
        return false;
    }
    memset(&summary, 0, sizeof(summary));
    memset(&previous_summary, 0, sizeof(previous_summary));
    core_mesh_asset_runtime_document_init(&previous_document);
    cells = config->base_cells;
    for (size_t pass_index = 0u;
         pass_index < config->maximum_passes &&
         cells <= config->maximum_cells;
         ++pass_index) {
        ProceduralSolidMeshConfig mesh_config = config->mesh;
        ProceduralSolidMeshSummary mesh_summary;
        CoreMeshAssetRuntimeDocument document;
        ProceduralSolidRemeshPass *pass = &summary.passes[pass_index];
        core_mesh_asset_runtime_document_init(&document);
        mesh_config.cells_x = cells;
        mesh_config.cells_y = cells;
        mesh_config.cells_z = cells;
        if (!ProceduralSolidMesh_Compile(
                graph, sources, &mesh_config, derived_asset_id,
                &document, &mesh_summary, report)) {
            core_mesh_asset_runtime_document_free(&previous_document);
            return false;
        }
        pass->cells = cells;
        pass->vertex_count = mesh_summary.vertex_count;
        pass->triangle_count = mesh_summary.triangle_count;
        pass->euler_characteristic = mesh_summary.euler_characteristic;
        pass->connected_component_count =
            mesh_summary.connected_component_count;
        pass->signed_volume_units3 = mesh_summary.signed_volume_units3;
        pass->maximum_edge_length_units =
            mesh_summary.maximum_edge_length_units;
        pass->thin_feature_floor_units =
            mesh_summary.thin_feature_floor_units;
        snprintf(pass->mesh_digest_sha256,
                 sizeof(pass->mesh_digest_sha256), "%s",
                 mesh_summary.mesh_digest_sha256);
        if (have_previous) {
            const double denominator =
                fmax(fabs(mesh_summary.signed_volume_units3), 1.0e-12);
            pass->relative_volume_delta =
                fabs(mesh_summary.signed_volume_units3 -
                     previous_summary.signed_volume_units3) / denominator;
            pass->bounds_delta_units =
                maximum_bounds_delta(&mesh_summary, &previous_summary);
            pass->topology_matches_previous =
                mesh_summary.euler_characteristic ==
                    previous_summary.euler_characteristic &&
                mesh_summary.connected_component_count ==
                    previous_summary.connected_component_count &&
                mesh_summary.boundary_edge_count == 0u &&
                mesh_summary.nonmanifold_edge_count == 0u;
            pass->converged =
                (!config->require_topology_convergence ||
                 pass->topology_matches_previous) &&
                pass->relative_volume_delta <=
                    config->maximum_relative_volume_delta &&
                pass->bounds_delta_units <=
                    config->maximum_bounds_delta_units &&
                mesh_summary.thin_feature_floor_units <=
                    config->requested_feature_size_units;
        }
        ++summary.pass_count;
        core_mesh_asset_runtime_document_free(&previous_document);
        previous_document = document;
        previous_summary = mesh_summary;
        have_previous = true;
        if (pass->converged) {
            summary.converged = true;
            summary.selected_pass = pass_index;
            break;
        }
        if (cells > config->maximum_cells / 2u) break;
        cells *= 2u;
    }
    if (!summary.converged) {
        core_mesh_asset_runtime_document_free(&previous_document);
        if (report) {
            memset(report, 0, sizeof(*report));
            report->status = PROCEDURAL_SOLID_MESH_STATUS_TOPOLOGY;
            snprintf(report->field, sizeof(report->field), "convergence");
            snprintf(report->message, sizeof(report->message),
                     "adaptive remesh did not converge within its budget");
        }
        return false;
    }
    summary.selected_mesh = previous_summary;
    *out_document = previous_document;
    *out_summary = summary;
    return true;
}
