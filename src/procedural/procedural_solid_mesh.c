#include "procedural/procedural_solid_mesh.h"

#include "app/ray_tracing_sha256.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct SolidEdgeKey {
    size_t lo;
    size_t hi;
} SolidEdgeKey;

typedef struct SolidIsoVertex {
    SolidEdgeKey key;
    CoreObjectVec3 position;
} SolidIsoVertex;

typedef struct SolidTempTriangle {
    SolidEdgeKey edges[3];
} SolidTempTriangle;

typedef struct SolidMeshEdge {
    size_t lo;
    size_t hi;
    size_t incidence;
} SolidMeshEdge;

typedef struct SolidSpatialEntry {
    long long qx;
    long long qy;
    long long qz;
    size_t edge_index;
} SolidSpatialEntry;

typedef struct SolidBuild {
    SolidIsoVertex *vertices;
    size_t vertex_count;
    size_t vertex_capacity;
    SolidTempTriangle *triangles;
    size_t triangle_count;
    size_t triangle_capacity;
    const ProceduralSolidMeshConfig *config;
} SolidBuild;

static void mesh_report(ProceduralSolidMeshReport *report,
                        ProceduralSolidMeshStatus status,
                        const char *field,
                        const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = status;
    snprintf(report->field, sizeof(report->field), "%s", field ? field : "");
    snprintf(report->message, sizeof(report->message), "%s",
             message ? message : "");
}

const char *ProceduralSolidMeshStatus_Name(ProceduralSolidMeshStatus status) {
    switch (status) {
        case PROCEDURAL_SOLID_MESH_STATUS_OK: return "ok";
        case PROCEDURAL_SOLID_MESH_STATUS_NULL_ARGUMENT:
            return "null_argument";
        case PROCEDURAL_SOLID_MESH_STATUS_CONFIG: return "config";
        case PROCEDURAL_SOLID_MESH_STATUS_CAPACITY: return "capacity";
        case PROCEDURAL_SOLID_MESH_STATUS_ALLOCATION: return "allocation";
        case PROCEDURAL_SOLID_MESH_STATUS_FIELD: return "field";
        case PROCEDURAL_SOLID_MESH_STATUS_DOMAIN_CLIPPED:
            return "domain_clipped";
        case PROCEDURAL_SOLID_MESH_STATUS_EMPTY: return "empty";
        case PROCEDURAL_SOLID_MESH_STATUS_DEGENERATE: return "degenerate";
        case PROCEDURAL_SOLID_MESH_STATUS_TOPOLOGY: return "topology";
        case PROCEDURAL_SOLID_MESH_STATUS_COMPONENT_POLICY:
            return "component_policy";
        case PROCEDURAL_SOLID_MESH_STATUS_CORE_MESH: return "core_mesh";
        case PROCEDURAL_SOLID_MESH_STATUS_IDENTITY: return "identity";
    }
    return "unknown";
}

const char *ProceduralSolidCollisionAuthority_Name(
    ProceduralSolidCollisionAuthority authority) {
    switch (authority) {
        case PROCEDURAL_SOLID_COLLISION_AUTHORITY_SEMANTIC_SOURCE:
            return "semantic_source";
        case PROCEDURAL_SOLID_COLLISION_AUTHORITY_DERIVED_SHELL:
            return "derived_shell";
    }
    return "unknown";
}

void ProceduralSolidMeshConfig_Init(ProceduralSolidMeshConfig *config) {
    if (!config) return;
    *config = (ProceduralSolidMeshConfig){
        .bounds_min = {-2.0, -2.0, -2.0},
        .bounds_max = {2.0, 2.0, 2.0},
        .cells_x = 32u,
        .cells_y = 32u,
        .cells_z = 32u,
        .max_samples = 2000000u,
        .max_vertices = 1000000u,
        .max_triangles = 2000000u,
        .min_components = 1u,
        .max_components = 1u,
        .gradient_step_units = 0.001,
        .minimum_triangle_area2 = 1.0e-8,
        .require_closed_manifold = true,
        .require_positive_volume = true,
        .collision_authority =
            PROCEDURAL_SOLID_COLLISION_AUTHORITY_SEMANTIC_SOURCE,
        .active_cell_mask = NULL,
        .active_cell_mask_count = 0u};
}

