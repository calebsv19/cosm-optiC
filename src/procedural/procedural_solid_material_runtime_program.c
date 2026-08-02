#include "procedural/procedural_solid_material_runtime_program.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static bool finite_barycentrics(double u, double v, double w) {
    double sum = u + v + w;
    return isfinite(u) && isfinite(v) && isfinite(w) &&
           u >= -1e-9 && v >= -1e-9 && w >= -1e-9 &&
           fabs(sum - 1.0) <= 1e-7;
}

static double interpolate(double a, double b, double c,
                          double u, double v, double w) {
    return (a * u) + (b * v) + (c * w);
}

static const ProceduralSolidAuthoredMaterialV1 *find_material(
    const ProceduralSolidMaterialRuntimeProgramV1 *program,
    const char *material_id) {
    if (!program || !material_id) return NULL;
    for (size_t i = 0u; i < program->material_count; ++i) {
        if (strcmp(program->materials[i].material_id, material_id) == 0) {
            return &program->materials[i];
        }
    }
    return NULL;
}

void ProceduralSolidMaterialRuntimeProgramV1_Init(
    ProceduralSolidMaterialRuntimeProgramV1 *program) {
    if (program) memset(program, 0, sizeof(*program));
}

void ProceduralSolidMaterialRuntimeProgramV1_Free(
    ProceduralSolidMaterialRuntimeProgramV1 *program) {
    if (!program) return;
    free(program->corner_inputs);
    memset(program, 0, sizeof(*program));
}

bool ProceduralSolidMaterialRuntimeProgramV1_Build(
    const ProceduralSolidMaterialGraphV1 *graph,
    const ProceduralSolidAuthoredMaterialV1 *materials,
    size_t material_count,
    const CoreMeshAssetRuntimeDocument *mesh,
    const char *const *region_kinds,
    ProceduralSolidMaterialRuntimeProgramV1 *out_program,
    ProceduralSolidMaterialGraphReport *report) {
    return ProceduralSolidMaterialRuntimeProgramV1_BuildWithImportedRegion(
        graph, materials, material_count, mesh, region_kinds, NULL,
        out_program, report);
}

bool ProceduralSolidMaterialRuntimeProgramV1_BuildWithImportedRegion(
    const ProceduralSolidMaterialGraphV1 *graph,
    const ProceduralSolidAuthoredMaterialV1 *materials,
    size_t material_count,
    const CoreMeshAssetRuntimeDocument *mesh,
    const char *const *region_kinds,
    const ProceduralImportedSurfaceRegionV1 *imported_region,
    ProceduralSolidMaterialRuntimeProgramV1 *out_program,
    ProceduralSolidMaterialGraphReport *report) {
    ProceduralSolidMaterialRuntimeProgramV1 program;
    size_t input_count;
    if (!graph || !materials || material_count != graph->layer_count ||
        material_count > PROCEDURAL_SOLID_MATERIAL_GRAPH_MAX_LAYERS ||
        !mesh || !out_program ||
        !ProceduralSolidMaterialGraphV1_Validate(graph, report)) {
        return false;
    }
    ProceduralSolidMaterialRuntimeProgramV1_Init(&program);
    if (mesh->triangle_count > SIZE_MAX / 3u) {
        return false;
    }
    input_count = mesh->triangle_count * 3u;
    if (
        input_count > SIZE_MAX / sizeof(*program.corner_inputs)) {
        return false;
    }
    program.corner_inputs = calloc(input_count, sizeof(*program.corner_inputs));
    if (!program.corner_inputs && input_count > 0u) return false;
    if (!ProceduralSolidMaterialGeometryCornerInputs_Build(
            mesh, region_kinds, program.corner_inputs, input_count, report)) {
        ProceduralSolidMaterialRuntimeProgramV1_Free(&program);
        return false;
    }
    if (imported_region) {
        if (!imported_region->vertex_weights ||
            imported_region->vertex_count != mesh->vertex_count ||
            imported_region->triangle_count != mesh->triangle_count) {
            ProceduralSolidMaterialRuntimeProgramV1_Free(&program);
            return false;
        }
        for (size_t triangle_index = 0u;
             triangle_index < mesh->triangle_count; ++triangle_index) {
            const CoreMeshAssetRuntimeTriangle *triangle =
                &mesh->triangles[triangle_index];
            const size_t indices[3] = {
                triangle->a, triangle->b, triangle->c};
            for (size_t corner = 0u; corner < 3u; ++corner) {
                program.corner_inputs[
                    (triangle_index * 3u) + corner].authored_region =
                        imported_region->vertex_weights[indices[corner]];
            }
        }
    }
    program.graph = *graph;
    memcpy(program.materials, materials,
           material_count * sizeof(*materials));
    program.material_count = material_count;
    program.triangle_count = mesh->triangle_count;
    program.valid = true;
    ProceduralSolidMaterialRuntimeProgramV1_Free(out_program);
    *out_program = program;
    return true;
}

bool ProceduralSolidMaterialRuntimeProgramV1_AttachFeatureField(
    ProceduralSolidMaterialRuntimeProgramV1 *program,
    const ProceduralSurfaceFeatureFieldV1 *field) {
    if (!program || !program->valid || !field ||
        !ProceduralSurfaceFeatureFieldV1_Validate(field) ||
        field->grid_index_count == 0u) return false;
    program->feature_field = *field;
    program->feature_field_valid = true;
    return true;
}

