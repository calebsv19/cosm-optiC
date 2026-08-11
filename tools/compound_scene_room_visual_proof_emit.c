#include "animation/evaluated_scene_snapshot.h"
#include "core_mesh_asset.h"
#include "import/compound_scene_evaluated_scene.h"
#include "render/compound_scene_assembly.h"
#include "render/compound_scene_room_geometry.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool build_base(RayEvaluatedSceneSnapshot *output) {
  RayEvaluatedObjectTransform transforms[3] = {0};
  RayEvaluatedSceneSnapshotInputs inputs = {0};
  TimelineEvaluationContext context = {0};
  const char *ids[3] = {"sim_body_c2", "room_static_anchor", "sim_body_c1"};
  if (TimelineEvaluationContextBuild(
          (TimelineRate){60u, 1u}, (TimelineRange){0, 720u},
          (TimelineSample){0, 0u, 1u}, &context) != TIMELINE_STATUS_OK)
    return false;
  for (size_t i = 0; i < 3u; ++i) {
    transforms[i].valid = true;
    snprintf(transforms[i].target_id, sizeof(transforms[i].target_id), "%s",
             ids[i]);
    transforms[i].source = RAY_EVALUATED_OBJECT_TRANSFORM_COMPATIBILITY_MOTION;
    transforms[i].has_position = true;
    transforms[i].has_rotation = true;
    transforms[i].frame = context;
  }
  inputs.source = RAY_EVALUATED_SCENE_SOURCE_AUTHORED_TIMELINE;
  inputs.playback_mode = RAY_EVALUATED_PLAYBACK_STOP;
  inputs.frame = context;
  inputs.identity.scene_revision = 43u;
  inputs.identity.timeline_revision = 9u;
  inputs.light.valid = true;
  inputs.light.enabled = true;
  snprintf(inputs.light.target_id, sizeof(inputs.light.target_id), "%s",
           "light/key");
  snprintf(inputs.light.runtime_light_id, sizeof(inputs.light.runtime_light_id),
           "%s", "key");
  inputs.light.color = (TimelineVec3){1.0, 1.0, 1.0};
  inputs.light.intensity = 4.0;
  inputs.light.property_provenance.valid = true;
  inputs.camera.valid = true;
  inputs.camera.fov_y_degrees = 52.0;
  inputs.camera.aspect_ratio = 4.0 / 3.0;
  inputs.camera.zoom = 1.0;
  inputs.object_transforms = transforms;
  inputs.object_transform_count = 3u;
  inputs.diagnostics = "S9-H3 six-plane room visual proof";
  return RayEvaluatedSceneSnapshotBuild(&inputs, output) == TIMELINE_STATUS_OK;
}

static void populate_bindings(RayCompoundSceneBindingManifest *manifest) {
  snprintf(manifest->bindings[0].object_id,
           sizeof(manifest->bindings[0].object_id), "%s", "sim_body_c2");
  snprintf(manifest->bindings[0].mesh_asset_id,
           sizeof(manifest->bindings[0].mesh_asset_id), "%s",
           "mesh_c2_u_channel");
  snprintf(manifest->bindings[1].object_id,
           sizeof(manifest->bindings[1].object_id), "%s", "sim_body_c1");
  snprintf(manifest->bindings[1].mesh_asset_id,
           sizeof(manifest->bindings[1].mesh_asset_id), "%s",
           "mesh_c1_l_bracket");
}

static void print_vec(RayCompoundSceneVec3 value) {
  printf("[%.17g,%.17g,%.17g]", value.x, value.y, value.z);
}

static bool parse_tick(const char *text, uint64_t *output) {
  char *end = NULL;
  errno = 0;
  unsigned long long value = strtoull(text, &end, 10);
  if (errno || !text[0] || !end || *end)
    return false;
  *output = (uint64_t)value;
  return true;
}

