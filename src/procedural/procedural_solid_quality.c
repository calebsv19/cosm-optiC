#include "procedural/procedural_solid_quality.h"

#include "procedural_solid_field_query.h"
#include "procedural_solid_geometry_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void quality_report(
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

static void quality_wrap_report(
    ProceduralSolidMeshReport *report,
    const char *field,
    const char *stage) {
    ProceduralSolidMeshReport prior;
    char message[256];
    if (!report) return;
    prior = *report;
    snprintf(message, sizeof(message), "%s failed (%s): %.180s",
             stage,
             prior.field[0] ? prior.field : "unknown",
             prior.message);
    quality_report(report, prior.status, field, message);
}

void ProceduralSolidQualityConfig_Init(ProceduralSolidQualityConfig *config) {
    if (!config) return;
    memset(config, 0, sizeof(*config));
    ProceduralSolidLocalRemeshConfig_Init(&config->local);
    ProceduralSolidCreaseConfig_Init(&config->crease);
    ProceduralSolidShadingConfig_Init(&config->shading);
    config->baseline_maximum_cells = 48u;
    config->quality_maximum_cells = 96u;
    config->maximum_signed_distance_rms_units = 0.0012;
    config->maximum_signed_distance_max_units = 0.010;
    config->maximum_face_gradient_rms_degrees = 8.0;
    config->minimum_refinement_improvement_ratio = 0.08;
    config->enable_error_driven_refinement = true;
}

static bool quality_config_valid(
    const ProceduralSolidQualityConfig *config) {
    return config &&
        config->baseline_maximum_cells >= config->local.base_cells &&
        config->quality_maximum_cells >= config->baseline_maximum_cells &&
        config->quality_maximum_cells <= 512u &&
        isfinite(config->maximum_signed_distance_rms_units) &&
        config->maximum_signed_distance_rms_units >= 0.0 &&
        isfinite(config->maximum_signed_distance_max_units) &&
        config->maximum_signed_distance_max_units >= 0.0 &&
        isfinite(config->maximum_face_gradient_rms_degrees) &&
        config->maximum_face_gradient_rms_degrees >= 0.0 &&
        config->maximum_face_gradient_rms_degrees < 180.0 &&
        isfinite(config->minimum_refinement_improvement_ratio) &&
        config->minimum_refinement_improvement_ratio >= 0.0 &&
        config->minimum_refinement_improvement_ratio < 1.0;
}

static double vector_angle_degrees(CoreObjectVec3 a, CoreObjectVec3 b) {
    CoreObjectVec3 na;
    CoreObjectVec3 nb;
    if (!psg_vec_normalize(a, &na) || !psg_vec_normalize(b, &nb)) {
        return 180.0;
    }
    return acos(psg_clamp_unit(psg_vec_dot(na, nb))) *
        180.0 / M_PI;
}

static bool accumulate_distance(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    CoreObjectVec3 point,
    ProceduralSolidSurfaceErrorSummary *summary) {
    double distance;
    if (!procedural_solid_field_distance(
            graph, sources, point, &distance)) {
        return false;
    }
    summary->signed_distance_rms_units += distance * distance;
    summary->signed_distance_max_units =
        fmax(summary->signed_distance_max_units, fabs(distance));
    ++summary->sample_count;
    return true;
}

