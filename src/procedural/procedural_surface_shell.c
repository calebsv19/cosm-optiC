#include "procedural/procedural_surface_shell.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ShellEdge {
    size_t lo;
    size_t hi;
    size_t midpoint;
    size_t incidence;
} ShellEdge;

static double signed_volume(const CoreMeshAssetRuntimeDocument *document);

static void shell_report(ProceduralSurfaceShellReport *report,
                         ProceduralSurfaceShellStatus status,
                         const char *field,
                         const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = status;
    snprintf(report->field, sizeof(report->field), "%s", field ? field : "");
    snprintf(report->message, sizeof(report->message), "%s",
             message ? message : "");
}

const char *ProceduralSurfaceShellStatus_Name(
    ProceduralSurfaceShellStatus status) {
    switch (status) {
        case PROCEDURAL_SURFACE_SHELL_STATUS_OK: return "ok";
        case PROCEDURAL_SURFACE_SHELL_STATUS_NULL_ARGUMENT:
            return "null_argument";
        case PROCEDURAL_SURFACE_SHELL_STATUS_CONFIG: return "config";
        case PROCEDURAL_SURFACE_SHELL_STATUS_SOURCE_INVALID:
            return "source_invalid";
        case PROCEDURAL_SURFACE_SHELL_STATUS_SOURCE_OPEN: return "source_open";
        case PROCEDURAL_SURFACE_SHELL_STATUS_CAPACITY: return "capacity";
        case PROCEDURAL_SURFACE_SHELL_STATUS_ALLOCATION: return "allocation";
        case PROCEDURAL_SURFACE_SHELL_STATUS_FIELD: return "field";
        case PROCEDURAL_SURFACE_SHELL_STATUS_DISPLACEMENT_LIMIT:
            return "displacement_limit";
        case PROCEDURAL_SURFACE_SHELL_STATUS_DERIVED_INVALID:
            return "derived_invalid";
        default: return "unknown";
    }
}

void ProceduralSurfaceShellConfig_Init(ProceduralSurfaceShellConfig *config) {
    if (!config) return;
    *config = (ProceduralSurfaceShellConfig){
        .target_edge_length_units = 0.1,
        .max_displacement_to_source_edge_ratio = 0.45,
        .max_vertices = 1000000u,
        .max_triangles = 2000000u,
        .max_refinement_levels = 6u,
        .require_closed_manifold = true,
        .require_positive_volume = true
    };
}

