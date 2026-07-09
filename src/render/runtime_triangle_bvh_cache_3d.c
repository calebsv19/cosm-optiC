#include "render/runtime_triangle_bvh_internal_3d.h"

static bool runtime_triangle_bvh_write_exact(FILE* file, const void* data, size_t size) {
    return file && data && fwrite(data, 1u, size, file) == size;
}

static bool runtime_triangle_bvh_read_exact(FILE* file, void* data, size_t size) {
    return file && data && fread(data, 1u, size, file) == size;
}

static void runtime_triangle_bvh_cache_diag(char* out_diagnostics,
                                            size_t out_diagnostics_size,
                                            const char* message) {
    if (!out_diagnostics || out_diagnostics_size == 0u || !message) return;
    snprintf(out_diagnostics, out_diagnostics_size, "%s", message);
}

bool RuntimeTriangleMesh3D_WriteBVHCachePayload(FILE* file,
                                                const RuntimeTriangleMesh3D* mesh) {
    const RuntimeTriangleBVH3D* bvh = NULL;
    if (!file || !RuntimeTriangleMesh3D_HasReadyBVH(mesh)) return false;
    bvh = mesh->bvh;
    if (bvh->nodeCount <= 0 || bvh->indexCount != mesh->triangleCount ||
        !bvh->nodes || !bvh->indices) {
        return false;
    }
    if (!runtime_triangle_bvh_write_exact(file, &bvh->nodeCount, sizeof(bvh->nodeCount)) ||
        !runtime_triangle_bvh_write_exact(file, &bvh->indexCount, sizeof(bvh->indexCount)) ||
        !runtime_triangle_bvh_write_exact(file, &bvh->leafCount, sizeof(bvh->leafCount)) ||
        !runtime_triangle_bvh_write_exact(file, &bvh->maxDepth, sizeof(bvh->maxDepth)) ||
        !runtime_triangle_bvh_write_exact(file,
                                          &bvh->maxLeafTriangleCount,
                                          sizeof(bvh->maxLeafTriangleCount))) {
        return false;
    }
    if (!runtime_triangle_bvh_write_exact(file,
                                          bvh->nodes,
                                          sizeof(*bvh->nodes) * (size_t)bvh->nodeCount)) {
        return false;
    }
    if (!runtime_triangle_bvh_write_exact(file,
                                          bvh->indices,
                                          sizeof(*bvh->indices) * (size_t)bvh->indexCount)) {
        return false;
    }
    return true;
}

static bool runtime_triangle_bvh_validate_import_node(const RuntimeTriangleBVH3DNode* node,
                                                      int node_count,
                                                      int index_count) {
    if (!node || node_count <= 0 || index_count <= 0) return false;
    if (!runtime_triangle_bvh_vec3_isfinite(node->min) ||
        !runtime_triangle_bvh_vec3_isfinite(node->max)) {
        return false;
    }
    if (node->left >= node_count || node->right >= node_count) return false;
    if (node->left < -1 || node->right < -1) return false;
    if (node->count > 0) {
        if (node->left != -1 || node->right != -1) return false;
        if (node->start < 0 || node->count < 0) return false;
        if (node->start > index_count || node->count > index_count - node->start) {
            return false;
        }
    } else {
        if (node->count < 0 || node->start < 0) return false;
        if ((node->left < 0) != (node->right < 0)) return false;
    }
    return true;
}

