#ifndef APP_MATERIAL_PREVIEW_REQUEST_H
#define APP_MATERIAL_PREVIEW_REQUEST_H

#include <stdbool.h>
#include <stddef.h>

#define MATERIAL_PREVIEW_REQUEST_SCHEMA "ray_tracing_material_preview_request_v1"
#define MATERIAL_PREVIEW_MAX_PATH 1024
#define MATERIAL_PREVIEW_MAX_OBJECT_ID 128
#define MATERIAL_PREVIEW_MAX_VARIANT_LABEL 64
#define MATERIAL_PREVIEW_MAX_VARIANTS 8
#define MATERIAL_PREVIEW_MAX_LAYER_KIND_ID 32

typedef struct MaterialPreviewVariantOverrides {
    char label[MATERIAL_PREVIEW_MAX_VARIANT_LABEL];
    bool has_alpha;
    double alpha;
    bool has_reflectivity;
    double reflectivity;
    bool has_roughness;
    double roughness;
    bool has_emissive_strength;
    double emissive_strength;
    bool has_texture_id;
    int texture_id;
    bool has_texture_strength;
    double texture_strength;
    bool has_texture_scale;
    double texture_scale;
    bool has_texture_offset_u;
    double texture_offset_u;
    bool has_texture_offset_v;
    double texture_offset_v;
    bool has_texture_seed;
    int texture_seed;
    bool has_texture_pattern_mode;
    int texture_pattern_mode;
    bool has_texture_coverage;
    double texture_coverage;
    bool has_texture_grain;
    double texture_grain;
    bool has_texture_edge_softness;
    double texture_edge_softness;
    bool has_texture_contrast;
    double texture_contrast;
    bool has_texture_flow;
    double texture_flow;
    bool has_texture_color_depth;
    double texture_color_depth;
    bool has_texture_surface_damage;
    double texture_surface_damage;
    bool has_preview_overlay;
    char preview_overlay_kind[MATERIAL_PREVIEW_MAX_LAYER_KIND_ID];
    double preview_overlay_opacity;
    bool has_preview_overlay_scale;
    double preview_overlay_scale;
    bool has_preview_overlay_strength;
    double preview_overlay_strength;
    bool has_preview_overlay_offset_u;
    double preview_overlay_offset_u;
    bool has_preview_overlay_offset_v;
    double preview_overlay_offset_v;
    bool has_preview_overlay_pattern_mode;
    int preview_overlay_pattern_mode;
    bool has_preview_overlay_coverage;
    double preview_overlay_coverage;
    bool has_preview_overlay_grain;
    double preview_overlay_grain;
    bool has_preview_overlay_edge_softness;
    double preview_overlay_edge_softness;
    bool has_preview_overlay_contrast;
    double preview_overlay_contrast;
    bool has_preview_overlay_flow;
    double preview_overlay_flow;
    bool has_preview_overlay_color_depth;
    double preview_overlay_color_depth;
    bool has_preview_overlay_surface_damage;
    double preview_overlay_surface_damage;
    bool has_preview_overlay_seed;
    int preview_overlay_seed;
} MaterialPreviewVariantOverrides;

typedef struct MaterialPreviewRequest {
    char schema[64];
    char request_path[MATERIAL_PREVIEW_MAX_PATH];
    char runtime_scene_path[MATERIAL_PREVIEW_MAX_PATH];
    char output_path[MATERIAL_PREVIEW_MAX_PATH];
    char summary_path[MATERIAL_PREVIEW_MAX_PATH];
    char object_id[MATERIAL_PREVIEW_MAX_OBJECT_ID];
    int scene_object_index;
    bool has_scene_object_index;
    int cell_width;
    int cell_height;
    int columns;
    bool has_background_color;
    unsigned int background_color_rgb;
    int variant_count;
    MaterialPreviewVariantOverrides variants[MATERIAL_PREVIEW_MAX_VARIANTS];
} MaterialPreviewRequest;

bool MaterialPreviewRequestLoadFromFile(const char* request_path,
                                        MaterialPreviewRequest* out_request,
                                        char* out_diagnostics,
                                        size_t out_diagnostics_size);

#endif
