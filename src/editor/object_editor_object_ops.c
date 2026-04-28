#include "editor/object_editor_object_ops.h"

#include <math.h>

#include "material/material_manager.h"

static double object_editor_object_ops_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

void ObjectEditorObjectAssignMaterial(SceneObject* obj, int material_id) {
    if (!obj) return;
    if (material_id < 0 || material_id >= MaterialManagerCount()) return;
    obj->material_id = material_id;
    MarkObjectDirty(obj);
}

void ObjectEditorObjectAssignColor(SceneObject* obj, int packed_color) {
    if (!obj) return;
    obj->color = packed_color & 0xFFFFFF;
    MarkObjectDirty(obj);
}

void ObjectEditorObjectAssignTransparency(SceneObject* obj, double transparency) {
    if (!obj) return;
    obj->transparency = object_editor_object_ops_clamp01(transparency);
    MarkObjectDirty(obj);
}

void ObjectEditorObjectAssignEmissiveStrength(SceneObject* obj, double emissive_strength) {
    if (!obj) return;
    obj->emissiveStrength = object_editor_object_ops_clamp01(emissive_strength);
    MarkObjectDirty(obj);
}