static CoreObjectVec3 vec_sub(CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static CoreObjectVec3 vec_add(CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static CoreObjectVec3 vec_scale(CoreObjectVec3 a, double scale) {
    return (CoreObjectVec3){a.x * scale, a.y * scale, a.z * scale};
}

static double vec_dot(CoreObjectVec3 a, CoreObjectVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static CoreObjectVec3 vec_cross(CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static double vec_length(CoreObjectVec3 a) {
    return sqrt(vec_dot(a, a));
}

static bool vec_normalize(CoreObjectVec3 value, CoreObjectVec3 *out) {
    double length = vec_length(value);
    if (!out || !isfinite(length) || length <= 1.0e-12) return false;
    *out = vec_scale(value, 1.0 / length);
    return true;
}

static int edge_compare(const void *lhs, const void *rhs) {
    const ShellEdge *a = lhs;
    const ShellEdge *b = rhs;
    if (a->lo < b->lo) return -1;
    if (a->lo > b->lo) return 1;
    if (a->hi < b->hi) return -1;
    return a->hi > b->hi ? 1 : 0;
}

static void edge_set(ShellEdge *edge, size_t a, size_t b) {
    edge->lo = a < b ? a : b;
    edge->hi = a < b ? b : a;
    edge->midpoint = SIZE_MAX;
    edge->incidence = 1u;
}

static bool collect_edges(const CoreMeshAssetRuntimeDocument *document,
                          ShellEdge **out_edges,
                          size_t *out_count,
                          size_t *out_boundary,
                          size_t *out_nonmanifold) {
    size_t raw_count;
    ShellEdge *edges;
    size_t unique = 0u;
    if (!document || !out_edges || !out_count || !out_boundary ||
        !out_nonmanifold ||
        document->triangle_count > SIZE_MAX / 3u) {
        return false;
    }
    raw_count = document->triangle_count * 3u;
    edges = calloc(raw_count, sizeof(*edges));
    if (!edges) return false;
    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *triangle = &document->triangles[i];
        edge_set(&edges[(i * 3u) + 0u], triangle->a, triangle->b);
        edge_set(&edges[(i * 3u) + 1u], triangle->b, triangle->c);
        edge_set(&edges[(i * 3u) + 2u], triangle->c, triangle->a);
    }
    qsort(edges, raw_count, sizeof(*edges), edge_compare);
    for (size_t i = 0u; i < raw_count; ++i) {
        if (unique > 0u && edges[unique - 1u].lo == edges[i].lo &&
            edges[unique - 1u].hi == edges[i].hi) {
            ++edges[unique - 1u].incidence;
        } else {
            edges[unique++] = edges[i];
        }
    }
    *out_boundary = 0u;
    *out_nonmanifold = 0u;
    for (size_t i = 0u; i < unique; ++i) {
        if (edges[i].incidence == 1u) ++*out_boundary;
        if (edges[i].incidence > 2u) ++*out_nonmanifold;
    }
    *out_edges = edges;
    *out_count = unique;
    return true;
}

static ShellEdge *find_edge(ShellEdge *edges,
                            size_t edge_count,
                            size_t a,
                            size_t b) {
    ShellEdge key;
    edge_set(&key, a, b);
    return bsearch(&key, edges, edge_count, sizeof(*edges), edge_compare);
}

static bool recompute_normals(CoreMeshAssetRuntimeDocument *document) {
    for (size_t i = 0u; i < document->vertex_count; ++i) {
        document->vertices[i].normal = (CoreObjectVec3){0.0, 0.0, 0.0};
    }
    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *triangle = &document->triangles[i];
        CoreObjectVec3 a = document->vertices[triangle->a].position;
        CoreObjectVec3 b = document->vertices[triangle->b].position;
        CoreObjectVec3 c = document->vertices[triangle->c].position;
        CoreObjectVec3 normal = vec_cross(vec_sub(b, a), vec_sub(c, a));
        if (vec_length(normal) <= 1.0e-12) return false;
        document->vertices[triangle->a].normal =
            vec_add(document->vertices[triangle->a].normal, normal);
        document->vertices[triangle->b].normal =
            vec_add(document->vertices[triangle->b].normal, normal);
        document->vertices[triangle->c].normal =
            vec_add(document->vertices[triangle->c].normal, normal);
    }
    for (size_t i = 0u; i < document->vertex_count; ++i) {
        CoreObjectVec3 normal;
        if (!vec_normalize(document->vertices[i].normal, &normal)) return false;
        document->vertices[i].normal = normal;
    }
    document->vertex_normal_count = document->vertex_count;
    document->normal_provenance =
        CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_GENERATED_SMOOTH;
    return true;
}

static void normalize_outward_winding(
    CoreMeshAssetRuntimeDocument *document) {
    if (signed_volume(document) >= 0.0) return;
    for (size_t i = 0u; i < document->triangle_count; ++i) {
        size_t swap = document->triangles[i].b;
        document->triangles[i].b = document->triangles[i].c;
        document->triangles[i].c = swap;
    }
}

static void update_bounds(CoreMeshAssetRuntimeDocument *document) {
    CoreObjectVec3 min = document->vertices[0].position;
    CoreObjectVec3 max = min;
    for (size_t i = 1u; i < document->vertex_count; ++i) {
        CoreObjectVec3 p = document->vertices[i].position;
        if (p.x < min.x) min.x = p.x;
        if (p.y < min.y) min.y = p.y;
        if (p.z < min.z) min.z = p.z;
        if (p.x > max.x) max.x = p.x;
        if (p.y > max.y) max.y = p.y;
        if (p.z > max.z) max.z = p.z;
    }
    document->contract.local_bounds.min = min;
    document->contract.local_bounds.max = max;
}

static void edge_lengths(const CoreMeshAssetRuntimeDocument *document,
                         const ShellEdge *edges,
                         size_t edge_count,
                         double *out_min,
                         double *out_max) {
    double min = HUGE_VAL;
    double max = 0.0;
    for (size_t i = 0u; i < edge_count; ++i) {
        double length = vec_length(vec_sub(
            document->vertices[edges[i].lo].position,
            document->vertices[edges[i].hi].position));
        if (length < min) min = length;
        if (length > max) max = length;
    }
    *out_min = min;
    *out_max = max;
}

static bool copy_document(const CoreMeshAssetRuntimeDocument *source,
                          const char *asset_id,
                          CoreMeshAssetRuntimeDocument *out) {
    CoreResult result;
    core_mesh_asset_runtime_document_init(out);
    result = core_mesh_asset_runtime_contract_set_asset_id(
        &out->contract, asset_id);
    if (result.code != CORE_OK) return false;
    result = core_mesh_asset_runtime_contract_set_source_asset_id(
        &out->contract, source->contract.source_asset_id);
    if (result.code != CORE_OK) return false;
    if (core_mesh_asset_runtime_document_set_vertex_count(
            out, source->vertex_count).code != CORE_OK ||
        core_mesh_asset_runtime_document_set_triangle_count(
            out, source->triangle_count).code != CORE_OK ||
        core_mesh_asset_runtime_document_set_surface_group_count(
            out, source->surface_group_count).code != CORE_OK) {
        return false;
    }
    memcpy(out->vertices, source->vertices,
           source->vertex_count * sizeof(*out->vertices));
    memcpy(out->triangles, source->triangles,
           source->triangle_count * sizeof(*out->triangles));
    memcpy(out->surface_groups, source->surface_groups,
           source->surface_group_count * sizeof(*out->surface_groups));
    out->contract.asset_type = source->contract.asset_type;
    out->contract.pivot = source->contract.pivot;
    out->contract.local_bounds = source->contract.local_bounds;
    out->contract.topology_closed_volume = source->contract.topology_closed_volume;
    out->contract.topology_manifold_expected =
        source->contract.topology_manifold_expected;
    out->vertex_normal_count = source->vertex_normal_count;
    out->normal_provenance = source->normal_provenance;
    return true;
}

static bool refine_once(CoreMeshAssetRuntimeDocument *document,
                        size_t max_vertices,
                        size_t max_triangles) {
    CoreMeshAssetRuntimeDocument next;
    ShellEdge *edges = NULL;
    size_t edge_count = 0u;
    size_t boundary = 0u;
    size_t nonmanifold = 0u;
    size_t next_vertices;
    size_t next_triangles;
    if (!collect_edges(document, &edges, &edge_count, &boundary,
                       &nonmanifold)) {
        return false;
    }
    if (edge_count > SIZE_MAX - document->vertex_count ||
        document->triangle_count > SIZE_MAX / 4u) {
        free(edges);
        return false;
    }
    next_vertices = document->vertex_count + edge_count;
    next_triangles = document->triangle_count * 4u;
    if (next_vertices > max_vertices || next_triangles > max_triangles) {
        free(edges);
        return false;
    }
    core_mesh_asset_runtime_document_init(&next);
    next.contract = document->contract;
    if (core_mesh_asset_runtime_document_set_vertex_count(
            &next, next_vertices).code != CORE_OK ||
        core_mesh_asset_runtime_document_set_triangle_count(
            &next, next_triangles).code != CORE_OK ||
        core_mesh_asset_runtime_document_set_surface_group_count(
            &next, document->surface_group_count).code != CORE_OK) {
        core_mesh_asset_runtime_document_free(&next);
        free(edges);
        return false;
    }
    memcpy(next.vertices, document->vertices,
           document->vertex_count * sizeof(*next.vertices));
    for (size_t i = 0u; i < edge_count; ++i) {
        edges[i].midpoint = document->vertex_count + i;
        next.vertices[edges[i].midpoint].position = vec_scale(
            vec_add(document->vertices[edges[i].lo].position,
                    document->vertices[edges[i].hi].position), 0.5);
    }
    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *source = &document->triangles[i];
        ShellEdge *ab = find_edge(edges, edge_count, source->a, source->b);
        ShellEdge *bc = find_edge(edges, edge_count, source->b, source->c);
        ShellEdge *ca = find_edge(edges, edge_count, source->c, source->a);
        size_t indices[4][3] = {
            {source->a, ab->midpoint, ca->midpoint},
            {ab->midpoint, source->b, bc->midpoint},
            {ca->midpoint, bc->midpoint, source->c},
            {ab->midpoint, bc->midpoint, ca->midpoint}
        };
        for (size_t child = 0u; child < 4u; ++child) {
            CoreMeshAssetRuntimeTriangle *target =
                &next.triangles[(i * 4u) + child];
            target->a = indices[child][0];
            target->b = indices[child][1];
            target->c = indices[child][2];
            snprintf(target->surface_group_id,
                     sizeof(target->surface_group_id), "%s",
                     source->surface_group_id);
        }
    }
    for (size_t i = 0u; i < document->surface_group_count; ++i) {
        next.surface_groups[i] = document->surface_groups[i];
        next.surface_groups[i].triangle_start *= 4u;
        next.surface_groups[i].triangle_count *= 4u;
    }
    next.vertex_normal_count = next.vertex_count;
    next.normal_provenance =
        CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_GENERATED_SMOOTH;
    core_mesh_asset_runtime_document_free(document);
    *document = next;
    free(edges);
    return true;
}

static double signed_volume(const CoreMeshAssetRuntimeDocument *document) {
    double volume6 = 0.0;
    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *triangle = &document->triangles[i];
        CoreObjectVec3 a = document->vertices[triangle->a].position;
        CoreObjectVec3 b = document->vertices[triangle->b].position;
        CoreObjectVec3 c = document->vertices[triangle->c].position;
        volume6 += vec_dot(a, vec_cross(b, c));
    }
    return volume6 / 6.0;
}

