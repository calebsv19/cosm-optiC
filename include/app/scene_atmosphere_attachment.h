#ifndef SCENE_ATMOSPHERE_ATTACHMENT_H
#define SCENE_ATMOSPHERE_ATTACHMENT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <json-c/json.h>
#include "animation/timeline_clock.h"

/* Project-relative, solver-free cache attachment. One attachment in v1. */
typedef struct SceneAtmosphereAttachment {
    bool present, enabled, visible, affects_lighting, animated, missing_is_error;
    char id[96], source_path[1024], source_kind[24];
    TimelineRate source_rate;
    int64_t timeline_start, source_start;
    double density_scale, density_gamma, absorption_gain, scatter_gain, step_scale;
    double tint[3], translation[3], rotation_degrees[3], scale[3];
} SceneAtmosphereAttachment;

void SceneAtmosphereAttachmentDefaults(SceneAtmosphereAttachment* out);
/* Missing declaration is valid and leaves legacy selection untouched. */
bool SceneAtmosphereAttachmentParse(json_object* authoring,
    SceneAtmosphereAttachment* out, char* diagnostic, size_t size);
json_object* SceneAtmosphereAttachmentToJson(const SceneAtmosphereAttachment* value);
bool SceneAtmosphereAttachmentValidate(const SceneAtmosphereAttachment* value,
    char* diagnostic, size_t size);
bool SceneAtmosphereAttachmentRequestedFrame(const SceneAtmosphereAttachment* value,
    const TimelineEvaluationContext* frame, int64_t* out, char* diagnostic, size_t size);
/* Sorted actual source IDs, not array ordinals. Hold chooses predecessor and
 * clamps ends. Error refuses any absent frame, including outside the range. */
bool SceneAtmosphereAttachmentSelectFrame(const SceneAtmosphereAttachment* value,
    int64_t requested, const int64_t* ids, size_t count, int64_t* selected,
    char* diagnostic, size_t size);
#endif
