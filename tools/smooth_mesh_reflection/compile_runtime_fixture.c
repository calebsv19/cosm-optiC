#include <stdio.h>

#include "core_mesh_asset.h"
#include "core_mesh_compile.h"

int main(int argc, char** argv) {
    CoreMeshAssetAuthoringDocument authoring;
    CoreResult result;
    if (argc != 5) {
        fprintf(stderr,
                "usage: %s <authoring.json> <source-root> <runtime-asset-id> <output.json>\n",
                argv[0]);
        return 2;
    }

    core_mesh_asset_authoring_document_init(&authoring);
    result = core_mesh_asset_authoring_document_load_file(argv[1], &authoring);
    if (result.code == CORE_OK) {
        result = core_mesh_compile_imported_mesh_to_runtime_file(&authoring,
                                                                 argv[2],
                                                                 argv[3],
                                                                 argv[4]);
    }
    core_mesh_asset_authoring_document_free(&authoring);
    if (result.code != CORE_OK) {
        fprintf(stderr, "%s\n", result.message ? result.message : "runtime compile failed");
        return 1;
    }
    printf("runtime mesh fixture ready: %s\n", argv[4]);
    return 0;
}
