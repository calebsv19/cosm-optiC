#pragma once
#include <stdbool.h>
#include <stddef.h>
/* Explicit, retained replacement. Relative bundle paths are scene-relative.
 * A failed admission leaves both authored state and active playback intact. */
bool SceneEditorDocumentSelectRigidResult(const char *bundle, const char *revision,
    unsigned long long expected_revision, char *diagnostic, size_t capacity);
bool SceneEditorDocumentValidateRigidResult(char *diagnostic, size_t capacity);
