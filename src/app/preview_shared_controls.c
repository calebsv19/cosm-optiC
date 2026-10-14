#include "app/preview_shared_controls.h"
#include "kit_ui_interaction_sdl.h"
#include <string.h>

SDL_Rect PreviewSharedCloseRect(int width) {
    int margin = width < 160 ? 4 : 12;
    int w = width - margin * 2;
    if (w > 132) w = 132;
    return (SDL_Rect){width - w - margin, margin, w, 34};
}
void PreviewSharedControlsCancel(PreviewSharedControls *c) {
    kit_ui_interaction_reset(&c->surface.interaction);
    c->surface.activation_count = 0;
    c->ready = c->keyboard_focus = false;
    c->blocked_release = true;
}
void PreviewSharedControlsPublish(PreviewSharedControls *c,
                                 const PreviewWorkspace *w) {
    PreviewWorkspaceRect rows[] = {w->layout.play_pause_button,
        w->layout.loop_button, w->layout.bounce_button};
    SDL_Rect close = PreviewSharedCloseRect(w->layout.panel.w);
    kit_ui_surface_begin(&c->surface, 1);
    for (unsigned i = 0; i < 4; ++i) {
        KitRenderRect r = i < 3 ? (KitRenderRect){rows[i].x, rows[i].y, rows[i].w, rows[i].h}
                               : (KitRenderRect){close.x, close.y, close.w, close.h};
        (void)kit_ui_surface_register(&c->surface, (KitUiSurfaceKey){1, i+1}, r, NULL, 1, NULL);
    }
    /* A release may only activate the same geometry that received its press. */
    for (unsigned i = 0; i < c->surface.count; ++i) {
        const KitUiInteractionControl *old = &c->surface.controls[i];
        if (old->id != c->surface.interaction.captured_id &&
            old->id != c->surface.interaction.key_owner_id) continue;
        for (unsigned j = 0; j < c->surface.next_count; ++j)
            if (old->id == c->surface.next_controls[j].id &&
                memcmp(&old->bounds, &c->surface.next_controls[j].bounds, sizeof(old->bounds)))
                PreviewSharedControlsCancel(c);
    }
    c->ready = kit_ui_surface_end(&c->surface).code == CORE_OK;
    if (!c->ready) PreviewSharedControlsCancel(c);
}
bool PreviewSharedControlsRoute(PreviewSharedControls *c, const SDL_Event *e,
                                PreviewSharedAction *action) {
    *action = PREVIEW_SHARED_NONE;
    if (!c->ready) return false;
    bool key = e->type == SDL_KEYDOWN || e->type == SDL_KEYUP;
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        c->blocked_release = false;
        c->keyboard_focus = false;
    }
    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT && c->blocked_release)
        return true;
    SDL_Event mapped = *e;
    if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_F6 && !e->key.repeat) {
        mapped.key.keysym.sym = SDLK_TAB;
        c->keyboard_focus = true;
    }
    if (key && (!c->keyboard_focus || e->key.keysym.sym == SDLK_ESCAPE)) return false;
    if (e->type == SDL_MOUSEWHEEL) {
        if (c->surface.interaction.pointer_owned || c->surface.interaction.key_owner_id)
            PreviewSharedControlsCancel(c);
        return false;
    }
    KitUiInteractionEvent event;
    KitUiInteractionResult result;
    if (!kit_ui_interaction_event_from_sdl(&mapped, &event)) return false;
    if (kit_ui_surface_route(&c->surface, &event, &result).code != CORE_OK) {
        PreviewSharedControlsCancel(c); return true;
    }
    KitUiSurfaceKey activated;
    if (result.activated_id && kit_ui_surface_take_activation(&c->surface, result.activated_id) &&
        kit_ui_surface_key(&c->surface, result.activated_id, &activated))
        *action = (PreviewSharedAction)activated.value;
    return result.consumed != 0;
}
