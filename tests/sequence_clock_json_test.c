#include "import/sequence_clock_json.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do {++checks;assert(x);}while(0)
static void reject(json_object *o) {
    SequenceClock out,before;memset(&out,0x5a,sizeof(out));before=out;
    CHECK(!SequenceClockFromJson(o,&out));CHECK(!memcmp(&out,&before,sizeof(out)));
}
int main(void) {
    SequenceClock c={.version=1,.rate={30,1},.range={5,30},.source={.id="fixture",.rate={240,1},.last_tick=240},.segment_count=1};
    memset(c.source.revision,'a',64);
    c.segments[0]=(SequenceSegment){.id="play",.frame_count=30,.kind=SEQUENCE_PLAY,.source_start={0,1},.speed={1,1}};
    json_object *o=SequenceClockToJson(&c);CHECK(o!=NULL);
    SequenceClock out;CHECK(SequenceClockFromJson(o,&out));json_object *roundtrip=SequenceClockToJson(&out);CHECK(roundtrip!=NULL);CHECK(!strcmp(json_object_to_json_string_ext(o,JSON_C_TO_STRING_PLAIN),json_object_to_json_string_ext(roundtrip,JSON_C_TO_STRING_PLAIN)));json_object_put(roundtrip);
    uint64_t fingerprint=SequenceClockFingerprint(&c);CHECK(fingerprint!=0);
    for(int i=0;i<30;++i) {
        SequenceEvaluation a,b;CHECK(SequenceClockEvaluate(&c,(TimelineSample){5+i,0,1},&a)==SEQUENCE_OK);
        CHECK(SequenceClockEvaluate(&out,(TimelineSample){5+i,0,1},&b)==SEQUENCE_OK);
        CHECK(SequenceClockEvaluationsEqual(&a,&b));
    }
    SequenceEvaluation same_a,same_b;
    CHECK(SequenceClockEvaluate(&c,(TimelineSample){5,0,1},&same_a)==SEQUENCE_OK);
    CHECK(SequenceClockEvaluate(&c,(TimelineSample){5,0,1000000},&same_b)==SEQUENCE_OK);
    CHECK(SequenceClockEvaluationsEqual(&same_a,&same_b));
    same_b.simulation_time.numerator++;CHECK(!SequenceClockEvaluationsEqual(&same_a,&same_b));
    json_object_put(o);reject(NULL);json_object *scalar=json_object_new_int(1);reject(scalar);json_object_put(scalar);
    const char *keys[]={"schema","version","rate","range","source","segments"};
    for(size_t i=0;i<6;++i) {
        o=SequenceClockToJson(&c);json_object_object_del(o,keys[i]);reject(o);json_object_put(o);
        o=SequenceClockToJson(&c);json_object_object_add(o,keys[i],NULL);reject(o);json_object_put(o);
    }
    o=SequenceClockToJson(&c);json_object_object_add(o,"unknown",json_object_new_int(1));reject(o);json_object_put(o);
    o=SequenceClockToJson(&c);json_object_object_add(o,"version",json_object_new_double(1));reject(o);json_object_put(o);
    o=SequenceClockToJson(&c);json_object *range=NULL;json_object_object_get_ex(o,"range",&range);
    json_object_object_add(range,"frame_count",json_object_new_uint64(UINT64_MAX));reject(o);json_object_put(o);
    o=SequenceClockToJson(&c);json_object_object_get_ex(o,"range",&range);
    json_object_object_add(range,"start_frame",json_object_new_uint64(UINT64_MAX));reject(o);json_object_put(o);
    o=SequenceClockToJson(&c);json_object *list=NULL,*speed=NULL;json_object_object_get_ex(o,"segments",&list);
    json_object_object_get_ex(json_object_array_get_idx(list,0),"speed",&speed);
    json_object_object_add(speed,"denominator",json_object_new_int(0));reject(o);json_object_put(o);
    c.segments[0].kind=SEQUENCE_HOLD;c.segments[0].speed=(SequenceTime){0,1};
    CHECK(SequenceClockFingerprint(&c)!=fingerprint);o=SequenceClockToJson(&c);CHECK(SequenceClockFromJson(o,&out));json_object_put(o);
    c.version=2;CHECK(!SequenceClockToJson(&c));
    printf("sequence codec: %u checks passed\n",checks);return 0;
}
