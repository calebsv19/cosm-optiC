#ifndef RENDER_RUNTIME_MATERIAL_AUTHORED_TEXTURE_3D_INTERNAL_H
#define RENDER_RUNTIME_MATERIAL_AUTHORED_TEXTURE_3D_INTERNAL_H

#include <json-c/json.h>

#include "core_authored_texture.h"
#include "render/runtime_material_authored_texture_3d.h"

typedef struct RuntimeMaterialAuthoredTextureFace {
    bool active;
    int faceGroupIndex;
    int width;
    int height;
    unsigned char* rgba;
    RuntimeMaterialAuthoredTextureFaceMetadata metadata;
} RuntimeMaterialAuthoredTextureFace;

typedef struct RuntimeMaterialAuthoredTextureBinding {
    bool active;
    bool invalidActive;
    int sceneObjectIndex;
    CoreAuthoredTexturePrimitiveKind primitiveKind;
    int baseFaceCount;
    int overlayFaceCount;
    char objectId[64];
    char manifestPath[RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY];
    char resolvedManifestPath[RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY];
    char bindingMode[RUNTIME_MATERIAL_AUTHORED_TEXTURE_MODE_CAPACITY];
    char invalidManifestPath[RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY];
    char invalidBindingMode[RUNTIME_MATERIAL_AUTHORED_TEXTURE_MODE_CAPACITY];
    char invalidReason[RUNTIME_MATERIAL_AUTHORED_TEXTURE_REASON_CAPACITY];
    char overlayMaterialIntentKind[RUNTIME_MATERIAL_AUTHORED_TEXTURE_INTENT_CAPACITY];
    RuntimeMaterialAuthoredTextureFace baseFaces[RUNTIME_MATERIAL_AUTHORED_TEXTURE_MAX_FACES];
    RuntimeMaterialAuthoredTextureFace overlayFaces[RUNTIME_MATERIAL_AUTHORED_TEXTURE_MAX_FACES];
} RuntimeMaterialAuthoredTextureBinding;

extern RuntimeMaterialAuthoredTextureBinding s_authored_texture_bindings[MAX_OBJECTS];

void runtime_material_authored_texture_copy_text(char* dst, size_t dst_size, const char* src);
double runtime_material_authored_texture_clamp01(double value);
void runtime_material_authored_texture_face_metadata_reset(
    RuntimeMaterialAuthoredTextureFaceMetadata* metadata);
RuntimeMaterialAuthoredTextureBinding* runtime_material_authored_texture_binding_at(
    int scene_object_index);
void runtime_material_authored_texture_face_reset(RuntimeMaterialAuthoredTextureFace* face);
void runtime_material_authored_texture_binding_reset(
    RuntimeMaterialAuthoredTextureBinding* binding);
void runtime_material_authored_texture_binding_record_invalid(
    RuntimeMaterialAuthoredTextureBinding* binding,
    int scene_object_index,
    const char* manifest_path,
    const char* binding_mode,
    const char* reason);

#endif
