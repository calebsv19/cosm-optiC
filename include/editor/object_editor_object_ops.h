#ifndef OBJECT_EDITOR_OBJECT_OPS_H
#define OBJECT_EDITOR_OBJECT_OPS_H

#include "scene/object_manager.h"

void ObjectEditorObjectAssignMaterial(SceneObject* obj, int material_id);
void ObjectEditorObjectAssignColor(SceneObject* obj, int packed_color);
void ObjectEditorObjectAssignAlpha(SceneObject* obj, double alpha);
void ObjectEditorObjectAssignEmissiveStrength(SceneObject* obj, double emissive_strength);

#endif
