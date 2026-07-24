#ifndef RAY_TRACING_CHECKPOINT_TRANSACTION_H
#define RAY_TRACING_CHECKPOINT_TRANSACTION_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct RayTracingCheckpointTransaction {
    FILE* stream;
    char targetPath[PATH_MAX];
    char temporaryPath[PATH_MAX];
    uint64_t generation;
} RayTracingCheckpointTransaction;

bool ray_tracing_checkpoint_transaction_begin(
    RayTracingCheckpointTransaction* transaction,
    const char* target_path,
    uint64_t generation);
void ray_tracing_checkpoint_transaction_reached_temporary_write(
    RayTracingCheckpointTransaction* transaction);
bool ray_tracing_checkpoint_transaction_commit(
    RayTracingCheckpointTransaction* transaction);
void ray_tracing_checkpoint_transaction_abort(
    RayTracingCheckpointTransaction* transaction);

#endif
