#include "app/preview_shared_controls.h"
#include "editor/scene_editor_shared_text.h"
#include "ui/shared_theme_font_adapter.h"
static CoreThemeColor color(SDL_Color c) { return (CoreThemeColor){c.r,c.g,c.b,c.a}; }
void PreviewSharedButtonRender(SDL_Renderer *renderer, const PreviewSharedControls *controls,
                              PreviewSharedAction action, SDL_Rect r, const char *label, bool selected) {
    RayTracingThemePalette p = {0};
    if (!ray_tracing_shared_theme_resolve_palette(&p)) return;
    KitUiButtonState state = {.selected = selected};
    for (unsigned i = 0; i < controls->surface.count; ++i)
        if (controls->surface.keys[i].value == (uint64_t)action) {
            state = kit_ui_interaction_button_state(&controls->surface.interaction,
                                                    &controls->surface.controls[i], selected);
            break;
        }
    KitUiButtonTheme theme = {color(p.button_fill),color(p.button_active_fill),
        color(p.button_active_fill),color(p.button_active_fill),color(p.panel_border),
        color(p.accent_primary),color(p.button_text),color(p.text_muted)};
    KitUiButtonSpec spec = {label,KIT_UI_BUTTON_VARIANT_DEFAULT,state};
    KitUiButtonStyle style;
    if (!kit_ui_button_style_resolve(&theme,&spec,&style)) return;
    int border = state.focused ? 2 : 1;
    SDL_Rect inner = {r.x+border,r.y+border,r.w-2*border,r.h-2*border};
    SDL_SetRenderDrawColor(renderer,style.outline.r,style.outline.g,style.outline.b,style.outline.a);
    SDL_RenderFillRect(renderer,&r);
    SDL_SetRenderDrawColor(renderer,style.fill.r,style.fill.g,style.fill.b,style.fill.a);
    SDL_RenderFillRect(renderer,&inner);
    SDL_Color fill = {style.fill.r,style.fill.g,style.fill.b,style.fill.a};
    scene_editor_shared_label(renderer,inner,label,ray_tracing_theme_choose_button_text(fill,p),13,0,1);
}
