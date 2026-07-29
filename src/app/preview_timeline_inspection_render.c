#include "app/preview_timeline_inspection_render.h"

#include "render/render_helper.h"

void PreviewTimelineInspectionRender(
    SDL_Renderer *renderer, const PreviewWorkspace *workspace,
    const PreviewTimelineInspection *inspection) {
  SDL_Rect readout;
  SDL_Rect line;
  size_t marker_index;
  if (!renderer || !workspace || !workspace->valid || !inspection ||
      !inspection->valid) {
    return;
  }

  for (marker_index = 0u; marker_index < inspection->marker_count;
       ++marker_index) {
    const PreviewTimelineMarker *marker = &inspection->markers[marker_index];
    int x = PreviewTimelineInspectionMarkerX(
        inspection, marker_index, workspace->layout.slider_track);
    bool selected =
        inspection->has_selection &&
        inspection->selected_marker_index == marker_index;
    SDL_Color color =
        !marker->track_enabled
            ? (SDL_Color){110, 112, 120, 220}
            : (selected ? (SDL_Color){255, 214, 92, 255}
                        : (SDL_Color){188, 152, 232, 235});
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(renderer, x, workspace->layout.slider_track.y - 4, x,
                       workspace->layout.slider_track.y +
                           workspace->layout.slider_track.h + 4);
    if (selected) {
      SDL_RenderDrawLine(renderer, x - 2,
                         workspace->layout.slider_track.y - 5, x + 2,
                         workspace->layout.slider_track.y - 5);
    }
  }

  if (!inspection->has_selection)
    return;
  readout = (SDL_Rect){12, workspace->layout.panel.y - 92,
                       workspace->layout.panel.w - 24, 84};
  SDL_SetRenderDrawColor(renderer, 24, 27, 34, 238);
  SDL_RenderFillRect(renderer, &readout);
  SDL_SetRenderDrawColor(renderer, 108, 86, 140, 255);
  SDL_RenderDrawRect(renderer, &readout);
  line = (SDL_Rect){readout.x + 10, readout.y + 5, readout.w - 20, 18};
  RenderLabelTextLeft(renderer, line, inspection->frame_line,
                      (SDL_Color){255, 218, 112, 255});
  line.y += 19;
  RenderLabelTextLeft(renderer, line, inspection->channel_line,
                      (SDL_Color){228, 230, 238, 255});
  line.y += 19;
  RenderLabelTextLeft(renderer, line, inspection->interpolation_line,
                      (SDL_Color){192, 198, 212, 255});
  line.y += 19;
  RenderLabelTextLeft(renderer, line, inspection->provenance_line,
                      (SDL_Color){168, 194, 224, 255});
}
