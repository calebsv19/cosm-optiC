#include <assert.h>
#include "l3_json_faults.h"
#include "../src/editor/scene_editor_managed_import.c"
const char *SceneEditorDocumentPath(void){return "/l3/current.json";}
int main(void){char diagnostic[256];SceneEditorManagedImportStarted("import-a","/l3/current.json");SceneEditorManagedImportFinished(true,"done");json_object *response=json_tokener_parse("{\"preserved\":true,\"import\":{\"prior\":true}}");assert(response);assert(SceneEditorManagedImportRead("import-a",response,diagnostic,sizeof(diagnostic)));int positions=l3_json_index;json_object_put(response);
 for(int n=1;n<=positions;n++){l3_json_fail=0;response=json_tokener_parse("{\"preserved\":true,\"import\":{\"prior\":true}}");json_object *prior=json_tokener_parse(json_object_to_json_string(response));l3_json_index=0;l3_json_fail=n;bool ok=SceneEditorManagedImportRead("import-a",response,diagnostic,sizeof(diagnostic));printf("import fault %d/%d ok=%d\n",n,positions,ok);fflush(stdout);assert(!ok);assert(json_object_equal(prior,response));json_object_put(prior);json_object_put(response);}
 puts("PASS every import receipt allocation/add refusal leaves caller result intact");return 0;}
