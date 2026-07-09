#ifndef SCENE_EDITOR_MATERIAL_PREVIEW_INTERNAL_H
#define SCENE_EDITOR_MATERIAL_PREVIEW_INTERNAL_H

#include "editor/scene_editor_material_preview.h"
#include "import/runtime_scene_bridge.h"
#include "render/runtime_scene_3d.h"

typedef struct SceneEditorMaterialPreviewProjectedTriangle {
    int x0;
    int y0;
    int x1;
    int y1;
    int x2;
    int y2;
    double depth;
    double depth0;
    double depth1;
    double depth2;
    SceneEditorMaterialPreviewTriangleAddress address;
} SceneEditorMaterialPreviewProjectedTriangle;

#define SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TEXTURE_BLOCKS 4096
#define SCENE_EDITOR_MATERIAL_PREVIEW_MAX_SAMPLE_STEP 16

double scene_editor_material_preview_view_depth(
    const SceneEditorDigestOverlayProjector* projector,
    double world_x,
    double world_y,
    double world_z);
int scene_editor_material_preview_face_group_for_triangle(int local_triangle_index);
void scene_editor_material_preview_fill_address(
    SceneEditorMaterialPreviewTriangleAddress* out_address,
    int focused_object_index,
    int primitive_index,
    int triangle_index,
    int local_triangle_index);
bool scene_editor_material_preview_build_scene(RuntimeScene3D* scene);
int scene_editor_material_preview_compare_depth_desc(const void* lhs, const void* rhs);
int scene_editor_material_preview_build_projected_triangles(
    RuntimeScene3D* scene,
    int focused_object_index,
    const SceneEditorDigestOverlayProjector* projector,
    SceneEditorMaterialPreviewProjectedTriangle* projected,
    int projected_capacity);
bool scene_editor_material_preview_barycentric_at_point(
    const SceneEditorMaterialPreviewProjectedTriangle* triangle,
    double px,
    double py,
    double* out_u,
    double* out_v,
    double* out_w);

#endif
