#include "test_runtime_caustic_transport_3d.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app/animation.h"
#include "config/config_file_io.h"
#include "material/material.h"
#include "material/material_manager.h"
#include "render/ray_tracing_integrator_catalog.h"
#include "render/runtime_caustic_settings_3d.h"
#include "render/runtime_disney_v2_caustic_sidecar_3d.h"
#include "render/runtime_caustic_transport_3d.h"
#include "render/runtime_caustic_transport_debug_3d.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_volume_3d.h"
#include "test_runtime_native_3d_render_prepared_suite_internal.h"
#include "test_support.h"

#define TEST_CAUSTIC_TRANSPORT_RENDER_WIDTH 31
#define TEST_CAUSTIC_TRANSPORT_RENDER_HEIGHT 31
#define TEST_CAUSTIC_TRANSPORT_RADIANCE_COUNT \
    ((size_t)TEST_CAUSTIC_TRANSPORT_RENDER_WIDTH * \
     (size_t)TEST_CAUSTIC_TRANSPORT_RENDER_HEIGHT * \
     (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS)

static RuntimeNative3DPreparedFrame* test_caustic_transport_alloc_prepared_frame(void) {
    RuntimeNative3DPreparedFrame* frame =
        (RuntimeNative3DPreparedFrame*)calloc(1u, sizeof(*frame));
    if (!frame) return NULL;
    RuntimeNative3DTileOccupancy_Init(&frame->tileOccupancy);
    RuntimeCausticVolumeCache3D_Init(&frame->causticVolumeCache);
    RuntimeCausticSurfaceCache3D_Init(&frame->causticSurfaceCache);
    return frame;
}

static void test_caustic_transport_free_prepared_frame(RuntimeNative3DPreparedFrame* frame) {
    if (!frame) return;
    RuntimeNative3DPreparedFrame_Free(frame);
    free(frame);
}

static bool test_caustic_transport_make_scene(RuntimeScene3D* scene) {
    if (!scene) return false;
    RuntimeScene3D_Init(scene);
    scene->hasLight = true;
    scene->light.position = vec3(0.0, -3.0, 0.0);
    scene->light.radius = 0.05;
    scene->light.intensity = 60.0;
    scene->light.falloffDistance = 6.0;
    scene->light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene->hasCamera = true;
    scene->camera.position = vec3(0.0, 1.8, 0.0);
    scene->camera.rotation = 0.0;
    scene->camera.lookPitch = 0.0;
    scene->camera.zoom = 1.0;
    scene->camera.nearPlane = 0.1;
    scene->primitiveCapacity = 2;
    scene->triangleMesh.triangleCapacity = 4;
    scene->primitives = (RuntimePrimitive3D*)calloc((size_t)scene->primitiveCapacity,
                                                    sizeof(*scene->primitives));
    scene->triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene->triangleMesh.triangleCapacity,
                                   sizeof(*scene->triangleMesh.triangles));
    if (!scene->primitives || !scene->triangleMesh.triangles) {
        RuntimeScene3D_Free(scene);
        return false;
    }

    scene->primitiveCount = 2;
    scene->triangleMesh.triangleCount = 4;
    scene->primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    scene->primitives[0].source.sceneObjectIndex = 0;
    snprintf(scene->primitives[0].source.objectId,
             sizeof(scene->primitives[0].source.objectId),
             "%s",
             "transport_glass_pane");
    scene->primitives[1].source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    scene->primitives[1].source.sceneObjectIndex = 1;
    snprintf(scene->primitives[1].source.objectId,
             sizeof(scene->primitives[1].source.objectId),
             "%s",
             "transport_receiver");

    scene->triangleMesh.triangles[0].p0 = vec3(-0.30, -1.0, -0.30);
    scene->triangleMesh.triangles[0].p1 = vec3(-0.30, -1.0, 0.30);
    scene->triangleMesh.triangles[0].p2 = vec3(0.30, -1.0, -0.30);
    scene->triangleMesh.triangles[0].normal = vec3(0.0, -1.0, 0.0);
    scene->triangleMesh.triangles[0].primitiveIndex = 0;
    scene->triangleMesh.triangles[0].sceneObjectIndex = 0;
    scene->triangleMesh.triangles[1].p0 = vec3(-0.30, -1.0, 0.30);
    scene->triangleMesh.triangles[1].p1 = vec3(0.30, -1.0, 0.30);
    scene->triangleMesh.triangles[1].p2 = vec3(0.30, -1.0, -0.30);
    scene->triangleMesh.triangles[1].normal = vec3(0.0, -1.0, 0.0);
    scene->triangleMesh.triangles[1].primitiveIndex = 0;
    scene->triangleMesh.triangles[1].sceneObjectIndex = 0;
    scene->triangleMesh.triangles[2].p0 = vec3(-0.80, 0.35, -0.80);
    scene->triangleMesh.triangles[2].p1 = vec3(0.80, 0.35, -0.80);
    scene->triangleMesh.triangles[2].p2 = vec3(-0.80, 0.35, 0.80);
    scene->triangleMesh.triangles[2].normal = vec3(0.0, -1.0, 0.0);
    scene->triangleMesh.triangles[2].primitiveIndex = 1;
    scene->triangleMesh.triangles[2].sceneObjectIndex = 1;
    scene->triangleMesh.triangles[3].p0 = vec3(0.80, 0.35, -0.80);
    scene->triangleMesh.triangles[3].p1 = vec3(0.80, 0.35, 0.80);
    scene->triangleMesh.triangles[3].p2 = vec3(-0.80, 0.35, 0.80);
    scene->triangleMesh.triangles[3].normal = vec3(0.0, -1.0, 0.0);
    scene->triangleMesh.triangles[3].primitiveIndex = 1;
    scene->triangleMesh.triangles[3].sceneObjectIndex = 1;

    if (!prepared_suite_attach_dense_volume(&scene->volume,
                                            vec3(-0.60, -0.85, -0.60),
                                            6u,
                                            9u,
                                            6u,
                                            0.15,
                                            1.0f)) {
        RuntimeScene3D_Free(scene);
        return false;
    }
    RuntimeScene3D_RefreshCapabilities(scene);
    return true;
}

static bool test_caustic_transport_make_analytic_sphere_scene(RuntimeScene3D* scene) {
    static const Vec3 unit_vertices[6] = {
        {0.0, 1.0, 0.0},
        {0.0, -1.0, 0.0},
        {-1.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 0.0, -1.0},
        {0.0, 0.0, 1.0}
    };
    static const int faces[8][3] = {
        {0, 3, 5},
        {0, 5, 2},
        {0, 2, 4},
        {0, 4, 3},
        {1, 5, 3},
        {1, 2, 5},
        {1, 4, 2},
        {1, 3, 4}
    };
    const Vec3 center = {0.0, -1.0, 0.0};
    const double radius = 0.30;

    if (!scene) return false;
    RuntimeScene3D_Init(scene);
    scene->hasLight = true;
    scene->light.position = vec3(0.0, -3.0, 0.0);
    scene->light.radius = 0.08;
    scene->light.intensity = 85.0;
    scene->light.falloffDistance = 6.0;
    scene->light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene->hasCamera = true;
    scene->camera.position = vec3(0.0, 1.8, 0.0);
    scene->camera.rotation = 0.0;
    scene->camera.lookPitch = 0.0;
    scene->camera.zoom = 1.0;
    scene->camera.nearPlane = 0.1;
    scene->primitiveCapacity = 2;
    scene->triangleMesh.triangleCapacity = 10;
    scene->primitives = (RuntimePrimitive3D*)calloc((size_t)scene->primitiveCapacity,
                                                    sizeof(*scene->primitives));
    scene->triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene->triangleMesh.triangleCapacity,
                                   sizeof(*scene->triangleMesh.triangles));
    if (!scene->primitives || !scene->triangleMesh.triangles) {
        RuntimeScene3D_Free(scene);
        return false;
    }

    scene->primitiveCount = 2;
    scene->triangleMesh.triangleCount = 10;
    scene->primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    scene->primitives[0].source.sceneObjectIndex = 0;
    snprintf(scene->primitives[0].source.objectId,
             sizeof(scene->primitives[0].source.objectId),
             "%s",
             "analytic_glass_sphere");
    scene->primitives[1].source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    scene->primitives[1].source.sceneObjectIndex = 1;
    snprintf(scene->primitives[1].source.objectId,
             sizeof(scene->primitives[1].source.objectId),
             "%s",
             "analytic_receiver");

    for (int face_i = 0; face_i < 8; ++face_i) {
        RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[face_i];
        triangle->p0 = vec3_add(center, vec3_scale(unit_vertices[faces[face_i][0]], radius));
        triangle->p1 = vec3_add(center, vec3_scale(unit_vertices[faces[face_i][1]], radius));
        triangle->p2 = vec3_add(center, vec3_scale(unit_vertices[faces[face_i][2]], radius));
        triangle->normal =
            vec3_normalize(vec3_cross(vec3_sub(triangle->p1, triangle->p0),
                                      vec3_sub(triangle->p2, triangle->p0)));
        triangle->primitiveIndex = 0;
        triangle->sceneObjectIndex = 0;
        triangle->localTriangleIndex = face_i;
    }
    scene->triangleMesh.triangles[8].p0 = vec3(-0.80, 0.35, -0.80);
    scene->triangleMesh.triangles[8].p1 = vec3(0.80, 0.35, -0.80);
    scene->triangleMesh.triangles[8].p2 = vec3(-0.80, 0.35, 0.80);
    scene->triangleMesh.triangles[8].normal = vec3(0.0, -1.0, 0.0);
    scene->triangleMesh.triangles[8].primitiveIndex = 1;
    scene->triangleMesh.triangles[8].sceneObjectIndex = 1;
    scene->triangleMesh.triangles[8].localTriangleIndex = 0;
    scene->triangleMesh.triangles[9].p0 = vec3(0.80, 0.35, -0.80);
    scene->triangleMesh.triangles[9].p1 = vec3(0.80, 0.35, 0.80);
    scene->triangleMesh.triangles[9].p2 = vec3(-0.80, 0.35, 0.80);
    scene->triangleMesh.triangles[9].normal = vec3(0.0, -1.0, 0.0);
    scene->triangleMesh.triangles[9].primitiveIndex = 1;
    scene->triangleMesh.triangles[9].sceneObjectIndex = 1;
    scene->triangleMesh.triangles[9].localTriangleIndex = 1;

    if (!prepared_suite_attach_dense_volume(&scene->volume,
                                            vec3(-0.70, -0.85, -0.70),
                                            8u,
                                            10u,
                                            8u,
                                            0.15,
                                            1.0f)) {
        RuntimeScene3D_Free(scene);
        return false;
    }
    RuntimeScene3D_RefreshCapabilities(scene);
    return true;
}

