#include "render/runtime_caustic_photon_sample_support_3d.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "render/runtime_caustic_photon_surface_kernel_3d.h"

/* Temporary balanced tree; optical records never move. Counts use whole
 * subtrees only after conservative bounds and uniform receiver predicates
 * prove every point qualifies. A separate nearest search tightens its radius
 * without truncating the full count required by the public readback. */
typedef struct { Vec3 position; uint64_t index; } SupportPoint;
typedef struct {
    Vec3 min, max, normal;
    uint64_t begin, count, left, right;
    int object, material;
    bool uniformObject, uniformMaterial, uniformNormal;
} SupportNode;
typedef struct {
    RuntimeCausticPhotonMapRecord3D* records;
    uint64_t self, limit, count, heapCount, tested;
    double minNormalDot;
    double* heap;
} SupportQuery;

static double component(Vec3 p, unsigned axis) {
    return axis == 0u ? p.x : axis == 1u ? p.y : p.z;
}
#define SUPPORT_COMPARE(name, axis) \
static int name(const void* a, const void* b) { \
    const SupportPoint* x = a; const SupportPoint* y = b; \
    double lhs = component(x->position, axis), rhs = component(y->position, axis); \
    return lhs < rhs ? -1 : lhs > rhs ? 1 : x->index < y->index ? -1 : x->index > y->index; \
}
SUPPORT_COMPARE(compare_x, 0u)
SUPPORT_COMPARE(compare_y, 1u)
SUPPORT_COMPARE(compare_z, 2u)
#undef SUPPORT_COMPARE

static uint64_t build(SupportPoint* points, uint64_t begin, uint64_t count,
                      unsigned depth, SupportNode* nodes, uint64_t* used,
                      const RuntimeCausticPhotonMapRecord3D* records) {
    uint64_t node_index = (*used)++;
    SupportNode* node = &nodes[node_index];
    node->begin = begin; node->count = count;
    node->left = node->right = UINT64_MAX;
    const RuntimeCausticPhotonMapRecord3D* first = &records[points[begin].index];
    node->min = node->max = first->position; node->normal = first->normal;
    node->object = first->sceneObjectIndex; node->material = first->materialId;
    node->uniformObject = node->uniformMaterial = node->uniformNormal = true;
    for (uint64_t i = begin + 1u; i < begin + count; ++i) {
        const RuntimeCausticPhotonMapRecord3D* r = &records[points[i].index];
        node->min = vec3(fmin(node->min.x,r->position.x),fmin(node->min.y,r->position.y),fmin(node->min.z,r->position.z));
        node->max = vec3(fmax(node->max.x,r->position.x),fmax(node->max.y,r->position.y),fmax(node->max.z,r->position.z));
        node->uniformObject &= r->sceneObjectIndex == node->object;
        node->uniformMaterial &= r->materialId == node->material;
        node->uniformNormal &= memcmp(&r->normal,&node->normal,sizeof(Vec3)) == 0;
    }
    if (count > 16u) {
        Vec3 extent=vec3_sub(node->max,node->min);
        unsigned axis=extent.y>extent.x ? 1u : 0u;
        if(extent.z>component(extent,axis)) axis=2u;
        int (*compare)(const void*, const void*) =
            axis==0u ? compare_x : axis==1u ? compare_y : compare_z;
        qsort(points + begin, (size_t)count, sizeof(*points), compare);
        uint64_t mid = count / 2u;
        node->left = build(points, begin, mid, depth + 1u, nodes, used, records);
        node->right = build(points, begin + mid + 1u, count - mid - 1u,
                            depth + 1u, nodes, used, records);
    }
    return node_index;
}

static void retain_distance(SupportQuery* q, double distance) {
    if (q->heapCount < q->limit) {
        uint64_t child = q->heapCount++;
        while (child > 0u) {
            uint64_t parent = (child - 1u) / 2u;
            if (!(q->heap[parent] < distance)) break;
            q->heap[child] = q->heap[parent]; child = parent;
        }
        q->heap[child] = distance;
    } else if (distance < q->heap[0]) {
        uint64_t parent = 0u;
        while (parent * 2u + 1u < q->heapCount) {
            uint64_t child = parent * 2u + 1u;
            if (child + 1u < q->heapCount && q->heap[child] < q->heap[child + 1u]) child++;
            if (!(distance < q->heap[child])) break;
            q->heap[parent] = q->heap[child]; parent = child;
        }
        q->heap[parent] = distance;
    }
}