static bool finite_vec(CoreObjectVec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool config_valid(const ProceduralSolidMeshConfig *config,
                         size_t *out_sample_count) {
    size_t sx;
    size_t sy;
    size_t sz;
    size_t cell_count;
    if (!config || !finite_vec(config->bounds_min) ||
        !finite_vec(config->bounds_max) ||
        !(config->bounds_max.x > config->bounds_min.x) ||
        !(config->bounds_max.y > config->bounds_min.y) ||
        !(config->bounds_max.z > config->bounds_min.z) ||
        config->cells_x < 2u || config->cells_y < 2u ||
        config->cells_z < 2u ||
        config->cells_x > 512u || config->cells_y > 512u ||
        config->cells_z > 512u ||
        config->max_samples == 0u || config->max_vertices == 0u ||
        config->max_triangles == 0u ||
        config->max_vertices > SIZE_MAX / (12u * sizeof(SolidIsoVertex)) ||
        config->max_triangles > SIZE_MAX / sizeof(SolidTempTriangle) ||
        config->min_components == 0u ||
        config->max_components < config->min_components ||
        !isfinite(config->gradient_step_units) ||
        config->gradient_step_units <= 0.0 ||
        !isfinite(config->minimum_triangle_area2) ||
        config->minimum_triangle_area2 <= 0.0 ||
        (config->collision_authority !=
             PROCEDURAL_SOLID_COLLISION_AUTHORITY_SEMANTIC_SOURCE &&
         config->collision_authority !=
             PROCEDURAL_SOLID_COLLISION_AUTHORITY_DERIVED_SHELL)) {
        return false;
    }
    if ((size_t)config->cells_x > SIZE_MAX / (size_t)config->cells_y ||
        (size_t)config->cells_x * (size_t)config->cells_y >
            SIZE_MAX / (size_t)config->cells_z) {
        return false;
    }
    cell_count = (size_t)config->cells_x *
        (size_t)config->cells_y * (size_t)config->cells_z;
    if ((config->active_cell_mask &&
         config->active_cell_mask_count != cell_count) ||
        (!config->active_cell_mask &&
         config->active_cell_mask_count != 0u)) {
        return false;
    }
    sx = (size_t)config->cells_x + 1u;
    sy = (size_t)config->cells_y + 1u;
    sz = (size_t)config->cells_z + 1u;
    if (sx > SIZE_MAX / sy || sx * sy > SIZE_MAX / sz) return false;
    *out_sample_count = sx * sy * sz;
    return *out_sample_count <= config->max_samples;
}

static CoreObjectVec3 vec_add(CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static CoreObjectVec3 vec_sub(CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static CoreObjectVec3 vec_scale(CoreObjectVec3 value, double scale) {
    return (CoreObjectVec3){
        value.x * scale, value.y * scale, value.z * scale};
}

static double vec_dot(CoreObjectVec3 a, CoreObjectVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static CoreObjectVec3 vec_cross(CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

static double vec_length(CoreObjectVec3 value) {
    return sqrt(vec_dot(value, value));
}

static bool vec_normalize(CoreObjectVec3 value, CoreObjectVec3 *out) {
    const double length = vec_length(value);
    if (!out || !isfinite(length) || length <= 1.0e-14) return false;
    *out = vec_scale(value, 1.0 / length);
    return true;
}

static SolidEdgeKey edge_key(size_t a, size_t b) {
    return (SolidEdgeKey){a < b ? a : b, a < b ? b : a};
}

static int edge_key_compare_value(SolidEdgeKey a, SolidEdgeKey b) {
    if (a.lo < b.lo) return -1;
    if (a.lo > b.lo) return 1;
    if (a.hi < b.hi) return -1;
    if (a.hi > b.hi) return 1;
    return 0;
}

static int iso_vertex_compare(const void *lhs, const void *rhs) {
    const SolidIsoVertex *a = lhs;
    const SolidIsoVertex *b = rhs;
    return edge_key_compare_value(a->key, b->key);
}

static int mesh_edge_compare(const void *lhs, const void *rhs) {
    const SolidMeshEdge *a = lhs;
    const SolidMeshEdge *b = rhs;
    return edge_key_compare_value(
        (SolidEdgeKey){a->lo, a->hi},
        (SolidEdgeKey){b->lo, b->hi});
}

static int spatial_entry_compare(const void *lhs, const void *rhs) {
    const SolidSpatialEntry *a = lhs;
    const SolidSpatialEntry *b = rhs;
    if (a->qx < b->qx) return -1;
    if (a->qx > b->qx) return 1;
    if (a->qy < b->qy) return -1;
    if (a->qy > b->qy) return 1;
    if (a->qz < b->qz) return -1;
    if (a->qz > b->qz) return 1;
    if (a->edge_index < b->edge_index) return -1;
    return a->edge_index > b->edge_index ? 1 : 0;
}

static bool append_vertex(SolidBuild *build, SolidIsoVertex vertex) {
    SolidIsoVertex *next;
    size_t capacity;
    if (build->vertex_count >= build->config->max_vertices * 12u) {
        return false;
    }
    if (build->vertex_count == build->vertex_capacity) {
        capacity = build->vertex_capacity ? build->vertex_capacity * 2u : 4096u;
        if (capacity > build->config->max_vertices * 12u) {
            capacity = build->config->max_vertices * 12u;
        }
        if (capacity <= build->vertex_capacity) return false;
        next = realloc(build->vertices, capacity * sizeof(*next));
        if (!next) return false;
        build->vertices = next;
        build->vertex_capacity = capacity;
    }
    build->vertices[build->vertex_count++] = vertex;
    return true;
}

static bool append_triangle(SolidBuild *build,
                            SolidEdgeKey a,
                            SolidEdgeKey b,
                            SolidEdgeKey c) {
    SolidTempTriangle *next;
    size_t capacity;
    if (edge_key_compare_value(a, b) == 0 ||
        edge_key_compare_value(a, c) == 0 ||
        edge_key_compare_value(b, c) == 0 ||
        build->triangle_count >= build->config->max_triangles) {
        return false;
    }
    if (build->triangle_count == build->triangle_capacity) {
        capacity = build->triangle_capacity
            ? build->triangle_capacity * 2u : 4096u;
        if (capacity > build->config->max_triangles) {
            capacity = build->config->max_triangles;
        }
        if (capacity <= build->triangle_capacity) return false;
        next = realloc(build->triangles, capacity * sizeof(*next));
        if (!next) return false;
        build->triangles = next;
        build->triangle_capacity = capacity;
    }
    build->triangles[build->triangle_count++] =
        (SolidTempTriangle){{a, b, c}};
    return true;
}

static bool iso_vertex(size_t id_a,
                       size_t id_b,
                       CoreObjectVec3 a,
                       CoreObjectVec3 b,
                       double da,
                       double db,
                       SolidBuild *build,
                       SolidEdgeKey *out_key) {
    double t;
    SolidIsoVertex vertex;
    const double denominator = da - db;
    const double cell_x =
        (build->config->bounds_max.x - build->config->bounds_min.x) /
        (double)build->config->cells_x;
    const double cell_y =
        (build->config->bounds_max.y - build->config->bounds_min.y) /
        (double)build->config->cells_y;
    const double cell_z =
        (build->config->bounds_max.z - build->config->bounds_min.z) /
        (double)build->config->cells_z;
    const double cell_size = fmax(cell_x, fmax(cell_y, cell_z));
    const double endpoint_fraction = fmin(
        0.01,
        fmax(1.0e-9,
             2.0 * sqrt(build->config->minimum_triangle_area2) /
                 cell_size));
    if (!isfinite(denominator) || fabs(denominator) <= 1.0e-18) {
        return false;
    }
    t = da / denominator;
    if (t < endpoint_fraction) t = endpoint_fraction;
    if (t > 1.0 - endpoint_fraction) t = 1.0 - endpoint_fraction;
    vertex.key = edge_key(id_a, id_b);
    vertex.position = vec_add(a, vec_scale(vec_sub(b, a), t));
    if (!finite_vec(vertex.position) || !append_vertex(build, vertex)) {
        return false;
    }
    *out_key = vertex.key;
    return true;
}

static bool polygonise_tetra(const size_t ids[4],
                             const CoreObjectVec3 positions[4],
                             const double values[4],
                             SolidBuild *build) {
    size_t inside[4];
    size_t outside[4];
    size_t inside_count = 0u;
    size_t outside_count = 0u;
    SolidEdgeKey edge[4];
    for (size_t i = 0u; i < 4u; ++i) {
        if (values[i] < 0.0) inside[inside_count++] = i;
        else outside[outside_count++] = i;
    }
    if (inside_count == 0u || inside_count == 4u) return true;
    if (inside_count == 1u || inside_count == 3u) {
        const bool complement = inside_count == 3u;
        const size_t pivot = complement ? outside[0] : inside[0];
        const size_t *others = complement ? inside : outside;
        for (size_t i = 0u; i < 3u; ++i) {
            if (!iso_vertex(ids[pivot], ids[others[i]],
                            positions[pivot], positions[others[i]],
                            values[pivot], values[others[i]],
                            build, &edge[i])) {
                return false;
            }
        }
        return complement
            ? append_triangle(build, edge[0], edge[2], edge[1])
            : append_triangle(build, edge[0], edge[1], edge[2]);
    }
    if (!iso_vertex(ids[inside[0]], ids[outside[0]],
                    positions[inside[0]], positions[outside[0]],
                    values[inside[0]], values[outside[0]], build, &edge[0]) ||
        !iso_vertex(ids[inside[0]], ids[outside[1]],
                    positions[inside[0]], positions[outside[1]],
                    values[inside[0]], values[outside[1]], build, &edge[1]) ||
        !iso_vertex(ids[inside[1]], ids[outside[0]],
                    positions[inside[1]], positions[outside[0]],
                    values[inside[1]], values[outside[0]], build, &edge[2]) ||
        !iso_vertex(ids[inside[1]], ids[outside[1]],
                    positions[inside[1]], positions[outside[1]],
                    values[inside[1]], values[outside[1]], build, &edge[3])) {
        return false;
    }
    return append_triangle(build, edge[0], edge[1], edge[3]) &&
           append_triangle(build, edge[0], edge[3], edge[2]);
}

static size_t grid_id(size_t x,
                      size_t y,
                      size_t z,
                      size_t sx,
                      size_t sy) {
    return x + sx * (y + sy * z);
}

static size_t cell_id(
    size_t x,
    size_t y,
    size_t z,
    const ProceduralSolidMeshConfig *config) {
    return x + (size_t)config->cells_x *
        (y + (size_t)config->cells_y * z);
}

static bool cell_active(
    const ProceduralSolidMeshConfig *config,
    size_t x,
    size_t y,
    size_t z) {
    return !config->active_cell_mask ||
        config->active_cell_mask[cell_id(x, y, z, config)] != 0u;
}

static CoreObjectVec3 grid_position(
    const ProceduralSolidMeshConfig *config,
    size_t x,
    size_t y,
    size_t z) {
    const double tx = (double)x / (double)config->cells_x;
    const double ty = (double)y / (double)config->cells_y;
    const double tz = (double)z / (double)config->cells_z;
    return (CoreObjectVec3){
        config->bounds_min.x +
            (config->bounds_max.x - config->bounds_min.x) * tx,
        config->bounds_min.y +
            (config->bounds_max.y - config->bounds_min.y) * ty,
        config->bounds_min.z +
            (config->bounds_max.z - config->bounds_min.z) * tz};
}

static bool evaluate_grid(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    const ProceduralSolidMeshConfig *config,
    double *values,
    ProceduralSolidMeshSummary *summary,
    ProceduralSolidMeshReport *report) {
    const size_t sx = (size_t)config->cells_x + 1u;
    const size_t sy = (size_t)config->cells_y + 1u;
    const size_t sz = (size_t)config->cells_z + 1u;
    const size_t sample_count = sx * sy * sz;
    uint8_t *required = NULL;
    double boundary_min = HUGE_VAL;
    double boundary_max = -HUGE_VAL;
    ProceduralSolidGraphReport graph_report;
    if (config->active_cell_mask) {
        required = calloc(sample_count, sizeof(*required));
        if (!required) {
            mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_ALLOCATION,
                        "active_samples",
                        "active sample mask allocation failed");
            return false;
        }
        for (size_t z = 0u; z < config->cells_z; ++z) {
            for (size_t y = 0u; y < config->cells_y; ++y) {
                for (size_t x = 0u; x < config->cells_x; ++x) {
                    if (!cell_active(config, x, y, z)) continue;
                    for (size_t dz = 0u; dz <= 1u; ++dz) {
                        for (size_t dy = 0u; dy <= 1u; ++dy) {
                            for (size_t dx = 0u; dx <= 1u; ++dx) {
                                required[grid_id(
                                    x + dx, y + dy, z + dz, sx, sy)] = 1u;
                            }
                        }
                    }
                }
            }
        }
        for (size_t z = 0u; z <= config->cells_z; ++z) {
            for (size_t y = 0u; y <= config->cells_y; ++y) {
                for (size_t x = 0u; x <= config->cells_x; ++x) {
                    if (x == 0u || y == 0u || z == 0u ||
                        x == config->cells_x ||
                        y == config->cells_y ||
                        z == config->cells_z) {
                        required[grid_id(x, y, z, sx, sy)] = 1u;
                    }
                }
            }
        }
    }
    for (size_t z = 0u; z <= config->cells_z; ++z) {
        for (size_t y = 0u; y <= config->cells_y; ++y) {
            for (size_t x = 0u; x <= config->cells_x; ++x) {
                const size_t id = grid_id(x, y, z, sx, sy);
                ProceduralSolidSample sample;
                if (required && !required[id]) {
                    values[id] = HUGE_VAL;
                    continue;
                }
                if (!ProceduralSolidGraphV1_Evaluate(
                        graph, sources, grid_position(config, x, y, z),
                        &sample, &graph_report)) {
                    mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_FIELD,
                                graph_report.field, graph_report.message);
                    free(required);
                    return false;
                }
                ++summary->evaluated_sample_count;
                values[id] = sample.signed_distance;
                summary->source_triangle_tests +=
                    sample.source_triangle_tests;
                summary->source_query_count += sample.source_query_count;
                summary->accelerated_source_query_count +=
                    sample.accelerated_source_query_count;
                if (fabs(values[id]) <= 1.0e-12) {
                    values[id] = 1.0e-12;
                }
                if (values[id] < 0.0) ++summary->inside_sample_count;
                if (x == 0u || y == 0u || z == 0u ||
                    x == config->cells_x || y == config->cells_y ||
                    z == config->cells_z) {
                    if (values[id] < boundary_min) boundary_min = values[id];
                    if (values[id] > boundary_max) boundary_max = values[id];
                }
            }
        }
    }
    summary->boundary_min_signed_distance = boundary_min;
    summary->boundary_max_signed_distance = boundary_max;
    free(required);
    if (!(boundary_min > 0.0)) {
        mesh_report(
            report, PROCEDURAL_SOLID_MESH_STATUS_DOMAIN_CLIPPED,
            "bounds", "solid intersects or crosses the sampling boundary");
        return false;
    }
    return true;
}

static bool build_tetrahedra(const ProceduralSolidMeshConfig *config,
                             const double *values,
                             SolidBuild *build) {
    static const size_t corner_offsets[8][3] = {
        {0u, 0u, 0u}, {1u, 0u, 0u}, {1u, 1u, 0u}, {0u, 1u, 0u},
        {0u, 0u, 1u}, {1u, 0u, 1u}, {1u, 1u, 1u}, {0u, 1u, 1u}};
    static const size_t tetrahedra[6][4] = {
        {0u, 5u, 1u, 6u}, {0u, 1u, 2u, 6u},
        {0u, 2u, 3u, 6u}, {0u, 3u, 7u, 6u},
        {0u, 7u, 4u, 6u}, {0u, 4u, 5u, 6u}};
    const size_t sx = (size_t)config->cells_x + 1u;
    const size_t sy = (size_t)config->cells_y + 1u;
    for (size_t z = 0u; z < config->cells_z; ++z) {
        for (size_t y = 0u; y < config->cells_y; ++y) {
            for (size_t x = 0u; x < config->cells_x; ++x) {
                size_t ids[8];
                CoreObjectVec3 positions[8];
                double cell_values[8];
                if (!cell_active(config, x, y, z)) continue;
                for (size_t c = 0u; c < 8u; ++c) {
                    const size_t cx = x + corner_offsets[c][0];
                    const size_t cy = y + corner_offsets[c][1];
                    const size_t cz = z + corner_offsets[c][2];
                    ids[c] = grid_id(cx, cy, cz, sx, sy);
                    positions[c] = grid_position(config, cx, cy, cz);
                    cell_values[c] = values[ids[c]];
                }
                for (size_t t = 0u; t < 6u; ++t) {
                    size_t tet_ids[4];
                    CoreObjectVec3 tet_positions[4];
                    double tet_values[4];
                    for (size_t v = 0u; v < 4u; ++v) {
                        const size_t c = tetrahedra[t][v];
                        tet_ids[v] = ids[c];
                        tet_positions[v] = positions[c];
                        tet_values[v] = cell_values[c];
                    }
                    if (!polygonise_tetra(
                            tet_ids, tet_positions, tet_values, build)) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

static size_t find_sorted_vertex(const SolidIsoVertex *vertices,
                                 size_t count,
                                 SolidEdgeKey key) {
    SolidIsoVertex target;
    const SolidIsoVertex *found;
    target.key = key;
    target.position = (CoreObjectVec3){0.0, 0.0, 0.0};
    found = bsearch(&target, vertices, count, sizeof(*vertices),
                    iso_vertex_compare);
    return found ? (size_t)(found - vertices) : SIZE_MAX;
}

static bool weld_spatial_vertices(SolidBuild *build,
                                  size_t *unique_vertex_count,
                                  double tolerance) {
    SolidSpatialEntry *spatial = NULL;
    SolidEdgeKey *representatives = NULL;
    const size_t count = *unique_vertex_count;
    size_t compact_count = 0u;
    if (!(tolerance > 0.0) || !isfinite(tolerance)) return false;
    spatial = malloc(count * sizeof(*spatial));
    representatives = malloc(count * sizeof(*representatives));
    if (!spatial || !representatives) {
        free(spatial);
        free(representatives);
        return false;
    }
    for (size_t i = 0u; i < count; ++i) {
        const CoreObjectVec3 p = build->vertices[i].position;
        const double qx = p.x / tolerance;
        const double qy = p.y / tolerance;
        const double qz = p.z / tolerance;
        if (!isfinite(qx) || !isfinite(qy) || !isfinite(qz) ||
            fabs(qx) > 9.0e18 || fabs(qy) > 9.0e18 ||
            fabs(qz) > 9.0e18) {
            free(spatial);
            free(representatives);
            return false;
        }
        spatial[i] = (SolidSpatialEntry){
            llround(qx), llround(qy), llround(qz), i};
        representatives[i] = build->vertices[i].key;
    }
    qsort(spatial, count, sizeof(*spatial), spatial_entry_compare);
    for (size_t start = 0u; start < count;) {
        size_t end = start + 1u;
        SolidEdgeKey representative =
            build->vertices[spatial[start].edge_index].key;
        while (end < count &&
               spatial[end].qx == spatial[start].qx &&
               spatial[end].qy == spatial[start].qy &&
               spatial[end].qz == spatial[start].qz) {
            const SolidEdgeKey candidate =
                build->vertices[spatial[end].edge_index].key;
            if (edge_key_compare_value(candidate, representative) < 0) {
                representative = candidate;
            }
            ++end;
        }
        for (size_t i = start; i < end; ++i) {
            representatives[spatial[i].edge_index] = representative;
        }
        start = end;
    }
    for (size_t i = 0u; i < build->triangle_count; ++i) {
        for (size_t e = 0u; e < 3u; ++e) {
            const size_t index = find_sorted_vertex(
                build->vertices, count, build->triangles[i].edges[e]);
            if (index == SIZE_MAX) {
                free(spatial);
                free(representatives);
                return false;
            }
            build->triangles[i].edges[e] = representatives[index];
        }
    }
    for (size_t i = 0u; i < count; ++i) {
        if (edge_key_compare_value(
                representatives[i], build->vertices[i].key) == 0) {
            build->vertices[compact_count++] = build->vertices[i];
        }
    }
    free(spatial);
    free(representatives);
    *unique_vertex_count = compact_count;
    return compact_count > 0u;
}

static size_t filter_degenerate_triangles(
    SolidBuild *build,
    size_t unique_vertex_count) {
    size_t kept = 0u;
    for (size_t i = 0u; i < build->triangle_count; ++i) {
        const size_t ia = find_sorted_vertex(
            build->vertices, unique_vertex_count,
            build->triangles[i].edges[0]);
        const size_t ib = find_sorted_vertex(
            build->vertices, unique_vertex_count,
            build->triangles[i].edges[1]);
        const size_t ic = find_sorted_vertex(
            build->vertices, unique_vertex_count,
            build->triangles[i].edges[2]);
        double area2;
        if (ia == SIZE_MAX || ib == SIZE_MAX || ic == SIZE_MAX ||
            ia == ib || ia == ic || ib == ic) {
            continue;
        }
        area2 = vec_length(vec_cross(
            vec_sub(build->vertices[ib].position,
                    build->vertices[ia].position),
            vec_sub(build->vertices[ic].position,
                    build->vertices[ia].position)));
        if (!isfinite(area2) ||
            area2 < build->config->minimum_triangle_area2) {
            continue;
        }
        build->triangles[kept++] = build->triangles[i];
    }
    return kept;
}

static bool evaluate_gradient(const ProceduralSolidGraphV1 *graph,
                              const ProceduralSolidSourceSet *sources,
                              CoreObjectVec3 point,
                              double step,
                              CoreObjectVec3 *out) {
    ProceduralSolidGraphReport report;
    ProceduralSolidSample plus;
    ProceduralSolidSample minus;
    CoreObjectVec3 gradient;
    CoreObjectVec3 p = point;
    p.x += step;
    if (!ProceduralSolidGraphV1_Evaluate(
            graph, sources, p, &plus, &report)) return false;
    p.x -= 2.0 * step;
    if (!ProceduralSolidGraphV1_Evaluate(
            graph, sources, p, &minus, &report)) return false;
    gradient.x = plus.signed_distance - minus.signed_distance;
    p = point;
    p.y += step;
    if (!ProceduralSolidGraphV1_Evaluate(
            graph, sources, p, &plus, &report)) return false;
    p.y -= 2.0 * step;
    if (!ProceduralSolidGraphV1_Evaluate(
            graph, sources, p, &minus, &report)) return false;
    gradient.y = plus.signed_distance - minus.signed_distance;
    p = point;
    p.z += step;
    if (!ProceduralSolidGraphV1_Evaluate(
            graph, sources, p, &plus, &report)) return false;
    p.z -= 2.0 * step;
    if (!ProceduralSolidGraphV1_Evaluate(
            graph, sources, p, &minus, &report)) return false;
    gradient.z = plus.signed_distance - minus.signed_distance;
    return vec_normalize(gradient, out);
}

static double signed_volume(
    const CoreMeshAssetRuntimeDocument *document) {
    double volume6 = 0.0;
    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *t = &document->triangles[i];
        const CoreObjectVec3 a = document->vertices[t->a].position;
        const CoreObjectVec3 b = document->vertices[t->b].position;
        const CoreObjectVec3 c = document->vertices[t->c].position;
        volume6 += vec_dot(a, vec_cross(b, c));
    }
    return volume6 / 6.0;
}

static bool create_document(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    const ProceduralSolidMeshConfig *config,
    const char *asset_id,
    SolidBuild *build,
    CoreMeshAssetRuntimeDocument *document,
    ProceduralSolidMeshSummary *summary,
    ProceduralSolidMeshReport *report) {
    size_t unique_count = 0u;
    CoreResult result;
    qsort(build->vertices, build->vertex_count,
          sizeof(*build->vertices), iso_vertex_compare);
    for (size_t i = 0u; i < build->vertex_count; ++i) {
        if (unique_count == 0u ||
            edge_key_compare_value(
                build->vertices[unique_count - 1u].key,
                build->vertices[i].key) != 0) {
            build->vertices[unique_count++] = build->vertices[i];
        }
    }
    /*
     * A rotated analytic face can cross a tetrahedron arbitrarily close to a
     * corner.  Collapse those numerically coincident crossings before the
     * area gate so every incident cell selects the same topological vertex.
     * Tie the tolerance to both cell scale and the configured minimum area;
     * it remains several orders of magnitude below the sampling resolution.
     */
    const double weld_tolerance = fmax(
        summary->maximum_cell_size_units * 1.0e-6,
        (config->minimum_triangle_area2 /
         summary->maximum_cell_size_units) * 4.0);
    if (!weld_spatial_vertices(
            build, &unique_count, fmax(weld_tolerance, 1.0e-12))) {
        mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_ALLOCATION,
                    "weld", "solid spatial weld failed");
        return false;
    }
    build->triangle_count =
        filter_degenerate_triangles(build, unique_count);
    if (unique_count == 0u || build->triangle_count == 0u ||
        unique_count > config->max_vertices) {
        mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_EMPTY,
                    "mesh", "solid extraction produced no bounded shell");
        return false;
    }
    core_mesh_asset_runtime_document_init(document);
    result = core_mesh_asset_runtime_contract_set_asset_id(
        &document->contract, asset_id);
    if (result.code != CORE_OK) goto core_fail;
    result = core_mesh_asset_runtime_contract_set_source_asset_id(
        &document->contract, graph->semantic_source_id);
    if (result.code != CORE_OK) goto core_fail;
    if (core_mesh_asset_runtime_document_set_vertex_count(
            document, unique_count).code != CORE_OK ||
        core_mesh_asset_runtime_document_set_triangle_count(
            document, build->triangle_count).code != CORE_OK ||
        core_mesh_asset_runtime_document_set_surface_group_count(
            document, 1u).code != CORE_OK) {
        goto core_fail;
    }
    for (size_t i = 0u; i < unique_count; ++i) {
        document->vertices[i].position = build->vertices[i].position;
        if (!evaluate_gradient(
                graph, sources, build->vertices[i].position,
                config->gradient_step_units,
                &document->vertices[i].normal)) {
            mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_FIELD,
                        "normal", "solid gradient normal evaluation failed");
            return false;
        }
    }
    document->vertex_normal_count = unique_count;
    document->normal_provenance =
        CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_GENERATED_SMOOTH;
    for (size_t i = 0u; i < build->triangle_count; ++i) {
        CoreMeshAssetRuntimeTriangle *triangle = &document->triangles[i];
        CoreObjectVec3 a;
        CoreObjectVec3 b;
        CoreObjectVec3 c;
        CoreObjectVec3 centroid;
        CoreObjectVec3 gradient;
        triangle->a = find_sorted_vertex(
            build->vertices, unique_count, build->triangles[i].edges[0]);
        triangle->b = find_sorted_vertex(
            build->vertices, unique_count, build->triangles[i].edges[1]);
        triangle->c = find_sorted_vertex(
            build->vertices, unique_count, build->triangles[i].edges[2]);
        if (triangle->a == SIZE_MAX || triangle->b == SIZE_MAX ||
            triangle->c == SIZE_MAX || triangle->a == triangle->b ||
            triangle->a == triangle->c || triangle->b == triangle->c) {
            mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_DEGENERATE,
                        "triangle", "solid triangle indices are degenerate");
            return false;
        }
        a = document->vertices[triangle->a].position;
        b = document->vertices[triangle->b].position;
        c = document->vertices[triangle->c].position;
        centroid = vec_scale(vec_add(vec_add(a, b), c), 1.0 / 3.0);
        if (!evaluate_gradient(
                graph, sources, centroid, config->gradient_step_units,
                &gradient)) {
            mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_FIELD,
                        "winding", "solid winding gradient evaluation failed");
            return false;
        }
        if (vec_dot(vec_cross(vec_sub(b, a), vec_sub(c, a)), gradient) < 0.0) {
            const size_t swap = triangle->b;
            triangle->b = triangle->c;
            triangle->c = swap;
        }
        snprintf(triangle->surface_group_id,
                 sizeof(triangle->surface_group_id),
                 "procedural_solid_shell");
    }
    snprintf(document->surface_groups[0].group_id,
             sizeof(document->surface_groups[0].group_id),
             "procedural_solid_shell");
    document->surface_groups[0].triangle_start = 0u;
    document->surface_groups[0].triangle_count = document->triangle_count;
    document->contract.topology_closed_volume = true;
    document->contract.topology_manifold_expected = true;
    document->contract.local_bounds.min = document->vertices[0].position;
    document->contract.local_bounds.max = document->vertices[0].position;
    for (size_t i = 1u; i < document->vertex_count; ++i) {
        const CoreObjectVec3 p = document->vertices[i].position;
        CoreObjectVec3 *min = &document->contract.local_bounds.min;
        CoreObjectVec3 *max = &document->contract.local_bounds.max;
        if (p.x < min->x) min->x = p.x;
        if (p.y < min->y) min->y = p.y;
        if (p.z < min->z) min->z = p.z;
        if (p.x > max->x) max->x = p.x;
        if (p.y > max->y) max->y = p.y;
        if (p.z > max->z) max->z = p.z;
    }
    result = core_mesh_asset_runtime_document_validate(document);
    if (result.code != CORE_OK) goto core_fail;
    summary->vertex_count = document->vertex_count;
    summary->triangle_count = document->triangle_count;
    summary->bounds_min = document->contract.local_bounds.min;
    summary->bounds_max = document->contract.local_bounds.max;
    return true;
core_fail:
    mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_CORE_MESH,
                "document", result.message);
    return false;
}

static size_t union_find_root(size_t *parents, size_t value) {
    while (parents[value] != value) {
        parents[value] = parents[parents[value]];
        value = parents[value];
    }
    return value;
}

static void union_find_join(size_t *parents, size_t a, size_t b) {
    a = union_find_root(parents, a);
    b = union_find_root(parents, b);
    if (a == b) return;
    if (a < b) parents[b] = a;
    else parents[a] = b;
}

static bool analyze_topology(
    const CoreMeshAssetRuntimeDocument *document,
    const ProceduralSolidMeshConfig *config,
    ProceduralSolidMeshSummary *summary,
    ProceduralSolidMeshReport *report) {
    SolidMeshEdge *edges;
    size_t *parents;
    size_t raw_count;
    size_t unique_count = 0u;
    double min_edge = HUGE_VAL;
    double max_edge = 0.0;
    double min_area2 = HUGE_VAL;
    if (document->triangle_count > SIZE_MAX / 3u) return false;
    raw_count = document->triangle_count * 3u;
    edges = calloc(raw_count, sizeof(*edges));
    parents = malloc(document->vertex_count * sizeof(*parents));
    if (!edges || !parents) {
        free(edges);
        free(parents);
        mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_ALLOCATION,
                    "topology", "solid topology allocation failed");
        return false;
    }
    for (size_t i = 0u; i < document->vertex_count; ++i) parents[i] = i;
    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *t = &document->triangles[i];
        const size_t ids[3] = {t->a, t->b, t->c};
        const CoreObjectVec3 a = document->vertices[t->a].position;
        const CoreObjectVec3 b = document->vertices[t->b].position;
        const CoreObjectVec3 c = document->vertices[t->c].position;
        const double area2 = vec_length(
            vec_cross(vec_sub(b, a), vec_sub(c, a)));
        if (area2 < min_area2) min_area2 = area2;
        for (size_t e = 0u; e < 3u; ++e) {
            const size_t x = ids[e];
            const size_t y = ids[(e + 1u) % 3u];
            edges[i * 3u + e] = (SolidMeshEdge){
                x < y ? x : y, x < y ? y : x, 1u};
            union_find_join(parents, x, y);
        }
    }
    if (!(min_area2 >= config->minimum_triangle_area2)) {
        free(edges);
        free(parents);
        mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_DEGENERATE,
                    "triangle_area", "solid triangle area floor failed");
        return false;
    }
    qsort(edges, raw_count, sizeof(*edges), mesh_edge_compare);
    for (size_t i = 0u; i < raw_count; ++i) {
        if (unique_count > 0u &&
            edges[unique_count - 1u].lo == edges[i].lo &&
            edges[unique_count - 1u].hi == edges[i].hi) {
            ++edges[unique_count - 1u].incidence;
        } else {
            edges[unique_count++] = edges[i];
        }
    }
    for (size_t i = 0u; i < unique_count; ++i) {
        const double length = vec_length(vec_sub(
            document->vertices[edges[i].lo].position,
            document->vertices[edges[i].hi].position));
        if (edges[i].incidence == 1u) ++summary->boundary_edge_count;
        if (edges[i].incidence > 2u) ++summary->nonmanifold_edge_count;
        if (length < min_edge) min_edge = length;
        if (length > max_edge) max_edge = length;
    }
    for (size_t i = 0u; i < document->vertex_count; ++i) {
        if (union_find_root(parents, i) == i) {
            ++summary->connected_component_count;
        }
    }
    summary->unique_edge_count = unique_count;
    summary->euler_characteristic =
        (int)document->vertex_count - (int)unique_count +
        (int)document->triangle_count;
    summary->minimum_triangle_area2 = min_area2;
    summary->minimum_edge_length_units = min_edge;
    summary->maximum_edge_length_units = max_edge;
    summary->signed_volume_units3 = signed_volume(document);
    free(edges);
    free(parents);
    if ((config->require_closed_manifold &&
         (summary->boundary_edge_count != 0u ||
          summary->nonmanifold_edge_count != 0u)) ||
        (config->require_positive_volume &&
         !(summary->signed_volume_units3 > 0.0))) {
        char message[256];
        snprintf(message, sizeof(message),
                 "solid shell topology failed boundary=%zu "
                 "nonmanifold=%zu volume=%.17g",
                 summary->boundary_edge_count,
                 summary->nonmanifold_edge_count,
                 summary->signed_volume_units3);
        mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_TOPOLOGY,
                    "topology", message);
        return false;
    }
    if (summary->connected_component_count < config->min_components ||
        summary->connected_component_count > config->max_components) {
        mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_COMPONENT_POLICY,
                    "components", "solid shell component policy failed");
        return false;
    }
    return true;
}

