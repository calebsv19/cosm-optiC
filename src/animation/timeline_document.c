#include "animation/timeline_document.h"

#include <string.h>

TimelineStatus TimelineDocumentInit(TimelineDocument* document,
                                    TimelineRate rate,
                                    TimelineRange range) {
    TimelineDocument candidate;
    if (!document) return TIMELINE_STATUS_INVALID_ARGUMENT;
    if (!TimelineRateIsValid(rate)) return TIMELINE_STATUS_INVALID_RATE;
    if (!TimelineRangeIsValid(range)) return TIMELINE_STATUS_INVALID_RANGE;
    memset(&candidate, 0, sizeof(candidate));
    candidate.rate = rate;
    candidate.range = range;
    *document = candidate;
    return TIMELINE_STATUS_OK;
}

TimelineStatus TimelineDocumentAddTrack(TimelineDocument* document,
                                        const TimelineTrack* track) {
    TimelineStatus status;
    if (!document || !track) return TIMELINE_STATUS_INVALID_ARGUMENT;
    if (document->track_count >= TIMELINE_DOCUMENT_TRACK_CAPACITY) {
        return TIMELINE_STATUS_CAPACITY_EXCEEDED;
    }
    status = TimelineTrackValidate(track, &document->range);
    if (status != TIMELINE_STATUS_OK) return status;
    for (size_t i = 0u; i < document->track_count; ++i) {
        if (strcmp(document->tracks[i].track_id, track->track_id) == 0) {
            return TIMELINE_STATUS_DUPLICATE_ID;
        }
    }
    document->tracks[document->track_count++] = *track;
    return TIMELINE_STATUS_OK;
}

TimelineStatus TimelineDocumentValidate(const TimelineDocument* document) {
    if (!document) return TIMELINE_STATUS_INVALID_ARGUMENT;
    if (!TimelineRateIsValid(document->rate)) return TIMELINE_STATUS_INVALID_RATE;
    if (!TimelineRangeIsValid(document->range)) return TIMELINE_STATUS_INVALID_RANGE;
    if (document->track_count > TIMELINE_DOCUMENT_TRACK_CAPACITY) {
        return TIMELINE_STATUS_CAPACITY_EXCEEDED;
    }
    for (size_t i = 0u; i < document->track_count; ++i) {
        TimelineStatus status = TimelineTrackValidate(&document->tracks[i],
                                                      &document->range);
        if (status != TIMELINE_STATUS_OK) return status;
        for (size_t j = i + 1u; j < document->track_count; ++j) {
            if (strcmp(document->tracks[i].track_id,
                       document->tracks[j].track_id) == 0) {
                return TIMELINE_STATUS_DUPLICATE_ID;
            }
        }
    }
    return TIMELINE_STATUS_OK;
}

TimelineStatus TimelineDocumentEvaluate(
    const TimelineDocument* document,
    const TimelineEvaluationContext* context,
    TimelineEvaluationResult* out_results,
    size_t result_capacity,
    size_t* out_result_count) {
    TimelineStatus status;
    size_t count = 0u;
    if (!document || !context || !out_result_count ||
        (document->track_count > 0u && !out_results)) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    status = TimelineDocumentValidate(document);
    if (status != TIMELINE_STATUS_OK) return status;
    if (context->rate.frames_per_second_numerator !=
            document->rate.frames_per_second_numerator ||
        context->rate.frames_per_second_denominator !=
            document->rate.frames_per_second_denominator ||
        context->range.start_frame != document->range.start_frame ||
        context->range.frame_count != document->range.frame_count) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    if (result_capacity < document->track_count) {
        return TIMELINE_STATUS_CAPACITY_EXCEEDED;
    }
    for (size_t i = 0u; i < document->track_count; ++i) {
        if (!document->tracks[i].enabled) continue;
        status = TimelineTrackEvaluate(&document->tracks[i], context,
                                       &out_results[count]);
        if (status != TIMELINE_STATUS_OK) return status;
        out_results[count].track_index = i;
        count += 1u;
    }
    *out_result_count = count;
    return TIMELINE_STATUS_OK;
}
