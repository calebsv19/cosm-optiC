#ifndef RAY_TRACING_WORKSPACE_AUTHORING_HOST_H
#define RAY_TRACING_WORKSPACE_AUTHORING_HOST_H

#include <SDL2/SDL.h>
#include <stdint.h>

#include "core_base.h"
#include "kit_workspace_authoring.h"
#include "kit_workspace_authoring_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum RayTracingWorkspaceAuthoringOverlayMode {
    RAY_TRACING_WORKSPACE_AUTHORING_OVERLAY_PANE = 0,
    RAY_TRACING_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME = 1
} RayTracingWorkspaceAuthoringOverlayMode;

typedef struct RayTracingWorkspaceAuthoringHostState {
    uint8_t active;
    uint8_t key_c_down;
    uint8_t key_v_down;
    uint8_t entry_chord_armed_key;
    RayTracingWorkspaceAuthoringOverlayMode overlay_mode;
    uint32_t enter_count;
    uint32_t apply_count;
    uint32_t cancel_count;
    uint32_t overlay_cycle_count;
    uint32_t consumed_event_count;
    uint32_t last_event_consumed;
    uint32_t last_event_entered;
    uint32_t last_event_exited;
    uint32_t captured_runtime_event_count;
    uint32_t viewport_width;
    uint32_t viewport_height;
    uint32_t last_pointer_x;
    uint32_t last_pointer_y;
    uint32_t last_pointer_ready;
    uint32_t overlay_button_click_count;
    uint32_t add_stub_count;
    uint32_t last_overlay_button_id;
    int baseline_text_zoom_step;
    char baseline_theme_preset[64];
    char baseline_font_preset[64];
    uint8_t font_theme_baseline_ready;
    uint8_t font_theme_pending_changes;
    uint8_t font_theme_accepted_changes;
    uint8_t font_theme_needs_font_reload;
    uint8_t font_theme_needs_theme_apply;
    uint8_t font_theme_status_active;
    uint32_t font_theme_button_click_count;
    uint32_t font_theme_change_count;
    uint32_t font_theme_status_count;
    uint32_t last_font_theme_button_id;
    char font_theme_status[160];
} RayTracingWorkspaceAuthoringHostState;

void ray_tracing_workspace_authoring_host_reset(
    RayTracingWorkspaceAuthoringHostState* host);
void ray_tracing_workspace_authoring_host_set_viewport(
    RayTracingWorkspaceAuthoringHostState* host,
    int width,
    int height);
int ray_tracing_workspace_authoring_host_active(
    const RayTracingWorkspaceAuthoringHostState* host);
int ray_tracing_workspace_authoring_host_pane_overlay_active(
    const RayTracingWorkspaceAuthoringHostState* host);
int ray_tracing_workspace_authoring_host_font_theme_overlay_active(
    const RayTracingWorkspaceAuthoringHostState* host);
CoreResult ray_tracing_workspace_authoring_host_enter(
    RayTracingWorkspaceAuthoringHostState* host);
CoreResult ray_tracing_workspace_authoring_host_apply(
    RayTracingWorkspaceAuthoringHostState* host);
CoreResult ray_tracing_workspace_authoring_host_cancel(
    RayTracingWorkspaceAuthoringHostState* host);
CoreResult ray_tracing_workspace_authoring_host_cancel_preview(
    RayTracingWorkspaceAuthoringHostState* host);
CoreResult ray_tracing_workspace_authoring_host_cycle_overlay(
    RayTracingWorkspaceAuthoringHostState* host);
int ray_tracing_workspace_authoring_host_apply_overlay_button(
    RayTracingWorkspaceAuthoringHostState* host,
    KitWorkspaceAuthoringOverlayButtonId button_id);
int ray_tracing_workspace_authoring_host_apply_font_theme_button(
    RayTracingWorkspaceAuthoringHostState* host,
    KitWorkspaceAuthoringFontThemeButtonId button_id);
int ray_tracing_workspace_authoring_host_consume_accepted_font_theme_changes(
    RayTracingWorkspaceAuthoringHostState* host);
int ray_tracing_workspace_authoring_host_handle_sdl_event(
    RayTracingWorkspaceAuthoringHostState* host,
    const SDL_Event* event,
    int text_entry_active);

#ifdef __cplusplus
}
#endif

#endif
