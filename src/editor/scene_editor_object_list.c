#include "editor/scene_editor_object_list.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "config/config_manager.h"
#include "editor/object_editor.h"
#include "editor/scene_editor_mesh_preview_store.h"
#include "import/runtime_scene_bridge.h"
#include "import/runtime_mesh_asset_loader.h"
#include "kit_ui.h"
#include "render/render_helper.h"

#define OBJECT_LIST_ROW_HEIGHT 24
#define OBJECT_LIST_ROW_GAP 4
#define OBJECT_LIST_VISIBLE_ROWS 6

static SDL_Rect g_viewport = {0, 0, 0, 0};
static float g_scroll_offset = 0.0f;
static float g_content_height = 0.0f;
static int g_last_selected = -1;

static void draw_scrollbar(SDL_Renderer* renderer) {
    SDL_Rect track = {0, 0, 0, 0};
    SDL_Rect thumb = {0, 0, 0, 0};
    int content_height = (int)ceilf(g_content_height);
    int max_offset = content_height - g_viewport.h;
    int thumb_height = 0;
    int travel = 0;
    if (!renderer || g_viewport.w <= 0 || g_viewport.h <= 0 || max_offset <= 0) return;
    track = (SDL_Rect){g_viewport.x + g_viewport.w - 8, g_viewport.y, 6, g_viewport.h};
    thumb_height = (g_viewport.h * g_viewport.h) / content_height;
    if (thumb_height < g_viewport.h / 10) thumb_height = g_viewport.h / 10;
    if (thumb_height < 1) thumb_height = 1;
    travel = track.h - thumb_height;
    thumb = (SDL_Rect){track.x,
                       track.y + ((int)lroundf(g_scroll_offset) * travel) / max_offset,
                       track.w,
                       thumb_height};
    SDL_SetRenderDrawColor(renderer, 48, 54, 60, 220);
    SDL_RenderFillRect(renderer, &track);
    SDL_SetRenderDrawColor(renderer, 150, 168, 188, 255);
    SDL_RenderFillRect(renderer, &thumb);
}

static bool point_in_rect(int x, int y, const SDL_Rect* rect) {
    return rect && rect->w > 0 && rect->h > 0 &&
           x >= rect->x && x < rect->x + rect->w &&
           y >= rect->y && y < rect->y + rect->h;
}

static int render_line(SDL_Renderer* renderer,
                       SDL_Rect bounds,
                       int cursor_y,
                       int bottom_y,
                       const char* text,
                       SDL_Color color) {
    SDL_Rect line_rect = {bounds.x, cursor_y, bounds.w, bottom_y - cursor_y};
    int used_height = 0;
    if (!renderer || !text || !text[0] || line_rect.w <= 0 || line_rect.h <= 0) {
        return cursor_y;
    }
    used_height = RenderLabelTextLeft(renderer, line_rect, text, color);
    if (used_height < 1) used_height = 18;
    return cursor_y + used_height + 6;
}

static const RayTracingRuntimeMeshAssetInstance* loaded_mesh(int object_index) {
    const RayTracingRuntimeMeshAssetSet* assets = ray_tracing_runtime_mesh_assets_last();
    if (!assets || object_index < 0) return NULL;
    for (int i = 0; i < assets->instance_count; ++i) {
        if (assets->instances[i].scene_object_index == object_index) return &assets->instances[i];
    }
    return NULL;
}

static const RayTracingRuntimeMeshAssetSkippedInstance* skipped_mesh(int object_index) {
    const RayTracingRuntimeMeshAssetSet* assets = ray_tracing_runtime_mesh_assets_last();
    if (!assets || object_index < 0) return NULL;
    for (int i = 0; i < assets->skipped_instance_count; ++i) {
        if (assets->skipped_instances[i].scene_object_index == object_index) {
            return &assets->skipped_instances[i];
        }
    }
    return NULL;
}

static const RuntimeSceneBridgePrimitiveDigest* primitive_for(
    const RuntimeSceneBridge3DDigestState* digest,
    int object_index) {
    if (!digest || !digest->valid || object_index < 0) return NULL;
    for (int i = 0; i < digest->primitive_count; ++i) {
        if (digest->primitives[i].scene_object_index == object_index) return &digest->primitives[i];
    }
    return NULL;
}

static const char* primitive_label(RuntimeSceneBridgePrimitiveKind kind) {
    switch (kind) {
        case RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE: return "plane";
        case RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM: return "prism";
        case RUNTIME_SCENE_BRIDGE_PRIMITIVE_BOX: return "box";
        case RUNTIME_SCENE_BRIDGE_PRIMITIVE_TRIANGLE_MESH: return "tri mesh";
        case RUNTIME_SCENE_BRIDGE_PRIMITIVE_UNKNOWN:
        default: return "primitive";
    }
}

