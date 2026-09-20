#ifndef SCENE_EDITOR_SURFACES_H
#define SCENE_EDITOR_SURFACES_H
#include "ui/shared_theme_font_adapter.h"

/* App-local presentation roles, derived from core_theme rather than RGB literals.
 * Apply once to a raw shared palette. Opaque surfaces keep nested panes stable. */
static inline SDL_Color SceneEditorSurfaceBlend(SDL_Color a, SDL_Color b, unsigned percent) {
    return (SDL_Color){(a.r*(100-percent)+b.r*percent)/100,
                       (a.g*(100-percent)+b.g*percent)/100,
                       (a.b*(100-percent)+b.b*percent)/100,255};
}
static inline RayTracingThemePalette SceneEditorSurfacePalette(RayTracingThemePalette raw) {
    RayTracingThemePalette out=raw;
    out.background_fill.a=255;
    out.panel_fill=SceneEditorSurfaceBlend(raw.panel_fill,raw.button_fill,65);
    out.panel_border=SceneEditorSurfaceBlend(raw.button_fill,raw.text_primary,17);
    out.button_fill=SceneEditorSurfaceBlend(raw.button_fill,raw.text_primary,6);
    return out;
}
static inline SDL_Color SceneEditorSurfaceGroup(RayTracingThemePalette palette) {
    return SceneEditorSurfaceBlend(palette.panel_fill,palette.button_fill,40);
}
static inline void SceneEditorSurfaceFill(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer,color.r,color.g,color.b,255);
    SDL_RenderFillRect(renderer,&rect);
}
#endif
