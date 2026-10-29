#pragma once
#include <stdbool.h>
#include <stddef.h>
/* Explicit local producer profile. No shell, no worker routing, no hidden
 * defaults. Completion opens a new prepared scene only if the source is saved
 * and still unchanged. Failure keeps the current scene/result. */
bool SceneEditorRigidJobBegin(const char *profile,char *message,size_t size);
bool SceneEditorRigidJobBusy(void);
void SceneEditorRigidJobPoll(char *message,size_t size);
