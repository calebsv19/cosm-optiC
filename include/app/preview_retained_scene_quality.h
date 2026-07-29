#ifndef PREVIEW_RETAINED_SCENE_QUALITY_H
#define PREVIEW_RETAINED_SCENE_QUALITY_H

#include <stdbool.h>

#include <SDL2/SDL.h>

#include "animation/evaluated_scene_snapshot.h"

typedef enum PreviewRetainedSceneQuality {
    PREVIEW_RETAINED_SCENE_QUALITY_WIREFRAME = 0,
    PREVIEW_RETAINED_SCENE_QUALITY_SOLID,
    PREVIEW_RETAINED_SCENE_QUALITY_INTERACTIVE_SHADED,
    PREVIEW_RETAINED_SCENE_QUALITY_COUNT
} PreviewRetainedSceneQuality;

typedef struct PreviewRetainedSceneFrame {
    PreviewRetainedSceneQuality quality;
    RayEvaluatedSceneSnapshot evaluated_scene;
} PreviewRetainedSceneFrame;

PreviewRetainedSceneQuality PreviewRetainedSceneQualityNormalize(
    PreviewRetainedSceneQuality quality);
PreviewRetainedSceneQuality PreviewRetainedSceneQualityCycle(
    PreviewRetainedSceneQuality quality);
const char* PreviewRetainedSceneQualityLabel(
    PreviewRetainedSceneQuality quality);
bool PreviewRetainedSceneQualityUsesSurface(
    PreviewRetainedSceneQuality quality);

bool PreviewRetainedSceneFrameBuild(
    PreviewRetainedSceneQuality quality,
    const RayEvaluatedSceneSnapshot* evaluated_scene,
    PreviewRetainedSceneFrame* out_frame);

SDL_Color PreviewRetainedSceneShadeColor(
    SDL_Color base,
    double normal_x,
    double normal_y,
    double normal_z,
    double world_x,
    double world_y,
    double world_z,
    const PreviewRetainedSceneFrame* frame);

#endif