bool ProceduralSolidMesh_Digest(
    const CoreMeshAssetRuntimeDocument *document,
    char out_digest[PROCEDURAL_SOLID_MESH_DIGEST_CAPACITY]) {
    size_t capacity;
    size_t length = 0u;
    char *canonical;
    if (document->vertex_count > (SIZE_MAX - 256u) / 128u ||
        document->triangle_count > (SIZE_MAX - 256u) / 96u) {
        return false;
    }
    capacity = 256u + document->vertex_count * 128u +
               document->triangle_count * 96u;
    canonical = malloc(capacity);
    if (!canonical) return false;
#define APPEND(...) do { \
    const int count = snprintf(canonical + length, capacity - length, \
                               __VA_ARGS__); \
    if (count < 0 || (size_t)count >= capacity - length) { \
        free(canonical); \
        return false; \
    } \
    length += (size_t)count; \
} while (0)
    APPEND("solid_mesh_v1|%s|%s|%zu|%zu|",
           document->contract.asset_id,
           document->contract.source_asset_id,
           document->vertex_count, document->triangle_count);
    for (size_t i = 0u; i < document->vertex_count; ++i) {
        const CoreObjectVec3 p = document->vertices[i].position;
        APPEND("v|%.17g|%.17g|%.17g|", p.x, p.y, p.z);
    }
    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *t = &document->triangles[i];
        APPEND("t|%zu|%zu|%zu|", t->a, t->b, t->c);
    }
