#include "app/preview_workspace_render.h"

#include <stdio.h>

#include "editor/scene_editor_shared_text.h"
#include "ui/shared_theme_font_adapter.h"

static SDL_Rect preview_workspace_sdl_rect(PreviewWorkspaceRect rect) {
  return (SDL_Rect){rect.x, rect.y, rect.w, rect.h};
}

void PreviewWorkspaceRender(SDL_Renderer *renderer,
                            const PreviewWorkspace *workspace,
                            const PreviewSharedControls *controls) {
  RayTracingThemePalette palette = {0};
  (void)ray_tracing_shared_theme_resolve_palette(&palette);
  SDL_Rect panel;
  SDL_Rect track;
  SDL_Rect fill;
  SDL_Rect knob;
  SDL_Rect frame_label;
  double normalized;
  char frame_text[96];

  if (!renderer || !workspace || !workspace->valid)
    return;
  panel = preview_workspace_sdl_rect(workspace->layout.panel);
  track = preview_workspace_sdl_rect(workspace->layout.slider_track);
  track.y += 6;
  track.h = 6;
  frame_label = preview_workspace_sdl_rect(workspace->layout.frame_label);
  normalized = PreviewWorkspaceNormalizedPosition(workspace);

  SDL_SetRenderDrawColor(renderer, palette.panel_fill.r, palette.panel_fill.g, palette.panel_fill.b, palette.panel_fill.a);
  SDL_RenderFillRect(renderer, &panel);
  SDL_SetRenderDrawColor(renderer, palette.panel_border.r, palette.panel_border.g, palette.panel_border.b, palette.panel_border.a);
  SDL_RenderDrawLine(renderer, panel.x, panel.y, panel.x + panel.w, panel.y);

  PreviewSharedButtonRender(renderer, controls, PREVIEW_SHARED_PLAY, preview_workspace_sdl_rect(workspace->layout.play_pause_button),
                                workspace->transport.playing ? "Pause" : "Play",
                                workspace->transport.playing);
  PreviewSharedButtonRender(renderer, controls, PREVIEW_SHARED_LOOP, preview_workspace_sdl_rect(workspace->layout.loop_button), "Loop",
                                workspace->transport.mode ==
                                    PREVIEW_TRANSPORT_MODE_LOOP);
  PreviewSharedButtonRender(
      renderer, controls, PREVIEW_SHARED_BOUNCE, preview_workspace_sdl_rect(workspace->layout.bounce_button), "Bounce",
      workspace->transport.mode == PREVIEW_TRANSPORT_MODE_BOUNCE);

  SDL_SetRenderDrawColor(renderer, 82, 88, 102, 255);
  SDL_RenderFillRect(renderer, &track);
  fill = track;
  fill.w = (int)(normalized * (double)track.w);
  SDL_SetRenderDrawColor(renderer, 92, 156, 224, 255);
  SDL_RenderFillRect(renderer, &fill);
  knob = (SDL_Rect){track.x + fill.w - 5, track.y - 6, 10, 18};
  SDL_SetRenderDrawColor(renderer, 218, 230, 244, 255);
  SDL_RenderFillRect(renderer, &knob);

  snprintf(frame_text, sizeof(frame_text), "Frame %lld / %lld",
           (long long)workspace->transport.sample.absolute_frame,
           (long long)(workspace->transport.range.start_frame +
                       (int64_t)workspace->transport.range.frame_count - 1));
  scene_editor_shared_label(renderer, frame_label, frame_text, palette.text_primary, 13, 0, 0);
}
