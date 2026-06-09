#ifndef CORE_MESH_COMPILE_H
#define CORE_MESH_COMPILE_H

#include <stdbool.h>

#include "core_base.h"
#include "core_mesh_asset.h"
#include "core_object.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum CoreMeshCompileGeometryRefKind {
    CORE_MESH_COMPILE_GEOMETRY_REF_KIND_UNKNOWN = 0,
    CORE_MESH_COMPILE_GEOMETRY_REF_KIND_MESH_ASSET = 1
} CoreMeshCompileGeometryRefKind;

typedef struct CoreMeshCompileInstanceContract {
    CoreObject object;
    CoreMeshCompileGeometryRefKind geometry_ref_kind;
    char asset_id[64];
    char variant[64];
    bool has_material_ref_id;
    char material_ref_id[64];
} CoreMeshCompileInstanceContract;

typedef struct CoreMeshCompileAuthoringContract {
    char source_asset_id[64];
    char runtime_asset_id[64];
    CoreMeshAssetSourceMode source_mode;
    bool emits_runtime_mesh_asset;
    bool preserves_surface_group_ids;
    bool requires_imported_mesh_source;
} CoreMeshCompileAuthoringContract;

const char *core_mesh_compile_geometry_ref_kind_name(CoreMeshCompileGeometryRefKind kind);
CoreResult core_mesh_compile_geometry_ref_kind_parse(const char *text,
                                                     CoreMeshCompileGeometryRefKind *out_kind);

void core_mesh_compile_instance_contract_init(CoreMeshCompileInstanceContract *contract);
CoreResult core_mesh_compile_instance_contract_prepare(CoreMeshCompileInstanceContract *contract,
                                                       const char *object_id,
                                                       const char *asset_id);
CoreResult core_mesh_compile_instance_contract_validate(
    const CoreMeshCompileInstanceContract *contract);

void core_mesh_compile_authoring_contract_init(CoreMeshCompileAuthoringContract *contract);
CoreResult core_mesh_compile_authoring_contract_prepare(
    CoreMeshCompileAuthoringContract *contract,
    const CoreMeshAssetAuthoringDocument *document,
    const char *runtime_asset_id);
CoreResult core_mesh_compile_authoring_contract_validate(
    const CoreMeshCompileAuthoringContract *contract);

CoreResult core_mesh_compile_imported_mesh_to_runtime_document(
    const CoreMeshAssetAuthoringDocument *document,
    const char *source_root,
    const char *runtime_asset_id,
    CoreMeshAssetRuntimeDocument *out_document);
CoreResult core_mesh_compile_imported_mesh_to_runtime_file(
    const CoreMeshAssetAuthoringDocument *document,
    const char *source_root,
    const char *runtime_asset_id,
    const char *runtime_output_path);

#ifdef __cplusplus
}
#endif

#endif
