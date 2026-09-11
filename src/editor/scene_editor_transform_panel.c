#include "editor/scene_editor_transform_panel.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

#include "app/data_paths.h"
#include "app/ray_tracing_deep_render_desktop_host.h"
#include "editor/object_editor.h"
#include "editor/object_editor_selection_tracker.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_runtime_scene_persistence.h"
#include "import/runtime_scene_bridge.h"
#include "platform/ray_tracing_folder_picker.h"
#include "render/render_helper.h"

#define TRANSFORM_FIELD_COUNT 9
#define PANEL_EDIT_CREASE_ANGLE TRANSFORM_FIELD_COUNT

typedef enum TransformPanelJobKind {
    TRANSFORM_PANEL_JOB_NONE = 0,
    TRANSFORM_PANEL_JOB_IMPORT,
    TRANSFORM_PANEL_JOB_SHADING
} TransformPanelJobKind;

static SDL_Rect s_fields[TRANSFORM_FIELD_COUNT];
static SDL_Rect s_name_field;
static SDL_Rect s_undo_button;
static SDL_Rect s_redo_button;
static SDL_Rect s_duplicate_button;
static SDL_Rect s_remove_button;
static SDL_Rect s_import_unit_buttons[2];
static SDL_Rect s_import_button;
static SDL_Rect s_crease_angle_field;
static SDL_Rect s_shading_buttons[4];
static int s_edit_field = -1;
static bool s_edit_name = false;
static char s_edit_buffer[128];
static char s_status[160];
static SDL_Color s_status_color = {210, 210, 215, 255};
static RayTracingFolderPickerRequest s_picker;
static bool s_picker_initialized = false;
static pid_t s_job_pid = -1;
static TransformPanelJobKind s_job_kind = TRANSFORM_PANEL_JOB_NONE;
static unsigned long long s_job_document_revision = 0u;
static bool s_controls_active = false;
static char s_candidate_path[PATH_MAX];
static double s_import_scale = 1.0;
static double s_crease_angle_degrees = 60.0;

static bool panel_point_in_rect(int x, int y, const SDL_Rect* rect) {
    return rect && rect->w > 0 && rect->h > 0 &&
           x >= rect->x && x <= rect->x + rect->w &&
           y >= rect->y && y <= rect->y + rect->h;
}

static void panel_status(const char* text, bool error) {
    snprintf(s_status, sizeof(s_status), "%s", text ? text : "");
    s_status_color = error ? (SDL_Color){255, 170, 140, 255}
                           : (SDL_Color){180, 225, 190, 255};
}

static bool panel_mutation_allowed(void) {
    if (RayTracingDeepRenderDesktopHost_HasActiveWork()) {
        panel_status("Document edits wait for the active render", true);
        return false;
    }
    if (s_picker.active || s_job_pid > 0) {
        panel_status("Managed mesh operation already active", true);
        return false;
    }
    return true;
}

static void panel_draw_button(SDL_Renderer* renderer,
                              SDL_Rect rect,
                              const char* label,
                              bool enabled,
                              bool active) {
    RayTracingThemePalette palette = SceneEditorChromeShellResolvePalette();
    SDL_Color fill = active ? palette.button_active_fill : palette.button_fill;
    SDL_Color text = ray_tracing_theme_choose_button_text(fill, palette);
    if (!enabled) {
        fill = (SDL_Color){92, 92, 100, 255};
        text = (SDL_Color){155, 155, 162, 255};
    }
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer,
                           palette.panel_border.r,
                           palette.panel_border.g,
                           palette.panel_border.b,
                           palette.panel_border.a);
    SDL_RenderDrawRect(renderer, &rect);
    RenderButtonTextWithColor(renderer, rect, label, text);
}

static const char* panel_field_label(int index) {
    static const char* labels[TRANSFORM_FIELD_COUNT] = {
        "PX", "PY", "PZ", "RX", "RY", "RZ", "SX", "SY", "SZ"
    };
    return index >= 0 && index < TRANSFORM_FIELD_COUNT ? labels[index] : "";
}

static double* panel_transform_component(SceneEditorDocumentTransform* transform, int index) {
    if (index < 3) return &transform->position[index];
    if (index < 6) return &transform->rotation_degrees[index - 3];
    return &transform->scale[index - 6];
}