static const char* short_object_id(int object_index, char* buffer, size_t buffer_size) {
    const char* prefix = NULL;
    if (!buffer || buffer_size == 0u) return "";
    buffer[0] = '\0';
    if (!runtime_scene_bridge_get_last_object_id_for_scene_index(object_index,
                                                                 buffer,
                                                                 buffer_size)) {
        return "";
    }
    prefix = strrchr(buffer, '_');
    return (prefix && prefix[1]) ? prefix + 1 : buffer;
}

static float clamp_offset(float offset) {
    float max_offset = g_content_height - (float)g_viewport.h;
    if (max_offset < 0.0f) max_offset = 0.0f;
    if (offset < 0.0f) return 0.0f;
    if (offset > max_offset) return max_offset;
    return offset;
}

static void reveal_selected(int selected_index) {
    const float row_pitch = (float)(OBJECT_LIST_ROW_HEIGHT + OBJECT_LIST_ROW_GAP);
    float row_top = 0.0f;
    float row_bottom = 0.0f;
    if (selected_index < 0 || selected_index >= sceneSettings.objectCount ||
        selected_index == g_last_selected || g_viewport.h <= 0) {
        g_last_selected = selected_index;
        return;
    }
    row_top = (float)selected_index * row_pitch;
    row_bottom = row_top + (float)OBJECT_LIST_ROW_HEIGHT;
    if (row_top < g_scroll_offset) {
        g_scroll_offset = row_top;
    } else if (row_bottom > g_scroll_offset + (float)g_viewport.h) {
        g_scroll_offset = row_bottom - (float)g_viewport.h;
    }
    g_scroll_offset = clamp_offset(g_scroll_offset);
    g_last_selected = selected_index;
}

bool SceneEditorObjectListContainsPoint(int x, int y) {
    return point_in_rect(x, y, &g_viewport);
}

bool SceneEditorObjectListHandleWheel(int x, int y, float wheel_delta_y) {
    KitUiScrollResult result = {0};
    KitRenderRect viewport = {0};
    if (!SceneEditorObjectListContainsPoint(x, y) || wheel_delta_y == 0.0f) return false;
    viewport = (KitRenderRect){(float)g_viewport.x,
                               (float)g_viewport.y,
                               (float)g_viewport.w,
                               (float)g_viewport.h};
    result = kit_ui_eval_scroll(viewport,
                                g_scroll_offset,
                                g_content_height,
                                wheel_delta_y);
    g_scroll_offset = result.offset_y;
    return result.changed != 0;
}

float SceneEditorObjectListScrollOffset(void) {
    return g_scroll_offset;
}

void SceneEditorObjectListReset(void) {
    g_viewport = (SDL_Rect){0, 0, 0, 0};
    g_scroll_offset = 0.0f;
    g_content_height = 0.0f;
    g_last_selected = -1;
}

