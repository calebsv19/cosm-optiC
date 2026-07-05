#ifndef MATERIAL_EDITOR_H
#define MATERIAL_EDITOR_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "editor/scene_editor_digest_overlay.h"
#include "editor/scene_editor_material_preview.h"
#include "render/runtime_material_texture_stack_3d.h"

typedef enum MaterialEditorHitRegion {
    MATERIAL_EDITOR_HIT_NONE = 0,
    MATERIAL_EDITOR_HIT_CONTROLS,
    MATERIAL_EDITOR_HIT_LIST_PANEL,
    MATERIAL_EDITOR_HIT_CANVAS
} MaterialEditorHitRegion;

typedef enum MaterialEditorSliderKind {
    MATERIAL_EDITOR_SLIDER_NONE = 0,
    MATERIAL_EDITOR_SLIDER_STRENGTH,
    MATERIAL_EDITOR_SLIDER_SCALE,
    MATERIAL_EDITOR_SLIDER_OFFSET_U,
    MATERIAL_EDITOR_SLIDER_OFFSET_V
} MaterialEditorSliderKind;

typedef enum MaterialEditorTextureParamKind {
    MATERIAL_EDITOR_TEXTURE_PARAM_NONE = 0,
    MATERIAL_EDITOR_TEXTURE_PARAM_COVERAGE,
    MATERIAL_EDITOR_TEXTURE_PARAM_GRAIN,
    MATERIAL_EDITOR_TEXTURE_PARAM_EDGE_SOFTNESS,
    MATERIAL_EDITOR_TEXTURE_PARAM_CONTRAST,
    MATERIAL_EDITOR_TEXTURE_PARAM_FLOW,
    MATERIAL_EDITOR_TEXTURE_PARAM_COLOR_DEPTH,
    MATERIAL_EDITOR_TEXTURE_PARAM_SURFACE_DAMAGE
} MaterialEditorTextureParamKind;

typedef enum MaterialEditorResponseField {
    MATERIAL_EDITOR_RESPONSE_FIELD_NONE = 0,
    MATERIAL_EDITOR_RESPONSE_FIELD_TRANSMISSION,
    MATERIAL_EDITOR_RESPONSE_FIELD_ROUGHNESS,
    MATERIAL_EDITOR_RESPONSE_FIELD_IOR,
    MATERIAL_EDITOR_RESPONSE_FIELD_REFLECTIVITY,
    MATERIAL_EDITOR_RESPONSE_FIELD_SPECULAR,
    MATERIAL_EDITOR_RESPONSE_FIELD_TINT,
    MATERIAL_EDITOR_RESPONSE_FIELD_ABSORPTION,
    MATERIAL_EDITOR_RESPONSE_FIELD_THIN_WALLED,
    MATERIAL_EDITOR_RESPONSE_FIELD_MIRROR_DOMINANCE,
    MATERIAL_EDITOR_RESPONSE_FIELD_MIRROR_BASE,
    MATERIAL_EDITOR_RESPONSE_FIELD_METALLIC,
    MATERIAL_EDITOR_RESPONSE_FIELD_DIFFUSE_BASE
} MaterialEditorResponseField;

typedef enum MaterialEditorLayerInfluenceKind {
    MATERIAL_EDITOR_LAYER_INFLUENCE_NONE = 0,
    MATERIAL_EDITOR_LAYER_INFLUENCE_ROUGHNESS,
    MATERIAL_EDITOR_LAYER_INFLUENCE_REFLECTIVITY,
    MATERIAL_EDITOR_LAYER_INFLUENCE_SPECULAR,
    MATERIAL_EDITOR_LAYER_INFLUENCE_DIFFUSE,
    MATERIAL_EDITOR_LAYER_INFLUENCE_TRANSPARENCY
} MaterialEditorLayerInfluenceKind;

typedef enum MaterialEditorViewMode {
    MATERIAL_EDITOR_VIEW_FOCUSED_ORIGIN = 0,
    MATERIAL_EDITOR_VIEW_SCENE_PLACEMENT
} MaterialEditorViewMode;

void InitializeMaterialEditor(void);
void RenderMaterialEditor(SDL_Renderer* renderer);
int MaterialEditorRenderPaneControls(SDL_Renderer* renderer,
                                     SDL_Rect content_bounds,
                                     int top_y,
                                     int bottom_y);
int MaterialEditorRenderRightPanePreview(SDL_Renderer* renderer,
                                         SDL_Rect content_bounds,
                                         int top_y,
                                         int bottom_y);
