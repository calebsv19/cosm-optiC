#include "app/ray_tracing_sha256.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct RayTracingSha256Context {
    uint32_t state[8];
    uint64_t bit_count;
    unsigned char block[64];
    size_t block_size;
} RayTracingSha256Context;

static const uint32_t k_sha256[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

static uint32_t rotate_right(uint32_t value, unsigned shift) {
    return (value >> shift) | (value << (32u - shift));
}

static void sha256_transform(RayTracingSha256Context *context,
                             const unsigned char block[64]) {
    uint32_t words[64];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
    uint32_t h;
    for (size_t i = 0; i < 16u; ++i) {
        const size_t offset = i * 4u;
        words[i] = ((uint32_t)block[offset] << 24u) |
                   ((uint32_t)block[offset + 1u] << 16u) |
                   ((uint32_t)block[offset + 2u] << 8u) |
                   (uint32_t)block[offset + 3u];
    }
    for (size_t i = 16u; i < 64u; ++i) {
        const uint32_t s0 = rotate_right(words[i - 15u], 7u) ^
                            rotate_right(words[i - 15u], 18u) ^
                            (words[i - 15u] >> 3u);
        const uint32_t s1 = rotate_right(words[i - 2u], 17u) ^
                            rotate_right(words[i - 2u], 19u) ^
                            (words[i - 2u] >> 10u);
        words[i] = words[i - 16u] + s0 + words[i - 7u] + s1;
    }
    a = context->state[0];
    b = context->state[1];
    c = context->state[2];
    d = context->state[3];
    e = context->state[4];
    f = context->state[5];
    g = context->state[6];
    h = context->state[7];
    for (size_t i = 0; i < 64u; ++i) {
        const uint32_t s1 = rotate_right(e, 6u) ^ rotate_right(e, 11u) ^
                            rotate_right(e, 25u);
        const uint32_t choose = (e & f) ^ ((~e) & g);
        const uint32_t temp1 = h + s1 + choose + k_sha256[i] + words[i];
        const uint32_t s0 = rotate_right(a, 2u) ^ rotate_right(a, 13u) ^
                            rotate_right(a, 22u);
        const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t temp2 = s0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    context->state[0] += a;
    context->state[1] += b;
    context->state[2] += c;
    context->state[3] += d;
    context->state[4] += e;
    context->state[5] += f;
    context->state[6] += g;
    context->state[7] += h;
}

static void sha256_init(RayTracingSha256Context *context) {
    static const uint32_t initial[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
    };
    memset(context, 0, sizeof(*context));
    memcpy(context->state, initial, sizeof(initial));
}

static void sha256_update(RayTracingSha256Context *context,
                          const unsigned char *data,
                          size_t size) {
    if (!context || (!data && size > 0u)) return;
    context->bit_count += (uint64_t)size * 8u;
    while (size > 0u) {
        const size_t available = sizeof(context->block) - context->block_size;
        const size_t copy_size = size < available ? size : available;
        memcpy(context->block + context->block_size, data, copy_size);
        context->block_size += copy_size;
        data += copy_size;
        size -= copy_size;
        if (context->block_size == sizeof(context->block)) {
            sha256_transform(context, context->block);
            context->block_size = 0u;
        }
    }
}

static void sha256_finish(RayTracingSha256Context *context,
                          unsigned char digest[32]) {
    const uint64_t bit_count = context->bit_count;
    context->block[context->block_size++] = 0x80u;
    if (context->block_size > 56u) {
        while (context->block_size < 64u) context->block[context->block_size++] = 0u;
        sha256_transform(context, context->block);
        context->block_size = 0u;
    }
    while (context->block_size < 56u) context->block[context->block_size++] = 0u;
    for (size_t i = 0; i < 8u; ++i) {
        context->block[63u - i] = (unsigned char)(bit_count >> (i * 8u));
    }
    sha256_transform(context, context->block);
    for (size_t i = 0; i < 8u; ++i) {
        digest[i * 4u] = (unsigned char)(context->state[i] >> 24u);
        digest[i * 4u + 1u] = (unsigned char)(context->state[i] >> 16u);
        digest[i * 4u + 2u] = (unsigned char)(context->state[i] >> 8u);
        digest[i * 4u + 3u] = (unsigned char)context->state[i];
    }
}

static void digest_to_hex(const unsigned char digest[32],
                          char out_hex[RAY_TRACING_SHA256_HEX_SIZE]) {
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < 32u; ++i) {
        out_hex[i * 2u] = digits[digest[i] >> 4u];
        out_hex[i * 2u + 1u] = digits[digest[i] & 0x0fu];
    }
    out_hex[64] = '\0';
}

bool ray_tracing_sha256_bytes(const void *data,
                              size_t size,
                              char out_hex[RAY_TRACING_SHA256_HEX_SIZE]) {
    RayTracingSha256Context context;
    unsigned char digest[32];
    if ((!data && size > 0u) || !out_hex) return false;
    sha256_init(&context);
    sha256_update(&context, (const unsigned char *)data, size);
    sha256_finish(&context, digest);
    digest_to_hex(digest, out_hex);
    return true;
}

bool ray_tracing_sha256_file(const char *path,
                             char out_hex[RAY_TRACING_SHA256_HEX_SIZE]) {
    RayTracingSha256Context context;
    unsigned char buffer[32768];
    unsigned char digest[32];
    FILE *file = NULL;
    size_t read_size = 0u;
    if (!path || !path[0] || !out_hex) return false;
    file = fopen(path, "rb");
    if (!file) return false;
    sha256_init(&context);
    while ((read_size = fread(buffer, 1u, sizeof(buffer), file)) > 0u) {
        sha256_update(&context, buffer, read_size);
    }
    if (ferror(file) || fclose(file) != 0) return false;
    sha256_finish(&context, digest);
    digest_to_hex(digest, out_hex);
    return true;
}

bool ray_tracing_sha256_is_valid_hex(const char *hex) {
    if (!hex || strlen(hex) != 64u) return false;
    for (size_t i = 0; i < 64u; ++i) {
        const char c = hex[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}
