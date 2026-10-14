/* Typed MCP adapter for retained motion intent and follower orientation. */
#include "editor/scene_editor_agent_motion.h"
#include "editor/scene_editor_motion_plan.h"
#include "editor/scene_editor_follower_orientation.h"
#include "editor/scene_editor_document.h"
#include "scene_editor_document_transaction.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static json_object *get(json_object *o,const char *k){json_object *v=NULL;if(json_object_is_type(o,json_type_object))json_object_object_get_ex(o,k,&v);return v;}
static const char *text(json_object *o,const char *k){json_object *v=get(o,k);if(!json_object_is_type(v,json_type_string))return NULL;const char *s=json_object_get_string(v);return strlen(s)==(size_t)json_object_get_string_len(v)?s:NULL;}
static bool fields(json_object *o,const char *const *keys,size_t n){if(!json_object_is_type(o,json_type_object)||json_object_object_length(o)!=(int)n)return false;for(size_t i=0;i<n;++i)if(!get(o,keys[i]))return false;return true;}
static bool number(json_object *o,const char *k,double *v){json_object *x=get(o,k);if(!json_object_is_type(x,json_type_int)&&!json_object_is_type(x,json_type_double))return false;*v=json_object_get_double(x);return isfinite(*v);}
static bool vector(json_object *x,double out[3]){if(!json_object_is_type(x,json_type_array)||json_object_array_length(x)!=3)return false;for(int i=0;i<3;++i){json_object *v=json_object_array_get_idx(x,i);if(!json_object_is_type(v,json_type_int)&&!json_object_is_type(v,json_type_double))return false;out[i]=json_object_get_double(v);if(!isfinite(out[i]))return false;}return true;}
static bool request(json_object *o,MotionTimingScheduleRequest *r){const char *keys[]={"start_time","max_speed","acceleration","braking","waypoints"};if(!fields(o,keys,5))return false;memset(r,0,sizeof(*r));if(!number(o,keys[0],&r->start_time)||!number(o,keys[1],&r->max_speed)||!number(o,keys[2],&r->acceleration)||!number(o,keys[3],&r->braking))return false;json_object *rows=get(o,"waypoints");if(!json_object_is_type(rows,json_type_array))return false;r->count=json_object_array_length(rows);if(r->count<2||r->count>MOTION_TIMING_WAYPOINT_CAPACITY)return false;
 for(size_t i=0;i<r->count;++i){json_object *p=json_object_array_get_idx(rows,i);const char *ks[]={"progress","speed","hold","fixed_arrival","arrival"};MotionTimingWaypoint *w=&r->points[i];json_object *fixed=get(p,"fixed_arrival");if(!fields(p,ks,5)||!number(p,ks[0],&w->position)||w->position<0||w->position>1||!number(p,ks[1],&w->speed)||!number(p,ks[2],&w->hold)||!number(p,ks[4],&w->arrival)||!json_object_is_type(fixed,json_type_boolean))return false;w->fixed_arrival=json_object_get_boolean(fixed);}return true;}
