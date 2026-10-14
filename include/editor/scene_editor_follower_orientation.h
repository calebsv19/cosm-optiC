#ifndef SCENE_EDITOR_FOLLOWER_ORIENTATION_H
#define SCENE_EDITOR_FOLLOWER_ORIENTATION_H
#include "motion/scene_motion_paths.h"
bool SceneEditorFollowerOrientationRead(const char *target,MotionPathBinding *out);
bool SceneEditorFollowerOrientationSet(const char *target,const MotionPathBinding *settings,unsigned long long revision,char *message,size_t size);
#endif
