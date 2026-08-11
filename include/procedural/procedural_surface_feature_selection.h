#ifndef PROCEDURAL_SURFACE_FEATURE_SELECTION_H
#define PROCEDURAL_SURFACE_FEATURE_SELECTION_H

#include "procedural/procedural_surface_feature_field.h"
#include "core_mesh_asset.h"
#include "procedural/procedural_imported_surface_region.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_SURFACE_FEATURE_SELECTION_MAX_IDS 256u
typedef struct ProceduralSurfaceFeatureSelectionV1 {
    char field_digest_sha256[PROCEDURAL_SURFACE_FEATURE_FIELD_DIGEST_CAPACITY];
    size_t feature_id_count;
    uint32_t feature_ids[PROCEDURAL_SURFACE_FEATURE_SELECTION_MAX_IDS];
} ProceduralSurfaceFeatureSelectionV1;
bool ProceduralSurfaceFeatureSelectionV1_Build(const ProceduralSurfaceFeatureFieldV1 *field,
    double minimum_radius, ProceduralSurfaceFeatureSelectionV1 *out_selection);
bool ProceduralSurfaceFeatureSelectionV1_BuildExplicit(
    const ProceduralSurfaceFeatureFieldV1 *field,
    const uint32_t *feature_ids, size_t feature_id_count,
    ProceduralSurfaceFeatureSelectionV1 *out_selection);
bool ProceduralSurfaceFeatureSelectionV1_Contains(const ProceduralSurfaceFeatureSelectionV1 *selection,
    uint32_t feature_id);
bool ProceduralSurfaceFeatureSelectionV1_BuildCarrierValues(
    const ProceduralSurfaceFeatureFieldV1 *field,
    const ProceduralSurfaceFeatureSelectionV1 *selection,
    const CoreMeshAssetRuntimeVertex *vertices, size_t vertex_count,
    double *out_values);
bool ProceduralSurfaceFeatureSelectionV1_BuildRegion(
    const ProceduralImportedSurfaceRegionV1 *source_region,
    const ProceduralSurfaceFeatureFieldV1 *field,
    const ProceduralSurfaceFeatureSelectionV1 *selection,
    const CoreMeshAssetRuntimeVertex *vertices, size_t vertex_count,
    const char *region_id, ProceduralImportedSurfaceRegionV1 *out_region);
#endif
