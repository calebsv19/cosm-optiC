#include "animation/sequence_clock.h"
#include "../src/animation/sequence_time_internal.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);assert(x);} }while(0)
static SequenceClock fixture(void) {
    SequenceClock c={0};
    c.version=1;c.rate=(TimelineRate){30,1};c.range=(TimelineRange){100,300};
    strcpy(c.source.id,"fixture");memset(c.source.revision,'a',64);
    c.source.rate=(TimelineRate){240,1};c.source.last_tick=1680;
    c.segment_count=3;
    c.segments[0]=(SequenceSegment){.id="play",.frame_count=120,.kind=SEQUENCE_PLAY,
        .source_start={0,1},.speed={1,1}};
    c.segments[1]=(SequenceSegment){.id="inspect",.start_frame=120,.frame_count=90,
        .kind=SEQUENCE_HOLD,.source_start={4,1},.speed={0,1}};
    c.segments[2]=(SequenceSegment){.id="resume",.start_frame=210,.frame_count=90,
        .kind=SEQUENCE_PLAY,.source_start={4,1},.speed={1,1}};
    return c;
}
static void time_is(SequenceTime value,uint64_t n,uint64_t d) {
    CHECK(value.numerator==n && value.denominator==d);
}
static void reject(SequenceClock c, SequenceStatus expected) {
    SequenceEvaluation out,before;memset(&out,0x5a,sizeof(out));before=out;
    CHECK(SequenceClockValidate(&c)==expected);
    CHECK(SequenceClockEvaluate(&c,(TimelineSample){100,0,1},&out)==expected);
    CHECK(!memcmp(&out,&before,sizeof(out)));
}
static void sample_reject(const SequenceClock *c, TimelineSample sample, SequenceStatus expected) {
    SequenceEvaluation out,before;memset(&out,0x5a,sizeof(out));before=out;
    CHECK(SequenceClockEvaluate(c,sample,&out)==expected);
    CHECK(!memcmp(&out,&before,sizeof(out)));
}
int main(void) {
    SequenceClock c=fixture(),before=c;SequenceEvaluation values[300],out,other;
    CHECK(SequenceClockValidate(&c)==SEQUENCE_OK);
    for(int i=0;i<300;++i) {
        CHECK(SequenceClockEvaluate(&c,(TimelineSample){100+i,0,1},&values[i])==SEQUENCE_OK);
        uint64_t tick=i<120?(uint64_t)i*8:i<210?960:960+(uint64_t)(i-210)*8;
        CHECK(values[i].source_tick==tick);
        CHECK(values[i].segment_index==(size_t)(i<120?0:i<210?1:2));
        TimelineEvaluationContext legacy;
        CHECK(TimelineEvaluationContextBuild(c.rate,c.range,(TimelineSample){100+i,0,1},&legacy)==TIMELINE_STATUS_OK);
        CHECK(!memcmp(&legacy,&values[i].presentation_context,sizeof(legacy)));
    }
    SequenceTime duration;
    CHECK(SequenceClockDuration(&c,&duration)==SEQUENCE_OK);time_is(duration,10,1);
    CHECK(!memcmp(&c,&before,sizeof(c)));
    time_is(values[120].presentation_time,4,1);time_is(values[120].segment_time,0,1);
    time_is(values[209].simulation_time,4,1);time_is(values[210].presentation_time,7,1);
    time_is(values[210].segment_time,0,1);time_is(values[299].simulation_time,209,30);
    for(int j=0;j<300;++j) {
        int i=(j*137)%300;
        CHECK(SequenceClockEvaluate(&c,(TimelineSample){100+i,0,1},&out)==SEQUENCE_OK);
        CHECK(!memcmp(&out,&values[i],sizeof(out)));
        CHECK(SequenceClockEvaluateChunk(&c,200,(uint64_t)(i>=100?i-100:0),0,1,&other)==SEQUENCE_OK);
        if(i>=100)CHECK(!memcmp(&out,&other,sizeof(out)));
    }
    CHECK(SequenceClockEvaluate(&c,(TimelineSample){220,1,2},&out)==SEQUENCE_OK);
    time_is(out.simulation_time,4,1);time_is(out.segment_time,1,60);
    CHECK(SequenceClockEvaluate(&c,(TimelineSample){100,1,2},&out)==SEQUENCE_OK);
    CHECK(out.source_tick==4);time_is(out.presentation_time,1,60);
    sample_reject(&c,(TimelineSample){100,1,3},SEQUENCE_UNSUPPORTED_SOURCE_SAMPLE);
    sample_reject(&c,(TimelineSample){99,0,1},SEQUENCE_SAMPLE_OUT_OF_RANGE);
    sample_reject(&c,(TimelineSample){400,0,1},SEQUENCE_SAMPLE_OUT_OF_RANGE);
    sample_reject(&c,(TimelineSample){100,1,1},SEQUENCE_SAMPLE_OUT_OF_RANGE);
    sample_reject(&c,(TimelineSample){100,0,0},SEQUENCE_SAMPLE_OUT_OF_RANGE);
    CHECK(SequenceClockEvaluate(&c,(TimelineSample){399,1,2},&out)==SEQUENCE_OK);
    CHECK(out.source_tick==1676);
    SequenceTime t={99,1},saved=t;
    CHECK(SequenceClockMotionTime(&values[130],SEQUENCE_MOTION_PRESENTATION,&t)==SEQUENCE_OK);time_is(t,13,3);
    CHECK(SequenceClockMotionTime(&values[130],SEQUENCE_MOTION_SIMULATION,&t)==SEQUENCE_OK);time_is(t,4,1);
    CHECK(SequenceClockMotionTime(&values[130],SEQUENCE_MOTION_SEGMENT_LOCAL,&t)==SEQUENCE_OK);time_is(t,1,3);
    t=saved;CHECK(SequenceClockMotionTime(&values[130],(SequenceMotionClock)99,&t)==SEQUENCE_INVALID_ARGUMENT);CHECK(!memcmp(&t,&saved,sizeof(t)));
    c.range.start_frame=-100;
    CHECK(SequenceClockEvaluate(&c,(TimelineSample){20,0,1},&out)==SEQUENCE_OK);time_is(out.presentation_time,4,1);
    c=fixture();c.version=2;reject(c,SEQUENCE_INVALID_VERSION);
    c=fixture();c.rate.frames_per_second_numerator=0;reject(c,SEQUENCE_INVALID_RANGE);
    c=fixture();c.range.frame_count=0;reject(c,SEQUENCE_INVALID_RANGE);
    c=fixture();c.range.start_frame=INT64_MAX;reject(c,SEQUENCE_INVALID_RANGE);
    c=fixture();c.source.first_tick=1681;reject(c,SEQUENCE_INVALID_RANGE);
    c=fixture();c.source.rate.frames_per_second_denominator=0;reject(c,SEQUENCE_INVALID_RANGE);
    c=fixture();c.source.last_tick=UINT64_MAX;reject(c,SEQUENCE_INVALID_RANGE);
    c=fixture();memset(c.source.id,'a',sizeof(c.source.id));reject(c,SEQUENCE_INVALID_IDENTITY);
    c=fixture();c.source.revision[1]='z';reject(c,SEQUENCE_INVALID_IDENTITY);
    c=fixture();c.source.revision[63]=0;reject(c,SEQUENCE_INVALID_IDENTITY);
    c=fixture();c.segment_count=65;reject(c,SEQUENCE_INVALID_SEGMENTS);
    c=fixture();c.segment_count=0;reject(c,SEQUENCE_INVALID_SEGMENTS);
    c=fixture();strcpy(c.segments[2].id,"play");reject(c,SEQUENCE_INVALID_IDENTITY);
    c=fixture();c.segments[1].start_frame++;reject(c,SEQUENCE_INVALID_SEGMENTS);
    c=fixture();c.segments[1].start_frame--;reject(c,SEQUENCE_INVALID_SEGMENTS);
    c=fixture();c.segments[2].frame_count--;reject(c,SEQUENCE_INVALID_SEGMENTS);
    c=fixture();c.segments[0].frame_count=0;reject(c,SEQUENCE_INVALID_SEGMENTS);
    c=fixture();c.segments[0].frame_count=UINT64_MAX;reject(c,SEQUENCE_INVALID_SEGMENTS);
    c=fixture();c.segments[0].kind=(SequenceSegmentKind)0;reject(c,SEQUENCE_INVALID_SEGMENTS);
    c=fixture();c.segments[0].speed.numerator=0;reject(c,SEQUENCE_INVALID_SEGMENTS);
    c=fixture();c.segments[1].speed.denominator=2;reject(c,SEQUENCE_INVALID_SEGMENTS);
    c=fixture();c.segments[1].source_start.numerator=5;reject(c,SEQUENCE_DISCONTINUOUS_SOURCE);
    c=fixture();c.source.last_tick=1679;reject(c,SEQUENCE_SOURCE_OUT_OF_RANGE);
    c=fixture();c.source.first_tick=1;reject(c,SEQUENCE_SOURCE_OUT_OF_RANGE);
    c=fixture();c.segments[0].speed=(SequenceTime){1,3};reject(c,SEQUENCE_UNSUPPORTED_SOURCE_SAMPLE);
    c=fixture();c.segments[0].source_start=(SequenceTime){1,241};reject(c,SEQUENCE_UNSUPPORTED_SOURCE_SAMPLE);
    c=fixture();c.segments[0].speed.denominator=0;reject(c,SEQUENCE_INVALID_TIME);
    c=fixture();c.segments[0].speed=(SequenceTime){INT64_MAX,1};reject(c,SEQUENCE_NUMERIC_RANGE);
    c=fixture();c.segments[0].source_start=(SequenceTime){UINT64_MAX,1};reject(c,SEQUENCE_NUMERIC_RANGE);
    /* Fractional rates, reduction and a nonzero origin need no float tolerance. */
    c=fixture();c.segment_count=1;c.range.frame_count=30;
    c.rate=(TimelineRate){30000,1001};c.source.rate=c.rate;c.source.last_tick=30;
    c.segments[0].frame_count=30;
    CHECK(SequenceClockEvaluate(&c,(TimelineSample){129,0,1},&out)==SEQUENCE_OK);
    time_is(out.presentation_time,29029,30000);CHECK(out.source_tick==29);
    c=fixture();c.segment_count=1;c.range.frame_count=64;c.segment_count=64;
    for(size_t i=0;i<64;++i) {
        c.segments[i]=(SequenceSegment){.start_frame=i,.frame_count=1,.kind=SEQUENCE_HOLD,.source_start={0,1},.speed={0,1}};
        snprintf(c.segments[i].id,sizeof(c.segments[i].id),"hold-%zu",i);
    }
    CHECK(SequenceClockValidate(&c)==SEQUENCE_OK);
    CHECK(SequenceClockEvaluate(&c,(TimelineSample){163,0,1},&out)==SEQUENCE_OK);CHECK(out.segment_index==63);
    /* Noncanonical equivalent ratios normalize without changing source meaning. */
    c=fixture();c.segments[0].speed=(SequenceTime){2,2};
    c.segments[1].source_start=(SequenceTime){8,2};
    CHECK(SequenceClockEvaluate(&c,(TimelineSample){220,0,1},&out)==SEQUENCE_OK);
    time_is(out.simulation_time,4,1);
    c=fixture();c.source.first_tick=480;c.source.last_tick+=480;
    for(size_t i=0;i<c.segment_count;++i)c.segments[i].source_start.numerator+=2;
    CHECK(SequenceClockEvaluate(&c,(TimelineSample){100,0,1},&out)==SEQUENCE_OK);
    CHECK(out.source_tick==480);
    c=fixture();c.range.start_frame=INT64_MIN;
    CHECK(SequenceClockEvaluate(&c,(TimelineSample){INT64_MIN+120,0,1},&out)==SEQUENCE_OK);
    time_is(out.presentation_time,4,1);
    /* A singleton source envelope can hold indefinitely without source travel. */
    c=fixture();c.segment_count=1;c.range.frame_count=300;
    c.source.first_tick=c.source.last_tick=960;
    c.segments[0]=(SequenceSegment){.id="held",.frame_count=300,.kind=SEQUENCE_HOLD,
        .source_start={4,1},.speed={0,1}};
    CHECK(SequenceClockEvaluate(&c,(TimelineSample){399,1,7},&out)==SEQUENCE_OK);
    CHECK(out.source_tick==960);
    CHECK(SequenceClockDuration(&c,&duration)==SEQUENCE_OK);time_is(duration,10,1);
    c.version=99;duration=saved;
    CHECK(SequenceClockDuration(&c,&duration)==SEQUENCE_INVALID_VERSION);
    CHECK(!memcmp(&duration,&saved,sizeof(duration)));
    /* Arithmetic boundary tests include cancellation and no overflowing comparison. */
    SequenceTime a={INT64_MAX,INT64_MAX-1},b={INT64_MAX-1,INT64_MAX};
    CHECK(SequenceTimeCompare(a,b)>0);CHECK(SequenceTimeCompare(b,a)<0);
    CHECK(SequenceTimeMultiply(a,b,&t)==SEQUENCE_OK);time_is(t,1,1);
    CHECK(SequenceTimeCompare((SequenceTime){0,1},(SequenceTime){0,1})==0);
    t=saved;CHECK(SequenceTimeAdd((SequenceTime){INT64_MAX,1},(SequenceTime){1,1},&t)==SEQUENCE_NUMERIC_RANGE);CHECK(!memcmp(&t,&saved,sizeof(t)));
    t=saved;CHECK(SequenceTimeSubtract((SequenceTime){0,1},(SequenceTime){1,1},&t)==SEQUENCE_INVALID_TIME);CHECK(!memcmp(&t,&saved,sizeof(t)));
    CHECK(SequenceClockValidate(NULL)==SEQUENCE_INVALID_ARGUMENT);
    CHECK(SequenceClockEvaluate(NULL,(TimelineSample){0,0,1},&out)==SEQUENCE_INVALID_ARGUMENT);
    CHECK(SequenceClockEvaluate(&c,(TimelineSample){100,0,1},NULL)==SEQUENCE_INVALID_ARGUMENT);
    c=fixture();out=values[0];other=out;
    CHECK(SequenceClockEvaluateChunk(&c,INT64_MAX,1,0,1,&out)==SEQUENCE_SAMPLE_OUT_OF_RANGE);CHECK(!memcmp(&out,&other,sizeof(out)));
    printf("sequence clock: %u checks passed\n",checks);
    return 0;
}
