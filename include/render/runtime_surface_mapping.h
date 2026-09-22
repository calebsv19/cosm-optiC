#pragma once
#include <json-c/json.h>
#include "core_mesh_preview.h"
#include "core_authored_surface_mapping.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_material_texture_stack_3d.h"
#include "import/runtime_scene_bridge.h"

/* Prepared per-object binding. Missing binding is the unchanged legacy path. */
bool RuntimeSurfaceMappingValidateScene(json_object* root, char* diagnostic, size_t size);
bool RuntimeSurfaceMappingLoadScene(json_object* root, double world_scale);
bool RuntimeSurfaceMappingActive(int object_index);
unsigned long long RuntimeSurfaceMappingRevision(void);
bool RuntimeSurfaceMappingCoordinates(const HitInfo3D* hit, CoreAuthoredSurfaceCoordinates* out);
bool RuntimeSurfaceMappingEvaluate(const SceneObject* object, const HitInfo3D* hit,
    const RuntimeMaterialSurfaceEval* base, RuntimeMaterialSurfaceEval* out);
/* Common primitive viewport adapter: face island -> geometric sample -> payload.
 * The adapter uses the final renderer's face ordering, independent of preview triangles. */
bool RuntimeSurfaceMaterialSamplePrimitive(int object_index, int face,
    double island_u, double island_v, RuntimeMaterialSurfaceEval* out);
bool RuntimeSurfaceMaterialPrimitiveIsland(int object_index, Vec3 position,
    Vec3 outward_normal, int* face, double* u, double* v);
bool RuntimeSurfaceMaterialPrimitiveIslandForSeed(const RuntimeSceneBridgePrimitiveSeed* seed,
    Vec3 position, Vec3 outward_normal, int* face, double* u, double* v);

bool RuntimeSurfaceMappingDefinition(int index,CoreAuthoredSurfaceMapping* out);
bool RuntimeSurfaceMappingEvaluateTiles(int index,double u,double v,const RuntimeMaterialSurfaceEval* base,RuntimeMaterialSurfaceEval* out);
void RuntimeSurfaceMappingBlendPole(const RuntimeMaterialSurfaceEval* base,double weight,RuntimeMaterialSurfaceEval* out);

bool RuntimeSurfaceMappingEvaluateReferencedStack(const HitInfo3D* hit,const char* reference,
    const RuntimeMaterialTextureStack* stack,const RuntimeMaterialSurfaceEval* base,RuntimeMaterialSurfaceEval* out);

bool RuntimeSurfaceMaterialCompileSource(json_object* source,RuntimeMaterialTextureStack* out);

bool RuntimeSurfaceMappingPreviewSupported(int index);

bool RuntimeSurfaceMappingNeedsMeshAttributes(int index);
bool RuntimeSurfaceMaterialSampleMesh(int index,int asset_index,size_t triangle,
    const double barycentric[3],Vec3 world,Vec3 normal,const CoreMeshPreviewLodMesh* lod,
    RuntimeMaterialSurfaceEval* out);

/* Frame-local triangle preparation. No retained cache or heap allocation; a
 * runtime mapping revision change invalidates the prepared pointers. */
typedef struct RuntimeSurfaceMeshSamplePrepared {
    HitInfo3D hit;
    const CoreMeshAssetSurfaceCorner *corners;
    unsigned long long revision;
    bool valid;
} RuntimeSurfaceMeshSamplePrepared;
bool RuntimeSurfaceMaterialPrepareMeshTriangle(int index,int asset_index,size_t triangle,
    const CoreMeshPreviewLodMesh *lod,const Vec3 *dpdx,const Vec3 *dpdy,
    RuntimeSurfaceMeshSamplePrepared *prepared);
bool RuntimeSurfaceMaterialSamplePreparedMesh(RuntimeSurfaceMeshSamplePrepared *prepared,
    const double barycentric[3],Vec3 world,Vec3 normal,RuntimeMaterialSurfaceEval *out);

bool RuntimeSurfaceMaterialSampleMeshFootprint(int index,int asset_index,size_t triangle,
    const double barycentric[3],Vec3 world,Vec3 normal,const CoreMeshPreviewLodMesh *lod,
    const Vec3 *dpdx,const Vec3 *dpdy,RuntimeMaterialSurfaceEval *out);

/* Clear all prepared mappings, sampled resources and graph programs together. */
void RuntimeSurfaceMappingReset(void);
/* Deterministic one-shot failures for lifecycle verification; disabled by default. */
typedef enum RuntimeSurfacePreparationFailure {
    RUNTIME_SURFACE_FAIL_NONE, RUNTIME_SURFACE_FAIL_ALLOCATION,
    RUNTIME_SURFACE_FAIL_IMAGE_READ, RUNTIME_SURFACE_FAIL_IMAGE_DECODE,
    RUNTIME_SURFACE_FAIL_PYRAMID_ALLOCATION
} RuntimeSurfacePreparationFailure;
void RuntimeSurfacePreparationFailNextForTests(RuntimeSurfacePreparationFailure failure);
