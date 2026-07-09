#include "editor/material_editor_authored_texture_binding.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/data_paths.h"
#include "editor/material_editor_internal.h"
#include "import/runtime_scene_bridge.h"
#include "render/render_helper.h"
#include "render/runtime_material_authored_texture_3d.h"

#define MATERIAL_EDITOR_AUTHORED_SECTION_HEIGHT 18
#define MATERIAL_EDITOR_AUTHORED_BUTTON_HEIGHT 22
#define MATERIAL_EDITOR_AUTHORED_BUTTON_GAP 5
#define MATERIAL_EDITOR_AUTHORED_STATUS_TTL_MS 2200u

static SDL_Rect s_authored_pick_rect = {0, 0, 0, 0};
static SDL_Rect s_authored_clear_rect = {0, 0, 0, 0};
static char s_authored_status_text[160];
static SDL_Color s_authored_status_color = {0, 0, 0, 0};
static Uint32 s_authored_status_expire_ms = 0u;

static bool material_editor_authored_point_in_rect(int x, int y, const SDL_Rect* rect) {
    return rect && rect->w > 0 && rect->h > 0 &&
           x >= rect->x && x < rect->x + rect->w &&
           y >= rect->y && y < rect->y + rect->h;
}

static void material_editor_authored_reset_rects(void) {
    s_authored_pick_rect = (SDL_Rect){0, 0, 0, 0};
    s_authored_clear_rect = (SDL_Rect){0, 0, 0, 0};
}

static void material_editor_authored_set_status(const char* text,
                                                SDL_Color color) {
    if (!text) text = "";
    snprintf(s_authored_status_text, sizeof(s_authored_status_text), "%s", text);
    s_authored_status_text[sizeof(s_authored_status_text) - 1] = '\0';
    s_authored_status_color = color;
    s_authored_status_expire_ms = SDL_GetTicks() + MATERIAL_EDITOR_AUTHORED_STATUS_TTL_MS;
}

static bool material_editor_authored_status_active(void) {
    if (s_authored_status_text[0] == '\0') return false;
    if (s_authored_status_expire_ms == 0u) return false;
    if (SDL_GetTicks() > s_authored_status_expire_ms) {
        s_authored_status_text[0] = '\0';
        s_authored_status_expire_ms = 0u;
        return false;
    }
    return true;
}

static void material_editor_authored_draw_button(SDL_Renderer* renderer,
                                                 SDL_Rect rect,
                                                 const char* label,
                                                 bool active,
                                                 RayTracingThemePalette palette) {
    SDL_Color fill = active ? ray_tracing_theme_resolve_button_active_fill(palette)
                            : palette.button_fill;
    SDL_Color text = ray_tracing_theme_choose_button_text(fill, palette);
    if (!renderer || !label || rect.w <= 0 || rect.h <= 0) return;
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, 255);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer,
                           palette.panel_border.r,
                           palette.panel_border.g,
                           palette.panel_border.b,
                           palette.panel_border.a);
    SDL_RenderDrawRect(renderer, &rect);
    RenderButtonTextWithColor(renderer, rect, label, text);
}

static bool material_editor_authored_object_id_for_focus(int focused_object_index,
                                                         char* out_object_id,
                                                         size_t out_size) {
    if (!out_object_id || out_size == 0u) return false;
    out_object_id[0] = '\0';
    if (focused_object_index < 0) return false;
    return runtime_scene_bridge_get_last_object_id_for_scene_index(focused_object_index,
                                                                   out_object_id,
                                                                   out_size);
}

static void material_editor_authored_basename(const char* path,
                                              char* out_name,
                                              size_t out_size) {
    const char* slash = NULL;
    if (!out_name || out_size == 0u) return;
    out_name[0] = '\0';
    if (!path || !path[0]) return;
    slash = strrchr(path, '/');
    snprintf(out_name, out_size, "%s", slash ? slash + 1 : path);
}

static bool material_editor_authored_escape_applescript_string(const char* src,
                                                               char* out,
                                                               size_t out_cap) {
    size_t write_index = 0u;
    size_t i = 0u;
    if (!src || !out || out_cap == 0u) return false;
    for (i = 0u; src[i] != '\0'; ++i) {
        char ch = src[i];
        if (ch == '"' || ch == '\\') {
            if (write_index + 2u >= out_cap) return false;
            out[write_index++] = '\\';
            out[write_index++] = ch;
            continue;
        }
        if ((unsigned char)ch < 32u) {
            if (write_index + 1u >= out_cap) return false;
            out[write_index++] = ' ';
            continue;
        }
        if (write_index + 1u >= out_cap) return false;
        out[write_index++] = ch;
    }
    out[write_index] = '\0';
    return true;
}

