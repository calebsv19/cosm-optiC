#include "editor/material_editor_face_preview.h"

#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "editor/material_preview_surface_eval.h"
#include "editor/scene_editor_material_face_metrics.h"
#include "render/render_helper.h"

#define MATERIAL_EDITOR_FACE_PREVIEW_MAX_SIZE 220
#define MATERIAL_EDITOR_FACE_PREVIEW_MIN_SIZE 96
#define MATERIAL_EDITOR_FACE_PREVIEW_MIN_SHORT_SIZE 24
#define MATERIAL_EDITOR_FACE_PREVIEW_LABEL_H 24
#define MATERIAL_EDITOR_FACE_PREVIEW_GAP 7

typedef struct MaterialEditorFacePreviewCache {
    SDL_Renderer* renderer;
    SDL_PixelFormat* pixel_format;
    Uint32* pixels;
    int pixel_capacity;
    int width;
    int height;
    int scene_object_index;
    int primitive_index;
    int face_group_index;
    bool dirty;
#if USE_VULKAN
    SDL_Surface* surface;
    VkRendererTexture texture;
    bool texture_ready;
#else
    SDL_Texture* texture;
#endif
} MaterialEditorFacePreviewCache;

static MaterialEditorFacePreviewCache s_face_preview_cache = {
    NULL,
    NULL,
    NULL,
    0,
    0,
    0,
    -1,
    -1,
    -1,
    true,
#if USE_VULKAN
    NULL,
    {0},
    false
#else
    NULL
#endif
};

static bool s_face_preview_use_transparency = false;
static SDL_Rect s_face_preview_alpha_toggle_rect = {0, 0, 0, 0};

static bool material_editor_face_preview_point_in_rect(int x, int y, const SDL_Rect* rect) {
    return rect && rect->w > 0 && rect->h > 0 &&
           x >= rect->x && x < rect->x + rect->w &&
           y >= rect->y && y < rect->y + rect->h;
}

static int material_editor_face_preview_size_for_width(int panel_width) {
    int preview_size = panel_width;
    if (preview_size > MATERIAL_EDITOR_FACE_PREVIEW_MAX_SIZE) {
        preview_size = MATERIAL_EDITOR_FACE_PREVIEW_MAX_SIZE;
    }
    if (preview_size < MATERIAL_EDITOR_FACE_PREVIEW_MIN_SIZE) {
        preview_size = MATERIAL_EDITOR_FACE_PREVIEW_MIN_SIZE;
    }
    return preview_size;
}

static bool material_editor_face_preview_clamped_display_size(
    double aspect_ratio,
    int panel_width,
    int* out_width,
    int* out_height) {
    int max_size = material_editor_face_preview_size_for_width(panel_width);
    int width = max_size;
    int height = max_size;

    if (!(aspect_ratio > 1e-6)) aspect_ratio = 1.0;
    if (aspect_ratio >= 1.0) {
        width = max_size;
        height = (int)lround((double)max_size / aspect_ratio);
    } else {
        height = max_size;
        width = (int)lround((double)max_size * aspect_ratio);
    }
    if (width < MATERIAL_EDITOR_FACE_PREVIEW_MIN_SHORT_SIZE) width = MATERIAL_EDITOR_FACE_PREVIEW_MIN_SHORT_SIZE;
    if (height < MATERIAL_EDITOR_FACE_PREVIEW_MIN_SHORT_SIZE) height = MATERIAL_EDITOR_FACE_PREVIEW_MIN_SHORT_SIZE;
    if (width > MATERIAL_EDITOR_FACE_PREVIEW_MAX_SIZE) width = MATERIAL_EDITOR_FACE_PREVIEW_MAX_SIZE;
    if (height > MATERIAL_EDITOR_FACE_PREVIEW_MAX_SIZE) height = MATERIAL_EDITOR_FACE_PREVIEW_MAX_SIZE;
    if (out_width) *out_width = width;
    if (out_height) *out_height = height;
    return true;
}

bool MaterialEditorFacePreviewResolveDisplaySize(const SceneObject* object,
                                                 int scene_object_index,
                                                 int active_face_group_index,
                                                 int panel_width,
                                                 int* out_width,
                                                 int* out_height) {
    SceneEditorMaterialPreviewTriangleAddress address = {0};
    address.sceneObjectIndex = scene_object_index;
    address.primitiveIndex = -1;
    address.faceGroupIndex = active_face_group_index;
    return MaterialEditorFacePreviewResolveDisplaySizeForAddress(object,
                                                                 &address,
                                                                 panel_width,
                                                                 out_width,
                                                                 out_height);
}