static bool test_caustic_transport_make_analytic_cylinder_scene(RuntimeScene3D* scene) {
    static const Vec3 unit_vertices[6] = {
        {0.0, 1.0, 0.0},
        {0.0, -1.0, 0.0},
        {-1.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 0.0, -1.0},
        {0.0, 0.0, 1.0}
    };
    static const int faces[8][3] = {
        {0, 3, 5},
        {0, 5, 2},
        {0, 2, 4},
        {0, 4, 3},
        {1, 5, 3},
        {1, 2, 5},
        {1, 4, 2},
        {1, 3, 4}
    };
    const Vec3 center = {0.0, -1.0, 0.0};
    const Vec3 scale = {0.30, 0.30, 0.85};

    if (!scene) return false;
    RuntimeScene3D_Init(scene);
    scene->hasLight = true;
    scene->light.position = vec3(0.0, -3.0, 0.0);
    scene->light.radius = 0.08;
    scene->light.intensity = 90.0;
    scene->light.falloffDistance = 6.0;
    scene->light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene->hasCamera = true;
    scene->camera.position = vec3(0.0, 1.8, 0.0);
    scene->camera.rotation = 0.0;
    scene->camera.lookPitch = 0.0;
    scene->camera.zoom = 1.0;
    scene->camera.nearPlane = 0.1;
    scene->primitiveCapacity = 2;
    scene->triangleMesh.triangleCapacity = 10;
    scene->primitives = (RuntimePrimitive3D*)calloc((size_t)scene->primitiveCapacity,
                                                    sizeof(*scene->primitives));
    scene->triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene->triangleMesh.triangleCapacity,
                                   sizeof(*scene->triangleMesh.triangles));
    if (!scene->primitives || !scene->triangleMesh.triangles) {
        RuntimeScene3D_Free(scene);
        return false;
    }

    scene->primitiveCount = 2;
    scene->triangleMesh.triangleCount = 10;
    scene->primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    scene->primitives[0].source.sceneObjectIndex = 0;
    snprintf(scene->primitives[0].source.objectId,
             sizeof(scene->primitives[0].source.objectId),
             "%s",
             "analytic_glass_cylinder");
    scene->primitives[1].source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    scene->primitives[1].source.sceneObjectIndex = 1;
    snprintf(scene->primitives[1].source.objectId,
             sizeof(scene->primitives[1].source.objectId),
             "%s",
             "analytic_receiver");

    for (int face_i = 0; face_i < 8; ++face_i) {
        RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[face_i];
        Vec3 a = unit_vertices[faces[face_i][0]];
        Vec3 b = unit_vertices[faces[face_i][1]];
        Vec3 c = unit_vertices[faces[face_i][2]];
        triangle->p0 = vec3_add(center, vec3(a.x * scale.x, a.y * scale.y, a.z * scale.z));
        triangle->p1 = vec3_add(center, vec3(b.x * scale.x, b.y * scale.y, b.z * scale.z));
        triangle->p2 = vec3_add(center, vec3(c.x * scale.x, c.y * scale.y, c.z * scale.z));
        triangle->normal =
            vec3_normalize(vec3_cross(vec3_sub(triangle->p1, triangle->p0),
                                      vec3_sub(triangle->p2, triangle->p0)));
        triangle->primitiveIndex = 0;
        triangle->sceneObjectIndex = 0;
        triangle->localTriangleIndex = face_i;
    }
    scene->triangleMesh.triangles[8].p0 = vec3(-0.80, 0.35, -0.80);
    scene->triangleMesh.triangles[8].p1 = vec3(0.80, 0.35, -0.80);
    scene->triangleMesh.triangles[8].p2 = vec3(-0.80, 0.35, 0.80);
    scene->triangleMesh.triangles[8].normal = vec3(0.0, -1.0, 0.0);
    scene->triangleMesh.triangles[8].primitiveIndex = 1;
    scene->triangleMesh.triangles[8].sceneObjectIndex = 1;
    scene->triangleMesh.triangles[8].localTriangleIndex = 0;
    scene->triangleMesh.triangles[9].p0 = vec3(0.80, 0.35, -0.80);
    scene->triangleMesh.triangles[9].p1 = vec3(0.80, 0.35, 0.80);
    scene->triangleMesh.triangles[9].p2 = vec3(-0.80, 0.35, 0.80);
    scene->triangleMesh.triangles[9].normal = vec3(0.0, -1.0, 0.0);
    scene->triangleMesh.triangles[9].primitiveIndex = 1;
    scene->triangleMesh.triangles[9].sceneObjectIndex = 1;
    scene->triangleMesh.triangles[9].localTriangleIndex = 1;

    if (!prepared_suite_attach_dense_volume(&scene->volume,
                                            vec3(-0.70, -0.85, -0.90),
                                            8u,
                                            10u,
                                            10u,
                                            0.15,
                                            1.0f)) {
        RuntimeScene3D_Free(scene);
        return false;
    }
    RuntimeScene3D_RefreshCapabilities(scene);
    return true;
}

static bool test_caustic_transport_make_analytic_bowl_scene(RuntimeScene3D* scene) {
    static const Vec3 unit_vertices[6] = {
        {0.0, 1.0, 0.0},
        {0.0, -1.0, 0.0},
        {-1.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 0.0, -1.0},
        {0.0, 0.0, 1.0}
    };
    static const int faces[8][3] = {
        {0, 3, 5},
        {0, 5, 2},
        {0, 2, 4},
        {0, 4, 3},
        {1, 5, 3},
        {1, 2, 5},
        {1, 4, 2},
        {1, 3, 4}
    };
    const Vec3 center = {0.0, -1.0, 0.0};
    const Vec3 scale = {0.66, 0.16, 0.62};

    if (!scene) return false;
    RuntimeScene3D_Init(scene);
    scene->hasLight = true;
    scene->light.position = vec3(0.0, -3.0, -1.60);
    scene->light.radius = 0.07;
    scene->light.intensity = 88.0;
    scene->light.falloffDistance = 6.0;
    scene->light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene->hasCamera = true;
    scene->camera.position = vec3(0.0, 1.8, 0.0);
    scene->camera.rotation = 0.0;
    scene->camera.lookPitch = 0.0;
    scene->camera.zoom = 1.0;
    scene->camera.nearPlane = 0.1;
    scene->primitiveCapacity = 2;
    scene->triangleMesh.triangleCapacity = 10;
    scene->primitives = (RuntimePrimitive3D*)calloc((size_t)scene->primitiveCapacity,
                                                    sizeof(*scene->primitives));
    scene->triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene->triangleMesh.triangleCapacity,
                                   sizeof(*scene->triangleMesh.triangles));
    if (!scene->primitives || !scene->triangleMesh.triangles) {
        RuntimeScene3D_Free(scene);
        return false;
    }

    scene->primitiveCount = 2;
    scene->triangleMesh.triangleCount = 10;
    scene->primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    scene->primitives[0].source.sceneObjectIndex = 0;
    snprintf(scene->primitives[0].source.objectId,
             sizeof(scene->primitives[0].source.objectId),
             "%s",
             "analytic_glass_bowl_lens");
    scene->primitives[1].source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    scene->primitives[1].source.sceneObjectIndex = 1;
    snprintf(scene->primitives[1].source.objectId,
             sizeof(scene->primitives[1].source.objectId),
             "%s",
             "analytic_receiver");

    for (int face_i = 0; face_i < 8; ++face_i) {
        RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[face_i];
        Vec3 a = unit_vertices[faces[face_i][0]];
        Vec3 b = unit_vertices[faces[face_i][1]];
        Vec3 c = unit_vertices[faces[face_i][2]];
        triangle->p0 = vec3_add(center, vec3(a.x * scale.x, a.y * scale.y, a.z * scale.z));
        triangle->p1 = vec3_add(center, vec3(b.x * scale.x, b.y * scale.y, b.z * scale.z));
        triangle->p2 = vec3_add(center, vec3(c.x * scale.x, c.y * scale.y, c.z * scale.z));
        triangle->normal =
            vec3_normalize(vec3_cross(vec3_sub(triangle->p1, triangle->p0),
                                      vec3_sub(triangle->p2, triangle->p0)));
        triangle->primitiveIndex = 0;
        triangle->sceneObjectIndex = 0;
        triangle->localTriangleIndex = face_i;
    }
    scene->triangleMesh.triangles[8].p0 = vec3(-0.90, 0.35, -0.90);
    scene->triangleMesh.triangles[8].p1 = vec3(0.90, 0.35, -0.90);
    scene->triangleMesh.triangles[8].p2 = vec3(-0.90, 0.35, 0.90);
    scene->triangleMesh.triangles[8].normal = vec3(0.0, -1.0, 0.0);
    scene->triangleMesh.triangles[8].primitiveIndex = 1;
    scene->triangleMesh.triangles[8].sceneObjectIndex = 1;
    scene->triangleMesh.triangles[8].localTriangleIndex = 0;
    scene->triangleMesh.triangles[9].p0 = vec3(0.90, 0.35, -0.90);
    scene->triangleMesh.triangles[9].p1 = vec3(0.90, 0.35, 0.90);
    scene->triangleMesh.triangles[9].p2 = vec3(-0.90, 0.35, 0.90);
    scene->triangleMesh.triangles[9].normal = vec3(0.0, -1.0, 0.0);
    scene->triangleMesh.triangles[9].primitiveIndex = 1;
    scene->triangleMesh.triangles[9].sceneObjectIndex = 1;
    scene->triangleMesh.triangles[9].localTriangleIndex = 1;

    if (!prepared_suite_attach_dense_volume(&scene->volume,
                                            vec3(-1.20, -1.25, -0.70),
                                            16u,
                                            16u,
                                            18u,
                                            0.15,
                                            1.0f)) {
        RuntimeScene3D_Free(scene);
        return false;
    }
    RuntimeScene3D_RefreshCapabilities(scene);
    return true;
}

