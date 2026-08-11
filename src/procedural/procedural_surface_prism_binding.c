#include "procedural/procedural_surface_prism_binding.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static bool finite_positive(double value) {
    return isfinite(value) && value > 0.0;
}

bool ProceduralSurfacePrismBindingContext_Init(
    ProceduralSurfacePrismBindingContext *context,
    const ProceduralSurfaceCageContract *cage,
    const ProceduralSurfaceBindingV1 *binding,
    const ProceduralSurfaceFieldGraphV1 *graph,
    ProceduralSurfaceBindingReport *report) {
    if (!context || !cage || !binding || !graph ||
        cage->kind != PROCEDURAL_SURFACE_CAGE_RECTANGULAR_PRISM ||
        !finite_positive(cage->width_units) ||
        !finite_positive(cage->height_units) ||
        !finite_positive(cage->depth_units) ||
        !ProceduralSurfaceBindingV1_Validate(binding, graph, report)) {
        return false;
    }
    *context = (ProceduralSurfacePrismBindingContext){
        .cage = cage, .binding = binding, .graph = graph};
    return true;
}

ProceduralSurfaceFieldPoint3D ProceduralSurfacePrismBinding_NominalNormal(
    const ProceduralSurfaceCageContract *cage,
    ProceduralSurfaceFieldPoint3D point,
    const char **out_surface_group_id) {
    static const char *const names[] = {
        "negative_x", "positive_x", "negative_y",
        "positive_y", "negative_z", "positive_z"};
    ProceduralSurfaceFieldPoint3D normal = {0.0, 0.0, 0.0};
    const double half[] = {
        cage ? cage->width_units * 0.5 : 0.0,
        cage ? cage->height_units * 0.5 : 0.0,
        cage ? cage->depth_units * 0.5 : 0.0};
    const double values[] = {point.x, point.y, point.z};
    const double scale =
        cage ? fmax(cage->width_units,
                    fmax(cage->height_units, cage->depth_units)) : 1.0;
    const double tolerance = fmax(1.0e-9, scale * 1.0e-9);
    size_t boundary_count = 0u;
    size_t single_index = 0u;
    if (out_surface_group_id) *out_surface_group_id = "";
    if (!cage) return normal;
    for (size_t axis = 0u; axis < 3u; ++axis) {
        if (fabs(fabs(values[axis]) - half[axis]) <= tolerance) {
            const double sign = values[axis] < 0.0 ? -1.0 : 1.0;
            if (axis == 0u) normal.x = sign;
            if (axis == 1u) normal.y = sign;
            if (axis == 2u) normal.z = sign;
            single_index = (axis * 2u) + (sign > 0.0 ? 1u : 0u);
            ++boundary_count;
        }
    }
    {
        const double length =
            sqrt(normal.x * normal.x + normal.y * normal.y +
                 normal.z * normal.z);
        if (length > 0.0) {
            normal.x /= length;
            normal.y /= length;
            normal.z /= length;
        }
    }
    if (out_surface_group_id && boundary_count == 1u) {
        *out_surface_group_id = names[single_index];
    }
    return normal;
}

bool ProceduralSurfacePrismBinding_EvaluateSample(
    const ProceduralSurfacePrismBindingContext *context,
    ProceduralSurfaceFieldPoint3D point,
    ProceduralSurfaceFieldBudget *budget,
    ProceduralSurfaceBoundSample *out_sample,
    ProceduralSurfaceBindingReport *report) {
    const char *surface_group_id = "";
    ProceduralSurfaceFieldPoint3D normal;
    if (!context || !context->cage || !context->binding ||
        !context->graph) return false;
    normal = ProceduralSurfacePrismBinding_NominalNormal(
        context->cage, point, &surface_group_id);
    return ProceduralSurfaceBinding_Evaluate(
        context->binding, context->graph, point, normal, surface_group_id,
        budget, out_sample, report);
}

bool ProceduralSurfacePrismBinding_ResolveDisplacementDirection(
    const void *raw_context,
    ProceduralSurfaceFieldPoint3D point,
    ProceduralSurfaceFieldPoint3D source_normal,
    ProceduralSurfaceFieldPoint3D *out_direction) {
    const ProceduralSurfacePrismBindingContext *context = raw_context;
    (void)point;
    if (!context || !context->binding || !out_direction) return false;
    *out_direction = ProceduralSurfaceBinding_DisplacementDirection(
        context->binding, source_normal, source_normal);
    return true;
}

bool ProceduralSurfacePrismBinding_EvaluateLegacy(
    const void *context,
    ProceduralSurfaceFieldPoint3D point,
    ProceduralSurfaceFieldBudget *budget,
    ProceduralSurfaceFieldOutput *out_field,
    ProceduralSurfaceFieldReport *report) {
    ProceduralSurfaceBoundSample sample;
    ProceduralSurfaceBindingReport binding_report = {
        .status = PROCEDURAL_SURFACE_BINDING_STATUS_NULL_ARGUMENT,
        .field = "context",
        .message = "prism binding context is invalid"
    };
    if (!out_field ||
        !ProceduralSurfacePrismBinding_EvaluateSample(
            context, point, budget, &sample, &binding_report)) {
        if (report) {
            memset(report, 0, sizeof(*report));
            report->status =
                binding_report.status ==
                        PROCEDURAL_SURFACE_BINDING_STATUS_EVALUATION
                    ? PROCEDURAL_SURFACE_FIELD_STATUS_NON_FINITE_OUTPUT
                    : PROCEDURAL_SURFACE_FIELD_STATUS_RECIPE;
            snprintf(report->field, sizeof(report->field), "%s",
                     binding_report.field);
            snprintf(report->message, sizeof(report->message), "%s",
                     binding_report.message);
        }
        return false;
    }
    *out_field = sample.legacy_field;
    if (report) {
        memset(report, 0, sizeof(*report));
        report->status = PROCEDURAL_SURFACE_FIELD_STATUS_OK;
        snprintf(report->message, sizeof(report->message), "ok");
    }
    return true;
}
