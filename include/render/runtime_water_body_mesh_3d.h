#ifndef RENDER_RUNTIME_WATER_BODY_MESH_3D_H
#define RENDER_RUNTIME_WATER_BODY_MESH_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_scene_3d.h"

#define RUNTIME_WATER_BODY_IDENTITY_MAX 64

typedef struct RuntimeWaterBodyMesh3DDesc {
    const char* object_id;
    const char* material_id;
    const char* medium_id;
    int scene_object_index;
    uint32_t grid_w;
    uint32_t grid_d;
    const float* heights_y;
    double sample_origin_x;
    double sample_origin_z;
    double sample_spacing_x;
    double sample_spacing_z;
    double bottom_height;
    bool map_y_height_to_scene_z;
} RuntimeWaterBodyMesh3DDesc;

typedef struct RuntimeWaterBodyMesh3DReport {
    char object_id[RUNTIME_WATER_BODY_IDENTITY_MAX];
    char material_id[RUNTIME_WATER_BODY_IDENTITY_MAX];
    char medium_id[RUNTIME_WATER_BODY_IDENTITY_MAX];
    int scene_object_index;
    int object_count;
    int connected_component_count;
    int boundary_edge_count;
    int nonmanifold_edge_count;
    int top_triangle_count;
    int side_triangle_count;
    int bottom_triangle_count;
    int total_triangle_count;
    double signed_volume;
    double max_perimeter_seam_error;
    bool winding_consistent;
    bool topology_valid;
    uint64_t topology_signature;
} RuntimeWaterBodyMesh3DReport;

bool RuntimeWaterBodyMesh3D_Append(RuntimeScene3D* scene,
                                   const RuntimeWaterBodyMesh3DDesc* desc,
                                   RuntimeWaterBodyMesh3DReport* out_report);

#endif