static json_object *vec(const double v[3]){json_object *a=json_object_new_array();for(int k=0;k<3;++k)json_object_array_add(a,json_object_new_double(v[k]));return a;}
static bool output(json_object *r,const char *k,json_object *v,char *m,size_t n){if(v&&strlen(json_object_to_json_string_ext(v,JSON_C_TO_STRING_PLAIN))>42000){json_object_put(v);snprintf(m,n,"Motion readback exceeds control capacity.");return false;}json_object_object_add(r,k,v);return true;}
bool SceneEditorAgentMotionReadOnly(const char *name){return !strcmp(name,"motion_plan.read")||!strcmp(name,"follower_orientation.read");}
bool SceneEditorAgentMotionDispatch(const char *name,json_object *p,json_object *response,char *m,size_t n){
 const char *target=text(p,"target_id");if(!target||!*target||strlen(target)>=TIMELINE_ID_CAPACITY)goto invalid;unsigned long long rev=SceneEditorDocumentRevision();
 if(!strcmp(name,"motion_plan.read")){const char *ks[]={"target_id"};if(!fields(p,ks,1))goto invalid;return output(response,"plan",json_object_get(MotionPlanFind(MotionPlansAuthor(document_authoring_root()),target)),m,n);}
 if(!strcmp(name,"motion_plan.restore")){const char *ks[]={"target_id","expected_revision"};if(!fields(p,ks,2))goto invalid;return SceneEditorMotionPlanRestore(target,rev,m,n);}
 if(!strcmp(name,"motion_plan.apply")||!strcmp(name,"motion_plan.preview")){const char *ks[]={"target_id","request","expected_revision"};MotionTimingScheduleRequest r;if(!fields(p,ks,3)||!request(get(p,"request"),&r))goto invalid;
  if(!strcmp(name,"motion_plan.apply"))return SceneEditorMotionPlanApply(target,&r,rev,m,n);
  MotionRouteSchedule s;if(!SceneEditorMotionPlanPreview(target,&r,&s,m,n))return false;json_object *out=json_object_new_object(),*rows=json_object_new_array();json_object_object_add(out,"end_time",json_object_new_double(s.timeline.end_time));json_object_object_add(out,"effective_max_speed",json_object_new_double(s.timeline.request.max_speed));json_object_object_add(out,"effective_acceleration",json_object_new_double(s.timeline.request.acceleration));json_object_object_add(out,"effective_braking",json_object_new_double(s.timeline.request.braking));json_object_object_add(out,"waypoints",rows);
  for(size_t i=0;i<r.count;++i){json_object *row=json_object_new_object();json_object_object_add(row,"distance_m",json_object_new_double(s.timeline.request.points[i].position));json_object_object_add(row,"arrival",json_object_new_double(s.timeline.arrivals[i]));json_object_object_add(row,"departure",json_object_new_double(s.timeline.departures[i]));json_object_array_add(rows,row);}return output(response,"preview",out,m,n);
 }
 MotionPathBinding b;if(!SceneEditorFollowerOrientationRead(target,&b))goto invalid;
 if(!strcmp(name,"follower_orientation.read")){const char *ks[]={"target_id"};if(!fields(p,ks,1))goto invalid;json_object *out=json_object_new_object();json_object_object_add(out,"start_up",vec(b.start_up));json_object_object_add(out,"start_roll_degrees",json_object_new_double(b.start_roll));json_object_object_add(out,"end_roll_degrees",json_object_new_double(b.end_roll));json_object_object_add(out,"end_roll_enabled",json_object_new_boolean(b.end_roll_enabled));
  if(!strncmp(target,"object/",7)){json_object_object_add(out,"follow_direction",json_object_new_boolean(b.follow_direction));json_object_object_add(out,"forward_axis",json_object_new_int(b.forward_axis));json_object_object_add(out,"rotation_offset_degrees",vec(b.rotation_offset));}
  if(!strcmp(target,"camera/main")){const char *modes[]={"authored","route","target_aim"};json_object_object_add(out,"camera_mode",json_object_new_string(modes[b.camera_orientation]));json_object_object_add(out,"use_focus_target",json_object_new_boolean(b.use_focus_target));}
  return output(response,"orientation",out,m,n);
 }
 if(strcmp(name,"follower_orientation.set"))goto invalid;const char *ks[]={"target_id","settings","expected_revision"};json_object *settings=get(p,"settings");if(!fields(p,ks,3)||!json_object_is_type(settings,json_type_object)||!json_object_object_length(settings))goto invalid;
 json_object_object_foreach(settings,key,v){
  if(!strcmp(key,"start_up")){if(!vector(v,b.start_up))goto invalid;}
  else if(!strcmp(key,"start_roll_degrees")){if(!number(settings,key,&b.start_roll))goto invalid;}
  else if(!strcmp(key,"end_roll_degrees")){if(!number(settings,key,&b.end_roll))goto invalid;}
  else if(!strcmp(key,"end_roll_enabled")||!strcmp(key,"follow_direction")||!strcmp(key,"use_focus_target")){if(!json_object_is_type(v,json_type_boolean))goto invalid;if(!strcmp(key,"end_roll_enabled"))b.end_roll_enabled=json_object_get_boolean(v);else if(!strcmp(key,"follow_direction")&&!strncmp(target,"object/",7))b.follow_direction=json_object_get_boolean(v);else if(!strcmp(key,"use_focus_target")&&!strcmp(target,"camera/main"))b.use_focus_target=json_object_get_boolean(v);else goto invalid;}
  else if(!strcmp(key,"forward_axis")&&!strncmp(target,"object/",7)){if(!json_object_is_type(v,json_type_int)||json_object_get_int64(v)<0||json_object_get_int64(v)>5)goto invalid;b.forward_axis=json_object_get_int(v);}
  else if(!strcmp(key,"rotation_offset_degrees")&&!strncmp(target,"object/",7)){if(!vector(v,b.rotation_offset))goto invalid;}
  else if(!strcmp(key,"camera_mode")&&!strcmp(target,"camera/main")){const char *mode=text(settings,key);if(!mode)goto invalid;if(!strcmp(mode,"authored"))b.camera_orientation=0;else if(!strcmp(mode,"route"))b.camera_orientation=1;else if(!strcmp(mode,"target_aim"))b.camera_orientation=2;else goto invalid;}
  else goto invalid;
 }return SceneEditorFollowerOrientationSet(target,&b,rev,m,n);
invalid:snprintf(m,n,"Invalid motion-plan/orientation arguments or unavailable follower. Attach first; progress is normalized, timing seconds are local, limits use world metres/seconds.");return false;
}