int main(int argc, char **argv) {
  if (argc != 6) {
    fprintf(stderr, "usage: %s HANDOFF STATIC_ROOM C2_MESH C1_MESH TICK\n",
            argv[0]);
    return 2;
  }
  uint64_t tick = 0;
  if (!parse_tick(argv[5], &tick))
    return 2;
  RayCompoundSceneHandoff handoff;
  RayCompoundSceneImportFailure handoff_failure;
  RayCompoundSceneStaticRoom source_room;
  RayCompoundSceneStaticRoomImportFailure room_failure;
  RayCompoundSceneRoomBasis basis;
  RayCompoundSceneMappedRoom mapped_room;
  RayCompoundSceneRoomBasisFailure basis_failure;
  RayCompoundSceneRoomGeometry room_geometry;
  RayCompoundSceneRoomGeometryFailure room_geometry_failure;
  RayCompoundSceneBindingManifest manifest;
  RayCompoundSceneEvaluatedSceneFailure evaluated_failure;
  RayCompoundSceneAssemblyFailure assembly_failure;
  RayEvaluatedSceneSnapshot base = {0};
  RayEvaluatedSceneSnapshot evaluated = {0};
  CoreMeshAssetRuntimeDocument meshes[2];
  RayCompoundSceneVec3 *source[2] = {NULL, NULL};
  RayCompoundSceneVec3 *world[2] = {NULL, NULL};
  RayCompoundSceneStaticObjectSpec statics[6] = {0};
  RayCompoundSceneAssemblyRequest request = {0};
  RayCompoundSceneAssembly assembly = {0};
  int result = 1;
  ray_compound_scene_handoff_init(&handoff);
  ray_compound_scene_static_room_init(&source_room);
  core_mesh_asset_runtime_document_init(&meshes[0]);
  core_mesh_asset_runtime_document_init(&meshes[1]);
  if (!ray_compound_scene_handoff_read(argv[1], &handoff, &handoff_failure) ||
      !ray_compound_scene_static_room_read(argv[2], &source_room,
                                           &room_failure) ||
      core_mesh_asset_runtime_document_load_file(argv[3], &meshes[0]).code !=
          CORE_OK ||
      core_mesh_asset_runtime_document_load_file(argv[4], &meshes[1]).code !=
          CORE_OK)
    goto done;
  ray_compound_scene_room_basis_init(&basis);
  if (!ray_compound_scene_room_basis_bind(&handoff, &source_room, &basis,
                                          &mapped_room, &basis_failure) ||
      !ray_compound_scene_room_geometry_build(&handoff, &source_room, &basis,
                                              &mapped_room, &room_geometry,
                                              &room_geometry_failure))
    goto done;
  ray_compound_scene_binding_manifest_init(&manifest, &handoff);
  populate_bindings(&manifest);
  if (!build_base(&base) ||
      !ray_compound_scene_evaluated_scene_apply_exact(
          &handoff, &manifest, tick, &base, &evaluated, &evaluated_failure))
    goto done;
  request.handoff = &handoff;
  request.manifest = &manifest;
  request.snapshot = &evaluated;
  request.static_objects = statics;
  request.static_object_count = room_geometry.plane_count;
  for (size_t i = 0; i < room_geometry.plane_count; ++i) {
    snprintf(statics[i].object_id, sizeof(statics[i].object_id), "%s",
             room_geometry.planes[i].object_id);
    snprintf(statics[i].geometry_id, sizeof(statics[i].geometry_id), "%s",
             "primitive/plane");
    snprintf(statics[i].material_id, sizeof(statics[i].material_id), "%s",
             room_geometry.planes[i].material_id);
  }
  for (size_t i = 0; i < 2u; ++i) {
    source[i] = calloc(meshes[i].vertex_count, sizeof(*source[i]));
    world[i] = calloc(meshes[i].vertex_count, sizeof(*world[i]));
    if (!source[i] || !world[i])
      goto done;
    for (size_t j = 0; j < meshes[i].vertex_count; ++j)
      source[i][j] = (RayCompoundSceneVec3){meshes[i].vertices[j].position.x,
                                            meshes[i].vertices[j].position.y,
                                            meshes[i].vertices[j].position.z};
    request.simulated_sources[i] = (RayCompoundSceneSourceGeometryView){
        handoff.bindings[i].body_id,
        manifest.bindings[i].object_id,
        manifest.bindings[i].mesh_asset_id,
        handoff.bindings[i].source_asset_id,
        handoff.bindings[i].source_sha256,
        source[i],
        meshes[i].vertex_count};
    request.simulated_targets[i] =
        (RayCompoundSceneGeometryTarget){world[i], meshes[i].vertex_count};
  }
  if (!ray_compound_scene_assembly_build_exact(&request, &assembly,
                                               &assembly_failure))
    goto done;
  for (size_t i = 0; i < 2u; ++i) {
    RayCompoundSceneVec3 minimum = {0};
    RayCompoundSceneVec3 maximum = {0};
    for (size_t j = 0; j < meshes[i].vertex_count; ++j) {
      world[i][j] = ray_compound_scene_room_basis_map_vec3(&basis, world[i][j]);
      if (!j)
        minimum = maximum = world[i][j];
      if (world[i][j].x < minimum.x)
        minimum.x = world[i][j].x;
      if (world[i][j].y < minimum.y)
        minimum.y = world[i][j].y;
      if (world[i][j].z < minimum.z)
        minimum.z = world[i][j].z;
      if (world[i][j].x > maximum.x)
        maximum.x = world[i][j].x;
      if (world[i][j].y > maximum.y)
        maximum.y = world[i][j].y;
      if (world[i][j].z > maximum.z)
        maximum.z = world[i][j].z;
    }
    assembly.objects[i].bounds_min = minimum;
    assembly.objects[i].bounds_max = maximum;
  }
  printf("{\"schema\":\"ray_compound_scene_s9h3_visual_payload_v1\","
         "\"tick\":%llu,\"handoff_digest\":\"%016llx\","
         "\"room_geometry_digest\":\"%016llx\","
         "\"mapped_room_digest\":\"%016llx\",\"planes\":[",
         (unsigned long long)tick, (unsigned long long)handoff.handoff_digest,
         (unsigned long long)room_geometry.geometry_digest,
         (unsigned long long)mapped_room.mapped_digest);
  for (size_t i = 0; i < room_geometry.plane_count; ++i) {
    const RayCompoundSceneRoomPlane *plane = &room_geometry.planes[i];
    printf("%s{\"object_id\":\"%s\",\"material_id\":\"%s\","
           "\"role\":%u,\"producer_body_id\":%d,"
           "\"contact_mask_bit\":%u,"
           "\"producer_surface_digest\":\"%016llx\","
           "\"visible\":%s,\"width\":%.17g,\"height\":%.17g,"
           "\"origin\":",
           i ? "," : "", plane->object_id, plane->material_id,
           (unsigned)plane->role, plane->producer_body_id,
           (unsigned)plane->contact_mask_bit,
           (unsigned long long)plane->producer_surface_digest,
           plane->render_visible ? "true" : "false", plane->width_m,
           plane->height_m);
    print_vec(plane->origin_m);
    printf(",\"axis_u\":");
    print_vec(plane->axis_u);
    printf(",\"axis_v\":");
    print_vec(plane->axis_v);
    printf(",\"normal\":");
    print_vec(plane->inward_normal);
    printf("}");
  }
  printf("],\"bodies\":[");
  for (size_t i = 0; i < 2u; ++i) {
    const RayCompoundSceneObjectRecord *object = &assembly.objects[i];
    printf("%s{\"object_id\":\"%s\",\"source_asset_id\":\"%s\","
           "\"source_sha256\":\"%s\",\"vertex_count\":%zu,"
           "\"triangle_count\":%zu,\"bounds_min\":",
           i ? "," : "", object->object_id, object->source_asset_id,
           object->source_sha256, object->vertex_count,
           meshes[i].triangle_count);
    print_vec(object->bounds_min);
    printf(",\"bounds_max\":");
    print_vec(object->bounds_max);
    printf(",\"vertices\":[");
    for (size_t j = 0; j < object->vertex_count; ++j) {
      if (j)
        printf(",");
      print_vec(world[i][j]);
    }
    printf("]}");
  }
  printf("],\"ownership\":{\"source_mesh_renderer_owned\":true,"
         "\"room_planes_renderer_owned\":true,"
         "\"collision_proxy_rendered\":false,"
         "\"six_producer_surfaces_matched\":true,"
         "\"visible_plane_count\":5,"
         "\"camera_opening_role\":\"z_max\","
         "\"default_request_or_worker_integration\":false}}\n");
  result = 0;
done:
  for (size_t i = 0; i < 2u; ++i) {
    free(world[i]);
    free(source[i]);
    core_mesh_asset_runtime_document_free(&meshes[i]);
  }
  ray_compound_scene_handoff_free(&handoff);
  return result;
}