static bool panel_commit_edit(void) {
    int selected = ObjectEditorGetSelectedObjectIndex();
    char diagnostics[256] = {0};
    if (s_edit_field >= 0) {
        SceneEditorDocumentTransform transform = {0};
        char* end = NULL;
        double value = 0.0;
        errno = 0;
        value = strtod(s_edit_buffer, &end);
        if (errno != 0 || end == s_edit_buffer || *end != '\0' || !isfinite(value)) {
            panel_status("Enter a finite number", true);
            return false;
        }
        if (s_edit_field == PANEL_EDIT_CREASE_ANGLE) {
            if (value <= 0.0 || value > 180.0) {
                panel_status("Crease angle must be greater than 0 through 180", true);
                return false;
            }
            s_crease_angle_degrees = value;
            panel_status("Crease angle ready", false);
        } else if (s_edit_field >= 6 && value <= 0.0) {
            panel_status("Enter a finite number; scale must be positive", true);
            return false;
        } else {
            if (!SceneEditorDocumentGetTransformForSceneIndex(selected,
                                                              &transform,
                                                              diagnostics,
                                                              sizeof(diagnostics))) {
                panel_status(diagnostics, true);
                return false;
            }
            *panel_transform_component(&transform, s_edit_field) = value;
            if (!SceneEditorDocumentSetTransformForSceneIndex(selected,
                                                              &transform,
                                                              diagnostics,
                                                              sizeof(diagnostics))) {
                panel_status(diagnostics, true);
                return false;
            }
            panel_status("Transform applied", false);
        }
    } else if (s_edit_name) {
        if (!SceneEditorDocumentRenameForSceneIndex(selected,
                                                    s_edit_buffer,
                                                    diagnostics,
                                                    sizeof(diagnostics))) {
            panel_status(diagnostics, true);
            return false;
        }
        panel_status("Display name applied", false);
    }
    s_edit_field = -1;
    s_edit_name = false;
    s_edit_buffer[0] = '\0';
    SDL_StopTextInput();
    return true;
}

static void panel_cancel_edit(void) {
    s_edit_field = -1;
    s_edit_name = false;
    s_edit_buffer[0] = '\0';
    SDL_StopTextInput();
}

static bool panel_program_tools(char* script,
                                size_t script_size,
                                char* compiler,
                                size_t compiler_size) {
    char root[PATH_MAX] = {0};
    struct utsname system_name;
    if (!ray_tracing_find_program_root(root, sizeof(root)) || uname(&system_name) != 0) {
        return false;
    }
    return snprintf(script,
                    script_size,
                    "%s/tools/managed_mesh_assets.py",
                    root) < (int)script_size &&
           snprintf(compiler,
                    compiler_size,
                    "%s/build/toolchains/clang/%s/tools/smooth_mesh_reflection/compile_runtime_fixture",
                    root,
                    system_name.machine) < (int)compiler_size &&
           access(script, R_OK) == 0 && access(compiler, X_OK) == 0;
}

