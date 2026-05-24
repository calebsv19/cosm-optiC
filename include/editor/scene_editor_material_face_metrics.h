#ifndef SCENE_EDITOR_MATERIAL_FACE_METRICS_H
#define SCENE_EDITOR_MATERIAL_FACE_METRICS_H

#include <stdbool.h>

typedef struct SceneEditorMaterialFaceMetrics {
    bool valid;
    double width;
    double height;
    bool swapAxes;
    bool flipU;
    bool flipV;
} SceneEditorMaterialFaceMetrics;

bool SceneEditorMaterialFaceMetricsResolve(int primitive_index,
                                           int scene_object_index,
                                           int face_group_index,
                                           SceneEditorMaterialFaceMetrics* out_metrics);

bool SceneEditorMaterialFaceMetricsGroundUV(
    const SceneEditorMaterialFaceMetrics* metrics,
    double face_u,
    double face_v,
    double* out_u,
    double* out_v);

bool SceneEditorMaterialFaceMetricsResolveGroundedUV(
    int primitive_index,
    int scene_object_index,
    int face_group_index,
    double face_u,
    double face_v,
    double* out_u,
    double* out_v);

#endif