#undef APPEND
    if (!ray_tracing_sha256_bytes(canonical, length, out_digest)) {
        free(canonical);
        return false;
    }
    free(canonical);
    return true;
}

bool ProceduralSolidMesh_Compile(
    const ProceduralSolidGraphV1 *graph,
    const ProceduralSolidSourceSet *sources,
    const ProceduralSolidMeshConfig *config,
    const char *derived_asset_id,
    CoreMeshAssetRuntimeDocument *out_document,
    ProceduralSolidMeshSummary *out_summary,
    ProceduralSolidMeshReport *report) {
    size_t sample_count = 0u;
    double *values = NULL;
    SolidBuild build;
    CoreMeshAssetRuntimeDocument document;
    ProceduralSolidMeshSummary summary;
    ProceduralSolidGraphReport graph_report;
    mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_OK, "", "ok");
    if (!graph || !config || !derived_asset_id || !derived_asset_id[0] ||
        !out_document || !out_summary) {
        mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_NULL_ARGUMENT,
                    "arguments", "solid compile arguments are required");
        return false;
    }
    if (!config_valid(config, &sample_count)) {
        mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_CONFIG,
                    "config", "solid mesh config is invalid or over budget");
        return false;
    }
    if (!ProceduralSolidGraphV1_Validate(graph, &graph_report)) {
        mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_FIELD,
                    graph_report.field, graph_report.message);
        return false;
    }
    memset(&summary, 0, sizeof(summary));
    summary.samples_x = config->cells_x + 1u;
    summary.samples_y = config->cells_y + 1u;
    summary.samples_z = config->cells_z + 1u;
    summary.sample_count = sample_count;
    summary.total_cell_count =
        (size_t)config->cells_x * (size_t)config->cells_y *
        (size_t)config->cells_z;
    summary.active_cell_count = summary.total_cell_count;
    if (config->active_cell_mask) {
        summary.active_cell_count = 0u;
        for (size_t i = 0u; i < config->active_cell_mask_count; ++i) {
            if (config->active_cell_mask[i]) ++summary.active_cell_count;
        }
    }
    summary.collision_authority = config->collision_authority;
    summary.maximum_cell_size_units = fmax(
        (config->bounds_max.x - config->bounds_min.x) / config->cells_x,
        fmax((config->bounds_max.y - config->bounds_min.y) / config->cells_y,
             (config->bounds_max.z - config->bounds_min.z) / config->cells_z));
    summary.thin_feature_floor_units =
        2.0 * summary.maximum_cell_size_units;
    summary.conforming_cell_self_intersection_free = true;
    if (!ProceduralSolidGraphV1_Digest(
            graph, summary.graph_digest_sha256, &graph_report)) {
        mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_IDENTITY,
                    graph_report.field, graph_report.message);
        return false;
    }
    values = malloc(sample_count * sizeof(*values));
    if (!values) {
        mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_ALLOCATION,
                    "samples", "solid sample allocation failed");
        return false;
    }
    if (!evaluate_grid(graph, sources, config, values, &summary, report)) {
        free(values);
        return false;
    }
    if (summary.inside_sample_count == 0u) {
        free(values);
        mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_EMPTY,
                    "samples", "solid domain contains no inside samples");
        return false;
    }
    memset(&build, 0, sizeof(build));
    build.config = config;
    if (!build_tetrahedra(config, values, &build)) {
        free(values);
        free(build.vertices);
        free(build.triangles);
        mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_CAPACITY,
                    "extraction",
                    "solid extraction exceeded capacity or allocation");
        return false;
    }
    free(values);
    core_mesh_asset_runtime_document_init(&document);
    if (!create_document(
            graph, sources, config, derived_asset_id, &build,
            &document, &summary, report) ||
        !analyze_topology(&document, config, &summary, report) ||
        !ProceduralSolidMesh_Digest(&document, summary.mesh_digest_sha256)) {
        if (report && report->status == PROCEDURAL_SOLID_MESH_STATUS_OK) {
            mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_IDENTITY,
                        "mesh_digest", "solid mesh digest failed");
        }
        core_mesh_asset_runtime_document_free(&document);
        free(build.vertices);
        free(build.triangles);
        return false;
    }
    free(build.vertices);
    free(build.triangles);
    *out_document = document;
    *out_summary = summary;
    return true;
}

