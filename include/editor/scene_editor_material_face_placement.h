#ifndef SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_H
#define SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_H

#include <stdbool.h>

#include "render/runtime_material_texture_stack_3d.h"
#include "scene/object_manager.h"

typedef enum SceneEditorMaterialFacePlacementField {
    SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_STRENGTH = 0,
    SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_SCALE,
    SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_OFFSET_U,
    SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_OFFSET_V,
    SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_ROTATION
} SceneEditorMaterialFacePlacementField;

typedef enum SceneEditorMaterialTextureParamField {
    SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_PATTERN = 0,
    SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_COVERAGE,
    SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_GRAIN,
    SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_EDGE_SOFTNESS,
    SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_CONTRAST,
    SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_FLOW,
    SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_COLOR_DEPTH,
    SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_SURFACE_DAMAGE,
    SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_SEED
} SceneEditorMaterialTextureParamField;

typedef struct SceneEditorMaterialFacePlacement {
    bool hasOverride;
    int sceneObjectIndex;
    int faceGroupIndex;
    int layerIndex;
    char layerId[RUNTIME_MATERIAL_TEXTURE_LAYER_ID_SIZE];
    int textureId;
    double offsetU;
    double offsetV;
    double scale;
    double strength;
    double rotation;
    RuntimeMaterialTexture3DParams params;
} SceneEditorMaterialFacePlacement;

void SceneEditorMaterialFacePlacementResetAll(void);
void SceneEditorMaterialFacePlacementResetObject(int scene_object_index);
bool SceneEditorMaterialFacePlacementResetFace(int scene_object_index, int face_group_index);
bool SceneEditorMaterialFacePlacementResetFaceLayer(int scene_object_index,
                                                    int face_group_index,
                                                    const char* layer_id);
bool SceneEditorMaterialFacePlacementHasOverride(int scene_object_index, int face_group_index);
bool SceneEditorMaterialFacePlacementHasOverrideForLayer(int scene_object_index,
                                                         int face_group_index,
                                                         const char* layer_id);
bool SceneEditorMaterialFacePlacementObjectHasActiveTextureOverride(int scene_object_index);
bool SceneEditorMaterialFacePlacementSetOverride(
    const SceneEditorMaterialFacePlacement* placement);
int SceneEditorMaterialFacePlacementOverrideCountForObject(int scene_object_index);
bool SceneEditorMaterialFacePlacementGetOverrideForObject(
    int scene_object_index,
    int ordinal,
    SceneEditorMaterialFacePlacement* out_placement);
SceneEditorMaterialFacePlacement SceneEditorMaterialFacePlacementGetEffective(
    const SceneObject* object,
    int scene_object_index,
    int face_group_index);
SceneEditorMaterialFacePlacement SceneEditorMaterialFacePlacementGetEffectiveForLayer(
    const SceneObject* object,
    int scene_object_index,
    int face_group_index,
    const char* layer_id);
bool SceneEditorMaterialFacePlacementApplyOverridesToStack(const SceneObject* object,
                                                           int scene_object_index,
                                                           int face_group_index,
                                                           RuntimeMaterialTextureStack* stack);
bool SceneEditorMaterialFacePlacementApplyNormalizedValue(const SceneObject* object,
                                                         int scene_object_index,
                                                         int face_group_index,
                                                         SceneEditorMaterialFacePlacementField field,
                                                         double normalized_value);
bool SceneEditorMaterialFacePlacementApplyTextureKind(const SceneObject* object,
                                                      int scene_object_index,
                                                      int face_group_index,
                                                      int texture_id);
bool SceneEditorMaterialFacePlacementApplyTextureParamNormalizedValue(
    const SceneObject* object,
    int scene_object_index,
    int face_group_index,
    SceneEditorMaterialTextureParamField field,
    double normalized_value);
bool SceneEditorMaterialFacePlacementApplyTextureParamPatternMode(
    const SceneObject* object,
    int scene_object_index,
    int face_group_index,
    int pattern_mode);
double SceneEditorMaterialFacePlacementGetNormalizedValue(
    const SceneObject* object,
    int scene_object_index,
    int face_group_index,
    SceneEditorMaterialFacePlacementField field);
double SceneEditorMaterialFacePlacementGetTextureParamNormalizedValue(
    const SceneObject* object,
    int scene_object_index,
    int face_group_index,
    SceneEditorMaterialTextureParamField field);
void SceneEditorMaterialFacePlacementResolveIslandUV(int local_triangle_index,
                                                    double bary_u,
                                                    double bary_v,
                                                    double bary_w,
                                                    double* out_u,
                                                    double* out_v);
RuntimeMaterialTexture3DPlacement SceneEditorMaterialFacePlacementToRuntime(
    const SceneEditorMaterialFacePlacement* placement);

#endif
