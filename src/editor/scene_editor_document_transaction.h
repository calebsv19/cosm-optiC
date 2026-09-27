#ifndef SCENE_EDITOR_DOCUMENT_TRANSACTION_H
#define SCENE_EDITOR_DOCUMENT_TRANSACTION_H
#include <json-c/json.h>
#include <stdbool.h>
#include <stddef.h>
/* Internal borrowed root. Mutations require begin followed by finish/rollback.
 * Callers must never retain a pointer across commands or release this root. */
json_object* document_authoring_root(void);
bool document_begin_command(char* diagnostics, size_t size);
bool document_finish_command(char* diagnostics, size_t size);
void document_rollback_command(void);
#endif