static bool accepted(SupportQuery* q, uint64_t index, double* distance) {
    const RuntimeCausticPhotonMapRecord3D* a = &q->records[q->self];
    const RuntimeCausticPhotonMapRecord3D* b = &q->records[index];
    q->tested++;
    if (a->sceneObjectIndex != b->sceneObjectIndex ||
        (a->materialId >= 0 && b->materialId >= 0 && a->materialId != b->materialId) ||
        !(vec3_dot(a->normal, b->normal) >= q->minNormalDot)) return false;
    /* Keep exactly the reference arithmetic and strict radius predicate. */
    *distance = vec3_length(vec3_sub(a->position, b->position));
    return *distance >= 0.0 && *distance < a->queryRadius;
}

/* Error allowance deliberately scales with translated coordinates. Ambiguous
 * boundary cases descend to the unchanged scalar predicate, never bulk count. */
static double bounds_margin(const SupportNode* node, Vec3 p, double radius) {
    double scale = fmax(radius,fmax(fabs(p.x),fmax(fabs(p.y),fabs(p.z))));
    scale = fmax(scale,fmax(fabs(node->min.x),fmax(fabs(node->min.y),fabs(node->min.z))));
    scale = fmax(scale,fmax(fabs(node->max.x),fmax(fabs(node->max.y),fabs(node->max.z))));
    return 32.0 * DBL_EPSILON * scale;
}
static double near_distance(const SupportNode* node, Vec3 p) {
    return vec3_length(vec3(fmax(0,fmax(node->min.x-p.x,p.x-node->max.x)),
                            fmax(0,fmax(node->min.y-p.y,p.y-node->max.y)),
                            fmax(0,fmax(node->min.z-p.z,p.z-node->max.z))));
}
static bool outside(const SupportNode* node, const SupportQuery* q, double radius) {
    const RuntimeCausticPhotonMapRecord3D* a=&q->records[q->self];
    if (node->uniformObject && node->object != a->sceneObjectIndex) return true;
    if (node->uniformMaterial && node->material >= 0 && a->materialId >= 0 &&
        node->material != a->materialId) return true;
    if (node->uniformNormal && !(vec3_dot(a->normal,node->normal)>=q->minNormalDot)) return true;
    return near_distance(node,a->position) > nextafter(radius + bounds_margin(node,a->position,radius),INFINITY);
}
static bool wholly_inside(const SupportNode* node, const SupportQuery* q) {
    const RuntimeCausticPhotonMapRecord3D* a=&q->records[q->self];
    if (!node->uniformObject || !node->uniformMaterial || !node->uniformNormal ||
        node->object != a->sceneObjectIndex ||
        (a->materialId >= 0 && node->material >= 0 && a->materialId != node->material) ||
        !(vec3_dot(a->normal,node->normal)>=q->minNormalDot)) return false;
    Vec3 p=a->position;
    double far=vec3_length(vec3(fmax(fabs(p.x-node->min.x),fabs(p.x-node->max.x)),
                               fmax(fabs(p.y-node->min.y),fabs(p.y-node->max.y)),
                               fmax(fabs(p.z-node->min.z),fabs(p.z-node->max.z))));
    return far < nextafter(a->queryRadius - bounds_margin(node,p,a->queryRadius),-INFINITY);
}
static void count_neighbors(const SupportPoint* points, const SupportNode* nodes,
                            uint64_t index, SupportQuery* q) {
    const SupportNode* node=&nodes[index];
    if (outside(node,q,q->records[q->self].queryRadius)) return;
    if (wholly_inside(node,q)) { q->count += node->count; return; }
    if (node->left == UINT64_MAX) {
        for(uint64_t i=node->begin;i<node->begin+node->count;i++) {
            double distance;
            if(accepted(q,points[i].index,&distance)) q->count++;
        }
    } else {
        double distance;
        if(accepted(q,points[node->begin+node->count/2u].index,&distance)) q->count++;
        count_neighbors(points,nodes,node->left,q);
        count_neighbors(points,nodes,node->right,q);
    }
}
static void nearest_neighbors(const SupportPoint* points, const SupportNode* nodes,
                              uint64_t index, SupportQuery* q) {
    const SupportNode* node=&nodes[index];
    /* No accepted distance can improve a full heap of coincident photons. */
    if(q->heapCount==q->limit && q->heap[0]==0.0) return;
    double radius=q->heapCount==q->limit ? q->heap[0] : q->records[q->self].queryRadius;
    if(outside(node,q,radius)) return;
    if(node->left==UINT64_MAX) {
        for(uint64_t i=node->begin;i<node->begin+node->count;i++) {
            double distance;uint64_t record=points[i].index;
            if(record!=q->self && accepted(q,record,&distance)) retain_distance(q,distance);
        }
    } else {
        double distance;uint64_t record=points[node->begin+node->count/2u].index;
        if(record!=q->self && accepted(q,record,&distance)) retain_distance(q,distance);
        uint64_t first=node->left,second=node->right;
        Vec3 p=q->records[q->self].position;
        if(near_distance(&nodes[second],p)<near_distance(&nodes[first],p)) {
            first=node->right;second=node->left;
        }
        nearest_neighbors(points,nodes,first,q);
        nearest_neighbors(points,nodes,second,q);
    }
}

