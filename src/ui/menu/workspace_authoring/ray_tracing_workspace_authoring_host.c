#include "ui/menu/workspace_authoring/ray_tracing_workspace_authoring_host.h"

#include <string.h>
#include <errno.h>

#include "config/config_manager.h"
#include "core_font.h"
#include "core_theme.h"
#include "ui/shared_theme_font_adapter.h"

#include <stdlib.h>

#include "ui/menu/workspace_authoring/ray_tracing_surface_authoring_canvas.h"

enum {
    RAY_TRACING_AUTHORING_TEXT_ZOOM_STEP_MIN = -4,
    RAY_TRACING_AUTHORING_TEXT_ZOOM_STEP_MAX = 5
};

static CoreResult ray_tracing_workspace_authoring_invalid(const char* message) {
    CoreResult result = { CORE_ERR_INVALID_ARG, message };
    return result;
}

static int ray_tracing_workspace_authoring_text_zoom_step_clamp(int step) {
    if (step < RAY_TRACING_AUTHORING_TEXT_ZOOM_STEP_MIN) {
        return RAY_TRACING_AUTHORING_TEXT_ZOOM_STEP_MIN;
    }
    if (step > RAY_TRACING_AUTHORING_TEXT_ZOOM_STEP_MAX) {
        return RAY_TRACING_AUTHORING_TEXT_ZOOM_STEP_MAX;
    }
    return step;
}

static void ray_tracing_workspace_authoring_copy_text(char* dst,
                                                      size_t dst_size,
                                                      const char* src) {
    if (!dst || dst_size == 0u) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_size - 1u);
    dst[dst_size - 1u] = '\0';
}

static void ray_tracing_workspace_authoring_note_font_theme_status(
    RayTracingWorkspaceAuthoringHostState* host,
    const char* status) {
    if (!host || !status) return;
    ray_tracing_workspace_authoring_copy_text(host->font_theme_status,
                                              sizeof(host->font_theme_status),
                                              status);
    host->font_theme_status_active = 1u;
    host->font_theme_status_count += 1u;
}

static void ray_tracing_workspace_authoring_note_font_theme_changed(
    RayTracingWorkspaceAuthoringHostState* host,
    int needs_font_reload,
    int needs_theme_apply) {
    if (!host) return;
    host->font_theme_pending_changes = 1u;
    host->font_theme_change_count += 1u;
    if (needs_font_reload) {
        host->font_theme_needs_font_reload = 1u;
    }
    if (needs_theme_apply) {
        host->font_theme_needs_theme_apply = 1u;
    }
}

static void ray_tracing_workspace_authoring_capture_font_theme_baseline(
    RayTracingWorkspaceAuthoringHostState* host) {
    if (!host || host->font_theme_baseline_ready) return;
    host->baseline_text_zoom_step = animSettings.textZoomStep;
    if (!ray_tracing_shared_theme_current_preset(host->baseline_theme_preset,
                                                 sizeof(host->baseline_theme_preset))) {
        ray_tracing_workspace_authoring_copy_text(host->baseline_theme_preset,
                                                  sizeof(host->baseline_theme_preset),
                                                  "midnight_contrast");
    }
    if (!ray_tracing_shared_font_current_preset(host->baseline_font_preset,
                                                sizeof(host->baseline_font_preset))) {
        ray_tracing_workspace_authoring_copy_text(host->baseline_font_preset,
                                                  sizeof(host->baseline_font_preset),
                                                  "ide");
    }
    host->font_theme_baseline_ready = 1u;
    host->font_theme_pending_changes = 0u;
    host->font_theme_status_active = 0u;
    host->font_theme_status[0] = '\0';
}

static void ray_tracing_workspace_authoring_clear_font_theme_baseline(
    RayTracingWorkspaceAuthoringHostState* host) {
    if (!host) return;
    host->font_theme_baseline_ready = 0u;
    host->font_theme_pending_changes = 0u;
    host->font_theme_status_active = 0u;
    host->font_theme_status[0] = '\0';
}

static void ray_tracing_workspace_authoring_restore_font_theme_baseline(
    RayTracingWorkspaceAuthoringHostState* host) {
    if (!host || !host->font_theme_baseline_ready) return;
    animSettings.textZoomStep =
        ray_tracing_workspace_authoring_text_zoom_step_clamp(host->baseline_text_zoom_step);
    host->font_theme_needs_font_reload = 1u;
    if (host->baseline_theme_preset[0] &&
        ray_tracing_shared_theme_set_preset(host->baseline_theme_preset)) {
        host->font_theme_needs_theme_apply = 1u;
    }
    if (host->baseline_font_preset[0] &&
        ray_tracing_shared_font_set_preset(host->baseline_font_preset)) {
        host->font_theme_needs_font_reload = 1u;
    }
    ray_tracing_workspace_authoring_clear_font_theme_baseline(host);
}

static uint32_t ray_tracing_workspace_authoring_mod_bits(SDL_Keymod mods) {
    uint32_t bits = 0u;
    if ((mods & KMOD_SHIFT) != 0) bits |= KIT_WORKSPACE_AUTHORING_MOD_SHIFT;
    if ((mods & KMOD_ALT) != 0) bits |= KIT_WORKSPACE_AUTHORING_MOD_ALT;
    if ((mods & KMOD_CTRL) != 0) bits |= KIT_WORKSPACE_AUTHORING_MOD_CTRL;
    if ((mods & KMOD_GUI) != 0) bits |= KIT_WORKSPACE_AUTHORING_MOD_GUI;
    return bits;
}