bool ProceduralSolidMesh_Reanalyze(
    const ProceduralSolidMeshConfig *config,
    CoreMeshAssetRuntimeDocument *document,
    ProceduralSolidMeshSummary *summary,
    ProceduralSolidMeshReport *report) {
    CoreResult result;
    if (!config || !document || !summary || document->vertex_count == 0u ||
        !document->vertices || document->triangle_count == 0u ||
        !document->triangles) {
        mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_NULL_ARGUMENT,
                    "reanalyze", "solid reanalysis inputs are required");
        return false;
    }
    mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_OK, "", "ok");
    document->contract.local_bounds.min = document->vertices[0].position;
    document->contract.local_bounds.max = document->vertices[0].position;
    for (size_t i = 1u; i < document->vertex_count; ++i) {
        const CoreObjectVec3 p = document->vertices[i].position;
        CoreObjectVec3 *minimum = &document->contract.local_bounds.min;
        CoreObjectVec3 *maximum = &document->contract.local_bounds.max;
        if (p.x < minimum->x) minimum->x = p.x;
        if (p.y < minimum->y) minimum->y = p.y;
        if (p.z < minimum->z) minimum->z = p.z;
        if (p.x > maximum->x) maximum->x = p.x;
        if (p.y > maximum->y) maximum->y = p.y;
        if (p.z > maximum->z) maximum->z = p.z;
    }
    result = core_mesh_asset_runtime_document_validate(document);
    if (result.code != CORE_OK) {
        mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_CORE_MESH,
                    "reanalyze_document", result.message);
        return false;
    }
    summary->vertex_count = document->vertex_count;
    summary->triangle_count = document->triangle_count;
    summary->bounds_min = document->contract.local_bounds.min;
    summary->bounds_max = document->contract.local_bounds.max;
    summary->unique_edge_count = 0u;
    summary->boundary_edge_count = 0u;
    summary->nonmanifold_edge_count = 0u;
    summary->connected_component_count = 0u;
    summary->euler_characteristic = 0;
    summary->signed_volume_units3 = 0.0;
    summary->minimum_triangle_area2 = 0.0;
    summary->minimum_edge_length_units = 0.0;
    summary->maximum_edge_length_units = 0.0;
    if (!analyze_topology(document, config, summary, report) ||
        !ProceduralSolidMesh_Digest(document, summary->mesh_digest_sha256)) {
        if (report && report->status == PROCEDURAL_SOLID_MESH_STATUS_OK) {
            mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_IDENTITY,
                        "reanalyze_digest",
                        "solid reanalysis digest failed");
        }
        return false;
    }
    return true;
}

bool ProceduralSolidMesh_RefreshIdentity(
    const CoreMeshAssetRuntimeDocument *document,
    ProceduralSolidMeshSummary *summary,
    ProceduralSolidMeshReport *report) {
    if (!document || !summary || document->vertex_count == 0u ||
        !document->vertices || document->triangle_count == 0u ||
        !document->triangles) {
        mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_NULL_ARGUMENT,
                    "refresh_identity",
                    "solid identity inputs are required");
        return false;
    }
    if (!ProceduralSolidMesh_Digest(
            document, summary->mesh_digest_sha256)) {
        mesh_report(report, PROCEDURAL_SOLID_MESH_STATUS_IDENTITY,
                    "refresh_identity",
                    "solid mesh identity refresh failed");
        return false;
    }
    summary->vertex_count = document->vertex_count;
    summary->triangle_count = document->triangle_count;
    return true;
}
