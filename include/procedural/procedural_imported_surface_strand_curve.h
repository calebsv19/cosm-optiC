#ifndef PROCEDURAL_IMPORTED_SURFACE_STRAND_CURVE_H
#define PROCEDURAL_IMPORTED_SURFACE_STRAND_CURVE_H

#include <stdbool.h>

#include "procedural/procedural_imported_surface_strands.h"
#include "render/runtime_curve_primitive_3d.h"

bool ProceduralImportedSurfaceStrands_BuildCurveAsset(
    const ProceduralImportedSurfaceStrandAsset *strands,
    RuntimeCurveAsset3D *out_curve_asset);

#endif
