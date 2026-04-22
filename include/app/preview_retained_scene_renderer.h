#ifndef PREVIEW_RETAINED_SCENE_RENDERER_H
#define PREVIEW_RETAINED_SCENE_RENDERER_H

#include <SDL2/SDL.h>

#include "app/preview_camera_projector.h"
#include "import/runtime_scene_bridge.h"

#define PREVIEW_RETAINED_SCENE_MAX_LINE_SEGMENTS 256

typedef struct PreviewRetainedSceneLineSegment {
    double ax;
    double ay;
    double az;
    double bx;
    double by;
    double bz;
    SDL_Color color;
} PreviewRetainedSceneLineSegment;

int PreviewRetainedSceneBuildLineSegments(
    const RuntimeSceneBridge3DDigestState* digest,
    PreviewRetainedSceneLineSegment* out_segments,
    int max_segments);

void PreviewRetainedSceneRender(SDL_Renderer* renderer,
                                const RuntimeSceneBridge3DDigestState* digest,
                                const PreviewCameraProjector* projector);

void PreviewRetainedSceneRenderLightMarker(SDL_Renderer* renderer,
                                           const PreviewCameraProjector* projector,
                                           double world_x,
                                           double world_y,
                                           double world_z,
                                           SDL_Color color,
                                           int radius_pixels);

#endif