int SceneEditorObjectListRender(SDL_Renderer* renderer,
                                SDL_Rect bounds,
                                int cursor_y,
                                int bottom_y,
                                int selected_index,
                                SDL_Color title_color,
                                SDL_Color body_color) {
    const int row_pitch = OBJECT_LIST_ROW_HEIGHT + OBJECT_LIST_ROW_GAP;
    RuntimeSceneBridge3DDigestState digest = {0};
    SDL_Rect previous_clip = {0, 0, 0, 0};
    SDL_bool clip_was_enabled = SDL_FALSE;
    int viewport_height = 0;
    int first_row = 0;
    int row_y = 0;
    char line[160];
    if (!renderer || bounds.w <= 0 || cursor_y >= bottom_y) return cursor_y;

    ObjectEditorClearObjectListRows();
    snprintf(line,
             sizeof(line),
             "Scene Objects  %d   Mesh Preview %d",
             sceneSettings.objectCount,
             SceneEditorMeshPreviewStoreInstanceCount());
    cursor_y = render_line(renderer, bounds, cursor_y, bottom_y, line, title_color);
    viewport_height = OBJECT_LIST_VISIBLE_ROWS * row_pitch - OBJECT_LIST_ROW_GAP;
    if (viewport_height > bottom_y - cursor_y) viewport_height = bottom_y - cursor_y;
    if (viewport_height < OBJECT_LIST_ROW_HEIGHT) {
        g_viewport = (SDL_Rect){0, 0, 0, 0};
        return cursor_y;
    }
    g_viewport = (SDL_Rect){bounds.x, cursor_y, bounds.w, viewport_height};
    g_content_height = kit_ui_scroll_content_height_top_anchor(sceneSettings.objectCount,
                                                               (float)row_pitch,
                                                               (float)viewport_height);
    g_scroll_offset = clamp_offset(g_scroll_offset);
    reveal_selected(selected_index);
    runtime_scene_bridge_get_last_3d_digest_state(&digest);

    clip_was_enabled = SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer, &previous_clip);
    SDL_RenderSetClipRect(renderer, &g_viewport);
    first_row = (int)floorf(g_scroll_offset / (float)row_pitch);
    if (first_row < 0) first_row = 0;
    row_y = g_viewport.y + first_row * row_pitch - (int)lroundf(g_scroll_offset);
    for (int i = first_row; i < sceneSettings.objectCount; ++i, row_y += row_pitch) {
        const SceneObject* obj = &sceneSettings.sceneObjects[i];
        const RayTracingRuntimeMeshAssetInstance* loaded = loaded_mesh(i);
        const RayTracingRuntimeMeshAssetSkippedInstance* skipped = skipped_mesh(i);
        const RuntimeSceneBridgePrimitiveDigest* primitive = primitive_for(&digest, i);
        char id_buffer[64];
        const char* short_id = short_object_id(i, id_buffer, sizeof(id_buffer));
        const char* role = NULL;
        bool selected = i == selected_index;
        SDL_Rect row = {bounds.x, row_y, bounds.w - 12, OBJECT_LIST_ROW_HEIGHT};
        SDL_Rect hit_row = row;
        SDL_Color fill = selected ? (SDL_Color){96, 104, 112, 220}
                                  : (SDL_Color){20, 23, 26, 210};
        SDL_Color border = selected ? (SDL_Color){188, 198, 208, 255}
                                    : (SDL_Color){48, 54, 60, 220};
        if (row.y >= g_viewport.y + g_viewport.h) break;
        if (row.y + row.h <= g_viewport.y) continue;
        if (loaded) {
            role = SceneEditorMeshPreviewStoreSceneObjectUsesBoundsFallback(i)
                       ? "mesh bounds" : "mesh loaded";
        } else if (skipped && SceneEditorMeshPreviewStoreHasSceneObject(i)) {
            role = SceneEditorMeshPreviewStoreSceneObjectUsesBoundsFallback(i)
                       ? "mesh bounds" : "mesh preview";
        } else if (skipped) {
            role = "mesh skipped";
        } else if (primitive) {
            role = primitive_label(primitive->kind);
        } else {
            role = obj->type[0] ? obj->type : "object";
        }
        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
        SDL_RenderFillRect(renderer, &row);
        SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
        SDL_RenderDrawRect(renderer, &row);
        if (hit_row.y < g_viewport.y) {
            hit_row.h -= g_viewport.y - hit_row.y;
            hit_row.y = g_viewport.y;
        }
        if (hit_row.y + hit_row.h > g_viewport.y + g_viewport.h) {
            hit_row.h = g_viewport.y + g_viewport.h - hit_row.y;
        }
        ObjectEditorRegisterObjectListRow(i, hit_row);
        if (SceneEditorMeshPreviewStoreSceneObjectUsesBoundsFallback(i)) {
            snprintf(line, sizeof(line), "#%d  %s  %s  AABB fallback", i, role, short_id);
        } else if (skipped && SceneEditorMeshPreviewStoreHasSceneObject(i)) {
            snprintf(line, sizeof(line), "#%d  %s  %s  LOD from %.1f MB", i, role, short_id,
                     (double)skipped->file_size_bytes / (1024.0 * 1024.0));
        } else if (skipped) {
            snprintf(line, sizeof(line), "#%d  %s  %s  %.1f/%.1f MB", i, role, short_id,
                     (double)skipped->file_size_bytes / (1024.0 * 1024.0),
                     (double)skipped->max_file_size_bytes / (1024.0 * 1024.0));
        } else if (loaded) {
            snprintf(line, sizeof(line), "#%d  %s  %s  z %.1f", i, role, short_id, obj->z);
        } else {
            snprintf(line, sizeof(line), "#%d  %s%s  %s  z %.1f", i, role,
                     SceneObjectIsGuideOnly(obj) ? " guide" : "", short_id, obj->z);
        }
        RenderLabelTextLeft(renderer,
                            (SDL_Rect){row.x + 8, row.y + 2, row.w - 16, row.h - 4},
                            line,
                            body_color);
    }
    SDL_RenderSetClipRect(renderer, clip_was_enabled ? &previous_clip : NULL);
    draw_scrollbar(renderer);
    return g_viewport.y + g_viewport.h + 8;
}
