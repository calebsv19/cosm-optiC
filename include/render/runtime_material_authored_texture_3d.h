#ifndef RENDER_RUNTIME_MATERIAL_AUTHORED_TEXTURE_3D_H
#define RENDER_RUNTIME_MATERIAL_AUTHORED_TEXTURE_3D_H

#include <stdbool.h>
#include <stddef.h>

#include "scene/object_manager.h"

#define RUNTIME_MATERIAL_AUTHORED_TEXTURE_MAX_FACES 6
#define RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY 512
#define RUNTIME_MATERIAL_AUTHORED_TEXTURE_MODE_CAPACITY 24
#define RUNTIME_MATERIAL_AUTHORED_TEXTURE_FACE_CORNER_COUNT 4
#define RUNTIME_MATERIAL_AUTHORED_TEXTURE_FACE_EDGE_COUNT 4

typedef struct RuntimeMaterialAuthoredTextureSample {
    bool active;
    int sceneObjectIndex;
    int faceGroupIndex;
    double u;
    double v;
    double colorR;
    double colorG;
    double colorB;
    double alpha;
} RuntimeMaterialAuthoredTextureSample;

typedef struct RuntimeMaterialAuthoredTextureFaceMetadata {
    bool active;
    int sceneObjectIndex;
    int faceGroupIndex;
    char netLayoutKind[24];
    char netSlot[24];
    char orientation[16];
    unsigned char cornerIds[RUNTIME_MATERIAL_AUTHORED_TEXTURE_FACE_CORNER_COUNT];
    unsigned char edgeIds[RUNTIME_MATERIAL_AUTHORED_TEXTURE_FACE_EDGE_COUNT];
    int adjacentFaceGroupIndices[RUNTIME_MATERIAL_AUTHORED_TEXTURE_FACE_EDGE_COUNT];
    double layoutOffsetX;
    double layoutOffsetY;
} RuntimeMaterialAuthoredTextureFaceMetadata;

void RuntimeMaterialAuthoredTextureResetAll(void);

bool RuntimeMaterialAuthoredTextureBindManifestForObject(int scene_object_index,
                                                         const char* object_id,
                                                         const char* manifest_path,
                                                         const char* binding_mode);

bool RuntimeMaterialAuthoredTextureGetBinding(int scene_object_index,
                                              char* out_manifest_path,
                                              size_t out_manifest_path_size,
                                              char* out_binding_mode,
                                              size_t out_binding_mode_size,
                                              int* out_face_count);

bool RuntimeMaterialAuthoredTextureGetFaceMetadata(
    int scene_object_index,
    int face_group_index,
    RuntimeMaterialAuthoredTextureFaceMetadata* out_metadata);

bool RuntimeMaterialAuthoredTextureSampleFace(int scene_object_index,
                                              int face_group_index,
                                              double u,
                                              double v,
                                              RuntimeMaterialAuthoredTextureSample* out_sample);

#endif
