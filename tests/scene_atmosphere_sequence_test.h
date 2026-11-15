#include "app/scene_atmosphere_sequence.h"
#include "import/sequence_clock_json.h"
static void atmosphere_sequence_clock(void) {
 SceneAtmosphereAttachment v,read;SceneAtmosphereAttachmentDefaults(&v);v.present=true;v.sequence_source_clock=true;v.animated=true;v.visible=v.affects_lighting=false;v.density_scale=v.opacity=0;strcpy(v.source_kind,"scene_bundle");strcpy(v.source_path,"water/scene_bundle.json");
 SequenceClock *c=&v.sequence_clock;c->version=1;c->rate=(TimelineRate){24,1};c->range=(TimelineRange){0,48};strcpy(c->source.id,"water_rigid");memset(c->source.revision,'a',64);c->source.rate=(TimelineRate){240,1};c->source.last_tick=240;c->segment_count=3;
 c->segments[0]=(SequenceSegment){.start_frame=0,.frame_count=12,.kind=SEQUENCE_PLAY,.source_start={0,1},.speed={1,1}};strcpy(c->segments[0].id,"play");
 c->segments[1]=(SequenceSegment){.start_frame=12,.frame_count=24,.kind=SEQUENCE_HOLD,.source_start={1,2},.speed={0,1}};strcpy(c->segments[1].id,"hold");
 c->segments[2]=(SequenceSegment){.start_frame=36,.frame_count=12,.kind=SEQUENCE_PLAY,.source_start={1,2},.speed={1,1}};strcpy(c->segments[2].id,"resume");
 char d[256];assert(SceneAtmosphereAttachmentValidate(&v,d,sizeof(d)));
 for(int frame=0;frame<48;frame++) {
  TimelineEvaluationContext p,m;SequenceEvaluation e;int64_t water;
  assert(TimelineEvaluationContextBuild(c->rate,c->range,(TimelineSample){frame,0,1},&p)==TIMELINE_STATUS_OK);
  assert(SceneAtmosphereSequenceContext(&v,&p,&m,d,sizeof(d)));
  assert(SceneAtmosphereAttachmentRequestedFrame(&v,&p,&water,d,sizeof(d)));
  int expected=frame<12?frame:frame<36?12:frame-24;assert(water==expected);assert(m.sample.absolute_frame==expected*10);
  assert(SequenceClockEvaluate(c,p.sample,&e)==SEQUENCE_OK);assert(SceneAtmosphereSequenceMatches(&v,&e));e.source_tick++;assert(!SceneAtmosphereSequenceMatches(&v,&e));
  p.rate.frames_per_second_numerator=25;TimelineEvaluationContext before=m;
  assert(!SceneAtmosphereSequenceContext(&v,&p,&m,d,sizeof(d)));assert(!memcmp(&m,&before,sizeof(m)));
 }
 json_object *a=json_object_new_object();json_object_object_add(a,"sequence",SequenceClockToJson(c));json_object_object_add(a,"atmosphere_attachment",SceneAtmosphereAttachmentToJson(&v));assert(SceneAtmosphereAttachmentParse(a,&read,d,sizeof(d))&&read.sequence_source_clock);
 json_object *o=NULL,*p=NULL;json_object_object_get_ex(a,"atmosphere_attachment",&o);json_object_object_get_ex(o,"playback",&p);SceneAtmosphereAttachment before=read;
 json_object_object_add(p,"clock",NULL);assert(!SceneAtmosphereAttachmentParse(a,&read,d,sizeof(d)));assert(!memcmp(&read,&before,sizeof(read)));
 json_object_object_add(p,"clock",json_object_new_string("wall_time"));assert(!SceneAtmosphereAttachmentParse(a,&read,d,sizeof(d)));
 json_object_object_add(p,"clock",json_object_new_string("sequence_source"));json_object_object_del(a,"sequence");assert(!SceneAtmosphereAttachmentParse(a,&read,d,sizeof(d)));
 v.visible=true;assert(!SceneAtmosphereAttachmentValidate(&v,d,sizeof(d)));v.visible=false;v.density_scale=1;assert(!SceneAtmosphereAttachmentValidate(&v,d,sizeof(d)));v.density_scale=0;strcpy(v.source_kind,"manifest");assert(!SceneAtmosphereAttachmentValidate(&v,d,sizeof(d)));
 json_object_put(a);puts("PASS water sequence clock: exact Hold/Resume, legacy default, codec and atomic refusals");
}