static bool panel_spawn_managed_job(TransformPanelJobKind kind,
                                    const char* source,
                                    const char* object_id,
                                    const char* shading,
                                    double source_scale,
                                    double crease_angle_degrees) {
    char diagnostics[256] = {0};
    char script[PATH_MAX] = {0};
    char compiler[PATH_MAX] = {0};
    char generated_id[65] = {0};
    char scale_text[32] = {0};
    char crease_text[32] = {0};
    char scene_directory[PATH_MAX] = {0};
    char* slash = NULL;
    const char* scene = SceneEditorDocumentPath();
    pid_t child = -1;
    if (!panel_mutation_allowed() || !scene[0]) return false;
    if (!panel_program_tools(script, sizeof(script), compiler, sizeof(compiler))) {
        panel_status("Managed STL helper is not built for this checkout", true);
        return false;
    }
    if (!SceneEditorRuntimeScenePersistAuthoring(diagnostics, sizeof(diagnostics))) {
        panel_status(diagnostics, true);
        return false;
    }
    snprintf(scene_directory, sizeof(scene_directory), "%s", scene);
    slash = strrchr(scene_directory, '/');
    if (!slash) {
        panel_status("Runtime scene path has no parent directory", true);
        return false;
    }
    *slash = '\0';
    snprintf(generated_id,
             sizeof(generated_id),
             "mesh_%llx",
             (unsigned long long)SDL_GetPerformanceCounter());
    snprintf(scale_text, sizeof(scale_text), "%.17g", source_scale);
    snprintf(crease_text, sizeof(crease_text), "%.17g", crease_angle_degrees);
    if (snprintf(s_candidate_path,
                 sizeof(s_candidate_path),
                 "%s/.scene-editor-managed-%ld-%s.json",
                 scene_directory,
                 (long)getpid(),
                 generated_id) >= (int)sizeof(s_candidate_path)) {
        panel_status("Managed candidate path is too long", true);
        s_candidate_path[0] = '\0';
        return false;
    }
    (void)unlink(s_candidate_path);
    child = fork();
    if (child < 0) {
        (void)unlink(s_candidate_path);
        s_candidate_path[0] = '\0';
        panel_status("Failed to start managed mesh helper", true);
        return false;
    }
    if (child == 0) {
        if (kind == TRANSFORM_PANEL_JOB_IMPORT) {
            execlp("python3",
                   "python3",
                   script,
                   "apply",
                   "--scene",
                   scene,
                   "--compiler",
                   compiler,
                   "--source",
                   source,
                   "--asset-id",
                   generated_id,
                   "--spawn-object-id",
                   generated_id,
                   "--default-mode",
                   "flat",
                   "--scale",
                   scale_text,
                   "--output-scene",
                   s_candidate_path,
                   (char*)NULL);
        } else {
            execlp("python3",
                   "python3",
                   script,
                   "apply",
                   "--scene",
                   scene,
                   "--compiler",
                   compiler,
                   "--object-id",
                   object_id,
                   "--shading",
                   shading,
                   "--crease-angle",
                   crease_text,
                   "--output-scene",
                   s_candidate_path,
                   (char*)NULL);
        }
        _exit(127);
    }
    s_job_pid = child;
    s_job_kind = kind;
    s_job_document_revision = SceneEditorDocumentRevision();
    panel_status(kind == TRANSFORM_PANEL_JOB_IMPORT ? "Compiling managed STL..."
                                                    : "Rebuilding shading variant...",
                 false);
    return true;
}

static bool panel_apply_shading(const char* mode) {
    char object_id[64] = {0};
    int selected = ObjectEditorGetSelectedObjectIndex();
    if (!runtime_scene_bridge_get_last_object_id_for_scene_index(selected,
                                                                 object_id,
                                                                 sizeof(object_id))) {
        panel_status("Selected object has no stable runtime ID", true);
        return false;
    }
    return panel_spawn_managed_job(TRANSFORM_PANEL_JOB_SHADING,
                                   NULL,
                                   object_id,
                                   mode,
                                   1.0,
                                   s_crease_angle_degrees);
}

