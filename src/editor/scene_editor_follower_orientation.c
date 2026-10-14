/* One native command for object, camera and light orientation settings.
 * Binding/source identities remain owned by path attachment commands. */
#include "editor/scene_editor_follower_orientation.h"
#include "editor/scene_editor_motion_paths.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_object_timeline.h"
#include "import/runtime_scene_bridge.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static bool match(const MotionPathBinding *b,const char *target){return target && (b->target_id[0]?!strcmp(b->target_id,target):!strncmp(target,"object/",7)&&!strcmp(b->object_id,target+7));}
bool SceneEditorFollowerOrientationRead(const char *target,MotionPathBinding *out){MotionPaths d;if(!out||!SceneEditorMotionPathsRead(&d))return false;for(size_t i=0;i<d.binding_count;++i)if(d.bindings[i].enabled&&match(&d.bindings[i],target)){*out=d.bindings[i];return true;}return false;}
static bool fail(char *m,size_t n,const char *why){if(m&&n)snprintf(m,n,"%s",why);return false;}
bool SceneEditorFollowerOrientationSet(const char *target,const MotionPathBinding *s,unsigned long long revision,char *m,size_t n){
 if(!s||!target||revision!=SceneEditorDocumentRevision())return fail(m,n,"Scene changed; reload follower orientation.");
 if(!SceneEditorObjectTimelineEditable(target,m,n))return false;
 MotionPaths d;if(!SceneEditorMotionPathsRead(&d))return fail(m,n,"Cannot read paths.");
 bool camera=!strcmp(target,"camera/main"),object=!strncmp(target,"object/",7);
 if(!isfinite(s->start_roll)||!isfinite(s->end_roll)||fabs(s->start_roll)>36000||fabs(s->end_roll)>36000||s->camera_orientation<0||s->camera_orientation>2||(!camera&&(s->camera_orientation||s->use_focus_target))||s->forward_axis<0||s->forward_axis>5)return fail(m,n,"Invalid follower orientation policy or roll.");
 for(int k=0;k<3;++k)if(!isfinite(s->start_up[k])||fabs(s->start_up[k])>1e12||!isfinite(s->rotation_offset[k])||fabs(s->rotation_offset[k])>36000)return fail(m,n,"Invalid up vector or rotation offset.");
 if(camera&&s->use_focus_target){RuntimeSceneBridge3DScaffoldState scaffold={0};runtime_scene_bridge_get_last_3d_scaffold_state(&scaffold);if(!scaffold.has_camera_focus_target)return fail(m,n,"Scene has no authored camera focus target.");}
 for(size_t i=0;i<d.binding_count;++i){MotionPathBinding *b=&d.bindings[i];if(!b->enabled||!match(b,target))continue;
  memcpy(b->start_up,s->start_up,sizeof(b->start_up));b->start_roll=s->start_roll;b->end_roll=s->end_roll;b->end_roll_enabled=s->end_roll_enabled;
  if(camera){b->camera_orientation=s->camera_orientation;b->use_focus_target=s->use_focus_target;}
  if(object){b->follow_direction=s->follow_direction;b->forward_axis=s->forward_axis;memcpy(b->rotation_offset,s->rotation_offset,sizeof(b->rotation_offset));}
  return SceneEditorMotionPathsSet(&d,revision,m,n);
 }return fail(m,n,"Attach this follower before editing orientation.");
}
