#ifndef SCENE_EDITOR_DOCUMENT_H
#define SCENE_EDITOR_DOCUMENT_H

#include <stdbool.h>
#include <stddef.h>

typedef struct SceneEditorDocumentTransform {
    double position[3];
    double rotation_degrees[3];
    double scale[3];
} SceneEditorDocumentTransform;

bool SceneEditorDocumentOpen(const char* path, char* diagnostics, size_t diagnostics_size);
bool SceneEditorDocumentOpenActive(char* diagnostics, size_t diagnostics_size);
void SceneEditorDocumentClose(void);
bool SceneEditorDocumentIsOpen(void);
bool SceneEditorDocumentIsDirty(void);
const char* SceneEditorDocumentUnitLabel(void);
double SceneEditorDocumentWorldScale(void);
const char* SceneEditorDocumentPath(void);
unsigned long long SceneEditorDocumentRevision(void);

bool SceneEditorDocumentGetTransformForSceneIndex(int scene_object_index,
                                                  SceneEditorDocumentTransform* out_transform,
                                                  char* diagnostics,
                                                  size_t diagnostics_size);
bool SceneEditorDocumentSetTransformForSceneIndex(int scene_object_index,
                                                  const SceneEditorDocumentTransform* transform,
                                                  char* diagnostics,
                                                  size_t diagnostics_size);
bool SceneEditorDocumentSetManagedShadingForSceneIndex(int scene_object_index,
                                                       const char* mode,
                                                       double crease_angle_degrees,
                                                       char* diagnostics,
                                                       size_t diagnostics_size);
bool SceneEditorDocumentSetMaterialIdForSceneIndex(int scene_object_index,
                                                   int material_id,
                                                   char* diagnostics,
                                                   size_t diagnostics_size);
/* Complete versioned mapping JSON, or NULL to return to explicit legacy meaning.
 * The retained command validates before applying and keeps source/unknown fields. */
bool SceneEditorDocumentSetSurfaceMappingForSceneIndex(int scene_object_index,
    const char* mapping_json, char* diagnostics, size_t diagnostics_size);
bool SceneEditorDocumentDuplicateForSceneIndex(int scene_object_index,
                                               int* out_new_scene_object_index,
                                               char* diagnostics,
                                               size_t diagnostics_size);
bool SceneEditorDocumentRemoveForSceneIndex(int scene_object_index,
                                            char* diagnostics,
                                            size_t diagnostics_size);
bool SceneEditorDocumentRenameForSceneIndex(int scene_object_index,
                                            const char* display_name,
                                            char* diagnostics,
                                            size_t diagnostics_size);

typedef struct SceneEditorDocumentObjectInfo {
    char id[128], name[128], type[64];
    int runtime_index;
    bool visible, locked;
} SceneEditorDocumentObjectInfo;
const char* SceneEditorDocumentTypeLabel(const char* type);
int SceneEditorDocumentObjectCount(void);
bool SceneEditorDocumentObjectAt(int ordinal, SceneEditorDocumentObjectInfo* out);
bool SceneEditorDocumentObjectById(const char* id, SceneEditorDocumentObjectInfo* out);
bool SceneEditorDocumentRenameById(const char* id,const char* name,unsigned long long revision,char* diagnostics,size_t size);
bool SceneEditorDocumentSetFlag(const char* id, const char* flag, bool value,
    unsigned long long revision, char* diagnostics, size_t diagnostics_size);
bool SceneEditorDocumentRequireEditable(int index);
bool SceneEditorDocumentObjectEditable(int index, char* diagnostics, size_t diagnostics_size);

bool SceneEditorDocumentObjectLabel(int scene_object_index, char* label, size_t size);
bool SceneEditorDocumentCanUndo(void);
bool SceneEditorDocumentCanRedo(void);
bool SceneEditorDocumentUndo(char* diagnostics, size_t diagnostics_size);
bool SceneEditorDocumentRedo(char* diagnostics, size_t diagnostics_size);

/* Merge the current Ray authoring overlay, then save through the locked atomic path. */
bool SceneEditorDocumentMergeOverlayAndSave(const char* overlay_json,
                                            char* diagnostics,
                                            size_t diagnostics_size);
bool SceneEditorDocumentSave(char* diagnostics, size_t diagnostics_size);

/* Validate and publish a same-directory managed-tool candidate as one command. */
bool SceneEditorDocumentAdoptCandidateAsCommand(const char* candidate_path,
                                                char* diagnostics,
                                                size_t diagnostics_size);


bool SceneEditorDocumentGetSurfaceMappingJSON(int index,char* out,size_t size);

#endif