bool MaterialEditorFacePreviewResolveDisplaySizeForAddress(
    const SceneObject* object,
    const SceneEditorMaterialPreviewTriangleAddress* active_face_address,
    int panel_width,
    int* out_width,
    int* out_height) {
    SceneEditorMaterialFaceMetrics metrics = {0};
    double aspect_ratio = 1.0;
    int scene_object_index = active_face_address ? active_face_address->sceneObjectIndex : -1;
    int primitive_index = active_face_address ? active_face_address->primitiveIndex : -1;
    int active_face_group_index = active_face_address ? active_face_address->faceGroupIndex : -1;

    if (out_width) *out_width = 0;
    if (out_height) *out_height = 0;
    if (!object || panel_width <= 0) return false;

    if (active_face_group_index >= 0 &&
        SceneEditorMaterialFaceMetricsResolve(primitive_index,
                                              scene_object_index,
                                              active_face_group_index,
                                              &metrics) &&
        metrics.valid &&
        metrics.height > 1e-6) {
        aspect_ratio = metrics.width / metrics.height;
    }

    return material_editor_face_preview_clamped_display_size(aspect_ratio,
                                                             panel_width,
                                                             out_width,
                                                             out_height);
}

static void material_editor_face_preview_checker_color(int x,
                                                       int y,
                                                       Uint8* r,
                                                       Uint8* g,
                                                       Uint8* b) {
    int block = ((x / 12) + (y / 12)) & 1;
    Uint8 light = 228;
    Uint8 dark = 190;
    Uint8 value = block ? light : dark;
    if (r) *r = value;
    if (g) *g = value;
    if (b) *b = value;
}

static void material_editor_face_preview_release_texture(void) {
#if USE_VULKAN
    if (s_face_preview_cache.texture_ready && s_face_preview_cache.renderer) {
        vk_renderer_queue_texture_destroy(s_face_preview_cache.renderer,
                                          &s_face_preview_cache.texture);
        memset(&s_face_preview_cache.texture, 0, sizeof(s_face_preview_cache.texture));
        s_face_preview_cache.texture_ready = false;
    }
    if (s_face_preview_cache.surface) {
        SDL_FreeSurface(s_face_preview_cache.surface);
        s_face_preview_cache.surface = NULL;
    }
#else
    if (s_face_preview_cache.texture) {
        SDL_DestroyTexture(s_face_preview_cache.texture);
        s_face_preview_cache.texture = NULL;
    }
#endif
    s_face_preview_cache.renderer = NULL;
    s_face_preview_cache.width = 0;
    s_face_preview_cache.height = 0;
}

void MaterialEditorFacePreviewReset(void) {
    material_editor_face_preview_release_texture();
    if (s_face_preview_cache.pixel_format) {
        SDL_FreeFormat(s_face_preview_cache.pixel_format);
        s_face_preview_cache.pixel_format = NULL;
    }
    free(s_face_preview_cache.pixels);
    s_face_preview_cache.pixels = NULL;
    s_face_preview_cache.pixel_capacity = 0;
    s_face_preview_cache.scene_object_index = -1;
    s_face_preview_cache.primitive_index = -1;
    s_face_preview_cache.face_group_index = -1;
    s_face_preview_cache.dirty = true;
    s_face_preview_use_transparency = false;
    s_face_preview_alpha_toggle_rect = (SDL_Rect){0, 0, 0, 0};
}

void MaterialEditorFacePreviewInvalidate(void) {
    s_face_preview_cache.dirty = true;
}

bool MaterialEditorFacePreviewGetUseTransparency(void) {
    return s_face_preview_use_transparency;
}

void MaterialEditorFacePreviewSetUseTransparency(bool enabled) {
    if (s_face_preview_use_transparency == enabled) return;
    s_face_preview_use_transparency = enabled;
    MaterialEditorFacePreviewInvalidate();
}

bool MaterialEditorFacePreviewHitTest(int mx, int my) {
    return material_editor_face_preview_point_in_rect(mx,
                                                      my,
                                                      &s_face_preview_alpha_toggle_rect);
}

bool MaterialEditorFacePreviewHandleEvent(const SDL_Event* event) {
    if (!event ||
        event->type != SDL_MOUSEBUTTONDOWN ||
        event->button.button != SDL_BUTTON_LEFT) {
        return false;
    }
    if (!MaterialEditorFacePreviewHitTest(event->button.x, event->button.y)) {
        return false;
    }
    MaterialEditorFacePreviewSetUseTransparency(!s_face_preview_use_transparency);
    return true;
}

