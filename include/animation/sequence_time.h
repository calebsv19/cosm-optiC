#ifndef RAY_TRACING_SEQUENCE_TIME_H
#define RAY_TRACING_SEQUENCE_TIME_H
#include "animation/sequence_clock.h"
SequenceStatus SequenceTimeReduce(SequenceTime value, SequenceTime *out);
SequenceStatus SequenceTimeAdd(SequenceTime a, SequenceTime b, SequenceTime *out);
SequenceStatus SequenceTimeSubtract(SequenceTime a, SequenceTime b, SequenceTime *out);
SequenceStatus SequenceTimeMultiply(SequenceTime a, SequenceTime b, SequenceTime *out);
/* Inputs must already be valid reduced nonnegative rationals. No cross-product. */
int SequenceTimeCompare(SequenceTime a, SequenceTime b);
#endif
