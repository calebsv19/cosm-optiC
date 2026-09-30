/* Explicit, atomic legacy conversion. Source channels and carriers stay intact
 * for detach/orientation; only new path/binding/timing records are committed. */
#include "editor/scene_editor_motion_paths.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_document_timeline.h"
#include "editor/scene_editor_timeline.h"
#include "scene_editor_motion_paths_internal.h"
#include "config/config_manager.h"
#include "import/runtime_scene_light_timeline_io.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ARC_STEPS 256
#define ARC_CAP ((MOTION_POINT_CAPACITY-1)*ARC_STEPS+1)
#define CONVERSION_MAX_FRAMES 4096
#define PROBES_PER_FRAME 8

typedef struct ConversionProbe { TimelineSample sample; double progress, position[3]; } ConversionProbe;
static bool refuse(char *m, size_t n, const char *why) {
  if (m && n) snprintf(m, n, "Conversion refused: %s", why);
  return false;
}
static double distance3(const double a[3], const double b[3]) {
  double sum=0; for(int k=0;k<3;++k) sum+=(a[k]-b[k])*(a[k]-b[k]); return sqrt(sum);
}
static double parameter_progress(const double *arc, size_t end, double parameter) {
  double f=fmax(0,fmin((double)end,parameter*ARC_STEPS)); size_t lo=(size_t)f;
  return (arc[lo]+(lo<end?(f-lo)*(arc[lo+1]-arc[lo]):0))/arc[end];
}
static void sample_route(const MotionPath *path, const double *arc, size_t end,
    double progress, double out[3]) {
  size_t lo=0,hi=end; double d=fmax(0,fmin(1,progress))*arc[end];
  while(hi-lo>1) {size_t mid=(lo+hi)/2;if(arc[mid]<d)lo=mid;else hi=mid;}
  double span=arc[hi]-arc[lo];
  MotionPathPointAt(path,(span>1e-14?lo+(d-arc[lo])/span:0)/ARC_STEPS,out);
}
static TimelineInterpolation interval_mode(const TimelineTrack *source,int64_t frame) {
  size_t i=0; while(i+1<source->key_count && source->keys[i+1].frame<=frame)++i;
  return source->keys[i].interpolation_to_next==TIMELINE_INTERPOLATION_STEP?
      TIMELINE_INTERPOLATION_STEP:TIMELINE_INTERPOLATION_LINEAR;
}
static bool add_key(TimelineTrack *track,const TimelineTrack *source,
    const ConversionProbe *probes,int64_t start,int64_t frame) {
  for(size_t i=0;i<track->key_count;++i)if(track->keys[i].frame==frame)return true;
  TimelineKeyframe key={.frame=frame,.value=TimelineValueScalar(probes[(frame-start)*PROBES_PER_FRAME].progress),
      .interpolation_to_next=interval_mode(source,frame)};
  return TimelineTrackInsertKey(track,key,&(size_t){0})==TIMELINE_STATUS_OK;
}
bool SceneEditorMotionPathConvertLegacy(bool camera,unsigned long long revision,
    char *message,size_t size) {
  if(revision!=SceneEditorDocumentRevision() || animSettings.spaceMode!=SPACE_MODE_3D)
    return refuse(message,size,"requires an unchanged 3D scene.");
  static TimelineDocument doc;
  static RuntimeSceneLightTimelineDocument light;
  MotionPaths paths;
  if(!SceneEditorMotionPathsRead(&paths) || SceneEditorDocumentGetTimeline(&doc)!=TIMELINE_STATUS_OK)
    return refuse(message,size,"activate the timeline first.");
  const Path *legacy=&sceneSettings.cameraPath;
  const CameraPath3D *depth=&sceneSettings.cameraPath3D;
  const char *target="camera/main";
  if(!camera) {
    if(!RuntimeSceneLightTimelineGetLast(&light) || light.progress_track_index>=light.timeline.track_count)
      return refuse(message,size,"no retained animated light.");
    legacy=&light.spatial_path;depth=&light.spatial_path_3d;
    target=light.timeline.tracks[light.progress_track_index].target_id;
  }
  if(legacy->numPoints<2 || legacy->numPoints>MOTION_POINT_CAPACITY)
    return refuse(message,size,"legacy route must have 2-32 points.");
  if(!doc.range.frame_count || doc.range.frame_count>CONVERSION_MAX_FRAMES)
    return refuse(message,size,"conversion audit supports at most 4096 frames.");
  size_t binding=paths.binding_count,prior=SIZE_MAX,route=SIZE_MAX;
  for(size_t i=0;i<paths.binding_count;++i)if(!strcmp(paths.bindings[i].target_id,target))binding=i;
  if(binding<paths.binding_count && paths.bindings[binding].enabled)
    return refuse(message,size,"detach the current route before converting.");
  if(paths.count==MOTION_PATH_CAPACITY || (binding==paths.binding_count && binding==MOTION_BINDING_CAPACITY))
    return refuse(message,size,"path or binding capacity reached.");
  const char *property=camera?MOTION_CAMERA_PROGRESS_PROPERTY:MOTION_LIGHT_PROGRESS_PROPERTY;
  for(size_t i=0;i<doc.track_count;++i) {
    TimelineTrack *t=&doc.tracks[i]; if(strcmp(t->target_id,target))continue;
    if(!strcmp(t->property_id,property))route=i;
    if(t->enabled && !strcmp(t->property_id,camera?"camera/path_progress":"light/path_progress"))prior=i;
  }
  if(prior==SIZE_MAX)return refuse(message,size,"requires an active legacy path-progress source.");
  if(route<doc.track_count)return refuse(message,size,"existing route timing would be overwritten; retain it or use attachment.");
  MotionPath path={.count=(size_t)legacy->numPoints};
  unsigned serial=1;bool used;
  do {snprintf(path.id,sizeof(path.id),"path-%u",serial++);used=false;
    for(size_t i=0;i<paths.count;++i)if(!strcmp(path.id,paths.paths[i].id))used=true;
  }while(used);
  snprintf(path.name,sizeof(path.name),"Converted %s route",camera?"camera":"light");
  double scale=SceneEditorDocumentWorldScale();
  for(size_t i=0;i<path.count;++i) {
    MotionPathPoint *p=&path.points[i];snprintf(p->id,sizeof(p->id),"point-%zu",i+1);
    p->position[0]=legacy->points[i].x/scale;p->position[1]=legacy->points[i].y/scale;p->position[2]=depth->point_z[i]/scale;
  }
  for(size_t i=0;i+1<path.count;++i) {
    double out[3]={legacy->handles[i][0].vx/scale,legacy->handles[i][0].vy/scale,depth->handles_vz[i][0]/scale};
    double in[3]={legacy->handles[i][1].vx/scale,legacy->handles[i][1].vy/scale,depth->handles_vz[i][1]/scale};
    for(int k=0;k<3;++k) {
      path.points[i].outgoing[k]=legacy->mode==BEZIER_CUBIC?out[k]:2*out[k]/3;
      path.points[i+1].incoming[k]=legacy->mode==BEZIER_CUBIC?in[k]:2*(path.points[i].position[k]+out[k]-path.points[i+1].position[k])/3;
    }
  }
  double arc[ARC_CAP]={0},previous[3];size_t end=(path.count-1)*ARC_STEPS;
  MotionPathPointAt(&path,0,previous);
  for(size_t i=1;i<=end;++i) {double p[3];MotionPathPointAt(&path,(double)i/ARC_STEPS,p);arc[i]=arc[i-1]+distance3(p,previous);memcpy(previous,p,sizeof(p));}
  if(!isfinite(arc[end]) || arc[end]<1e-12)return refuse(message,size,"zero-length route.");
  size_t count=(doc.range.frame_count-1)*PROBES_PER_FRAME+1;
  ConversionProbe *probes=calloc(count,sizeof(*probes));
  if(!probes)return refuse(message,size,"allocation failed.");
  bool ok=true;
  for(size_t i=0;i<count && ok;++i) {
    ConversionProbe *p=&probes[i];TimelineEvaluationContext context;TimelineEvaluationResult value;
    p->sample=(TimelineSample){doc.range.start_frame+(int64_t)(i/PROBES_PER_FRAME),(uint32_t)(i%PROBES_PER_FRAME),PROBES_PER_FRAME};
    ok=TimelineEvaluationContextBuild(doc.rate,doc.range,p->sample,&context)==TIMELINE_STATUS_OK &&
        TimelineTrackEvaluate(&doc.tracks[prior],&context,&value)==TIMELINE_STATUS_OK;
    if(!ok)break;
    double parameter;
    if(camera) {
      parameter=PathResolveNormalizedGlobalT(legacy,value.value.as.scalar);
      Point xy=GetPositionAlongPath((Path*)legacy,parameter);
      p->position[0]=xy.x/scale;p->position[1]=xy.y/scale;p->position[2]=CameraPath3D_GetPositionZ(legacy,depth,parameter)/scale;
    } else {
      TimelineLightMotionSample sample;
      ok=TimelineLightMotionEvaluateResult(&doc.tracks[prior],&value,legacy,depth,&context,&sample)==TIMELINE_STATUS_OK;
      if(!ok)break;
      parameter=sample.global_path_t;
      p->position[0]=sample.position.x/scale;p->position[1]=sample.position.y/scale;p->position[2]=sample.position.z/scale;
    }
    p->progress=parameter_progress(arc,end,parameter*(path.count-1));
  }
  TimelineTrack converted;
  char track_id[TIMELINE_ID_CAPACITY];serial=1;
  do {snprintf(track_id,sizeof(track_id),"converted-%s-%u",camera?"camera":"light",serial++);used=false;
    for(size_t i=0;i<doc.track_count;++i)if(!strcmp(track_id,doc.tracks[i].track_id))used=true;
  }while(used);
  TimelineTrackInit(&converted,track_id,target,property,TIMELINE_VALUE_SCALAR);
  TimelineTrackSetUnit(&converted,TIMELINE_UNIT_UNITLESS);
  int64_t start=doc.range.start_frame,last=start+(int64_t)doc.range.frame_count-1;
  if(ok)ok=add_key(&converted,&doc.tracks[prior],probes,start,start) && add_key(&converted,&doc.tracks[prior],probes,start,last);
  for(size_t i=0;ok && i<doc.tracks[prior].key_count;++i) {
    int64_t frame=doc.tracks[prior].keys[i].frame;
    if(frame>=start && frame<=last)ok=add_key(&converted,&doc.tracks[prior],probes,start,frame);
  }
  /* Measured positional bound at every integer and eighth-frame, including
   * source key boundaries. No claim of a continuous analytic error bound. */
  double tolerance=1e-4*fmax(1,arc[end]),worst=0;
  for(size_t iteration=0;ok && iteration<TIMELINE_TRACK_KEY_CAPACITY;++iteration) {
    worst=0;size_t worst_i=0;
    for(size_t i=0;i<count;++i) {
      TimelineEvaluationContext context;TimelineEvaluationResult value;double p[3];
      ok=TimelineEvaluationContextBuild(doc.rate,doc.range,probes[i].sample,&context)==TIMELINE_STATUS_OK && TimelineTrackEvaluate(&converted,&context,&value)==TIMELINE_STATUS_OK;
      if(!ok)break;
      sample_route(&path,arc,end,value.value.as.scalar,p);double error=distance3(p,probes[i].position);
      if(error>worst){worst=error;worst_i=i;}
    }
    if(!ok || worst<=tolerance)break;
    int64_t frame=probes[worst_i].sample.absolute_frame;size_t before=converted.key_count;
    ok=add_key(&converted,&doc.tracks[prior],probes,start,frame);
    if(ok && frame<last)ok=add_key(&converted,&doc.tracks[prior],probes,start,frame+1);
    if(before==converted.key_count)ok=false;
  }
  free(probes);
  if(!ok || worst>tolerance)return refuse(message,size,"timing tolerance exceeds 128-key/integer-frame capacity; original retained.");
  MotionPathBinding b={.enabled=true,.restore_known=true,.restore_count=1};
  snprintf(b.target_id,sizeof(b.target_id),"%s",target);snprintf(b.path_id,sizeof(b.path_id),"%s",path.id);
  snprintf(b.restore_xyz_tracks[0],TIMELINE_ID_CAPACITY,"%s",doc.tracks[prior].track_id);
  doc.tracks[prior].enabled=false;
  if(TimelineDocumentAddTrack(&doc,&converted)!=TIMELINE_STATUS_OK)return refuse(message,size,"timeline capacity reached.");
  paths.paths[paths.count++]=path;paths.bindings[binding]=b;if(binding==paths.binding_count)++paths.binding_count;
  if(!SceneEditorMotionPathsCommit(&paths,&doc,revision,message,size))return false;
  SceneEditorTimelinePause();SceneEditorTimelineSelectTrack(doc.track_count-1);
  SceneEditorMotionPathPanelSelect(true);
  if(message && size)snprintf(message,size,"Converted: max error %.3g <= %.3g scene units; %zu keys.",worst,tolerance,converted.key_count);
  return true;
}
