#include "app/agent_render_request.h"
#include "app/ray_tracing_frame_recovery.h"
#include "app/ray_tracing_job_runner_internal.h"
#include "app/ray_tracing_request_utils.h"
#include "app/ray_tracing_sequence_recovery.h"
#include "app/ray_tracing_worker_protocol.h"
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static json_object *get(json_object *o, const char *key) {
  json_object *v = NULL;
  if (o)
    json_object_object_get_ex(o, key, &v);
  return v;
}
static bool text(json_object *o, const char *key, const char *s) {
  json_object *v = get(o, key);
  return json_object_is_type(v, json_type_string) &&
         json_object_get_string_len(v) == (int)strlen(s) &&
         !strcmp(json_object_get_string(v), s);
}
static bool integer(json_object *o, const char *key, int v) {
  return json_object_is_type(get(o, key), json_type_int) &&
         json_object_get_int64(get(o, key)) == v;
}
static bool yes(json_object *o, const char *key) {
  return json_object_is_type(get(o, key), json_type_boolean) &&
         json_object_get_boolean(get(o, key));
}
static bool latest_event(const RayTracingDetachedJobPaths *p, const char *id,
                         RayTracingWorkerEvent *out) {
  DIR *dir = opendir(p->worker_event_directory);
  if (!dir)
    return false;
  struct dirent *entry;
  char latest[PATH_MAX] = {0};
  uint64_t sequence = 0;
  bool valid = true;
  /* Scan durable names, then parse only the latest event. Long jobs should not
   * reread every progress document or hit an arbitrary event-count ceiling. */
  while ((entry = readdir(dir)) != NULL) {
    const char *name = entry->d_name;
    size_t length = strlen(name);
    if (name[0] < '0' || name[0] > '9' || length < 6 ||
        strcmp(name + length - 5, ".json"))
      continue;
    char *end = NULL;
    errno = 0;
    unsigned long long value = strtoull(name, &end, 10);
    if (errno || !end || *end != '_')
      continue;
    if (latest[0] && value == sequence && strcmp(name, latest)) {
      valid = false;
      break;
    }
    if (!latest[0] || value > sequence) {
      if (snprintf(latest, sizeof(latest), "%s", name) >= (int)sizeof(latest)) {
        valid = false;
        break;
      }
      sequence = value;
    }
  }
  closedir(dir);
  char path[PATH_MAX];
  int size =
      snprintf(path, sizeof(path), "%s/%s", p->worker_event_directory, latest);
  return valid && latest[0] && size > 0 && size < (int)sizeof(path) &&
         ray_tracing_worker_event_load_file(path, out, NULL, 0) &&
         out->sequence == sequence && !strcmp(out->job_id, id);
}
static bool completion_valid(const RayTracingDetachedJobPaths *p,
                             const RayTracingDetachedJobRecord *record, char *d,
                             size_t n) {
  RayTracingAgentRenderRequest r;
  char hash[65], summary_hash[65];
  if (strcmp(record->execution_mode, "worker_protocol") ||
      record->exit_code != 0 ||
      !ray_tracing_agent_render_request_load_file(p->job_request_path, &r, d,
                                                  n) ||
      !ray_tracing_sha256_file(p->job_request_path, hash) ||
      strcmp(hash, record->immutable_request_sha256) ||
      !ray_tracing_sha256_file(p->result_summary_path, summary_hash))
    return false;
  json_object *s = json_object_from_file(p->result_summary_path),
              *render = get(s, "render");
  bool ok = s && text(s, "schema_version", "ray_tracing_headless_summary_v1") &&
            yes(s, "request_loaded") && yes(s, "scene_applied") &&
            yes(s, "rendered_frames") && text(s, "run_id", r.run_id) &&
            text(s, "runtime_scene_path", r.runtime_scene_path) &&
            integer(s, "frames_rendered", r.frame_count) &&
            integer(render, "start_frame", r.start_frame) &&
            integer(render, "frame_count", r.frame_count) &&
            integer(render, "width", r.width) &&
            integer(render, "height", r.height);
  if (s)
    json_object_put(s);
  if (!ok)
    return false;
  RayTracingWorkerEvent last = {0};
  if (!latest_event(p, record->job_id, &last))
    return false;
  /* Worker completion is emitted only after waitpid reports native exit zero.
   * Its summary digest must bind the exact result we just inspected. */
  if (!ok || last.type != RAY_TRACING_WORKER_MESSAGE_COMPLETION ||
      last.exit_code != 0 || strcmp(last.summary_sha256, summary_hash))
    return false;
  int start =
      r.start_frame - (r.has_sampling_window ? r.sampling_frame_offset : 0);
  int count = r.has_sampling_window ? r.sampling_frame_count : r.frame_count;
  RayTracingFrameRecoveryScan scan;
  return ray_tracing_frame_recovery_scan(r.output_root, start, count, r.width,
                                         r.height, &scan) &&
         scan.durable_prefix_count == count && scan.first_invalid_frame < 0 &&
         scan.first_noncontiguous_frame < 0 &&
         ray_tracing_sequence_recovery_validate(&r, start, count, d, n);
}
static double seconds(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec / 1e9;
}
int ray_tracing_job_runner_wait(FILE *out, const char *argv0, const char *id,
                                const char *override, int timeout, char *d,
                                size_t n) {
  char root[PATH_MAX];
  RayTracingDetachedJobPaths paths;
  if (!out || timeout < 1 || timeout > 86400 ||
      !ray_tracing_job_runner_build_jobs_root(argv0, override, root,
                                              sizeof(root)) ||
      !ray_tracing_job_runner_build_job_paths(root, id, &paths))
    return 2;
  double deadline = seconds() + timeout, unverified_since = 0;
  for (;;) {
    RayTracingDetachedJobRecord record;
    if (!ray_tracing_job_runner_load_job_status_record(&paths, &record) ||
        !ray_tracing_job_runner_refresh_job_status_record(&paths, &record))
      return 1;
    if (!strcmp(record.state, "completed")) {
      if (completion_valid(&paths, &record, d, n)) {
        record.completion_verified = true;
        return ray_tracing_job_runner_write_status_to_stream(out, &record) ? 0
                                                                           : 1;
      }
      if (!unverified_since)
        unverified_since = seconds();
      if (seconds() - unverified_since >= 1) {
        ray_tracing_job_runner_write_status_to_stream(out, &record);
        RayTracingRequestSetDiag(
            d, n,
            "reported completion lacks valid native completion event, "
            "request/result binding, or durable frames/receipts");
        return 1;
      }
    } else if (!strcmp(record.state, "failed") ||
               !strcmp(record.state, "cancelled") ||
               !strcmp(record.state, "interrupted")) {
      RayTracingWorkerEvent terminal = {0};
      bool cancel_pending =
          !strcmp(record.state, "cancelled") &&
          !strcmp(record.execution_mode, "worker_protocol") &&
          (!latest_event(&paths, record.job_id, &terminal) ||
           (terminal.type != RAY_TRACING_WORKER_MESSAGE_INTERRUPTION &&
            terminal.type != RAY_TRACING_WORKER_MESSAGE_COMPLETION));
      if (!cancel_pending) {
        ray_tracing_job_runner_write_status_to_stream(out, &record);
        RayTracingRequestSetDiag(
            d, n,
            "native job ended without verified completion; inspect this same "
            "job before recovery");
        return 1;
      }
    }
    if (seconds() >= deadline) {
      ray_tracing_job_runner_write_status_to_stream(out, &record);
      RayTracingRequestSetDiag(d, n,
                               "wait timed out; native job may still run; "
                               "resume waiting on this same job ID");
      return 124;
    }
    struct timespec pause = {0, 200000000};
    nanosleep(&pause, NULL);
  }
}
