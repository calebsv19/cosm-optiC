#include "app/preview_timeline_inspection_render.h"

#include "editor/scene_editor_shared_text.h"
#include "ui/shared_theme_font_adapter.h"

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
  RayTracingThemePalette palette = {0};
  (void)ray_tracing_shared_theme_resolve_palette(&palette);
  int available = workspace->layout.panel.y - 56;
  if (available <= 0) return;
  int height = available < 112 ? available : 112;
  readout = (SDL_Rect){8, workspace->layout.panel.y - height - 8,
                       workspace->layout.panel.w - 16, height};
  SDL_SetRenderDrawColor(renderer, palette.panel_fill.r, palette.panel_fill.g, palette.panel_fill.b, palette.panel_fill.a);
  SDL_RenderFillRect(renderer, &readout);
  SDL_SetRenderDrawColor(renderer, palette.panel_border.r, palette.panel_border.g, palette.panel_border.b, palette.panel_border.a);
  SDL_RenderDrawRect(renderer, &readout);
  const char *rows[] = {inspection->frame_line, inspection->channel_line,
      inspection->interpolation_line, inspection->provenance_line};
  int row_height = (height - 8) / 4;
  for (unsigned i = 0; i < 4; ++i) {
    line = (SDL_Rect){readout.x + 8, readout.y + 4 + (int)i * row_height,
                     readout.w - 16, row_height};
    scene_editor_shared_label(renderer, line, rows[i],
        i == 0 ? palette.accent_primary : palette.text_primary, 13, 0, 0);
  }
}
