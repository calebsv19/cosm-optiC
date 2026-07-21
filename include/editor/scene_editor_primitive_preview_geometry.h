#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "import/runtime_scene_bridge.h"

#define SCENE_EDITOR_PRIMITIVE_PREVIEW_MAX_TRIANGLES 12u

typedef struct SceneEditorPrimitivePreviewPoint3 {
    double x;
    double y;
    double z;
} SceneEditorPrimitivePreviewPoint3;

typedef struct SceneEditorPrimitivePreviewTriangle {
    SceneEditorPrimitivePreviewPoint3 a;
    SceneEditorPrimitivePreviewPoint3 b;
    SceneEditorPrimitivePreviewPoint3 c;
} SceneEditorPrimitivePreviewTriangle;

bool SceneEditorPrimitivePreviewBuildTriangles(
    const RuntimeSceneBridgePrimitiveSeed* primitive,
    SceneEditorPrimitivePreviewTriangle
        out_triangles[SCENE_EDITOR_PRIMITIVE_PREVIEW_MAX_TRIANGLES],
    size_t* out_triangle_count);
