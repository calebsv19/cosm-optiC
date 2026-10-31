/* Pinned complete VF3D frames are a source adapter, never a solver. */
#include "import/runtime_sequence_source.h"
#include "app/ray_tracing_sha256.h"
#include "app/scene_atmosphere_assets.h"
#include "app/scene_atmosphere_runtime.h"
#include "import/fluid_volume_import_3d.h"
#include "import/scene_timeline_document_io.h"
#include "import/sequence_clock_json.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static SequenceClock clock_value;
static json_object *pins;
static json_object *get(json_object *o, const char *k) {
  json_object *v = NULL;
  if (o)
    json_object_object_get_ex(o, k, &v);
  return v;
}
static const char *str(json_object *o, const char *k) {
  json_object *v = get(o, k);
  return json_object_is_type(v, json_type_string) ? json_object_get_string(v)
                                                  : "";
}
static bool fail(char *d, size_t n, const char *s) {
  if (d && n)
    snprintf(d, n, "%s", s);
  return false;
}
static bool digest(const char *s) {
  if (strlen(s) != 64)
    return false;
  for (; *s; s++)
    if (!strchr("0123456789abcdef", *s))
      return false;
  return true;
}
static json_object *author(json_object *s) {
  return get(get(get(s, "extensions"), "ray_tracing"), "authoring");
}
static bool exact_object(json_object *o, size_t n) {
  return json_object_is_type(o, json_type_object) &&
         json_object_object_length(o) == n;
}
bool RuntimeSequenceValidate(json_object *scene, const char *path, char *d,
                             size_t n) {
  json_object *a = author(scene), *binding = get(a, "sequence_volume"),
              *seq = get(a, "sequence");
  if (!binding)
    return true;
  SequenceClock c;
  SceneAtmosphereAttachment v;
  if (!exact_object(binding, 2) || json_object_get_string_len(get(binding,"schema")) != 22 ||
      strcmp(str(binding, "schema"), "ray_sequence_volume_v1") ||
      !SequenceClockFromJson(seq, &c) ||
      !SceneAtmosphereAttachmentParse(a, &v, d, n) || !v.present ||
      !v.enabled || !v.animated || strcmp(v.source_kind, "manifest") ||
      !v.missing_is_error ||
      get(get(get(scene, "extensions"), "ray_tracing"), "rigid_result") ||
      get(get(scene, "extensions"), "rigid_scene") ||
      (get(a, "inspection_moves") || get(a,"event_cameras")))
    return fail(d, n,
                "sequence_volume requires one enabled exact VF3D manifest "
                "source; rigid, bundle and inspection targets cannot be mixed");
  if (strcmp(c.source.id, str(scene, "scene_id")) ||
      c.source.rate.frames_per_second_numerator !=
          v.source_rate.frames_per_second_numerator ||
      c.source.rate.frames_per_second_denominator !=
          v.source_rate.frames_per_second_denominator ||
      c.source.last_tick > INT_MAX ||
      c.source.last_tick - c.source.first_tick >= 10000)
    return fail(d, n, "sequence_volume source identity/rate/envelope mismatch");
  TimelineDocument *t = malloc(sizeof(*t));
  bool timeline = t &&
                  SceneTimelineDocumentFromJson(get(a, "scene_timeline"), t) ==
                      TIMELINE_STATUS_OK &&
                  t->rate.frames_per_second_numerator ==
                      c.rate.frames_per_second_numerator &&
                  t->rate.frames_per_second_denominator ==
                      c.rate.frames_per_second_denominator &&
                  t->range.start_frame == c.range.start_frame &&
                  t->range.frame_count == c.range.frame_count;
  free(t);
  if (!timeline)
    return fail(
        d, n,
        "sequence_volume requires matching retained presentation timeline");
  json_object *list = get(binding, "frames");
  size_t count = (size_t)(c.source.last_tick - c.source.first_tick + 1);
  if (!json_object_is_type(list, json_type_array) ||
      json_object_array_length(list) != count)
    return fail(
        d, n,
        "sequence_volume pins must cover the complete contiguous envelope");
  for (size_t i = 0; i < count; i++) {
    json_object *row = json_object_array_get_idx(list, i),
                *tick = get(row, "frame_index");
    if (!exact_object(row, 2) || !json_object_is_type(tick, json_type_int) ||
        json_object_get_int64(tick) < 0 ||
        json_object_get_uint64(tick) != c.source.first_tick + i ||
        (!digest(str(row, "sha256")) ||
         json_object_get_string_len(get(row, "sha256")) != 64))
      return fail(d, n, "invalid sequence_volume frame pin");
  }
  if (!path || !*path)
    return true; /* Typed JSON check; file admission also authenticates bytes.
                  */
  char manifest[4096], dir[4096], hash[65];
  snprintf(dir, sizeof(dir), "%s", path);
  char *slash = strrchr(dir, '/');
  if (!slash)
    return fail(d, n, "sequence_volume needs a file-backed project");
  *slash = 0;
  if (!SceneAtmosphereAssetPath(path, v.source_path, dir, manifest,
                                sizeof(manifest), d, n) ||
      !ray_tracing_sha256_file(manifest, hash) ||
      strcmp(hash, c.source.revision))
    return fail(d, n, "sequence_volume manifest revision mismatch");
  json_object *m = json_object_from_file(manifest), *frames = get(m, "frames");
  bool ok = m && json_object_is_type(frames, json_type_array) &&
            json_object_array_length(frames) == count;
  for (size_t i = 0; ok && i < count; i++) {
    json_object *entry = json_object_array_get_idx(frames, i);
    char selected[4096], verified[4096];
    uint64_t actual;
    ok = json_object_is_type(get(entry, "frame_index"), json_type_int) &&
         json_object_get_uint64(get(entry, "frame_index")) ==
             c.source.first_tick + i &&
         fluid_volume_import_3d_resolve_source_frame_path(
             manifest, RUNTIME_VOLUME_3D_SOURCE_MANIFEST,
             (int)(c.source.first_tick + i), selected, sizeof(selected), d, n);
    size_t len = strlen(dir);
    ok = ok && !strncmp(selected, dir, len) && selected[len] == '/' &&
         SceneAtmosphereAssetPath(path, selected + len + 1, dir, verified,
                                  sizeof(verified), d, n) &&
         ray_tracing_sha256_file(verified, hash) &&
         !strcmp(hash, str(json_object_array_get_idx(list, i), "sha256")) &&
         fluid_volume_import_3d_single_frame_index(
             verified, RUNTIME_VOLUME_3D_SOURCE_RAW_VF3D, &actual) &&
         actual == c.source.first_tick + i;
    if (ok) {
      RuntimeVolumeAttachment3D volume;
      RuntimeVolumeAttachment3D_Init(&volume);
      ok = fluid_volume_import_3d_load_source_at_frame(
          manifest, RUNTIME_VOLUME_3D_SOURCE_MANIFEST, (int)actual, &volume, d,
          n);
      RuntimeVolumeAttachment3D_Free(&volume);
    }
  }
  if (m)
    json_object_put(m);
  return ok || fail(d, n,
                    "sequence_volume missing, changed, malformed or "
                    "noncontiguous frame");
}
void RuntimeSequenceReset(void) {
  if (pins)
    json_object_put(pins);
  pins = NULL;
  memset(&clock_value, 0, sizeof(clock_value));
}
bool RuntimeSequenceLoad(json_object *scene) {
  RuntimeSequenceReset();
  json_object *a = author(scene), *binding = get(a, "sequence_volume");
  if (!binding)
    return true;
  if (!RuntimeSequenceValidate(scene, NULL, NULL, 0) ||
      !SequenceClockFromJson(get(a, "sequence"), &clock_value))
    return false;
  return json_object_deep_copy(get(binding, "frames"), &pins, NULL) == 0 &&
         pins;
}
bool RuntimeSequenceVolumeActive(void) { return pins != NULL; }
uint64_t RuntimeSequenceRevision(void) {
  return pins ? SequenceClockFingerprint(&clock_value)
              : RuntimeRigidResultSequenceRevision();
}
bool RuntimeSequenceClock(TimelineRate *r, TimelineRange *g) {
  if (!pins)
    return RuntimeRigidResultClock(r, g);
  if (!r || !g)
    return false;
  *r = clock_value.rate;
  *g = clock_value.range;
  return true;
}
TimelineStatus RuntimeSequenceEvaluate(const TimelineEvaluationContext *c,
                                       SequenceEvaluation *out, bool *present) {
  if (!pins)
    return RuntimeRigidResultSequence(c, out, present);
  if (!c || !out || !present)
    return TIMELINE_STATUS_INVALID_ARGUMENT;
  if (c->rate.frames_per_second_numerator !=
          clock_value.rate.frames_per_second_numerator ||
      c->rate.frames_per_second_denominator !=
          clock_value.rate.frames_per_second_denominator ||
      c->range.start_frame != clock_value.range.start_frame ||
      c->range.frame_count != clock_value.range.frame_count)
    return TIMELINE_STATUS_INVALID_SNAPSHOT;
  SequenceEvaluation e;
  if (SequenceClockEvaluate(&clock_value, c->sample, &e) != SEQUENCE_OK)
    return TIMELINE_STATUS_INVALID_SNAPSHOT;
  *out = e;
  *present = true;
  return TIMELINE_STATUS_OK;
}
bool RuntimeSequenceVolumeFrame(const TimelineEvaluationContext *c,
                                int64_t *out) {
  SequenceEvaluation e;
  bool p = false;
  if (!pins || !out ||
      RuntimeSequenceEvaluate(c, &e, &p) != TIMELINE_STATUS_OK || !p)
    return false;
  *out = (int64_t)e.source_tick;
  return true;
}
bool RuntimeSequenceVolumeVerify(const char *path, const char *content,
                                 const char *manifest, int64_t tick) {
  (void)path;
  if (!pins)
    return true;
  if (tick < 0 || (uint64_t)tick < clock_value.source.first_tick ||
      (uint64_t)tick > clock_value.source.last_tick ||
      strcmp(manifest, clock_value.source.revision))
    return false;
  return !strcmp(
      content,
      str(json_object_array_get_idx(
              pins, (size_t)((uint64_t)tick - clock_value.source.first_tick)),
          "sha256"));
}
