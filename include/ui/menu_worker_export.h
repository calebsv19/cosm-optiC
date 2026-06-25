#ifndef RAY_TRACING_MENU_WORKER_EXPORT_H
#define RAY_TRACING_MENU_WORKER_EXPORT_H

#include <stdbool.h>
#include <stddef.h>

typedef struct RayTracingMenuWorkerExportStatus {
    bool ok;
    char message[96];
    char item_name[128];
    char output_root[512];
} RayTracingMenuWorkerExportStatus;

bool ray_tracing_menu_worker_export_scene_only(RayTracingMenuWorkerExportStatus *status);

#endif
