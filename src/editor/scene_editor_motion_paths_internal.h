#ifndef SCENE_EDITOR_MOTION_PATHS_INTERNAL_H
#define SCENE_EDITOR_MOTION_PATHS_INTERNAL_H
#include "motion/scene_motion_paths.h"
bool SceneEditorMotionPathsCommit(const MotionPaths *paths,
    const TimelineDocument *timeline, unsigned long long revision,
    char *message, size_t size);
#endif