static bool material_editor_face_preview_ensure_pixels(int width, int height) {
    int pixel_count = 0;
    Uint32* pixels = NULL;
    if (width <= 0 || height <= 0) return false;
    if (height > 0 && width > INT_MAX / height) return false;
    pixel_count = width * height;
    if (pixel_count <= s_face_preview_cache.pixel_capacity &&
        s_face_preview_cache.pixels) {
        return true;
    }
    pixels = (Uint32*)realloc(s_face_preview_cache.pixels,
                              sizeof(Uint32) * (size_t)pixel_count);
    if (!pixels) return false;
    s_face_preview_cache.pixels = pixels;
    s_face_preview_cache.pixel_capacity = pixel_count;
    return true;
}

static bool material_editor_face_preview_ensure_texture(SDL_Renderer* renderer,
                                                        int width,
                                                        int height) {
    if (!renderer || width <= 0 || height <= 0) return false;
    if (s_face_preview_cache.renderer == renderer &&
        s_face_preview_cache.width == width &&
        s_face_preview_cache.height == height
#if USE_VULKAN
        && s_face_preview_cache.surface
#else
        && s_face_preview_cache.texture
#endif
    ) {
        return true;
    }
    material_editor_face_preview_release_texture();
    s_face_preview_cache.renderer = renderer;
    s_face_preview_cache.width = width;
    s_face_preview_cache.height = height;
#if USE_VULKAN
    s_face_preview_cache.surface = SDL_CreateRGBSurfaceWithFormat(0,
                                                                  width,
                                                                  height,
                                                                  32,
                                                                  SDL_PIXELFORMAT_ARGB8888);
    if (!s_face_preview_cache.surface) return false;
#else
    s_face_preview_cache.texture = SDL_CreateTexture(renderer,
                                                     SDL_PIXELFORMAT_ARGB8888,
                                                     SDL_TEXTUREACCESS_STATIC,
                                                     width,
                                                     height);
    if (!s_face_preview_cache.texture) return false;
    SDL_SetTextureBlendMode(s_face_preview_cache.texture, SDL_BLENDMODE_NONE);
#endif
    return true;
}

static bool material_editor_face_preview_ensure_resources(SDL_Renderer* renderer,
                                                          int width,
                                                          int height) {
    if (!s_face_preview_cache.pixel_format) {
        s_face_preview_cache.pixel_format = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
        if (!s_face_preview_cache.pixel_format) return false;
    }
    if (!material_editor_face_preview_ensure_pixels(width, height)) return false;
    if (!material_editor_face_preview_ensure_texture(renderer, width, height)) return false;
    return true;
}

static bool material_editor_face_preview_needs_rebuild(SDL_Renderer* renderer,
                                                       int width,
                                                       int height,
                                                       int scene_object_index,
                                                       int primitive_index,
                                                       int active_face_group_index) {
    if (s_face_preview_cache.dirty) return true;
    if (s_face_preview_cache.renderer != renderer) return true;
    if (s_face_preview_cache.width != width || s_face_preview_cache.height != height) {
        return true;
    }
    if (s_face_preview_cache.scene_object_index != scene_object_index) return true;
    if (s_face_preview_cache.primitive_index != primitive_index) return true;
    if (s_face_preview_cache.face_group_index != active_face_group_index) return true;
#if USE_VULKAN
    if (!s_face_preview_cache.texture_ready) return true;
#else
    if (!s_face_preview_cache.texture) return true;
#endif
    return false;
}

