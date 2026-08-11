#include "procedural/procedural_surface_selected_face_shell.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static void report_set(
    ProceduralSurfaceSelectedFaceShellReport *report,
    ProceduralSurfaceSelectedFaceShellStatus status,
    const char *field,
    const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = status;
    snprintf(report->field, sizeof(report->field), "%s", field ? field : "");
    snprintf(report->message, sizeof(report->message), "%s",
             message ? message : "");
}

static bool identity_valid(const char *value) {
    const size_t length = value ? strlen(value) : 0u;
    return length > 0u &&
           length < PROCEDURAL_SURFACE_SELECTED_FACE_ASSET_ID_CAPACITY;
}

bool ProceduralSurfacePrismFace_Parse(
    const char *name,
    ProceduralSurfacePrismFace *out_face) {
    if (!name || !out_face) return false;
    for (int face = 0; face < PROCEDURAL_SURFACE_PRISM_FACE_COUNT; ++face) {
        if (strcmp(name, ProceduralSurfacePrismFace_Name(
                             (ProceduralSurfacePrismFace)face)) == 0) {
            *out_face = (ProceduralSurfacePrismFace)face;
            return true;
        }
    }
    return false;
}

static double displacement_magnitude(
    const ProceduralSurfacePrismVertex *vertex) {
    const double dx = vertex->position.x - vertex->cage_position.x;
    const double dy = vertex->position.y - vertex->cage_position.y;
    const double dz = vertex->position.z - vertex->cage_position.z;
    return sqrt(dx * dx + dy * dy + dz * dz);
}

bool ProceduralSurfaceSelectedFaceShell_Compile(
    const ProceduralSurfaceSelectedFaceShellRequest *request,
    ProceduralSurfaceFieldBudget *field_budget,
    ProceduralSurfacePrismMeshBuffers *buffers,
    ProceduralSurfacePrismMeshRequirements *out_requirements,
    ProceduralSurfacePrismMeshSummary *out_summary,
    ProceduralSurfaceSelectedFaceShellReceipt *out_receipt,
    ProceduralSurfaceSelectedFaceShellReport *report) {
    return ProceduralSurfaceSelectedFaceShell_CompileWithEvaluator(
        request, NULL, NULL, NULL, NULL, field_budget, buffers,
        out_requirements, out_summary, out_receipt, report);
}

