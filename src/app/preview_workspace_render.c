#include "app/preview_workspace_render.h"

#include <stdio.h>

#include "render/render_helper.h"

static SDL_Rect preview_workspace_sdl_rect(PreviewWorkspaceRect rect) {
  return (SDL_Rect){rect.x, rect.y, rect.w, rect.h};
}

static void preview_workspace_draw_button(SDL_Renderer *renderer,
                                          PreviewWorkspaceRect workspace_rect,
                                          const char *label, bool active) {
  SDL_Rect rect = preview_workspace_sdl_rect(workspace_rect);
  SDL_Color fill =
      active ? (SDL_Color){66, 110, 164, 255} : (SDL_Color){50, 54, 64, 255};
  SDL_Color border = active ? (SDL_Color){126, 186, 246, 255}
                            : (SDL_Color){104, 110, 124, 255};
  SDL_Color text = {232, 236, 244, 255};
  SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
  SDL_RenderDrawRect(renderer, &rect);
  RenderButtonTextWithColor(renderer, rect, label, text);
}

void PreviewWorkspaceRender(SDL_Renderer *renderer,
                            const PreviewWorkspace *workspace) {
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

  SDL_SetRenderDrawColor(renderer, 28, 31, 38, 244);
  SDL_RenderFillRect(renderer, &panel);
  SDL_SetRenderDrawColor(renderer, 72, 78, 90, 255);
  SDL_RenderDrawLine(renderer, panel.x, panel.y, panel.x + panel.w, panel.y);

  preview_workspace_draw_button(renderer, workspace->layout.play_pause_button,
                                workspace->transport.playing ? "Pause" : "Play",
                                workspace->transport.playing);
  preview_workspace_draw_button(renderer, workspace->layout.loop_button, "Loop",
                                workspace->transport.mode ==
                                    PREVIEW_TRANSPORT_MODE_LOOP);
  preview_workspace_draw_button(
      renderer, workspace->layout.bounce_button, "Bounce",
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
  RenderLabelTextLeft(renderer, frame_label, frame_text,
                      (SDL_Color){218, 224, 234, 255});
}
