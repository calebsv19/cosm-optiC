#ifndef RUNTIME_SEQUENCE_SOURCE_H
#define RUNTIME_SEQUENCE_SOURCE_H
#include "import/runtime_rigid_result.h"
#include <json-c/json.h>
bool RuntimeSequenceValidate(json_object *, const char *, char *, size_t);
bool RuntimeSequenceLoad(json_object *);
void RuntimeSequenceReset(void);
bool RuntimeSequenceVolumeActive(void);
uint64_t RuntimeSequenceRevision(void);
bool RuntimeSequenceClock(TimelineRate *, TimelineRange *);
TimelineStatus RuntimeSequenceEvaluate(const TimelineEvaluationContext *,
                                       SequenceEvaluation *, bool *);
bool RuntimeSequenceVolumeFrame(const TimelineEvaluationContext *, int64_t *);
bool RuntimeSequenceVolumeVerify(const char *, const char *, const char *,
                                 int64_t);
#endif