static KitWorkspaceAuthoringKey ray_tracing_workspace_authoring_key_from_sdl_keysym(
    const SDL_Keysym* keysym) {
    if (!keysym) return KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    switch (keysym->scancode) {
        case SDL_SCANCODE_C:
            return KIT_WORKSPACE_AUTHORING_KEY_C;
        case SDL_SCANCODE_V:
            return KIT_WORKSPACE_AUTHORING_KEY_V;
        default:
            break;
    }
    switch (keysym->sym) {
        case SDLK_TAB:
            return KIT_WORKSPACE_AUTHORING_KEY_TAB;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            return KIT_WORKSPACE_AUTHORING_KEY_ENTER;
        case SDLK_ESCAPE:
            return KIT_WORKSPACE_AUTHORING_KEY_ESCAPE;
        case SDLK_h:
            return KIT_WORKSPACE_AUTHORING_KEY_H;
        case SDLK_v:
            return KIT_WORKSPACE_AUTHORING_KEY_V;
        case SDLK_x:
            return KIT_WORKSPACE_AUTHORING_KEY_X;
        case SDLK_BACKSPACE:
        case SDLK_DELETE:
            return KIT_WORKSPACE_AUTHORING_KEY_BACKSPACE;
        case SDLK_r:
            return KIT_WORKSPACE_AUTHORING_KEY_R;
        case SDLK_0:
        case SDLK_KP_0:
            return KIT_WORKSPACE_AUTHORING_KEY_DIGIT_0;
        case SDLK_1:
        case SDLK_KP_1:
            return KIT_WORKSPACE_AUTHORING_KEY_DIGIT_1;
        case SDLK_2:
        case SDLK_KP_2:
            return KIT_WORKSPACE_AUTHORING_KEY_DIGIT_2;
        case SDLK_3:
        case SDLK_KP_3:
            return KIT_WORKSPACE_AUTHORING_KEY_DIGIT_3;
        case SDLK_4:
        case SDLK_KP_4:
            return KIT_WORKSPACE_AUTHORING_KEY_DIGIT_4;
        case SDLK_5:
        case SDLK_KP_5:
            return KIT_WORKSPACE_AUTHORING_KEY_DIGIT_5;
        case SDLK_6:
        case SDLK_KP_6:
            return KIT_WORKSPACE_AUTHORING_KEY_DIGIT_6;
        case SDLK_c:
            return KIT_WORKSPACE_AUTHORING_KEY_C;
        case SDLK_z:
            return KIT_WORKSPACE_AUTHORING_KEY_Z;
        default:
            return KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    }
}

static void ray_tracing_workspace_authoring_note_consumed(
    RayTracingWorkspaceAuthoringHostState* host,
    int runtime_event) {
    if (!host) return;
    host->last_event_consumed = 1u;
    host->consumed_event_count += 1u;
    if (runtime_event) {
        host->captured_runtime_event_count += 1u;
    }
}

static bool ray_tracing_workspace_authoring_canvas_snapshot(
    RayTracingSurfaceAuthoringCanvasSnapshot* snapshot) {
    const char* path = getenv("RAY_TRACING_SURFACE_AUTHORING_CANVAS_PATH");
    if (!snapshot) return false;
    if (path && path[0] &&
        RayTracingSurfaceAuthoringCanvasSnapshot_LoadJsonFile(path, snapshot)) {
        return true;
    }
    return RayTracingSurfaceAuthoringCanvasSnapshot_DefaultCube(snapshot);
}

static CoreResult ray_tracing_workspace_authoring_document_failure(
    RayTracingWorkspaceAuthoringHostState* host,
    const ProceduralSurfaceAuthoringDocumentReport* report,
    const char* fallback) {
    if (host) {
        snprintf(host->document_status, sizeof(host->document_status), "%s",
                 report && report->message[0] ? report->message : fallback);
    }
    return ray_tracing_workspace_authoring_invalid(fallback);
}

static bool ray_tracing_workspace_authoring_document_target(
    ProceduralSurfaceAuthoringDocumentV1* document,
    const char* field,
    ProceduralSurfaceAuthoringDocumentRef** out_target) {
    if (!document || !field || !out_target) return false;
    *out_target = NULL;
    if (strcmp(field, "material_graph") == 0) *out_target = &document->material_graph;
    else if (strcmp(field, "surface_field_graph") == 0) *out_target = &document->surface_field_graph;
    else if (strcmp(field, "face_region_selector") == 0) *out_target = &document->face_region_selector;
    else if (strncmp(field, "attachment:", 11u) == 0) {
        for (size_t i = 0u; i < document->attachment_count; ++i) {
            if (strcmp(document->attachments[i].id, field + 11u) == 0) {
                *out_target = &document->attachments[i];
                break;
            }
        }
    }
    return *out_target != NULL;
}

void ray_tracing_workspace_authoring_host_set_document_path(
    RayTracingWorkspaceAuthoringHostState* host,
    const char* path) {
    if (!host) return;
    ray_tracing_workspace_authoring_copy_text(host->document_path,
                                              sizeof(host->document_path), path);
}

CoreResult ray_tracing_workspace_authoring_host_load_document(
    RayTracingWorkspaceAuthoringHostState* host) {
    ProceduralSurfaceAuthoringDocumentReport report;
    const char* env_path;
    if (!host) return ray_tracing_workspace_authoring_invalid("null authoring host");
    if (!host->document_path[0]) {
        env_path = getenv("RAY_TRACING_SURFACE_AUTHORING_DOCUMENT_PATH");
        if (env_path && env_path[0]) ray_tracing_workspace_authoring_host_set_document_path(host, env_path);
    }
    if (!host->document_path[0]) {
        return ray_tracing_workspace_authoring_document_failure(host, NULL,
                                                                "document path is not configured");
    }
    if (!ProceduralSurfaceAuthoringDocumentV1_ReadbackJsonFile(
            host->document_path, &host->document, &host->document_readback_plan, &report)) {
        host->document_loaded = 0u;
        return ray_tracing_workspace_authoring_document_failure(host, &report,
                                                                "document readback failed");
    }
    host->document_loaded = 1u;
    host->document_dirty = 0u;
    host->document_readback_count += 1u;
    snprintf(host->document_status, sizeof(host->document_status), "document readback ok");
    return core_result_ok();
}

