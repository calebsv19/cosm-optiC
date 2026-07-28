#ifndef PREVIEW_RETAINED_SCENE_SURFACE_H
#define PREVIEW_RETAINED_SCENE_SURFACE_H

#include <stdbool.h>
#include <stddef.h>

#include <SDL2/SDL.h>

#include "app/preview_camera_projector.h"
#include "app/preview_retained_scene_quality.h"

typedef struct PreviewRetainedSceneSurfaceStats {
    PreviewRetainedSceneQuality quality;
    size_t rendered_triangles;
    int rendered_instances;
} PreviewRetainedSceneSurfaceStats;

void PreviewRetainedSceneSurfacePrepare(void);

bool PreviewRetainedSceneSurfaceRender(
    SDL_Renderer* renderer,
    const PreviewCameraProjector* projector,
    const PreviewRetainedSceneFrame* frame,
    PreviewRetainedSceneSurfaceStats* out_stats);

void PreviewRetainedSceneSurfaceReset(SDL_Renderer* renderer);

#endif
