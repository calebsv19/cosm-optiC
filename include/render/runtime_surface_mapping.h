#pragma once
#include <json-c/json.h>
#include "core_authored_surface_mapping.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_material_texture_stack_3d.h"
#include "import/runtime_scene_bridge.h"

/* Prepared per-object binding. Missing binding is the unchanged legacy path. */
bool RuntimeSurfaceMappingValidateScene(json_object* root, char* diagnostic, size_t size);
void RuntimeSurfaceMappingLoadScene(json_object* root, double world_scale);
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
