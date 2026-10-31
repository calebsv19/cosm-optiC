#include "sequence_time_internal.h"
#include <limits.h>
#include <string.h>

#define TRY(call) do { SequenceStatus s_=(call); if(s_!=SEQUENCE_OK)return s_; } while(0)

const char *SequenceStatusLabel(SequenceStatus status) {
    switch (status) {
    case SEQUENCE_OK: return "ok";
    case SEQUENCE_INVALID_ARGUMENT: return "invalid_argument";
    case SEQUENCE_INVALID_VERSION: return "invalid_version";
    case SEQUENCE_INVALID_TIME: return "invalid_time";
    case SEQUENCE_NUMERIC_RANGE: return "numeric_range";
    case SEQUENCE_INVALID_RANGE: return "invalid_range";
    case SEQUENCE_INVALID_IDENTITY: return "invalid_identity";
    case SEQUENCE_INVALID_SEGMENTS: return "invalid_segments";
    case SEQUENCE_DISCONTINUOUS_SOURCE: return "discontinuous_source";
    case SEQUENCE_SOURCE_OUT_OF_RANGE: return "source_out_of_range";
    case SEQUENCE_UNSUPPORTED_SOURCE_SAMPLE: return "unsupported_source_sample";
    case SEQUENCE_SAMPLE_OUT_OF_RANGE: return "sample_out_of_range";
    default: return "unknown";
    }
}
static bool identifier(const char *s, size_t capacity, bool revision) {
    for (size_t i=0;i<capacity;++i) {
        unsigned char c=(unsigned char)s[i];
        if (!c) return revision ? i==64 : i>0;
        bool hex=(c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');
        if (revision ? !hex : !(hex||(c>='g'&&c<='z')||
            (c>='G'&&c<='Z')||c=='_'||c=='-')) return false;
    }
    return false;
}
static SequenceTime seconds_per_frame(TimelineRate rate) {
    return (SequenceTime){rate.frames_per_second_denominator,
                          rate.frames_per_second_numerator};
}
static SequenceStatus tick_at(const SequenceClock *clock, SequenceTime seconds,
                              SequenceTime *out) {
    SequenceTime rate={clock->source.rate.frames_per_second_numerator,
                       clock->source.rate.frames_per_second_denominator};
    return SequenceTimeMultiply(seconds,rate,out);
}
static SequenceStatus in_envelope(const SequenceClock *clock, SequenceTime tick) {
    SequenceTime lo={clock->source.first_tick,1}, hi={clock->source.last_tick,1};
    return SequenceTimeCompare(tick,lo)<0 || SequenceTimeCompare(tick,hi)>0
        ? SEQUENCE_SOURCE_OUT_OF_RANGE : SEQUENCE_OK;
}
static SequenceStatus source_at(const SequenceClock *clock,
    const SequenceSegment *segment, SequenceTime local_frames, SequenceTime *out) {
    SequenceTime elapsed, travel;
    TRY(SequenceTimeMultiply(local_frames,seconds_per_frame(clock->rate),&elapsed));
    TRY(SequenceTimeMultiply(elapsed,segment->speed,&travel));
    return SequenceTimeAdd(segment->source_start,travel,out);
}
SequenceStatus SequenceClockValidate(const SequenceClock *clock) {
    if (!clock) return SEQUENCE_INVALID_ARGUMENT;
    if (clock->version!=SEQUENCE_CLOCK_VERSION) return SEQUENCE_INVALID_VERSION;
    if (!TimelineRateIsValid(clock->rate) || !TimelineRangeIsValid(clock->range) ||
        clock->range.frame_count>INT64_MAX || !TimelineRateIsValid(clock->source.rate) ||
        clock->source.first_tick>clock->source.last_tick || clock->source.last_tick>INT64_MAX)
        return SEQUENCE_INVALID_RANGE;
    if (!identifier(clock->source.id,sizeof(clock->source.id),false) ||
        !identifier(clock->source.revision,sizeof(clock->source.revision),true))
        return SEQUENCE_INVALID_IDENTITY;
    if (!clock->segment_count || clock->segment_count>SEQUENCE_SEGMENT_CAPACITY)
        return SEQUENCE_INVALID_SEGMENTS;
    uint64_t next=0;
    SequenceTime previous_end={0,1};
    for (size_t i=0;i<clock->segment_count;++i) {
        const SequenceSegment *seg=&clock->segments[i];
        if (!identifier(seg->id,sizeof(seg->id),false)) return SEQUENCE_INVALID_IDENTITY;
        for (size_t j=0;j<i;++j)
            if (!strcmp(seg->id,clock->segments[j].id)) return SEQUENCE_INVALID_IDENTITY;
        if (seg->start_frame!=next || !seg->frame_count ||
            seg->frame_count>clock->range.frame_count-next)
            return SEQUENCE_INVALID_SEGMENTS;
        SequenceTime start, speed, end, tick, step;
        TRY(SequenceTimeReduce(seg->source_start,&start));
        TRY(SequenceTimeReduce(seg->speed,&speed));
        if ((seg->kind!=SEQUENCE_PLAY && seg->kind!=SEQUENCE_HOLD) ||
            (seg->kind==SEQUENCE_PLAY && !speed.numerator) ||
            (seg->kind==SEQUENCE_HOLD && (seg->speed.numerator || seg->speed.denominator!=1)))
            return SEQUENCE_INVALID_SEGMENTS;
        if (i && SequenceTimeCompare(start,previous_end)!=0)
            return SEQUENCE_DISCONTINUOUS_SOURCE;
        TRY(tick_at(clock,start,&tick));
        TRY(in_envelope(clock,tick));
        if (tick.denominator!=1) return SEQUENCE_UNSUPPORTED_SOURCE_SAMPLE;
        TRY(SequenceTimeMultiply(seconds_per_frame(clock->rate),speed,&step));
        TRY(tick_at(clock,step,&step));
        if (seg->frame_count>1 && step.denominator!=1)
            return SEQUENCE_UNSUPPORTED_SOURCE_SAMPLE;
        TRY(source_at(clock,seg,(SequenceTime){seg->frame_count,1},&end));
        TRY(tick_at(clock,end,&tick));
        TRY(in_envelope(clock,tick));
        previous_end=end;
        next+=seg->frame_count;
    }
    return next==clock->range.frame_count ? SEQUENCE_OK : SEQUENCE_INVALID_SEGMENTS;
}
SequenceStatus SequenceClockDuration(const SequenceClock *clock, SequenceTime *out) {
    if (!out) return SEQUENCE_INVALID_ARGUMENT;
    TRY(SequenceClockValidate(clock));
    return SequenceTimeMultiply((SequenceTime){clock->range.frame_count,1},
                                seconds_per_frame(clock->rate),out);
}
SequenceStatus SequenceClockEvaluate(const SequenceClock *clock,
    TimelineSample sample, SequenceEvaluation *out) {
    if (!out) return SEQUENCE_INVALID_ARGUMENT;
    TRY(SequenceClockValidate(clock));
    SequenceEvaluation result;
    memset(&result,0,sizeof(result));
    TimelineStatus status=TimelineEvaluationContextBuild(clock->rate,clock->range,
                                                         sample,&result.presentation_context);
    if (status!=TIMELINE_STATUS_OK) return SEQUENCE_SAMPLE_OUT_OF_RANGE;
    SequenceTime frame, sub={sample.subframe_numerator,sample.subframe_denominator};
    TRY(SequenceTimeAdd((SequenceTime){result.presentation_context.local_frame,1},sub,&frame));
    TRY(SequenceTimeMultiply(frame,seconds_per_frame(clock->rate),&result.presentation_time));
    size_t index=0;
    while (index+1<clock->segment_count &&
           result.presentation_context.local_frame>=clock->segments[index+1].start_frame) ++index;
    const SequenceSegment *seg=&clock->segments[index];
    SequenceTime local;
    TRY(SequenceTimeSubtract(frame,(SequenceTime){seg->start_frame,1},&local));
    TRY(SequenceTimeMultiply(local,seconds_per_frame(clock->rate),&result.segment_time));
    TRY(source_at(clock,seg,local,&result.simulation_time));
    SequenceTime tick;
    TRY(tick_at(clock,result.simulation_time,&tick));
    TRY(in_envelope(clock,tick));
    if (tick.denominator!=1) return SEQUENCE_UNSUPPORTED_SOURCE_SAMPLE;
    result.version=SEQUENCE_CLOCK_VERSION;
    result.segment_index=index;
    result.source_tick=tick.numerator;
    result.source=clock->source;
    *out=result;
    return SEQUENCE_OK;
}
SequenceStatus SequenceClockEvaluateChunk(const SequenceClock *clock,
    int64_t start, uint64_t offset, uint32_t numerator, uint32_t denominator,
    SequenceEvaluation *out) {
    if (!out) return SEQUENCE_INVALID_ARGUMENT;
    TRY(SequenceClockValidate(clock));
    TimelineEvaluationContext context;
    TimelineStatus status=TimelineEvaluationContextBuildFromChunk(clock->rate,clock->range,
        start,offset,numerator,denominator,&context);
    if (status!=TIMELINE_STATUS_OK) return SEQUENCE_SAMPLE_OUT_OF_RANGE;
    return SequenceClockEvaluate(clock,context.sample,out);
}
SequenceStatus SequenceClockMotionTime(const SequenceEvaluation *evaluation,
    SequenceMotionClock domain, SequenceTime *out) {
    if (!evaluation || !out) return SEQUENCE_INVALID_ARGUMENT;
    if (evaluation->version!=SEQUENCE_CLOCK_VERSION) return SEQUENCE_INVALID_VERSION;
    SequenceTime value;
    switch(domain) {
    case SEQUENCE_MOTION_PRESENTATION: value=evaluation->presentation_time; break;
    case SEQUENCE_MOTION_SIMULATION: value=evaluation->simulation_time; break;
    case SEQUENCE_MOTION_SEGMENT_LOCAL: value=evaluation->segment_time; break;
    default: return SEQUENCE_INVALID_ARGUMENT;
    }
    return SequenceTimeReduce(value,out);
}

static bool equal_time(SequenceTime a,SequenceTime b) {
    return SequenceTimeReduce(a,&a)==SEQUENCE_OK && SequenceTimeReduce(b,&b)==SEQUENCE_OK &&
        SequenceTimeCompare(a,b)==0;
}
static bool equal_sample(const TimelineEvaluationContext *a,const TimelineEvaluationContext *b) {
    return a->rate.frames_per_second_numerator==b->rate.frames_per_second_numerator &&
      a->rate.frames_per_second_denominator==b->rate.frames_per_second_denominator &&
      a->range.start_frame==b->range.start_frame && a->range.frame_count==b->range.frame_count &&
      a->sample.absolute_frame==b->sample.absolute_frame && a->sample.subframe_denominator && b->sample.subframe_denominator &&
      (uint64_t)a->sample.subframe_numerator*b->sample.subframe_denominator==
      (uint64_t)b->sample.subframe_numerator*a->sample.subframe_denominator;
}
bool SequenceClockEvaluationsEqual(const SequenceEvaluation *a,const SequenceEvaluation *b) {
    return a && b && a->version==SEQUENCE_CLOCK_VERSION && b->version==SEQUENCE_CLOCK_VERSION &&
      equal_sample(&a->presentation_context,&b->presentation_context) &&
      equal_time(a->presentation_time,b->presentation_time) && equal_time(a->simulation_time,b->simulation_time) &&
      equal_time(a->segment_time,b->segment_time) && a->source_tick==b->source_tick && a->segment_index==b->segment_index &&
      !strncmp(a->source.id,b->source.id,SEQUENCE_ID_CAPACITY) &&
      !strncmp(a->source.revision,b->source.revision,SEQUENCE_REVISION_CAPACITY) &&
      a->source.rate.frames_per_second_numerator==b->source.rate.frames_per_second_numerator &&
      a->source.rate.frames_per_second_denominator==b->source.rate.frames_per_second_denominator &&
      a->source.first_tick==b->source.first_tick && a->source.last_tick==b->source.last_tick;
}