bool RuntimeTriangleMesh3D_ReadBVHCachePayload(FILE* file,
                                               RuntimeTriangleMesh3D* mesh,
                                               int expected_triangle_count,
                                               char* out_diagnostics,
                                               size_t out_diagnostics_size) {
    RuntimeTriangleBVH3D* bvh = NULL;
    int node_count = 0;
    int index_count = 0;
    int leaf_count = 0;
    int max_depth = 0;
    int max_leaf_triangle_count = 0;

    if (!file || !mesh || expected_triangle_count <= 0) {
        runtime_triangle_bvh_cache_diag(out_diagnostics,
                                        out_diagnostics_size,
                                        "bvh cache import input missing");
        return false;
    }
    if (!runtime_triangle_bvh_read_exact(file, &node_count, sizeof(node_count)) ||
        !runtime_triangle_bvh_read_exact(file, &index_count, sizeof(index_count)) ||
        !runtime_triangle_bvh_read_exact(file, &leaf_count, sizeof(leaf_count)) ||
        !runtime_triangle_bvh_read_exact(file, &max_depth, sizeof(max_depth)) ||
        !runtime_triangle_bvh_read_exact(file,
                                         &max_leaf_triangle_count,
                                         sizeof(max_leaf_triangle_count))) {
        runtime_triangle_bvh_cache_diag(out_diagnostics,
                                        out_diagnostics_size,
                                        "bvh cache header read failed");
        return false;
    }
    if (node_count <= 0 || index_count != expected_triangle_count || leaf_count <= 0 ||
        max_depth <= 0 || max_leaf_triangle_count <= 0) {
        runtime_triangle_bvh_cache_diag(out_diagnostics,
                                        out_diagnostics_size,
                                        "bvh cache header invalid");
        return false;
    }

    bvh = (RuntimeTriangleBVH3D*)calloc(1u, sizeof(*bvh));
    if (!bvh) {
        runtime_triangle_bvh_cache_diag(out_diagnostics,
                                        out_diagnostics_size,
                                        "bvh cache allocation failed");
        return false;
    }
    bvh->nodeCount = node_count;
    bvh->nodeCapacity = node_count;
    bvh->leafCount = leaf_count;
    bvh->maxDepth = max_depth;
    bvh->maxLeafTriangleCount = max_leaf_triangle_count;
    bvh->indexCount = index_count;
    bvh->nodes = (RuntimeTriangleBVH3DNode*)malloc(sizeof(*bvh->nodes) *
                                                   (size_t)bvh->nodeCount);
    bvh->indices = (int*)malloc(sizeof(*bvh->indices) * (size_t)bvh->indexCount);
    if (!bvh->nodes || !bvh->indices) {
        runtime_triangle_bvh_destroy(bvh);
        runtime_triangle_bvh_cache_diag(out_diagnostics,
                                        out_diagnostics_size,
                                        "bvh cache payload allocation failed");
        return false;
    }
    if (!runtime_triangle_bvh_read_exact(file,
                                         bvh->nodes,
                                         sizeof(*bvh->nodes) * (size_t)bvh->nodeCount) ||
        !runtime_triangle_bvh_read_exact(file,
                                         bvh->indices,
                                         sizeof(*bvh->indices) * (size_t)bvh->indexCount)) {
        runtime_triangle_bvh_destroy(bvh);
        runtime_triangle_bvh_cache_diag(out_diagnostics,
                                        out_diagnostics_size,
                                        "bvh cache payload read failed");
        return false;
    }
    for (int i = 0; i < bvh->nodeCount; ++i) {
        if (!runtime_triangle_bvh_validate_import_node(&bvh->nodes[i],
                                                       bvh->nodeCount,
                                                       bvh->indexCount)) {
            runtime_triangle_bvh_destroy(bvh);
            runtime_triangle_bvh_cache_diag(out_diagnostics,
                                            out_diagnostics_size,
                                            "bvh cache node invalid");
            return false;
        }
    }
    for (int i = 0; i < bvh->indexCount; ++i) {
        if (bvh->indices[i] < 0 || bvh->indices[i] >= expected_triangle_count) {
            runtime_triangle_bvh_destroy(bvh);
            runtime_triangle_bvh_cache_diag(out_diagnostics,
                                            out_diagnostics_size,
                                            "bvh cache index invalid");
            return false;
        }
    }

    bvh->nodeBytes = (uint64_t)sizeof(*bvh->nodes) * (uint64_t)bvh->nodeCapacity;
    bvh->indexBytes = (uint64_t)sizeof(*bvh->indices) * (uint64_t)bvh->indexCount;
    bvh->totalBytes = bvh->nodeBytes + bvh->indexBytes + (uint64_t)sizeof(*bvh);
    RuntimeTriangleMesh3D_ClearBVH(mesh);
    mesh->bvh = bvh;
    mesh->bvhDirty = false;
    return true;
}