CoreResult ray_tracing_workspace_authoring_host_save_document(
    RayTracingWorkspaceAuthoringHostState* host) {
    ProceduralSurfaceAuthoringDocumentReport report;
    ProceduralSurfaceAuthoringDocumentV1 readback;
    ProceduralSurfaceAuthoringDocumentCompilePlan plan;
    if (!host || !host->document_loaded || !host->document_path[0]) {
        return ray_tracing_workspace_authoring_document_failure(host, NULL,
                                                                "document save path is not configured");
    }
    if (!ProceduralSurfaceAuthoringDocumentV1_SaveJsonFileAtomic(
            host->document_path, &host->document, &report) ||
        !ProceduralSurfaceAuthoringDocumentV1_ReadbackJsonFile(
            host->document_path, &readback, &plan, &report)) {
        return ray_tracing_workspace_authoring_document_failure(host, &report,
                                                                "document save/readback failed");
    }
    host->document = readback;
    host->document_readback_plan = plan;
    host->document_dirty = 0u;
    host->document_save_count += 1u;
    host->document_readback_count += 1u;
    snprintf(host->document_status, sizeof(host->document_status), "document saved and read back");
    return core_result_ok();
}

CoreResult ray_tracing_workspace_authoring_host_replace_reference(
    RayTracingWorkspaceAuthoringHostState* host,
    const char* field,
    const ProceduralSurfaceAuthoringDocumentRef* replacement) {
    ProceduralSurfaceAuthoringDocumentV1 edited;
    ProceduralSurfaceAuthoringDocumentV1 undo;
    ProceduralSurfaceAuthoringDocumentReport report;
    ProceduralSurfaceAuthoringDocumentRef* target = NULL;
    char digest[PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_DIGEST_CAPACITY];
    const char *expected_document = NULL;
    const char *expected_source = NULL;
    const char *expected_reference = NULL;
    memset(&report, 0, sizeof(report));
    if (!host || !host->document_loaded || !replacement ||
        !ray_tracing_workspace_authoring_document_target(&host->document, field, &target) ||
        !ProceduralSurfaceAuthoringDocumentV1_Digest(&host->document, digest, &report)) {
        return ray_tracing_workspace_authoring_document_failure(host, &report,
                                                                "document edit rejected");
    }
    expected_document = (host->edit_active && strcmp(host->edit_field, field) == 0)
                            ? host->edit_expected_document_digest : digest;
    expected_source = (host->edit_active && strcmp(host->edit_field, field) == 0)
                          ? host->edit_expected_source_mesh_digest
                          : host->document.source_mesh_digest_sha256;
    expected_reference = (host->edit_active && strcmp(host->edit_field, field) == 0)
                             ? host->edit_expected_reference_digest : target->digest_sha256;
    if (!ProceduralSurfaceAuthoringDocumentV1_ReplaceReferenceTransactional(
            &host->document, field, replacement, expected_document,
            expected_source, expected_reference,
            &edited, &undo, &report)) {
        return ray_tracing_workspace_authoring_document_failure(host, &report,
                                                                "document edit rejected");
    }
    host->undo_document = undo;
    host->document = edited;
    host->document_dirty = 1u;
    host->document_edit_count += 1u;
    snprintf(host->document_status, sizeof(host->document_status), "document edit staged");
    return core_result_ok();
}

static bool ray_tracing_workspace_authoring_canvas_panel(
    const RayTracingWorkspaceAuthoringHostState* host,
    SDL_Rect* out_panel) {
    if (!host || !out_panel || host->viewport_width == 0u || host->viewport_height == 0u ||
        !ray_tracing_workspace_authoring_host_pane_overlay_active(host)) {
        return false;
    }
    if (host->canvas_panel_ready) {
        *out_panel = host->canvas_panel;
    } else {
        ray_tracing_surface_authoring_canvas_view_panel_for_viewport(
            (int)host->viewport_width, (int)host->viewport_height, out_panel);
    }
    return out_panel->w >= 420 && out_panel->h >= 240;
}

static bool ray_tracing_workspace_authoring_selected_field(
    const RayTracingWorkspaceAuthoringHostState* host,
    char* out_field,
    size_t out_capacity) {
    RayTracingSurfaceAuthoringCanvasSnapshot snapshot;
    int selected;
    if (!host || !out_field || out_capacity == 0u ||
        !ray_tracing_workspace_authoring_canvas_snapshot(&snapshot)) return false;
    selected = host->canvas_view.selected_node;
    if (selected < 0 || (size_t)selected >= snapshot.node_count) return false;
    if (strncmp(snapshot.nodes[selected].id, "ref:", 4u) != 0) return false;
    if (strcmp(snapshot.nodes[selected].id, "ref:material_graph") == 0) {
        snprintf(out_field, out_capacity, "material_graph");
    } else if (strcmp(snapshot.nodes[selected].id, "ref:surface_field_graph") == 0) {
        snprintf(out_field, out_capacity, "surface_field_graph");
    } else if (strcmp(snapshot.nodes[selected].id, "ref:face_region_selector") == 0) {
        snprintf(out_field, out_capacity, "face_region_selector");
    } else if (strncmp(snapshot.nodes[selected].id, "ref:attachment:", 15u) == 0) {
        snprintf(out_field, out_capacity, "attachment:%s", snapshot.nodes[selected].id + 15u);
    } else {
        return false;
    }
    return true;
}

