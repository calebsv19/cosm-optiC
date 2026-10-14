#ifndef SCENE_EDITOR_TIMELINE_MANAGE_H
#define SCENE_EDITOR_TIMELINE_MANAGE_H
#include "animation/timeline_document.h"
bool SceneEditorTimelineSetClock(TimelineRate rate,TimelineRange range,unsigned long long revision,char* message,size_t size);
bool SceneEditorTimelineSetChannelEnabled(const char* id,bool enabled,unsigned long long revision,char* message,size_t size);
bool SceneEditorTimelineRemoveChannel(const char* id,unsigned long long revision,char* message,size_t size);
#endif