bool ProceduralSolidQuality_Analyze(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    const ProceduralSolidMeshConfig *mesh_config,
    const CoreMeshAssetRuntimeDocument *document,
    ProceduralSolidSurfaceErrorSummary *out_summary,
    ProceduralSolidMeshReport *report) {
    ProceduralSolidSurfaceErrorSummary summary;
    double angle_square_sum = 0.0;
    if (!graph || !mesh_config || !document || !out_summary ||
        document->triangle_count == 0u || !document->triangles ||
        document->vertex_count == 0u || !document->vertices) {
        quality_report(report, PROCEDURAL_SOLID_MESH_STATUS_NULL_ARGUMENT,
                       "quality_analyze",
                       "surface quality analysis inputs are required");
        return false;
    }
    memset(&summary, 0, sizeof(summary));
    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *triangle =
            &document->triangles[i];
        CoreObjectVec3 a;
        CoreObjectVec3 b;
        CoreObjectVec3 c;
        CoreObjectVec3 centroid;
        CoreObjectVec3 face_normal;
        CoreObjectVec3 gradient;
        double angle;
        if (triangle->a >= document->vertex_count ||
            triangle->b >= document->vertex_count ||
            triangle->c >= document->vertex_count) {
            quality_report(report, PROCEDURAL_SOLID_MESH_STATUS_DEGENERATE,
                           "quality_triangle",
                           "surface quality analysis found an invalid triangle");
            return false;
        }
        a = document->vertices[triangle->a].position;
        b = document->vertices[triangle->b].position;
        c = document->vertices[triangle->c].position;
        centroid = psg_vec_scale(
            psg_vec_add(psg_vec_add(a, b), c), 1.0 / 3.0);
        if (!psg_vec_normalize(
                psg_vec_cross(psg_vec_sub(b, a), psg_vec_sub(c, a)),
                &face_normal) ||
            !procedural_solid_field_gradient(
                graph, sources, centroid,
                mesh_config->gradient_step_units, &gradient) ||
            !accumulate_distance(graph, sources, centroid, &summary) ||
            !accumulate_distance(
                graph, sources,
                psg_vec_scale(psg_vec_add(a, b), 0.5), &summary) ||
            !accumulate_distance(
                graph, sources,
                psg_vec_scale(psg_vec_add(b, c), 0.5), &summary) ||
            !accumulate_distance(
                graph, sources,
                psg_vec_scale(psg_vec_add(c, a), 0.5), &summary)) {
            quality_report(report, PROCEDURAL_SOLID_MESH_STATUS_FIELD,
                           "quality_field",
                           "surface quality field sampling failed");
            return false;
        }
        angle = vector_angle_degrees(face_normal, gradient);
        angle_square_sum += angle * angle;
        summary.face_gradient_max_degrees =
            fmax(summary.face_gradient_max_degrees, angle);
    }
    summary.signed_distance_rms_units = sqrt(
        summary.signed_distance_rms_units / (double)summary.sample_count);
    summary.face_gradient_rms_degrees = sqrt(
        angle_square_sum / (double)document->triangle_count);
    summary.composite_score =
        summary.signed_distance_rms_units +
        mesh_config->gradient_step_units *
            summary.face_gradient_rms_degrees * M_PI / 180.0;
    *out_summary = summary;
    return true;
}

static bool error_exceeds_contract(
    const ProceduralSolidQualityConfig *config,
    const ProceduralSolidSurfaceErrorSummary *summary) {
    return summary->signed_distance_rms_units >
            config->maximum_signed_distance_rms_units ||
        summary->signed_distance_max_units >
            config->maximum_signed_distance_max_units ||
        summary->face_gradient_rms_degrees >
            config->maximum_face_gradient_rms_degrees;
}