static void ray_tracing_workspace_authoring_cancel_edit(
    RayTracingWorkspaceAuthoringHostState* host) {
    if (!host) return;
    host->edit_active = 0u;
    host->edit_buffer_length = 0u;
    host->edit_buffer[0] = '\0';
    SDL_StopTextInput();
    snprintf(host->document_status, sizeof(host->document_status), "document edit cancelled");
}

static bool ray_tracing_workspace_authoring_begin_edit(
    RayTracingWorkspaceAuthoringHostState* host) {
    ProceduralSurfaceAuthoringDocumentRef* target = NULL;
    ProceduralSurfaceAuthoringDocumentReport report;
    if (!host || host->edit_active || !host->document_loaded ||
        !ray_tracing_workspace_authoring_selected_field(
            host, host->edit_field, sizeof(host->edit_field)) ||
        !ray_tracing_workspace_authoring_document_target(
            &host->document, host->edit_field, &target) ||
        !ProceduralSurfaceAuthoringDocumentV1_Digest(
            &host->document, host->edit_expected_document_digest, &report)) {
        if (host) snprintf(host->document_status, sizeof(host->document_status),
                           "select a loaded reference to edit");
        return false;
    }
    snprintf(host->edit_expected_source_mesh_digest,
             sizeof(host->edit_expected_source_mesh_digest), "%s",
             host->document.source_mesh_digest_sha256);
    snprintf(host->edit_expected_reference_digest,
             sizeof(host->edit_expected_reference_digest), "%s",
             target->digest_sha256);
    snprintf(host->edit_buffer, sizeof(host->edit_buffer), "%s|%s|%u",
             target->id, target->digest_sha256, target->output_domains);
    host->edit_buffer_length = strlen(host->edit_buffer);
    host->edit_active = 1u;
    SDL_StartTextInput();
    snprintf(host->document_status, sizeof(host->document_status),
             "edit format: id|sha256|domains");
    return true;
}

static bool ray_tracing_workspace_authoring_parse_edit_buffer(
    const RayTracingWorkspaceAuthoringHostState* host,
    ProceduralSurfaceAuthoringDocumentRef* out_replacement) {
    char buffer[sizeof(host->edit_buffer)];
    char* first;
    char* second;
    char* end = NULL;
    unsigned long domains;
    if (!host || !out_replacement || host->edit_buffer_length >= sizeof(buffer)) return false;
    memset(out_replacement, 0, sizeof(*out_replacement));
    snprintf(buffer, sizeof(buffer), "%s", host->edit_buffer);
    first = strchr(buffer, '|');
    if (!first) return false;
    *first++ = '\0';
    second = strchr(first, '|');
    if (!second || !first[0] || !buffer[0]) return false;
    *second++ = '\0';
    errno = 0;
    domains = strtoul(second, &end, 0);
    if (errno != 0 || !end || *end != '\0' || domains == 0u || domains > UINT32_MAX ||
        strlen(buffer) >= sizeof(out_replacement->id) ||
        strlen(first) >= sizeof(out_replacement->digest_sha256)) return false;
    snprintf(out_replacement->id, sizeof(out_replacement->id), "%s", buffer);
    snprintf(out_replacement->digest_sha256,
             sizeof(out_replacement->digest_sha256), "%s", first);
    out_replacement->output_domains = (uint32_t)domains;
    return true;
}

static bool ray_tracing_workspace_authoring_commit_edit(
    RayTracingWorkspaceAuthoringHostState* host) {
    ProceduralSurfaceAuthoringDocumentRef replacement;
    CoreResult result;
    if (!host || !host->edit_active ||
        !ray_tracing_workspace_authoring_parse_edit_buffer(host, &replacement)) {
        if (host) snprintf(host->document_status, sizeof(host->document_status),
                           "edit rejected: use id|sha256|domains");
        return false;
    }
    result = ray_tracing_workspace_authoring_host_replace_reference(
        host, host->edit_field, &replacement);
    if (result.code != CORE_OK) return false;
    host->edit_active = 0u;
    host->edit_buffer_length = 0u;
    host->edit_buffer[0] = '\0';
    SDL_StopTextInput();
    return true;
}

void ray_tracing_workspace_authoring_host_reset(
    RayTracingWorkspaceAuthoringHostState* host) {
    if (!host) return;
    memset(host, 0, sizeof(*host));
    ray_tracing_surface_authoring_canvas_view_reset(&host->canvas_view);
    host->overlay_mode = RAY_TRACING_WORKSPACE_AUTHORING_OVERLAY_PANE;
}

void ray_tracing_workspace_authoring_host_set_viewport(
    RayTracingWorkspaceAuthoringHostState* host,
    int width,
    int height) {
    if (!host) return;
    host->viewport_width = width > 0 ? (uint32_t)width : 0u;
    host->viewport_height = height > 0 ? (uint32_t)height : 0u;
}

void ray_tracing_workspace_authoring_host_set_canvas_panel(
    RayTracingWorkspaceAuthoringHostState* host,
    const SDL_Rect* panel) {
    if (!host || !panel) return;
    host->canvas_panel = *panel;
    host->canvas_panel_ready = 1u;
}

int ray_tracing_workspace_authoring_host_active(
    const RayTracingWorkspaceAuthoringHostState* host) {
    return host && host->active ? 1 : 0;
}

int ray_tracing_workspace_authoring_host_pane_overlay_active(
    const RayTracingWorkspaceAuthoringHostState* host) {
    if (!ray_tracing_workspace_authoring_host_active(host)) return 0;
    return host->overlay_mode == RAY_TRACING_WORKSPACE_AUTHORING_OVERLAY_PANE ? 1 : 0;
}