void HandleMaterialEditorEvents(SDL_Event* event);
MaterialEditorHitRegion MaterialEditorHitRegionAtPoint(int mx, int my);
bool MaterialEditorHandleCanvasPointerDown(const SceneEditorDigestOverlayProjector* projector,
                                           int mx,
                                           int my,
                                           bool additive);

int MaterialEditorResolveFocusedObjectIndex(void);
void MaterialEditorSetFocusedObjectIndex(int index);
void MaterialEditorClearTriangleSelection(void);
int MaterialEditorSelectedTriangleCount(void);
int MaterialEditorSelectedFaceGroupCount(void);
int MaterialEditorFocusedFaceGroupCount(void);
int MaterialEditorGetActiveFaceGroupIndex(void);
bool MaterialEditorSetActiveFaceGroupIndex(int face_group_index);
bool MaterialEditorSetFaceGroupSelectionByIndex(int face_group_index);
int MaterialEditorGetActiveLayerIndex(void);
bool MaterialEditorSetActiveLayerIndex(int layer_index);
int MaterialEditorFocusedLayerCount(void);
bool MaterialEditorAddOverlayLayerToFocused(void);
bool MaterialEditorDeleteActiveLayer(void);
bool MaterialEditorMoveActiveLayer(int direction);
bool MaterialEditorToggleActiveLayerEnabled(void);
bool MaterialEditorApplyLayerKindToFocused(RuntimeMaterialTextureLayerKind kind);
bool MaterialEditorGetSelectedTriangle(int index, SceneEditorMaterialPreviewTriangleAddress* out_address);
bool MaterialEditorGetActiveFaceAddress(SceneEditorMaterialPreviewTriangleAddress* out_address);
bool MaterialEditorSetTriangleSelection(const SceneEditorMaterialPreviewTriangleAddress* address);
bool MaterialEditorToggleTriangleSelection(const SceneEditorMaterialPreviewTriangleAddress* address);
bool MaterialEditorSetFaceGroupSelection(const SceneEditorMaterialPreviewTriangleAddress* address);
bool MaterialEditorToggleFaceGroupSelection(const SceneEditorMaterialPreviewTriangleAddress* address);
MaterialEditorViewMode MaterialEditorGetViewMode(void);
void MaterialEditorSetViewMode(MaterialEditorViewMode mode);
bool MaterialEditorGetSolidFacesEnabled(void);
void MaterialEditorSetSolidFacesEnabled(bool enabled);
bool MaterialEditorToggleSolidFaces(void);
bool MaterialEditorApplyTextureKindToFocused(int texture_id);
bool MaterialEditorApplySliderValueToFocused(MaterialEditorSliderKind kind, double value);
bool MaterialEditorApplyTexturePatternToFocused(int pattern_mode);
bool MaterialEditorApplyTextureParamValueToFocused(MaterialEditorTextureParamKind kind, double value);
bool MaterialEditorApplyLayerInfluenceValueToFocused(MaterialEditorLayerInfluenceKind kind,
                                                     double value);
bool MaterialEditorApplyLayerInfluenceStepToFocused(MaterialEditorLayerInfluenceKind kind,
                                                    double delta);
bool MaterialEditorApplyResponseValueToFocused(MaterialEditorResponseField field, double value);
bool MaterialEditorApplyResponseStepToFocused(MaterialEditorResponseField field, double delta);
bool MaterialEditorApplyResponseTintToFocused(int packed_color);
bool MaterialEditorApplyGlassOverlayForFocused(RuntimeMaterialTextureLayerKind kind);
bool MaterialEditorResetActiveFacePlacement(void);
bool MaterialEditorCopyActiveFacePlacementToSelected(void);
bool MaterialEditorBindAuthoredTextureManifestForFocused(const char* manifest_path);
bool MaterialEditorClearAuthoredTextureBindingForFocused(void);
bool MaterialEditorGetAuthoredTextureBindingSummary(char* out_manifest_path,
                                                    size_t out_manifest_path_size,
                                                    char* out_binding_mode,
                                                    size_t out_binding_mode_size,
                                                    int* out_face_count);
bool MaterialEditorGetAuthoredTextureInvalidSummary(char* out_manifest_path,
                                                    size_t out_manifest_path_size,
                                                    char* out_binding_mode,
                                                    size_t out_binding_mode_size,
                                                    char* out_reason,
                                                    size_t out_reason_size);

#endif
