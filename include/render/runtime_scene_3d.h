#ifndef RENDER_RUNTIME_SCENE_3D_H
#define RENDER_RUNTIME_SCENE_3D_H

#include <stdbool.h>

#include "config/config_manager.h"
#include "math/vec3.h"
#include "render/runtime_volume_3d.h"

#define RUNTIME_SCENE_3D_MAX_OBJECT_ID 64

typedef struct RuntimeTriangleBVH3D RuntimeTriangleBVH3D;

typedef enum {
    RUNTIME_PRIMITIVE_3D_KIND_INVALID = 0,
    RUNTIME_PRIMITIVE_3D_KIND_PLANE = 1,
    RUNTIME_PRIMITIVE_3D_KIND_RECT_PRISM = 2,
    RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH = 3
} RuntimePrimitive3DKind;

typedef struct {
    bool planeEnabled;
    bool rectPrismEnabled;
    bool triangleMeshEnabled;
} RuntimeScene3DPrimitiveScope;

typedef struct {
    bool rendererOwnsGeometryTruth;
    bool rendererOwnsVolumeAttachmentTruth;
    bool sceneObjectsRemainCompatOnly;
    bool previewDigestIsNonAuthoritative;
    bool volumeAttachmentIsOptional;
    bool geometryAndVolumeSourcesRemainSeparate;
    bool legacyPlanarFluidOverlayRemainsSeparate;
} RuntimeScene3DOwnershipContract;

typedef struct {
    char objectId[RUNTIME_SCENE_3D_MAX_OBJECT_ID];
    int sceneObjectIndex;
    RuntimePrimitive3DKind kind;
} RuntimePrimitive3DSourceRef;

typedef struct {
    Vec3 origin;
    Vec3 axisU;
    Vec3 axisV;
    Vec3 normal;
    double width;
    double height;
} RuntimePlane3D;

typedef struct {
    Vec3 origin;
    Vec3 axisU;
    Vec3 axisV;
    Vec3 normal;
    double width;
    double height;
    double depth;
} RuntimeRectPrism3D;

typedef struct {
    RuntimePrimitive3DSourceRef source;
    RuntimePrimitive3DKind kind;
    union {
        RuntimePlane3D plane;
        RuntimeRectPrism3D rectPrism;
    } shape;
} RuntimePrimitive3D;

typedef struct {
    Vec3 p0;
    Vec3 p1;
    Vec3 p2;
    Vec3 normal;
    bool twoSided;
    bool hasObjectTextureCoords;
    Vec3 objectTexture0;
    Vec3 objectTexture1;
    Vec3 objectTexture2;
    int primitiveIndex;
    int sceneObjectIndex;
    int localTriangleIndex;
} RuntimeTriangle3D;

typedef struct {
    RuntimeTriangle3D* triangles;
    int triangleCount;
    int triangleCapacity;
    RuntimeTriangleBVH3D* bvh;
    bool bvhDirty;
} RuntimeTriangleMesh3D;

typedef struct {
    Vec3 position;
    double radius;
    double intensity;
    double falloffDistance;
    ForwardFalloffMode falloffMode;
} RuntimeLight3D;

typedef struct {
    EnvironmentLightMode lightMode;
    double ambientIntensity;
    double topFillIntensity;
    Vec3 ambientColor;
    Vec3 backgroundTopColor;
    Vec3 backgroundBottomColor;
    double topDownBias;
} RuntimeEnvironment3D;

typedef struct {
    Vec3 position;
    double rotation;
    double lookPitch;
    double zoom;
    double nearPlane;
} RuntimeCamera3D;

typedef struct {
    bool valid;
    bool hasTransparentSurfaces;
    bool hasEmissiveSurfaces;
    bool hasUnresolvedSurfaces;
} RuntimeScene3DMaterialFlags;

typedef struct {
    bool valid;
    bool hasTransparentSurfaces;
    bool hasEmissiveSurfaces;
    bool hasUnresolvedSurfaces;
    bool hasSpecularSurfaces;
    bool hasTransmissionSurfaces;
    bool hasLightingExtinctionVolume;
    bool hasLightingScatterVolume;
    bool canUseOpaqueNoVolumeVisibilityFastPath;
    bool canSkipEmissionSupport;
    bool canSkipTransparencySupport;
    bool canSkipVolumeScatter;
} RuntimeScene3DCapabilities;

typedef struct {
    RuntimeScene3DPrimitiveScope scope;
    RuntimeScene3DOwnershipContract ownership;
    RuntimePrimitive3D* primitives;
    int primitiveCount;
    int primitiveCapacity;
    RuntimeTriangleMesh3D triangleMesh;
    RuntimeVolumeAttachment3D volume;
    RuntimeEnvironment3D environment;
    RuntimeLight3D light;
    bool hasLight;
    RuntimeCamera3D camera;
    bool hasCamera;
    RuntimeScene3DMaterialFlags materialFlags;
    RuntimeScene3DCapabilities capabilities;
} RuntimeScene3D;

const char* RuntimePrimitive3DKindLabel(RuntimePrimitive3DKind kind);
bool RuntimePrimitive3DKindSupportedByR0(RuntimePrimitive3DKind kind);

void RuntimeTriangleMesh3D_Init(RuntimeTriangleMesh3D* mesh);
void RuntimeTriangleMesh3D_Free(RuntimeTriangleMesh3D* mesh);
bool RuntimeTriangleMesh3D_CopyFrom(RuntimeTriangleMesh3D* dst,
                                    const RuntimeTriangleMesh3D* src);

void RuntimeScene3D_Init(RuntimeScene3D* scene);
void RuntimeScene3D_Reset(RuntimeScene3D* scene);
void RuntimeScene3D_Free(RuntimeScene3D* scene);
bool RuntimeScene3D_CopyGeometryFrom(RuntimeScene3D* dst, const RuntimeScene3D* src);
void RuntimeScene3D_RefreshCapabilities(RuntimeScene3D* scene);
void RuntimeScene3D_RefreshMaterialFlags(RuntimeScene3D* scene);

#endif