bool ProceduralSolidMaterialRuntimeProgramV1_AttachCurveField(
    ProceduralSolidMaterialRuntimeProgramV1 *program,
    const ProceduralSurfaceFeatureCurveFieldV1 *field) {
    if (!program || !program->valid || !field ||
        !ProceduralSurfaceFeatureCurveFieldV1_Validate(field) ||
        field->grid_index_count == 0u) return false;
    program->curve_field = *field;
    program->curve_field_valid = true;
    return true;
}

bool ProceduralSolidMaterialRuntimeProgramV1_EvaluateTriangleHit(
    const ProceduralSolidMaterialRuntimeProgramV1 *program,
    size_t triangle_index,
    double bary_u,
    double bary_v,
    double bary_w,
    ProceduralSolidMaterialRuntimeSampleV1 *out_sample,
    ProceduralSolidMaterialGraphReport *report) {
    ProceduralSolidMaterialRuntimeSampleV1 sample;
    const ProceduralSolidMaterialGeometryInputs *a;
    const ProceduralSolidMaterialGeometryInputs *b;
    const ProceduralSolidMaterialGeometryInputs *c;
#define INTERPOLATE_FIELD(field) \
    sample.geometry.field = interpolate( \
        a->field, b->field, c->field, bary_u, bary_v, bary_w)
    if (!program || !program->valid || !out_sample ||
        triangle_index >= program->triangle_count ||
        !finite_barycentrics(bary_u, bary_v, bary_w)) {
        return false;
    }
    memset(&sample, 0, sizeof(sample));
    a = &program->corner_inputs[(triangle_index * 3u) + 0u];
    b = &program->corner_inputs[(triangle_index * 3u) + 1u];
    c = &program->corner_inputs[(triangle_index * 3u) + 2u];
    INTERPOLATE_FIELD(height);
    INTERPOLATE_FIELD(slope);
    INTERPOLATE_FIELD(curvature);
    INTERPOLATE_FIELD(cavity);
    INTERPOLATE_FIELD(authored_region);
    INTERPOLATE_FIELD(boundary_distance);
    INTERPOLATE_FIELD(region_retained);
    INTERPOLATE_FIELD(region_cut);
    INTERPOLATE_FIELD(region_blend);
    INTERPOLATE_FIELD(object_x);
    INTERPOLATE_FIELD(object_y);
    INTERPOLATE_FIELD(object_z);
    INTERPOLATE_FIELD(normal_x);
    INTERPOLATE_FIELD(normal_y);
    INTERPOLATE_FIELD(normal_z);
#undef INTERPOLATE_FIELD
    if (program->feature_field_valid) {
        ProceduralSurfaceFeatureSampleV1 feature;
        double normal_length = sqrt(sample.geometry.normal_x * sample.geometry.normal_x +
            sample.geometry.normal_y * sample.geometry.normal_y +
            sample.geometry.normal_z * sample.geometry.normal_z);
        if (!isfinite(normal_length) || normal_length <= 1e-12) return false;
        if (!ProceduralSurfaceFeatureFieldV1_Sample(&program->feature_field,
                (ProceduralSurfaceFeatureVec3){sample.geometry.object_x,
                    sample.geometry.object_y, sample.geometry.object_z},
                (ProceduralSurfaceFeatureVec3){sample.geometry.normal_x / normal_length,
                    sample.geometry.normal_y / normal_length,
                    sample.geometry.normal_z / normal_length}, &feature)) return false;
        sample.geometry.feature_coverage = feature.coverage;
        sample.geometry.feature_interior = feature.interior;
        sample.geometry.feature_rim = feature.rim;
        sample.geometry.feature_id = (double)feature.feature_id;
    }
    if (program->curve_field_valid) {
        double normal_length = sqrt(sample.geometry.normal_x * sample.geometry.normal_x +
            sample.geometry.normal_y * sample.geometry.normal_y +
            sample.geometry.normal_z * sample.geometry.normal_z);
        if (!isfinite(normal_length) || normal_length <= 1e-12 ||
            !ProceduralSurfaceFeatureCurveFieldV1_Sample(&program->curve_field,
                (ProceduralSurfaceFeatureVec3){sample.geometry.object_x,
                    sample.geometry.object_y, sample.geometry.object_z},
                (ProceduralSurfaceFeatureVec3){sample.geometry.normal_x / normal_length,
                    sample.geometry.normal_y / normal_length,
                    sample.geometry.normal_z / normal_length}, &sample.curve_feature))
            return false;
    }
    if (!ProceduralSolidMaterialGraphV1_EvaluateWithReadback(
            &program->graph, &sample.geometry, program->materials,
            program->material_count, &sample.surface,
            sample.layer_weights, report)) {
        return false;
    }
    sample.layer_count = program->graph.layer_count;
    sample.primary_layer_weight =
        sample.layer_weights[sample.layer_count > 1u ? 1u : 0u];
    sample.surface.texture.enabled = false;
    for (size_t i = 0u; i < program->graph.layer_count; ++i) {
        const ProceduralSolidAuthoredMaterialV1 *material =
            find_material(program, program->graph.layers[i].material_id);
        if (!material) return false;
        if (material->surface.texture.enabled) {
            ProceduralSolidMaterialWeightedTextureV1 *weighted =
                &sample.textures[sample.texture_count++];
            weighted->texture = material->surface.texture;
            weighted->weight = sample.layer_weights[i];
            weighted->graph_layer_index = i;
        }
    }
    *out_sample = sample;
    return true;
}
