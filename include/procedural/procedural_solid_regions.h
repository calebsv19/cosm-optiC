#ifndef PROCEDURAL_SOLID_REGIONS_H
#define PROCEDURAL_SOLID_REGIONS_H

#include "procedural/procedural_solid_mesh.h"

#define PROCEDURAL_SOLID_REGION_MAX 128u

typedef struct ProceduralSolidRegionRecord {
    char region_id[64];
    ProceduralSolidRegionKind kind;
    char primary_node_id[PROCEDURAL_SOLID_GRAPH_ID_CAPACITY];
    char secondary_node_id[PROCEDURAL_SOLID_GRAPH_ID_CAPACITY];
    size_t triangle_count;
} ProceduralSolidRegionRecord;

typedef struct ProceduralSolidRegionSummary {
    size_t region_count;
    size_t retained_triangle_count;
    size_t cut_triangle_count;
    size_t blend_triangle_count;
    ProceduralSolidRegionRecord regions[PROCEDURAL_SOLID_REGION_MAX];
    char region_digest_sha256[PROCEDURAL_SOLID_MESH_DIGEST_CAPACITY];
} ProceduralSolidRegionSummary;

bool ProceduralSolidRegions_Assign(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    const ProceduralSolidMeshConfig *mesh_config,
    CoreMeshAssetRuntimeDocument *document,
    ProceduralSolidMeshSummary *mesh_summary,
    ProceduralSolidRegionSummary *out_summary,
    ProceduralSolidMeshReport *report);

#endif
