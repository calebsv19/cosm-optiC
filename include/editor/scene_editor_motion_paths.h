#ifndef SCENE_EDITOR_MOTION_PATHS_H
#define SCENE_EDITOR_MOTION_PATHS_H
#include "editor/scene_editor.h"
#include "motion/scene_motion_paths.h"
bool SceneEditorMotionPathsRead(MotionPaths *out);
/* All writes are one revision-checked, validated scene undo command. */
bool SceneEditorMotionPathsSet(const MotionPaths *paths,
                               unsigned long long revision, char *message,
                               size_t size);
bool SceneEditorMotionPathBind(const char *object_id, const char *path_id,
                               bool attach, unsigned long long revision,
                               char *message, size_t size);
bool SceneEditorMotionPathBindCamera(const char *path_id, bool attach,
    unsigned long long revision, char *message, size_t size);
bool SceneEditorMotionPathCreate(const char *name, const double origin[3],
                                 double length, unsigned long long revision,
                                 char *id, size_t id_size, char *message,
                                 size_t size);
void SceneEditorMotionPathPanelReset(void);
void SceneEditorMotionPathPanelSelect(bool selected);
bool SceneEditorMotionPathPanelActive(void);
bool SceneEditorMotionPathPanelEvent(SceneEditor *editor, SDL_Event *event,
                                     const SceneEditorPaneLayout *layout);
void SceneEditorMotionPathOverlayDraw(SceneEditor *editor,
                                      const SceneEditorPaneLayout *layout);
void SceneEditorMotionPathPanelDraw(SceneEditor *editor,
                                    const SceneEditorPaneLayout *layout);
bool SceneEditorMotionPathPanelControl(const char *name, SDL_Rect *out);
#endif
