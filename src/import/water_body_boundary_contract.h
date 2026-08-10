#ifndef RAY_TRACING_WATER_BODY_BOUNDARY_CONTRACT_H
#define RAY_TRACING_WATER_BODY_BOUNDARY_CONTRACT_H

#include <stdbool.h>
#include <stddef.h>

#include "cJSON.h"
#include "import/water_surface_import.h"

bool WaterBodyBoundaryContract_Parse(
    const cJSON* manifest_root,
    RuntimeWaterBodyBoundaryV1* out_boundary,
    bool* out_closed_volume_boundary,
    bool* out_dynamic_volume_boundary,
    double* out_boundary_height_y,
    double* out_boundary_bottom_height_y,
    char* out_boundary_shell_object_id,
    size_t out_boundary_shell_object_id_size,
    char* out_diagnostics,
    size_t out_diagnostics_size);

#endif
