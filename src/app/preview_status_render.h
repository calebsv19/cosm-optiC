#ifndef RAY_TRACING_PREVIEW_STATUS_RENDER_H
#define RAY_TRACING_PREVIEW_STATUS_RENDER_H
#include <SDL2/SDL.h>
#include "app/preview_mode_route.h"
#include "app/evaluated_scene_service.h"
#include "app/preview_retained_scene_quality.h"
void PreviewStatusRender(SDL_Renderer *, int width, int panel_y, bool inspection_selected,
                         const PreviewModeRouteDecision *,
                         const RayEvaluatedSceneServiceResult *, PreviewRetainedSceneQuality);
#endif
