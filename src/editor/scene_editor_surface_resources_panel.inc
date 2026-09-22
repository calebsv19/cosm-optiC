/* Image resources are prepared by document commands, never by the drawing path. */
#include "editor/material_editor.h"
#include "platform/ray_tracing_folder_picker.h"
static const char *resource_channels[] = {"base_color", "roughness", "normal", "height"};
static const char *resource_labels[] = {"Base color", "Roughness", "Normal", "Height"};
static const char *resource_properties[] = {"normal_strength", "height_m", "period_u", "period_v"};
static const char *resource_property_labels[] = {"Normal strength [0..2]", "Height meters [0..1]",
                                                 "Period U [1..32]", "Period V [1..32]"};
static SDL_Rect resource_bounds, resource_cards[4], resource_actions[4], resource_fields[4],
    resource_disable, resource_picker_cancel, resource_adopt;
static bool resource_visible, resource_draft_selected;
static int resource_channel, resource_scroll, resource_rows, resource_edit = -1,
                                                             resource_target = -1;
static double resource_values[4];
static char resource_draft[80], resource_health[4][256], resource_encoding[16] = "srgb";
static unsigned long long resource_edit_revision, resource_health_revision;
static RayTracingFolderPickerRequest resource_picker = {.output_fd = -1};
static int resource_picker_target = -1, resource_picker_channel;
static bool resource_picker_candidate;
static char resource_picker_id[64], resource_picker_encoding[16];
static char resource_picker_scene[RAY_TRACING_FOLDER_PICKER_PATH_CAPACITY];
static unsigned long long resource_picker_revision;

