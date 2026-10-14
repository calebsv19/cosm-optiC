#include "scene_editor_motion_path_preserve.h"
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
static json_object *get(json_object *o,const char *k){json_object *v=NULL;if(json_object_is_type(o,json_type_object))json_object_object_get_ex(o,k,&v);return v;}
static json_object *copy(json_object *o){json_object *v=NULL;if(o)json_object_deep_copy(o,&v,NULL);return v;}
static bool same(json_object *a,json_object *b){return (!a&&!b)||(a&&b&&json_object_equal(a,b));}
static const char *identity(json_object *o){const char *keys[]={"id","target_id","object_id"};for(int i=0;i<3;++i){json_object *v=get(o,keys[i]);if(json_object_is_type(v,json_type_string))return json_object_get_string(v);}return NULL;}
static size_t find(json_object *a,const char *id){if(json_object_is_type(a,json_type_array)&&id)for(size_t i=0;i<json_object_array_length(a);++i){const char *key=identity(json_object_array_get_idx(a,i));if(key&&!strcmp(id,key))return i;}return SIZE_MAX;}
json_object *SceneEditorMotionRetain(json_object *raw,json_object *before,json_object *after){
 if(!raw)return copy(after);
 if(same(before,after))return copy(raw);
 if(json_object_is_type(after,json_type_object)){
  json_object *out=json_object_is_type(raw,json_type_object)?copy(raw):json_object_new_object();
  if(json_object_is_type(before,json_type_object)){json_object_object_foreach(before,k,v){(void)v;if(!get(after,k))json_object_object_del(out,k);}}
  json_object_object_foreach(after,k,v){json_object *old=get(before,k),*saved=get(raw,k);if(same(old,v)){/* Keep optional omissions and old representations. */if(saved)continue;if(old)continue;}json_object_object_add(out,k,SceneEditorMotionRetain(saved,old,v));}return out;
 }
 if(json_object_is_type(after,json_type_array)&&json_object_array_length(after)&&identity(json_object_array_get_idx(after,0))){
  json_object *out=json_object_new_array();for(size_t i=0;i<json_object_array_length(after);++i){json_object *row=json_object_array_get_idx(after,i);const char *id=identity(row);size_t b=find(before,id),s=find(raw,id);json_object_array_add(out,SceneEditorMotionRetain(s==SIZE_MAX?NULL:json_object_array_get_idx(raw,s),b==SIZE_MAX?NULL:json_object_array_get_idx(before,b),row));}return out;
 }return copy(after);
}
