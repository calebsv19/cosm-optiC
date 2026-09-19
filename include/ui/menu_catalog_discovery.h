#ifndef MENU_CATALOG_DISCOVERY_H
#define MENU_CATALOG_DISCOVERY_H
#include <stdbool.h>
#include <stdint.h>
#include "ui/scene_source_catalog.h"
#include "app/render_export_batch.h"
#include "ui/volume_source_catalog.h"

/* Main-thread request/readback; filesystem work owns copied inputs and never UI state. */
void menu_catalog_discovery_request(const char *const *roots, size_t root_count,
                                    const char *manifest, const char *runtime,
                                    const char *volume, const char *frame_dir, const char *video_path);
bool menu_catalog_discovery_read(uint64_t *revision,
                                 SceneSourceCatalogEntry *scenes, size_t *scene_count,
                                 VolumeSourceCatalogEntry *volumes, size_t *volume_count,
                                 size_t capacity, RayTracingRenderExportStatus *frames, bool *frames_ready);
#endif
