#include "ui/menu/workspace_authoring/ray_tracing_workspace_authoring_host.h"

#include <assert.h>
#include <string.h>

#include "config/config_manager.h"
#include "ui/shared_theme_font_adapter.h"

AnimationConfig animSettings = {0};

int kit_render_text_zoom_percent(const KitRenderContext* ctx) {
    (void)ctx;
    return 100;
}

CoreResult kit_render_measure_text(const KitRenderContext* ctx,
                                   CoreFontRoleId font_role,
                                   CoreFontTextSizeTier text_tier,
                                   const char* text,
                                   KitRenderTextMetrics* out_metrics) {
    (void)ctx;
    (void)font_role;
    (void)text_tier;
    if (!out_metrics) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "null metrics" };
    }
    out_metrics->width_px = text ? (float)strlen(text) * 7.0f : 0.0f;
    out_metrics->height_px = 12.0f;
    return core_result_ok();
}

static SDL_Event authoring_key_event(Uint32 type,
                                     SDL_Scancode scancode,
                                     SDL_Keycode sym,
                                     SDL_Keymod mods) {
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.key.type = type;
    event.key.state = type == SDL_KEYUP ? SDL_RELEASED : SDL_PRESSED;
    event.key.repeat = 0;
    event.key.keysym.scancode = scancode;
    event.key.keysym.sym = sym;
    event.key.keysym.mod = mods;
    return event;
}

static void authoring_button_point(const RayTracingWorkspaceAuthoringHostState* host,
                                   KitWorkspaceAuthoringOverlayButtonId button_id,
                                   int* out_x,
                                   int* out_y) {
    KitWorkspaceAuthoringOverlayButton buttons[4];
    uint32_t count = 0u;
    uint32_t i = 0u;
    assert(host && out_x && out_y);
    count = kit_workspace_authoring_ui_build_overlay_buttons(
        (int)host->viewport_width,
        ray_tracing_workspace_authoring_host_active(host),
        ray_tracing_workspace_authoring_host_pane_overlay_active(host),
        buttons,
        (uint32_t)(sizeof(buttons) / sizeof(buttons[0])));
    for (i = 0u; i < count; ++i) {
        if (buttons[i].id == button_id) {
            *out_x = (int)(buttons[i].rect.x + buttons[i].rect.width * 0.5f);
            *out_y = (int)(buttons[i].rect.y + buttons[i].rect.height * 0.5f);
            return;
        }
    }
    assert(0 && "button was not visible");
}

static void test_entry_chord_and_cancel(void) {
    RayTracingWorkspaceAuthoringHostState host;
    SDL_Event plain_c;
    SDL_Event alt_c;
    SDL_Event alt_v;
    SDL_Event tab;
    SDL_Event escape;

    ray_tracing_workspace_authoring_host_reset(&host);
    plain_c = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_C, SDLK_c, KMOD_NONE);
    assert(!ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &plain_c, 0));
    assert(!ray_tracing_workspace_authoring_host_active(&host));

    alt_c = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_C, SDLK_c, KMOD_ALT);
    alt_v = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_V, SDLK_v, KMOD_ALT);
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &alt_c, 0));
    assert(!ray_tracing_workspace_authoring_host_active(&host));
    assert(host.entry_chord_armed_key != 0u);
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &alt_v, 0));
    assert(ray_tracing_workspace_authoring_host_active(&host));
    assert(ray_tracing_workspace_authoring_host_pane_overlay_active(&host));
    assert(host.enter_count == 1u);

    tab = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_TAB, SDLK_TAB, KMOD_NONE);
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &tab, 0));
    assert(ray_tracing_workspace_authoring_host_font_theme_overlay_active(&host));
    assert(host.overlay_cycle_count == 1u);

    escape = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, KMOD_NONE);
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &escape, 0));
    assert(!ray_tracing_workspace_authoring_host_active(&host));
    assert(host.cancel_count == 1u);
}

static void test_text_entry_blocks_inactive_entry_chord(void) {
    RayTracingWorkspaceAuthoringHostState host;
    SDL_Event alt_c;
    SDL_Event alt_v;

    ray_tracing_workspace_authoring_host_reset(&host);
    alt_c = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_C, SDLK_c, KMOD_ALT);
    alt_v = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_V, SDLK_v, KMOD_ALT);
    assert(!ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &alt_c, 1));
    assert(!ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &alt_v, 1));
    assert(!ray_tracing_workspace_authoring_host_active(&host));
    assert(host.consumed_event_count == 0u);
}

