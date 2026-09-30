#ifndef EVALUATED_LIGHT_ROUTE_H
#define EVALUATED_LIGHT_ROUTE_H
#include "animation/timeline_light_motion.h"
TimelineStatus EvaluatedLightRouteSample(const TimelineEvaluationResult *progress,
    const TimelineEvaluationContext *context, TimelineLightMotionSample *out);
#endif
