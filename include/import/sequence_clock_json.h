#pragma once
#include "animation/sequence_clock.h"
#include <json-c/json.h>
/* Exact versioned codec. Unknown fields/types refuse; output changes on success only. */
bool SequenceClockFromJson(json_object *root, SequenceClock *out);
json_object *SequenceClockToJson(const SequenceClock *clock);
uint64_t SequenceClockFingerprint(const SequenceClock *clock);