int SceneEditorTransformPanelRender(SDL_Renderer* renderer,
                                    SDL_Rect bounds,
                                    int top_y,
                                    int bottom_y) {
    static const char* shading_labels[4] = {"Inherit", "Flat", "Smooth", "Crease"};
    SceneEditorDocumentTransform transform = {0};
    RayTracingThemePalette palette = SceneEditorChromeShellResolvePalette();
    char diagnostics[256] = {0};
    char line[128] = {0};
    int selected = ObjectEditorGetSelectedObjectIndex();
    int y = top_y;
    int gap = 4;
    int field_h = 25;
    int cell_w = (bounds.w - gap * 2) / 3;
    bool editable = SceneEditorDocumentIsOpen() && selected >= 0 && s_job_pid <= 0;
    s_controls_active = false;
    memset(s_fields, 0, sizeof(s_fields));
    memset(&s_name_field, 0, sizeof(s_name_field));
    memset(&s_undo_button, 0, sizeof(s_undo_button));
    memset(&s_redo_button, 0, sizeof(s_redo_button));
    memset(&s_duplicate_button, 0, sizeof(s_duplicate_button));
    memset(&s_remove_button, 0, sizeof(s_remove_button));
    memset(s_import_unit_buttons, 0, sizeof(s_import_unit_buttons));
    memset(&s_import_button, 0, sizeof(s_import_button));
    memset(&s_crease_angle_field, 0, sizeof(s_crease_angle_field));
    memset(s_shading_buttons, 0, sizeof(s_shading_buttons));
    if (!renderer || bounds.w < 120 || bottom_y - top_y < 270 ||
        !SceneEditorDocumentIsOpen()) return top_y;

    snprintf(line,
             sizeof(line),
             "Document Inspector%s",
             SceneEditorDocumentIsDirty() ? " *" : "");
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){bounds.x, y, bounds.w, 20},
                        line,
                        palette.text_primary);
    y += 23;
    if (selected < 0 ||
        !SceneEditorDocumentGetTransformForSceneIndex(selected,
                                                      &transform,
                                                      diagnostics,
                                                      sizeof(diagnostics))) {
        RenderLabelTextWrappedLeft(renderer,
                                   (SDL_Rect){bounds.x, y, bounds.w, 42},
                                   selected < 0 ? "Select a runtime object to edit its retained transform."
                                                : diagnostics,
                                   palette.text_muted);
        return y + 46;
    }
    s_controls_active = true;

    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            int index = row * 3 + column;
            double value = *panel_transform_component(&transform, index);
            s_fields[index] = (SDL_Rect){bounds.x + column * (cell_w + gap), y, cell_w, field_h};
            if (s_edit_field == index) {
                snprintf(line, sizeof(line), "%s %s", panel_field_label(index), s_edit_buffer);
            } else {
                snprintf(line, sizeof(line), "%s %.3g", panel_field_label(index), value);
            }
            panel_draw_button(renderer, s_fields[index], line, editable, s_edit_field == index);
        }
        y += field_h + gap;
    }

    s_name_field = (SDL_Rect){bounds.x, y, bounds.w, field_h};
    snprintf(line,
             sizeof(line),
             "Name: %s",
             s_edit_name ? s_edit_buffer : "click to rename");
    panel_draw_button(renderer, s_name_field, line, editable, s_edit_name);
    y += field_h + gap;

    s_undo_button = (SDL_Rect){bounds.x, y, (bounds.w - gap) / 2, field_h};
    s_redo_button = (SDL_Rect){s_undo_button.x + s_undo_button.w + gap,
                               y,
                               bounds.w - s_undo_button.w - gap,
                               field_h};
    panel_draw_button(renderer,
                      s_undo_button,
                      "Undo",
                      editable && SceneEditorDocumentCanUndo(),
                      false);
    panel_draw_button(renderer,
                      s_redo_button,
                      "Redo",
                      editable && SceneEditorDocumentCanRedo(),
                      false);
    y += field_h + gap;

    s_duplicate_button = (SDL_Rect){bounds.x, y, (bounds.w - gap) / 2, field_h};
    s_remove_button = (SDL_Rect){s_duplicate_button.x + s_duplicate_button.w + gap,
                                 y,
                                 bounds.w - s_duplicate_button.w - gap,
                                 field_h};
    panel_draw_button(renderer, s_duplicate_button, "Duplicate", editable, false);
    panel_draw_button(renderer, s_remove_button, "Remove", editable, false);
    y += field_h + gap;

    s_import_unit_buttons[0] = (SDL_Rect){bounds.x, y, (bounds.w - gap) / 2, field_h};
    s_import_unit_buttons[1] = (SDL_Rect){s_import_unit_buttons[0].x + s_import_unit_buttons[0].w + gap,
                                         y,
                                         bounds.w - s_import_unit_buttons[0].w - gap,
                                         field_h};
    panel_draw_button(renderer,
                      s_import_unit_buttons[0],
                      "Source: meters",
                      s_job_pid <= 0,
                      s_import_scale == 1.0);
    panel_draw_button(renderer,
                      s_import_unit_buttons[1],
                      "Source: mm",
                      s_job_pid <= 0,
                      s_import_scale == 0.001);
    y += field_h + gap;

    s_import_button = (SDL_Rect){bounds.x, y, bounds.w, field_h};
    panel_draw_button(renderer,
                      s_import_button,
                      s_picker.active ? "STL picker open" : (s_job_pid > 0 ? "Managed mesh busy" : "Import STL"),
                      SceneEditorDocumentIsOpen() && s_job_pid <= 0,
                      s_picker.active);
    y += field_h + gap;

    s_crease_angle_field = (SDL_Rect){bounds.x, y, bounds.w, field_h};
    snprintf(line,
             sizeof(line),
             "Crease angle: %s",
             s_edit_field == PANEL_EDIT_CREASE_ANGLE ? s_edit_buffer : "click to edit");
    if (s_edit_field != PANEL_EDIT_CREASE_ANGLE) {
        snprintf(line, sizeof(line), "Crease angle: %.3g deg", s_crease_angle_degrees);
    }
    panel_draw_button(renderer,
                      s_crease_angle_field,
                      line,
                      editable,
                      s_edit_field == PANEL_EDIT_CREASE_ANGLE);
    y += field_h + gap;

    for (int i = 0; i < 4; ++i) {
        int button_w = (bounds.w - gap * 3) / 4;
        s_shading_buttons[i] = (SDL_Rect){bounds.x + i * (button_w + gap), y, button_w, field_h};
        panel_draw_button(renderer, s_shading_buttons[i], shading_labels[i], editable, false);
    }
    y += field_h + gap;
    if (s_status[0] && y < bottom_y) {
        RenderLabelTextWrappedLeft(renderer,
                                   (SDL_Rect){bounds.x, y, bounds.w, bottom_y - y},
                                   s_status,
                                   s_status_color);
        y += 35;
    }
    return y + 4;
}

