#ifndef RENDER_RUNTIME_MATERIAL_AUTHORED_TEXTURE_3D_H
#define RENDER_RUNTIME_MATERIAL_AUTHORED_TEXTURE_3D_H

#include <stdbool.h>
#include <stddef.h>

#include "scene/object_manager.h"

#define RUNTIME_MATERIAL_AUTHORED_TEXTURE_MAX_FACES 6
#define RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY 512
#define RUNTIME_MATERIAL_AUTHORED_TEXTURE_MODE_CAPACITY 24
#define RUNTIME_MATERIAL_AUTHORED_TEXTURE_INTENT_CAPACITY 32
#define RUNTIME_MATERIAL_AUTHORED_TEXTURE_REASON_CAPACITY 128
#define RUNTIME_MATERIAL_AUTHORED_TEXTURE_CHANNEL_CAPACITY 48
#define RUNTIME_MATERIAL_AUTHORED_TEXTURE_CHANNEL_SOURCE_CAPACITY 24
#define RUNTIME_MATERIAL_AUTHORED_TEXTURE_CHANNEL_FILE_CAPACITY 128
#define RUNTIME_MATERIAL_AUTHORED_TEXTURE_MAX_CHANNEL_REFS 8
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

typedef struct RuntimeMaterialAuthoredTextureChannelRef {
    bool active;
    char channel[RUNTIME_MATERIAL_AUTHORED_TEXTURE_CHANNEL_CAPACITY];
    char source[RUNTIME_MATERIAL_AUTHORED_TEXTURE_CHANNEL_SOURCE_CAPACITY];
    char fileName[RUNTIME_MATERIAL_AUTHORED_TEXTURE_CHANNEL_FILE_CAPACITY];
} RuntimeMaterialAuthoredTextureChannelRef;

typedef struct RuntimeMaterialAuthoredTextureFaceMetadata {
    bool active;
    int sceneObjectIndex;
    int faceGroupIndex;
    char netLayoutKind[24];
    char netSlot[24];
    char orientation[16];
    char baseMaterialIntentKind[RUNTIME_MATERIAL_AUTHORED_TEXTURE_INTENT_CAPACITY];
    char overlayMaterialIntentKind[RUNTIME_MATERIAL_AUTHORED_TEXTURE_INTENT_CAPACITY];
    unsigned char cornerIds[RUNTIME_MATERIAL_AUTHORED_TEXTURE_FACE_CORNER_COUNT];
    unsigned char edgeIds[RUNTIME_MATERIAL_AUTHORED_TEXTURE_FACE_EDGE_COUNT];
    int adjacentFaceGroupIndices[RUNTIME_MATERIAL_AUTHORED_TEXTURE_FACE_EDGE_COUNT];
    double layoutOffsetX;
    double layoutOffsetY;
    int channelRefCount;
    RuntimeMaterialAuthoredTextureChannelRef
        channelRefs[RUNTIME_MATERIAL_AUTHORED_TEXTURE_MAX_CHANNEL_REFS];
} RuntimeMaterialAuthoredTextureFaceMetadata;

void RuntimeMaterialAuthoredTextureResetAll(void);
bool RuntimeMaterialAuthoredTextureClearBindingForObject(int scene_object_index);

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

bool RuntimeMaterialAuthoredTextureGetInvalidBinding(int scene_object_index,
                                                     char* out_manifest_path,
                                                     size_t out_manifest_path_size,
                                                     char* out_binding_mode,
                                                     size_t out_binding_mode_size,
                                                     char* out_reason,
                                                     size_t out_reason_size);

bool RuntimeMaterialAuthoredTextureGetFaceMetadata(
    int scene_object_index,
    int face_group_index,
    RuntimeMaterialAuthoredTextureFaceMetadata* out_metadata);

bool RuntimeMaterialAuthoredTextureGetFaceChannels(
    int scene_object_index,
    int face_group_index,
    RuntimeMaterialAuthoredTextureChannelRef* out_channels,
    size_t max_channels,
    int* out_channel_count);

bool RuntimeMaterialAuthoredTextureGetChannelSummary(int scene_object_index,
                                                     char* out_summary,
                                                     size_t out_summary_size);

bool RuntimeMaterialAuthoredTextureChannelNameSupported(const char* channel);
bool RuntimeMaterialAuthoredTextureChannelIsVisual(const char* channel);
bool RuntimeMaterialAuthoredTextureChannelIsPhysicalScalar(const char* channel);
bool RuntimeMaterialAuthoredTextureChannelIsShadingNormal(const char* channel);
bool RuntimeMaterialAuthoredTextureChannelIsDisplacement(const char* channel);

bool RuntimeMaterialAuthoredTextureGetOverlayMaterialIntent(int scene_object_index,
                                                            char* out_overlay_material_intent,
                                                            size_t out_overlay_material_intent_size);

bool RuntimeMaterialAuthoredTextureSampleFace(int scene_object_index,
                                              int face_group_index,
                                              double u,
                                              double v,
                                              RuntimeMaterialAuthoredTextureSample* out_sample);

bool RuntimeMaterialAuthoredTextureSampleOverlayFace(int scene_object_index,
                                                     int face_group_index,
                                                     double u,
                                                     double v,
                                                     RuntimeMaterialAuthoredTextureSample* out_sample);

#endif
