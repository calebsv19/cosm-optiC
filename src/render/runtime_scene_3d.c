#include "render/runtime_scene_3d.h"
#include "render/runtime_triangle_bvh_3d.h"

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
    RuntimeTriangleMesh3D_ClearBVH(mesh);
    free(mesh->triangles);
    mesh->triangles = NULL;
    mesh->triangleCount = 0;
    mesh->triangleCapacity = 0;
    mesh->bvhDirty = false;
}

bool RuntimeTriangleMesh3D_CopyFrom(RuntimeTriangleMesh3D* dst,
                                    const RuntimeTriangleMesh3D* src) {
    if (!dst || !src) return false;

    RuntimeTriangleMesh3D_Free(dst);
    RuntimeTriangleMesh3D_Init(dst);
    if (src->triangleCount > 0) {
        if (!src->triangles) {
            RuntimeTriangleMesh3D_Free(dst);
            return false;
        }
        dst->triangles = (RuntimeTriangle3D*)malloc(sizeof(*dst->triangles) *
                                                    (size_t)src->triangleCount);
        if (!dst->triangles) {
            RuntimeTriangleMesh3D_Free(dst);
            return false;
        }
        memcpy(dst->triangles,
               src->triangles,
               sizeof(*dst->triangles) * (size_t)src->triangleCount);
        dst->triangleCount = src->triangleCount;
        dst->triangleCapacity = src->triangleCount;
    }
    dst->bvhDirty = src->bvhDirty;
    if (!RuntimeTriangleMesh3D_CopyBVH(dst, src)) {
        RuntimeTriangleMesh3D_Free(dst);
        return false;
    }
    return true;
}

void RuntimeScene3D_Init(RuntimeScene3D* scene) {
    if (!scene) return;
    memset(scene, 0, sizeof(*scene));

    scene->scope.planeEnabled = true;
    scene->scope.rectPrismEnabled = true;
    scene->scope.triangleMeshEnabled = false;

    scene->ownership.rendererOwnsGeometryTruth = true;
    scene->ownership.rendererOwnsVolumeAttachmentTruth = true;
    scene->ownership.sceneObjectsRemainCompatOnly = true;
    scene->ownership.previewDigestIsNonAuthoritative = true;
    scene->ownership.volumeAttachmentIsOptional = true;
    scene->ownership.geometryAndVolumeSourcesRemainSeparate = true;
    scene->ownership.legacyPlanarFluidOverlayRemainsSeparate = true;

    RuntimeEnvironment3D_Init(&scene->environment);

    scene->light.falloffMode = FORWARD_FALLOFF_MODE_QUADRATIC;
    scene->camera.zoom = 1.0;
    scene->camera.nearPlane = 0.1;

    RuntimeTriangleMesh3D_Init(&scene->triangleMesh);
    RuntimeEmissiveLightSet3D_Init(&scene->emissiveLightSet);
    RuntimeVolumeAttachment3D_Init(&scene->volume);
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
    RuntimeEmissiveLightSet3D_Free(&scene->emissiveLightSet);
    RuntimeVolumeAttachment3D_Free(&scene->volume);
    scene->hasLight = false;
    scene->hasCamera = false;
}

bool RuntimeScene3D_CopyGeometryFrom(RuntimeScene3D* dst, const RuntimeScene3D* src) {
    if (!dst || !src) return false;

    RuntimeScene3D_Free(dst);
    RuntimeScene3D_Init(dst);
    dst->scope = src->scope;
    dst->ownership = src->ownership;
    dst->environment = src->environment;
    dst->light = src->light;
    dst->hasLight = src->hasLight;
    dst->camera = src->camera;
    dst->hasCamera = src->hasCamera;
    dst->materialFlags = src->materialFlags;
    dst->capabilities = src->capabilities;

    if (src->primitiveCount > 0) {
        dst->primitives = (RuntimePrimitive3D*)malloc(sizeof(*dst->primitives) *
                                                      (size_t)src->primitiveCount);
        if (!dst->primitives) {
            RuntimeScene3D_Free(dst);
            RuntimeScene3D_Init(dst);
            return false;
        }
        memcpy(dst->primitives,
               src->primitives,
               sizeof(*dst->primitives) * (size_t)src->primitiveCount);
        dst->primitiveCount = src->primitiveCount;
        dst->primitiveCapacity = src->primitiveCount;
    }

    if (!RuntimeTriangleMesh3D_CopyFrom(&dst->triangleMesh, &src->triangleMesh)) {
        RuntimeScene3D_Free(dst);
        RuntimeScene3D_Init(dst);
        return false;
    }
    if (!RuntimeEmissiveLightSet3D_CopyFrom(&dst->emissiveLightSet,
                                            &src->emissiveLightSet)) {
        RuntimeScene3D_Free(dst);
        RuntimeScene3D_Init(dst);
        return false;
    }
    return true;
}
