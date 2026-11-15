#include "render/runtime_spectral_sampling.h"
#include <math.h>
#include <string.h>

RuntimeSpectralSamplingConfig RuntimeSpectralSampling_Defaults(void) {
  return (RuntimeSpectralSamplingConfig){.enabled = true,
                                         .min_passes = 16,
                                         .camera_samples_per_map = 1,
                                         .visible_error = 2.5,
                                         .edge_error = 3.5,
                                         .local_error = 2.5,
                                         .energy_error = .01};
}
static bool integer(json_object *obj, const char *key, int *out) {
  json_object *value;
  if (!json_object_object_get_ex(obj, key, &value))
    return true;
  if (!json_object_is_type(value, json_type_int))
    return false;
  int64_t n = json_object_get_int64(value);
  if (n < 0 || n > 1048576 || n % 8)
    return false;
  *out = (int)n;
  return true;
}
static bool error_limit(json_object *obj, const char *key, double maximum,
                        double *out) {
  json_object *value;
  if (!json_object_object_get_ex(obj, key, &value))
    return true;
  if (!json_object_is_type(value, json_type_double) &&
      !json_object_is_type(value, json_type_int))
    return false;
  double n = json_object_get_double(value);
  if (!isfinite(n) || n <= 0 || n > maximum)
    return false;
  *out = n;
  return true;
}
bool RuntimeSpectralSampling_Parse(json_object *value,
                                   RuntimeSpectralSamplingConfig *out) {
  *out = (RuntimeSpectralSamplingConfig){0};
  if (!value)
    return true;
  if (!json_object_is_type(value, json_type_object))
    return false;
  json_object_object_foreach(value, key, unused) {
    (void)unused;
    if (strcmp(key, "mode") && strcmp(key, "min_passes") &&
        strcmp(key, "max_passes") && strcmp(key, "visible_error") &&
        strcmp(key, "edge_error") && strcmp(key, "energy_error") &&
        strcmp(key, "local_error") && strcmp(key, "camera_coverage") &&
        strcmp(key, "camera_sequence") && strcmp(key, "camera_seed") &&
        strcmp(key, "camera_samples_per_map"))
      return false;
  }
  json_object *mode = json_object_object_get(value, "mode");
  if (!json_object_is_type(mode, json_type_string))
    return false;
  const char *name = json_object_get_string(mode);
  if (strlen(name) != (size_t)json_object_get_string_len(mode))
    return false;
  if (!strcmp(name, "single"))
    return json_object_object_length(value) == 1;
  if (strcmp(name, "converge"))
    return false;
  *out = RuntimeSpectralSampling_Defaults();
  json_object *coverage;
  if (json_object_object_get_ex(value, "camera_coverage", &coverage)) {
    if (!json_object_is_type(coverage, json_type_string))
      return false;
    const char *s = json_object_get_string(coverage);
    if (strlen(s) != (size_t)json_object_get_string_len(coverage))
      return false;
    out->full_camera = !strcmp(s, "full");
    if (!out->full_camera && strcmp(s, "adaptive"))
      return false;
  }
  json_object *sequence;
  if (json_object_object_get_ex(value, "camera_sequence", &sequence)) {
    if (!json_object_is_type(sequence, json_type_string))
      return false;
    const char *s = json_object_get_string(sequence);
    if (strlen(s) != (size_t)json_object_get_string_len(sequence))
      return false;
    out->progressive_camera = !strcmp(s, "progressive");
    if (!out->progressive_camera && strcmp(s, "balanced"))
      return false;
  }
  json_object *bundle;
  if (json_object_object_get_ex(value, "camera_samples_per_map", &bundle)) {
    if (!json_object_is_type(bundle, json_type_int))
      return false;
    int64_t n = json_object_get_int64(bundle);
    if (n != 1 && n != 2 && n != 4 && n != 8)
      return false;
    out->camera_samples_per_map = (unsigned)n;
  }
  if (out->camera_samples_per_map > 1 && !out->progressive_camera)
    return false;
  json_object *seed;
  if (json_object_object_get_ex(value, "camera_seed", &seed)) {
    if (!json_object_is_type(seed, json_type_int))
      return false;
    int64_t n = json_object_get_int64(seed);
    if (n < 0 || n > UINT32_MAX)
      return false;
    out->camera_seed = (uint32_t)n;
    out->camera_seed_set = true;
  }
  return integer(value, "min_passes", &out->min_passes) &&
         out->min_passes >= 16 &&
         integer(value, "max_passes", &out->max_passes) &&
         (!out->max_passes || out->max_passes >= out->min_passes) &&
         error_limit(value, "visible_error", 255, &out->visible_error) &&
         error_limit(value, "edge_error", 255, &out->edge_error) &&
         error_limit(value, "local_error", 255, &out->local_error) &&
         error_limit(value, "energy_error", 1, &out->energy_error);
}
static double uniform(uint64_t n) {
  n = (n ^ (n >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  n = (n ^ (n >> 27)) * UINT64_C(0x94d049bb133111eb);
  return ((n ^ (n >> 31)) >> 11) * 0x1p-53;
}
void RuntimeSpectralSampling_Jitter(unsigned seed, int pass, double *x,
                                    double *y) {
  /* Both independent groups contain each quadrant once in every eight-pass
   * checkpoint. Group identity never selects an inner/outer pixel region. */
  unsigned quadrant = (unsigned)pass % 4;
  uint64_t id = ((uint64_t)seed << 32) ^
                ((uint64_t)(unsigned)pass * UINT64_C(0x9e3779b97f4a7c15));
  *x = ((quadrant & 1) + uniform(id)) * .5 - .5;
  *y = ((quadrant >> 1) + uniform(id + UINT64_C(0xd1b54a32d192ed03))) * .5 - .5;
}
RuntimeNative3DSamplingContext RuntimeSpectralSampling_Camera(uint32_t seed,
                                                              int pass) {
  return RuntimeSpectralSampling_CameraForMap(seed, pass, 0, 1);
}
RuntimeNative3DSamplingContext RuntimeSpectralSampling_CameraForMap(
    uint32_t seed, int pass, unsigned sample, unsigned samples_per_map) {
  /* Extend each comparison group's own sequence. A bundle never crosses
   * groups and never reuses a ray within that group's sequence. */
  uint64_t map_index = ((uint32_t)pass / 8) * UINT64_C(4) + (uint32_t)pass % 4;
  uint64_t index = map_index * samples_per_map + sample;
  uint32_t group = ((uint32_t)pass % 8) / 4;
  /* The native context has a 16-bit index. Start a distinct scrambled epoch
   * after 65536 samples per group instead of repeating the same camera rays. */
  return (RuntimeNative3DSamplingContext){
      .temporalSubpassIndex = (uint16_t)index,
      .temporalSubpassCount = 2,
      .cameraPixelSampler = RUNTIME_CAMERA_PIXEL_PROGRESSIVE,
      .cameraSequenceSeed =
          seed ^ (group ? UINT32_C(0xa511e9b3) : UINT32_C(0x63d83595)) ^
          ((uint32_t)(index >> 16) * UINT32_C(0x9e3779b9))};
}
json_object *
RuntimeSpectralSampling_JSON(const RuntimeSpectralSamplingStats *s) {
  json_object *o = json_object_new_object();
  json_object_object_add(o, "enabled", json_object_new_boolean(s->enabled));
  const char *status = s->canceled     ? "canceled"
                       : s->converged  ? "converged"
                       : s->pass_limit ? "pass_limit"
                       : s->enabled    ? "sampling"
                                       : "single";
  json_object_object_add(o, "status", json_object_new_string(status));
  json_object_object_add(
      o, "camera_coverage",
      json_object_new_string(s->full_camera ? "full" : "adaptive"));
  json_object_object_add(o, "camera_sequence",
                         json_object_new_string(s->progressive_camera
                                                    ? "progressive"
                                                    : "balanced"));
  json_object_object_add(o, "camera_seed",
                         json_object_new_int64(s->camera_seed));
  json_object_object_add(o, "validation_pending",
                         json_object_new_boolean(s->validation_pending));
#define COUNT(name)                                                            \
  json_object_object_add(o, #name, json_object_new_int64(s->name))
  COUNT(camera_samples_per_map);
  COUNT(min_pixel_maps);
  COUNT(max_pixel_maps);
  COUNT(passes);
  COUNT(full_passes);
  COUNT(checkpoints);
  COUNT(full_checkpoints);
  COUNT(min_pixel_samples);
  COUNT(max_pixel_samples);
  COUNT(consecutive);
  COUNT(camera_samples);
  COUNT(edge_pixels);
#undef COUNT
#define ERROR(name)                                                            \
  json_object_object_add(                                                      \
      o, #name, s->checkpoints ? json_object_new_double(s->name) : NULL)
  ERROR(visible_error);
  ERROR(edge_error);
  ERROR(local_error);
  ERROR(energy_error);
#undef ERROR
  return o;
}
