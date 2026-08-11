#ifndef PROCEDURAL_SURFACE_TOPOLOGY_CONTRACT_H
#define PROCEDURAL_SURFACE_TOPOLOGY_CONTRACT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum ProceduralSurfaceCageKind {
    PROCEDURAL_SURFACE_CAGE_PLANE = 1,
    PROCEDURAL_SURFACE_CAGE_RECTANGULAR_PRISM = 2
} ProceduralSurfaceCageKind;

typedef struct ProceduralSurfaceCageContract {
    ProceduralSurfaceCageKind kind;
    double width_units;
    double height_units;
    double depth_units;
    double target_edge_length_units;
} ProceduralSurfaceCageContract;

typedef struct ProceduralSurfaceTopologyExpectation {
    uint32_t subdivisions_x;
    uint32_t subdivisions_y;
    uint32_t subdivisions_z;
    uint64_t vertex_count;
    uint64_t triangle_count;
    uint64_t unique_edge_count;
    uint64_t boundary_edge_count;
    uint32_t connected_component_count;
    uint32_t surface_group_count;
    int32_t euler_characteristic;
    bool requires_outward_winding;
    bool requires_positive_signed_volume;
    bool requires_two_incident_triangles_per_edge;
} ProceduralSurfaceTopologyExpectation;

/*
 * PSG-0 freezes count and validity expectations only. These functions do not
 * generate vertices, evaluate a field, or claim that a derived mesh exists.
 */
bool ProceduralSurfaceTopologyContract_Derive(
    const ProceduralSurfaceCageContract *cage,
    ProceduralSurfaceTopologyExpectation *out_expectation);

#endif
