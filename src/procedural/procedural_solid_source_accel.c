#include "procedural/procedural_solid_source_accel.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SOLID_ACCEL_NONE SIZE_MAX

static CoreObjectVec3 vec_sub(CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){a.x - b.x, a.y - b.y, a.z - b.z};
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

static double component(CoreObjectVec3 value, int axis) {
    return axis == 0 ? value.x : (axis == 1 ? value.y : value.z);
}

static CoreObjectVec3 vec_min(CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){
        fmin(a.x, b.x), fmin(a.y, b.y), fmin(a.z, b.z)};
}

static CoreObjectVec3 vec_max(CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){
        fmax(a.x, b.x), fmax(a.y, b.y), fmax(a.z, b.z)};
}

void ProceduralSolidSourceAccel_Init(ProceduralSolidSourceAccel *accel) {
    if (!accel) return;
    memset(accel, 0, sizeof(*accel));
}

void ProceduralSolidSourceAccel_Free(ProceduralSolidSourceAccel *accel) {
    if (!accel) return;
    free(accel->nodes);
    free(accel->triangles);
    ProceduralSolidSourceAccel_Init(accel);
}

static int triangle_compare(
    const ProceduralSolidSourceAccelTriangle *a,
    const ProceduralSolidSourceAccelTriangle *b,
    int axis) {
    const double ca = component(a->centroid, axis);
    const double cb = component(b->centroid, axis);
    if (ca < cb) return -1;
    if (ca > cb) return 1;
    if (a->triangle_index < b->triangle_index) return -1;
    if (a->triangle_index > b->triangle_index) return 1;
    return 0;
}

static void heap_sift(
    ProceduralSolidSourceAccelTriangle *values,
    size_t count,
    size_t root,
    int axis) {
    for (;;) {
        size_t child = root * 2u + 1u;
        size_t selected = root;
        ProceduralSolidSourceAccelTriangle swap;
        if (child < count &&
            triangle_compare(&values[selected], &values[child], axis) < 0) {
            selected = child;
        }
        if (child + 1u < count &&
            triangle_compare(
                &values[selected], &values[child + 1u], axis) < 0) {
            selected = child + 1u;
        }
        if (selected == root) return;
        swap = values[root];
        values[root] = values[selected];
        values[selected] = swap;
        root = selected;
    }
}

static void sort_triangles(
    ProceduralSolidSourceAccelTriangle *values,
    size_t count,
    int axis) {
    if (count < 2u) return;
    for (size_t i = count / 2u; i > 0u; --i) {
        heap_sift(values, count, i - 1u, axis);
    }
    for (size_t end = count; end > 1u; --end) {
        ProceduralSolidSourceAccelTriangle swap = values[0];
        values[0] = values[end - 1u];
        values[end - 1u] = swap;
        heap_sift(values, end - 1u, 0u, axis);
    }
}

static bool build_node(
    ProceduralSolidSourceAccel *accel,
    size_t start,
    size_t count,
    size_t depth,
    size_t *out_index) {
    ProceduralSolidSourceAccelNode *node;
    CoreObjectVec3 centroid_min;
    CoreObjectVec3 centroid_max;
    size_t index;
    int axis = 0;
    if (!accel || !out_index || count == 0u ||
        accel->node_count >= accel->node_capacity) {
        return false;
    }
    index = accel->node_count++;
    node = &accel->nodes[index];
    memset(node, 0, sizeof(*node));
    node->start = start;
    node->count = count;
    node->left = SOLID_ACCEL_NONE;
    node->right = SOLID_ACCEL_NONE;
    node->bounds_min = accel->triangles[start].bounds_min;
    node->bounds_max = accel->triangles[start].bounds_max;
    centroid_min = accel->triangles[start].centroid;
    centroid_max = centroid_min;
    for (size_t i = start + 1u; i < start + count; ++i) {
        node->bounds_min =
            vec_min(node->bounds_min, accel->triangles[i].bounds_min);
        node->bounds_max =
            vec_max(node->bounds_max, accel->triangles[i].bounds_max);
        centroid_min = vec_min(centroid_min, accel->triangles[i].centroid);
        centroid_max = vec_max(centroid_max, accel->triangles[i].centroid);
    }
    if (depth > accel->maximum_depth) accel->maximum_depth = depth;
    if (count <= accel->leaf_size) {
        node->leaf = true;
        *out_index = index;
        return true;
    }
    {
        const CoreObjectVec3 extent = vec_sub(centroid_max, centroid_min);
        if (extent.y > extent.x) axis = 1;
        if (component(extent, 2) > component(extent, axis)) axis = 2;
    }
    sort_triangles(&accel->triangles[start], count, axis);
    {
        const size_t left_count = count / 2u;
        const size_t right_count = count - left_count;
        size_t left;
        size_t right;
        if (!build_node(accel, start, left_count, depth + 1u, &left) ||
            !build_node(
                accel, start + left_count, right_count, depth + 1u,
                &right)) {
            return false;
        }
        node = &accel->nodes[index];
        node->left = left;
        node->right = right;
    }
    *out_index = index;
    return true;
}

