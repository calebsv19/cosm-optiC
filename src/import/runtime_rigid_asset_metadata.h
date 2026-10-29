#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Data-only metadata from an already authenticated collision artifact.
 * No mesh conversion, collision queries, mass calculation or solver link. */
bool RuntimeRigidCompoundMetadata(const char *bytes, size_t size,
                                  uint32_t *children, char source_id[64],
                                  char source_sha256[65], double center[3],
                                  double basis[9], double moments[3]);
