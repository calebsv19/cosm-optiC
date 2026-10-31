#include "scene_editor_sequence_segments.h"
#include "animation/sequence_time.h"
#include "import/runtime_rigid_result.h"
#include "import/runtime_scene_timeline.h"
#include "import/sequence_clock_json.h"
#include <stdio.h>
#include <string.h>
static json_object *get(json_object *o, const char *k) {
  json_object *v = NULL;
  if (o)
    json_object_object_get_ex(o, k, &v);
  return v;
}
static const char *str(json_object *o, const char *k) {
  return json_object_get_string(get(o, k));
}
bool SceneEditorSequenceSegments(json_object *scene, json_object *author,
                                 const char *action, json_object *p,
                                 unsigned long long revision) {
  SequenceClock c = {0};
  if (!strcmp(action, "source_setup")) {
    if (get(author, "sequence") ||
        RuntimeSceneTimelineClock(&c.rate, &c.range) != TIMELINE_STATUS_OK)
      return false;
    json_object *source = RuntimeRigidResultInspection();
    if (!source)
      return false;
    c.version = 1;
    c.source.rate = (TimelineRate){240, 1};
    c.source.last_tick = json_object_get_uint64(get(source, "endpoint_tick"));
    snprintf(c.source.id, 64, "%s", str(scene, "scene_id"));
    snprintf(c.source.revision, 65, "%s", str(source, "result_revision"));
    json_object_put(source);
    uint64_t play = c.source.last_tick * c.rate.frames_per_second_numerator /
                    ((uint64_t)240 * c.rate.frames_per_second_denominator);
    if (play >= c.range.frame_count)
      play = c.range.frame_count - 1;
    c.segment_count = play ? 2 : 1;
    if (play)
      c.segments[0] = (SequenceSegment){.id = "play",
                                        .frame_count = play,
                                        .kind = SEQUENCE_PLAY,
                                        .source_start = {0, 1},
                                        .speed = {1, 1}};
    SequenceTime at;
    if (SequenceTimeMultiply(
            (SequenceTime){play, 1},
            (SequenceTime){c.rate.frames_per_second_denominator,
                           c.rate.frames_per_second_numerator},
            &at) != SEQUENCE_OK)
      return false;
    c.segments[c.segment_count - 1] =
        (SequenceSegment){.id = "hold",
                          .start_frame = play,
                          .frame_count = c.range.frame_count - play,
                          .kind = SEQUENCE_HOLD,
                          .source_start = at,
                          .speed = {0, 1}};
  } else {
    if (!SequenceClockFromJson(get(author, "sequence"), &c))
      return false;
    if (!strcmp(action, "segment_split")) {
      json_object *frame = get(p, "frame");
      if (!json_object_is_type(frame, json_type_int) ||
          json_object_get_int64(frame) < 0 ||
          c.segment_count == SEQUENCE_SEGMENT_CAPACITY)
        return false;
      uint64_t local = json_object_get_uint64(frame);
      size_t index = c.segment_count;
      for (size_t i = 0; i < c.segment_count; i++)
        if (local > c.segments[i].start_frame &&
            local < c.segments[i].start_frame + c.segments[i].frame_count)
          index = i;
      if (index == c.segment_count)
        return false;
      SequenceEvaluation at;
      if (SequenceClockEvaluate(
              &c, (TimelineSample){c.range.start_frame + (int64_t)local, 0, 1},
              &at) != SEQUENCE_OK)
        return false;
      memmove(&c.segments[index + 2], &c.segments[index + 1],
              (c.segment_count - index - 1) * sizeof(SequenceSegment));
      c.segments[index + 1] = c.segments[index];
      snprintf(c.segments[index + 1].id, 64, "split_%llu", revision);
      c.segments[index + 1].frame_count =
          c.segments[index].start_frame + c.segments[index].frame_count - local;
      c.segments[index + 1].start_frame = local;
      c.segments[index + 1].source_start = at.simulation_time;
      c.segments[index].frame_count = local - c.segments[index].start_frame;
      c.segment_count++;
    } else if (!strcmp(action, "segment_boundary")) {
      const char *id = str(p, "id");
      json_object *v = get(p, "end_frame");
      if (!id || !json_object_is_type(v, json_type_int) ||
          json_object_get_int64(v) < 1)
        return false;
      uint64_t end = json_object_get_uint64(v);
      size_t index = c.segment_count;
      for (size_t i = 0; i + 1 < c.segment_count; i++)
        if (!strcmp(c.segments[i].id, id))
          index = i;
      if (index == c.segment_count || end <= c.segments[index].start_frame ||
          end >= c.segments[index + 1].start_frame +
                     c.segments[index + 1].frame_count)
        return false;
      c.segments[index].frame_count = end - c.segments[index].start_frame;
      c.segments[index + 1].frame_count = c.segments[index + 1].start_frame +
                                          c.segments[index + 1].frame_count -
                                          end;
      c.segments[index + 1].start_frame = end;
      SequenceTime next = c.segments[0].source_start;
      for (size_t i = 0; i < c.segment_count; i++) {
        c.segments[i].source_start = next;
        SequenceTime duration, advance;
        if (SequenceTimeMultiply(
                (SequenceTime){c.segments[i].frame_count, 1},
                (SequenceTime){c.rate.frames_per_second_denominator,
                               c.rate.frames_per_second_numerator},
                &duration) != SEQUENCE_OK ||
            SequenceTimeMultiply(duration, c.segments[i].speed, &advance) !=
                SEQUENCE_OK ||
            SequenceTimeAdd(next, advance, &next) != SEQUENCE_OK)
          return false;
      }
    } else if (!strcmp(action, "segment_mode")) {
      const char *id = str(p, "id"), *kind = str(p, "kind");
      if (!id || !kind)
        return false;
      bool found = false;
      for (size_t i = 0; i < c.segment_count; i++)
        if (!strcmp(id, c.segments[i].id)) {
          found = true;
          if (!strcmp(kind, "hold")) {
            c.segments[i].kind = SEQUENCE_HOLD;
            c.segments[i].speed = (SequenceTime){0, 1};
          } else if (!strcmp(kind, "play")) {
            c.segments[i].kind = SEQUENCE_PLAY;
            c.segments[i].speed = (SequenceTime){1, 1};
          } else
            return false;
        }
      if (!found)
        return false;
      SequenceTime next = c.segments[0].source_start;
      for (size_t i = 0; i < c.segment_count; i++) {
        c.segments[i].source_start = next;
        SequenceTime duration, advance;
        if (SequenceTimeMultiply(
                (SequenceTime){c.segments[i].frame_count, 1},
                (SequenceTime){c.rate.frames_per_second_denominator,
                               c.rate.frames_per_second_numerator},
                &duration) != SEQUENCE_OK ||
            SequenceTimeMultiply(duration, c.segments[i].speed, &advance) !=
                SEQUENCE_OK ||
            SequenceTimeAdd(next, advance, &next) != SEQUENCE_OK)
          return false;
      }
    } else
      return false;
  }
  json_object *value = SequenceClockToJson(&c);
  if (!value)
    return false;
  json_object_object_add(author, "sequence", value);
  return true;
}
