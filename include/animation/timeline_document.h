#ifndef RAY_TRACING_TIMELINE_DOCUMENT_H
#define RAY_TRACING_TIMELINE_DOCUMENT_H

#include <stddef.h>

#include "animation/timeline_track.h"

#define TIMELINE_DOCUMENT_TRACK_CAPACITY 64u

typedef struct TimelineDocument {
    TimelineRate rate;
    TimelineRange range;
    size_t track_count;
    TimelineTrack tracks[TIMELINE_DOCUMENT_TRACK_CAPACITY];
} TimelineDocument;

TimelineStatus TimelineDocumentInit(TimelineDocument* document,
                                    TimelineRate rate,
                                    TimelineRange range);
TimelineStatus TimelineDocumentAddTrack(TimelineDocument* document,
                                        const TimelineTrack* track);
TimelineStatus TimelineDocumentValidate(const TimelineDocument* document);
TimelineStatus TimelineDocumentEvaluate(
    const TimelineDocument* document,
    const TimelineEvaluationContext* context,
    TimelineEvaluationResult* out_results,
    size_t result_capacity,
    size_t* out_result_count);

#endif