bool ProceduralSolidSourceAccel_Build(
    const CoreMeshAssetRuntimeDocument *mesh,
    size_t leaf_size,
    ProceduralSolidSourceAccel *out_accel) {
    ProceduralSolidSourceAccel accel;
    size_t root;
    if (!mesh || !out_accel || mesh->triangle_count == 0u ||
        !mesh->vertices || !mesh->triangles || leaf_size < 2u ||
        leaf_size > 64u ||
        !mesh->contract.topology_closed_volume ||
        !mesh->contract.topology_manifold_expected ||
        core_mesh_asset_runtime_document_validate(mesh).code != CORE_OK ||
        mesh->triangle_count > (SIZE_MAX - 1u) / 2u) {
        return false;
    }
    ProceduralSolidSourceAccel_Init(&accel);
    accel.mesh = mesh;
    accel.triangle_count = mesh->triangle_count;
    accel.leaf_size = leaf_size;
    accel.node_capacity = mesh->triangle_count * 2u - 1u;
    accel.triangles = calloc(
        accel.triangle_count, sizeof(*accel.triangles));
    accel.nodes = calloc(accel.node_capacity, sizeof(*accel.nodes));
    if (!accel.triangles || !accel.nodes) {
        ProceduralSolidSourceAccel_Free(&accel);
        return false;
    }
    for (size_t i = 0u; i < mesh->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *triangle = &mesh->triangles[i];
        const CoreObjectVec3 a = mesh->vertices[triangle->a].position;
        const CoreObjectVec3 b = mesh->vertices[triangle->b].position;
        const CoreObjectVec3 c = mesh->vertices[triangle->c].position;
        ProceduralSolidSourceAccelTriangle *entry = &accel.triangles[i];
        entry->triangle_index = i;
        entry->bounds_min = vec_min(a, vec_min(b, c));
        entry->bounds_max = vec_max(a, vec_max(b, c));
        entry->centroid = (CoreObjectVec3){
            (a.x + b.x + c.x) / 3.0,
            (a.y + b.y + c.y) / 3.0,
            (a.z + b.z + c.z) / 3.0};
    }
    if (!build_node(&accel, 0u, accel.triangle_count, 1u, &root) ||
        root != 0u) {
        ProceduralSolidSourceAccel_Free(&accel);
        return false;
    }
    *out_accel = accel;
    return true;
}

static double point_aabb_distance2(
    CoreObjectVec3 point,
    CoreObjectVec3 bounds_min,
    CoreObjectVec3 bounds_max) {
    double total = 0.0;
#define AXIS_DISTANCE(VALUE, MINIMUM, MAXIMUM) do { \
    double delta = 0.0; \
    if ((VALUE) < (MINIMUM)) delta = (MINIMUM) - (VALUE); \
    else if ((VALUE) > (MAXIMUM)) delta = (VALUE) - (MAXIMUM); \
    total += delta * delta; \
} while (0)
    AXIS_DISTANCE(point.x, bounds_min.x, bounds_max.x);
    AXIS_DISTANCE(point.y, bounds_min.y, bounds_max.y);
    AXIS_DISTANCE(point.z, bounds_min.z, bounds_max.z);
#undef AXIS_DISTANCE
    return total;
}