bool SceneEditorTransformPanelHandleEvent(const SDL_Event* event) {
    static const char* shading_modes[4] = {"inherit", "flat", "smooth", "crease_aware"};
    char diagnostics[256] = {0};
    int selected = ObjectEditorGetSelectedObjectIndex();
    if (!event || !SceneEditorDocumentIsOpen() || !s_controls_active) return false;
    if (event->type == SDL_KEYDOWN &&
        (event->key.keysym.mod & (KMOD_CTRL | KMOD_GUI)) != 0) {
        if (event->key.keysym.sym == SDLK_z && panel_mutation_allowed()) {
            bool redo = (event->key.keysym.mod & KMOD_SHIFT) != 0;
            bool ok = redo ? SceneEditorDocumentRedo(diagnostics, sizeof(diagnostics))
                           : SceneEditorDocumentUndo(diagnostics, sizeof(diagnostics));
            panel_status(ok ? (redo ? "Redo applied" : "Undo applied") : diagnostics, !ok);
            return true;
        }
        if (event->key.keysym.sym == SDLK_y && panel_mutation_allowed()) {
            bool ok = SceneEditorDocumentRedo(diagnostics, sizeof(diagnostics));
            panel_status(ok ? "Redo applied" : diagnostics, !ok);
            return true;
        }
    }
    if (s_edit_field >= 0 || s_edit_name) {
        if (event->type == SDL_TEXTINPUT) {
            size_t used = strlen(s_edit_buffer);
            size_t incoming = strlen(event->text.text);
            if (used + incoming < sizeof(s_edit_buffer)) {
                if (s_edit_name) {
                    memcpy(s_edit_buffer + used, event->text.text, incoming + 1u);
                } else {
                    for (size_t i = 0; i < incoming; ++i) {
                        char c = event->text.text[i];
                        if (isdigit((unsigned char)c) || c == '.' || c == '-' || c == '+' ||
                            c == 'e' || c == 'E') {
                            s_edit_buffer[used++] = c;
                        }
                    }
                    s_edit_buffer[used] = '\0';
                }
            }
            return true;
        }
        if (event->type == SDL_KEYDOWN) {
            if (event->key.keysym.sym == SDLK_BACKSPACE) {
                size_t used = strlen(s_edit_buffer);
                if (used > 0u) s_edit_buffer[used - 1u] = '\0';
                return true;
            }
            if (event->key.keysym.sym == SDLK_RETURN || event->key.keysym.sym == SDLK_KP_ENTER) {
                (void)panel_commit_edit();
                return true;
            }
            if (event->key.keysym.sym == SDLK_ESCAPE) {
                panel_cancel_edit();
                return true;
            }
        }
    }
    if (event->type != SDL_MOUSEBUTTONDOWN || event->button.button != SDL_BUTTON_LEFT) {
        return false;
    }
    for (int i = 0; i < TRANSFORM_FIELD_COUNT; ++i) {
        if (panel_point_in_rect(event->button.x, event->button.y, &s_fields[i]) &&
            panel_mutation_allowed()) {
            SceneEditorDocumentTransform transform = {0};
            if (!SceneEditorDocumentGetTransformForSceneIndex(selected,
                                                              &transform,
                                                              diagnostics,
                                                              sizeof(diagnostics))) {
                panel_status(diagnostics, true);
                return true;
            }
            s_edit_field = i;
            s_edit_name = false;
            snprintf(s_edit_buffer,
                     sizeof(s_edit_buffer),
                     "%.9g",
                     *panel_transform_component(&transform, i));
            SDL_StartTextInput();
            return true;
        }
    }
    if (panel_point_in_rect(event->button.x, event->button.y, &s_name_field) &&
        panel_mutation_allowed()) {
        s_edit_field = -1;
        s_edit_name = true;
        s_edit_buffer[0] = '\0';
        SDL_StartTextInput();
        return true;
    }
    if (panel_point_in_rect(event->button.x, event->button.y, &s_undo_button)) {
        bool ok = panel_mutation_allowed() &&
                  SceneEditorDocumentUndo(diagnostics, sizeof(diagnostics));
        panel_status(ok ? "Undo applied" : diagnostics, !ok);
        return true;
    }
    if (panel_point_in_rect(event->button.x, event->button.y, &s_redo_button)) {
        bool ok = panel_mutation_allowed() &&
                  SceneEditorDocumentRedo(diagnostics, sizeof(diagnostics));
        panel_status(ok ? "Redo applied" : diagnostics, !ok);
        return true;
    }
    if (panel_point_in_rect(event->button.x, event->button.y, &s_duplicate_button)) {
        int new_index = -1;
        bool ok = panel_mutation_allowed() &&
                  SceneEditorDocumentDuplicateForSceneIndex(selected,
                                                             &new_index,
                                                             diagnostics,
                                                             sizeof(diagnostics));
        if (ok) ObjectEditorSetSelectedObjectIndex(new_index);
        panel_status(ok ? "Object duplicated" : diagnostics, !ok);
        return true;
    }
    if (panel_point_in_rect(event->button.x, event->button.y, &s_remove_button)) {
        bool ok = panel_mutation_allowed() &&
                  SceneEditorDocumentRemoveForSceneIndex(selected,
                                                          diagnostics,
                                                          sizeof(diagnostics));
        if (ok) ObjectEditorSetSelectedObjectIndex(-1);
        panel_status(ok ? "Object removed" : diagnostics, !ok);
        return true;
    }
    if (panel_point_in_rect(event->button.x, event->button.y, &s_import_button)) {
        char initial[PATH_MAX] = {0};
        if (!panel_mutation_allowed()) return true;
        if (!s_picker_initialized) {
            RayTracing_FolderPicker_RequestInit(&s_picker);
            s_picker_initialized = true;
        }
        (void)ray_tracing_resolve_import_dir(initial, sizeof(initial));
        if (!RayTracing_FilePicker_Begin(&s_picker,
                                         "Select STL mesh",
                                         initial[0] ? initial : NULL)) {
            panel_status("STL picker unavailable", true);
        } else {
            panel_status("STL picker open", false);
        }
        return true;
    }
    for (int i = 0; i < 2; ++i) {
        if (panel_point_in_rect(event->button.x,
                                event->button.y,
                                &s_import_unit_buttons[i]) &&
            panel_mutation_allowed()) {
            s_import_scale = i == 0 ? 1.0 : 0.001;
            panel_status(i == 0 ? "STL source units set to meters"
                                : "STL source units set to millimeters",
                         false);
            return true;
        }
    }
    if (panel_point_in_rect(event->button.x, event->button.y, &s_crease_angle_field) &&
        panel_mutation_allowed()) {
        s_edit_field = PANEL_EDIT_CREASE_ANGLE;
        s_edit_name = false;
        snprintf(s_edit_buffer, sizeof(s_edit_buffer), "%.9g", s_crease_angle_degrees);
        SDL_StartTextInput();
        return true;
    }
    for (int i = 0; i < 4; ++i) {
        if (panel_point_in_rect(event->button.x, event->button.y, &s_shading_buttons[i])) {
            (void)panel_apply_shading(shading_modes[i]);
            return true;
        }
    }
    return false;
}

