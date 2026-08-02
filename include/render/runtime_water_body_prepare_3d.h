#ifndef RENDER_RUNTIME_WATER_BODY_PREPARE_3D_H
#define RENDER_RUNTIME_WATER_BODY_PREPARE_3D_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "import/water_surface_import.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_water_body_mesh_3d.h"

typedef struct RuntimeWaterBodyPrepare3DReport {
    bool active;
    char closure_mode[32];
    char body_id[RUNTIME_WATER_BODY_ID_MAX];
    char container_id[RUNTIME_WATER_BODY_ID_MAX];
    char material_id[RUNTIME_WATER_BODY_ID_MAX];
    char medium_id[RUNTIME_WATER_BODY_ID_MAX];
    uint64_t selected_frame_index;
    uint32_t wet_sample_count;
    uint32_t dry_container_sample_count;
    uint32_t solid_occluder_sample_count;
    bool legacy_shell_suppressed;
    bool material_parity_valid;
    double min_x;
    double max_x;
    double min_y;
    double max_y;
    double min_z;
    double max_z;
    double bottom_height_m;
    double base_surface_height_m;
    RuntimeWaterBodyMesh3DReport geometry;
} RuntimeWaterBodyPrepare3DReport;

bool RuntimeWaterBodyPrepare3D_Append(RuntimeScene3D* scene,
                                      const RuntimeWaterSurfaceFrame* water,
                                      int scene_object_index,
                                      RuntimeWaterBodyPrepare3DReport* out_report,
                                      char* out_diagnostics,
                                      size_t out_diagnostics_size);

void RuntimeWaterBodyPrepare3D_ResetLastReport(void);
bool RuntimeWaterBodyPrepare3D_GetLastReport(RuntimeWaterBodyPrepare3DReport* out_report);

#endif
