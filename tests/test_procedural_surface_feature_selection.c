#include "procedural/procedural_surface_feature_selection.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    ProceduralSurfaceFeatureFieldV1 field = {0};
    ProceduralSurfaceFeatureSelectionV1 selection;
    ProceduralImportedSurfaceRegionV1 base = {0}, derived = {0};
    CoreMeshAssetRuntimeVertex vertex = {.position = {0, 0, 0},
                                         .normal = {0, 0, 1}};
    double value = 0.0;
    field.feature_count = 2u;
    field.normal_compatibility_cosine = .5;
    memset(field.source_mesh_digest_sha256, 'a', 64u);
    field.source_mesh_digest_sha256[64] = '\0';
    memset(field.authoring_digest_sha256, 'b', 64u);
    field.authoring_digest_sha256[64] = '\0';
    for (size_t i = 0u; i < 2u; ++i) {
        ProceduralSurfaceFeatureRootV1 *root = &field.features[i];
        root->barycentric[0] = 1.0;
        root->normal.z = 1.0;
        root->tangent.x = 1.0;
        root->bitangent.y = 1.0;
        root->aspect = 1.0;
        root->edge_softness = .1;
        root->rim_width = .2;
        root->feature_id = (uint32_t)(1u + i);
    }
    field.features[0].position.x = 1.0;
    field.features[0].radius = .02;
    field.features[1].radius = .2;
    base.schema_version = PROCEDURAL_IMPORTED_SURFACE_REGION_SCHEMA_VERSION;
    base.vertex_count = 1u;
    base.triangle_count = 1u;
    base.topology_unchanged = true;
    base.source_triangle_provenance_retained = true;
    snprintf(base.region_id, sizeof(base.region_id), "base");
    snprintf(base.source_mesh_digest_sha256, sizeof(base.source_mesh_digest_sha256), "mesh");
    assert(ProceduralSurfaceFeatureFieldV1_BuildIndex(&field));
    assert(ProceduralSurfaceFeatureSelectionV1_Build(&field, .1, &selection));
    assert(selection.feature_id_count == 1u &&
           ProceduralSurfaceFeatureSelectionV1_Contains(&selection, 2u) &&
           !ProceduralSurfaceFeatureSelectionV1_Contains(&selection, 1u));
    assert(ProceduralSurfaceFeatureSelectionV1_BuildCarrierValues(
        &field, &selection, &vertex, 1u, &value) && value > 0.0);
    assert(ProceduralSurfaceFeatureSelectionV1_BuildRegion(
        &base, &field, &selection, &vertex, 1u, "from_field", &derived));
    assert(derived.vertex_weights[0] > 0.0 &&
           strcmp(derived.region_id, "from_field") == 0 &&
           strcmp(derived.source_mesh_digest_sha256, "mesh") == 0 &&
           strcmp(derived.recipe_digest_sha256, selection.field_digest_sha256) == 0);
    free(derived.vertex_weights);
    puts("PSG-24C/D feature selection contract passed");
    return 0;
}