bool SceneEditorTransformPanelPoll(void) {
    bool changed = false;
    if (s_picker.active) {
        char selected[PATH_MAX] = {0};
        RayTracingFolderPickerResult result = RayTracing_FolderPicker_Poll(&s_picker,
                                                                           selected,
                                                                           sizeof(selected));
        if (result == RAY_TRACING_FOLDER_PICKER_SELECTED) {
            const char* extension = strrchr(selected, '.');
            if (!extension || strcasecmp(extension, ".stl") != 0) {
                panel_status("Select an STL file", true);
            } else {
                (void)panel_spawn_managed_job(TRANSFORM_PANEL_JOB_IMPORT,
                                              selected,
                                              NULL,
                                              NULL,
                                              s_import_scale,
                                              s_crease_angle_degrees);
            }
            changed = true;
        } else if (result == RAY_TRACING_FOLDER_PICKER_CANCELLED) {
            panel_status("STL import cancelled", false);
            changed = true;
        } else if (result == RAY_TRACING_FOLDER_PICKER_FAILED ||
                   result == RAY_TRACING_FOLDER_PICKER_UNAVAILABLE) {
            panel_status("STL picker failed", true);
            changed = true;
        }
    }
    if (s_job_pid > 0) {
        int status = 0;
        pid_t result = waitpid(s_job_pid, &status, WNOHANG);
        if (result == s_job_pid) {
            char diagnostics[256] = {0};
            bool success = WIFEXITED(status) && WEXITSTATUS(status) == 0;
            if (success && SceneEditorDocumentRevision() != s_job_document_revision) {
                snprintf(diagnostics,
                         sizeof(diagnostics),
                         "document changed while managed mesh was compiling; retry");
                success = false;
            }
            if (success) {
                success = SceneEditorDocumentAdoptCandidateAsCommand(s_candidate_path,
                                                                      diagnostics,
                                                                      sizeof(diagnostics));
            }
            if (s_candidate_path[0]) {
                (void)unlink(s_candidate_path);
                s_candidate_path[0] = '\0';
            }
            panel_status(success ? (s_job_kind == TRANSFORM_PANEL_JOB_IMPORT
                                        ? "Managed STL imported"
                                        : "Shading variant rebuilt")
                                 : (diagnostics[0] ? diagnostics : "Managed mesh operation failed"),
                         !success);
            s_job_pid = -1;
            s_job_kind = TRANSFORM_PANEL_JOB_NONE;
            s_job_document_revision = 0u;
            changed = true;
        }
    }
    return changed;
}

