#include "procedural/procedural_surface_feature_relief_shell.h"

#include "procedural/procedural_surface_prism_binding.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct ReliefEvaluationContext {
    ProceduralSurfacePrismBindingContext binding;
    const ProceduralSurfaceFeatureFieldV1 *field;
    const ProceduralSurfaceRecipeV1 *recipe;
    const ProceduralSurfaceCageContract *cage;
    const char *selected_face_name;
    double relief_scale;
    uint64_t maximum_candidates_considered;
} ReliefEvaluationContext;

static void report_set(
    ProceduralSurfaceFeatureReliefShellReport *report,
    ProceduralSurfaceFeatureReliefShellStatus status,
    const char *field,
    const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = status;
    snprintf(report->field, sizeof(report->field), "%s", field ? field : "");
    snprintf(report->message, sizeof(report->message), "%s",
             message ? message : "");
}

static double dot(
    ProceduralSurfaceFeatureVec3 a,
    ProceduralSurfaceFeatureVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static double clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static bool sha256_valid(const char *value) {
    if (!value || strlen(value) != 64u) return false;
    for (size_t i = 0u; i < 64u; ++i) {
        const char c = value[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }
    return true;
}

static bool sample_all_bounded(
    ReliefEvaluationContext *context,
    ProceduralSurfaceFeatureVec3 position,
    ProceduralSurfaceFeatureVec3 normal,
    ProceduralSurfaceFeatureSampleV1 *out_sample) {
    double best_coverage = 0.0;
    if (!context || !out_sample) return false;
    memset(out_sample, 0, sizeof(*out_sample));
    context->maximum_candidates_considered =
        context->maximum_candidates_considered > context->field->feature_count
            ? context->maximum_candidates_considered
            : (uint64_t)context->field->feature_count;
    for (size_t i = 0u; i < context->field->feature_count; ++i) {
        const ProceduralSurfaceFeatureRootV1 *root =
            &context->field->features[i];
        const ProceduralSurfaceFeatureVec3 delta = {
            position.x - root->position.x,
            position.y - root->position.y,
            position.z - root->position.z};
        double local_x;
        double local_y;
        double cosine;
        double sine;
        double u;
        double v;
        double q;
        double edge;
        double coverage;
        if (dot(normal, root->normal) <
            context->field->normal_compatibility_cosine) {
            continue;
        }
        local_x = dot(delta, root->tangent);
        local_y = dot(delta, root->bitangent);
        cosine = cos(root->rotation);
        sine = sin(root->rotation);
        u = (local_x * cosine + local_y * sine) / root->radius;
        v = (-local_x * sine + local_y * cosine) /
            (root->radius * root->aspect);
        q = sqrt(u * u + v * v);
        if (q > 1.0) continue;
        ++out_sample->candidates_considered;
        edge = clamp01((1.0 - q) / fmax(root->edge_softness, 1.0e-9));
        coverage = root->edge_softness > 0.0
            ? edge * edge * (3.0 - 2.0 * edge)
            : 1.0;
        if (coverage > best_coverage) {
            best_coverage = coverage;
            out_sample->coverage = coverage;
            out_sample->interior = clamp01(
                (1.0 - q - root->rim_width) /
                fmax(1.0 - root->rim_width, 1.0e-9));
            out_sample->rim = clamp01(coverage - out_sample->interior);
            out_sample->height_or_depth =
                root->height_or_depth * coverage * (1.0 - 0.35 * q);
            out_sample->feature_id = root->feature_id;
            out_sample->direction = root->tangent;
        }
    }
    return true;
}

static bool evaluate_relief(
    const void *opaque_context,
    ProceduralSurfaceFieldPoint3D point,
    ProceduralSurfaceFieldBudget *budget,
    ProceduralSurfaceFieldOutput *out_field,
    ProceduralSurfaceFieldReport *report) {
    ReliefEvaluationContext *context =
        (ReliefEvaluationContext *)opaque_context;
    ProceduralSurfaceFeatureSampleV1 sample;
    ProceduralSurfaceFeatureVec3 position = {point.x, point.y, point.z};
    ProceduralSurfaceFeatureVec3 normal;
    ProceduralSurfaceFieldPoint3D nominal_normal;
    const char *surface_group = "";
    if (!context || !out_field ||
        !ProceduralSurfacePrismBinding_EvaluateLegacy(
            &context->binding, point, budget, out_field, report)) {
        return false;
    }
    nominal_normal = ProceduralSurfacePrismBinding_NominalNormal(
        context->cage, point, &surface_group);
    out_field->height = 0.0;
    if (strcmp(surface_group, context->selected_face_name) != 0) return true;
    normal = (ProceduralSurfaceFeatureVec3){
        nominal_normal.x, nominal_normal.y, nominal_normal.z};
    if (!sample_all_bounded(context, position, normal, &sample)) return false;
    out_field->height =
        sample.height_or_depth * context->relief_scale /
        context->recipe->displacement_amplitude_units;
    if (!isfinite(out_field->height) || fabs(out_field->height) > 1.0) {
        if (report) {
            memset(report, 0, sizeof(*report));
            report->status = PROCEDURAL_SURFACE_FIELD_STATUS_NON_FINITE_OUTPUT;
            snprintf(report->field, sizeof(report->field), "height");
            snprintf(report->message, sizeof(report->message),
                     "signed feature relief exceeds normalized displacement range");
        }
        return false;
    }
    return true;
}

bool ProceduralSurfaceFeatureReliefShell_Compile(
    const ProceduralSurfaceFeatureReliefShellRequest *request,
    ProceduralSurfaceFieldBudget *field_budget,
    ProceduralSurfacePrismMeshBuffers *buffers,
    ProceduralSurfacePrismMeshRequirements *out_requirements,
    ProceduralSurfacePrismMeshSummary *out_summary,
    ProceduralSurfaceFeatureReliefShellReceipt *out_receipt,
    ProceduralSurfaceFeatureReliefShellReport *report) {
    ReliefEvaluationContext context;
    ProceduralSurfaceBindingReport binding_report;
    ProceduralSurfaceSelectedFaceShellReport shell_report;
    ProceduralSurfaceSelectedFaceShellReceipt shell_receipt;
    ProceduralSurfaceFeatureReliefShellReceipt receipt;
    double maximum_requested_relief = 0.0;
    bool has_nonzero_feature = false;
    report_set(report, PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_OK,
               "", "ok");
    if (!request || !field_budget || !buffers || !out_requirements ||
        !out_summary || !out_receipt || !request->feature_field ||
        !request->selected_face_shell.cage ||
        !request->selected_face_shell.recipe ||
        !request->selected_face_shell.graph ||
        !request->selected_face_shell.binding) {
        report_set(report,
                   PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_NULL_ARGUMENT,
                   "arguments", "request, field, buffers, and outputs are required");
        return false;
    }
    if (!ProceduralSurfaceFeatureFieldV1_Validate(request->feature_field)) {
        report_set(report,
                   PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_FIELD,
                   "feature_field", "surface feature field is invalid");
        return false;
    }
    if (!sha256_valid(request->expected_source_mesh_digest_sha256) ||
        strcmp(request->expected_source_mesh_digest_sha256,
               request->feature_field->source_mesh_digest_sha256) != 0) {
        report_set(report,
                   PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_SOURCE_IDENTITY,
                   "source_mesh_digest_sha256",
                   "feature field does not match the expected source mesh");
        return false;
    }
    if (!isfinite(request->relief_scale) || !(request->relief_scale > 0.0) ||
        !(request->selected_face_shell.recipe->displacement_amplitude_units >
          0.0)) {
        report_set(report,
                   PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_RANGE,
                   "relief_scale",
                   "positive relief scale and displacement amplitude are required");
        return false;
    }
    memset(&receipt, 0, sizeof(receipt));
    receipt.schema_version =
        PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_SCHEMA_VERSION;
    receipt.minimum_authored_height_or_depth_units = INFINITY;
    receipt.maximum_authored_height_or_depth_units = -INFINITY;
    receipt.minimum_emitted_displacement_units = INFINITY;
    receipt.maximum_emitted_displacement_units = -INFINITY;
    receipt.feature_count = request->feature_field->feature_count;
    receipt.relief_scale = request->relief_scale;
    for (size_t i = 0u; i < request->feature_field->feature_count; ++i) {
        const double value =
            request->feature_field->features[i].height_or_depth;
        receipt.minimum_authored_height_or_depth_units = fmin(
            receipt.minimum_authored_height_or_depth_units, value);
        receipt.maximum_authored_height_or_depth_units = fmax(
            receipt.maximum_authored_height_or_depth_units, value);
        maximum_requested_relief = fmax(
            maximum_requested_relief, fabs(value * request->relief_scale));
        if (value < 0.0) {
            ++receipt.negative_depth_feature_count;
            has_nonzero_feature = true;
        } else if (value > 0.0) {
            ++receipt.positive_height_feature_count;
            has_nonzero_feature = true;
        } else {
            ++receipt.zero_height_feature_count;
        }
    }
    if (!has_nonzero_feature ||
        maximum_requested_relief >
            request->selected_face_shell.recipe->displacement_amplitude_units) {
        report_set(report,
                   PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_RANGE,
                   "height_or_depth",
                   "signed relief must be nonzero and fit the displacement amplitude");
        return false;
    }
    memset(&context, 0, sizeof(context));
    if (!ProceduralSurfacePrismBindingContext_Init(
            &context.binding, request->selected_face_shell.cage,
            request->selected_face_shell.binding,
            request->selected_face_shell.graph, &binding_report)) {
        report_set(report,
                   PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_BINDING,
                   binding_report.field, binding_report.message);
        return false;
    }
    context.field = request->feature_field;
    context.recipe = request->selected_face_shell.recipe;
    context.cage = request->selected_face_shell.cage;
    context.selected_face_name = ProceduralSurfacePrismFace_Name(
        request->selected_face_shell.selected_face);
    context.relief_scale = request->relief_scale;
    if (!ProceduralSurfaceSelectedFaceShell_CompileWithEvaluator(
            &request->selected_face_shell, evaluate_relief, &context,
            ProceduralSurfacePrismBinding_ResolveDisplacementDirection,
            &context.binding, field_budget, buffers, out_requirements,
            out_summary, &shell_receipt, &shell_report)) {
        report_set(report,
                   PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_GENERATION,
                   shell_report.field, shell_report.message);
        return false;
    }
    for (size_t i = 0u; i < buffers->vertex_count; ++i) {
        const double displacement = buffers->vertices[i].displacement_units;
        if (displacement < 0.0) {
            ++receipt.negatively_displaced_vertex_count;
            receipt.minimum_emitted_displacement_units = fmin(
                receipt.minimum_emitted_displacement_units, displacement);
        } else if (displacement > 0.0) {
            ++receipt.positively_displaced_vertex_count;
            receipt.maximum_emitted_displacement_units = fmax(
                receipt.maximum_emitted_displacement_units, displacement);
        }
    }
    if (receipt.negatively_displaced_vertex_count == 0u &&
        receipt.positively_displaced_vertex_count == 0u) {
        report_set(report,
                   PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_RECEIPT,
                   "displacement",
                   "no signed feature reached the refined selected-face vertices");
        return false;
    }
    if (receipt.negatively_displaced_vertex_count == 0u) {
        receipt.minimum_emitted_displacement_units = 0.0;
    }
    if (receipt.positively_displaced_vertex_count == 0u) {
        receipt.maximum_emitted_displacement_units = 0.0;
    }
    receipt.selected_face_shell = shell_receipt;
    receipt.maximum_candidates_considered_per_vertex =
        context.maximum_candidates_considered;
    snprintf(receipt.source_mesh_digest_sha256,
             sizeof(receipt.source_mesh_digest_sha256), "%s",
             request->expected_source_mesh_digest_sha256);
    if (!ProceduralSurfaceFeatureFieldV1_Digest(
            request->feature_field, receipt.feature_field_digest_sha256)) {
        report_set(report,
                   PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_RECEIPT,
                   "feature_field_digest_sha256",
                   "unable to digest the feature field");
        return false;
    }
    receipt.feature_source_identity_bound = true;
    receipt.one_coherent_derived_shell =
        out_summary->boundary_edge_count == 0u &&
        out_summary->connected_component_count == 1u &&
        out_summary->euler_characteristic == 2;
    if (!receipt.one_coherent_derived_shell ||
        receipt.maximum_candidates_considered_per_vertex >
            PROCEDURAL_SURFACE_FEATURE_FIELD_MAX_FEATURES) {
        report_set(report,
                   PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_RECEIPT,
                   "receipt", "signed relief shell failed topology or bound checks");
        return false;
    }
    *out_receipt = receipt;
    return true;
}

const char *ProceduralSurfaceFeatureReliefShellStatus_Name(
    ProceduralSurfaceFeatureReliefShellStatus status) {
    switch (status) {
        case PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_OK: return "ok";
        case PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_NULL_ARGUMENT:
            return "null_argument";
        case PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_FIELD: return "field";
        case PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_SOURCE_IDENTITY:
            return "source_identity";
        case PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_RANGE: return "range";
        case PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_BINDING:
            return "binding";
        case PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_GENERATION:
            return "generation";
        case PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_STATUS_RECEIPT:
            return "receipt";
    }
    return "unknown";
}