static double point_triangle_distance(
    CoreObjectVec3 p,
    CoreObjectVec3 a,
    CoreObjectVec3 b,
    CoreObjectVec3 c) {
    const CoreObjectVec3 ab = vec_sub(b, a);
    const CoreObjectVec3 ac = vec_sub(c, a);
    const CoreObjectVec3 ap = vec_sub(p, a);
    const double d1 = vec_dot(ab, ap);
    const double d2 = vec_dot(ac, ap);
    if (d1 <= 0.0 && d2 <= 0.0) return vec_length(ap);
    {
        const CoreObjectVec3 bp = vec_sub(p, b);
        const double d3 = vec_dot(ab, bp);
        const double d4 = vec_dot(ac, bp);
        if (d3 >= 0.0 && d4 <= d3) return vec_length(bp);
        {
            const double vc = d1 * d4 - d3 * d2;
            if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) {
                const double v = d1 / (d1 - d3);
                const CoreObjectVec3 q = {
                    a.x + v * ab.x, a.y + v * ab.y, a.z + v * ab.z};
                return vec_length(vec_sub(p, q));
            }
        }
        {
            const CoreObjectVec3 cp = vec_sub(p, c);
            const double d5 = vec_dot(ab, cp);
            const double d6 = vec_dot(ac, cp);
            if (d6 >= 0.0 && d5 <= d6) return vec_length(cp);
            {
                const double vb = d5 * d2 - d1 * d6;
                if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) {
                    const double w = d2 / (d2 - d6);
                    const CoreObjectVec3 q = {
                        a.x + w * ac.x, a.y + w * ac.y,
                        a.z + w * ac.z};
                    return vec_length(vec_sub(p, q));
                }
            }
            {
                const double va = d3 * d6 - d5 * d4;
                if (va <= 0.0 && (d4 - d3) >= 0.0 &&
                    (d5 - d6) >= 0.0) {
                    const CoreObjectVec3 bc = vec_sub(c, b);
                    const double w =
                        (d4 - d3) / ((d4 - d3) + (d5 - d6));
                    const CoreObjectVec3 q = {
                        b.x + w * bc.x, b.y + w * bc.y,
                        b.z + w * bc.z};
                    return vec_length(vec_sub(p, q));
                }
            }
        }
    }
    {
        const CoreObjectVec3 normal = vec_cross(ab, ac);
        const double length = vec_length(normal);
        return length <= DBL_EPSILON
            ? HUGE_VAL
            : fabs(vec_dot(ap, normal)) / length;
    }
}

static void nearest_node(
    const ProceduralSolidSourceAccel *accel,
    size_t node_index,
    CoreObjectVec3 point,
    double *best,
    ProceduralSolidSourceQuery *query) {
    const ProceduralSolidSourceAccelNode *node = &accel->nodes[node_index];
    const double bound_distance2 =
        point_aabb_distance2(point, node->bounds_min, node->bounds_max);
    ++query->nodes_visited;
    if (bound_distance2 > (*best) * (*best)) return;
    if (node->leaf) {
        for (size_t i = node->start; i < node->start + node->count; ++i) {
            const size_t triangle_index =
                accel->triangles[i].triangle_index;
            const CoreMeshAssetRuntimeTriangle *triangle =
                &accel->mesh->triangles[triangle_index];
            const double distance = point_triangle_distance(
                point,
                accel->mesh->vertices[triangle->a].position,
                accel->mesh->vertices[triangle->b].position,
                accel->mesh->vertices[triangle->c].position);
            ++query->distance_triangle_tests;
            if (distance < *best) *best = distance;
        }
        return;
    }
    {
        const ProceduralSolidSourceAccelNode *left =
            &accel->nodes[node->left];
        const ProceduralSolidSourceAccelNode *right =
            &accel->nodes[node->right];
        const double left_distance = point_aabb_distance2(
            point, left->bounds_min, left->bounds_max);
        const double right_distance = point_aabb_distance2(
            point, right->bounds_min, right->bounds_max);
        if (left_distance <= right_distance) {
            nearest_node(accel, node->left, point, best, query);
            nearest_node(accel, node->right, point, best, query);
        } else {
            nearest_node(accel, node->right, point, best, query);
            nearest_node(accel, node->left, point, best, query);
        }
    }
}

