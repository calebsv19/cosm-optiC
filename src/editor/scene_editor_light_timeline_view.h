#ifndef RAY_TRACING_SCENE_EDITOR_LIGHT_TIMELINE_VIEW_H
#define RAY_TRACING_SCENE_EDITOR_LIGHT_TIMELINE_VIEW_H

#include <SDL2/SDL.h>

#include "import/runtime_scene_light_timeline_io.h"

typedef struct SceneEditorLightTimelineView {
    double start_normalized;
    double span_normalized;
} SceneEditorLightTimelineView;

typedef struct SceneEditorLightTimelinePanelGeometry {
    SDL_Rect metrics_line;
    SDL_Rect timing_graph;
    SDL_Rect path_point_strip;
    SDL_Rect speed_strip;
    SDL_Rect footer_hint;
    SDL_Rect add_key_button;
    SDL_Rect play_button;
    SDL_Rect constant_speed_button;
    SDL_Rect equal_segments_button;
    SDL_Rect custom_mode_indicator;
} SceneEditorLightTimelinePanelGeometry;

void scene_editor_light_timeline_panel_geometry(
    const SDL_Rect* panel,
    SceneEditorLightTimelinePanelGeometry* out_geometry);

void scene_editor_light_timeline_view_reset(
    SceneEditorLightTimelineView* view);

void scene_editor_light_timeline_view_zoom(
    SceneEditorLightTimelineView* view,
    double anchor_normalized,
    double factor,
    double minimum_span);

void scene_editor_light_timeline_view_pan(
    SceneEditorLightTimelineView* view,
    double delta_normalized);

double scene_editor_light_timeline_view_normalized_at_x(
    const SceneEditorLightTimelineView* view,
    const SDL_Rect* graph,
    int x);

int scene_editor_light_timeline_view_x_at_normalized(
    const SceneEditorLightTimelineView* view,
    const SDL_Rect* graph,
    double normalized);

double scene_editor_light_timeline_progress_at_y(
    const SDL_Rect* graph,
    int y);

void scene_editor_light_timeline_key_point(
    const SceneEditorLightTimelineView* view,
    const RuntimeSceneLightTimelineDocument* document,
    const SDL_Rect* graph,
    const TimelineKeyframe* key,
    int* out_x,
    int* out_y);

int scene_editor_light_timeline_pick_key(
    const SceneEditorLightTimelineView* view,
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    const SDL_Rect* graph,
    int x,
    int y);

double scene_editor_light_timeline_nice_ceiling(double value);

#endif