static void resource_invalidate(void) {
    resource_visible = false;
    resource_bounds = resource_disable = resource_picker_cancel = resource_adopt = (SDL_Rect){0};
    memset(resource_cards, 0, sizeof(resource_cards));
    memset(resource_actions, 0, sizeof(resource_actions));
    memset(resource_fields, 0, sizeof(resource_fields));
}
static void resource_cancel_edit(void) {
    if (resource_edit >= 0)
        SDL_StopTextInput();
    resource_edit = -1;
    resource_draft_selected = false;
}
static void resource_reset(void) {
    resource_cancel_edit();
    resource_invalidate();
    resource_scroll = 0;
    resource_channel = 0;
    memset(resource_health, 0, sizeof(resource_health));
    resource_target = -1;
    resource_health_revision = 0;
}
static json_object *resource_sampling(int index) {
    size_t size = SceneEditorDocumentSurfaceSamplingJSONSize(index);
    if (!size)
        return NULL;
    char *text = malloc(size);
    if (!text)
        return NULL;
    json_object *result = SceneEditorDocumentGetSurfaceSamplingJSON(index, text, size)
                              ? json_tokener_parse(text)
                              : NULL;
    free(text);
    return result;
}
static bool resource_eligible(int index, json_object *row) {
    char diagnostic[256];
    if (!SceneEditorDocumentSurfaceSamplingSupported(index, diagnostic, sizeof(diagnostic)))
        return false;
    if (!RuntimeSurfaceMappingActive(index) || RuntimeSurfaceGraphActive(index))
        return false;
    const char *blocked[] = {
        "surface_graph",    "material_graph",           "materialGraph",
        "authored_texture", "surface_material_binding", "procedural_solid_material_ref"};
    for (size_t i = 0; i < sizeof(blocked) / sizeof(blocked[0]); ++i)
        if (get(row, blocked[i]))
            return false;
    json_object *stack = get(row, "material_texture_stack");
    if (!stack)
        stack = get(row, "materialTextureStack");
    json_object *list = get(stack, "layers");
    if (!json_object_is_type(list, json_type_array))
        return false;
    for (size_t i = 0; i < json_object_array_length(list); ++i) {
        const char *kind = json_object_get_string(get(json_object_array_get_idx(list, i), "kind"));
        if (!kind || (strcmp(kind, "brick") && strcmp(kind, "solid")))
            return false;
    }
    return true;
}
static void resource_row(SDL_Renderer *r, SDL_Rect b, int line, SDL_Rect *control, const char *text,
                         bool active) {
    int y = b.y + (line - resource_scroll) * 27;
    *control = (SDL_Rect){0};
    if (line >= resource_scroll && y + 24 <= b.y + b.h) {
        *control = (SDL_Rect){b.x, y, b.w, 24};
        draw(r, *control, text, active);
    }
}
static int resource_render(SDL_Renderer *r, SDL_Rect b, int index) {
    resource_invalidate();
    /* Sidebar layout measures in a tall virtual rectangle; only the clip is interactive. */
    if (SDL_RenderIsClipEnabled(r)) {
        SDL_Rect clip, visible;
        SDL_RenderGetClipRect(r, &clip);
        if (!SDL_IntersectRect(&b, &clip, &visible))
            return b.y;
        b = visible;
    }
    resource_visible = true;
    resource_bounds = b;
    if (resource_target != index) {
        resource_reset();
        resource_target = index;
        resource_visible = true;
        resource_bounds = b;
    }
    if (resource_health_revision != SceneEditorDocumentRevision()) {
        memset(resource_health, 0, sizeof(resource_health));
        resource_health_revision = SceneEditorDocumentRevision();
    }
    json_object *sampling = resource_sampling(index), *channels = get(sampling, "channels");
    int prior_max = resource_rows - b.h / 27;
    if (prior_max < 0)
        prior_max = 0;
    if (resource_scroll > prior_max)
        resource_scroll = prior_max;
    int line = 0;
    SDL_Rect label;
    char text[256];
    resource_row(r, b, line++, &label,
                 sampling ? "Filtered image channels" : "Image channels: select a PNG to enable",
                 false);
    for (int c = 0; c < 4; ++c) {
        snprintf(text, sizeof(text), "%s: %s", resource_labels[c],
                 get(channels, resource_channels[c]) ? "assigned" : "not assigned");
        resource_row(r, b, line++, &resource_cards[c], text, resource_channel == c);
    }
    json_object *entry = get(channels, resource_channels[resource_channel]);
    const char *path = json_object_get_string(get(entry, "path")),
               *digest = json_object_get_string(get(entry, "sha256")),
               *encoding = json_object_get_string(get(entry, "color_space"));
    if (encoding)
        snprintf(resource_encoding, sizeof(resource_encoding), "%s", encoding);
    else if (resource_channel)
        snprintf(resource_encoding, sizeof(resource_encoding), "data");
    else if (strcmp(resource_encoding, "linear") && strcmp(resource_encoding, "srgb"))
        snprintf(resource_encoding, sizeof(resource_encoding), "srgb");
    snprintf(text, sizeof(text), "File: %.220s", path ? path : "none");
    resource_row(r, b, line++, &label, text, false);
    if (digest) {
        snprintf(text, sizeof(text), "SHA256: %.32s", digest);
        resource_row(r, b, line++, &label, text, false);
        snprintf(text, sizeof(text), "        %.32s", strlen(digest) > 32 ? digest + 32 : "");
        resource_row(r, b, line++, &label, text, false);
    }
    snprintf(text, sizeof(text), "Encoding: %s%s", resource_encoding,
             resource_channel ? " (data channel)" : "  >");
    resource_row(r, b, line++, &resource_actions[3], text, false);
    resource_row(r, b, line++, &resource_actions[0],
                 path ? "Relink / replace PNG..." : "Select PNG...", false);
    if (path) {
        resource_row(r, b, line++, &resource_actions[1], "Remove channel", false);
        resource_row(r, b, line++, &resource_actions[2], "Check pinned file", false);
    }
    const char *health = resource_health[resource_channel][0] ? resource_health[resource_channel]
                         : path                               ? "Health: not rechecked; use Check"
                                                              : "No file assigned";
    resource_row(r, b, line++, &label, health, false);
    if ((resource_channel == 2 && get(channels, "height")) ||
        (resource_channel == 3 && get(channels, "normal")))
        resource_row(r, b, line++, &label, "Remove the other normal/height channel first", false);
    json_object *period = get(sampling, "period_tiles");
    resource_values[0] = get(sampling, "normal_strength")
                             ? json_object_get_double(get(sampling, "normal_strength"))
                             : 1;
    resource_values[1] = json_object_get_double(get(sampling, "height_m"));
    resource_values[2] = period ? json_object_get_double(json_object_array_get_idx(period, 0)) : 8;
    resource_values[3] = period ? json_object_get_double(json_object_array_get_idx(period, 1)) : 8;
    for (int i = 0; i < 4; ++i) {
        if ((i == 0 && resource_channel != 2) || (i == 1 && resource_channel != 3))
            continue;
        snprintf(text, sizeof(text), "%s: %.9g", resource_property_labels[i], resource_values[i]);
        if (resource_edit == i)
            snprintf(text, sizeof(text), "%s: %s", resource_property_labels[i], resource_draft);
        resource_row(r, b, line++, &resource_fields[i], text, resource_edit == i);
    }
    if (sampling)
        resource_row(r, b, line++, &resource_disable, "Remove image sampling declaration", false);
    resource_row(r, b, line++, &resource_adopt, "Adopt reviewed UV candidate...", false);
    if (resource_picker.active)
        resource_row(r, b, line++, &resource_picker_cancel, "Cancel open file chooser", false);
    if (status[0])
        resource_row(r, b, line++, &label, status, false);
    resource_rows = line;
    int maximum = line - b.h / 27;
    if (maximum < 0)
        maximum = 0;
    if (resource_scroll > maximum)
        resource_scroll = maximum;
    if (sampling)
        json_object_put(sampling);
    return b.y + b.h;
}
static bool resource_active(void) { return resource_edit >= 0 || resource_picker.active; }
static void resource_release(const SDL_Event *event) {
    if (resource_edit < 0)
        return;
    if ((event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_FOCUS_LOST) ||
        (event->type == SDL_MOUSEBUTTONDOWN &&
         !inside(resource_fields[resource_edit], event->button.x, event->button.y)))
        resource_cancel_edit();
}
static void resource_check(int index, int channel) {
    if (resource_health_revision != SceneEditorDocumentRevision())
        memset(resource_health, 0, sizeof(resource_health));
    bool ok = SceneEditorDocumentSurfaceSamplingCheckChannel(index, resource_channels[channel],
                                                             resource_health[channel],
                                                             sizeof(resource_health[channel]));
    if (ok && !resource_health[channel][0])
        snprintf(resource_health[channel], sizeof(resource_health[channel]),
                 "Pinned file verified");
    resource_health_revision = SceneEditorDocumentRevision();
}
static void resource_picker_cancelled(const char *message) {
    RayTracing_FolderPicker_Cancel(&resource_picker);
    resource_picker_target = -1;
    resource_picker_id[0] = 0;
    snprintf(status, sizeof(status), "%s", message);
}
bool SceneEditorSurfaceMaterialPanelPoll(void) {
    if (!resource_picker.active)
        return false;
    SceneEditorDocumentObjectInfo info;
    if (resource_picker_revision != SceneEditorDocumentRevision() || !SceneEditorDocumentPath() ||
        strcmp(resource_picker_scene, SceneEditorDocumentPath()) ||
        MaterialEditorResolveFocusedObjectIndex() != resource_picker_target ||
        !assignment_object(resource_picker_target, &info) || strcmp(info.id, resource_picker_id)) {
        resource_picker_cancelled("Material target changed; file selection cancelled");
        return true;
    }
    char path[RAY_TRACING_FOLDER_PICKER_PATH_CAPACITY];
    RayTracingFolderPickerResult result =
        RayTracing_FolderPicker_Poll(&resource_picker, path, sizeof(path));
    if (result == RAY_TRACING_FOLDER_PICKER_PENDING)
        return false;
    if (result == RAY_TRACING_FOLDER_PICKER_SELECTED) {
        if (RayTracingDeepRenderDesktopHost_HasActiveWork())
            snprintf(status, sizeof(status), "Rendering started; file selection was not applied");
        else if (resource_picker_candidate) {
            char receipt[RAY_TRACING_FOLDER_PICKER_PATH_CAPACITY];
            if (snprintf(receipt, sizeof(receipt), "%s.receipt.json", path) >= (int)sizeof(receipt))
                snprintf(status, sizeof(status), "Candidate receipt path is too long");
            else if (SceneEditorDocumentAdoptSurfaceCandidate(path, receipt, status,
                                                              sizeof(status))) {
                resource_reset();
                snprintf(status, sizeof(status), "Reviewed UV candidate adopted");
            }
        } else if (SceneEditorDocumentSurfaceSamplingSetChannel(
                       resource_picker_target, resource_channels[resource_picker_channel], path,
                       resource_picker_encoding, resource_picker_revision, status,
                       sizeof(status))) {
            resource_check(resource_picker_target, resource_picker_channel);
            snprintf(status, sizeof(status), "Image channel updated; Undo available");
        }
    } else
        snprintf(status, sizeof(status), "%s",
                 result == RAY_TRACING_FOLDER_PICKER_CANCELLED
                     ? "File selection cancelled"
                     : "File chooser failed or unavailable");
    resource_picker_target = -1;
    resource_picker_id[0] = 0;
    return true;
}
void SceneEditorSurfaceMaterialPanelShutdown(void) {
    RayTracing_FolderPicker_Cancel(&resource_picker);
    resource_reset();
}
static bool resource_event(const SDL_Event *event, int index) {
    resource_release(event);
    if (resource_picker.active && event->type == SDL_KEYDOWN &&
        event->key.keysym.sym == SDLK_ESCAPE) {
        resource_picker_cancelled("File selection cancelled");
        return true;
    }
    if (!resource_visible)
        return false;
    if (resource_edit >= 0) {
        if (resource_edit_revision != SceneEditorDocumentRevision()) {
            resource_cancel_edit();
            snprintf(status, sizeof(status), "Document changed; edit cancelled");
            return true;
        }
        if (event->type == SDL_TEXTINPUT) {
            if (resource_draft_selected) {
                resource_draft[0] = 0;
                resource_draft_selected = false;
            }
            if (strlen(resource_draft) + strlen(event->text.text) < sizeof(resource_draft))
                strcat(resource_draft, event->text.text);
            return true;
        }
        if (event->type == SDL_KEYDOWN) {
            if (event->key.keysym.sym == SDLK_ESCAPE) {
                resource_cancel_edit();
                return true;
            }
            if (event->key.keysym.sym == SDLK_a &&
                (event->key.keysym.mod & (KMOD_GUI | KMOD_CTRL))) {
                resource_draft_selected = true;
                return true;
            }
            if (event->key.keysym.sym == SDLK_BACKSPACE) {
                if (resource_draft_selected) {
                    resource_draft[0] = 0;
                    resource_draft_selected = false;
                } else {
                    size_t n = strlen(resource_draft);
                    if (n)
                        resource_draft[n - 1] = 0;
                }
                return true;
            }
            if (event->key.keysym.sym == SDLK_RETURN) {
                char *end;
                double value = strtod(resource_draft, &end);
                if (end == resource_draft || *end || !isfinite(value)) {
                    snprintf(status, sizeof(status), "Enter a finite numeric value");
                    return true;
                }
                if (RayTracingDeepRenderDesktopHost_HasActiveWork()) {
                    snprintf(status, sizeof(status), "Wait for rendering before editing");
                    return true;
                }
                if (SceneEditorDocumentSurfaceSamplingSetValue(
                        index, resource_properties[resource_edit], value, resource_edit_revision,
                        status, sizeof(status))) {
                    resource_cancel_edit();
                    snprintf(status, sizeof(status), "Sampling updated; Undo available");
                }
                return true;
            }
            return true;
        }
    }
    if (event->type == SDL_MOUSEWHEEL) {
        int x, y;
        SDL_GetMouseState(&x, &y);
        if (!inside(resource_bounds, x, y))
            return false;
        resource_cancel_edit();
        resource_scroll -= event->wheel.y;
        int maximum = resource_rows - resource_bounds.h / 27;
        if (maximum < 0)
            maximum = 0;
        if (resource_scroll < 0)
            resource_scroll = 0;
        if (resource_scroll > maximum)
            resource_scroll = maximum;
        return true;
    }
    if (event->type != SDL_MOUSEBUTTONDOWN || event->button.button != SDL_BUTTON_LEFT)
        return false;
    int x = event->button.x, y = event->button.y;
    if (inside(resource_picker_cancel, x, y)) {
        resource_picker_cancelled("File selection cancelled");
        return true;
    }
    if (inside(resource_adopt, x, y)) {
        if (resource_picker.active)
            snprintf(status, sizeof(status), "A file chooser is already open");
        else if (SceneEditorDocumentIsDirty())
            snprintf(status, sizeof(status),
                     "Save the current scene before adopting a UV candidate");
        else if (RayTracingDeepRenderDesktopHost_HasActiveWork())
            snprintf(status, sizeof(status), "Wait for rendering before adopting a candidate");
        else {
            SceneEditorDocumentObjectInfo info;
            if (assignment_object(index, &info) &&
                RayTracing_FilePicker_Begin(&resource_picker, "Adopt reviewed UV candidate JSON",
                                            SceneEditorDocumentPath())) {
                resource_picker_candidate = true;
                resource_picker_target = index;
                resource_picker_revision = SceneEditorDocumentRevision();
                snprintf(resource_picker_id, sizeof(resource_picker_id), "%s", info.id);
                snprintf(resource_picker_scene, sizeof(resource_picker_scene), "%s",
                         SceneEditorDocumentPath());
                snprintf(status, sizeof(status),
                         "Choose reviewed candidate JSON with its receipt; Escape cancels");
            } else
                snprintf(status, sizeof(status), "File chooser unavailable");
        }
        return true;
    }
    for (int c = 0; c < 4; ++c)
        if (inside(resource_cards[c], x, y)) {
            resource_channel = c;
            resource_scroll = 0;
            resource_cancel_edit();
            return true;
        }
    if (RayTracingDeepRenderDesktopHost_HasActiveWork())
        return false;
    for (int i = 0; i < 4; ++i)
        if (inside(resource_fields[i], x, y)) {
            resource_edit = i;
            resource_edit_revision = SceneEditorDocumentRevision();
            resource_draft_selected = true;
            snprintf(resource_draft, sizeof(resource_draft), "%.9g", resource_values[i]);
            SDL_StartTextInput();
            return true;
        }
    if (inside(resource_disable, x, y)) {
        SceneEditorDocumentSetSurfaceSampling(index, NULL, SceneEditorDocumentRevision(), status,
                                              sizeof(status));
        return true;
    }
    for (int action = 0; action < 4; ++action)
        if (inside(resource_actions[action], x, y)) {
            json_object *sampling = resource_sampling(index),
                        *entry =
                            get(get(sampling, "channels"), resource_channels[resource_channel]);
            const char *path = json_object_get_string(get(entry, "path"));
            if (action == 0) {
                if (resource_picker.active)
                    snprintf(status, sizeof(status), "A file chooser is already open");
                else if ((resource_channel == 2 && get(get(sampling, "channels"), "height")) ||
                         (resource_channel == 3 && get(get(sampling, "channels"), "normal")))
                    snprintf(status, sizeof(status),
                             "Remove the other normal/height channel before assignment");
                else {
                    SceneEditorDocumentObjectInfo info;
                    if (assignment_object(index, &info) &&
                        RayTracing_FilePicker_Begin(&resource_picker,
                                                    "Select a PNG material channel", path)) {
                        resource_picker_target = index;
                        resource_picker_candidate = false;
                        resource_picker_channel = resource_channel;
                        resource_picker_revision = SceneEditorDocumentRevision();
                        snprintf(resource_picker_id, sizeof(resource_picker_id), "%s", info.id);
                        snprintf(resource_picker_scene, sizeof(resource_picker_scene), "%s",
                                 SceneEditorDocumentPath());
                        snprintf(resource_picker_encoding, sizeof(resource_picker_encoding), "%s",
                                 resource_encoding);
                        snprintf(status, sizeof(status), "PNG chooser open; Escape cancels");
                    } else
                        snprintf(status, sizeof(status), "File chooser unavailable");
                }
            } else if (action == 1)
                SceneEditorDocumentSurfaceSamplingSetChannel(
                    index, resource_channels[resource_channel], NULL, NULL,
                    SceneEditorDocumentRevision(), status, sizeof(status));
            else if (action == 2)
                resource_check(index, resource_channel);
            else if (!resource_channel) {
                const char *encoding = !strcmp(resource_encoding, "srgb") ? "linear" : "srgb";
                if (path)
                    SceneEditorDocumentSurfaceSamplingSetChannel(
                        index, resource_channels[0], path, encoding, SceneEditorDocumentRevision(),
                        status, sizeof(status));
                else
                    snprintf(resource_encoding, sizeof(resource_encoding), "%s", encoding);
            }
            if (sampling)
                json_object_put(sampling);
            return true;
        }
    return false;
}
static bool resource_control(const char *name, SDL_Rect *out) {
    if (!resource_visible || strncmp(name, "resource:", 9))
        return false;
    const char *key = name + 9;
    for (int i = 0; i < 4; ++i)
        if (!strcmp(key, resource_properties[i])) {
            *out = resource_fields[i];
            return out->w > 0;
        }
    if (!strcmp(key, "disable"))
        *out = resource_disable;
    else if (!strcmp(key, "adopt_candidate"))
        *out = resource_adopt;
    else if (!strcmp(key, "picker_cancel"))
        *out = resource_picker_cancel;
    else {
        const char *actions[] = {"select", "remove", "check", "encoding"};
        for (int c = 0; c < 4; ++c) {
            char token[80];
            snprintf(token, sizeof(token), "%s:card", resource_channels[c]);
            if (!strcmp(key, token)) {
                *out = resource_cards[c];
                return out->w > 0;
            }
        }
        for (int a = 0; a < 4; ++a) {
            char token[80];
            snprintf(token, sizeof(token), "%s:%s", resource_channels[resource_channel],
                     actions[a]);
            if (!strcmp(key, token)) {
                *out = resource_actions[a];
                return out->w > 0;
            }
        }
        return false;
    }
    return out->w > 0;
}