static size_t connected_components(
    const CoreMeshAssetRuntimeDocument *document,
    const ShellEdge *edges,
    size_t edge_count) {
    size_t *parent = malloc(document->vertex_count * sizeof(*parent));
    size_t components = document->vertex_count;
    if (!parent) return 0u;
    for (size_t i = 0u; i < document->vertex_count; ++i) parent[i] = i;
    for (size_t i = 0u; i < edge_count; ++i) {
        size_t a = edges[i].lo;
        size_t b = edges[i].hi;
        while (parent[a] != a) {
            parent[a] = parent[parent[a]];
            a = parent[a];
        }
        while (parent[b] != b) {
            parent[b] = parent[parent[b]];
            b = parent[b];
        }
        if (a != b) {
            parent[b] = a;
            --components;
        }
    }
    free(parent);
    return components;
}

static const char *vertex_group(
    const CoreMeshAssetRuntimeDocument *document,
    size_t vertex,
    const ProceduralSurfaceBindingV1 *binding) {
    const char *fallback = "";
    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *triangle = &document->triangles[i];
        if (triangle->a != vertex && triangle->b != vertex &&
            triangle->c != vertex) {
            continue;
        }
        if (!fallback[0]) fallback = triangle->surface_group_id;
        if (binding->selector == PROCEDURAL_SURFACE_SELECTOR_SURFACE_GROUP &&
            strcmp(triangle->surface_group_id,
                   binding->surface_group_id) == 0) {
            return triangle->surface_group_id;
        }
    }
    return fallback;
}

