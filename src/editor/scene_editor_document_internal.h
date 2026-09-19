#ifndef SCENE_EDITOR_DOCUMENT_INTERNAL_H
#define SCENE_EDITOR_DOCUMENT_INTERNAL_H
#include <json-c/json.h>
#include "editor/scene_editor_document.h"
json_object* SceneEditorDocumentRetainedRoot(void);
json_object* document_object_for_scene_index(int index,char* diagnostics,size_t size);
bool document_begin_command(char* diagnostics,size_t size);
bool document_finish_command(char* diagnostics,size_t size);
#endif