bool ProceduralSolidQuality_Compile(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    const ProceduralSolidQualityConfig *config,
    const char *derived_asset_id,
    CoreMeshAssetRuntimeDocument *out_document,
    ProceduralSolidQualitySummary *out_summary,
    ProceduralSolidMeshReport *report) {
    ProceduralSolidQualitySummary summary;
    ProceduralSolidLocalRemeshConfig baseline_config;
    ProceduralSolidLocalRemeshConfig refined_config;
    CoreMeshAssetRuntimeDocument baseline_document;
    CoreMeshAssetRuntimeDocument refined_document;
    ProceduralSolidLocalRemeshSummary baseline_local;
    ProceduralSolidLocalRemeshSummary refined_local;
    CoreMeshAssetRuntimeDocument *selected_document;
    ProceduralSolidLocalRemeshSummary *selected_local;
    if (!graph || !quality_config_valid(config) ||
        !derived_asset_id || !derived_asset_id[0] ||
        !out_document || !out_summary) {
        quality_report(report, PROCEDURAL_SOLID_MESH_STATUS_CONFIG,
                       "quality_config",
                       "quality compiler inputs are invalid");
        return false;
    }
    memset(&summary, 0, sizeof(summary));
    memset(&baseline_local, 0, sizeof(baseline_local));
    memset(&refined_local, 0, sizeof(refined_local));
    core_mesh_asset_runtime_document_init(&baseline_document);
    core_mesh_asset_runtime_document_init(&refined_document);
    baseline_config = config->local;
    baseline_config.maximum_cells = config->baseline_maximum_cells;
    baseline_config.maximum_passes =
        PROCEDURAL_SOLID_LOCAL_REMESH_MAX_PASSES;
    if (!ProceduralSolidLocalRemesh_Compile(
            graph, sources, &baseline_config, derived_asset_id,
            &baseline_document, &baseline_local, report)) {
        return false;
    }
    summary.baseline_cells =
        baseline_local.passes[baseline_local.selected_pass].fine_cells;
    if (!ProceduralSolidQuality_Analyze(
            graph, sources, &baseline_config.mesh, &baseline_document,
            &summary.baseline_error, report)) {
        core_mesh_asset_runtime_document_free(&baseline_document);
        return false;
    }
    summary.refinement_triggered =
        config->enable_error_driven_refinement &&
        config->quality_maximum_cells > summary.baseline_cells &&
        error_exceeds_contract(config, &summary.baseline_error);
    selected_document = &baseline_document;
    selected_local = &baseline_local;
    summary.selected_error = summary.baseline_error;
    if (summary.refinement_triggered) {
        double improvement;
        refined_config = config->local;
        refined_config.base_cells = summary.baseline_cells;
        refined_config.maximum_cells = config->quality_maximum_cells;
        refined_config.maximum_passes = 2u;
        if (!ProceduralSolidLocalRemesh_Compile(
                graph, sources, &refined_config, derived_asset_id,
                &refined_document, &refined_local, report)) {
            quality_wrap_report(
                report, "quality_refined_local", "refined local remesh");
            core_mesh_asset_runtime_document_free(&baseline_document);
            core_mesh_asset_runtime_document_free(&refined_document);
            return false;
        }
        if (!ProceduralSolidQuality_Analyze(
                graph, sources, &refined_config.mesh, &refined_document,
                &summary.selected_error, report)) {
            quality_wrap_report(
                report, "quality_refined_analysis",
                "refined surface analysis");
            core_mesh_asset_runtime_document_free(&baseline_document);
            core_mesh_asset_runtime_document_free(&refined_document);
            return false;
        }
        improvement =
            summary.baseline_error.composite_score > 1.0e-15
                ? 1.0 - summary.selected_error.composite_score /
                    summary.baseline_error.composite_score
                : 0.0;
        summary.refinement_improvement_ratio = improvement;
        if (improvement <
                config->minimum_refinement_improvement_ratio ||
            summary.selected_error.signed_distance_rms_units >
                summary.baseline_error.signed_distance_rms_units ||
            summary.selected_error.face_gradient_rms_degrees >
                summary.baseline_error.face_gradient_rms_degrees) {
            core_mesh_asset_runtime_document_free(&baseline_document);
            core_mesh_asset_runtime_document_free(&refined_document);
            quality_report(
                report, PROCEDURAL_SOLID_MESH_STATUS_TOPOLOGY,
                "quality_refinement",
                "error-driven refinement did not improve its quality metrics");
            return false;
        }
        summary.refinement_selected = true;
        selected_document = &refined_document;
        selected_local = &refined_local;
    }
    summary.local = *selected_local;
    summary.selected_cells =
        selected_local->passes[selected_local->selected_pass].fine_cells;
    if (!ProceduralSolidCrease_Optimize(
            graph, sources, &config->crease, &config->local.mesh,
            selected_document, &summary.local.selected_mesh,
            &summary.crease, report)) {
        quality_wrap_report(
            report, "quality_crease", "crease QEF optimization");
        core_mesh_asset_runtime_document_free(&baseline_document);
        core_mesh_asset_runtime_document_free(&refined_document);
        return false;
    }
    if (!ProceduralSolidQuality_Analyze(
            graph, sources, &config->local.mesh, selected_document,
            &summary.selected_error, report)) {
        quality_wrap_report(
            report, "quality_selected_analysis",
            "selected surface analysis");
        core_mesh_asset_runtime_document_free(&baseline_document);
        core_mesh_asset_runtime_document_free(&refined_document);
        return false;
    }
    if (summary.selected_error.signed_distance_rms_units >
            summary.baseline_error.signed_distance_rms_units ||
        summary.selected_error.composite_score >
            summary.baseline_error.composite_score) {
        quality_report(
            report, PROCEDURAL_SOLID_MESH_STATUS_TOPOLOGY,
            "quality_final_error",
            "feature positioning did not preserve the selected quality gain");
        core_mesh_asset_runtime_document_free(&baseline_document);
        core_mesh_asset_runtime_document_free(&refined_document);
        return false;
    }
    if (!ProceduralSolidShading_SplitCreases(
            &config->shading, selected_document,
            &summary.local.selected_mesh, &summary.shading, report)) {
        quality_wrap_report(
            report, "quality_shading", "crease normal splitting");
        core_mesh_asset_runtime_document_free(&baseline_document);
        core_mesh_asset_runtime_document_free(&refined_document);
        return false;
    }
    summary.local.passes[summary.local.selected_pass].mesh =
        summary.local.selected_mesh;
    if (summary.refinement_selected) {
        core_mesh_asset_runtime_document_free(&baseline_document);
        *out_document = refined_document;
        core_mesh_asset_runtime_document_init(&refined_document);
    } else {
        core_mesh_asset_runtime_document_free(&refined_document);
        *out_document = baseline_document;
        core_mesh_asset_runtime_document_init(&baseline_document);
    }
    *out_summary = summary;
    return true;
}
