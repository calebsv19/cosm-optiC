#include "procedural/procedural_solid_mesh.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    CoreMeshAssetRuntimeDocument mesh;
    CoreResult result;
    char digest[65] = {0};
    if (argc != 2) {
        fprintf(stderr, "usage: %s RUNTIME_MESH.json\n", argv[0]);
        return 2;
    }
    core_mesh_asset_runtime_document_init(&mesh);
    result = core_mesh_asset_runtime_document_load_file(argv[1], &mesh);
    if (result.code != CORE_OK || !ProceduralSolidMesh_Digest(&mesh, digest)) {
        fprintf(stderr, "could not load or digest runtime mesh: %s\n", argv[1]);
        core_mesh_asset_runtime_document_free(&mesh);
        return 1;
    }
    printf("%s\n", digest);
    core_mesh_asset_runtime_document_free(&mesh);
    return 0;
}