bool ProceduralSurfaceShell_Compile(
    const CoreMeshAssetRuntimeDocument *source,
    const ProceduralSurfaceFieldGraphV1 *graph,
    const ProceduralSurfaceBindingV1 *binding,
    const ProceduralSurfaceShellConfig *config,
    const char *derived_asset_id,
    CoreMeshAssetRuntimeDocument *out_document,
    ProceduralSurfaceMaterialSample **out_vertex_materials,
    ProceduralSurfaceShellSummary *out_summary,
    ProceduralSurfaceShellReport *report) {
    CoreMeshAssetRuntimeDocument result;
    ProceduralSurfaceMaterialSample *materials = NULL;
    ProceduralSurfaceShellSummary summary = {0};
    ProceduralSurfaceFieldBudget budget = {0};
    ProceduralSurfaceBindingReport binding_report;
    ShellEdge *edges = NULL;
    size_t edge_count = 0u;
    size_t boundary = 0u;
    size_t nonmanifold = 0u;
    double source_min_edge;
    double source_max_edge;
    double limit;
    CoreResult core_result;
    if (!source || !graph || !binding || !config || !derived_asset_id ||
        !out_document || !out_vertex_materials || !out_summary) {
        shell_report(report, PROCEDURAL_SURFACE_SHELL_STATUS_NULL_ARGUMENT,
                     "arguments", "shell compile arguments are required");
        return false;
    }
    core_result = core_mesh_asset_runtime_document_validate(source);
    if (core_result.code != CORE_OK || source->vertex_count == 0u ||
        source->triangle_count == 0u) {
        shell_report(report, PROCEDURAL_SURFACE_SHELL_STATUS_SOURCE_INVALID,
                     "source", "source mesh asset contract is invalid");
        return false;
    }
    if (!isfinite(config->target_edge_length_units) ||
        config->target_edge_length_units <= 0.0 ||
        !isfinite(config->max_displacement_to_source_edge_ratio) ||
        config->max_displacement_to_source_edge_ratio <= 0.0 ||
        config->max_displacement_to_source_edge_ratio >= 0.5 ||
        config->max_vertices < source->vertex_count ||
        config->max_triangles < source->triangle_count) {
        shell_report(report, PROCEDURAL_SURFACE_SHELL_STATUS_CONFIG,
                     "config", "shell refinement configuration is invalid");
        return false;
    }
    if (!collect_edges(source, &edges, &edge_count, &boundary,
                       &nonmanifold)) {
        shell_report(report, PROCEDURAL_SURFACE_SHELL_STATUS_ALLOCATION,
                     "edges", "unable to inspect source edges");
        return false;
    }
    edge_lengths(source, edges, edge_count, &source_min_edge,
                 &source_max_edge);
    if (config->require_closed_manifold &&
        (boundary != 0u || nonmanifold != 0u)) {
        free(edges);
        shell_report(report, PROCEDURAL_SURFACE_SHELL_STATUS_SOURCE_OPEN,
                     "topology",
                     "source must be a closed two-manifold shell");
        return false;
    }
    free(edges);
    edges = NULL;
    if (!copy_document(source, derived_asset_id, &result)) {
        core_mesh_asset_runtime_document_free(&result);
        shell_report(report, PROCEDURAL_SURFACE_SHELL_STATUS_ALLOCATION,
                     "copy", "unable to initialize derived shell");
        return false;
    }
    normalize_outward_winding(&result);
    if (!recompute_normals(&result)) {
        core_mesh_asset_runtime_document_free(&result);
        shell_report(report, PROCEDURAL_SURFACE_SHELL_STATUS_SOURCE_INVALID,
                     "normals", "source shell contains degenerate triangles");
        return false;
    }
    summary.source_vertex_count = source->vertex_count;
    summary.source_triangle_count = source->triangle_count;
    summary.source_min_edge_length_units = source_min_edge;
    summary.source_max_edge_length_units = source_max_edge;
    while (source_max_edge / pow(2.0, (double)summary.refinement_levels) >
               config->target_edge_length_units &&
           summary.refinement_levels < config->max_refinement_levels) {
        if (!refine_once(&result, config->max_vertices,
                         config->max_triangles)) {
            core_mesh_asset_runtime_document_free(&result);
            shell_report(report, PROCEDURAL_SURFACE_SHELL_STATUS_CAPACITY,
                         "refinement",
                         "refinement exceeds configured capacity");
            return false;
        }
        ++summary.refinement_levels;
    }
    if (!recompute_normals(&result)) {
        core_mesh_asset_runtime_document_free(&result);
        shell_report(report, PROCEDURAL_SURFACE_SHELL_STATUS_DERIVED_INVALID,
                     "normals", "refined shell contains degenerate triangles");
        return false;
    }
    materials = calloc(result.vertex_count, sizeof(*materials));
    if (!materials) {
        core_mesh_asset_runtime_document_free(&result);
        shell_report(report, PROCEDURAL_SURFACE_SHELL_STATUS_ALLOCATION,
                     "materials", "unable to allocate material samples");
        return false;
    }
    budget.max_evaluations = result.vertex_count;
    limit = source_min_edge *
            config->max_displacement_to_source_edge_ratio;
    for (size_t i = 0u; i < result.vertex_count; ++i) {
        CoreObjectVec3 source_position = result.vertices[i].position;
        CoreObjectVec3 source_normal = result.vertices[i].normal;
        ProceduralSurfaceBoundSample sample;
        ProceduralSurfaceFieldPoint3D direction;
        double displacement;
        const char *group = vertex_group(&result, i, binding);
        if (!ProceduralSurfaceBinding_Evaluate(
                binding, graph,
                (ProceduralSurfaceFieldPoint3D){
                    source_position.x, source_position.y, source_position.z},
                (ProceduralSurfaceFieldPoint3D){
                    source_normal.x, source_normal.y, source_normal.z},
                group, &budget, &sample, &binding_report)) {
            free(materials);
            core_mesh_asset_runtime_document_free(&result);
            shell_report(report, PROCEDURAL_SURFACE_SHELL_STATUS_FIELD,
                         binding_report.field, binding_report.message);
            return false;
        }
        displacement = sample.legacy_field.height *
                       binding->displacement_scale;
        if (fabs(displacement) > limit + 1.0e-12) {
            free(materials);
            core_mesh_asset_runtime_document_free(&result);
            shell_report(
                report,
                PROCEDURAL_SURFACE_SHELL_STATUS_DISPLACEMENT_LIMIT,
                "displacement",
                "field displacement exceeds source-shell safety ratio");
            return false;
        }
        if (fabs(displacement) > summary.max_abs_displacement_units) {
            summary.max_abs_displacement_units = fabs(displacement);
        }
        direction = ProceduralSurfaceBinding_DisplacementDirection(
            binding,
            (ProceduralSurfaceFieldPoint3D){
                source_normal.x, source_normal.y, source_normal.z},
            (ProceduralSurfaceFieldPoint3D){
                source_normal.x, source_normal.y, source_normal.z});
        result.vertices[i].position = vec_add(
            source_position,
            vec_scale((CoreObjectVec3){direction.x, direction.y, direction.z},
                      displacement));
        materials[i] = (ProceduralSurfaceMaterialSample){
            .final_color_r = sample.graph_sample.color_r,
            .final_color_g = sample.graph_sample.color_g,
            .final_color_b = sample.graph_sample.color_b,
            .final_roughness = sample.graph_sample.roughness,
            .snow_likelihood = sample.graph_sample.mask
        };
    }
    if (!recompute_normals(&result) ||
        !collect_edges(&result, &edges, &edge_count, &boundary,
                       &nonmanifold)) {
        free(materials);
        core_mesh_asset_runtime_document_free(&result);
        shell_report(report, PROCEDURAL_SURFACE_SHELL_STATUS_DERIVED_INVALID,
                     "topology", "derived shell topology is invalid");
        return false;
    }
    edge_lengths(&result, edges, edge_count, &source_min_edge,
                 &summary.final_max_edge_length_units);
    summary.vertex_count = result.vertex_count;
    summary.triangle_count = result.triangle_count;
    summary.unique_edge_count = edge_count;
    summary.boundary_edge_count = boundary;
    summary.nonmanifold_edge_count = nonmanifold;
    summary.connected_component_count =
        connected_components(&result, edges, edge_count);
    summary.euler_characteristic =
        (int)result.vertex_count - (int)edge_count +
        (int)result.triangle_count;
    summary.signed_volume_units3 = signed_volume(&result);
    free(edges);
    if ((config->require_closed_manifold &&
         (summary.boundary_edge_count != 0u ||
          summary.nonmanifold_edge_count != 0u ||
          summary.connected_component_count != 1u)) ||
        (config->require_positive_volume &&
         !(summary.signed_volume_units3 > 0.0))) {
        free(materials);
        core_mesh_asset_runtime_document_free(&result);
        shell_report(report, PROCEDURAL_SURFACE_SHELL_STATUS_DERIVED_INVALID,
                     "topology",
                     "derived shell failed closed-volume invariants");
        return false;
    }
    update_bounds(&result);
    result.contract.topology_closed_volume =
        summary.boundary_edge_count == 0u;
    result.contract.topology_manifold_expected =
        summary.nonmanifold_edge_count == 0u;
    core_result = core_mesh_asset_runtime_document_validate(&result);
    if (core_result.code != CORE_OK) {
        free(materials);
        core_mesh_asset_runtime_document_free(&result);
        shell_report(report, PROCEDURAL_SURFACE_SHELL_STATUS_DERIVED_INVALID,
                     "core_mesh_asset", core_result.message);
        return false;
    }
    *out_document = result;
    *out_vertex_materials = materials;
    *out_summary = summary;
    shell_report(report, PROCEDURAL_SURFACE_SHELL_STATUS_OK, "", "ok");
    return true;
}
