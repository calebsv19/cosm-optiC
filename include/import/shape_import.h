#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ShapeLib/shape_core.h"

typedef struct {
    float min_x;
    float min_y;
    float max_x;
    float max_y;
    bool  valid;
} ShapeBounds;

typedef struct {
    float margin_cells;   // padding (cells) around fitted shape when center_fit=true
    float stroke;         // line thickness in cells for open paths
    float max_error;      // flatten tolerance
    float position_x_norm; // 0..1 target center (used when center_fit=false)
    float position_y_norm; // 0..1 target center (used when center_fit=false)
    float rotation_deg;    // rotation around shape bounds center
    float scale;           // uniform scale (used when center_fit=false; <=0 => 1)
    bool  center_fit;      // if true, auto-fit to grid with margin
} ShapeRasterOptions;

bool shape_import_load(const char *path, ShapeDocument *out_doc);
bool shape_import_bounds(const Shape *shape, ShapeBounds *out_bounds);
bool shape_import_rasterize(const Shape *shape,
                            int grid_w,
                            int grid_h,
                            const ShapeRasterOptions *opts,
                            uint8_t *mask_out);