static void test_caustic_transport_enable_transport_with_flags(int sample_budget,
                                                               bool volume_cache,
                                                               bool surface_cache) {
    RuntimeCausticSettings3D settings;
    RuntimeCausticSettings3D_Default(&settings);
    settings.mode = RUNTIME_CAUSTIC_MODE_TRANSPORT;
    settings.volumeCacheEnabled = volume_cache;
    settings.surfaceCacheEnabled = surface_cache;
    settings.sampleBudget = sample_budget;
    settings.maxPathDepth = 2;
    RuntimeCausticTransport3D_SetRequestState(&settings);
}

static void test_caustic_transport_enable_analytic_sphere_lens(int sample_budget,
                                                               bool volume_cache,
                                                               bool surface_cache,
                                                               bool debug_export,
                                                               const char* output_root) {
    RuntimeCausticSettings3D settings;
    RuntimeCausticSettings3D_Default(&settings);
    settings.mode = RUNTIME_CAUSTIC_MODE_TRANSPORT;
    settings.volumeCacheEnabled = volume_cache;
    settings.surfaceCacheEnabled = surface_cache;
    settings.sampleBudget = sample_budget;
    settings.maxPathDepth = 2;
    settings.emissionPolicy = RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_SPHERE_LENS;
    settings.debugExportEnabled = debug_export;
    RuntimeCausticTransport3D_SetRequestState(&settings);
    if (output_root) {
        RuntimeCausticTransportDebug3D_SetOutputRoot(output_root);
    }
}

static void test_caustic_transport_enable_analytic_cylinder_lens(int sample_budget,
                                                                 bool volume_cache,
                                                                 bool surface_cache,
                                                                 bool debug_export,
                                                                 const char* output_root) {
    RuntimeCausticSettings3D settings;
    RuntimeCausticSettings3D_Default(&settings);
    settings.mode = RUNTIME_CAUSTIC_MODE_TRANSPORT;
    settings.volumeCacheEnabled = volume_cache;
    settings.surfaceCacheEnabled = surface_cache;
    settings.sampleBudget = sample_budget;
    settings.maxPathDepth = 2;
    settings.emissionPolicy = RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_CYLINDER_LENS;
    settings.debugExportEnabled = debug_export;
    RuntimeCausticTransport3D_SetRequestState(&settings);
    if (output_root) {
        RuntimeCausticTransportDebug3D_SetOutputRoot(output_root);
    }
}

static void test_caustic_transport_enable_focused_analytic_cylinder_lens(
    int sample_budget,
    bool volume_cache,
    bool surface_cache,
    bool debug_export,
    const char* output_root) {
    RuntimeCausticSettings3D settings;
    RuntimeCausticSettings3D_Default(&settings);
    settings.mode = RUNTIME_CAUSTIC_MODE_TRANSPORT;
    settings.volumeCacheEnabled = volume_cache;
    settings.surfaceCacheEnabled = surface_cache;
    settings.sampleBudget = sample_budget;
    settings.maxPathDepth = 2;
    settings.emissionPolicy =
        RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_CYLINDER_LENS_FOCUSED;
    settings.debugExportEnabled = debug_export;
    RuntimeCausticTransport3D_SetRequestState(&settings);
    if (output_root) {
        RuntimeCausticTransportDebug3D_SetOutputRoot(output_root);
    }
}

static void test_caustic_transport_enable_analytic_prism_lens(int sample_budget,
                                                              bool volume_cache,
                                                              bool surface_cache,
                                                              bool debug_export,
                                                              const char* output_root) {
    RuntimeCausticSettings3D settings;
    RuntimeCausticSettings3D_Default(&settings);
    settings.mode = RUNTIME_CAUSTIC_MODE_TRANSPORT;
    settings.volumeCacheEnabled = volume_cache;
    settings.surfaceCacheEnabled = surface_cache;
    settings.sampleBudget = sample_budget;
    settings.maxPathDepth = 2;
    settings.emissionPolicy = RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_PRISM_LENS;
    settings.debugExportEnabled = debug_export;
    RuntimeCausticTransport3D_SetRequestState(&settings);
    if (output_root) {
        RuntimeCausticTransportDebug3D_SetOutputRoot(output_root);
    }
}

static void test_caustic_transport_enable_analytic_bowl_lens(int sample_budget,
                                                             bool volume_cache,
                                                             bool surface_cache,
                                                             bool debug_export,
                                                             const char* output_root) {
    RuntimeCausticSettings3D settings;
    RuntimeCausticSettings3D_Default(&settings);
    settings.mode = RUNTIME_CAUSTIC_MODE_TRANSPORT;
    settings.volumeCacheEnabled = volume_cache;
    settings.surfaceCacheEnabled = surface_cache;
    settings.sampleBudget = sample_budget;
    settings.maxPathDepth = 2;
    settings.emissionPolicy = RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_BOWL_LENS;
    settings.debugExportEnabled = debug_export;
    RuntimeCausticTransport3D_SetRequestState(&settings);
    if (output_root) {
        RuntimeCausticTransportDebug3D_SetOutputRoot(output_root);
    }
}

static void test_caustic_transport_enable_transport_with_surface_calibration(
    int sample_budget,
    double radiance_scale,
    double footprint_scale) {
    RuntimeCausticSettings3D settings;
    RuntimeCausticSettings3D_Default(&settings);
    settings.mode = RUNTIME_CAUSTIC_MODE_TRANSPORT;
    settings.surfaceCacheEnabled = true;
    settings.sampleBudget = sample_budget;
    settings.maxPathDepth = 2;
    settings.surfaceRadianceScale = radiance_scale;
    settings.surfaceFootprintScale = footprint_scale;
    RuntimeCausticTransport3D_SetRequestState(&settings);
}

static void test_caustic_transport_enable_transport_surface_without_fallback(
    int sample_budget) {
    RuntimeCausticSettings3D settings;
    RuntimeCausticSettings3D_Default(&settings);
    settings.mode = RUNTIME_CAUSTIC_MODE_TRANSPORT;
    settings.surfaceCacheEnabled = true;
    settings.sampleBudget = sample_budget;
    settings.maxPathDepth = 2;
    settings.surfaceReceiverFallbackEnabled = false;
    RuntimeCausticTransport3D_SetRequestState(&settings);
}

static void test_caustic_transport_enable_transport_with_budget(int sample_budget) {
    test_caustic_transport_enable_transport_with_flags(sample_budget, true, false);
}

static void test_caustic_transport_enable_transport_debug_export(
    int sample_budget,
    const char* output_root) {
    RuntimeCausticSettings3D settings;
    RuntimeCausticSettings3D_Default(&settings);
    settings.mode = RUNTIME_CAUSTIC_MODE_TRANSPORT;
    settings.volumeCacheEnabled = true;
    settings.sampleBudget = sample_budget;
    settings.maxPathDepth = 2;
    settings.debugExportEnabled = true;
    RuntimeCausticTransport3D_SetRequestState(&settings);
    RuntimeCausticTransportDebug3D_SetOutputRoot(output_root);
}

static void test_caustic_transport_enable_transport(void) {
    test_caustic_transport_enable_transport_with_budget(8);
}

static void test_caustic_transport_seed_material_state(void) {
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    MaterialManagerResetDefaults();
    sceneSettings.objectCount = 2;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[0].color = 0xFFFFFF;
    sceneSettings.sceneObjects[0].alpha = 1.0;
    sceneSettings.sceneObjects[1].material_id = MATERIAL_PRESET_DEFAULT;
    sceneSettings.sceneObjects[1].color = 0xA0A0A0;
    sceneSettings.sceneObjects[1].alpha = 1.0;
}

