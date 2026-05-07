#ifndef EDITOR_MATERIAL_EDITOR_LAYER_MODEL_H
#define EDITOR_MATERIAL_EDITOR_LAYER_MODEL_H

#include <stdbool.h>

#include "render/runtime_material_texture_stack_3d.h"
#include "scene/object_manager.h"

void MaterialEditorLayerModelReset(void);
int MaterialEditorLayerModelGetActiveIndex(const SceneObject* object, int scene_object_index);
bool MaterialEditorLayerModelSetActiveIndex(const SceneObject* object,
                                            int scene_object_index,
                                            int layer_index);
bool MaterialEditorLayerModelGetEffectiveStack(const SceneObject* object,
                                               int scene_object_index,
                                               RuntimeMaterialTextureStack* out_stack);
bool MaterialEditorLayerModelHasEditableStack(int scene_object_index);
bool MaterialEditorLayerModelEnsureEditableStack(const SceneObject* object,
                                                 int scene_object_index,
                                                 RuntimeMaterialTextureStack* out_stack);
bool MaterialEditorLayerModelSetStack(int scene_object_index,
                                      const RuntimeMaterialTextureStack* stack);
bool MaterialEditorLayerModelAddOverlay(const SceneObject* object, int scene_object_index);
bool MaterialEditorLayerModelDeleteActiveLayer(const SceneObject* object, int scene_object_index);
bool MaterialEditorLayerModelMoveActiveLayer(const SceneObject* object,
                                             int scene_object_index,
                                             int direction);
bool MaterialEditorLayerModelToggleActiveLayerEnabled(const SceneObject* object,
                                                      int scene_object_index);
bool MaterialEditorLayerModelApplyLayerKind(const SceneObject* object,
                                            int scene_object_index,
                                            RuntimeMaterialTextureLayerKind kind);
bool MaterialEditorLayerModelApplyLegacyTextureKind(const SceneObject* object,
                                                    int scene_object_index,
                                                    int texture_id);
bool MaterialEditorLayerModelApplyPlacementValue(const SceneObject* object,
                                                 int scene_object_index,
                                                 int field_kind,
                                                 double normalized_value);
bool MaterialEditorLayerModelApplyPatternMode(const SceneObject* object,
                                              int scene_object_index,
                                              int pattern_mode);
bool MaterialEditorLayerModelApplyParamValue(const SceneObject* object,
                                             int scene_object_index,
                                             int param_kind,
                                             double normalized_value);

#endif