int ray_tracing_workspace_authoring_host_font_theme_overlay_active(
    const RayTracingWorkspaceAuthoringHostState* host) {
    if (!ray_tracing_workspace_authoring_host_active(host)) return 0;
    return host->overlay_mode == RAY_TRACING_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME ? 1 : 0;
}

CoreResult ray_tracing_workspace_authoring_host_enter(
    RayTracingWorkspaceAuthoringHostState* host) {
    if (!host) return ray_tracing_workspace_authoring_invalid("null authoring host");
    if (!ray_tracing_workspace_authoring_host_active(host)) {
        host->active = 1u;
        host->overlay_mode = RAY_TRACING_WORKSPACE_AUTHORING_OVERLAY_PANE;
        host->enter_count += 1u;
        ray_tracing_workspace_authoring_capture_font_theme_baseline(host);
        if (!host->document_loaded && getenv("RAY_TRACING_SURFACE_AUTHORING_DOCUMENT_PATH")) {
            (void)ray_tracing_workspace_authoring_host_load_document(host);
        }
    }
    host->last_event_entered = 1u;
    return core_result_ok();
}

CoreResult ray_tracing_workspace_authoring_host_apply(
    RayTracingWorkspaceAuthoringHostState* host) {
    if (!host) return ray_tracing_workspace_authoring_invalid("null authoring host");
    if (host->document_dirty &&
        ray_tracing_workspace_authoring_host_save_document(host).code != CORE_OK) {
        return ray_tracing_workspace_authoring_invalid("document save failed");
    }
    if (host->font_theme_pending_changes) {
        host->font_theme_accepted_changes = 1u;
    }
    ray_tracing_workspace_authoring_clear_font_theme_baseline(host);
    if (ray_tracing_workspace_authoring_host_active(host)) {
        host->active = 0u;
        host->apply_count += 1u;
    }
    host->key_c_down = 0u;
    host->key_v_down = 0u;
    host->entry_chord_armed_key = KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    host->overlay_mode = RAY_TRACING_WORKSPACE_AUTHORING_OVERLAY_PANE;
    host->last_event_exited = 1u;
    return core_result_ok();
}

CoreResult ray_tracing_workspace_authoring_host_cancel(
    RayTracingWorkspaceAuthoringHostState* host) {
    if (!host) return ray_tracing_workspace_authoring_invalid("null authoring host");
    if (ray_tracing_workspace_authoring_host_active(host)) {
        host->active = 0u;
        host->cancel_count += 1u;
    }
    if (host->document_dirty) {
        host->document = host->undo_document;
        host->document_dirty = 0u;
        snprintf(host->document_status, sizeof(host->document_status), "document edit discarded");
    }
    host->key_c_down = 0u;
    host->key_v_down = 0u;
    host->entry_chord_armed_key = KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    host->overlay_mode = RAY_TRACING_WORKSPACE_AUTHORING_OVERLAY_PANE;
    host->last_event_exited = 1u;
    return core_result_ok();
}

CoreResult ray_tracing_workspace_authoring_host_cancel_preview(
    RayTracingWorkspaceAuthoringHostState* host) {
    if (!host) return ray_tracing_workspace_authoring_invalid("null authoring host");
    ray_tracing_workspace_authoring_restore_font_theme_baseline(host);
    return ray_tracing_workspace_authoring_host_cancel(host);
}

int ray_tracing_workspace_authoring_host_consume_accepted_font_theme_changes(
    RayTracingWorkspaceAuthoringHostState* host) {
    if (!host || !host->font_theme_accepted_changes) {
        return 0;
    }
    host->font_theme_accepted_changes = 0u;
    return 1;
}

int ray_tracing_workspace_authoring_host_apply_font_theme_button(
    RayTracingWorkspaceAuthoringHostState* host,
    KitWorkspaceAuthoringFontThemeButtonId button_id) {
    KitWorkspaceAuthoringFontThemeAction action;
    const char* preset_name = NULL;
    if (!host ||
        !ray_tracing_workspace_authoring_host_font_theme_overlay_active(host) ||
        !kit_workspace_authoring_ui_font_theme_button_enabled(button_id)) {
        return 0;
    }

    host->last_font_theme_button_id = (uint32_t)button_id;
    action = kit_workspace_authoring_ui_font_theme_action_for_button(button_id);
    switch (action.type) {
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_TEXT_SIZE_DEC: {
            int next_step = ray_tracing_workspace_authoring_text_zoom_step_clamp(
                animSettings.textZoomStep - 1);
            if (next_step != animSettings.textZoomStep) {
                animSettings.textZoomStep = next_step;
                ray_tracing_workspace_authoring_note_font_theme_changed(host, 1, 0);
            }
            ray_tracing_workspace_authoring_note_font_theme_status(host, "Text size decreased.");
            host->font_theme_button_click_count += 1u;
            return 1;
        }
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_TEXT_SIZE_INC: {
            int next_step = ray_tracing_workspace_authoring_text_zoom_step_clamp(
                animSettings.textZoomStep + 1);
            if (next_step != animSettings.textZoomStep) {
                animSettings.textZoomStep = next_step;
                ray_tracing_workspace_authoring_note_font_theme_changed(host, 1, 0);
            }
            ray_tracing_workspace_authoring_note_font_theme_status(host, "Text size increased.");
            host->font_theme_button_click_count += 1u;
            return 1;
        }
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_TEXT_SIZE_RESET:
            if (animSettings.textZoomStep != 0) {
                animSettings.textZoomStep = 0;
                ray_tracing_workspace_authoring_note_font_theme_changed(host, 1, 0);
            }
            ray_tracing_workspace_authoring_note_font_theme_status(host, "Text size reset.");
            host->font_theme_button_click_count += 1u;
            return 1;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_SET_FONT_PRESET:
            preset_name = core_font_preset_name(action.font_preset_id);
            if (preset_name && ray_tracing_shared_font_set_preset(preset_name)) {
                ray_tracing_workspace_authoring_note_font_theme_changed(host, 1, 0);
                ray_tracing_workspace_authoring_note_font_theme_status(host, "Font preset changed.");
                host->font_theme_button_click_count += 1u;
                return 1;
            }
            ray_tracing_workspace_authoring_note_font_theme_status(host, "Font preset change failed.");
            return 1;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_SET_THEME_PRESET:
            preset_name = core_theme_preset_name(action.theme_preset_id);
            if (preset_name && ray_tracing_shared_theme_set_preset(preset_name)) {
                ray_tracing_workspace_authoring_note_font_theme_changed(host, 0, 1);
                ray_tracing_workspace_authoring_note_font_theme_status(host, "Theme preset changed.");
                host->font_theme_button_click_count += 1u;
                return 1;
            }
            ray_tracing_workspace_authoring_note_font_theme_status(host, "Theme preset change failed.");
            return 1;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_CUSTOM_THEME_STATUS:
            ray_tracing_workspace_authoring_note_font_theme_status(
                host,
                action.custom_status_text ? action.custom_status_text
                                          : "Custom theme action requested.");
            host->font_theme_button_click_count += 1u;
            return 1;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_NONE:
        default:
            break;
    }
    return 0;
}

