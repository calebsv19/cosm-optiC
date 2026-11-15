#pragma once
#include "runtime_rigid_result_internal.h"
typedef struct RuntimeRigidBundleFile {
  char path[128], hash[65];
  char *bytes;
  size_t size;
} RuntimeRigidBundleFile;
bool RuntimeRigidBundleV2Path(const char *path);
cJSON *RuntimeRigidBundleV2Catalog(RuntimeRigidBundleFile *files, size_t count,
                                   const char *engine,
                                   RuntimeRigidResult *result);
