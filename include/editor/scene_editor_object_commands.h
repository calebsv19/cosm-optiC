#ifndef SCENE_EDITOR_OBJECT_COMMANDS_H
#define SCENE_EDITOR_OBJECT_COMMANDS_H
#include "editor/scene_editor_document.h"
typedef enum SceneEditorObjectCommand {
    SCENE_OBJECT_SELECT, SCENE_OBJECT_RENAME, SCENE_OBJECT_VISIBILITY, SCENE_OBJECT_LOCK
} SceneEditorObjectCommand;
typedef struct SceneEditorObjectReadback {
    SceneEditorDocumentObjectInfo selection;
    unsigned long long revision;
    bool has_selection, dirty, can_undo, can_redo;
    int selection_count; /* U2.3 supports zero or one object. */
} SceneEditorObjectReadback;
void SceneEditorObjectInspect(SceneEditorObjectReadback* out);
bool SceneEditorObjectExecute(SceneEditorObjectCommand command,const char* id,
    const char* name,bool value,unsigned long long expected_revision,
    SceneEditorObjectReadback* out,char* diagnostics,size_t size);
#endif
