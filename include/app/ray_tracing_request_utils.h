#ifndef RAY_TRACING_REQUEST_UTILS_H
#define RAY_TRACING_REQUEST_UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include <json-c/json.h>

void RayTracingRequestSetDiag(char *out, size_t out_size, const char *message);
bool RayTracingCopyString(char *dst, size_t dst_size, const char *src);
bool RayTracingReadTextFile(const char *path, char **out_text);
void RayTracingDirnameOf(const char *path, char *out_dir, size_t out_dir_size);
bool RayTracingPathIsAbsolute(const char *path);
bool RayTracingResolveRequestPath(const char *request_dir,
                                  const char *path,
                                  char *out_path,
                                  size_t out_path_size);
bool RayTracingNormalizeResolvedPath(const char *path, char *out_path, size_t out_path_size);
bool RayTracingResolveExistingRequestPath(const char *request_dir,
                                          const char *path,
                                          char *out_path,
                                          size_t out_path_size);
bool RayTracingResolveRequestInputPath(const char *request_dir,
                                       const char *path,
                                       char *out_path,
                                       size_t out_path_size);
bool RayTracingResolveRequestOutputPath(const char *request_dir,
                                        const char *path,
                                        char *out_path,
                                        size_t out_path_size);
bool RayTracingEnvGetInt(const char *name, int *out_value);

bool RayTracingJsonGetObject(json_object *owner, const char *key, json_object **out_obj);
bool RayTracingJsonGetString(json_object *owner, const char *key, const char **out_value);
bool RayTracingJsonGetInt(json_object *owner, const char *key, int *out_value);
bool RayTracingJsonGetDouble(json_object *owner, const char *key, double *out_value);
bool RayTracingJsonGetBool(json_object *owner, const char *key, bool *out_value);
bool RayTracingJsonGetDoubleAny(json_object *owner,
                                const char *key_a,
                                const char *key_b,
                                double *out_value);
bool RayTracingJsonGetIntAny(json_object *owner,
                             const char *key_a,
                             const char *key_b,
                             int *out_value);
void RayTracingJsonWriteString(FILE *file, const char *value);
double RayTracingProgressRatioCompleted(int frames_completed,
                                        int frame_count,
                                        int temporal_subpasses_completed,
                                        int temporal_subpasses_total);
double RayTracingProgressRatioActive(int frames_completed,
                                     int frame_count,
                                     int temporal_subpasses_started,
                                     int temporal_subpasses_completed,
                                     int temporal_subpasses_total,
                                     size_t completed_tiles_in_subpass,
                                     size_t total_tiles_in_subpass);

#endif
