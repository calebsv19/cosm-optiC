#ifndef RUNTIME_HEIGHTFIELD_NORMALS_3D_H
#define RUNTIME_HEIGHTFIELD_NORMALS_3D_H
#include "render/runtime_scene_3d.h"
/* Derivatives use the actual prepared grid spacing, including boundary insets.
 * Top quads share vertex normals; wall and bottom creases remain geometric. */
bool RuntimeHeightfieldNormals3D_ApplyTopQuads(RuntimeTriangle3D* triangles,
    const float* heights, unsigned width, unsigned depth,
    double spacing_x, double spacing_z, bool map_y_height_to_scene_z);
#endif
