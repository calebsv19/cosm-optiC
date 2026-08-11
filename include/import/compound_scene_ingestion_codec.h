#pragma once

#include "import/compound_scene_ingestion.h"

#include <limits.h>

typedef struct RayCompoundSceneIngestionFile {
    RayCompoundSceneHandoff handoff;
    RayCompoundSceneStaticRoom room;
    RayCompoundSceneIngestionDescriptor descriptor;
    char handoff_path[PATH_MAX];
    char room_path[PATH_MAX];
} RayCompoundSceneIngestionFile;

void ray_compound_scene_ingestion_file_init(RayCompoundSceneIngestionFile* file);
void ray_compound_scene_ingestion_file_free(RayCompoundSceneIngestionFile* file);
bool ray_compound_scene_ingestion_file_read(const char* path,
                                            RayCompoundSceneIngestionFile* output,
                                            char* diagnostics,
                                            size_t diagnostics_size);