static void material_editor_authored_trim_path(char* path) {
    size_t len = 0u;
    if (!path) return;
    len = strlen(path);
    while (len > 0u &&
           (path[len - 1u] == '\n' || path[len - 1u] == '\r' || path[len - 1u] == ' ')) {
        path[--len] = '\0';
    }
    while (len > 1u && path[len - 1u] == '/') {
        path[--len] = '\0';
    }
}

static bool material_editor_authored_pick_file_macos(const char* prompt,
                                                     const char* initial_dir,
                                                     char* out_path,
                                                     size_t out_cap) {
#if defined(__APPLE__)
    FILE* pipe = NULL;
    char escaped_prompt[256];
    char escaped_initial_dir[768];
    char cmd[1600];
    char line[1024];
    if (!prompt || !out_path || out_cap == 0u) return false;
    if (!material_editor_authored_escape_applescript_string(prompt,
                                                            escaped_prompt,
                                                            sizeof(escaped_prompt))) {
        return false;
    }
    if (initial_dir && initial_dir[0] &&
        material_editor_authored_escape_applescript_string(initial_dir,
                                                           escaped_initial_dir,
                                                           sizeof(escaped_initial_dir))) {
        snprintf(cmd,
                 sizeof(cmd),
                 "/usr/bin/osascript -e 'set chosenFile to choose file with prompt \"%s\" default location POSIX file \"%s\"' -e 'POSIX path of chosenFile'",
                 escaped_prompt,
                 escaped_initial_dir);
    } else {
        snprintf(cmd,
                 sizeof(cmd),
                 "/usr/bin/osascript -e 'set chosenFile to choose file with prompt \"%s\"' -e 'POSIX path of chosenFile'",
                 escaped_prompt);
    }
    pipe = popen(cmd, "r");
    if (!pipe) return false;
    if (!fgets(line, sizeof(line), pipe)) {
        (void)pclose(pipe);
        return false;
    }
    (void)pclose(pipe);
    line[strcspn(line, "\r\n")] = '\0';
    material_editor_authored_trim_path(line);
    if (line[0] == '\0') return false;
    snprintf(out_path, out_cap, "%s", line);
    return true;
#else
    (void)prompt;
    (void)initial_dir;
    (void)out_path;
    (void)out_cap;
    return false;
#endif
}

static void material_editor_authored_default_pick_dir(char* out_dir,
                                                      size_t out_dir_size) {
    char output_root[PATH_MAX];
    char *marker = NULL;
    if (!out_dir || out_dir_size == 0u) return;
    out_dir[0] = '\0';
    if (!ray_tracing_find_stable_output_root(output_root, sizeof(output_root))) return;
    marker = strstr(output_root, "/ray_tracing/data/runtime");
    if (marker) {
        *marker = '\0';
        snprintf(out_dir, out_dir_size, "%s/drawing_program/data/output", output_root);
        return;
    }
    snprintf(out_dir, out_dir_size, "%s", output_root);
}

void MaterialEditorAuthoredTextureBindingReset(void) {
    material_editor_authored_reset_rects();
    s_authored_status_text[0] = '\0';
    s_authored_status_expire_ms = 0u;
    s_authored_status_color = (SDL_Color){0, 0, 0, 0};
}

bool MaterialEditorAuthoredTextureBindingBindForFocused(int focused_object_index,
                                                        const char* manifest_path) {
    char object_id[64];
    char reason[RUNTIME_MATERIAL_AUTHORED_TEXTURE_REASON_CAPACITY];
    char ignored_manifest_path[RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY];
    char ignored_binding_mode[RUNTIME_MATERIAL_AUTHORED_TEXTURE_MODE_CAPACITY];
    char status[192];
    if (!material_editor_authored_object_id_for_focus(focused_object_index,
                                                      object_id,
                                                      sizeof(object_id))) {
        material_editor_authored_set_status("Bind failed: object id unavailable",
                                            (SDL_Color){255, 170, 140, 255});
        return false;
    }
    if (!RuntimeMaterialAuthoredTextureBindManifestForObject(focused_object_index,
                                                             object_id,
                                                             manifest_path,
                                                             "override")) {
        if (RuntimeMaterialAuthoredTextureGetInvalidBinding(focused_object_index,
                                                            ignored_manifest_path,
                                                            sizeof(ignored_manifest_path),
                                                            ignored_binding_mode,
                                                            sizeof(ignored_binding_mode),
                                                            reason,
                                                            sizeof(reason)) &&
            reason[0]) {
            snprintf(status, sizeof(status), "Bind failed: %s", reason);
            material_editor_authored_set_status(status, (SDL_Color){255, 170, 140, 255});
        } else {
            material_editor_authored_set_status("Bind failed: manifest validation failed",
                                                (SDL_Color){255, 170, 140, 255});
        }
        return false;
    }
    if (focused_object_index >= 0 && focused_object_index < sceneSettings.objectCount) {
        MarkObjectDirty(&sceneSettings.sceneObjects[focused_object_index]);
    }
    material_editor_authored_set_status("Authored texture bound",
                                        (SDL_Color){120, 220, 180, 255});
    return true;
}

