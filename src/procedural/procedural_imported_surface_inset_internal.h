#ifndef PROCEDURAL_IMPORTED_SURFACE_INSET_INTERNAL_H
#define PROCEDURAL_IMPORTED_SURFACE_INSET_INTERNAL_H

#include "procedural/procedural_imported_surface_inset.h"

#include <math.h>

typedef struct InsetEdge {
    size_t lo;
    size_t hi;
    size_t midpoint;
    size_t first_triangle;
    size_t second_triangle;
    unsigned int incidence;
    bool split;
} InsetEdge;

typedef struct InsetTriangle {
    size_t a;
    size_t b;
    size_t c;
    size_t source_triangle;
} InsetTriangle;

typedef struct RefinedMesh {
    size_t vertex_count;
    CoreMeshAssetRuntimeVertex *vertices;
    double *weights;
    size_t triangle_count;
    InsetTriangle *triangles;
} RefinedMesh;

typedef struct InsetRefinementSummary {
    size_t transition_source_triangle_count;
    size_t pass_count;
    double target_boundary_edge_length_units;
    double initial_max_boundary_edge_length_units;
    double final_max_boundary_edge_length_units;
    bool converged;
} InsetRefinementSummary;

static inline CoreObjectVec3 vec_add(CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline CoreObjectVec3 vec_sub(CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static inline CoreObjectVec3 vec_scale(CoreObjectVec3 value, double scale) {
    return (CoreObjectVec3){
        value.x * scale, value.y * scale, value.z * scale};
}

static inline double vec_dot(CoreObjectVec3 a, CoreObjectVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline CoreObjectVec3 vec_cross(CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

static inline double vec_length(CoreObjectVec3 value) {
    return sqrt(vec_dot(value, value));
}

static inline bool vec_normalize(CoreObjectVec3 value, CoreObjectVec3 *out) {
    const double length = vec_length(value);
    if (!(length > 1.0e-15) || !out) return false;
    *out = vec_scale(value, 1.0 / length);
    return true;
}

void refined_free(RefinedMesh *mesh);

bool inset_collect_edges(
    const InsetTriangle *triangles,
    size_t triangle_count,
    InsetEdge **out_edges,
    size_t *out_edge_count);

InsetEdge *inset_find_edge(
    InsetEdge *edges,
    size_t edge_count,
    size_t a,
    size_t b);

bool refine_transition_band(
    const CoreMeshAssetRuntimeDocument *source,
    const ProceduralImportedSurfaceRegionV1 *region,
    const ProceduralImportedSurfaceInsetConfig *config,
    RefinedMesh *out,
    InsetRefinementSummary *out_summary);

bool classify_patch(
    const RefinedMesh *mesh,
    double threshold,
    bool **out_selected,
    InsetEdge **out_edges,
    size_t *out_edge_count,
    size_t *out_selected_count,
    size_t *out_discarded_candidate_count,
    size_t *out_selected_component_count,
    size_t *out_boundary_edge_count,
    size_t *out_boundary_loop_count,
    double *out_max_boundary_edge_length_units,
    size_t minimum_component_triangles);

bool selected_directed_edge(
    const InsetTriangle *triangle,
    size_t lo,
    size_t hi,
    size_t *out_a,
    size_t *out_b);

#endif