bool ProceduralSurfaceSelectedFaceShell_CompileWithEvaluator(
    const ProceduralSurfaceSelectedFaceShellRequest *request,
    ProceduralSurfacePrismFieldEvaluator evaluator,
    const void *evaluator_context,
    ProceduralSurfacePrismDisplacementDirectionResolver direction_resolver,
    const void *direction_context,
    ProceduralSurfaceFieldBudget *field_budget,
    ProceduralSurfacePrismMeshBuffers *buffers,
    ProceduralSurfacePrismMeshRequirements *out_requirements,
    ProceduralSurfacePrismMeshSummary *out_summary,
    ProceduralSurfaceSelectedFaceShellReceipt *out_receipt,
    ProceduralSurfaceSelectedFaceShellReport *report) {
    ProceduralSurfacePrismBindingContext binding_context;
    ProceduralSurfaceBindingReport binding_report;
    ProceduralSurfacePrismMeshReport mesh_report;
    ProceduralSurfaceRecipeReport recipe_report;
    ProceduralSurfaceFieldGraphReport graph_report;
    ProceduralSurfacePrismMeshRequirements requirements;
    ProceduralSurfacePrismMeshSummary summary;
    ProceduralSurfaceSelectedFaceShellReceipt receipt;
    const char *selected_name;

    report_set(report, PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_OK, "",
               "ok");
    if (!request || !field_budget || !buffers || !out_requirements ||
        !out_summary || !out_receipt || !request->cage || !request->recipe ||
        !request->graph || !request->binding) {
        report_set(report,
                   PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_NULL_ARGUMENT,
                   "arguments", "request, buffers, budget, and outputs are required");
        return false;
    }
    if (!identity_valid(request->source_asset_id) ||
        !identity_valid(request->derived_asset_id) ||
        strcmp(request->source_asset_id, request->derived_asset_id) == 0) {
        report_set(report,
                   PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_IDENTITY,
                   "asset_id",
                   "distinct non-empty source and derived asset ids are required");
        return false;
    }
    if (request->selected_face < PROCEDURAL_SURFACE_PRISM_FACE_NEGATIVE_X ||
        request->selected_face >= PROCEDURAL_SURFACE_PRISM_FACE_COUNT) {
        report_set(report,
                   PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_SELECTION,
                   "selected_face", "selected face is invalid");
        return false;
    }
    selected_name = ProceduralSurfacePrismFace_Name(request->selected_face);
    if (request->binding->selector !=
            PROCEDURAL_SURFACE_SELECTOR_SURFACE_GROUP ||
        strcmp(request->binding->surface_group_id, selected_name) != 0) {
        report_set(report,
                   PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_BINDING,
                   "binding.selector",
                   "PSG-18 requires an exact surface_group binding matching selected_face");
        return false;
    }
    if (!ProceduralSurfacePrismBindingContext_Init(
            &binding_context, request->cage, request->binding, request->graph,
            &binding_report)) {
        report_set(report,
                   PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_BINDING,
                   binding_report.field, binding_report.message);
        return false;
    }
    if (!ProceduralSurfacePrismMesh_DeriveRequirements(
            request->cage, request->recipe, request->quality, &requirements,
            &mesh_report)) {
        report_set(report,
                   PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_REQUIREMENTS,
                   mesh_report.field, mesh_report.message);
        return false;
    }
    if (!ProceduralSurfacePrismMesh_GenerateWithEvaluatorAndDirection(
            request->cage, request->recipe, request->quality,
            evaluator ? evaluator : ProceduralSurfacePrismBinding_EvaluateLegacy,
            evaluator ? evaluator_context : (const void *)&binding_context,
            direction_resolver
                ? direction_resolver
                : ProceduralSurfacePrismBinding_ResolveDisplacementDirection,
            direction_resolver
                ? direction_context
                : (const void *)&binding_context,
            field_budget, buffers, &summary, &mesh_report)) {
        report_set(report,
                   PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_GENERATION,
                   mesh_report.field, mesh_report.message);
        return false;
    }

    memset(&receipt, 0, sizeof(receipt));
    receipt.schema_version =
        PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_SCHEMA_VERSION;
    snprintf(receipt.source_asset_id, sizeof(receipt.source_asset_id), "%s",
             request->source_asset_id);
    snprintf(receipt.derived_asset_id, sizeof(receipt.derived_asset_id), "%s",
             request->derived_asset_id);
    receipt.selected_face = request->selected_face;
    receipt.source_triangle_count = 12u;
    receipt.source_selected_face_triangle_count = 2u;
    receipt.derived_vertex_count = summary.vertex_count;
    receipt.derived_triangle_count = summary.triangle_count;
    receipt.source_semantic_identity_retained = true;
    receipt.replaceable_derived_geometry = true;
    for (size_t i = 0u; i < buffers->triangle_count; ++i) {
        if (buffers->triangles[i].surface_group == request->selected_face) {
            ++receipt.derived_selected_face_triangle_count;
        } else {
            ++receipt.closure_support_triangle_count;
        }
    }
    for (size_t i = 0u; i < buffers->vertex_count; ++i) {
        const ProceduralSurfacePrismVertex *vertex = &buffers->vertices[i];
        const char *group_name = "";
        const double displacement = displacement_magnitude(vertex);
        (void)ProceduralSurfacePrismBinding_NominalNormal(
            request->cage, vertex->cage_position, &group_name);
        if (strcmp(group_name, selected_name) == 0) {
            receipt.maximum_selected_face_absolute_displacement_units =
                fmax(receipt.maximum_selected_face_absolute_displacement_units,
                     displacement);
        } else {
            receipt.maximum_unselected_face_absolute_displacement_units =
                fmax(receipt.maximum_unselected_face_absolute_displacement_units,
                     displacement);
        }
    }
    receipt.geometry_displacement_active =
        receipt.maximum_selected_face_absolute_displacement_units > 0.0;
    if (receipt.derived_selected_face_triangle_count <=
            receipt.source_selected_face_triangle_count ||
        receipt.derived_selected_face_triangle_count +
                receipt.closure_support_triangle_count !=
            receipt.derived_triangle_count ||
        receipt.maximum_unselected_face_absolute_displacement_units != 0.0 ||
        summary.boundary_edge_count != 0u ||
        summary.connected_component_count != 1u ||
        summary.euler_characteristic != 2 ||
        summary.signed_volume_units3 <= 0.0 ||
        summary.minimum_outward_winding_dot <= 0.0 ||
        summary.maximum_edge_absolute_displacement_units != 0.0 ||
        !ProceduralSurfaceRecipeV1_Digest(
            request->recipe, receipt.recipe_digest_sha256, &recipe_report) ||
        !ProceduralSurfaceFieldGraphV1_Digest(
            request->graph, receipt.field_graph_digest_sha256, &graph_report) ||
        !ProceduralSurfaceBindingV1_Digest(
            request->binding, receipt.binding_digest_sha256, &binding_report)) {
        report_set(report,
                   PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_RECEIPT,
                   "receipt",
                   "derived shell failed selected-face or closed-manifold acceptance");
        return false;
    }
    snprintf(receipt.mesh_digest_sha256,
             sizeof(receipt.mesh_digest_sha256), "%s",
             summary.mesh_digest_sha256);
    *out_requirements = requirements;
    *out_summary = summary;
    *out_receipt = receipt;
    return true;
}

const char *ProceduralSurfaceSelectedFaceShellStatus_Name(
    ProceduralSurfaceSelectedFaceShellStatus status) {
    switch (status) {
        case PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_OK: return "ok";
        case PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_NULL_ARGUMENT:
            return "null_argument";
        case PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_IDENTITY:
            return "identity";
        case PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_SELECTION:
            return "selection";
        case PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_BINDING:
            return "binding";
        case PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_REQUIREMENTS:
            return "requirements";
        case PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_GENERATION:
            return "generation";
        case PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_STATUS_RECEIPT:
            return "receipt";
    }
    return "unknown";
}
