#ifndef RAY_TRACING_SHA256_H
#define RAY_TRACING_SHA256_H

#include <stdbool.h>
#include <stddef.h>

#define RAY_TRACING_SHA256_HEX_SIZE 65

bool ray_tracing_sha256_bytes(const void *data,
                              size_t size,
                              char out_hex[RAY_TRACING_SHA256_HEX_SIZE]);
bool ray_tracing_sha256_file(const char *path,
                             char out_hex[RAY_TRACING_SHA256_HEX_SIZE]);
bool ray_tracing_sha256_is_valid_hex(const char *hex);

#endif