bool RuntimeCausticPhotonSampleSupport3D_PrepareSpatial(
    RuntimeCausticPhotonMapRecord3D* records, uint64_t count,
    uint64_t limit, double min_normal_dot,
    RuntimeCausticPhotonSampleSupportReadback3D* out) {
    if (!records || !out || count == 0u || limit == 0u ||
        count > SIZE_MAX / sizeof(SupportPoint)) return false;
    /* Unsupported numeric domains use the unchanged reference, never a partial
     * spatial result. Ordinary stored map radii are bounded to [0.001, 10]. */
    for (uint64_t i = 0; i < count; ++i) {
        Vec3 p = records[i].position;
        if (!isfinite(p.x) || !isfinite(p.y) || !isfinite(p.z) ||
            fabs(p.x) > 1e150 || fabs(p.y) > 1e150 || fabs(p.z) > 1e150 ||
            !isfinite(records[i].queryRadius) || records[i].queryRadius < 1e-100 ||
            records[i].queryRadius > 1e150) return false;
    }
    uint64_t heap_capacity = limit < count ? limit : count;
    SupportPoint* points = malloc((size_t)count * sizeof(*points));
    /* Leaves contain at least eight points except for the single-root case;
     * count/4+1 safely bounds all nodes of this median-split tree. */
    SupportNode* nodes = calloc((size_t)(count/4u+1u),sizeof(*nodes));
    double* heap = malloc((size_t)heap_capacity * sizeof(*heap));
    if (!points || !nodes || !heap) { free(points); free(nodes); free(heap); return false; }
    for (uint64_t i = 0; i < count; ++i) {
        points[i].position = records[i].position; points[i].index = i;
    }
    uint64_t used=0;
    build(points, 0u, count, 0u, nodes, &used, records);
    RuntimeCausticPhotonSampleSupportReadback3D result;
    memset(&result, 0, sizeof(result));
    result.recordCount = count; result.neighborLimit = limit;
    result.minimumSupportRadius = DBL_MAX; result.spatialIndexUsed = true;
    for (uint64_t i = 0; i < count; ++i) {
        SupportQuery q = { .records = records, .self = i, .limit = heap_capacity,
                           .minNormalDot = min_normal_dot, .heap = heap };
        count_neighbors(points,nodes,0u,&q);
        /* Count includes self iff its normal predicate qualifies. Subtract it
         * once without requiring a second sorted-index ownership structure. */
        if(vec3_dot(records[i].normal,records[i].normal)>=min_normal_dot) q.count--;
        if(q.count>=limit) nearest_neighbors(points,nodes,0u,&q);
        RuntimeCausticPhotonMapRecord3D* record = &records[i];
        bool adaptive = false;
        double radius = record->queryRadius;
        if (q.count >= limit)
            radius = RuntimeCausticPhotonSurfaceKernel3D_BoundedAdaptiveRadius(
                heap[0], record->queryRadius, &adaptive);
        record->sampleCenteredSupportRadius = radius;
        record->sampleCenteredSupportNeighborCount = q.count;
        record->sampleCenteredSupportAdaptive = adaptive;
        record->sampleCenteredSupportPrepared = true;
        if (adaptive) result.adaptiveRecordCount++;
        else result.maximumRadiusRecordCount++;
        if (radius < result.minimumSupportRadius) result.minimumSupportRadius = radius;
        if (radius > result.maximumSupportRadius) result.maximumSupportRadius = radius;
        result.meanSupportRadius += radius;
        result.searchCandidateCount += q.tested;
    }
    result.meanSupportRadius /= (double)count;
    result.valid = true;
    *out = result;
    free(points); free(nodes); free(heap);
    return true;
}
