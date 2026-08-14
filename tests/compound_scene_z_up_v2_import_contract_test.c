#include "import/compound_scene_handoff_import.h"
#include "import/compound_scene_room_basis.h"
#include "import/compound_scene_static_room_import.h"

#include <stdio.h>
#include <string.h>

#define CHECK(value) do { if (!(value)) { \
    fprintf(stderr, "compound_scene_z_up_v2_import_contract failed line=%d check=%s\n", \
            __LINE__, #value); return 1; } } while (0)

int main(int argc, char** argv) {
    RayCompoundSceneHandoff handoff;
    RayCompoundSceneStaticRoom room;
    RayCompoundSceneRoomBasis basis;
    RayCompoundSceneMappedRoom mapped;
    RayCompoundSceneImportFailure handoff_failure;
    RayCompoundSceneStaticRoomImportFailure room_failure;
    ray_compound_scene_handoff_init(&handoff);
    ray_compound_scene_static_room_init(&room);
    CHECK(argc == 3);
    if (!ray_compound_scene_handoff_read(argv[1], &handoff,
                                         &handoff_failure)) {
        fprintf(stderr, "handoff failure=%s\n",
                ray_compound_scene_import_failure_name(handoff_failure));
        return 1;
    }
    if (!ray_compound_scene_static_room_read(argv[2], &room, &room_failure)) {
        fprintf(stderr, "room failure=%s\n",
                ray_compound_scene_static_room_import_failure_name(room_failure));
        return 1;
    }
    CHECK(!strcmp(handoff.coordinate_system,
                  RAY_COMPOUND_SCENE_COORDINATE_Z_UP));
    CHECK(!strcmp(room.coordinate_system,
                  RAY_COMPOUND_SCENE_COORDINATE_Z_UP));
    CHECK(handoff.handoff_digest ==
          room.provenance.transform_fixture_digest);
    CHECK(ray_compound_scene_room_basis_init_for_source(
        &basis, handoff.coordinate_system));
    CHECK(!strcmp(basis.mapping_id,
                  RAY_COMPOUND_SCENE_ROOM_IDENTITY_BASIS_ID));
    CHECK(ray_compound_scene_room_basis_bind(
        &handoff, &room, &basis, &mapped, NULL));
    CHECK(!strcmp(mapped.surfaces[RAY_COMPOUND_SCENE_STATIC_ROOM_Y_MIN].surface_id,
                  "y_min"));
    CHECK(!strcmp(mapped.surfaces[RAY_COMPOUND_SCENE_STATIC_ROOM_Y_MAX].surface_id,
                  "y_max"));
    CHECK(mapped.surfaces[0].inward_normal.z == 1.0);
    CHECK(mapped.surfaces[1].inward_normal.z == -1.0);
    CHECK(ray_compound_scene_room_basis_map_vec3(
        &basis, (RayCompoundSceneVec3){1, 2, 3}).x == 1.0);
    printf("compound scene native z-up v2 import: PASS handoff=%016llx room=%016llx basis=%016llx frames=%zu\n",
           (unsigned long long)handoff.handoff_digest,
           (unsigned long long)room.artifact_digest,
           (unsigned long long)basis.basis_digest, handoff.frame_count);
    ray_compound_scene_handoff_free(&handoff);
    return 0;
}