static bool ray_intersects_aabb(
    CoreObjectVec3 origin,
    CoreObjectVec3 direction,
    CoreObjectVec3 bounds_min,
    CoreObjectVec3 bounds_max) {
    double t_min = 0.0;
    double t_max = HUGE_VAL;
    const double origin_values[3] = {origin.x, origin.y, origin.z};
    const double direction_values[3] = {
        direction.x, direction.y, direction.z};
    const double min_values[3] = {
        bounds_min.x, bounds_min.y, bounds_min.z};
    const double max_values[3] = {
        bounds_max.x, bounds_max.y, bounds_max.z};
    for (size_t axis = 0u; axis < 3u; ++axis) {
        const double inverse = 1.0 / direction_values[axis];
        double a = (min_values[axis] - origin_values[axis]) * inverse;
        double b = (max_values[axis] - origin_values[axis]) * inverse;
        if (a > b) {
            const double swap = a;
            a = b;
            b = swap;
        }
        if (a > t_min) t_min = a;
        if (b < t_max) t_max = b;
        if (t_max < t_min) return false;
    }
    return t_max > 1.0e-12;
}

static bool ray_triangle_hit(
    CoreObjectVec3 origin,
    CoreObjectVec3 direction,
    CoreObjectVec3 a,
    CoreObjectVec3 b,
    CoreObjectVec3 c) {
    const CoreObjectVec3 edge1 = vec_sub(b, a);
    const CoreObjectVec3 edge2 = vec_sub(c, a);
    const CoreObjectVec3 p = vec_cross(direction, edge2);
    const double determinant = vec_dot(edge1, p);
    CoreObjectVec3 t;
    CoreObjectVec3 q;
    double u;
    double v;
    double distance;
    if (fabs(determinant) <= 1.0e-13) return false;
    t = vec_sub(origin, a);
    u = vec_dot(t, p) / determinant;
    if (u < -1.0e-12 || u > 1.0 + 1.0e-12) return false;
    q = vec_cross(t, edge1);
    v = vec_dot(direction, q) / determinant;
    if (v < -1.0e-12 || u + v > 1.0 + 1.0e-12) return false;
    distance = vec_dot(edge2, q) / determinant;
    return distance > 1.0e-10;
}

static size_t ray_count_node(
    const ProceduralSolidSourceAccel *accel,
    size_t node_index,
    CoreObjectVec3 origin,
    CoreObjectVec3 direction,
    ProceduralSolidSourceQuery *query) {
    const ProceduralSolidSourceAccelNode *node = &accel->nodes[node_index];
    size_t count = 0u;
    ++query->nodes_visited;
    if (!ray_intersects_aabb(
            origin, direction, node->bounds_min, node->bounds_max)) {
        return 0u;
    }
    if (!node->leaf) {
        return ray_count_node(
                   accel, node->left, origin, direction, query) +
               ray_count_node(
                   accel, node->right, origin, direction, query);
    }
    for (size_t i = node->start; i < node->start + node->count; ++i) {
        const CoreMeshAssetRuntimeTriangle *triangle =
            &accel->mesh->triangles[
                accel->triangles[i].triangle_index];
        ++query->sign_triangle_tests;
        if (ray_triangle_hit(
                origin, direction,
                accel->mesh->vertices[triangle->a].position,
                accel->mesh->vertices[triangle->b].position,
                accel->mesh->vertices[triangle->c].position)) {
            ++count;
        }
    }
    return count;
}

bool ProceduralSolidSourceAccel_Query(
    const ProceduralSolidSourceAccel *accel,
    CoreObjectVec3 point,
    ProceduralSolidSourceQuery *out_query) {
    static const CoreObjectVec3 directions[3] = {
        {1.0, 0.3713906763541037, 0.52999894000318},
        {0.293177, 1.0, 0.6172133998483676},
        {0.443113462726379, 0.274921, 1.0}};
    ProceduralSolidSourceQuery query;
    double minimum = HUGE_VAL;
    size_t inside_votes = 0u;
    if (!accel || !out_query || !accel->mesh || !accel->nodes ||
        !accel->triangles || accel->node_count == 0u ||
        !isfinite(point.x) || !isfinite(point.y) || !isfinite(point.z)) {
        return false;
    }
    memset(&query, 0, sizeof(query));
    nearest_node(accel, 0u, point, &minimum, &query);
    if (!isfinite(minimum)) return false;
    for (size_t i = 0u; i < 3u; ++i) {
        if ((ray_count_node(
                 accel, 0u, point, directions[i], &query) & 1u) != 0u) {
            ++inside_votes;
        }
    }
    query.signed_distance = inside_votes >= 2u ? -minimum : minimum;
    *out_query = query;
    return true;
}