bool MaterialEditorAuthoredTextureBindingClearForFocused(int focused_object_index) {
    if (!RuntimeMaterialAuthoredTextureClearBindingForObject(focused_object_index)) {
        material_editor_authored_set_status("Clear failed: invalid object",
                                            (SDL_Color){255, 170, 140, 255});
        return false;
    }
    if (focused_object_index >= 0 && focused_object_index < sceneSettings.objectCount) {
        MarkObjectDirty(&sceneSettings.sceneObjects[focused_object_index]);
    }
    material_editor_authored_set_status("Authored texture cleared",
                                        (SDL_Color){120, 220, 180, 255});
    return true;
}

bool MaterialEditorAuthoredTextureBindingGetSummary(int focused_object_index,
                                                    char* out_manifest_path,
                                                    size_t out_manifest_path_size,
                                                    char* out_binding_mode,
                                                    size_t out_binding_mode_size,
                                                    int* out_face_count) {
    return RuntimeMaterialAuthoredTextureGetBinding(focused_object_index,
                                                    out_manifest_path,
                                                    out_manifest_path_size,
                                                    out_binding_mode,
                                                    out_binding_mode_size,
                                                    out_face_count);
}

bool MaterialEditorAuthoredTextureBindingGetInvalidSummary(int focused_object_index,
                                                           char* out_manifest_path,
                                                           size_t out_manifest_path_size,
                                                           char* out_binding_mode,
                                                           size_t out_binding_mode_size,
                                                           char* out_reason,
                                                           size_t out_reason_size) {
    return RuntimeMaterialAuthoredTextureGetInvalidBinding(focused_object_index,
                                                           out_manifest_path,
                                                           out_manifest_path_size,
                                                           out_binding_mode,
                                                           out_binding_mode_size,
                                                           out_reason,
                                                           out_reason_size);
}

bool MaterialEditorAuthoredTextureBindingGetChannelSummary(int focused_object_index,
                                                           char* out_channel_summary,
                                                           size_t out_channel_summary_size) {
    return RuntimeMaterialAuthoredTextureGetChannelSummary(focused_object_index,
                                                           out_channel_summary,
                                                           out_channel_summary_size);
}

