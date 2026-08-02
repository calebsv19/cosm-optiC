#include "render/runtime_water_body_mesh_3d.h"

#include "render/runtime_scene_3d_builder_internal.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct RuntimeWaterBodyEdge3D {
    int lo;
    int hi;
    int incidence_count;
    int forward_count;
    int reverse_count;
    int first_triangle;
} RuntimeWaterBodyEdge3D;

typedef struct RuntimeWaterBodyVertexSlot3D {
    uint64_t x_bits;
    uint64_t y_bits;
    uint64_t z_bits;
    int vertex_index;
    bool occupied;
} RuntimeWaterBodyVertexSlot3D;

typedef struct RuntimeWaterBodyEdgeSlot3D {
    int lo;
    int hi;
    int edge_index;
    bool occupied;
} RuntimeWaterBodyEdgeSlot3D;

static size_t runtime_water_body_table_capacity(size_t required_count) {
    size_t capacity = 16u;
    if (required_count > SIZE_MAX / 2u) return 0u;
    required_count *= 2u;
    while (capacity < required_count) {
        if (capacity > SIZE_MAX / 2u) return 0u;
        capacity *= 2u;
    }
    return capacity;
}

static uint64_t runtime_water_body_mix_u64(uint64_t value) {
    value ^= value >> 30u;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27u;
    value *= UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31u);
}