bool SceneEditorTransformPanelInteractionActive(void) {
    return s_edit_field >= 0 || s_edit_name || s_picker.active || s_job_pid > 0;
}

void SceneEditorTransformPanelReset(void) {
    panel_cancel_edit();
    if (s_picker_initialized && s_picker.active) {
        RayTracing_FolderPicker_Cancel(&s_picker);
    }
    if (s_job_pid > 0) {
        (void)kill(s_job_pid, SIGTERM);
        (void)waitpid(s_job_pid, NULL, 0);
    }
    if (s_candidate_path[0]) {
        (void)unlink(s_candidate_path);
        s_candidate_path[0] = '\0';
    }
    memset(s_fields, 0, sizeof(s_fields));
    memset(&s_name_field, 0, sizeof(s_name_field));
    memset(s_import_unit_buttons, 0, sizeof(s_import_unit_buttons));
    memset(&s_import_button, 0, sizeof(s_import_button));
    memset(&s_crease_angle_field, 0, sizeof(s_crease_angle_field));
    memset(s_shading_buttons, 0, sizeof(s_shading_buttons));
    s_job_pid = -1;
    s_job_kind = TRANSFORM_PANEL_JOB_NONE;
    s_job_document_revision = 0u;
    s_status[0] = '\0';
    s_controls_active = false;
}
