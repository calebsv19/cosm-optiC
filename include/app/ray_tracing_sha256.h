#ifndef RAY_TRACING_SHA256_H
#define RAY_TRACING_SHA256_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RAY_TRACING_SHA256_HEX_SIZE 65

typedef struct RayTracingSha256Context {
    uint32_t state[8];
    uint64_t bit_count;
    unsigned char block[64];
    size_t block_size;
} RayTracingSha256Context;
void ray_tracing_sha256_init(RayTracingSha256Context *context);
void ray_tracing_sha256_update(RayTracingSha256Context *context, const void *bytes, size_t size);
void ray_tracing_sha256_finish_hex(RayTracingSha256Context *context, char out_hex[RAY_TRACING_SHA256_HEX_SIZE]);

bool ray_tracing_sha256_bytes(const void *data,
                              size_t size,
                              char out_hex[RAY_TRACING_SHA256_HEX_SIZE]);
bool ray_tracing_sha256_file(const char *path,
                             char out_hex[RAY_TRACING_SHA256_HEX_SIZE]);
bool ray_tracing_sha256_is_valid_hex(const char *hex);

#endif