static bool material_editor_face_preview_rebuild(SDL_Renderer* renderer,
                                                 const SceneObject* object,
                                                 const SceneEditorMaterialPreviewTriangleAddress* active_face_address,
                                                 int width,
                                                 int height) {
#if !USE_VULKAN
    int update_result = 0;
#endif
    if (!material_editor_face_preview_ensure_resources(renderer, width, height)) {
        return false;
    }
    for (int py = 0; py < height; ++py) {
        for (int px = 0; px < width; ++px) {
            Uint8 bg_r = 0u;
            Uint8 bg_g = 0u;
            Uint8 bg_b = 0u;
            Uint8 out_r = 0u;
            Uint8 out_g = 0u;
            Uint8 out_b = 0u;
            double u = 0.0;
            double v = 0.0;
            RuntimeMaterialSurfaceEval eval = {0};
            material_editor_face_preview_checker_color(px, py, &bg_r, &bg_g, &bg_b);
            if (object && active_face_address &&
                active_face_address->faceGroupIndex >= 0 && width > 1 && height > 1) {
                u = (double)px / (double)(width - 1);
                v = (double)py / (double)(height - 1);
                if (MaterialPreviewSurfaceEvaluateFacePrimitive(
                        object,
                        active_face_address->sceneObjectIndex,
                        active_face_address->primitiveIndex,
                        active_face_address->faceGroupIndex,
                        u,
                        v,
                        &eval)) {
                    if (!s_face_preview_use_transparency) {
                        eval.transparency = 0.0;
                    }
                    MaterialPreviewSurfaceShadePixel(&eval,
                                                    object,
                                                    u,
                                                    v,
                                                    bg_r,
                                                    bg_g,
                                                    bg_b,
                                                    &out_r,
                                                    &out_g,
                                                    &out_b);
                } else {
                    out_r = bg_r;
                    out_g = bg_g;
                    out_b = bg_b;
                }
            } else {
                out_r = bg_r;
                out_g = bg_g;
                out_b = bg_b;
            }
            s_face_preview_cache.pixels[py * width + px] =
                SDL_MapRGBA(s_face_preview_cache.pixel_format,
                            out_r,
                            out_g,
                            out_b,
                            255);
        }
    }
#if USE_VULKAN
    for (int py = 0; py < height; ++py) {
        memcpy((Uint8*)s_face_preview_cache.surface->pixels +
                   py * s_face_preview_cache.surface->pitch,
               s_face_preview_cache.pixels + py * width,
               sizeof(Uint32) * (size_t)width);
    }
    if (s_face_preview_cache.texture_ready) {
        vk_renderer_queue_texture_destroy(renderer, &s_face_preview_cache.texture);
        memset(&s_face_preview_cache.texture, 0, sizeof(s_face_preview_cache.texture));
        s_face_preview_cache.texture_ready = false;
    }
    if (vk_renderer_upload_sdl_surface_with_filter(renderer,
                                                   s_face_preview_cache.surface,
                                                   &s_face_preview_cache.texture,
                                                   VK_FILTER_LINEAR) != VK_SUCCESS) {
        material_editor_face_preview_release_texture();
        return false;
    }
    s_face_preview_cache.texture_ready = true;
#else
    update_result = SDL_UpdateTexture(s_face_preview_cache.texture,
                                      NULL,
                                      s_face_preview_cache.pixels,
                                      width * (int)sizeof(Uint32));
    if (update_result != 0) {
        material_editor_face_preview_release_texture();
        return false;
    }
#endif
    s_face_preview_cache.scene_object_index =
        active_face_address ? active_face_address->sceneObjectIndex : -1;
    s_face_preview_cache.primitive_index =
        active_face_address ? active_face_address->primitiveIndex : -1;
    s_face_preview_cache.face_group_index =
        active_face_address ? active_face_address->faceGroupIndex : -1;
    s_face_preview_cache.dirty = false;
    return true;
}

int MaterialEditorFacePreviewPreferredHeight(const SceneObject* object,
                                             int scene_object_index,
                                             int active_face_group_index,
                                             int panel_width) {
    SceneEditorMaterialPreviewTriangleAddress address = {0};
    address.sceneObjectIndex = scene_object_index;
    address.primitiveIndex = -1;
    address.faceGroupIndex = active_face_group_index;
    return MaterialEditorFacePreviewPreferredHeightForAddress(object,
                                                              &address,
                                                              panel_width);
}

int MaterialEditorFacePreviewPreferredHeightForAddress(
    const SceneObject* object,
    const SceneEditorMaterialPreviewTriangleAddress* active_face_address,
    int panel_width) {
    int preview_width = 0;
    int preview_height = 0;
    if (!MaterialEditorFacePreviewResolveDisplaySizeForAddress(object,
                                                               active_face_address,
                                                               panel_width,
                                                               &preview_width,
                                                               &preview_height)) {
        preview_height = material_editor_face_preview_size_for_width(panel_width);
    }
    return MATERIAL_EDITOR_FACE_PREVIEW_LABEL_H +
           preview_height +
           MATERIAL_EDITOR_FACE_PREVIEW_GAP;
}

int MaterialEditorFacePreviewRenderPane(SDL_Renderer* renderer,
                                        SDL_Rect content_bounds,
                                        int cursor_y,
                                        const SceneObject* object,
                                        int scene_object_index,
                                        int active_face_group_index,
                                        RayTracingThemePalette palette) {
    SceneEditorMaterialPreviewTriangleAddress address = {0};
    address.sceneObjectIndex = scene_object_index;
    address.primitiveIndex = -1;
    address.faceGroupIndex = active_face_group_index;
    return MaterialEditorFacePreviewRenderPaneForAddress(renderer,
                                                         content_bounds,
                                                         cursor_y,
                                                         object,
                                                         &address,
                                                         palette);
}

