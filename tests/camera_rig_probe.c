#include "app/evaluated_scene_service.h"
#include "import/runtime_scene_bridge.h"
#include "import/runtime_camera_rig.h"
#include "config/config_manager.h"
#include "render/runtime_native_3d_render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv){
 if(argc<4)return 2;LoadAllSettings();RuntimeSceneBridgePreflight preflight;
 if(!runtime_scene_bridge_apply_file(argv[1],&preflight)){fprintf(stderr,"%s\n",preflight.diagnostics);return 3;}
 TimelineRate rate;TimelineRange range;if(RayEvaluatedSceneResolveTimelineClock(&rate,&range)!=TIMELINE_STATUS_OK)return 4;
 for(int i=3;i<argc;i++){
  TimelineSample t={range.start_frame+atoi(argv[i]),0,1};RayEvaluatedSceneServiceResult value,again,preview;
  bool active=!strcmp(argv[2],"active");
  if(!(active?RayEvaluatedSceneCaptureSample(t,&value):RayEvaluatedSceneCaptureViewSample(t,argv[2],&value)))return 5;
  if(!(active?RayEvaluatedSceneCaptureSample(t,&again):RayEvaluatedSceneCaptureViewSample(t,argv[2],&again)))return 6;
  if(memcmp(&value.snapshot.camera,&again.snapshot.camera,sizeof(RayEvaluatedCamera)))return 7;
  if(active&&(!RayEvaluatedSceneCaptureForElapsed((double)atoi(argv[i])*rate.frames_per_second_denominator/rate.frames_per_second_numerator,&preview)||memcmp(&value.snapshot.camera,&preview.snapshot.camera,sizeof(RayEvaluatedCamera))))return 8;
  RuntimeNative3DPreparedFrame prepared={0};if(!RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(&prepared,32,24,&value.snapshot,NULL)){fprintf(stderr,"%s\n",RuntimeNative3DPrepareFrameLastDiagnostics());return 9;}RuntimeNative3DPreparedFrame_Free(&prepared);
  RayEvaluatedSceneSnapshot forged=value.snapshot;forged.camera.position.x+=.01;
  if(RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(&prepared,32,24,&forged,NULL))return 10;
  forged=value.snapshot;forged.camera.motion_time.numerator++;
  if(RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(&prepared,32,24,&forged,NULL))return 11;
  const RayEvaluatedCamera *c=&value.snapshot.camera;
  printf("CAMERA {\"frame\":%d,\"id\":\"%s\",\"shot\":\"%s\",\"selected\":%s,\"time\":[%llu,%llu],\"position\":[%.17g,%.17g,%.17g],\"source_tick\":%llu,\"yaw\":%.17g,\"pitch\":%.17g,\"fov\":%.17g,\"route\":%s}\n",atoi(argv[i]),c->target_id,c->shot_id,c->selected?"true":"false",(unsigned long long)c->motion_time.numerator,(unsigned long long)c->motion_time.denominator,c->position.x,c->position.y,c->position.z,(unsigned long long)value.snapshot.sequence.source_tick,c->yaw_radians,c->pitch_radians,c->fov_y_degrees,c->uses_authored_path?"true":"false");
 }
 return 0;
}
