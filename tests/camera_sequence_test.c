#include "animation/camera_sequence.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int checks;
#define CHECK(x) do{checks++;assert(x);}while(0)
int main(void){
 CameraSequence s={.rate={24,1},.range={12,48},.camera_count=3,.shot_count=6};
 const char *ids[]={"camera/A","camera/B","camera/C"};
 for(int i=0;i<3;i++){snprintf(s.cameras[i].id,64,"%s",ids[i]);s.cameras[i].policy=(CameraClockPolicy)(i+1);s.cameras[i].speed=(SequenceTime){1,1};s.cameras[i].offset=(SequenceTime){0,1};s.cameras[i].duration_frames=48;}
 for(int i=0;i<6;i++){snprintf(s.shots[i].id,64,"shot%d",i);snprintf(s.shots[i].camera_id,64,"%s",ids[i%3]);s.shots[i].start_frame=i*8;s.shots[i].frame_count=8;s.shots[i].clip_start=(SequenceTime){i==5?1:0,1};}
 CHECK(CameraSequenceValidate(&s));
 for(int f=0;f<48;f++)for(int camera=0;camera<3;camera++){
  CameraSequenceSample v;CHECK(CameraSequenceEvaluate(&s,(TimelineSample){f+12,0,1},ids[camera],&v));
  unsigned expected=camera==0?(unsigned)f:camera==1?(f<8?0:f<16?f-8:f<32?8:f<40?f-24:16):(f<16?0:f<24?f-16:f<40?8:f-16);
  CHECK(v.motion_time.numerator*24==expected*v.motion_time.denominator);CHECK(v.selected==(f/8%3==camera));CHECK(v.shot_index==(unsigned)f/8);
  CameraSequenceSample selected;CHECK(CameraSequenceEvaluate(&s,(TimelineSample){f+12,0,1},NULL,&selected));CHECK(selected.camera_index==(unsigned)f/8%3);
 }
 CameraSequenceSample v={0},before=v;CHECK(!CameraSequenceEvaluate(&s,(TimelineSample){60,0,1},NULL,&v));CHECK(!memcmp(&v,&before,sizeof(v)));
 CHECK(!CameraSequenceEvaluate(&s,(TimelineSample){12,0,1},"camera/missing",&v));
 CHECK(CameraSequenceEvaluate(&s,(TimelineSample){20,1,2},"camera/B",&v));CHECK(v.motion_time.numerator==1&&v.motion_time.denominator==48);
 s.cameras[0].duration_frames=5;CHECK(CameraSequenceEvaluate(&s,(TimelineSample){59,0,1},"camera/A",&v));CHECK(v.held_at_end&&v.motion_time.numerator==1&&v.motion_time.denominator==6);
 CameraSequence bad=s;bad.shots[1].start_frame++;CHECK(!CameraSequenceValidate(&bad));bad=s;strcpy(bad.cameras[1].id,bad.cameras[0].id);CHECK(!CameraSequenceValidate(&bad));bad=s;bad.shots[0].clip_start.numerator=1;CHECK(!CameraSequenceValidate(&bad));bad=s;bad.cameras[0].speed.denominator=0;CHECK(!CameraSequenceValidate(&bad));bad=s;bad.shots[5].frame_count--;CHECK(!CameraSequenceValidate(&bad));
 s.range.start_frame=-20;CHECK(CameraSequenceEvaluate(&s,(TimelineSample){-12,0,1},NULL,&v));CHECK(v.motion_sample.absolute_frame==-20);
 printf("camera sequence: %d checks passed\n",checks);return 0;
}