static void test_sequential_physical_chord_and_apply(void) {
    RayTracingWorkspaceAuthoringHostState host;
    SDL_Event alt_c_down;
    SDL_Event alt_c_up;
    SDL_Event alt_v_down;
    SDL_Event enter;

    ray_tracing_workspace_authoring_host_reset(&host);
    alt_c_down = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_C, SDLK_UNKNOWN, KMOD_ALT);
    alt_c_up = authoring_key_event(SDL_KEYUP, SDL_SCANCODE_C, SDLK_UNKNOWN, KMOD_ALT);
    alt_v_down = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_V, SDLK_UNKNOWN, KMOD_ALT);
    enter = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_RETURN, SDLK_RETURN, KMOD_NONE);

    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &alt_c_down, 0));
    assert(!ray_tracing_workspace_authoring_host_active(&host));
    assert(!ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &alt_c_up, 0));
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &alt_v_down, 0));
    assert(ray_tracing_workspace_authoring_host_active(&host));
    assert(host.enter_count == 1u);

    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &enter, 0));
    assert(!ray_tracing_workspace_authoring_host_active(&host));
    assert(host.apply_count == 1u);
}

static void test_runtime_events_captured_while_active(void) {
    RayTracingWorkspaceAuthoringHostState host;
    SDL_Event h_key;
    SDL_Event mouse_down;

    ray_tracing_workspace_authoring_host_reset(&host);
    assert(ray_tracing_workspace_authoring_host_enter(&host).code == CORE_OK);

    h_key = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_H, SDLK_h, KMOD_NONE);
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &h_key, 0));
    assert(host.captured_runtime_event_count == 1u);

    memset(&mouse_down, 0, sizeof(mouse_down));
    mouse_down.type = SDL_MOUSEBUTTONDOWN;
    mouse_down.button.type = SDL_MOUSEBUTTONDOWN;
    mouse_down.button.button = SDL_BUTTON_LEFT;
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &mouse_down, 0));
    assert(host.captured_runtime_event_count == 2u);
}

static void test_overlay_buttons_control_state(void) {
    RayTracingWorkspaceAuthoringHostState host;
    SDL_Event click;
    int x = 0;
    int y = 0;

    ray_tracing_workspace_authoring_host_reset(&host);
    ray_tracing_workspace_authoring_host_set_viewport(&host, 1280, 720);
    assert(ray_tracing_workspace_authoring_host_enter(&host).code == CORE_OK);
    assert(ray_tracing_workspace_authoring_host_pane_overlay_active(&host));

    memset(&click, 0, sizeof(click));
    click.type = SDL_MOUSEBUTTONDOWN;
    click.button.type = SDL_MOUSEBUTTONDOWN;
    click.button.button = SDL_BUTTON_LEFT;

    authoring_button_point(&host, KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_MODE, &x, &y);
    click.button.x = x;
    click.button.y = y;
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &click, 0));
    assert(ray_tracing_workspace_authoring_host_font_theme_overlay_active(&host));
    assert(host.overlay_button_click_count == 1u);

    assert(ray_tracing_workspace_authoring_host_cycle_overlay(&host).code == CORE_OK);
    assert(ray_tracing_workspace_authoring_host_pane_overlay_active(&host));
    authoring_button_point(&host, KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_ADD, &x, &y);
    click.button.x = x;
    click.button.y = y;
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &click, 0));
    assert(host.add_stub_count == 1u);
    assert(ray_tracing_workspace_authoring_host_active(&host));

    authoring_button_point(&host, KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_CANCEL, &x, &y);
    click.button.x = x;
    click.button.y = y;
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &click, 0));
    assert(!ray_tracing_workspace_authoring_host_active(&host));
    assert(host.cancel_count == 1u);
}

