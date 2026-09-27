#ifndef SCENE_EDITOR_CAMERA_AUTHORING_H
#define SCENE_EDITOR_CAMERA_AUTHORING_H
#include "camera/camera_path_3d.h"
#include <stddef.h>
bool SceneEditorDocumentSetCameraPath(const Path* path, const CameraPath3D* depth,
    unsigned long long revision, char* diagnostics, size_t size);
bool SceneEditorCameraGestureBegin(void);
bool SceneEditorCameraGestureValid(void);
bool SceneEditorCameraGestureCommit(void);
void SceneEditorCameraGestureCancel(void);
bool SceneEditorCameraGestureActive(void);
#endif
