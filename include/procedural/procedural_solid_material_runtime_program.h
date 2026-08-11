#ifndef PROCEDURAL_SOLID_MATERIAL_RUNTIME_PROGRAM_H
#define PROCEDURAL_SOLID_MATERIAL_RUNTIME_PROGRAM_H

#include "procedural/procedural_solid_material_graph.h"
#include "procedural/procedural_solid_material_weighted_texture.h"
#include "procedural/procedural_imported_surface_region.h"
#include "procedural/procedural_surface_feature_field.h"
#include "procedural/procedural_surface_feature_curve.h"
#include "procedural/procedural_surface_wood_grain.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct ProceduralSolidMaterialRuntimeSampleV1 {
    ProceduralSolidMaterialGeometryInputs geometry;
    ProceduralSolidAuthoredMaterialSurfaceV1 surface;
    size_t layer_count;
    double layer_weights[PROCEDURAL_SOLID_MATERIAL_GRAPH_MAX_LAYERS];
    double primary_layer_weight;
    size_t texture_count;
    ProceduralSolidMaterialWeightedTextureV1
        textures[PROCEDURAL_SOLID_MATERIAL_GRAPH_MAX_LAYERS];
    ProceduralSurfaceFeatureCurveSampleV1 curve_feature;
    bool wood_grain_valid;
    ProceduralSurfaceWoodGrainSampleV1 wood_grain;
} ProceduralSolidMaterialRuntimeSampleV1;

typedef struct ProceduralSolidMaterialRuntimeProgramV1 {
    bool valid;
    ProceduralSolidMaterialGraphV1 graph;
    size_t material_count;
    ProceduralSolidAuthoredMaterialV1
        materials[PROCEDURAL_SOLID_MATERIAL_GRAPH_MAX_LAYERS];
    size_t triangle_count;
    size_t vertex_count;
    ProceduralSolidMaterialGeometryInputs *corner_inputs;
    size_t *corner_vertex_indices;
    bool feature_field_valid;
    ProceduralSurfaceFeatureFieldV1 feature_field;
    bool curve_field_valid;
    ProceduralSurfaceFeatureCurveFieldV1 curve_field;
    bool wood_grain_valid;
    ProceduralSurfaceWoodGrainFieldV1 wood_grain;
} ProceduralSolidMaterialRuntimeProgramV1;

void ProceduralSolidMaterialRuntimeProgramV1_Init(
    ProceduralSolidMaterialRuntimeProgramV1 *program);
void ProceduralSolidMaterialRuntimeProgramV1_Free(
    ProceduralSolidMaterialRuntimeProgramV1 *program);
bool ProceduralSolidMaterialRuntimeProgramV1_Build(
    const ProceduralSolidMaterialGraphV1 *graph,
    const ProceduralSolidAuthoredMaterialV1 *materials,
    size_t material_count,
    const CoreMeshAssetRuntimeDocument *mesh,
    const char *const *region_kinds,
    ProceduralSolidMaterialRuntimeProgramV1 *out_program,
    ProceduralSolidMaterialGraphReport *report);
bool ProceduralSolidMaterialRuntimeProgramV1_BuildWithImportedRegion(
    const ProceduralSolidMaterialGraphV1 *graph,
    const ProceduralSolidAuthoredMaterialV1 *materials,
    size_t material_count,
    const CoreMeshAssetRuntimeDocument *mesh,
    const char *const *region_kinds,
    const ProceduralImportedSurfaceRegionV1 *imported_region,
    ProceduralSolidMaterialRuntimeProgramV1 *out_program,
    ProceduralSolidMaterialGraphReport *report);
bool ProceduralSolidMaterialRuntimeProgramV1_AttachFeatureField(
    ProceduralSolidMaterialRuntimeProgramV1 *program,
    const ProceduralSurfaceFeatureFieldV1 *field);
bool ProceduralSolidMaterialRuntimeProgramV1_AttachNamedSelector(
    ProceduralSolidMaterialRuntimeProgramV1 *program, const char *selector_id,
    const ProceduralImportedSurfaceRegionV1 *region);
bool ProceduralSolidMaterialRuntimeProgramV1_AttachCurveField(
    ProceduralSolidMaterialRuntimeProgramV1 *program,
    const ProceduralSurfaceFeatureCurveFieldV1 *field);
bool ProceduralSolidMaterialRuntimeProgramV1_AttachWoodGrain(
    ProceduralSolidMaterialRuntimeProgramV1 *program,
    const ProceduralSurfaceWoodGrainFieldV1 *field);
bool ProceduralSolidMaterialRuntimeProgramV1_EvaluateTriangleHit(
    const ProceduralSolidMaterialRuntimeProgramV1 *program,
    size_t triangle_index,
    double bary_u,
    double bary_v,
    double bary_w,
    ProceduralSolidMaterialRuntimeSampleV1 *out_sample,
    ProceduralSolidMaterialGraphReport *report);

#endif
