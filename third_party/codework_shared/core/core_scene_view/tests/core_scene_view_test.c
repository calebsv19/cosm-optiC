#include "core_scene_view.h"

#include <stdio.h>
#include <string.h>

#define CHECK(cond) do { if (!(cond)) { printf("fail:%d\n", __LINE__); return 1; } } while (0)

static const char *kPacket =
    "{"
    "\"schema_family\":\"codework_scene_view\","
    "\"schema_variant\":\"ray_tracing_scene_view_packet_v0\","
    "\"focused_object_index\":4,"
    "\"preview_quality\":\"material_preview\","
    "\"preview_quality_id\":2,"
    "\"degraded_reason\":\"none\","
    "\"degraded_reason_id\":0,"
    "\"projected\":true,"
    "\"complete\":true,"
    "\"triangle_count\":2,"
    "\"face_group_count\":6,"
    "\"triangles\":["
    "{"
    "\"pick_id\":{"
    "\"scene_object_index\":4,"
    "\"primitive_index\":0,"
    "\"triangle_index\":0,"
    "\"local_triangle_index\":0,"
    "\"face_group_index\":0"
    "},"
    "\"rgba\":[80,120,200,96],"
    "\"display_flags\":9"
    "},"
    "{"
    "\"pick_id\":{"
    "\"scene_object_index\":4,"
    "\"primitive_index\":0,"
    "\"triangle_index\":11,"
    "\"local_triangle_index\":1,"
    "\"face_group_index\":5"
    "},"
    "\"rgba\":[90,130,210,255],"
    "\"display_flags\":0"
    "}"
    "]"
    "}";

static int test_names_and_parse(void) {
    CoreSceneViewPreviewQuality quality = CORE_SCENE_VIEW_PREVIEW_OUTLINE;
    CoreSceneViewDegradedReason reason = CORE_SCENE_VIEW_DEGRADED_NONE;

    CHECK(strcmp(core_scene_view_preview_quality_name(CORE_SCENE_VIEW_PREVIEW_MATERIAL),
                 "material_preview") == 0);
    CHECK(strcmp(core_scene_view_degraded_reason_name(CORE_SCENE_VIEW_DEGRADED_OBJECT_NOT_FOUND),
                 "object_not_found") == 0);
    CHECK(core_scene_view_preview_quality_parse("flat_solid", &quality).code == CORE_OK);
    CHECK(quality == CORE_SCENE_VIEW_PREVIEW_FLAT_SOLID);
    CHECK(core_scene_view_degraded_reason_parse("projection_unavailable", &reason).code == CORE_OK);
    CHECK(reason == CORE_SCENE_VIEW_DEGRADED_PROJECTION_UNAVAILABLE);
    CHECK(core_scene_view_preview_quality_parse("bad", &quality).code == CORE_ERR_NOT_FOUND);
    CHECK(core_scene_view_degraded_reason_parse(NULL, &reason).code == CORE_ERR_INVALID_ARG);
    return 0;
}

static int test_packet_readback(void) {
    CoreSceneViewPacketReadback readback;
    CoreResult result = core_scene_view_packet_readback_from_json_string(kPacket, &readback);

    CHECK(result.code == CORE_OK);
    CHECK(readback.valid);
    CHECK(readback.focusedObjectIndex == 4);
    CHECK(readback.previewQuality == CORE_SCENE_VIEW_PREVIEW_MATERIAL);
    CHECK(readback.degradedReason == CORE_SCENE_VIEW_DEGRADED_NONE);
    CHECK(readback.projected);
    CHECK(readback.complete);
    CHECK(readback.triangleCount == 2);
    CHECK(readback.faceGroupCount == 6);
    CHECK(readback.firstPickId.faceGroupIndex == 0);
    CHECK(readback.lastPickId.triangleIndex == 11);
    CHECK(readback.lastPickId.faceGroupIndex == 5);
    CHECK(readback.firstAlpha == 96u);
    CHECK((readback.firstDisplayFlags & CORE_SCENE_VIEW_DISPLAY_TRANSPARENT) != 0u);
    CHECK((readback.firstDisplayFlags & CORE_SCENE_VIEW_DISPLAY_TEXTURED) != 0u);
    return 0;
}

static int test_packet_rejects_bad_schema(void) {
    static const char *bad_schema =
        "{\"schema_family\":\"codework_scene_view\","
        "\"schema_variant\":\"unexpected\","
        "\"triangles\":[]}";
    CoreSceneViewPacketReadback readback;
    CoreResult result = core_scene_view_packet_readback_from_json_string(bad_schema, &readback);

    CHECK(result.code == CORE_ERR_FORMAT);
    CHECK(!readback.valid);
    return 0;
}

int main(void) {
    int failed = 0;
    failed |= test_names_and_parse();
    failed |= test_packet_readback();
    failed |= test_packet_rejects_bad_schema();
    if (failed) {
        printf("core_scene_view_test failed\n");
        return 1;
    }
    printf("core_scene_view_test passed\n");
    return 0;
}
