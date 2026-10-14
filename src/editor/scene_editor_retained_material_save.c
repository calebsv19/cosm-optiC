/* Compare the current runtime export to its document-load baseline. Retained
 * rows (including rowless legacy omissions) remain authoritative if unchanged;
 * genuine legacy runtime material edits still export normally. */
#include "scene_editor_retained_material_save.h"
#include "scene_editor_document_transaction.h"
#include "editor/scene_editor_document.h"
#include <string.h>
static json_object *baseline;
static json_object *get(json_object *o,const char *k){json_object *v=NULL;if(o)json_object_object_get_ex(o,k,&v);return v;}
static json_object *row(json_object *rows,const char *id){if(json_object_is_type(rows,json_type_array))for(size_t i=0;i<json_object_array_length(rows);++i){json_object *r=json_object_array_get_idx(rows,i),*v=get(r,"object_id");if(v&&!strcmp(id,json_object_get_string(v)))return r;}return NULL;}
void SceneEditorRetainedMaterialBaseline(json_object *encoded){json_object_put(baseline);baseline=json_object_get(encoded);}
json_object *SceneEditorRetainedMaterialsForSave(json_object *encoded){
 if(!baseline||!SceneEditorDocumentIsOpen())return json_object_get(encoded);
 json_object *author=get(get(get(document_authoring_root(),"extensions"),"ray_tracing"),"authoring"),*retained=get(author,"object_materials"),*out=json_object_new_array();
 for(size_t i=0;i<json_object_array_length(encoded);++i){json_object *current=json_object_array_get_idx(encoded,i),*id=get(current,"object_id");if(!id){json_object_array_add(out,json_object_get(current));continue;}
  json_object *old=row(baseline,json_object_get_string(id)),*saved=row(retained,json_object_get_string(id));
  if(old&&json_object_equal(old,current)){if(saved)json_object_array_add(out,json_object_get(saved));}
  else json_object_array_add(out,json_object_get(current));
 }return out;
}