int MaterialEditorAuthoredTextureBindingRenderPaneControls(SDL_Renderer* renderer,
                                                           SDL_Rect content_bounds,
                                                           int cursor_y,
                                                           int bottom_y,
                                                           int focused_object_index,
                                                           RayTracingThemePalette palette) {
    char manifest_path[RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY];
    char binding_mode[RUNTIME_MATERIAL_AUTHORED_TEXTURE_MODE_CAPACITY];
    char invalid_reason[RUNTIME_MATERIAL_AUTHORED_TEXTURE_REASON_CAPACITY];
    char manifest_name[96];
    char line[192];
    int face_count = 0;
    bool has_binding = false;
    bool has_invalid = false;
    int button_y = 0;
    int button_w = 0;
    int button_h = MATERIAL_EDITOR_AUTHORED_BUTTON_HEIGHT;
    if (!renderer || content_bounds.w <= 0 || cursor_y >= bottom_y) return cursor_y;
    material_editor_authored_reset_rects();

    has_binding = MaterialEditorAuthoredTextureBindingGetSummary(focused_object_index,
                                                                 manifest_path,
                                                                 sizeof(manifest_path),
                                                                 binding_mode,
                                                                 sizeof(binding_mode),
                                                                 &face_count);
    has_invalid = !has_binding &&
                  MaterialEditorAuthoredTextureBindingGetInvalidSummary(focused_object_index,
                                                                        manifest_path,
                                                                        sizeof(manifest_path),
                                                                        binding_mode,
                                                                        sizeof(binding_mode),
                                                                        invalid_reason,
                                                                        sizeof(invalid_reason));
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){content_bounds.x, cursor_y, content_bounds.w, 16},
                        MaterialEditorPanelGroupLabel(
                            MATERIAL_EDITOR_PANEL_GROUP_TEXTURE_BINDING),
                        palette.text_primary);
    cursor_y += MATERIAL_EDITOR_AUTHORED_SECTION_HEIGHT;

    snprintf(line,
             sizeof(line),
             "Binding %s",
             has_binding ? (binding_mode[0] ? binding_mode : "override")
                         : (has_invalid ? "invalid" : "none"));
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){content_bounds.x, cursor_y, content_bounds.w, 16},
                        line,
                        has_binding ? palette.text_primary
                                    : (has_invalid ? palette.accent_primary
                                                   : palette.text_muted));
    cursor_y += 18;

    if (has_binding) {
        material_editor_authored_basename(manifest_path, manifest_name, sizeof(manifest_name));
        snprintf(line, sizeof(line), "%s  %d faces", manifest_name, face_count);
    } else if (has_invalid) {
        material_editor_authored_basename(manifest_path, manifest_name, sizeof(manifest_name));
        snprintf(line,
                 sizeof(line),
                 "%s  %s",
                 manifest_name[0] ? manifest_name : "(manifest)",
                 invalid_reason[0] ? invalid_reason : "validation failed");
    } else {
        snprintf(line, sizeof(line), "No authored texture set bound");
    }
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){content_bounds.x, cursor_y, content_bounds.w, 16},
                        line,
                        has_invalid ? palette.accent_primary : palette.text_muted);
    cursor_y += 20;

    if (cursor_y + button_h <= bottom_y) {
        button_y = cursor_y;
        button_w = (content_bounds.w - MATERIAL_EDITOR_AUTHORED_BUTTON_GAP) / 2;
        s_authored_pick_rect = (SDL_Rect){content_bounds.x, button_y, button_w, button_h};
        s_authored_clear_rect =
            (SDL_Rect){content_bounds.x + button_w + MATERIAL_EDITOR_AUTHORED_BUTTON_GAP,
                       button_y,
                       content_bounds.w - button_w - MATERIAL_EDITOR_AUTHORED_BUTTON_GAP,
                       button_h};
        material_editor_authored_draw_button(renderer,
                                             s_authored_pick_rect,
                                             (has_binding || has_invalid) ? "Replace Manifest"
                                                                          : "Pick Manifest",
                                             false,
                                             palette);
        material_editor_authored_draw_button(renderer,
                                             s_authored_clear_rect,
                                             "Clear Binding",
                                             has_binding || has_invalid,
                                             palette);
        cursor_y += button_h + MATERIAL_EDITOR_AUTHORED_BUTTON_GAP;
    }

    if (material_editor_authored_status_active() && cursor_y + 16 <= bottom_y) {
        RenderLabelTextWrappedLeft(renderer,
                                   (SDL_Rect){content_bounds.x, cursor_y, content_bounds.w, 32},
                                   s_authored_status_text,
                                   s_authored_status_color);
        cursor_y += 20;
    }

    return cursor_y;
}

bool MaterialEditorAuthoredTextureBindingHandleEvent(const SDL_Event* event,
                                                     int focused_object_index) {
    char picked_path[RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY];
    char manifest_path[RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY];
    char initial_dir[PATH_MAX];
    if (!event) return false;
    if (event->type != SDL_MOUSEBUTTONDOWN || event->button.button != SDL_BUTTON_LEFT) {
        return false;
    }
    if (material_editor_authored_point_in_rect(event->button.x,
                                               event->button.y,
                                               &s_authored_pick_rect)) {
        initial_dir[0] = '\0';
        picked_path[0] = '\0';
        if (MaterialEditorAuthoredTextureBindingGetSummary(focused_object_index,
                                                           manifest_path,
                                                           sizeof(manifest_path),
                                                           NULL,
                                                           0u,
                                                           NULL)) {
            snprintf(initial_dir, sizeof(initial_dir), "%s", manifest_path);
        } else {
            material_editor_authored_default_pick_dir(initial_dir, sizeof(initial_dir));
        }
        if (!material_editor_authored_pick_file_macos("Select authored texture manifest",
                                                      initial_dir[0] ? initial_dir : NULL,
                                                      picked_path,
                                                      sizeof(picked_path))) {
            material_editor_authored_set_status("Manifest picker cancelled or unavailable",
                                                (SDL_Color){210, 210, 215, 255});
            return true;
        }
        (void)MaterialEditorAuthoredTextureBindingBindForFocused(focused_object_index, picked_path);
        return true;
    }
    if (material_editor_authored_point_in_rect(event->button.x,
                                               event->button.y,
                                               &s_authored_clear_rect)) {
        (void)MaterialEditorAuthoredTextureBindingClearForFocused(focused_object_index);
        return true;
    }
    return false;
}
