/* Spatial geometry adapter; retained UI commands own validation and history. */
#include "editor/scene_editor_agent_paths.h"
#include "editor/scene_editor_motion_paths.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_timeline.h"
#include "import/runtime_scene_light_timeline_io.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static json_object *get(json_object *o,const char *k) {json_object *v=NULL;if(json_object_is_type(o,json_type_object))json_object_object_get_ex(o,k,&v);return v;}
static const char *text(json_object *o,const char *k) {json_object *v=get(o,k);if(!json_object_is_type(v,json_type_string))return NULL;const char *s=json_object_get_string(v);return strlen(s)==(size_t)json_object_get_string_len(v)?s:NULL;}
static bool fields(json_object *o,const char *const *ks,size_t n) {if(!json_object_is_type(o,json_type_object)||json_object_object_length(o)!=(int)n)return false;for(size_t i=0;i<n;++i)if(!get(o,ks[i]))return false;return true;}
static bool vector(json_object *o,const char *k,double v[3]) {json_object *a=get(o,k);if(!json_object_is_type(a,json_type_array)||json_object_array_length(a)!=3)return false;for(int i=0;i<3;++i){json_object *x=json_object_array_get_idx(a,i);if(!json_object_is_type(x,json_type_int)&&!json_object_is_type(x,json_type_double))return false;v[i]=json_object_get_double(x);if(!isfinite(v[i])||fabs(v[i])>1e12)return false;}return true;}
static bool output(json_object *r,const char *k,json_object *v,char *m,size_t n) {if(!v)return false;if(strlen(json_object_to_json_string_ext(v,JSON_C_TO_STRING_PLAIN))>42000){json_object_put(v);snprintf(m,n,"Path read exceeds capacity; read individual paths.");return false;}json_object_object_add(r,k,v);return true;}
bool SceneEditorAgentPathsReadOnly(const char *name) {return !strcmp(name,"path.list")||!strcmp(name,"path.read");}
bool SceneEditorAgentPathsDispatch(const char *name,json_object *p,json_object *response,char *m,size_t n) {
 MotionPaths d;if(!SceneEditorMotionPathsRead(&d)){snprintf(m,n,"Cannot read retained motion paths.");return false;}
 unsigned long long rev=SceneEditorDocumentRevision();const char *id=text(p,"path_id"),*point_id=text(p,"point_id");MotionPath *path=NULL;size_t index=0,point=SIZE_MAX;
 for(size_t i=0;id&&i<d.count;++i)if(!strcmp(id,d.paths[i].id)){path=&d.paths[i];index=i;break;}
 if(path)for(size_t i=0;point_id&&i<path->count;++i)if(!strcmp(point_id,path->points[i].id))point=i;
 if(!strcmp(name,"path.list")) {
  if(!fields(p,NULL,0))goto invalid;json_object *encoded=MotionPathsToJson(&d),*rows=get(encoded,"paths"),*summary=json_object_new_array();
  for(size_t i=0;i<d.count;++i){json_object *row=json_object_new_object();json_object_object_add(row,"id",json_object_new_string(d.paths[i].id));json_object_object_add(row,"name",json_object_new_string(d.paths[i].name));json_object_object_add(row,"point_count",json_object_new_int64(d.paths[i].count));json_object_array_add(summary,row);}
  (void)rows;json_object_object_del(encoded,"paths");json_object_object_add(encoded,"paths",summary);json_object_object_add(encoded,"world_scale",json_object_new_double(SceneEditorDocumentWorldScale()));return output(response,"motion_paths",encoded,m,n);
 }
 if(!strcmp(name,"path.read")) {const char *ks[]={"path_id"};if(!fields(p,ks,1)||!path)goto invalid;json_object *encoded=MotionPathsToJson(&d);json_object *row=json_object_get(json_object_array_get_idx(get(encoded,"paths"),index));json_object_put(encoded);return output(response,"path",row,m,n);}
 if(!strcmp(name,"path.create")) {const char *ks[]={"name","origin","length","expected_revision"};double origin[3];json_object *v=get(p,"length");const char *label=text(p,"name");if(!fields(p,ks,4)||!label||!vector(p,"origin",origin)||(!json_object_is_type(v,json_type_int)&&!json_object_is_type(v,json_type_double)))goto invalid;char created[64];if(!SceneEditorMotionPathCreate(label,origin,json_object_get_double(v),rev,created,sizeof(created),m,n))return false;json_object_object_add(response,"path_id",json_object_new_string(created));return true;}
 if(!strcmp(name,"path.bind")) {const char *ks[]={"target_id","path_id","attach","expected_revision"};const char *target=text(p,"target_id");json_object *v=get(p,"attach");if(!fields(p,ks,4)||!target||!id||!json_object_is_type(v,json_type_boolean))goto invalid;bool attach=json_object_get_boolean(v);if(!strcmp(target,"camera/main"))return SceneEditorMotionPathBindCamera(id,attach,rev,m,n);
  if(!strncmp(target,"object/",7)&&target[7])return SceneEditorMotionPathBind(target+7,id,attach,rev,m,n);
  static RuntimeSceneLightTimelineDocument light;if(!RuntimeSceneLightTimelineGetLast(&light)||light.progress_track_index>=light.timeline.track_count||strcmp(target,light.timeline.tracks[light.progress_track_index].target_id))goto invalid;
  return SceneEditorMotionPathBindLight(id,attach,rev,m,n);
 }
 if(!strcmp(name,"path.prepare_light")) {const char *ks[]={"expected_revision"};if(!fields(p,ks,1))goto invalid;return SceneEditorMotionPathPrepareLight(m,n);}
 if(!strcmp(name,"path.convert_legacy")) {const char *ks[]={"target","expected_revision"};const char *target=text(p,"target");if(!fields(p,ks,2)||!target||(strcmp(target,"camera")&&strcmp(target,"light")))goto invalid;return SceneEditorMotionPathConvertLegacy(!strcmp(target,"camera"),rev,m,n);}
 if(!path)goto invalid;
 if(!strcmp(name,"path.rename")){const char *ks[]={"path_id","name","expected_revision"};const char *label=text(p,"name");if(!fields(p,ks,3)||!label||!*label||strlen(label)>=sizeof(path->name))goto invalid;snprintf(path->name,sizeof(path->name),"%s",label);}
 else if(!strcmp(name,"path.remove")){const char *ks[]={"path_id","expected_revision"};if(!fields(p,ks,2))goto invalid;for(size_t i=0;i<d.binding_count;++i)if(!strcmp(id,d.bindings[i].path_id)&&d.bindings[i].enabled){snprintf(m,n,"Detach every follower before deleting its path.");return false;}for(size_t i=0;i<d.binding_count;)if(!strcmp(id,d.bindings[i].path_id))memmove(&d.bindings[i],&d.bindings[i+1],(--d.binding_count-i)*sizeof(d.bindings[0]));else ++i;memmove(path,path+1,(--d.count-index)*sizeof(*path));}
 else if(point==SIZE_MAX)goto invalid;
 else if(!strcmp(name,"path.set_point")){const char *ks[]={"path_id","point_id","position","incoming","outgoing","segment","expected_revision"};double pos[3],in[3],out[3];const char *segment=text(p,"segment");if(!fields(p,ks,7)||!segment||(strcmp(segment,"line")&&strcmp(segment,"cubic"))||!vector(p,"position",pos)||!vector(p,"incoming",in)||!vector(p,"outgoing",out))goto invalid;
  MotionPathPoint *v=&path->points[point];/* Full offsets apply exactly; linked policy must be consistent, never silently break it. */
  if(v->handle_mode==MOTION_HANDLE_CORNER && (in[0]||in[1]||in[2]||out[0]||out[1]||out[2]))goto invalid;
  if(v->handle_mode==MOTION_HANDLE_LINKED){double cross[3]={in[1]*out[2]-in[2]*out[1],in[2]*out[0]-in[0]*out[2],in[0]*out[1]-in[1]*out[0]},dot=0,scale=0;for(int i=0;i<3;++i){dot+=in[i]*out[i];scale+=in[i]*in[i]+out[i]*out[i];}for(int i=0;i<3;++i)if(fabs(cross[i])>1e-9*fmax(1,scale))goto invalid;if(dot>0)goto invalid;}
  memcpy(v->position,pos,sizeof(pos));memcpy(v->incoming,in,sizeof(in));memcpy(v->outgoing,out,sizeof(out));v->linear=!strcmp(segment,"line");}
 else if(!strcmp(name,"path.point_mode")){const char *ks[]={"path_id","point_id","mode","expected_revision"};const char *mode=text(p,"mode");if(!fields(p,ks,4)||!mode)goto invalid;bool ok=!strcmp(mode,"smooth")?MotionPathSmoothPoint(path,point):!strcmp(mode,"corner")?MotionPathSetHandleMode(&path->points[point],MOTION_HANDLE_CORNER):!strcmp(mode,"independent")?MotionPathSetHandleMode(&path->points[point],MOTION_HANDLE_INDEPENDENT):false;if(!ok)goto invalid;}
 else if(!strcmp(name,"path.add_point")){const char *ks[]={"path_id","point_id","expected_revision"};if(!fields(p,ks,3)||path->count>=MOTION_POINT_CAPACITY)goto invalid;char created[64];if(!SceneEditorMotionPathInsertPoint(path,point,created,sizeof(created)))goto invalid;json_object_object_add(response,"point_id",json_object_new_string(created));}
 else if(!strcmp(name,"path.remove_point")){const char *ks[]={"path_id","point_id","expected_revision"};if(!fields(p,ks,3)||path->count<=2)goto invalid;memmove(&path->points[point],&path->points[point+1],(--path->count-point)*sizeof(path->points[0]));}
 else goto invalid;
 return SceneEditorMotionPathsSet(&d,rev,m,n);
invalid:snprintf(m,n,"Invalid spatial path arguments, unavailable identity, handle policy, or light source. Prepare light before attaching; positions/handle offsets use authored scene units.");return false;
}
