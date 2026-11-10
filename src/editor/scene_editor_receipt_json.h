#ifndef SCENE_EDITOR_RECEIPT_JSON_H
#define SCENE_EDITOR_RECEIPT_JSON_H
#include <stdbool.h>
#include <json-c/json.h>
/* Receipt fields are required, never JSON null. Build privately and publish only
 * after every child allocation and insertion succeeds. Own value on both paths. */
static inline bool SceneEditorReceiptJsonAdd(json_object *owner,const char *key,json_object *value){
    if(!owner || !value){if(value)json_object_put(value);return false;}
    if(json_object_object_add(owner,key,value)!=0){json_object_put(value);return false;}
    return true;
}
#endif
