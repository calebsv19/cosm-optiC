#ifndef SCENE_EDITOR_TIMELINE_H
#define SCENE_EDITOR_TIMELINE_H
#include "editor/scene_editor_pane_host.h"
#include "animation/evaluated_scene_snapshot.h"
#include "editor/scene_editor_digest_overlay.h"
void SceneEditorTimelineRenderEvaluatedMarkers(SDL_Renderer* renderer,
    const SceneEditorDigestOverlayProjector* projector);
bool SceneEditorTimelineAdvance(void);
void SceneEditorTimelinePause(void);
bool SceneEditorTimelineSelectedTrack(TimelineTrack* track,TimelineRate* rate,TimelineRange* range,TimelineSample* sample);
void SceneEditorTimelineReleaseFocus(void);
bool SceneEditorTimelineHandleEvent(SDL_Event* event, const SceneEditorPaneLayout* layout);
void SceneEditorTimelineRender(SDL_Renderer* renderer, const SceneEditorPaneLayout* layout);
bool SceneEditorTimelineActivate(void);
bool SceneEditorTimelineSetKey(double value);
bool SceneEditorTimelineAddChannel(const char* property);
bool SceneEditorTimelineDeleteKey(void);
bool SceneEditorTimelineMoveKey(int64_t destination_frame);
bool SceneEditorTimelineSetInterpolation(TimelineInterpolation interpolation);
bool SceneEditorTimelineSetHandles(double incoming_frames, double incoming_value,
    double outgoing_frames, double outgoing_value);
bool SceneEditorTimelineSelectTrack(size_t index);
bool SceneEditorTimelineSeek(int64_t frame);
bool SceneEditorTimelineCurrentSample(TimelineSample* sample);
bool SceneEditorTimelineSeekSample(TimelineSample sample);
bool SceneEditorTimelineCopyEvaluated(RayEvaluatedSceneSnapshot* out);
#endif
