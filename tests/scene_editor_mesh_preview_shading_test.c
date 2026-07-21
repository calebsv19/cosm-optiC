#include <math.h>
#include <stdio.h>

#include "editor/scene_editor_mesh_preview_shading.h"

static int g_failures = 0;

static void expect_true(const char* name, int condition) {
    if (condition) return;
    fprintf(stderr, "FAIL: %s\n", name);
    g_failures += 1;
}

static void test_angle_weighted_normals_smooth_shared_vertices(void) {
    CoreObjectVec3 vertices[] = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {1.0, 1.0, 0.0},
        {0.0, 1.0, 0.0}
    };
    uint32_t indices[] = {0u, 1u, 2u, 0u, 2u, 3u};
    CoreObjectVec3 normals[4];
    CoreMeshPreviewLodMesh lod = {
        .vertices = vertices,
        .vertex_count = 4u,
        .indices = indices,
        .triangle_count = 2u
    };
    expect_true("smooth_normal_build",
                SceneEditorMeshPreviewBuildSmoothNormals(&lod, normals, 4u));
    for (size_t i = 0u; i < 4u; ++i) {
        expect_true("smooth_normal_x", fabs(normals[i].x) < 1e-12);
        expect_true("smooth_normal_y", fabs(normals[i].y) < 1e-12);
        expect_true("smooth_normal_z", fabs(normals[i].z - 1.0) < 1e-12);
    }
}

static void test_smooth_normal_rejects_invalid_index(void) {
    CoreObjectVec3 vertices[] = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0}
    };
    uint32_t indices[] = {0u, 1u, 4u};
    CoreObjectVec3 normals[3];
    CoreMeshPreviewLodMesh lod = {
        .vertices = vertices,
        .vertex_count = 3u,
        .indices = indices,
        .triangle_count = 1u
    };
    expect_true("invalid_index_rejected",
                !SceneEditorMeshPreviewBuildSmoothNormals(&lod, normals, 3u));
}

int main(void) {
    test_angle_weighted_normals_smooth_shared_vertices();
    test_smooth_normal_rejects_invalid_index();
    if (g_failures != 0) return 1;
    puts("scene editor mesh preview shading tests passed");
    return 0;
}