CoreResult ray_tracing_workspace_authoring_host_cycle_overlay(
    RayTracingWorkspaceAuthoringHostState* host) {
    if (!host) return ray_tracing_workspace_authoring_invalid("null authoring host");
    if (!ray_tracing_workspace_authoring_host_active(host)) return core_result_ok();
    host->overlay_mode =
        host->overlay_mode == RAY_TRACING_WORKSPACE_AUTHORING_OVERLAY_PANE
            ? RAY_TRACING_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME
            : RAY_TRACING_WORKSPACE_AUTHORING_OVERLAY_PANE;
    host->overlay_cycle_count += 1u;
    return core_result_ok();
}

int ray_tracing_workspace_authoring_host_apply_overlay_button(
    RayTracingWorkspaceAuthoringHostState* host,
    KitWorkspaceAuthoringOverlayButtonId button_id) {
    if (!host || !ray_tracing_workspace_authoring_host_active(host)) return 0;
    host->last_overlay_button_id = (uint32_t)button_id;
    switch (button_id) {
        case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_MODE:
            (void)ray_tracing_workspace_authoring_host_cycle_overlay(host);
            host->overlay_button_click_count += 1u;
            return 1;
        case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_APPLY:
            (void)ray_tracing_workspace_authoring_host_apply(host);
            host->overlay_button_click_count += 1u;
            return 1;
        case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_CANCEL:
            (void)ray_tracing_workspace_authoring_host_cancel_preview(host);
            host->overlay_button_click_count += 1u;
            return 1;
        case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_ADD:
            host->add_stub_count += 1u;
            host->overlay_button_click_count += 1u;
            return 1;
        case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_NONE:
        default:
            break;
    }
    return 0;
}

static int ray_tracing_workspace_authoring_host_handle_overlay_click(
    RayTracingWorkspaceAuthoringHostState* host,
    int x,
    int y) {
    KitWorkspaceAuthoringOverlayButton buttons[4];
    KitWorkspaceAuthoringOverlayButtonId hit = KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_NONE;
    uint32_t count = 0u;
    if (!host || !ray_tracing_workspace_authoring_host_active(host)) return 0;
    if (host->viewport_width == 0u) return 0;

    count = kit_workspace_authoring_ui_build_overlay_buttons(
        (int)host->viewport_width,
        1,
        ray_tracing_workspace_authoring_host_pane_overlay_active(host),
        buttons,
        (uint32_t)(sizeof(buttons) / sizeof(buttons[0])));
    hit = kit_workspace_authoring_ui_overlay_hit_test(buttons, count, (float)x, (float)y);
    return ray_tracing_workspace_authoring_host_apply_overlay_button(host, hit);
}

static int ray_tracing_workspace_authoring_host_handle_font_theme_click(
    RayTracingWorkspaceAuthoringHostState* host,
    int x,
    int y) {
    KitWorkspaceAuthoringFontThemeLayout layout;
    KitWorkspaceAuthoringFontThemeButtonId hit;
    if (!host || !ray_tracing_workspace_authoring_host_font_theme_overlay_active(host)) return 0;
    if (host->viewport_width == 0u || host->viewport_height == 0u) return 0;
    if (!kit_workspace_authoring_ui_font_theme_build_layout(NULL,
                                                            (int)host->viewport_width,
                                                            (int)host->viewport_height,
                                                            &layout)) {
        return 0;
    }
    hit = kit_workspace_authoring_ui_font_theme_hit_button(&layout, (float)x, (float)y);
    if (hit == KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_NONE) {
        return 0;
    }
    return ray_tracing_workspace_authoring_host_apply_font_theme_button(host, hit);
}

