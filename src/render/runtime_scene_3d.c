#include "render/runtime_scene_3d.h"

#include <stdlib.h>
#include <string.h>

const char* RuntimePrimitive3DKindLabel(RuntimePrimitive3DKind kind) {
    switch (kind) {
        case RUNTIME_PRIMITIVE_3D_KIND_PLANE:
            return "plane";
        case RUNTIME_PRIMITIVE_3D_KIND_RECT_PRISM:
            return "rect_prism";
        case RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH:
            return "triangle_mesh";
        case RUNTIME_PRIMITIVE_3D_KIND_INVALID:
        default:
            return "invalid";
    }
}

bool RuntimePrimitive3DKindSupportedByR0(RuntimePrimitive3DKind kind) {
    return kind == RUNTIME_PRIMITIVE_3D_KIND_PLANE ||
           kind == RUNTIME_PRIMITIVE_3D_KIND_RECT_PRISM;
}

void RuntimeTriangleMesh3D_Init(RuntimeTriangleMesh3D* mesh) {
    if (!mesh) return;
    memset(mesh, 0, sizeof(*mesh));
}

void RuntimeTriangleMesh3D_Free(RuntimeTriangleMesh3D* mesh) {
    if (!mesh) return;
    free(mesh->triangles);
    mesh->triangles = NULL;
    mesh->triangleCount = 0;
    mesh->triangleCapacity = 0;
}

void RuntimeScene3D_Init(RuntimeScene3D* scene) {
    if (!scene) return;
    memset(scene, 0, sizeof(*scene));

    scene->scope.planeEnabled = true;
    scene->scope.rectPrismEnabled = true;
    scene->scope.triangleMeshEnabled = false;

    scene->ownership.rendererOwnsGeometryTruth = true;
    scene->ownership.sceneObjectsRemainCompatOnly = true;
    scene->ownership.previewDigestIsNonAuthoritative = true;

    scene->light.falloffMode = FORWARD_FALLOFF_MODE_QUADRATIC;
    scene->camera.zoom = 1.0;
    scene->camera.nearPlane = 0.1;

    RuntimeTriangleMesh3D_Init(&scene->triangleMesh);
}

void RuntimeScene3D_Reset(RuntimeScene3D* scene) {
    RuntimeScene3DPrimitiveScope scope = {0};
    RuntimeScene3DOwnershipContract ownership = {0};
    if (!scene) return;

    scope = scene->scope;
    ownership = scene->ownership;
    RuntimeScene3D_Free(scene);
    RuntimeScene3D_Init(scene);
    scene->scope = scope;
    scene->ownership = ownership;
}

void RuntimeScene3D_Free(RuntimeScene3D* scene) {
    if (!scene) return;
    free(scene->primitives);
    scene->primitives = NULL;
    scene->primitiveCount = 0;
    scene->primitiveCapacity = 0;
    RuntimeTriangleMesh3D_Free(&scene->triangleMesh);
    scene->hasLight = false;
    scene->hasCamera = false;
}
