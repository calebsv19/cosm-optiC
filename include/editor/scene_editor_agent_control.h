#ifndef SCENE_EDITOR_AGENT_CONTROL_H
#define SCENE_EDITOR_AGENT_CONTROL_H
#include <stdbool.h>
#include <json-c/json.h>
/* Main-thread commands: same retained operations as the editor, never file edits
 * behind an open document. Poll is bounded and nonblocking. */
bool SceneEditorAgentControlPoll(void);
void SceneEditorAgentControlClose(void);
json_object* SceneEditorAgentControlDispatch(json_object* request);
#endif
