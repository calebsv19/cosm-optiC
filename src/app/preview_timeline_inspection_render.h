#ifndef RAY_TRACING_PREVIEW_TIMELINE_INSPECTION_RENDER_H
#define RAY_TRACING_PREVIEW_TIMELINE_INSPECTION_RENDER_H

#include <SDL2/SDL.h>

#include "app/preview_timeline_inspection.h"

void PreviewTimelineInspectionRender(
    SDL_Renderer *renderer, const PreviewWorkspace *workspace,
    const PreviewTimelineInspection *inspection);

#endif
