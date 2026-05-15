#include "core_authored_texture.h"

#include <assert.h>
#include <string.h>

int main(void) {
    CoreAuthoredTexturePrimitiveKind primitive_kind =
        CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_NONE;
    CoreAuthoredTextureBindingKind binding_kind = CORE_AUTHORED_TEXTURE_BINDING_KIND_NONE;
    CoreAuthoredTextureOutputKind output_kind = CORE_AUTHORED_TEXTURE_OUTPUT_KIND_NONE;
    CoreAuthoredTextureNetLayoutKind net_layout_kind =
        CORE_AUTHORED_TEXTURE_NET_LAYOUT_KIND_NONE;
    CoreAuthoredTextureNetOrientation net_orientation =
        CORE_AUTHORED_TEXTURE_NET_ORIENTATION_NONE;
    CoreAuthoredTextureNetSlot net_slot = CORE_AUTHORED_TEXTURE_NET_SLOT_NONE;
    CoreAuthoredTextureFaceRole plane_roles[1] = {
        CORE_AUTHORED_TEXTURE_FACE_ROLE_FRONT
    };
    CoreAuthoredTextureFaceRole prism_roles[6] = {
        CORE_AUTHORED_TEXTURE_FACE_ROLE_FRONT,
        CORE_AUTHORED_TEXTURE_FACE_ROLE_BACK,
        CORE_AUTHORED_TEXTURE_FACE_ROLE_LEFT,
        CORE_AUTHORED_TEXTURE_FACE_ROLE_RIGHT,
        CORE_AUTHORED_TEXTURE_FACE_ROLE_TOP,
        CORE_AUTHORED_TEXTURE_FACE_ROLE_BOTTOM
    };
    CoreAuthoredTextureFaceRole duplicate_roles[6] = {
        CORE_AUTHORED_TEXTURE_FACE_ROLE_FRONT,
        CORE_AUTHORED_TEXTURE_FACE_ROLE_BACK,
        CORE_AUTHORED_TEXTURE_FACE_ROLE_LEFT,
        CORE_AUTHORED_TEXTURE_FACE_ROLE_RIGHT,
        CORE_AUTHORED_TEXTURE_FACE_ROLE_TOP,
        CORE_AUTHORED_TEXTURE_FACE_ROLE_TOP
    };
    uint8_t plane_corner_ids[CORE_AUTHORED_TEXTURE_FACE_CORNER_COUNT] = {
        CORE_AUTHORED_TEXTURE_UNKNOWN_ID,
        CORE_AUTHORED_TEXTURE_UNKNOWN_ID,
        CORE_AUTHORED_TEXTURE_UNKNOWN_ID,
        CORE_AUTHORED_TEXTURE_UNKNOWN_ID
    };
    uint8_t plane_edge_ids[CORE_AUTHORED_TEXTURE_FACE_EDGE_COUNT] = {
        CORE_AUTHORED_TEXTURE_UNKNOWN_ID,
        CORE_AUTHORED_TEXTURE_UNKNOWN_ID,
        CORE_AUTHORED_TEXTURE_UNKNOWN_ID,
        CORE_AUTHORED_TEXTURE_UNKNOWN_ID
    };
    CoreAuthoredTextureFaceRole plane_adjacent_roles[CORE_AUTHORED_TEXTURE_FACE_EDGE_COUNT] = {
        CORE_AUTHORED_TEXTURE_FACE_ROLE_NONE,
        CORE_AUTHORED_TEXTURE_FACE_ROLE_NONE,
        CORE_AUTHORED_TEXTURE_FACE_ROLE_NONE,
        CORE_AUTHORED_TEXTURE_FACE_ROLE_NONE
    };
    uint8_t prism_corner_ids[CORE_AUTHORED_TEXTURE_FACE_CORNER_COUNT] = {0u, 1u, 2u, 3u};
    uint8_t prism_edge_ids[CORE_AUTHORED_TEXTURE_FACE_EDGE_COUNT] = {0u, 1u, 2u, 3u};
    CoreAuthoredTextureFaceRole prism_adjacent_roles[CORE_AUTHORED_TEXTURE_FACE_EDGE_COUNT] = {
        CORE_AUTHORED_TEXTURE_FACE_ROLE_TOP,
        CORE_AUTHORED_TEXTURE_FACE_ROLE_RIGHT,
        CORE_AUTHORED_TEXTURE_FACE_ROLE_BOTTOM,
        CORE_AUTHORED_TEXTURE_FACE_ROLE_LEFT
    };
    CoreAuthoredTextureManifestContract contract;

    assert(strcmp(core_authored_texture_primitive_kind_name(
                      CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_PLANE),
                  "PLANE") == 0);
    assert(core_authored_texture_primitive_kind_parse("RECT_PRISM", &primitive_kind));
    assert(primitive_kind == CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_RECT_PRISM);

    assert(core_authored_texture_binding_kind_parse("SEPARATE_FACES", &binding_kind));
    assert(binding_kind == CORE_AUTHORED_TEXTURE_BINDING_KIND_SEPARATE_FACES);

    assert(core_authored_texture_output_kind_parse("BASE_PLUS_OVERLAY", &output_kind));
    assert(output_kind == CORE_AUTHORED_TEXTURE_OUTPUT_KIND_BASE_PLUS_OVERLAY);

    assert(core_authored_texture_face_role_parse("BOTTOM", &plane_roles[0]));
    assert(plane_roles[0] == CORE_AUTHORED_TEXTURE_FACE_ROLE_BOTTOM);
    plane_roles[0] = CORE_AUTHORED_TEXTURE_FACE_ROLE_FRONT;
    assert(core_authored_texture_face_role_parse("SURFACE", &plane_adjacent_roles[0]));
    assert(plane_adjacent_roles[0] == CORE_AUTHORED_TEXTURE_FACE_ROLE_NONE);
    plane_adjacent_roles[0] = CORE_AUTHORED_TEXTURE_FACE_ROLE_NONE;

    assert(core_authored_texture_net_layout_kind_parse("PRISM_CROSS", &net_layout_kind));
    assert(net_layout_kind == CORE_AUTHORED_TEXTURE_NET_LAYOUT_KIND_RECT_PRISM_CROSS);
    assert(core_authored_texture_net_orientation_parse("R90", &net_orientation));
    assert(net_orientation == CORE_AUTHORED_TEXTURE_NET_ORIENTATION_R90);
    assert(core_authored_texture_net_slot_parse("BOTTOM", &net_slot));
    assert(net_slot == CORE_AUTHORED_TEXTURE_NET_SLOT_BOTTOM);

    assert(core_authored_texture_schema_version_supported(CORE_AUTHORED_TEXTURE_SCHEMA_V1));
    assert(core_authored_texture_schema_version_supported(CORE_AUTHORED_TEXTURE_SCHEMA_V5));
    assert(!core_authored_texture_schema_version_supported(99));

    assert(core_authored_texture_expected_face_count(
               CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_PLANE) == 1u);
    assert(core_authored_texture_expected_face_count(
               CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_RECT_PRISM) == 6u);

    assert(core_authored_texture_face_role_allowed_for_primitive(
        CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_PLANE,
        CORE_AUTHORED_TEXTURE_FACE_ROLE_FRONT));
    assert(!core_authored_texture_face_role_allowed_for_primitive(
        CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_PLANE,
        CORE_AUTHORED_TEXTURE_FACE_ROLE_BACK));

    assert(core_authored_texture_face_roles_complete(
        CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_PLANE, plane_roles, 1u));
    assert(core_authored_texture_face_roles_complete(
        CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_RECT_PRISM, prism_roles, 6u));
    assert(!core_authored_texture_face_roles_complete(
        CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_RECT_PRISM, duplicate_roles, 6u));

    memset(&contract, 0, sizeof(contract));
    contract.schema_version = CORE_AUTHORED_TEXTURE_SCHEMA_V1;
    contract.binding_kind = CORE_AUTHORED_TEXTURE_BINDING_KIND_SEPARATE_FACES;
    contract.primitive_kind = CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_PLANE;
    contract.output_kind = CORE_AUTHORED_TEXTURE_OUTPUT_KIND_LEGACY_FLATTENED;
    contract.has_legacy_surfaces = true;
    assert(core_authored_texture_manifest_contract_validate(&contract));

    memset(&contract, 0, sizeof(contract));
    contract.schema_version = CORE_AUTHORED_TEXTURE_SCHEMA_V5;
    contract.binding_kind = CORE_AUTHORED_TEXTURE_BINDING_KIND_SEPARATE_FACES;
    contract.primitive_kind = CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_RECT_PRISM;
    contract.output_kind = CORE_AUTHORED_TEXTURE_OUTPUT_KIND_FLATTENED_ONLY;
    contract.has_base_surfaces = true;
    assert(core_authored_texture_manifest_contract_validate(&contract));

    contract.output_kind = CORE_AUTHORED_TEXTURE_OUTPUT_KIND_BASE_PLUS_OVERLAY;
    contract.has_overlay_surfaces = true;
    assert(core_authored_texture_manifest_contract_validate(&contract));

    contract.has_overlay_surfaces = false;
    assert(!core_authored_texture_manifest_contract_validate(&contract));

    contract.output_kind = CORE_AUTHORED_TEXTURE_OUTPUT_KIND_FLATTENED_ONLY;
    contract.has_overlay_surfaces = true;
    assert(!core_authored_texture_manifest_contract_validate(&contract));

    contract.output_kind = CORE_AUTHORED_TEXTURE_OUTPUT_KIND_FLATTENED_ONLY;
    contract.has_overlay_surfaces = false;
    contract.has_legacy_surfaces = true;
    assert(!core_authored_texture_manifest_contract_validate(&contract));

    assert(core_authored_texture_semantic_net_validate(
        CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_PLANE,
        CORE_AUTHORED_TEXTURE_FACE_ROLE_FRONT,
        CORE_AUTHORED_TEXTURE_NET_LAYOUT_KIND_PLANE,
        CORE_AUTHORED_TEXTURE_NET_SLOT_FRONT,
        CORE_AUTHORED_TEXTURE_NET_ORIENTATION_R0,
        plane_corner_ids,
        plane_edge_ids,
        plane_adjacent_roles));

    assert(core_authored_texture_semantic_net_validate(
        CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_RECT_PRISM,
        CORE_AUTHORED_TEXTURE_FACE_ROLE_FRONT,
        CORE_AUTHORED_TEXTURE_NET_LAYOUT_KIND_RECT_PRISM_CROSS,
        CORE_AUTHORED_TEXTURE_NET_SLOT_FRONT,
        CORE_AUTHORED_TEXTURE_NET_ORIENTATION_R0,
        prism_corner_ids,
        prism_edge_ids,
        prism_adjacent_roles));

    prism_edge_ids[3] = prism_edge_ids[0];
    assert(!core_authored_texture_semantic_net_validate(
        CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_RECT_PRISM,
        CORE_AUTHORED_TEXTURE_FACE_ROLE_FRONT,
        CORE_AUTHORED_TEXTURE_NET_LAYOUT_KIND_RECT_PRISM_CROSS,
        CORE_AUTHORED_TEXTURE_NET_SLOT_FRONT,
        CORE_AUTHORED_TEXTURE_NET_ORIENTATION_R0,
        prism_corner_ids,
        prism_edge_ids,
        prism_adjacent_roles));

    return 0;
}