int ray_tracing_workspace_authoring_host_handle_sdl_event(
    RayTracingWorkspaceAuthoringHostState* host,
    const SDL_Event* event,
    int text_entry_active) {
    KitWorkspaceAuthoringKey key = KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    uint32_t mod_bits = 0u;
    int authoring_alt_only = 0;
    int chord_pair_pressed = 0;
    const char* trigger = NULL;

    if (!host || !event) return 0;
    host->last_event_consumed = 0u;
    host->last_event_entered = 0u;
    host->last_event_exited = 0u;

    if (event->type == SDL_KEYUP) {
        key = ray_tracing_workspace_authoring_key_from_sdl_keysym(&event->key.keysym);
        if (key == KIT_WORKSPACE_AUTHORING_KEY_C) {
            host->key_c_down = 0u;
        } else if (key == KIT_WORKSPACE_AUTHORING_KEY_V) {
            host->key_v_down = 0u;
        }
        return 0;
    }

    if (event->type == SDL_TEXTINPUT &&
        ray_tracing_workspace_authoring_host_active(host) &&
        ray_tracing_workspace_authoring_host_pane_overlay_active(host) &&
        host->edit_active) {
        size_t incoming = strlen(event->text.text);
        size_t available = sizeof(host->edit_buffer) - 1u - host->edit_buffer_length;
        if (incoming > available) incoming = available;
        if (incoming > 0u) {
            memcpy(host->edit_buffer + host->edit_buffer_length,
                   event->text.text, incoming);
            host->edit_buffer_length += incoming;
            host->edit_buffer[host->edit_buffer_length] = '\0';
        }
        ray_tracing_workspace_authoring_note_consumed(host, 1);
        return 1;
    }

    if (event->type == SDL_KEYDOWN &&
        ray_tracing_workspace_authoring_host_active(host) &&
        ray_tracing_workspace_authoring_host_pane_overlay_active(host)) {
        if (host->edit_active) {
            if (event->key.keysym.sym == SDLK_ESCAPE) {
                ray_tracing_workspace_authoring_cancel_edit(host);
                ray_tracing_workspace_authoring_note_consumed(host, 0);
                return 1;
            }
            if (event->key.keysym.sym == SDLK_BACKSPACE) {
                if (host->edit_buffer_length > 0u) {
                    host->edit_buffer[--host->edit_buffer_length] = '\0';
                }
                ray_tracing_workspace_authoring_note_consumed(host, 1);
                return 1;
            }
            if (event->key.keysym.sym == SDLK_RETURN ||
                event->key.keysym.sym == SDLK_KP_ENTER) {
                (void)ray_tracing_workspace_authoring_commit_edit(host);
                ray_tracing_workspace_authoring_note_consumed(host, 0);
                return 1;
            }
        } else if (event->key.keysym.sym == SDLK_e) {
            (void)ray_tracing_workspace_authoring_begin_edit(host);
            ray_tracing_workspace_authoring_note_consumed(host, 0);
            return 1;
        }
    }

    if (event->type == SDL_MOUSEMOTION &&
        ray_tracing_workspace_authoring_host_active(host)) {
        host->last_pointer_x = event->motion.x > 0 ? (uint32_t)event->motion.x : 0u;
        host->last_pointer_y = event->motion.y > 0 ? (uint32_t)event->motion.y : 0u;
        host->last_pointer_ready = 1u;
        if (host->canvas_view.panning) {
            ray_tracing_surface_authoring_canvas_view_update_pan(
                &host->canvas_view, event->motion.x, event->motion.y);
        }
        ray_tracing_workspace_authoring_note_consumed(host, 1);
        return 1;
    }

    if (event->type == SDL_MOUSEBUTTONUP &&
        event->button.button == SDL_BUTTON_MIDDLE &&
        ray_tracing_workspace_authoring_host_active(host)) {
        ray_tracing_surface_authoring_canvas_view_end_pan(&host->canvas_view);
        ray_tracing_workspace_authoring_note_consumed(host, 0);
        return 1;
    }

    if (event->type == SDL_MOUSEWHEEL &&
        ray_tracing_workspace_authoring_host_active(host) &&
        ray_tracing_workspace_authoring_host_pane_overlay_active(host)) {
        SDL_Rect panel;
        int pointer_x = host->last_pointer_ready ? (int)host->last_pointer_x : event->wheel.mouseX;
        int pointer_y = host->last_pointer_ready ? (int)host->last_pointer_y : event->wheel.mouseY;
        if (ray_tracing_workspace_authoring_canvas_panel(host, &panel) &&
            ray_tracing_surface_authoring_canvas_view_contains(&panel, pointer_x, pointer_y)) {
            ray_tracing_surface_authoring_canvas_view_zoom(
                &host->canvas_view, &panel, pointer_x, pointer_y, event->wheel.y);
            ray_tracing_workspace_authoring_note_consumed(host, 0);
            return 1;
        }
    }

    if (event->type == SDL_MOUSEBUTTONDOWN &&
        ray_tracing_workspace_authoring_host_active(host)) {
        SDL_Rect panel;
        if (event->button.button == SDL_BUTTON_MIDDLE &&
            ray_tracing_workspace_authoring_canvas_panel(host, &panel) &&
            ray_tracing_surface_authoring_canvas_view_contains(
                &panel, event->button.x, event->button.y)) {
            ray_tracing_surface_authoring_canvas_view_begin_pan(
                &host->canvas_view, event->button.x, event->button.y);
            ray_tracing_workspace_authoring_note_consumed(host, 1);
            return 1;
        }
        if (event->button.button != SDL_BUTTON_LEFT) {
            goto authoring_event_fallback;
        }
        int overlay_hit = 0;
        host->last_pointer_x = event->button.x > 0 ? (uint32_t)event->button.x : 0u;
        host->last_pointer_y = event->button.y > 0 ? (uint32_t)event->button.y : 0u;
        host->last_pointer_ready = 1u;
        if (ray_tracing_workspace_authoring_canvas_panel(host, &panel) &&
            ray_tracing_surface_authoring_canvas_view_contains(
                &panel, event->button.x, event->button.y)) {
            RayTracingSurfaceAuthoringCanvasSnapshot snapshot;
            if (ray_tracing_workspace_authoring_canvas_snapshot(&snapshot)) {
                (void)ray_tracing_surface_authoring_canvas_view_select(
                    &host->canvas_view, &snapshot, &panel,
                    event->button.x, event->button.y);
            }
            ray_tracing_workspace_authoring_note_consumed(host, 0);
            return 1;
        }
        overlay_hit = ray_tracing_workspace_authoring_host_handle_overlay_click(
            host,
            event->button.x,
            event->button.y);
        if (!overlay_hit &&
            ray_tracing_workspace_authoring_host_handle_font_theme_click(host,
                                                                         event->button.x,
                                                                         event->button.y)) {
            ray_tracing_workspace_authoring_note_consumed(host, 0);
            return 1;
        }
        ray_tracing_workspace_authoring_note_consumed(host, overlay_hit ? 0 : 1);
        return 1;
    }

authoring_event_fallback:

    if (ray_tracing_workspace_authoring_host_active(host) &&
        (event->type == SDL_MOUSEBUTTONDOWN ||
         event->type == SDL_MOUSEBUTTONUP ||
         event->type == SDL_MOUSEWHEEL ||
         event->type == SDL_TEXTINPUT)) {
        ray_tracing_workspace_authoring_note_consumed(host, 1);
        return 1;
    }

    if (event->type != SDL_KEYDOWN || event->key.repeat != 0) {
        return 0;
    }

    key = ray_tracing_workspace_authoring_key_from_sdl_keysym(&event->key.keysym);
    mod_bits = ray_tracing_workspace_authoring_mod_bits((SDL_Keymod)event->key.keysym.mod);
    authoring_alt_only = ((mod_bits & KIT_WORKSPACE_AUTHORING_MOD_ALT) != 0u) &&
                         ((mod_bits & (KIT_WORKSPACE_AUTHORING_MOD_SHIFT |
                                       KIT_WORKSPACE_AUTHORING_MOD_CTRL |
                                       KIT_WORKSPACE_AUTHORING_MOD_GUI)) == 0u);

    if (text_entry_active && !ray_tracing_workspace_authoring_host_active(host)) {
        return 0;
    }

    if (authoring_alt_only) {
        if (key == KIT_WORKSPACE_AUTHORING_KEY_C) {
            host->key_c_down = 1u;
        } else if (key == KIT_WORKSPACE_AUTHORING_KEY_V) {
            host->key_v_down = 1u;
        }
    }

    chord_pair_pressed = kit_workspace_authoring_entry_chord_pressed(
        key,
        mod_bits,
        host->key_c_down ? 1 : 0,
        host->key_v_down ? 1 : 0);
    if (authoring_alt_only &&
        (key == KIT_WORKSPACE_AUTHORING_KEY_C || key == KIT_WORKSPACE_AUTHORING_KEY_V) &&
        host->entry_chord_armed_key != KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN &&
        host->entry_chord_armed_key != (uint8_t)key) {
        chord_pair_pressed = 1;
    }
    if (chord_pair_pressed) {
        if (ray_tracing_workspace_authoring_host_active(host)) {
            (void)ray_tracing_workspace_authoring_host_cancel_preview(host);
        } else {
            (void)ray_tracing_workspace_authoring_host_enter(host);
        }
        host->entry_chord_armed_key = KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
        ray_tracing_workspace_authoring_note_consumed(host, 0);
        return 1;
    }

    if (authoring_alt_only &&
        (key == KIT_WORKSPACE_AUTHORING_KEY_C || key == KIT_WORKSPACE_AUTHORING_KEY_V)) {
        host->entry_chord_armed_key = (uint8_t)key;
        ray_tracing_workspace_authoring_note_consumed(host, 0);
        return 1;
    }

    if (!ray_tracing_workspace_authoring_host_active(host)) {
        return 0;
    }

    if (ray_tracing_workspace_authoring_host_font_theme_overlay_active(host) &&
        ((mod_bits & (KIT_WORKSPACE_AUTHORING_MOD_CTRL | KIT_WORKSPACE_AUTHORING_MOD_GUI)) != 0u)) {
        if (event->key.keysym.sym == SDLK_EQUALS || event->key.keysym.sym == SDLK_PLUS ||
            event->key.keysym.sym == SDLK_KP_PLUS) {
            (void)ray_tracing_workspace_authoring_host_apply_font_theme_button(
                host,
                KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_INC);
            ray_tracing_workspace_authoring_note_consumed(host, 0);
            return 1;
        }
        if (event->key.keysym.sym == SDLK_MINUS || event->key.keysym.sym == SDLK_KP_MINUS) {
            (void)ray_tracing_workspace_authoring_host_apply_font_theme_button(
                host,
                KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_DEC);
            ray_tracing_workspace_authoring_note_consumed(host, 0);
            return 1;
        }
        if (event->key.keysym.sym == SDLK_0 || event->key.keysym.sym == SDLK_KP_0) {
            (void)ray_tracing_workspace_authoring_host_apply_font_theme_button(
                host,
                KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_RESET);
            ray_tracing_workspace_authoring_note_consumed(host, 0);
            return 1;
        }
    }

    if (key == KIT_WORKSPACE_AUTHORING_KEY_ESCAPE) {
        (void)ray_tracing_workspace_authoring_host_cancel_preview(host);
        ray_tracing_workspace_authoring_note_consumed(host, 0);
        return 1;
    }
    if (key == KIT_WORKSPACE_AUTHORING_KEY_ENTER) {
        (void)ray_tracing_workspace_authoring_host_apply(host);
        ray_tracing_workspace_authoring_note_consumed(host, 0);
        return 1;
    }

    trigger = kit_workspace_authoring_trigger_from_key(key, mod_bits);
    if (trigger && strcmp(trigger, "tab") == 0) {
        (void)ray_tracing_workspace_authoring_host_cycle_overlay(host);
        ray_tracing_workspace_authoring_note_consumed(host, 0);
        return 1;
    }
    if (trigger) {
        ray_tracing_workspace_authoring_note_consumed(host, 1);
        return 1;
    }
    return 0;
}
