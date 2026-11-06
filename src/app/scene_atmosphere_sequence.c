#include "app/scene_atmosphere_sequence.h"
#include "import/sequence_clock_json.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
static bool fail(char *d, size_t n, const char *s) {
  if (d && n)
    snprintf(d, n, "%s", s);
  return false;
}
static json_object *get(json_object *j, const char *k) {
  json_object *v = NULL;
  if (j)
    json_object_object_get_ex(j, k, &v);
  return v;
}
bool SceneAtmosphereSequenceRead(json_object *author, json_object *playback,
                                 SceneAtmosphereAttachment *v, char *d,
                                 size_t n) {
  json_object *clock = NULL;
  if (!playback || !json_object_object_get_ex(playback, "clock", &clock))
    return true;
  if (!json_object_is_type(clock, json_type_string))
    return fail(d, n,
                "attachment clock must be presentation or sequence_source");
  const char *name = json_object_get_string(clock);
  if ((size_t)json_object_get_string_len(clock) != strlen(name))
    return fail(d, n, "attachment clock contains NUL");
  if (!strcmp(name, "presentation"))
    return true;
  if (strcmp(name, "sequence_source"))
    return fail(d, n, "unsupported attachment clock");
  if (!SequenceClockFromJson(get(author, "sequence"), &v->sequence_clock))
    return fail(
        d, n, "sequence_source attachment requires a valid retained sequence");
  v->sequence_source_clock = true;
  return true;
}
bool SceneAtmosphereSequenceValidate(const SceneAtmosphereAttachment *v,
                                     char *d, size_t n) {
  if (!v->sequence_source_clock)
    return true;
  if (strcmp(v->source_kind, "scene_bundle") || v->visible ||
      v->affects_lighting || v->density_scale != 0 || v->opacity != 0)
    return fail(d, n,
                "sequence_source currently requires a water-only scene bundle "
                "with hidden zero atmosphere");
  if (SequenceClockValidate(&v->sequence_clock) != SEQUENCE_OK ||
      v->sequence_clock.source.last_tick > INT64_MAX)
    return fail(d, n, "invalid retained water source clock");
  return true;
}
bool SceneAtmosphereSequenceContext(const SceneAtmosphereAttachment *v,
                                    const TimelineEvaluationContext *frame,
                                    TimelineEvaluationContext *out, char *d,
                                    size_t n) {
  if (!v || !frame || !out)
    return fail(d, n, "missing attachment clock context");
  if (!v->sequence_source_clock) {
    *out = *frame;
    return true;
  }
  const SequenceClock *c = &v->sequence_clock;
  SequenceEvaluation e;
  if (!SceneAtmosphereSequenceValidate(v, d, n) ||
      frame->rate.frames_per_second_numerator !=
          c->rate.frames_per_second_numerator ||
      frame->rate.frames_per_second_denominator !=
          c->rate.frames_per_second_denominator ||
      frame->range.start_frame != c->range.start_frame ||
      frame->range.frame_count != c->range.frame_count ||
      SequenceClockEvaluate(c, frame->sample, &e) != SEQUENCE_OK)
    return fail(
        d, n, "attachment presentation clock does not match retained sequence");
  TimelineEvaluationContext mapped;
  if (TimelineEvaluationContextBuild(
          c->source.rate,
          (TimelineRange){(int64_t)c->source.first_tick,
                          c->source.last_tick - c->source.first_tick + 1},
          (TimelineSample){(int64_t)e.source_tick, 0, 1},
          &mapped) != TIMELINE_STATUS_OK)
    return fail(d, n, "attachment recorded source sample unavailable");
  *out = mapped;
  return true;
}
bool SceneAtmosphereSequenceMatches(const SceneAtmosphereAttachment *v,
                                    const SequenceEvaluation *e) {
  SequenceEvaluation own;
  return v && e && v->present && v->enabled && v->sequence_source_clock &&
         SceneAtmosphereSequenceValidate(v, NULL, 0) &&
         SequenceClockEvaluate(&v->sequence_clock,
                               e->presentation_context.sample,
                               &own) == SEQUENCE_OK &&
         SequenceClockEvaluationsEqual(&own, e);
}
