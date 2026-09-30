#include "../src/editor/scene_editor_motion_feedback.h"
static void dm4_feedback(SceneEditor *editor) {
  (void)editor;static TimelineDocument doc;char text[256];
  assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK);
  size_t at=0;while(at<doc.track_count && strcmp(doc.tracks[at].property_id,MOTION_CAMERA_PROGRESS_PROPERTY))++at;
  assert(at<doc.track_count);TimelineTrack *t=&doc.tracks[at];
  t->key_count=0;
  assert(TimelineTrackAddKey(t,0,TimelineValueScalar(0),TIMELINE_INTERPOLATION_LINEAR)==TIMELINE_STATUS_OK);
  assert(TimelineTrackAddKey(t,60,TimelineValueScalar(.5),TIMELINE_INTERPOLATION_STEP)==TIMELINE_STATUS_OK);
  assert(TimelineTrackAddKey(t,119,TimelineValueScalar(1),TIMELINE_INTERPOLATION_LINEAR)==TIMELINE_STATUS_OK);
  TimelineVec3 pos;double length,parameter;
  assert(MotionPathsRuntimeTargetSample(t->target_id,.25,&pos,&length,&parameter));
  assert(SceneEditorMotionFeedback(&doc,at,(TimelineSample){30,0,1},text,sizeof(text)));
  double progress,speed;assert(sscanf(text,"Progress %lf | Speed %lf world/s",&progress,&speed)==2);
  double expected=length*.5/60*doc.rate.frames_per_second_numerator/doc.rate.frames_per_second_denominator;
  assert(fabs(speed-expected)<.001*fmax(1,expected) && fabs(progress-.25)<1e-9);
  doc.rate.frames_per_second_numerator*=2;
  assert(SceneEditorMotionFeedback(&doc,at,(TimelineSample){30,0,1},text,sizeof(text)));
  assert(sscanf(text,"Progress %lf | Speed %lf world/s",&progress,&speed)==2 && fabs(speed-2*expected)<.001*fmax(1,expected));
  assert(SceneEditorMotionFeedback(&doc,at,(TimelineSample){90,0,1},text,sizeof(text)) && strstr(text,"hold interval"));
  assert(SceneEditorMotionFeedback(&doc,at,(TimelineSample){119,0,1},text,sizeof(text)) && strstr(text,"jump: speed undefined"));
  assert(SceneEditorMotionFeedback(&doc,at,(TimelineSample){60,0,1},text,sizeof(text)) && strstr(text,"velocity discontinuity"));
  t->keys[0].value.as.scalar=.5;t->keys[1].value.as.scalar=0;
  assert(SceneEditorMotionFeedback(&doc,at,(TimelineSample){30,0,1},text,sizeof(text)));
  assert(sscanf(text,"Progress %lf | Speed %lf world/s",&progress,&speed)==2 && speed>0);
  t->keys[1].value.as.scalar=t->keys[0].value.as.scalar;
  assert(SceneEditorMotionFeedback(&doc,at,(TimelineSample){30,0,1},text,sizeof(text)) && strstr(text,"hold interval"));
  t->key_count=1;
  assert(SceneEditorMotionFeedback(&doc,at,(TimelineSample){0,0,1},text,sizeof(text)) && strstr(text,"hold interval"));
  /* Scalar XYZ values are authored distances; only route speed uses the
   * runtime's world-scaled arc length. Do not mislabel raw channel units. */
  TimelineTrack saved=*t;
  strcpy(t->property_id,"position_x");t->unit=TIMELINE_UNIT_WORLD_DISTANCE;
  assert(SceneEditorMotionFeedback(&doc,at,(TimelineSample){0,0,1},text,sizeof(text)) && strstr(text,"authored/s") && !strstr(text,"world"));
  *t=saved;
  choose_menu(editor,-1,SCENE_WORKSPACE_RENDER);assert(SceneEditorTimelineSelectTrack(at));
  SceneEditorRenderAuthoringSetTiming(true);SceneEditorSessionRuntimeRender(editor);capture(editor,"dm4_feedback.ppm");
  SDL_SetWindowSize(editor->window,1024,640);SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);capture(editor,"dm4_feedback_compact.ppm");
  fprintf(stderr,"D-M4 feedback PASS: route length/world scale, frame rate, reverse speed, hold, jump and velocity discontinuity\n");
}