static uint64_t runtime_water_body_double_bits(double value) {
    uint64_t bits = 0u;
    if (value == 0.0) value = 0.0;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static Vec3 runtime_water_body_point(const RuntimeWaterBodyMesh3DDesc* desc,
                                     uint32_t x,
                                     uint32_t z,
                                     double height) {
    const double sample_x = desc->sample_origin_x + (double)x * desc->sample_spacing_x;
    const double sample_z = desc->sample_origin_z + (double)z * desc->sample_spacing_z;
    if (desc->map_y_height_to_scene_z) return vec3(sample_x, sample_z, height);
    return vec3(sample_x, height, sample_z);
}

static Vec3 runtime_water_body_top_point(const RuntimeWaterBodyMesh3DDesc* desc,
                                         uint32_t x,
                                         uint32_t z) {
    const size_t index = (size_t)z * (size_t)desc->grid_w + (size_t)x;
    return runtime_water_body_point(desc, x, z, desc->heights_y[index]);
}

static Vec3 runtime_water_body_bottom_point(const RuntimeWaterBodyMesh3DDesc* desc,
                                            uint32_t x,
                                            uint32_t z) {
    return runtime_water_body_point(desc, x, z, desc->bottom_height);
}

static bool runtime_water_body_identity_valid(const char* value) {
    return value && value[0] && strlen(value) < RUNTIME_WATER_BODY_IDENTITY_MAX;
}

static bool runtime_water_body_desc_valid(const RuntimeWaterBodyMesh3DDesc* desc) {
    uint64_t sample_count = 0u;
    uint64_t triangle_count = 0u;
    if (!desc || !desc->heights_y || desc->scene_object_index < 0) return false;
    if (!runtime_water_body_identity_valid(desc->object_id) ||
        !runtime_water_body_identity_valid(desc->material_id) ||
        !runtime_water_body_identity_valid(desc->medium_id)) {
        return false;
    }
    if (desc->grid_w < 2u || desc->grid_d < 2u ||
        !(desc->sample_spacing_x > 0.0) || !(desc->sample_spacing_z > 0.0) ||
        !isfinite(desc->sample_origin_x) || !isfinite(desc->sample_origin_z) ||
        !isfinite(desc->sample_spacing_x) || !isfinite(desc->sample_spacing_z) ||
        !isfinite(desc->bottom_height)) {
        return false;
    }
    sample_count = (uint64_t)desc->grid_w * (uint64_t)desc->grid_d;
    triangle_count = 4u * (uint64_t)(desc->grid_w - 1u) *
                         (uint64_t)(desc->grid_d - 1u) +
                     4u * ((uint64_t)(desc->grid_w - 1u) +
                           (uint64_t)(desc->grid_d - 1u));
    if (sample_count > SIZE_MAX / sizeof(*desc->heights_y) || triangle_count > INT_MAX) {
        return false;
    }
    for (uint64_t i = 0u; i < sample_count; ++i) {
        if (!isfinite(desc->heights_y[i]) ||
            !((double)desc->heights_y[i] > desc->bottom_height + 1e-9)) {
            return false;
        }
    }
    return true;
}

static int runtime_water_body_vertex_index(Vec3* vertices,
                                           int* io_vertex_count,
                                           int vertex_capacity,
                                           RuntimeWaterBodyVertexSlot3D* slots,
                                           size_t slot_capacity,
                                           Vec3 point) {
    const uint64_t x_bits = runtime_water_body_double_bits(point.x);
    const uint64_t y_bits = runtime_water_body_double_bits(point.y);
    const uint64_t z_bits = runtime_water_body_double_bits(point.z);
    const uint64_t hash = runtime_water_body_mix_u64(x_bits) ^
                          runtime_water_body_mix_u64(y_bits + UINT64_C(0x9e3779b97f4a7c15)) ^
                          runtime_water_body_mix_u64(z_bits + UINT64_C(0x3c6ef372fe94f82a));
    size_t slot = 0u;
    if (!vertices || !io_vertex_count || !slots || slot_capacity == 0u ||
        (slot_capacity & (slot_capacity - 1u)) != 0u) {
        return -1;
    }
    slot = (size_t)hash & (slot_capacity - 1u);
    for (size_t probe = 0u; probe < slot_capacity; ++probe) {
        RuntimeWaterBodyVertexSlot3D* entry = &slots[slot];
        if (!entry->occupied) {
            if (*io_vertex_count >= vertex_capacity) return -1;
            entry->x_bits = x_bits;
            entry->y_bits = y_bits;
            entry->z_bits = z_bits;
            entry->vertex_index = *io_vertex_count;
            entry->occupied = true;
            vertices[*io_vertex_count] = point;
            *io_vertex_count += 1;
            return *io_vertex_count - 1;
        }
        if (entry->x_bits == x_bits && entry->y_bits == y_bits &&
            entry->z_bits == z_bits) {
            return entry->vertex_index;
        }
        slot = (slot + 1u) & (slot_capacity - 1u);
    }
    return -1;
}

static int runtime_water_body_dsu_find(int* parents, int value) {
    int root = value;
    while (parents[root] != root) root = parents[root];
    while (parents[value] != value) {
        const int next = parents[value];
        parents[value] = root;
        value = next;
    }
    return root;
}

static void runtime_water_body_dsu_union(int* parents, int a, int b) {
    const int root_a = runtime_water_body_dsu_find(parents, a);
    const int root_b = runtime_water_body_dsu_find(parents, b);
    if (root_a != root_b) parents[root_b] = root_a;
}

static bool runtime_water_body_add_edge(RuntimeWaterBodyEdge3D* edges,
                                        int* io_edge_count,
                                        int edge_capacity,
                                        RuntimeWaterBodyEdgeSlot3D* slots,
                                        size_t slot_capacity,
                                        int* parents,
                                        int triangle_index,
                                        int from,
                                        int to) {
    const int lo = from < to ? from : to;
    const int hi = from < to ? to : from;
    const bool forward = from == lo;
    const uint64_t key = ((uint64_t)(uint32_t)lo << 32u) | (uint32_t)hi;
    size_t slot = 0u;
    if (!edges || !io_edge_count || !slots || slot_capacity == 0u ||
        (slot_capacity & (slot_capacity - 1u)) != 0u) {
        return false;
    }
    slot = (size_t)runtime_water_body_mix_u64(key) & (slot_capacity - 1u);
    for (size_t probe = 0u; probe < slot_capacity; ++probe) {
        RuntimeWaterBodyEdgeSlot3D* entry = &slots[slot];
        if (entry->occupied && entry->lo == lo && entry->hi == hi) {
            RuntimeWaterBodyEdge3D* edge = &edges[entry->edge_index];
            edge->incidence_count += 1;
            if (forward) edge->forward_count += 1;
            else edge->reverse_count += 1;
            runtime_water_body_dsu_union(parents, triangle_index, edge->first_triangle);
            return true;
        }
        if (!entry->occupied) {
            if (*io_edge_count >= edge_capacity) return false;
            edges[*io_edge_count].lo = lo;
            edges[*io_edge_count].hi = hi;
            edges[*io_edge_count].incidence_count = 1;
            edges[*io_edge_count].forward_count = forward ? 1 : 0;
            edges[*io_edge_count].reverse_count = forward ? 0 : 1;
            edges[*io_edge_count].first_triangle = triangle_index;
            entry->lo = lo;
            entry->hi = hi;
            entry->edge_index = *io_edge_count;
            entry->occupied = true;
            *io_edge_count += 1;
            return true;
        }
        slot = (slot + 1u) & (slot_capacity - 1u);
    }
    return false;
}

static uint64_t runtime_water_body_hash_u64(uint64_t hash, uint64_t value) {
    for (int byte = 0; byte < 8; ++byte) {
        hash ^= (value >> (byte * 8)) & 0xffu;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static bool runtime_water_body_audit(const RuntimeScene3D* scene,
                                     int first_triangle,
                                     RuntimeWaterBodyMesh3DReport* report) {
    const int triangle_count = report ? report->total_triangle_count : 0;
    const int edge_capacity = triangle_count * 3;
    const int vertex_capacity = triangle_count * 3;
    Vec3* vertices = NULL;
    RuntimeWaterBodyEdge3D* edges = NULL;
    RuntimeWaterBodyVertexSlot3D* vertex_slots = NULL;
    RuntimeWaterBodyEdgeSlot3D* edge_slots = NULL;
    int* parents = NULL;
    const size_t vertex_slot_capacity =
        runtime_water_body_table_capacity((size_t)vertex_capacity);
    const size_t edge_slot_capacity =
        runtime_water_body_table_capacity((size_t)edge_capacity);
    int vertex_count = 0;
    int edge_count = 0;
    bool ok = true;
    bool orientation_consistent = true;
    uint64_t topology_hash = UINT64_C(1469598103934665603);
    if (!scene || !report || first_triangle < 0 || triangle_count <= 0 ||
        first_triangle + triangle_count > scene->triangleMesh.triangleCount) {
        return false;
    }
    vertices = (Vec3*)calloc((size_t)vertex_capacity, sizeof(*vertices));
    edges = (RuntimeWaterBodyEdge3D*)calloc((size_t)edge_capacity, sizeof(*edges));
    vertex_slots = (RuntimeWaterBodyVertexSlot3D*)calloc(vertex_slot_capacity,
                                                         sizeof(*vertex_slots));
    edge_slots = (RuntimeWaterBodyEdgeSlot3D*)calloc(edge_slot_capacity,
                                                     sizeof(*edge_slots));
    parents = (int*)calloc((size_t)triangle_count, sizeof(*parents));
    if (!vertices || !edges || !vertex_slots || !edge_slots || !parents) {
        free(vertices);
        free(edges);
        free(vertex_slots);
        free(edge_slots);
        free(parents);
        return false;
    }
    for (int i = 0; i < triangle_count; ++i) parents[i] = i;
    for (int i = 0; i < triangle_count && ok; ++i) {
        const RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[first_triangle + i];
        const Vec3 points[3] = {triangle->p0, triangle->p1, triangle->p2};
        int indices[3] = {-1, -1, -1};
        const Vec3 cross = vec3_cross(vec3_sub(triangle->p1, triangle->p0),
                                      vec3_sub(triangle->p2, triangle->p0));
        if (triangle->sceneObjectIndex != report->scene_object_index ||
            vec3_length(cross) <= 1e-12 || !isfinite(vec3_length(cross))) {
            orientation_consistent = false;
        }
        report->signed_volume += vec3_dot(triangle->p0,
                                          vec3_cross(triangle->p1, triangle->p2)) /
                                 6.0;
        for (int corner = 0; corner < 3; ++corner) {
            indices[corner] = runtime_water_body_vertex_index(vertices,
                                                               &vertex_count,
                                                               vertex_capacity,
                                                               vertex_slots,
                                                               vertex_slot_capacity,
                                                               points[corner]);
            if (indices[corner] < 0) ok = false;
        }
        if (ok) {
            ok = runtime_water_body_add_edge(edges,
                                             &edge_count,
                                             edge_capacity,
                                             edge_slots,
                                             edge_slot_capacity,
                                             parents,
                                             i,
                                             indices[0],
                                             indices[1]) &&
                 runtime_water_body_add_edge(edges,
                                             &edge_count,
                                             edge_capacity,
                                             edge_slots,
                                             edge_slot_capacity,
                                             parents,
                                             i,
                                             indices[1],
                                             indices[2]) &&
                 runtime_water_body_add_edge(edges,
                                             &edge_count,
                                             edge_capacity,
                                             edge_slots,
                                             edge_slot_capacity,
                                             parents,
                                             i,
                                             indices[2],
                                             indices[0]);
        }
    }
    if (ok) {
        for (int i = 0; i < edge_count; ++i) {
            if (edges[i].incidence_count == 1) report->boundary_edge_count += 1;
            if (edges[i].incidence_count > 2) report->nonmanifold_edge_count += 1;
            if (edges[i].incidence_count != 2 || edges[i].forward_count != 1 ||
                edges[i].reverse_count != 1) {
                orientation_consistent = false;
            }
            topology_hash = runtime_water_body_hash_u64(topology_hash, (uint64_t)edges[i].lo);
            topology_hash = runtime_water_body_hash_u64(topology_hash, (uint64_t)edges[i].hi);
            topology_hash = runtime_water_body_hash_u64(
                topology_hash, (uint64_t)edges[i].incidence_count);
        }
        for (int i = 0; i < triangle_count; ++i) {
            if (runtime_water_body_dsu_find(parents, i) == i) {
                report->connected_component_count += 1;
            }
        }
    }
    report->winding_consistent =
        ok && orientation_consistent && report->signed_volume > 0.0;
    report->topology_signature = topology_hash;
    report->topology_valid = ok && report->object_count == 1 &&
                             report->connected_component_count == 1 &&
                             report->boundary_edge_count == 0 &&
                             report->nonmanifold_edge_count == 0 &&
                             report->winding_consistent &&
                             report->max_perimeter_seam_error <= 1e-6;
    free(vertices);
    free(edges);
    free(vertex_slots);
    free(edge_slots);
    free(parents);
    return ok;
}

static double runtime_water_body_point_distance(Vec3 a, Vec3 b) {
    return vec3_length(vec3_sub(a, b));
}

static double runtime_water_body_perimeter_seam_error(
    const RuntimeScene3D* scene,
    const RuntimeWaterBodyMesh3DDesc* desc,
    int first_side_triangle,
    int side_triangle_count) {
    double max_error = 0.0;
    if (!scene || !desc || first_side_triangle < 0 || side_triangle_count <= 0) {
        return DBL_MAX;
    }
    for (uint32_t z = 0u; z < desc->grid_d; ++z) {
        for (uint32_t x = 0u; x < desc->grid_w; ++x) {
            double closest = DBL_MAX;
            Vec3 expected;
            if (x != 0u && x + 1u != desc->grid_w &&
                z != 0u && z + 1u != desc->grid_d) {
                continue;
            }
            expected = runtime_water_body_top_point(desc, x, z);
            for (int i = 0; i < side_triangle_count; ++i) {
                const RuntimeTriangle3D* triangle =
                    &scene->triangleMesh.triangles[first_side_triangle + i];
                const Vec3 points[3] = {triangle->p0, triangle->p1, triangle->p2};
                for (int corner = 0; corner < 3; ++corner) {
                    const double distance =
                        runtime_water_body_point_distance(expected, points[corner]);
                    if (distance < closest) closest = distance;
                }
            }
            if (closest > max_error) max_error = closest;
        }
    }
    return max_error;
}

static bool runtime_water_body_append_quad(RuntimeScene3D* scene,
                                           int primitive_index,
                                           int scene_object_index,
                                           Vec3 p0,
                                           Vec3 p1,
                                           Vec3 p2,
                                           Vec3 p3,
                                           Vec3 outward) {
    return runtime_scene_3d_builder_append_quad(scene,
                                                primitive_index,
                                                scene_object_index,
                                                p0,
                                                p1,
                                                p2,
                                                p3,
                                                outward,
                                                false);
}

bool RuntimeWaterBodyMesh3D_Append(RuntimeScene3D* scene,
                                   const RuntimeWaterBodyMesh3DDesc* desc,
                                   RuntimeWaterBodyMesh3DReport* out_report) {
    const int old_primitive_count = scene ? scene->primitiveCount : 0;
    const int old_triangle_count = scene ? scene->triangleMesh.triangleCount : 0;
    const int top_triangle_count = desc ? (int)(2u * (desc->grid_w - 1u) *
                                                 (desc->grid_d - 1u)) : 0;
    const int side_triangle_count = desc ? (int)(4u * ((desc->grid_w - 1u) +
                                                       (desc->grid_d - 1u))) : 0;
    const int bottom_triangle_count = top_triangle_count;
    const int total_triangle_count = top_triangle_count + side_triangle_count +
                                     bottom_triangle_count;
    const Vec3 up = desc && desc->map_y_height_to_scene_z ? vec3(0.0, 0.0, 1.0)
                                                          : vec3(0.0, 1.0, 0.0);
    const Vec3 axis_z = desc && desc->map_y_height_to_scene_z ? vec3(0.0, 1.0, 0.0)
                                                              : vec3(0.0, 0.0, 1.0);
    RuntimePrimitive3D* primitive = NULL;
    int primitive_index = 0;
    bool ok = true;
    if (out_report) memset(out_report, 0, sizeof(*out_report));
    if (!scene || !out_report || !runtime_water_body_desc_valid(desc)) return false;
    if (!runtime_scene_3d_builder_reserve_primitives(scene, scene->primitiveCount + 1) ||
        !runtime_scene_3d_builder_reserve_triangles(scene,
                                                    scene->triangleMesh.triangleCount +
                                                        total_triangle_count)) {
        return false;
    }
    primitive_index = scene->primitiveCount;
    primitive = &scene->primitives[primitive_index];
    memset(primitive, 0, sizeof(*primitive));
    primitive->kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    primitive->source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    primitive->source.sceneObjectIndex = desc->scene_object_index;
    snprintf(primitive->source.objectId, sizeof(primitive->source.objectId), "%s", desc->object_id);

    for (uint32_t z = 0u; z + 1u < desc->grid_d && ok; ++z) {
        for (uint32_t x = 0u; x + 1u < desc->grid_w && ok; ++x) {
            ok = runtime_water_body_append_quad(scene,
                                                primitive_index,
                                                desc->scene_object_index,
                                                runtime_water_body_top_point(desc, x, z),
                                                runtime_water_body_top_point(desc, x, z + 1u),
                                                runtime_water_body_top_point(desc, x + 1u, z + 1u),
                                                runtime_water_body_top_point(desc, x + 1u, z),
                                                up);
        }
    }
    for (uint32_t x = 0u; x + 1u < desc->grid_w && ok; ++x) {
        ok = runtime_water_body_append_quad(scene,
                                            primitive_index,
                                            desc->scene_object_index,
                                            runtime_water_body_top_point(desc, x, 0u),
                                            runtime_water_body_top_point(desc, x + 1u, 0u),
                                            runtime_water_body_bottom_point(desc, x + 1u, 0u),
                                            runtime_water_body_bottom_point(desc, x, 0u),
                                            vec3_scale(axis_z, -1.0));
        if (ok) {
            ok = runtime_water_body_append_quad(
                scene,
                primitive_index,
                desc->scene_object_index,
                runtime_water_body_top_point(desc, x + 1u, desc->grid_d - 1u),
                runtime_water_body_top_point(desc, x, desc->grid_d - 1u),
                runtime_water_body_bottom_point(desc, x, desc->grid_d - 1u),
                runtime_water_body_bottom_point(desc, x + 1u, desc->grid_d - 1u),
                axis_z);
        }
    }
    for (uint32_t z = 0u; z + 1u < desc->grid_d && ok; ++z) {
        ok = runtime_water_body_append_quad(
            scene,
            primitive_index,
            desc->scene_object_index,
            runtime_water_body_top_point(desc, desc->grid_w - 1u, z),
            runtime_water_body_top_point(desc, desc->grid_w - 1u, z + 1u),
            runtime_water_body_bottom_point(desc, desc->grid_w - 1u, z + 1u),
            runtime_water_body_bottom_point(desc, desc->grid_w - 1u, z),
            vec3(1.0, 0.0, 0.0));
        if (ok) {
            ok = runtime_water_body_append_quad(scene,
                                                primitive_index,
                                                desc->scene_object_index,
                                                runtime_water_body_top_point(desc, 0u, z + 1u),
                                                runtime_water_body_top_point(desc, 0u, z),
                                                runtime_water_body_bottom_point(desc, 0u, z),
                                                runtime_water_body_bottom_point(desc, 0u, z + 1u),
                                                vec3(-1.0, 0.0, 0.0));
        }
    }
    for (uint32_t z = 0u; z + 1u < desc->grid_d && ok; ++z) {
        for (uint32_t x = 0u; x + 1u < desc->grid_w && ok; ++x) {
            ok = runtime_water_body_append_quad(scene,
                                                primitive_index,
                                                desc->scene_object_index,
                                                runtime_water_body_bottom_point(desc, x, z),
                                                runtime_water_body_bottom_point(desc, x + 1u, z),
                                                runtime_water_body_bottom_point(desc, x + 1u, z + 1u),
                                                runtime_water_body_bottom_point(desc, x, z + 1u),
                                                vec3_scale(up, -1.0));
        }
    }
    if (!ok || scene->triangleMesh.triangleCount != old_triangle_count + total_triangle_count) {
        scene->primitiveCount = old_primitive_count;
        scene->triangleMesh.triangleCount = old_triangle_count;
        scene->triangleMesh.bvhDirty = true;
        (void)runtime_scene_3d_builder_rebuild_bvh(scene);
        return false;
    }

    scene->primitiveCount += 1;
    scene->scope.triangleMeshEnabled = true;
    snprintf(out_report->object_id, sizeof(out_report->object_id), "%s", desc->object_id);
    snprintf(out_report->material_id, sizeof(out_report->material_id), "%s", desc->material_id);
    snprintf(out_report->medium_id, sizeof(out_report->medium_id), "%s", desc->medium_id);
    out_report->scene_object_index = desc->scene_object_index;
    out_report->object_count = 1;
    out_report->top_triangle_count = top_triangle_count;
    out_report->side_triangle_count = side_triangle_count;
    out_report->bottom_triangle_count = bottom_triangle_count;
    out_report->total_triangle_count = total_triangle_count;
    out_report->max_perimeter_seam_error = runtime_water_body_perimeter_seam_error(
        scene,
        desc,
        old_triangle_count + top_triangle_count,
        side_triangle_count);
    if (!runtime_water_body_audit(scene, old_triangle_count, out_report) ||
        !out_report->topology_valid) {
        scene->primitiveCount = old_primitive_count;
        scene->triangleMesh.triangleCount = old_triangle_count;
        scene->triangleMesh.bvhDirty = true;
        (void)runtime_scene_3d_builder_rebuild_bvh(scene);
        memset(out_report, 0, sizeof(*out_report));
        return false;
    }
    scene->triangleMesh.bvhDirty = true;
    return true;
}
