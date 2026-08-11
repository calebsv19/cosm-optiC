#include "procedural/procedural_surface_topology_contract.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

static bool checked_add_u64(uint64_t a, uint64_t b, uint64_t *out_value) {
    if (!out_value || UINT64_MAX - a < b) return false;
    *out_value = a + b;
    return true;
}

static bool checked_mul_u64(uint64_t a, uint64_t b, uint64_t *out_value) {
    if (!out_value || (a != 0u && b > UINT64_MAX / a)) return false;
    *out_value = a * b;
    return true;
}

static bool subdivision_count(double extent,
                              double target_edge_length,
                              uint32_t *out_count) {
    double count = 0.0;
    if (!out_count || !isfinite(extent) || !isfinite(target_edge_length) ||
        extent <= 0.0 || target_edge_length <= 0.0) {
        return false;
    }
    count = ceil(extent / target_edge_length);
    if (!isfinite(count) || count < 1.0 || count > (double)UINT32_MAX) {
        return false;
    }
    *out_count = (uint32_t)count;
    return true;
}

static bool derive_plane(const ProceduralSurfaceCageContract *cage,
                         ProceduralSurfaceTopologyExpectation *out) {
    uint64_t nx = 0u;
    uint64_t ny = 0u;
    uint64_t horizontal_edges = 0u;
    uint64_t vertical_edges = 0u;
    uint64_t diagonal_edges = 0u;
    uint64_t edge_count = 0u;
    uint64_t cell_count = 0u;

    if (!subdivision_count(cage->width_units,
                           cage->target_edge_length_units,
                           &out->subdivisions_x) ||
        !subdivision_count(cage->height_units,
                           cage->target_edge_length_units,
                           &out->subdivisions_y)) {
        return false;
    }
    nx = out->subdivisions_x;
    ny = out->subdivisions_y;
    if (!checked_mul_u64(nx + 1u, ny + 1u, &out->vertex_count) ||
        !checked_mul_u64(nx, ny, &cell_count) ||
        !checked_mul_u64(cell_count, 2u, &out->triangle_count) ||
        !checked_mul_u64(nx, ny + 1u, &horizontal_edges) ||
        !checked_mul_u64(nx + 1u, ny, &vertical_edges) ||
        !checked_mul_u64(nx, ny, &diagonal_edges) ||
        !checked_add_u64(horizontal_edges, vertical_edges, &edge_count) ||
        !checked_add_u64(edge_count, diagonal_edges, &out->unique_edge_count)) {
        return false;
    }
    if (!checked_add_u64(nx, ny, &out->boundary_edge_count) ||
        !checked_mul_u64(out->boundary_edge_count,
                         2u,
                         &out->boundary_edge_count)) {
        return false;
    }
    out->connected_component_count = 1u;
    out->surface_group_count = 1u;
    out->euler_characteristic = 1;
    out->requires_outward_winding = false;
    out->requires_positive_signed_volume = false;
    out->requires_two_incident_triangles_per_edge = false;
    return true;
}

static bool derive_rectangular_prism(
    const ProceduralSurfaceCageContract *cage,
    ProceduralSurfaceTopologyExpectation *out) {
    uint64_t nx = 0u;
    uint64_t ny = 0u;
    uint64_t nz = 0u;
    uint64_t all_lattice_vertices = 0u;
    uint64_t interior_vertices = 0u;
    uint64_t xy_cells = 0u;
    uint64_t xz_cells = 0u;
    uint64_t yz_cells = 0u;
    uint64_t face_cells = 0u;

    if (!subdivision_count(cage->width_units,
                           cage->target_edge_length_units,
                           &out->subdivisions_x) ||
        !subdivision_count(cage->height_units,
                           cage->target_edge_length_units,
                           &out->subdivisions_y) ||
        !subdivision_count(cage->depth_units,
                           cage->target_edge_length_units,
                           &out->subdivisions_z)) {
        return false;
    }
    nx = out->subdivisions_x;
    ny = out->subdivisions_y;
    nz = out->subdivisions_z;
    if (!checked_mul_u64(nx + 1u, ny + 1u, &all_lattice_vertices) ||
        !checked_mul_u64(all_lattice_vertices,
                         nz + 1u,
                         &all_lattice_vertices) ||
        !checked_mul_u64(nx - 1u, ny - 1u, &interior_vertices) ||
        !checked_mul_u64(interior_vertices,
                         nz - 1u,
                         &interior_vertices) ||
        all_lattice_vertices < interior_vertices) {
        return false;
    }
    out->vertex_count = all_lattice_vertices - interior_vertices;
    if (!checked_mul_u64(nx, ny, &xy_cells) ||
        !checked_mul_u64(nx, nz, &xz_cells) ||
        !checked_mul_u64(ny, nz, &yz_cells) ||
        !checked_add_u64(xy_cells, xz_cells, &face_cells) ||
        !checked_add_u64(face_cells, yz_cells, &face_cells) ||
        !checked_mul_u64(face_cells, 4u, &out->triangle_count) ||
        !checked_mul_u64(out->triangle_count,
                         3u,
                         &out->unique_edge_count)) {
        return false;
    }
    out->unique_edge_count /= 2u;
    out->boundary_edge_count = 0u;
    out->connected_component_count = 1u;
    out->surface_group_count = 6u;
    out->euler_characteristic = 2;
    out->requires_outward_winding = true;
    out->requires_positive_signed_volume = true;
    out->requires_two_incident_triangles_per_edge = true;
    return true;
}

bool ProceduralSurfaceTopologyContract_Derive(
    const ProceduralSurfaceCageContract *cage,
    ProceduralSurfaceTopologyExpectation *out_expectation) {
    ProceduralSurfaceTopologyExpectation result = {0};
    if (!cage || !out_expectation || !isfinite(cage->depth_units)) return false;
    switch (cage->kind) {
        case PROCEDURAL_SURFACE_CAGE_PLANE:
            if (cage->depth_units != 0.0 || !derive_plane(cage, &result)) {
                return false;
            }
            break;
        case PROCEDURAL_SURFACE_CAGE_RECTANGULAR_PRISM:
            if (cage->depth_units <= 0.0 ||
                !derive_rectangular_prism(cage, &result)) {
                return false;
            }
            break;
        default:
            return false;
    }
    *out_expectation = result;
    return true;
}
