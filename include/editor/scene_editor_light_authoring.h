#ifndef SCENE_EDITOR_LIGHT_AUTHORING_H
#define SCENE_EDITOR_LIGHT_AUTHORING_H
#include "camera/camera_path_3d.h"
#include <stddef.h>
bool SceneEditorDocumentSetLightPath(const Path* path,const CameraPath3D* depth,
    unsigned long long revision,char* diagnostics,size_t size);
bool SceneEditorLightGestureBegin(void);
bool SceneEditorLightGestureActive(void);
bool SceneEditorLightGestureValid(void);
bool SceneEditorLightGestureCommit(void);
void SceneEditorLightGestureCancel(void);
#endif
