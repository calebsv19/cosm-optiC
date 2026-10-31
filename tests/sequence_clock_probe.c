/* Text fixture driver for the independent Python Fraction oracle. */
#include "animation/sequence_clock.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    unsigned on,od,sn,sd,rn,rd,subn,subd;
    int64_t origin,frame;uint64_t play,hold,tail,last;
    while(scanf("%u %u %" SCNd64 " %" SCNu64 " %" SCNu64 " %" SCNu64
        " %u %u %u %u %" SCNu64 " %" SCNd64 " %u %u",
        &on,&od,&origin,&play,&hold,&tail,&rn,&rd,&sn,&sd,&last,&frame,&subn,&subd)==14) {
        SequenceClock c={0};c.version=1;c.rate=(TimelineRate){on,od};
        c.range=(TimelineRange){origin,play+hold+tail};strcpy(c.source.id,"oracle");
        memset(c.source.revision,'b',64);c.source.rate=(TimelineRate){sn,sd};c.source.last_tick=last;
        c.segment_count=3;
        SequenceTime join={play*(uint64_t)od*rn,(uint64_t)on*rd};
        c.segments[0]=(SequenceSegment){.id="a",.frame_count=play,.kind=SEQUENCE_PLAY,.source_start={0,1},.speed={rn,rd}};
        c.segments[1]=(SequenceSegment){.id="b",.start_frame=play,.frame_count=hold,.kind=SEQUENCE_HOLD,.source_start=join,.speed={0,1}};
        c.segments[2]=(SequenceSegment){.id="c",.start_frame=play+hold,.frame_count=tail,.kind=SEQUENCE_PLAY,.source_start=join,.speed={rn,rd}};
        SequenceEvaluation result;SequenceStatus status=SequenceClockEvaluate(&c,(TimelineSample){frame,subn,subd},&result);
        if(status!=SEQUENCE_OK) printf("%s\n",SequenceStatusLabel(status));
        else printf("ok %zu %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n",
            result.segment_index,result.presentation_time.numerator,result.presentation_time.denominator,
            result.segment_time.numerator,result.segment_time.denominator,
            result.simulation_time.numerator,result.simulation_time.denominator,result.source_tick);
    }
    return ferror(stdin)?1:0;
}