static void test_font_theme_buttons_preview_and_cancel(void) {
    RayTracingWorkspaceAuthoringHostState host;
    KitWorkspaceAuthoringFontThemeLayout layout;
    SDL_Event click;
    SDL_Event escape;
    char font_preset[64] = {0};
    char theme_preset[64] = {0};

    animSettings.textZoomStep = 0;
    assert(ray_tracing_shared_font_set_preset("ide"));
    assert(ray_tracing_shared_theme_set_preset("midnight_contrast"));

    ray_tracing_workspace_authoring_host_reset(&host);
    ray_tracing_workspace_authoring_host_set_viewport(&host, 1280, 720);
    assert(ray_tracing_workspace_authoring_host_enter(&host).code == CORE_OK);
    assert(ray_tracing_workspace_authoring_host_cycle_overlay(&host).code == CORE_OK);
    assert(ray_tracing_workspace_authoring_host_font_theme_overlay_active(&host));
    assert(kit_workspace_authoring_ui_font_theme_build_layout(NULL, 1280, 720, &layout));

    memset(&click, 0, sizeof(click));
    click.type = SDL_MOUSEBUTTONDOWN;
    click.button.type = SDL_MOUSEBUTTONDOWN;
    click.button.button = SDL_BUTTON_LEFT;
    click.button.x = (int)(layout.text_size_inc_button.x + layout.text_size_inc_button.width * 0.5f);
    click.button.y = (int)(layout.text_size_inc_button.y + layout.text_size_inc_button.height * 0.5f);
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &click, 0));
    assert(animSettings.textZoomStep == 1);
    assert(host.font_theme_button_click_count == 1u);
    assert(host.font_theme_needs_font_reload == 1u);

    assert(ray_tracing_workspace_authoring_host_apply_font_theme_button(
        &host,
        KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_FONT_PRESET_DAW_DEFAULT));
    assert(ray_tracing_shared_font_current_preset(font_preset, sizeof(font_preset)));
    assert(strcmp(font_preset, "daw_default") == 0);

    assert(ray_tracing_workspace_authoring_host_apply_font_theme_button(
        &host,
        KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_SOFT_LIGHT));
    assert(ray_tracing_shared_theme_current_preset(theme_preset, sizeof(theme_preset)));
    assert(strcmp(theme_preset, "soft_light") == 0);
    assert(host.font_theme_needs_theme_apply == 1u);

    escape = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, KMOD_NONE);
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &escape, 0));
    assert(!ray_tracing_workspace_authoring_host_active(&host));
    assert(animSettings.textZoomStep == 0);
    assert(ray_tracing_shared_font_current_preset(font_preset, sizeof(font_preset)));
    assert(strcmp(font_preset, "ide") == 0);
    assert(ray_tracing_shared_theme_current_preset(theme_preset, sizeof(theme_preset)));
    assert(strcmp(theme_preset, "midnight_contrast") == 0);
    assert(!ray_tracing_workspace_authoring_host_consume_accepted_font_theme_changes(&host));
}

static void test_font_theme_buttons_apply_marks_accepted(void) {
    RayTracingWorkspaceAuthoringHostState host;
    SDL_Event enter;
    char font_preset[64] = {0};
    char theme_preset[64] = {0};

    animSettings.textZoomStep = 0;
    assert(ray_tracing_shared_font_set_preset("ide"));
    assert(ray_tracing_shared_theme_set_preset("midnight_contrast"));

    ray_tracing_workspace_authoring_host_reset(&host);
    ray_tracing_workspace_authoring_host_set_viewport(&host, 1280, 720);
    assert(ray_tracing_workspace_authoring_host_enter(&host).code == CORE_OK);
    assert(ray_tracing_workspace_authoring_host_cycle_overlay(&host).code == CORE_OK);
    assert(ray_tracing_workspace_authoring_host_apply_font_theme_button(
        &host,
        KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_INC));
    assert(ray_tracing_workspace_authoring_host_apply_font_theme_button(
        &host,
        KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_FONT_PRESET_DAW_DEFAULT));
    assert(ray_tracing_workspace_authoring_host_apply_font_theme_button(
        &host,
        KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_SOFT_LIGHT));

    enter = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_RETURN, SDLK_RETURN, KMOD_NONE);
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &enter, 0));
    assert(!ray_tracing_workspace_authoring_host_active(&host));
    assert(animSettings.textZoomStep == 1);
    assert(ray_tracing_shared_font_current_preset(font_preset, sizeof(font_preset)));
    assert(strcmp(font_preset, "daw_default") == 0);
    assert(ray_tracing_shared_theme_current_preset(theme_preset, sizeof(theme_preset)));
    assert(strcmp(theme_preset, "soft_light") == 0);
    assert(ray_tracing_workspace_authoring_host_consume_accepted_font_theme_changes(&host));
    assert(!ray_tracing_workspace_authoring_host_consume_accepted_font_theme_changes(&host));
}

int main(void) {
    test_entry_chord_and_cancel();
    test_text_entry_blocks_inactive_entry_chord();
    test_sequential_physical_chord_and_apply();
    test_runtime_events_captured_while_active();
    test_overlay_buttons_control_state();
    test_font_theme_buttons_preview_and_cancel();
    test_font_theme_buttons_apply_marks_accepted();
    return 0;
}
