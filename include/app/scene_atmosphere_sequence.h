#pragma once
#include "app/scene_atmosphere_attachment.h"
bool SceneAtmosphereSequenceRead(json_object *author, json_object *playback,
                                 SceneAtmosphereAttachment *value,
                                 char *diagnostic, size_t size);
bool SceneAtmosphereSequenceValidate(const SceneAtmosphereAttachment *value,
                                     char *diagnostic, size_t size);
bool SceneAtmosphereSequenceContext(
    const SceneAtmosphereAttachment *value,
    const TimelineEvaluationContext *presentation,
    TimelineEvaluationContext *source, char *diagnostic, size_t size);
bool SceneAtmosphereSequenceMatches(const SceneAtmosphereAttachment *value,
                                    const SequenceEvaluation *sequence);