static int test_runtime_caustic_transport_populates_volume_cache(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticTransport3DDiagnostics diagnostics;
    bool ok = false;

    test_caustic_transport_seed_material_state();
    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_transport_scene",
                test_caustic_transport_make_scene(&scene));
    test_caustic_transport_enable_transport();

    ok = RuntimeCausticTransport3D_PopulateVolumeCache(&scene, &cache, &diagnostics);
    assert_true("runtime_caustic_transport_populate_ok", ok);
    assert_true("runtime_caustic_transport_active", diagnostics.active);
    assert_true("runtime_caustic_transport_allocated", diagnostics.cacheAllocated);
    assert_true("runtime_caustic_transport_light_count", diagnostics.lightCount > 0u);
    assert_true("runtime_caustic_transport_multi_target_eval",
                diagnostics.evaluatedPathCount > 2u);
    assert_true("runtime_caustic_transport_paths", diagnostics.emittedPathCount > 0u);
    assert_true("runtime_caustic_transport_multi_target_emit",
                diagnostics.emittedPathCount > 1u);
    assert_true("runtime_caustic_transport_specular", diagnostics.specularEventCount > 0u);
    assert_true("runtime_caustic_transport_segments", diagnostics.volumeSegmentCount > 0u);
    assert_true("runtime_caustic_transport_deposits",
                diagnostics.depositAcceptedCount > 0u);
    assert_true("runtime_caustic_transport_nonzero",
                diagnostics.cache.nonZeroCellCount > 0u);
    assert_true("runtime_caustic_transport_radiance",
                diagnostics.cache.totalRadianceR > 0.0);
    assert_true("runtime_caustic_transport_footprints",
                diagnostics.cache.footprintDepositCount > 0u);
    assert_true("runtime_caustic_transport_footprint_cells",
                diagnostics.cache.footprintCellContributionCount >
                    diagnostics.cache.footprintDepositCount);
    assert_true("runtime_caustic_transport_footprint_radius",
                diagnostics.cache.averageFootprintRadiusVoxels > 0.0);
    assert_true("runtime_caustic_transport_footprint_radius_broad_enough",
                diagnostics.cache.averageFootprintRadiusVoxels >= 1.5);
    assert_close("runtime_caustic_transport_footprint_energy_r",
                 diagnostics.cache.footprintDepositedRadianceR,
                 diagnostics.cache.footprintInputRadianceR,
                 1e-4);

    RuntimeCausticVolumeCache3D_Free(&cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_respects_sample_budget(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticTransport3DDiagnostics diagnostics;
    bool ok = false;

    test_caustic_transport_seed_material_state();
    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_transport_budget_scene",
                test_caustic_transport_make_scene(&scene));
    test_caustic_transport_enable_transport_with_budget(3);

    ok = RuntimeCausticTransport3D_PopulateVolumeCache(&scene, &cache, &diagnostics);
    assert_true("runtime_caustic_transport_budget_populate_ok", ok);
    assert_true("runtime_caustic_transport_budget_eval_count",
                diagnostics.evaluatedPathCount == 3u);
    assert_true("runtime_caustic_transport_budget_emit_count",
                diagnostics.emittedPathCount > 0u && diagnostics.emittedPathCount <= 3u);
    assert_true("runtime_caustic_transport_budget_deposits",
                diagnostics.depositAcceptedCount > 0u);

    RuntimeCausticVolumeCache3D_Free(&cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_analytic_sphere_lens_populates_volume_cache(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticTransport3DDiagnostics diagnostics;
    bool ok = false;

    test_caustic_transport_seed_material_state();
    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_transport_analytic_scene",
                test_caustic_transport_make_analytic_sphere_scene(&scene));
    test_caustic_transport_enable_analytic_sphere_lens(12, true, false, false, NULL);

    ok = RuntimeCausticTransport3D_PopulateVolumeCache(&scene, &cache, &diagnostics);
    assert_true("runtime_caustic_transport_analytic_populate_ok", ok);
    assert_true("runtime_caustic_transport_analytic_policy_resolved",
                diagnostics.analyticSphereLensResolvedCount == 1u);
    assert_true("runtime_caustic_transport_analytic_eval_count",
                diagnostics.analyticSphereLensEvaluatedPathCount > 0u &&
                    diagnostics.analyticSphereLensEvaluatedPathCount <= 12u);
    assert_true("runtime_caustic_transport_analytic_emit_count",
                diagnostics.analyticSphereLensEmittedPathCount > 0u);
    assert_true("runtime_caustic_transport_analytic_only",
                diagnostics.evaluatedPathCount ==
                    diagnostics.analyticSphereLensEvaluatedPathCount);
    assert_close("runtime_caustic_transport_analytic_sample_weight",
                 diagnostics.analyticSphereLensSampleWeight,
                 0.2,
                 1.0e-9);
    assert_close("runtime_caustic_transport_analytic_total_sample_weight",
                 diagnostics.analyticSphereLensTotalSampleWeight,
                 (double)diagnostics.analyticSphereLensEvaluatedPathCount * 0.2,
                 1.0e-9);
    assert_true("runtime_caustic_transport_analytic_cache",
                diagnostics.cache.nonZeroCellCount > 0u);
    assert_true("runtime_caustic_transport_analytic_deposits",
                diagnostics.depositAcceptedCount > 0u);

    RuntimeCausticVolumeCache3D_Free(&cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_analytic_sphere_lens_debug_export_records_policy(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticTransport3DDiagnostics diagnostics;
    const RuntimeCausticTransportDebugPath3D* path = NULL;
    const char* output_root = "/tmp/ray_tracing_caustic_transport_analytic_debug_test";

    test_caustic_transport_seed_material_state();
    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_transport_analytic_debug_scene",
                test_caustic_transport_make_analytic_sphere_scene(&scene));
    assert_true("runtime_caustic_transport_analytic_debug_output_root",
                config_io_ensure_directory_exists(output_root));
    test_caustic_transport_enable_analytic_sphere_lens(12, true, false, true, output_root);

    assert_true("runtime_caustic_transport_analytic_debug_populate",
                RuntimeCausticTransport3D_PopulateVolumeCache(&scene,
                                                              &cache,
                                                              &diagnostics));
    assert_true("runtime_caustic_transport_analytic_debug_records",
                RuntimeCausticTransportDebug3D_RecordCount() > 0u);
    path = RuntimeCausticTransportDebug3D_RecordAt(0u);
    assert_true("runtime_caustic_transport_analytic_debug_first_path", path != NULL);
    assert_true("runtime_caustic_transport_analytic_debug_policy",
                strcmp(path->emissionPolicy, "analytic_sphere_lens") == 0);
    assert_true("runtime_caustic_transport_analytic_debug_event",
                strcmp(path->eventType, "analytic_sphere_lens") == 0);
    assert_true("runtime_caustic_transport_analytic_debug_no_triangle_target",
                path->targetTriangleIndex == -1);
    assert_true("runtime_caustic_transport_analytic_debug_entry_exit",
                vec3_length(vec3_sub(path->sphereLensEntryPosition,
                                     path->sphereLensExitPosition)) > 0.1);
    assert_true("runtime_caustic_transport_analytic_debug_inside_distance",
                path->sphereLensInsideDistance > 0.1);
    assert_true("runtime_caustic_transport_analytic_debug_lens_shape",
                strcmp(path->lensShapeKind, "sphere") == 0);
    assert_true("runtime_caustic_transport_analytic_debug_lens_identity",
                path->lensSceneObjectIndex == path->targetSceneObjectIndex &&
                    path->lensPrimitiveIndex == path->targetPrimitiveIndex);
    assert_true("runtime_caustic_transport_analytic_debug_lens_events",
                path->lensInterfaceEventCount == 2u);
    assert_true("runtime_caustic_transport_analytic_debug_lens_entry_exit",
                vec3_length(vec3_sub(path->lensEntryPosition,
                                     path->lensExitPosition)) > 0.1);
    assert_true("runtime_caustic_transport_analytic_debug_lens_directions",
                vec3_length(path->lensEntryOutgoingDirection) > 0.9 &&
                    vec3_length(path->lensPostExitDirection) > 0.9);
    assert_true("runtime_caustic_transport_analytic_debug_lens_ior",
                path->lensEntryEtaFrom == 1.0 && path->lensEntryEtaTo > 1.0 &&
                    path->lensExitEtaFrom > 1.0 && path->lensExitEtaTo == 1.0);
    assert_true("runtime_caustic_transport_analytic_debug_lens_fresnel",
                path->lensEntryFresnel >= 0.0 && path->lensEntryFresnel <= 1.0 &&
                    path->lensExitFresnel >= 0.0 && path->lensExitFresnel <= 1.0);
    assert_close("runtime_caustic_transport_analytic_debug_lens_inside_distance",
                 path->lensInsideDistance,
                 path->sphereLensInsideDistance,
                 1e-9);
    assert_true("runtime_caustic_transport_analytic_debug_lens_sampling",
                path->lensSampleWeight > 0.0 && path->lensPathPdf > 0.0);
    assert_true("runtime_caustic_transport_analytic_debug_lens_no_tir",
                !path->lensTotalInternalReflection);
    assert_true("runtime_caustic_transport_analytic_debug_outside_before_volume",
                path->exitedSpecularObjectBeforeVolumeDeposit);
    assert_true("runtime_caustic_transport_analytic_debug_volume",
                path->volumeDepositAcceptedCount > 0u);

    RuntimeCausticVolumeCache3D_Free(&cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_analytic_sphere_lens_rejects_non_sphere(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticTransport3DDiagnostics diagnostics;

    test_caustic_transport_seed_material_state();
    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_transport_analytic_reject_scene",
                test_caustic_transport_make_scene(&scene));
    test_caustic_transport_enable_analytic_sphere_lens(8, true, false, false, NULL);

    assert_true("runtime_caustic_transport_analytic_reject_populate",
                !RuntimeCausticTransport3D_PopulateVolumeCache(&scene,
                                                               &cache,
                                                               &diagnostics));
    assert_true("runtime_caustic_transport_analytic_reject_count",
                diagnostics.analyticSphereLensRejectedCount == 1u);
    assert_true("runtime_caustic_transport_analytic_reject_no_fallback",
                diagnostics.evaluatedPathCount == 0u &&
                    diagnostics.emittedPathCount == 0u);

    RuntimeCausticVolumeCache3D_Free(&cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_analytic_cylinder_lens_populates_volume_cache(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticTransport3DDiagnostics diagnostics;
    bool ok = false;

    test_caustic_transport_seed_material_state();
    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_transport_analytic_cylinder_scene",
                test_caustic_transport_make_analytic_cylinder_scene(&scene));
    test_caustic_transport_enable_analytic_cylinder_lens(12, true, false, false, NULL);

    ok = RuntimeCausticTransport3D_PopulateVolumeCache(&scene, &cache, &diagnostics);
    assert_true("runtime_caustic_transport_analytic_cylinder_populate_ok", ok);
    assert_true("runtime_caustic_transport_analytic_cylinder_policy_resolved",
                diagnostics.analyticCylinderLensResolvedCount == 1u);
    assert_true("runtime_caustic_transport_analytic_cylinder_no_sphere",
                diagnostics.analyticSphereLensResolvedCount == 0u);
    assert_true("runtime_caustic_transport_analytic_cylinder_eval_count",
                diagnostics.analyticCylinderLensEvaluatedPathCount > 0u &&
                    diagnostics.analyticCylinderLensEvaluatedPathCount <= 12u);
    assert_true("runtime_caustic_transport_analytic_cylinder_emit_count",
                diagnostics.analyticCylinderLensEmittedPathCount > 0u);
    assert_true("runtime_caustic_transport_analytic_cylinder_only",
                diagnostics.evaluatedPathCount ==
                    diagnostics.analyticCylinderLensEvaluatedPathCount);
    assert_close("runtime_caustic_transport_analytic_cylinder_sample_weight",
                 diagnostics.analyticCylinderLensSampleWeight,
                 0.2,
                 1.0e-9);
    assert_close("runtime_caustic_transport_analytic_cylinder_total_sample_weight",
                 diagnostics.analyticCylinderLensTotalSampleWeight,
                 (double)diagnostics.analyticCylinderLensEvaluatedPathCount * 0.2,
                 1.0e-9);
    assert_true("runtime_caustic_transport_analytic_cylinder_cache",
                diagnostics.cache.nonZeroCellCount > 0u);
    assert_true("runtime_caustic_transport_analytic_cylinder_deposits",
                diagnostics.depositAcceptedCount > 0u);

    RuntimeCausticVolumeCache3D_Free(&cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_analytic_cylinder_lens_debug_export_records_policy(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticTransport3DDiagnostics diagnostics;
    const RuntimeCausticTransportDebugPath3D* path = NULL;
    const char* output_root = "/tmp/ray_tracing_caustic_transport_analytic_cylinder_debug_test";

    test_caustic_transport_seed_material_state();
    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_transport_analytic_cylinder_debug_scene",
                test_caustic_transport_make_analytic_cylinder_scene(&scene));
    assert_true("runtime_caustic_transport_analytic_cylinder_debug_output_root",
                config_io_ensure_directory_exists(output_root));
    test_caustic_transport_enable_analytic_cylinder_lens(12, true, false, true, output_root);

    assert_true("runtime_caustic_transport_analytic_cylinder_debug_populate",
                RuntimeCausticTransport3D_PopulateVolumeCache(&scene,
                                                              &cache,
                                                              &diagnostics));
    assert_true("runtime_caustic_transport_analytic_cylinder_debug_records",
                RuntimeCausticTransportDebug3D_RecordCount() > 0u);
    path = RuntimeCausticTransportDebug3D_RecordAt(0u);
    assert_true("runtime_caustic_transport_analytic_cylinder_debug_first_path", path != NULL);
    assert_true("runtime_caustic_transport_analytic_cylinder_debug_policy",
                strcmp(path->emissionPolicy, "analytic_cylinder_lens") == 0);
    assert_true("runtime_caustic_transport_analytic_cylinder_debug_event",
                strcmp(path->eventType, "analytic_cylinder_lens") == 0);
    assert_true("runtime_caustic_transport_analytic_cylinder_debug_lens_shape",
                strcmp(path->lensShapeKind, "cylinder") == 0);
    assert_true("runtime_caustic_transport_analytic_cylinder_debug_no_triangle_target",
                path->targetTriangleIndex == -1);
    assert_true("runtime_caustic_transport_analytic_cylinder_debug_lens_identity",
                path->lensSceneObjectIndex == path->targetSceneObjectIndex &&
                    path->lensPrimitiveIndex == path->targetPrimitiveIndex);
    assert_true("runtime_caustic_transport_analytic_cylinder_debug_lens_events",
                path->lensInterfaceEventCount == 2u);
    assert_true("runtime_caustic_transport_analytic_cylinder_debug_lens_entry_exit",
                vec3_length(vec3_sub(path->lensEntryPosition,
                                     path->lensExitPosition)) > 0.1);
    assert_true("runtime_caustic_transport_analytic_cylinder_debug_lens_ior",
                path->lensEntryEtaFrom == 1.0 && path->lensEntryEtaTo > 1.0 &&
                    path->lensExitEtaFrom > 1.0 && path->lensExitEtaTo == 1.0);
    assert_true("runtime_caustic_transport_analytic_cylinder_debug_lens_sampling",
                path->lensSampleWeight > 0.0 && path->lensPathPdf > 0.0);
    assert_true("runtime_caustic_transport_analytic_cylinder_debug_volume",
                path->volumeDepositAcceptedCount > 0u);

    RuntimeCausticVolumeCache3D_Free(&cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_debug_export_disabled_records_nothing(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticTransport3DDiagnostics diagnostics;

    test_caustic_transport_seed_material_state();
    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_transport_debug_disabled_scene",
                test_caustic_transport_make_scene(&scene));
    test_caustic_transport_enable_transport();

    assert_true("runtime_caustic_transport_debug_disabled_populate",
                RuntimeCausticTransport3D_PopulateVolumeCache(&scene,
                                                              &cache,
                                                              &diagnostics));
    assert_true("runtime_caustic_transport_debug_disabled_count",
                RuntimeCausticTransportDebug3D_RecordCount() == 0u);
    assert_true("runtime_caustic_transport_debug_disabled_state",
                !RuntimeCausticTransportDebug3D_IsEnabled());

    RuntimeCausticVolumeCache3D_Free(&cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_debug_export_records_path_geometry(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticTransport3DDiagnostics diagnostics;
    RuntimeCausticTransportDebug3DState debug_state;
    const RuntimeCausticTransportDebugPath3D* path = NULL;
    const char* output_root = "/tmp/ray_tracing_caustic_transport_debug_export_test";

    test_caustic_transport_seed_material_state();
    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_transport_debug_scene",
                test_caustic_transport_make_scene(&scene));
    assert_true("runtime_caustic_transport_debug_output_root",
                config_io_ensure_directory_exists(output_root));
    test_caustic_transport_enable_transport_debug_export(8, output_root);

    assert_true("runtime_caustic_transport_debug_populate",
                RuntimeCausticTransport3D_PopulateVolumeCache(&scene,
                                                              &cache,
                                                              &diagnostics));
    debug_state = RuntimeCausticTransportDebug3D_State();
    assert_true("runtime_caustic_transport_debug_record_count",
                RuntimeCausticTransportDebug3D_RecordCount() > 0u);
    assert_true("runtime_caustic_transport_debug_state_recorded",
                debug_state.recordedPathCount == RuntimeCausticTransportDebug3D_RecordCount());
    assert_true("runtime_caustic_transport_debug_summary_path",
                debug_state.summaryPath[0] != '\0' && access(debug_state.summaryPath, F_OK) == 0);
    assert_true("runtime_caustic_transport_debug_paths_path",
                debug_state.pathsPath[0] != '\0' && access(debug_state.pathsPath, F_OK) == 0);
    path = RuntimeCausticTransportDebug3D_RecordAt(0u);
    assert_true("runtime_caustic_transport_debug_first_path", path != NULL);
    assert_true("runtime_caustic_transport_debug_light",
                path->lightIntensity > 0.0 && path->lightRadius > 0.0);
    assert_true("runtime_caustic_transport_debug_target",
                path->targetTriangleIndex >= 0 && path->targetSampleIndex >= 0);
    assert_true("runtime_caustic_transport_debug_first_hit",
                path->materialId == MATERIAL_PRESET_TRANSPARENT && path->eligible);
    assert_true("runtime_caustic_transport_debug_event",
                strcmp(path->eventType, "refraction") == 0 ||
                    strcmp(path->eventType, "reflection") == 0);
    assert_true("runtime_caustic_transport_debug_direction",
                vec3_length(path->outgoingDirection) > 0.9);
    assert_true("runtime_caustic_transport_debug_volume_clip",
                path->volumeClipHit);
    assert_true("runtime_caustic_transport_debug_volume_steps",
                path->volumeStepCount > 0);
    assert_true("runtime_caustic_transport_debug_volume_deposits",
                path->volumeDepositAcceptedCount > 0u);
    assert_true("runtime_caustic_transport_debug_footprint",
                path->footprintRadiusMax >= path->footprintRadiusMin &&
                    path->footprintRadiusMax > 0.0);

    RuntimeCausticVolumeCache3D_Free(&cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_populates_surface_cache(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D volume_cache;
    RuntimeCausticSurfaceCache3D surface_cache;
    RuntimeCausticTransport3DDiagnostics diagnostics;
    bool ok = false;

    test_caustic_transport_seed_material_state();
    RuntimeCausticVolumeCache3D_Init(&volume_cache);
    RuntimeCausticSurfaceCache3D_Init(&surface_cache);
    assert_true("runtime_caustic_transport_surface_scene",
                test_caustic_transport_make_scene(&scene));
    test_caustic_transport_enable_transport_with_flags(8, false, true);

    ok = RuntimeCausticTransport3D_PopulateCaches(&scene,
                                                  &volume_cache,
                                                  &surface_cache,
                                                  &diagnostics);
    assert_true("runtime_caustic_transport_surface_populate_ok", ok);
    assert_true("runtime_caustic_transport_surface_active", diagnostics.active);
    assert_true("runtime_caustic_transport_surface_allocated",
                diagnostics.surfaceCacheAllocated);
    assert_true("runtime_caustic_transport_surface_records",
                diagnostics.surfaceCache.recordCount > 0u);
    assert_true("runtime_caustic_transport_surface_deposits",
                diagnostics.surfaceCache.depositAcceptedCount > 0u);
    assert_true("runtime_caustic_transport_surface_no_volume",
                !RuntimeCausticVolumeCache3D_IsAllocated(&volume_cache));

    RuntimeCausticSurfaceCache3D_Free(&surface_cache);
    RuntimeCausticVolumeCache3D_Free(&volume_cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_suppresses_volume_without_vf3d(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D volume_cache;
    RuntimeCausticSurfaceCache3D surface_cache;
    RuntimeCausticTransport3DDiagnostics diagnostics;
    bool ok = false;

    test_caustic_transport_seed_material_state();
    RuntimeCausticVolumeCache3D_Init(&volume_cache);
    RuntimeCausticSurfaceCache3D_Init(&surface_cache);
    assert_true("runtime_caustic_transport_no_vf3d_scene",
                test_caustic_transport_make_scene(&scene));
    RuntimeVolumeAttachment3D_Reset(&scene.volume);
    RuntimeScene3D_RefreshCapabilities(&scene);
    test_caustic_transport_enable_transport_with_flags(8, true, true);

    ok = RuntimeCausticTransport3D_PopulateCaches(&scene,
                                                  &volume_cache,
                                                  &surface_cache,
                                                  &diagnostics);
    assert_true("runtime_caustic_transport_no_vf3d_surface_ok", ok);
    assert_true("runtime_caustic_transport_no_vf3d_suppressed",
                diagnostics.volumeCacheSuppressedNoSampleableVolume);
    assert_true("runtime_caustic_transport_no_vf3d_no_volume_alloc",
                !RuntimeCausticVolumeCache3D_IsAllocated(&volume_cache));
    assert_true("runtime_caustic_transport_no_vf3d_no_volume_deposits",
                diagnostics.cache.nonZeroCellCount == 0u);
    assert_true("runtime_caustic_transport_no_vf3d_no_volume_segments",
                diagnostics.volumeSegmentCount == 0u);
    assert_true("runtime_caustic_transport_no_vf3d_surface_allocated",
                diagnostics.surfaceCacheAllocated);
    assert_true("runtime_caustic_transport_no_vf3d_surface_records",
                diagnostics.surfaceCache.recordCount > 0u);
    assert_true("runtime_caustic_transport_no_vf3d_surface_deposits",
                diagnostics.surfaceCache.depositAcceptedCount > 0u);

    RuntimeCausticSurfaceCache3D_Free(&surface_cache);
    RuntimeCausticVolumeCache3D_Free(&volume_cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_surface_calibration_scales_records(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D volume_cache;
    RuntimeCausticSurfaceCache3D default_cache;
    RuntimeCausticSurfaceCache3D calibrated_cache;
    RuntimeCausticTransport3DDiagnostics default_diagnostics;
    RuntimeCausticTransport3DDiagnostics calibrated_diagnostics;
    double default_radius = 0.0;
    double calibrated_radius = 0.0;
    bool ok = false;

    test_caustic_transport_seed_material_state();
    RuntimeCausticVolumeCache3D_Init(&volume_cache);
    RuntimeCausticSurfaceCache3D_Init(&default_cache);
    RuntimeCausticSurfaceCache3D_Init(&calibrated_cache);
    assert_true("runtime_caustic_transport_calibrated_scene",
                test_caustic_transport_make_scene(&scene));

    test_caustic_transport_enable_transport_with_surface_calibration(8, 1.0, 1.0);
    ok = RuntimeCausticTransport3D_PopulateCaches(&scene,
                                                  &volume_cache,
                                                  &default_cache,
                                                  &default_diagnostics);
    assert_true("runtime_caustic_transport_calibrated_default_ok", ok);
    assert_true("runtime_caustic_transport_calibrated_default_records",
                default_cache.recordCount > 0u);
    default_radius = default_cache.records[0].radius;

    test_caustic_transport_enable_transport_with_surface_calibration(8, 8.0, 3.0);
    ok = RuntimeCausticTransport3D_PopulateCaches(&scene,
                                                  &volume_cache,
                                                  &calibrated_cache,
                                                  &calibrated_diagnostics);
    assert_true("runtime_caustic_transport_calibrated_scaled_ok", ok);
    assert_true("runtime_caustic_transport_calibrated_scaled_records",
                calibrated_cache.recordCount > 0u);
    calibrated_radius = calibrated_cache.records[0].radius;

    assert_true("runtime_caustic_transport_calibrated_radius",
                calibrated_radius > default_radius * 2.0);
    assert_true("runtime_caustic_transport_calibrated_radiance",
                calibrated_diagnostics.surfaceCache.maxRecordRadiance >
                    default_diagnostics.surfaceCache.maxRecordRadiance * 4.0);

    RuntimeCausticSurfaceCache3D_Free(&calibrated_cache);
    RuntimeCausticSurfaceCache3D_Free(&default_cache);
    RuntimeCausticVolumeCache3D_Free(&volume_cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_surface_without_receiver_fallback(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D volume_cache;
    RuntimeCausticSurfaceCache3D surface_cache;
    RuntimeCausticTransport3DDiagnostics diagnostics;
    bool ok = false;

    test_caustic_transport_seed_material_state();
    RuntimeCausticVolumeCache3D_Init(&volume_cache);
    RuntimeCausticSurfaceCache3D_Init(&surface_cache);
    assert_true("runtime_caustic_transport_no_fallback_scene",
                test_caustic_transport_make_scene(&scene));
    test_caustic_transport_enable_transport_surface_without_fallback(8);

    ok = RuntimeCausticTransport3D_PopulateCaches(&scene,
                                                  &volume_cache,
                                                  &surface_cache,
                                                  &diagnostics);
    assert_true("runtime_caustic_transport_no_fallback_ok", ok);
    assert_true("runtime_caustic_transport_no_fallback_records",
                diagnostics.surfaceCache.recordCount > 0u);
    assert_true("runtime_caustic_transport_no_fallback_hits",
                diagnostics.surfaceReceiverHitCount > 0u);
    assert_true("runtime_caustic_transport_no_fallback_unused",
                diagnostics.surfaceReceiverFallbackCount == 0u);

    RuntimeCausticSurfaceCache3D_Free(&surface_cache);
    RuntimeCausticVolumeCache3D_Free(&volume_cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_render_samples_volume_cache(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimeNative3DPreparedFrame* frame = test_caustic_transport_alloc_prepared_frame();
    RuntimeNative3DRenderStats stats = {0};
    float* radiance = (float*)calloc(TEST_CAUSTIC_TRANSPORT_RADIANCE_COUNT, sizeof(*radiance));
    bool ok = false;

    assert_true("runtime_caustic_transport_render_alloc", frame && radiance);
    if (!frame || !radiance) {
        free(radiance);
        test_caustic_transport_free_prepared_frame(frame);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        RuntimeCausticTransport3D_ResetRequestState();
        return 0;
    }
    test_caustic_transport_seed_material_state();
    animSettings.environmentBrightness = 0.0;
    animSettings.environmentLightMode = ENVIRONMENT_LIGHT_MODE_OFF;
    assert_true("runtime_caustic_transport_render_scene",
                test_caustic_transport_make_scene(&scene));
    ok = RuntimeCameraProjector3D_Build(&scene.camera,
                                        TEST_CAUSTIC_TRANSPORT_RENDER_WIDTH,
                                        TEST_CAUSTIC_TRANSPORT_RENDER_HEIGHT,
                                        &frame->projector);
    assert_true("runtime_caustic_transport_render_projector", ok);
    frame->scene = scene;
    frame->width = TEST_CAUSTIC_TRANSPORT_RENDER_WIDTH;
    frame->height = TEST_CAUSTIC_TRANSPORT_RENDER_HEIGHT;
    frame->valid = true;
    test_caustic_transport_enable_transport();
    assert_true("runtime_caustic_transport_render_populate",
                RuntimeCausticTransport3D_PopulateVolumeCache(
                    &frame->scene,
                    &frame->causticVolumeCache,
                    &frame->causticTransportDiagnostics));

    ok = RuntimeNative3DRenderPreparedRegionRadianceRGB(radiance,
                                                        TEST_CAUSTIC_TRANSPORT_RENDER_WIDTH,
                                                        RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
                                                        frame,
                                                        0,
                                                        0,
                                                        TEST_CAUSTIC_TRANSPORT_RENDER_WIDTH,
                                                        TEST_CAUSTIC_TRANSPORT_RENDER_HEIGHT,
                                                        &stats);
    assert_true("runtime_caustic_transport_render_ok", ok);
    assert_true("runtime_caustic_transport_render_transport_stats",
                stats.causticTransportPathEmissionActive > 0);
    assert_true("runtime_caustic_transport_render_no_bootstrap",
                stats.causticBootstrapTemporaryBridgeActive == 0);
    assert_true("runtime_caustic_transport_render_cache_bound",
                stats.causticVolumeCacheBound > 0);
    assert_true("runtime_caustic_transport_render_scatter_samples",
                stats.causticVolumeScatterSampleCount > 0);
    assert_true("runtime_caustic_transport_render_scatter_contrib",
                stats.causticVolumeScatterContributingSampleCount > 0);
    assert_true("runtime_caustic_transport_render_scatter_radiance",
                stats.totalCausticVolumeScatterRadianceR > 0.0);

    test_caustic_transport_free_prepared_frame(frame);
    free(radiance);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_render_samples_surface_cache(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeDisneyV2CausticMode3D saved_caustic_mode = RuntimeDisneyV2_3D_CausticMode();
    double saved_caustic_strength = RuntimeDisneyV2_3D_CausticSidecarStrength();
    RuntimeScene3D scene;
    RuntimeNative3DPreparedFrame* frame = test_caustic_transport_alloc_prepared_frame();
    RuntimeNative3DRenderStats stats = {0};
    float* radiance = (float*)calloc(TEST_CAUSTIC_TRANSPORT_RADIANCE_COUNT, sizeof(*radiance));
    bool ok = false;

    assert_true("runtime_caustic_transport_render_surface_alloc", frame && radiance);
    if (!frame || !radiance) {
        free(radiance);
        test_caustic_transport_free_prepared_frame(frame);
        RuntimeDisneyV2_3D_SetCausticMode(saved_caustic_mode, saved_caustic_strength);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        RuntimeCausticTransport3D_ResetRequestState();
        return 0;
    }
    test_caustic_transport_seed_material_state();
    animSettings.environmentBrightness = 0.0;
    animSettings.environmentLightMode = ENVIRONMENT_LIGHT_MODE_OFF;
    RuntimeDisneyV2_3D_SetCausticMode(RUNTIME_DISNEY_V2_CAUSTIC_MODE_OFF, 0.0);
    assert_true("runtime_caustic_transport_render_surface_scene",
                test_caustic_transport_make_scene(&scene));
    ok = RuntimeCameraProjector3D_Build(&scene.camera,
                                        TEST_CAUSTIC_TRANSPORT_RENDER_WIDTH,
                                        TEST_CAUSTIC_TRANSPORT_RENDER_HEIGHT,
                                        &frame->projector);
    assert_true("runtime_caustic_transport_render_surface_projector", ok);
    frame->scene = scene;
    frame->width = TEST_CAUSTIC_TRANSPORT_RENDER_WIDTH;
    frame->height = TEST_CAUSTIC_TRANSPORT_RENDER_HEIGHT;
    frame->valid = true;
    test_caustic_transport_enable_transport_with_flags(8, false, true);
    assert_true("runtime_caustic_transport_render_surface_populate",
                RuntimeCausticTransport3D_PopulateCaches(
                    &frame->scene,
                    &frame->causticVolumeCache,
                    &frame->causticSurfaceCache,
                    &frame->causticTransportDiagnostics));

    ok = RuntimeNative3DRenderPreparedRegionRadianceRGB(radiance,
                                                        TEST_CAUSTIC_TRANSPORT_RENDER_WIDTH,
                                                        RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,
                                                        frame,
                                                        0,
                                                        0,
                                                        TEST_CAUSTIC_TRANSPORT_RENDER_WIDTH,
                                                        TEST_CAUSTIC_TRANSPORT_RENDER_HEIGHT,
                                                        &stats);
    assert_true("runtime_caustic_transport_render_surface_ok", ok);
    assert_true("runtime_caustic_transport_render_surface_bound",
                stats.causticSurfaceCacheBound > 0);
    assert_true("runtime_caustic_transport_render_surface_records",
                stats.causticSurfaceCacheRecordCount > 0);
    assert_true("runtime_caustic_transport_render_surface_samples",
                stats.causticSurfaceCacheSampleLookupCount > 0);
    assert_true("runtime_caustic_transport_render_surface_contrib",
                stats.causticSurfaceCacheSampleContributingCount > 0);
    assert_true("runtime_caustic_transport_render_surface_radiance",
                stats.totalCausticSurfaceRadianceR > 0.0);
    assert_true("runtime_caustic_transport_render_surface_no_sidecar",
                stats.causticSidecarEnabled == 0);

    test_caustic_transport_free_prepared_frame(frame);
    free(radiance);
    RuntimeDisneyV2_3D_SetCausticMode(saved_caustic_mode, saved_caustic_strength);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_sidecar_uses_prepared_probe_snapshot(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeDisneyV2CausticMode3D saved_caustic_mode = RuntimeDisneyV2_3D_CausticMode();
    double saved_caustic_strength = RuntimeDisneyV2_3D_CausticSidecarStrength();
    RuntimeDisneyV2CausticSidecarDiagnostics3D sidecar_diagnostics = {0};
    RuntimeScene3D scene;
    RuntimeNative3DPreparedFrame* frame = test_caustic_transport_alloc_prepared_frame();
    RuntimeNative3DRenderStats stats = {0};
    float* radiance = (float*)calloc(TEST_CAUSTIC_TRANSPORT_RADIANCE_COUNT, sizeof(*radiance));
    bool ok = false;

    assert_true("runtime_caustic_sidecar_snapshot_alloc", frame && radiance);
    if (!frame || !radiance) {
        free(radiance);
        test_caustic_transport_free_prepared_frame(frame);
        RuntimeDisneyV2_3D_SetCausticMode(saved_caustic_mode, saved_caustic_strength);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        RuntimeCausticTransport3D_ResetRequestState();
        return 0;
    }
    test_caustic_transport_seed_material_state();
    animSettings.environmentBrightness = 0.0;
    animSettings.environmentLightMode = ENVIRONMENT_LIGHT_MODE_OFF;
    RuntimeDisneyV2_3D_SetCausticMode(RUNTIME_DISNEY_V2_CAUSTIC_MODE_ANALYTIC, 1.0);
    assert_true("runtime_caustic_sidecar_snapshot_scene",
                test_caustic_transport_make_scene(&scene));
    ok = RuntimeCameraProjector3D_Build(&scene.camera,
                                        TEST_CAUSTIC_TRANSPORT_RENDER_WIDTH,
                                        TEST_CAUSTIC_TRANSPORT_RENDER_HEIGHT,
                                        &frame->projector);
    assert_true("runtime_caustic_sidecar_snapshot_projector", ok);
    frame->scene = scene;
    frame->width = TEST_CAUSTIC_TRANSPORT_RENDER_WIDTH;
    frame->height = TEST_CAUSTIC_TRANSPORT_RENDER_HEIGHT;
    RuntimeDisneyV2_3D_ResetCausticSidecarDiagnostics();
    frame->causticSidecarProbeValid =
        RuntimeDisneyV2_3D_BuildCausticSidecarProbe(&frame->scene,
                                                    &frame->causticSidecarProbe);
    RuntimeDisneyV2_3D_SnapshotCausticSidecarDiagnostics(&sidecar_diagnostics);
    frame->valid = true;
    assert_true("runtime_caustic_sidecar_snapshot_probe_valid",
                frame->causticSidecarProbeValid);
    assert_true("runtime_caustic_sidecar_snapshot_probe_build_count",
                sidecar_diagnostics.probeBuildCount == 1u);
    assert_true("runtime_caustic_sidecar_snapshot_triangle_scan_count",
                sidecar_diagnostics.triangleScanCount == 4u);
    assert_true("runtime_caustic_sidecar_snapshot_object_lookup_count",
                sidecar_diagnostics.objectTransmissiveLookupCount == 1u);
    assert_true("runtime_caustic_sidecar_snapshot_material_resolve_count",
                sidecar_diagnostics.materialResolveCount == 0u);

    RuntimeDisneyV2_3D_SetCausticMode(RUNTIME_DISNEY_V2_CAUSTIC_MODE_OFF, 0.0);
    ok = RuntimeNative3DRenderPreparedRegionRadianceRGB(radiance,
                                                        TEST_CAUSTIC_TRANSPORT_RENDER_WIDTH,
                                                        RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,
                                                        frame,
                                                        0,
                                                        0,
                                                        TEST_CAUSTIC_TRANSPORT_RENDER_WIDTH,
                                                        TEST_CAUSTIC_TRANSPORT_RENDER_HEIGHT,
                                                        &stats);
    assert_true("runtime_caustic_sidecar_snapshot_render_ok", ok);
    assert_true("runtime_caustic_sidecar_snapshot_enabled",
                stats.causticSidecarEnabled > 0);
    assert_true("runtime_caustic_sidecar_snapshot_samples",
                stats.causticSidecarSampleCount > 0);

    test_caustic_transport_free_prepared_frame(frame);
    free(radiance);
    RuntimeDisneyV2_3D_SetCausticMode(saved_caustic_mode, saved_caustic_strength);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_focused_analytic_cylinder_lens_debug_export_records_policy(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticTransport3DDiagnostics diagnostics;
    const RuntimeCausticTransportDebugPath3D* path = NULL;
    const char* output_root =
        "/tmp/ray_tracing_caustic_transport_focused_analytic_cylinder_debug_test";

    test_caustic_transport_seed_material_state();
    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_transport_focused_analytic_cylinder_debug_scene",
                test_caustic_transport_make_analytic_cylinder_scene(&scene));
    assert_true("runtime_caustic_transport_focused_analytic_cylinder_debug_output_root",
                config_io_ensure_directory_exists(output_root));
    test_caustic_transport_enable_focused_analytic_cylinder_lens(12,
                                                                 true,
                                                                 false,
                                                                 true,
                                                                 output_root);

    assert_true("runtime_caustic_transport_focused_analytic_cylinder_debug_populate",
                RuntimeCausticTransport3D_PopulateVolumeCache(&scene,
                                                              &cache,
                                                              &diagnostics));
    assert_true("runtime_caustic_transport_focused_analytic_cylinder_debug_records",
                RuntimeCausticTransportDebug3D_RecordCount() > 0u);
    path = RuntimeCausticTransportDebug3D_RecordAt(0u);
    assert_true("runtime_caustic_transport_focused_analytic_cylinder_debug_first_path",
                path != NULL);
    assert_true("runtime_caustic_transport_focused_analytic_cylinder_debug_policy",
                strcmp(path->emissionPolicy, "analytic_cylinder_lens_focused") == 0);
    assert_true("runtime_caustic_transport_focused_analytic_cylinder_debug_event",
                strcmp(path->eventType, "analytic_cylinder_lens") == 0);
    assert_true("runtime_caustic_transport_focused_analytic_cylinder_debug_lens_shape",
                strcmp(path->lensShapeKind, "cylinder") == 0);
    assert_true("runtime_caustic_transport_focused_analytic_cylinder_debug_lens_events",
                path->lensInterfaceEventCount == 2u);
    assert_true("runtime_caustic_transport_focused_analytic_cylinder_debug_volume",
                path->volumeDepositAcceptedCount > 0u);
    assert_true("runtime_caustic_transport_focused_analytic_cylinder_debug_count",
                diagnostics.analyticCylinderLensEmittedPathCount > 0u);

    RuntimeCausticVolumeCache3D_Free(&cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_analytic_prism_lens_populates_volume_cache(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticTransport3DDiagnostics diagnostics;

    test_caustic_transport_seed_material_state();
    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_transport_analytic_prism_scene",
                test_caustic_transport_make_analytic_cylinder_scene(&scene));
    test_caustic_transport_enable_analytic_prism_lens(16, true, false, false, NULL);

    assert_true("runtime_caustic_transport_analytic_prism_populate",
                RuntimeCausticTransport3D_PopulateVolumeCache(&scene,
                                                              &cache,
                                                              &diagnostics));
    assert_true("runtime_caustic_transport_analytic_prism_resolved",
                diagnostics.analyticPrismLensResolvedCount == 1u);
    assert_true("runtime_caustic_transport_analytic_prism_no_reject",
                diagnostics.analyticPrismLensRejectedCount == 0u);
    assert_true("runtime_caustic_transport_analytic_prism_evaluated",
                diagnostics.analyticPrismLensEvaluatedPathCount > 0u &&
                    diagnostics.analyticPrismLensEvaluatedPathCount <= 16u);
    assert_true("runtime_caustic_transport_analytic_prism_emit_count",
                diagnostics.analyticPrismLensEmittedPathCount > 0u);
    assert_true("runtime_caustic_transport_analytic_prism_only",
                diagnostics.evaluatedPathCount ==
                    diagnostics.analyticPrismLensEvaluatedPathCount);
    assert_true("runtime_caustic_transport_analytic_prism_cache",
                diagnostics.cache.nonZeroCellCount > 0u);
    assert_true("runtime_caustic_transport_analytic_prism_deposits",
                diagnostics.depositAcceptedCount > 0u);

    RuntimeCausticVolumeCache3D_Free(&cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_analytic_prism_lens_debug_export_records_policy(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticTransport3DDiagnostics diagnostics;
    const RuntimeCausticTransportDebugPath3D* path = NULL;
    const char* output_root = "/tmp/ray_tracing_caustic_transport_analytic_prism_debug_test";

    test_caustic_transport_seed_material_state();
    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_transport_analytic_prism_debug_scene",
                test_caustic_transport_make_analytic_cylinder_scene(&scene));
    assert_true("runtime_caustic_transport_analytic_prism_debug_output_root",
                config_io_ensure_directory_exists(output_root));
    test_caustic_transport_enable_analytic_prism_lens(16, true, false, true, output_root);

    assert_true("runtime_caustic_transport_analytic_prism_debug_populate",
                RuntimeCausticTransport3D_PopulateVolumeCache(&scene,
                                                              &cache,
                                                              &diagnostics));
    assert_true("runtime_caustic_transport_analytic_prism_debug_records",
                RuntimeCausticTransportDebug3D_RecordCount() > 0u);
    path = RuntimeCausticTransportDebug3D_RecordAt(0u);
    assert_true("runtime_caustic_transport_analytic_prism_debug_first_path", path != NULL);
    assert_true("runtime_caustic_transport_analytic_prism_debug_policy",
                strcmp(path->emissionPolicy, "analytic_prism_lens") == 0);
    assert_true("runtime_caustic_transport_analytic_prism_debug_event",
                strcmp(path->eventType, "analytic_prism_lens") == 0);
    assert_true("runtime_caustic_transport_analytic_prism_debug_lens_shape",
                strcmp(path->lensShapeKind, "prism") == 0);
    assert_true("runtime_caustic_transport_analytic_prism_debug_lens_events",
                path->lensInterfaceEventCount == 2u);
    assert_true("runtime_caustic_transport_analytic_prism_debug_lens_ior",
                path->lensEntryEtaFrom == 1.0 && path->lensEntryEtaTo > 1.0 &&
                    path->lensExitEtaFrom > 1.0 && path->lensExitEtaTo == 1.0);
    assert_true("runtime_caustic_transport_analytic_prism_debug_volume",
                path->volumeDepositAcceptedCount > 0u);

    RuntimeCausticVolumeCache3D_Free(&cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_analytic_bowl_lens_populates_volume_cache(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticTransport3DDiagnostics diagnostics;

    test_caustic_transport_seed_material_state();
    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_transport_analytic_bowl_scene",
                test_caustic_transport_make_analytic_bowl_scene(&scene));
    test_caustic_transport_enable_analytic_bowl_lens(16, true, false, false, NULL);

    assert_true("runtime_caustic_transport_analytic_bowl_populate",
                RuntimeCausticTransport3D_PopulateVolumeCache(&scene,
                                                              &cache,
                                                              &diagnostics));
    assert_true("runtime_caustic_transport_analytic_bowl_resolved",
                diagnostics.analyticBowlLensResolvedCount == 1u);
    assert_true("runtime_caustic_transport_analytic_bowl_no_reject",
                diagnostics.analyticBowlLensRejectedCount == 0u);
    assert_true("runtime_caustic_transport_analytic_bowl_evaluated",
                diagnostics.analyticBowlLensEvaluatedPathCount > 0u &&
                    diagnostics.analyticBowlLensEvaluatedPathCount <= 16u);
    assert_true("runtime_caustic_transport_analytic_bowl_emit_count",
                diagnostics.analyticBowlLensEmittedPathCount > 0u);
    assert_true("runtime_caustic_transport_analytic_bowl_only",
                diagnostics.evaluatedPathCount ==
                    diagnostics.analyticBowlLensEvaluatedPathCount);
    assert_true("runtime_caustic_transport_analytic_bowl_cache",
                diagnostics.cache.nonZeroCellCount > 0u);
    assert_true("runtime_caustic_transport_analytic_bowl_deposits",
                diagnostics.depositAcceptedCount > 0u);

    RuntimeCausticVolumeCache3D_Free(&cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_analytic_bowl_lens_debug_export_records_policy(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticTransport3DDiagnostics diagnostics;
    const RuntimeCausticTransportDebugPath3D* path = NULL;
    size_t record_count = 0u;
    const char* output_root = "/tmp/ray_tracing_caustic_transport_analytic_bowl_debug_test";

    test_caustic_transport_seed_material_state();
    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_transport_analytic_bowl_debug_scene",
                test_caustic_transport_make_analytic_bowl_scene(&scene));
    assert_true("runtime_caustic_transport_analytic_bowl_debug_output_root",
                config_io_ensure_directory_exists(output_root));
    test_caustic_transport_enable_analytic_bowl_lens(16, true, false, true, output_root);

    assert_true("runtime_caustic_transport_analytic_bowl_debug_populate",
                RuntimeCausticTransport3D_PopulateVolumeCache(&scene,
                                                              &cache,
                                                              &diagnostics));
    assert_true("runtime_caustic_transport_analytic_bowl_debug_records",
                RuntimeCausticTransportDebug3D_RecordCount() > 0u);
    record_count = RuntimeCausticTransportDebug3D_RecordCount();
    for (size_t record_i = 0u; record_i < record_count; ++record_i) {
        const RuntimeCausticTransportDebugPath3D* candidate =
            RuntimeCausticTransportDebug3D_RecordAt(record_i);
        if (candidate &&
            (fabs(candidate->lensEntryNormal.x) > 0.01 ||
             fabs(candidate->lensEntryNormal.y) > 0.01)) {
            path = candidate;
            break;
        }
    }
    assert_true("runtime_caustic_transport_analytic_bowl_debug_first_path", path != NULL);
    if (!path) {
        RuntimeCausticVolumeCache3D_Free(&cache);
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        RuntimeCausticTransport3D_ResetRequestState();
        return 0;
    }
    assert_true("runtime_caustic_transport_analytic_bowl_debug_policy",
                strcmp(path->emissionPolicy, "analytic_bowl_lens") == 0);
    assert_true("runtime_caustic_transport_analytic_bowl_debug_event",
                strcmp(path->eventType, "analytic_bowl_lens") == 0);
    assert_true("runtime_caustic_transport_analytic_bowl_debug_lens_shape",
                strcmp(path->lensShapeKind, "bowl") == 0);
    assert_true("runtime_caustic_transport_analytic_bowl_debug_lens_events",
                path->lensInterfaceEventCount == 2u);
    assert_true("runtime_caustic_transport_analytic_bowl_debug_lens_ior",
                path->lensEntryEtaFrom == 1.0 && path->lensEntryEtaTo > 1.0 &&
                    path->lensExitEtaFrom > 1.0 && path->lensExitEtaTo == 1.0);
    assert_true("runtime_caustic_transport_analytic_bowl_debug_curved_entry",
                fabs(path->lensEntryNormal.x) > 0.01 || fabs(path->lensEntryNormal.y) > 0.01);
    assert_true("runtime_caustic_transport_analytic_bowl_debug_flat_exit",
                fabs(path->lensExitNormal.x) < 1.0e-9 &&
                    fabs(path->lensExitNormal.y) < 1.0e-9);
    assert_true("runtime_caustic_transport_analytic_bowl_debug_volume",
                path->volumeDepositAcceptedCount > 0u);

    RuntimeCausticVolumeCache3D_Free(&cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

int run_test_runtime_caustic_transport_3d_tests(void) {
    int before = test_support_failures();

    test_runtime_caustic_transport_populates_volume_cache();
    test_runtime_caustic_transport_respects_sample_budget();
    test_runtime_caustic_transport_analytic_sphere_lens_populates_volume_cache();
    test_runtime_caustic_transport_analytic_sphere_lens_debug_export_records_policy();
    test_runtime_caustic_transport_analytic_sphere_lens_rejects_non_sphere();
    test_runtime_caustic_transport_analytic_cylinder_lens_populates_volume_cache();
    test_runtime_caustic_transport_analytic_cylinder_lens_debug_export_records_policy();
    test_runtime_caustic_transport_focused_analytic_cylinder_lens_debug_export_records_policy();
    test_runtime_caustic_transport_analytic_prism_lens_populates_volume_cache();
    test_runtime_caustic_transport_analytic_prism_lens_debug_export_records_policy();
    test_runtime_caustic_transport_analytic_bowl_lens_populates_volume_cache();
    test_runtime_caustic_transport_analytic_bowl_lens_debug_export_records_policy();
    test_runtime_caustic_transport_debug_export_disabled_records_nothing();
    test_runtime_caustic_transport_debug_export_records_path_geometry();
    test_runtime_caustic_transport_populates_surface_cache();
    test_runtime_caustic_transport_suppresses_volume_without_vf3d();
    test_runtime_caustic_transport_surface_calibration_scales_records();
    test_runtime_caustic_transport_surface_without_receiver_fallback();
    test_runtime_caustic_transport_render_samples_volume_cache();
    test_runtime_caustic_transport_render_samples_surface_cache();
    test_runtime_caustic_sidecar_uses_prepared_probe_snapshot();

    return test_support_failures() - before;
}