static void material_editor_face_preview_draw_toggle(SDL_Renderer* renderer,
                                                     SDL_Rect rect,
                                                     RayTracingThemePalette palette) {
    SDL_Color fill = s_face_preview_use_transparency ? palette.button_active_fill
                                                     : palette.button_fill;
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, 255);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer,
                           palette.panel_border.r,
                           palette.panel_border.g,
                           palette.panel_border.b,
                           255);
    SDL_RenderDrawRect(renderer, &rect);
    RenderLabelText(renderer,
                    rect,
                    s_face_preview_use_transparency ? "Alpha On" : "Alpha Off",
                    palette.button_text);
}

int MaterialEditorFacePreviewRenderPaneForAddress(
    SDL_Renderer* renderer,
    SDL_Rect content_bounds,
    int cursor_y,
    const SceneObject* object,
    const SceneEditorMaterialPreviewTriangleAddress* active_face_address,
    RayTracingThemePalette palette) {
    int preview_width = 0;
    int preview_height = 0;
    SDL_Rect preview_rect = {0, 0, 0, 0};
    int scene_object_index = active_face_address ? active_face_address->sceneObjectIndex : -1;
    int primitive_index = active_face_address ? active_face_address->primitiveIndex : -1;
    int active_face_group_index = active_face_address ? active_face_address->faceGroupIndex : -1;
    if (!renderer || !object || content_bounds.w <= 0) return cursor_y;
    if (!MaterialEditorFacePreviewResolveDisplaySizeForAddress(object,
                                                               active_face_address,
                                                               content_bounds.w,
                                                               &preview_width,
                                                               &preview_height)) {
        preview_width = material_editor_face_preview_size_for_width(content_bounds.w);
        preview_height = preview_width;
    }
    preview_rect = (SDL_Rect){
        content_bounds.x + (content_bounds.w - preview_width) / 2,
        cursor_y + MATERIAL_EDITOR_FACE_PREVIEW_LABEL_H,
        preview_width,
        preview_height};
    s_face_preview_alpha_toggle_rect =
        (SDL_Rect){content_bounds.x + content_bounds.w - 84,
                   cursor_y,
                   84,
                   MATERIAL_EDITOR_FACE_PREVIEW_LABEL_H - 3};
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){content_bounds.x, cursor_y,
                                   content_bounds.w - 90,
                                   MATERIAL_EDITOR_FACE_PREVIEW_LABEL_H},
                        "Active Face Preview",
                        palette.text_primary);
    material_editor_face_preview_draw_toggle(renderer,
                                             s_face_preview_alpha_toggle_rect,
                                             palette);
    SDL_SetRenderDrawColor(renderer,
                           palette.panel_fill.r,
                           palette.panel_fill.g,
                           palette.panel_fill.b,
                           255);
    SDL_RenderFillRect(renderer, &preview_rect);
    if (material_editor_face_preview_needs_rebuild(renderer,
                                                   preview_rect.w,
                                                   preview_rect.h,
                                                   scene_object_index,
                                                   primitive_index,
                                                   active_face_group_index)) {
        if (!material_editor_face_preview_rebuild(renderer,
                                                  object,
                                                  active_face_address,
                                                  preview_rect.w,
                                                  preview_rect.h)) {
            MaterialEditorFacePreviewInvalidate();
        }
    }
    if (
#if USE_VULKAN
        s_face_preview_cache.texture_ready
#else
        s_face_preview_cache.texture
#endif
    ) {
#if USE_VULKAN
        vk_renderer_draw_texture(renderer, &s_face_preview_cache.texture, NULL, &preview_rect);
#else
        SDL_RenderCopy(renderer, s_face_preview_cache.texture, NULL, &preview_rect);
#endif
    }
    SDL_SetRenderDrawColor(renderer,
                           palette.panel_border.r,
                           palette.panel_border.g,
                           palette.panel_border.b,
                           255);
    SDL_RenderDrawRect(renderer, &preview_rect);
    if (active_face_group_index < 0) {
        SDL_Rect label = {
            preview_rect.x + 10,
            preview_rect.y + (preview_rect.h / 2) - 18,
            preview_rect.w - 20,
            40};
        RenderLabelTextWrappedLeft(renderer,
                                   label,
                                   "Select one face group to inspect a higher-quality detail preview.",
                                   palette.text_muted);
    }
    return preview_rect.y + preview_rect.h + MATERIAL_EDITOR_FACE_PREVIEW_GAP;
}
