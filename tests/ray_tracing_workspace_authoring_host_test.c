#include "ui/menu/workspace_authoring/ray_tracing_workspace_authoring_host.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

static void test_canvas_navigation_events_are_read_only(void) {
    RayTracingWorkspaceAuthoringHostState host;
    SDL_Event click;
    SDL_Event wheel;
    SDL_Event middle_down;
    SDL_Event motion;
    SDL_Event middle_up;
    SDL_Rect solved_panel = {100, 80, 900, 500};

    ray_tracing_workspace_authoring_host_reset(&host);
    ray_tracing_workspace_authoring_host_set_viewport(&host, 1280, 720);
    ray_tracing_workspace_authoring_host_set_canvas_panel(&host, &solved_panel);
    assert(ray_tracing_workspace_authoring_host_enter(&host).code == CORE_OK);

    memset(&click, 0, sizeof(click));
    click.type = SDL_MOUSEBUTTONDOWN;
    click.button.type = SDL_MOUSEBUTTONDOWN;
    click.button.button = SDL_BUTTON_LEFT;
    click.button.x = 165;
    click.button.y = 343;
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &click, 0));
    assert(host.canvas_view.selected_node == 0);
    assert(host.apply_count == 0u && host.cancel_count == 0u);

    memset(&wheel, 0, sizeof(wheel));
    wheel.type = SDL_MOUSEWHEEL;
    wheel.wheel.type = SDL_MOUSEWHEEL;
    wheel.wheel.x = 0;
    wheel.wheel.y = 2;
    wheel.wheel.mouseX = 165;
    wheel.wheel.mouseY = 343;
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &wheel, 0));
    assert(host.canvas_view.zoom > 1.0f);

    memset(&middle_down, 0, sizeof(middle_down));
    middle_down.type = SDL_MOUSEBUTTONDOWN;
    middle_down.button.type = SDL_MOUSEBUTTONDOWN;
    middle_down.button.button = SDL_BUTTON_MIDDLE;
    middle_down.button.x = 165;
    middle_down.button.y = 343;
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &middle_down, 0));
    assert(host.canvas_view.panning);

    memset(&motion, 0, sizeof(motion));
    motion.type = SDL_MOUSEMOTION;
    motion.motion.type = SDL_MOUSEMOTION;
    motion.motion.x = 370;
    motion.motion.y = 400;
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &motion, 0));

    memset(&middle_up, 0, sizeof(middle_up));
    middle_up.type = SDL_MOUSEBUTTONUP;
    middle_up.button.type = SDL_MOUSEBUTTONUP;
    middle_up.button.button = SDL_BUTTON_MIDDLE;
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &middle_up, 0));
    assert(!host.canvas_view.panning && host.canvas_view.pan_x != 0);
    assert(host.apply_count == 0u && host.cancel_count == 0u);
}

static void test_canvas_edit_event_transaction_and_save(void) {
    const char *path = "/tmp/ray_tracing_surface_authoring_host_edit.json";
    const char *digest = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    RayTracingWorkspaceAuthoringHostState host;
    SDL_Event click;
    SDL_Event key;
    SDL_Event backspace;
    SDL_Event text;
    FILE *file;

    file = fopen(path, "w");
    assert(file != NULL);
    fprintf(file,
            "{\"schema\":\"ray_tracing.surface_authoring_document\","
            "\"schema_version\":1,\"document_id\":\"host_edit\","
            "\"source_object_id\":\"cube\","
            "\"source_mesh_digest_sha256\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
            "\"material_graph\":{\"id\":\"brown_mix\",\"digest_sha256\":\"%s\",\"output_domains\":3},"
            "\"surface_field_graph\":null,\"face_region_selector\":null,\"attachments\":[]}\n",
            digest);
    fclose(file);

    ray_tracing_workspace_authoring_host_reset(&host);
    ray_tracing_workspace_authoring_host_set_viewport(&host, 1280, 720);
    {
        SDL_Rect panel = {100, 80, 900, 500};
        ray_tracing_workspace_authoring_host_set_canvas_panel(&host, &panel);
    }
    ray_tracing_workspace_authoring_host_set_document_path(&host, path);
    assert(ray_tracing_workspace_authoring_host_load_document(&host).code == CORE_OK);
    assert(ray_tracing_workspace_authoring_host_enter(&host).code == CORE_OK);

    memset(&click, 0, sizeof(click));
    click.type = SDL_MOUSEBUTTONDOWN;
    click.button.type = SDL_MOUSEBUTTONDOWN;
    click.button.button = SDL_BUTTON_LEFT;
    click.button.x = 750;
    click.button.y = 210;
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &click, 0));
    assert(host.canvas_view.selected_node >= 0);
    assert(host.canvas_view.selected_node != 0);

    memset(&key, 0, sizeof(key));
    key.type = SDL_KEYDOWN;
    key.key.type = SDL_KEYDOWN;
    key.key.keysym.sym = SDLK_e;
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &key, 0));
    assert(host.edit_active);
    memset(&backspace, 0, sizeof(backspace));
    backspace.type = SDL_KEYDOWN;
    backspace.key.type = SDL_KEYDOWN;
    backspace.key.keysym.sym = SDLK_BACKSPACE;
    while (host.edit_buffer_length > 0u) {
        assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &backspace, 0));
    }

    memset(&text, 0, sizeof(text));
    text.type = SDL_TEXTINPUT;
    text.text.type = SDL_TEXTINPUT;
    snprintf(text.text.text, sizeof(text.text.text), "edited_material|");
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &text, 0));
    snprintf(text.text.text, sizeof(text.text.text), "fffffffffffffffffffffffffffffff");
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &text, 0));
    snprintf(text.text.text, sizeof(text.text.text), "fffffffffffffffffffffffffffffff");
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &text, 0));
    snprintf(text.text.text, sizeof(text.text.text), "ff|3");
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &text, 0));
    key.key.keysym.sym = SDLK_RETURN;
    assert(ray_tracing_workspace_authoring_host_handle_sdl_event(&host, &key, 0));
    assert(!host.edit_active && host.document_dirty);
    assert(strcmp(host.document.material_graph.id, "edited_material") == 0);
    assert(host.document_edit_count == 1u);
    assert(ray_tracing_workspace_authoring_host_apply(&host).code == CORE_OK);
    assert(!host.document_dirty && host.document_save_count == 1u);
    assert(host.document_readback_count >= 2u);
    unlink(path);
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
    test_canvas_navigation_events_are_read_only();
    test_canvas_edit_event_transaction_and_save();
    test_font_theme_buttons_preview_and_cancel();
    test_font_theme_buttons_apply_marks_accepted();
    return 0;
}
