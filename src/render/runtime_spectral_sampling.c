#include "render/runtime_spectral_sampling.h"
#include <math.h>
#include <string.h>

RuntimeSpectralSamplingConfig RuntimeSpectralSampling_Defaults(void) {
  return (RuntimeSpectralSamplingConfig){.enabled = true,
                                         .min_passes = 16,
                                         .visible_error = 2.5,
                                         .edge_error = 3.5,
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
        strcmp(key, "edge_error") && strcmp(key, "energy_error"))
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
  return integer(value, "min_passes", &out->min_passes) &&
         out->min_passes >= 16 &&
         integer(value, "max_passes", &out->max_passes) &&
         (!out->max_passes || out->max_passes >= out->min_passes) &&
         error_limit(value, "visible_error", 255, &out->visible_error) &&
         error_limit(value, "edge_error", 255, &out->edge_error) &&
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
#define COUNT(name)                                                            \
  json_object_object_add(o, #name, json_object_new_int64(s->name))
  COUNT(passes);
  COUNT(full_passes);
  COUNT(checkpoints);
  COUNT(consecutive);
  COUNT(camera_samples);
  COUNT(edge_pixels);
#undef COUNT
#define ERROR(name)                                                            \
  json_object_object_add(                                                      \
      o, #name, s->checkpoints ? json_object_new_double(s->name) : NULL)
  ERROR(visible_error);
  ERROR(edge_error);
  ERROR(energy_error);
#undef ERROR
  return o;
}
