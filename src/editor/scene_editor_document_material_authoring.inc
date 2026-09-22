/* Retained material authoring stays in the document transaction owner. Runtime
 * validation is authoritative; failed publication restores the complete source. */
static const char *material_source_keys[] = {"surface_graph",
                                             "material_graph",
                                             "materialGraph",
                                             "material_texture_stack",
                                             "materialTextureStack",
                                             "surface_material_binding",
                                             "authored_texture",
                                             "procedural_texture",
                                             "texture_id",
                                             "textureColorDepth",
                                             "textureContrast",
                                             "textureCoverage",
                                             "textureEdgeSoftness",
                                             "textureFlow",
                                             "textureGrain",
                                             "textureId",
                                             "textureOffsetU",
                                             "textureOffsetV",
                                             "texturePatternMode",
                                             "textureScale",
                                             "textureSeed",
                                             "textureStrength",
                                             "textureSurfaceDamage",
                                             "texture_color_depth",
                                             "texture_contrast",
                                             "texture_coverage",
                                             "texture_edge_softness",
                                             "texture_flow",
                                             "texture_grain",
                                             "texture_offset_u",
                                             "texture_offset_v",
                                             "texture_pattern_mode",
                                             "texture_scale",
                                             "texture_seed",
                                             "texture_strength",
                                             "texture_surface_damage"};
static const char *material_response_keys[] = {"material_id",
                                               "object_color",
                                               "roughness",
                                               "reflectivity",
                                               "alpha",
                                               "transparency",
                                               "emissive_strength",
                                               "glassAbsorptionColor",
                                               "glassAbsorptionDistance",
                                               "glassInterfaceTint",
                                               "glassIor",
                                               "glassThinWalled",
                                               "glassTransmission",
                                               "glassTransportOverride",
                                               "glass_absorption_color",
                                               "glass_absorption_distance",
                                               "glass_interface_tint",
                                               "glass_ior",
                                               "glass_thin_walled",
                                               "glass_transmission",
                                               "glass_transport_override",
                                               "hairAbsorptionB",
                                               "hairAbsorptionG",
                                               "hairAbsorptionR",
                                               "hairAzimuthalRoughness",
                                               "hairCuticleTiltDegrees",
                                               "hairIor",
                                               "hairLongitudinalRoughness",
                                               "hairOpticsEnabled",
                                               "hair_absorption_b",
                                               "hair_absorption_g",
                                               "hair_absorption_r",
                                               "hair_azimuthal_roughness",
                                               "hair_cuticle_tilt_degrees",
                                               "hair_ior",
                                               "hair_longitudinal_roughness",
                                               "hair_optics_enabled"};
static size_t material_array_size(json_object *array) {
    return json_object_is_type(array, json_type_array) ? json_object_array_length(array) : 0;
}
static json_object *material_ensure_object(json_object *parent, const char *key) {
    json_object *value = NULL;
    if (!json_object_is_type(parent, json_type_object))
        return NULL;
    if (json_object_object_get_ex(parent, key, &value))
        return json_object_is_type(value, json_type_object) ? value : NULL;
    value = json_object_new_object();
    if (value)
        json_object_object_add(parent, key, value);
    return value;
}
static bool material_has(json_object *object, const char *key) {
    json_object *value = NULL;
    return object && json_object_object_get_ex(object, key, &value);
}
static json_object *material_clone(json_object *value) {
    return value ? json_tokener_parse(json_object_to_json_string_ext(value, JSON_C_TO_STRING_PLAIN))
                 : NULL;
}
static bool material_guard(int index, unsigned long long revision, char *diagnostic, size_t size) {
    if (revision != s_document.revision) {
        document_diag(diagnostic, size, "stale material edit; refresh the selected source");
        return false;
    }
    return SceneEditorDocumentObjectEditable(index, diagnostic, size);
}
static json_object *material_ray(json_object *object, bool create) {
    json_object *extensions = document_member(object, "extensions");
    if (!extensions && create)
        extensions = material_ensure_object(object, "extensions");
    json_object *ray = document_member(extensions, "ray_tracing");
    if (!ray && create)
        ray = material_ensure_object(extensions, "ray_tracing");
    return json_object_is_type(ray, json_type_object) ? ray : NULL;
}
static json_object *material_ensure_row(int index) {
    json_object *row = document_material_row(index);
    if (row)
        return row;
    json_object *object = document_object_for_scene_index(index, NULL, 0);
    json_object *id = document_member(object, "object_id");
    json_object *ext = material_ensure_object(s_document.root, "extensions");
    json_object *ray = material_ensure_object(ext, "ray_tracing");
    json_object *author = material_ensure_object(ray, "authoring");
    if (!author || !id)
        return NULL;
    json_object *rows = document_member(author, "object_materials");
    if (!material_has(author, "object_materials")) {
        rows = json_object_new_array();
        json_object_object_add(author, "object_materials", rows);
    }
    if (!id || !json_object_is_type(rows, json_type_array))
        return NULL;
    row = json_object_new_object();
    if (!row)
        return NULL;
    json_object_object_add(row, "object_id", json_object_get(id));
    json_object_object_add(row, "material_id",
                           json_object_new_int(sceneSettings.sceneObjects[index].material_id));
    json_object_object_add(row, "object_color",
                           json_object_new_int(sceneSettings.sceneObjects[index].color));
    json_object_object_add(row, "roughness",
                           json_object_new_double(sceneSettings.sceneObjects[index].roughness));
    json_object_object_add(row, "reflectivity",
                           json_object_new_double(sceneSettings.sceneObjects[index].reflectivity));
    json_object_array_add(rows, row);
    return row;
}
static bool material_explicit_source(json_object *row, json_object *object) {
    for (size_t i = 0; i < sizeof(material_source_keys) / sizeof(*material_source_keys); ++i)
        if (strncmp(material_source_keys[i], "texture", 7) &&
            material_has(row, material_source_keys[i]))
            return true;
    json_object *ray = material_ray(object, false);
    return material_has(ray, "surface_mapping") || material_has(ray, "surface_sampling") ||
           material_has(object, "procedural_solid_material_ref");
}
static bool material_supported_geometry(json_object *object, char *diagnostic, size_t size) {
    const char *kind = json_object_get_string(document_member(object, "object_type"));
    if (kind && (!strcmp(kind, "mesh_asset_instance") || !strcmp(kind, "plane_primitive") ||
                 !strcmp(kind, "rect_prism_primitive")))
        return true;
    document_diag(diagnostic, size,
                  "typed material presets require a mesh instance, plane or rectangular prism; "
                  "geometry was not changed");
    return false;
}
/* Preserve replaced declarations (including unknown nested metadata) outside the
 * executable source namespace. Unknown active row fields are never removed. */
static bool material_archive(json_object *row, json_object *object) {
    json_object *meta = document_member(row, "material_authoring");
    if (material_has(row, "material_authoring") && !json_object_is_type(meta, json_type_object))
        return false;
    if (!meta)
        meta = material_ensure_object(row, "material_authoring");
    if (!meta)
        return false;
    json_object *history = document_member(meta, "replaced_sources");
    if (material_has(meta, "replaced_sources") && !json_object_is_type(history, json_type_array))
        return false;
    if (!history) {
        history = json_object_new_array();
        json_object_object_add(meta, "replaced_sources", history);
    }
    json_object *archived = json_object_new_object();
    if (!archived)
        return false;
    json_object *previous_meta = material_clone(meta);
    if (!previous_meta) {
        json_object_put(archived);
        return false;
    }
    json_object_object_del(previous_meta, "replaced_sources");
    if (json_object_object_length(previous_meta))
        json_object_object_add(archived, "material_authoring", previous_meta);
    else
        json_object_put(previous_meta);
    for (size_t i = 0; i < sizeof(material_source_keys) / sizeof(*material_source_keys); ++i) {
        const char *key = material_source_keys[i];
        if (material_has(row, key))
            json_object_object_add(archived, key, json_object_get(document_member(row, key)));
    }
    for (size_t i = 0; i < sizeof(material_response_keys) / sizeof(*material_response_keys); ++i)
        if (material_has(row, material_response_keys[i]))
            json_object_object_add(
                archived, material_response_keys[i],
                json_object_get(document_member(row, material_response_keys[i])));
    json_object *ray = material_ray(object, false);
    if (material_has(ray, "surface_mapping"))
        json_object_object_add(archived, "surface_mapping",
                               json_object_get(document_member(ray, "surface_mapping")));
    if (material_has(ray, "surface_sampling"))
        json_object_object_add(archived, "surface_sampling",
                               json_object_get(document_member(ray, "surface_sampling")));
    if (material_has(object, "procedural_solid_material_ref"))
        json_object_object_add(
            archived, "procedural_solid_material_ref",
            json_object_get(document_member(object, "procedural_solid_material_ref")));
    if (material_has(meta, "source_id"))
        json_object_object_add(archived, "source_id",
                               json_object_get(document_member(meta, "source_id")));
    if (json_object_object_length(archived))
        json_object_array_add(history, archived);
    else
        json_object_put(archived);
    return true;
}
static void material_clear_source(json_object *row, json_object *object) {
    for (size_t i = 0; i < sizeof(material_source_keys) / sizeof(*material_source_keys); ++i)
        json_object_object_del(row, material_source_keys[i]);
    json_object *ray = material_ray(object, false);
    if (ray) {
        json_object_object_del(ray, "surface_mapping");
        json_object_object_del(ray, "surface_sampling");
    }
    json_object_object_del(object, "procedural_solid_material_ref");
}
/* Reserve IDs in every retained source history and provenance branch as well as
 * active assignments. Reopening resets the command revision, not these identities.
 * Bound traversal and fail closed rather than allocating an incompletely checked ID. */
static bool material_identity_used(json_object *value, const char *identity, unsigned depth,
                                   size_t *remaining, bool *complete) {
    if (!*remaining || depth > 128) {
        *complete = false;
        return false;
    }
    --*remaining;
    if (json_object_is_type(value, json_type_object)) {
        json_object_object_foreach(value, key, child) {
            if ((!strcmp(key, "source_id") || !strcmp(key, "origin_source_id")) &&
                json_object_is_type(child, json_type_string) &&
                !strcmp(json_object_get_string(child), identity))
                return true;
            if (material_identity_used(child, identity, depth + 1, remaining, complete))
                return true;
            if (!*complete)
                return false;
        }
    } else if (json_object_is_type(value, json_type_array)) {
        for (size_t i = 0; i < json_object_array_length(value); ++i) {
            if (material_identity_used(json_object_array_get_idx(value, i), identity, depth + 1,
                                       remaining, complete))
                return true;
            if (!*complete)
                return false;
        }
    }
    return false;
}
static bool material_fresh_identity(json_object *row, const char *name, json_object *source) {
    json_object *meta = document_member(row, "material_authoring");
    if (material_has(row, "material_authoring") && !json_object_is_type(meta, json_type_object))
        return false;
    if (!meta)
        meta = material_ensure_object(row, "material_authoring");
    if (!meta)
        return false;
    char identity[64];
    for (unsigned suffix = 1; suffix < 1000000; ++suffix) {
        snprintf(identity, sizeof(identity), "material_%llu_%u", s_document.revision + 1, suffix);
        size_t remaining = 1000000;
        bool complete = true;
        bool used = material_identity_used(s_document.root, identity, 0, &remaining, &complete);
        if (!complete)
            return false;
        if (!used) {
            if (source)
                json_object_object_add(meta, "assigned_from", json_object_get(source));
            else
                json_object_object_del(meta, "assigned_from");
            json_object_object_add(meta, "source_id", json_object_new_string(identity));
            json_object_object_add(meta, "name", json_object_new_string(name));
            return true;
        }
    }
    return false;
}
static bool material_abort(char *diagnostic, size_t size, const char *message) {
    document_rollback_command();
    document_diag(diagnostic, size, message);
    return false;
}
static bool material_composition(json_object *graph) {
    const char *cap=json_object_get_string(document_member(graph,"required_capability"));
    return json_object_get_int(document_member(graph,"version"))==2 && cap && !strcmp(cap,"optic.surface_composition_v1");
}
static json_object *material_preset(const char *preset) {
    if (!preset || (strcmp(preset, "noise3d") && strcmp(preset, "triplanar_checker") &&
                    strcmp(preset, "solid") && strcmp(preset,"composition")))
        return NULL;
    const char *template =
        "{\"version\":1,\"required_capability\":\"optic.surface_graph_v1\",\"color_space\":"
        "\"linear\",\"producer\":{\"name\":\"optiC material "
        "editor\",\"revision\":1},\"nodes\":[{\"id\":\"position\",\"kind\":\"coordinate\","
        "\"space\":\"object_rest\",\"scale_m\":0.2,\"offset\":[0,0,0]},{\"id\":\"pattern\","
        "\"kind\":\"noise3d\",\"inputs\":[\"position\"],\"seed\":17},{\"id\":\"dark\",\"kind\":"
        "\"color\",\"value\":[0.05,0.08,0.12]},{\"id\":\"light\",\"kind\":\"color\",\"value\":[0.8,"
        "0.45,0.12]},{\"id\":\"rough\",\"kind\":\"scalar\",\"value\":0.65},{\"id\":\"finish\","
        "\"kind\":\"mix\",\"inputs\":[\"dark\",\"light\",\"pattern\"]}],\"outputs\":{\"base_"
        "color\":\"finish\",\"roughness\":\"rough\"}}";
    json_object *graph = json_tokener_parse(template);
    if (!graph)
        return NULL;
    json_object *nodes = document_member(graph, "nodes");
    if(!strcmp(preset,"composition")) {
        json_object_object_add(graph,"version",json_object_new_int(2));
        json_object_object_add(graph,"required_capability",json_object_new_string("optic.surface_composition_v1"));
    }
    if (!strcmp(preset, "triplanar_checker")) {
        json_object *pattern = json_object_array_get_idx(nodes, 1);
        json_object_object_add(pattern, "kind", json_object_new_string("triplanar_checker"));
        json_object_object_del(pattern, "seed");
        json_object_object_add(pattern, "sharpness", json_object_new_double(2));
    } else if (!strcmp(preset, "solid")) {
        json_object_object_add(document_member(graph, "outputs"), "base_color",
                               json_object_new_string("light"));
        /* Keep a minimal solid source, with stable names shared by the presets. */
        json_object_array_del_idx(nodes, 5, 1);
        json_object_array_del_idx(nodes, 0, 3);
    }
    return graph;
}
bool SceneEditorDocumentMaterialPreset(int index, const char *preset, bool replace,
                                       unsigned long long revision, char *diagnostic, size_t size) {
    if (!material_guard(index, revision, diagnostic, size))
        return false;
    json_object *object = document_object_for_scene_index(index, diagnostic, size);
    if (!material_supported_geometry(object, diagnostic, size))
        return false;
    if (material_has(object, "procedural_solid_material_ref")) {
        document_diag(diagnostic, size,
                      "procedural solid asset geometry requires asset authoring; its source "
                      "reference was not removed");
        return false;
    }
    if (!replace && material_explicit_source(document_material_row(index), object)) {
        document_diag(
            diagnostic, size,
            "material already has a retained source; choose Replace to archive and replace it");
        return false;
    }
    json_object *graph = material_preset(preset);
    if (!graph) {
        document_diag(diagnostic, size, "choose noise3d, triplanar_checker, solid or composition preset");
        return false;
    }
    if (!document_begin_command(diagnostic, size)) {
        json_object_put(graph);
        return false;
    }
    json_object *row = material_ensure_row(index);
    if (!row || !material_archive(row, object)) {
        json_object_put(graph);
        return material_abort(diagnostic, size, "cannot preserve existing material metadata");
    }
    bool composition=!strcmp(preset,"composition");
    json_object *chart=NULL,*sampling=NULL;
    if(composition) {
        json_object *ray=material_ray(object,false);
        chart=material_clone(document_member(ray,"surface_mapping"));
        sampling=material_clone(document_member(ray,"surface_sampling"));
        const char *type=json_object_get_string(document_member(object,"object_type"));
        if(!strcmp(type,"mesh_asset_instance") && (!chart || json_object_get_int(document_member(chart,"version"))!=3)) {
            if(chart)json_object_put(chart);if(sampling)json_object_put(sampling);json_object_put(graph);
            return material_abort(diagnostic,size,"Composition on meshes requires an existing named UV mapping");
        }
        if(chart && json_object_get_int(document_member(chart,"version"))==2) {
            json_object_put(chart);if(sampling)json_object_put(sampling);json_object_put(graph);
            return material_abort(diagnostic,size,"Composition requires a planar or named UV chart; choose Planar in Coordinates first");
        }
        if(!chart)chart=json_tokener_parse("{\"version\":1,\"required_capability\":\"optic.planar_surface_v1\",\"method\":\"planar\",\"space\":\"object_rest\",\"source_domain\":\"brick_cells_v1\",\"scale_policy\":\"stretch_with_object\",\"origin_m\":[0,0,0],\"axis_u\":[1,0,0],\"axis_v\":[0,1,0],\"tile_m\":[0.5,0.25],\"offset_m\":[0,0],\"pivot_m\":[0,0],\"rotation_rad\":0,\"seed\":1729}");
        if(!sampling)sampling=json_tokener_parse("{\"version\":1,\"required_capability\":\"optic.surface_sampling_v1\",\"filter\":\"trilinear\",\"address\":\"repeat\",\"color_space\":\"linear\",\"period_tiles\":[8,8],\"normal_strength\":1,\"height_m\":0,\"channels\":{}}");
        if(!chart || !sampling){if(chart)json_object_put(chart);if(sampling)json_object_put(sampling);json_object_put(graph);return material_abort(diagnostic,size,"Cannot prepare composition mapping");}
    }
    material_clear_source(row, object);
    if(composition){json_object *ray=material_ray(object,true);json_object_object_add(ray,"surface_mapping",chart);json_object_object_add(ray,"surface_sampling",sampling);}
    json_object_object_add(row, "surface_graph", graph);
    if (!material_fresh_identity(row, preset, NULL))
        return material_abort(diagnostic, size, "cannot allocate material source identity");
    return document_finish_command(diagnostic, size);
}
static bool material_copy_member(json_object *to, json_object *from, const char *key) {
    if (!material_has(from, key))
        return true;
    json_object *value = document_member(from, key), *copy = material_clone(value);
    if (value && !copy)
        return false;
    json_object_object_add(to, key, copy);
    return true;
}
bool SceneEditorDocumentMaterialAssign(int index, int source_index, unsigned long long revision,
                                       char *diagnostic, size_t size) {
    if (!material_guard(index, revision, diagnostic, size))
        return false;
    json_object *source_object = document_object_for_scene_index(source_index, diagnostic, size);
    json_object *target_object = document_object_for_scene_index(index, diagnostic, size);
    if (!source_object || !target_object)
        return false;
    if (material_has(source_object, "procedural_solid_material_ref") ||
        material_has(target_object, "procedural_solid_material_ref")) {
        document_diag(diagnostic, size,
                      "procedural solid asset references require asset authoring; use an explicit "
                      "supported material replacement first");
        return false;
    }
    json_object *source_row = document_material_row(source_index);
    json_object *source = material_clone(source_row);
    if (source_row && !source) {
        document_diag(diagnostic, size, "failed to copy retained source");
        return false;
    }
    if (!source) {
        source = json_object_new_object();
        json_object_object_add(source, "object_id",
                               json_object_get(document_member(source_object, "object_id")));
        json_object_object_add(
            source, "material_id",
            json_object_new_int(sceneSettings.sceneObjects[source_index].material_id));
        json_object_object_add(source, "object_color",
                               json_object_new_int(sceneSettings.sceneObjects[source_index].color));
        json_object_object_add(
            source, "roughness",
            json_object_new_double(sceneSettings.sceneObjects[source_index].roughness));
        json_object_object_add(
            source, "reflectivity",
            json_object_new_double(sceneSettings.sceneObjects[source_index].reflectivity));
    }
    json_object *source_ray = material_clone(material_ray(source_object, false));
    if (material_has(source, "surface_graph") &&
        !material_supported_geometry(target_object, diagnostic, size)) {
        json_object_put(source);
        if (source_ray)
            json_object_put(source_ray);
        return false;
    }
    if (!document_begin_command(diagnostic, size)) {
        json_object_put(source);
        if (source_ray)
            json_object_put(source_ray);
        return false;
    }
    json_object *row = material_ensure_row(index);
    if (!row || !material_archive(row, target_object))
        goto failure;
    material_clear_source(row, target_object);
    for (size_t i = 0; i < sizeof(material_source_keys) / sizeof(*material_source_keys); ++i)
        if (!material_copy_member(row, source, material_source_keys[i]))
            goto failure;
    for (size_t i = 0; i < sizeof(material_response_keys) / sizeof(*material_response_keys); ++i) {
        json_object_object_del(row, material_response_keys[i]);
        if (!material_copy_member(row, source, material_response_keys[i]))
            goto failure;
    }
    /* Carry optional producer metadata without overwriting unrelated target metadata.
     * Conflicting source metadata remains recoverable in the provenance snapshot. */
    json_object_object_foreach(source, key, value) {
        (void)value;
        if (strcmp(key, "object_id") && strcmp(key, "material_authoring") &&
            !material_has(row, key))
            if (!material_copy_member(row, source, key))
                goto failure;
    }
    json_object *ray = material_ray(target_object, true);
    if (!ray || !material_copy_member(ray, source_ray, "surface_mapping") ||
        !material_copy_member(ray, source_ray, "surface_sampling"))
        goto failure;
    json_object *provenance = material_clone(source);
    if (!provenance)
        goto failure;
    json_object *source_id =
        document_member(document_member(source, "material_authoring"), "source_id");
    if (source_id)
        json_object_object_add(provenance, "origin_source_id", json_object_get(source_id));
    json_object *source_meta = material_clone(document_member(source, "material_authoring"));
    if (source_meta) {
        json_object_object_del(source_meta, "replaced_sources");
        json_object_object_add(provenance, "source_authoring", source_meta);
    }
    json_object_object_del(provenance, "material_authoring");
    bool identity_ok = material_fresh_identity(
        row, index == source_index ? "Material copy" : "Assigned material", provenance);
    json_object_put(provenance);
    if (!identity_ok)
        goto failure;
    json_object_put(source);
    if (source_ray)
        json_object_put(source_ray);
    return document_finish_command(diagnostic, size);
failure:
    json_object_put(source);
    if (source_ray)
        json_object_put(source_ray);
    return material_abort(diagnostic, size,
                          "cannot copy source while preserving material metadata");
}
bool SceneEditorDocumentMaterialDuplicate(int index, unsigned long long revision, char *diagnostic,
                                          size_t size) {
    return SceneEditorDocumentMaterialAssign(index, index, revision, diagnostic, size);
}
static json_object *material_graph_copy(int index, unsigned long long revision, char *diagnostic,
                                        size_t size) {
    if (!material_guard(index, revision, diagnostic, size))
        return NULL;
    json_object *graph = document_member(document_material_row(index), "surface_graph");
    if (!graph) {
        document_diag(diagnostic, size, "select a typed graph source for this operation");
        return NULL;
    }
    json_object *copy = material_clone(graph);
    if (!copy)
        document_diag(diagnostic, size, "failed to copy graph source");
    return copy;
}
static bool material_publish_graph(int index, json_object *graph, unsigned long long revision,
                                   char *diagnostic, size_t size) {
    bool ok = SceneEditorDocumentSetSurfaceGraph(
        index, json_object_to_json_string_ext(graph, JSON_C_TO_STRING_PLAIN), revision, diagnostic,
        size);
    json_object_put(graph);
    return ok;
}
static json_object *material_find_node(json_object *graph, const char *id, size_t *out_index) {
    json_object *nodes = document_member(graph, "nodes");
    for (size_t i = 0; id && i < material_array_size(nodes); ++i) {
        json_object *node = json_object_array_get_idx(nodes, i);
        const char *name = json_object_get_string(document_member(node, "id"));
        if (name && !strcmp(name, id)) {
            if (out_index)
                *out_index = i;
            return node;
        }
    }
    return NULL;
}
bool SceneEditorDocumentSurfaceGraphAddNode(int index, const char *node_json,
                                            unsigned long long revision, char *diagnostic,
                                            size_t size) {
    json_object *graph = material_graph_copy(index, revision, diagnostic, size);
    if (!graph)
        return false;
    json_object *node = node_json ? json_tokener_parse(node_json) : NULL;
    if (!json_object_is_type(node, json_type_object)) {
        if (node)
            json_object_put(node);
        json_object_put(graph);
        document_diag(diagnostic, size, "provide a typed node object with a unique ID");
        return false;
    }
    json_object_array_add(document_member(graph, "nodes"), node);
    return material_publish_graph(index, graph, revision, diagnostic, size);
}
bool SceneEditorDocumentSurfaceGraphDeleteNode(int index, const char *node_id,
                                               unsigned long long revision, char *diagnostic,
                                               size_t size) {
    json_object *graph = material_graph_copy(index, revision, diagnostic, size);
    if (!graph)
        return false;
    size_t selected = 0;
    if (!material_find_node(graph, node_id, &selected)) {
        json_object_put(graph);
        document_diag(diagnostic, size, "node ID is missing");
        return false;
    }
    json_object *nodes = document_member(graph, "nodes"),
                *outputs = document_member(graph, "outputs");
    char dependents[512] = {0};
    size_t used = 0;
    for (size_t i = 0; i < material_array_size(nodes); ++i) {
        json_object *node = json_object_array_get_idx(nodes, i),
                    *inputs = document_member(node, "inputs");
        for (size_t port = 0; port < material_array_size(inputs); ++port) {
            const char *source = json_object_get_string(json_object_array_get_idx(inputs, port));
            if (source && !strcmp(source, node_id) && used < sizeof(dependents)) {
                int count = snprintf(dependents + used, sizeof(dependents) - used,
                                     "%s%s.inputs[%zu]", used ? ", " : "",
                                     json_object_get_string(document_member(node, "id")), port);
                if (count > 0)
                    used += (size_t)count;
            }
        }
    }
    json_object_object_foreach(outputs, key, value) {
        const char *source = json_object_get_string(value);
        if (source && !strcmp(source, node_id) && used < sizeof(dependents)) {
            int count = snprintf(dependents + used, sizeof(dependents) - used, "%soutputs.%s",
                                 used ? ", " : "", key);
            if (count > 0)
                used += (size_t)count;
        }
    }
    json_object *regions=document_member(graph,"regions");
    for(size_t i=0;i<material_array_size(regions);++i) {
        json_object *region=json_object_array_get_idx(regions,i),*ports=document_member(region,"outputs");
        if(ports){json_object_object_foreach(ports,region_key,region_value) {
            (void)region_key;
            const char *source=json_object_get_string(region_value);
            if(source && !strcmp(source,node_id)){json_object_put(graph);document_diag(diagnostic,size,"Node is referenced by a face output; reset or rewire the face first");return false;}
        }}
    }
    if (used) {
        if (diagnostic && size)
            snprintf(diagnostic, size, "cannot delete %s; referenced by %s", node_id, dependents);
        json_object_put(graph);
        return false;
    }
    json_object_array_del_idx(nodes, selected, 1);
    return material_publish_graph(index, graph, revision, diagnostic, size);
}
static int material_named_port(json_object *node, const char *port) {
    const char *kind = json_object_get_string(document_member(node, "kind"));
    if (!kind || !port)
        return -1;
    if ((!strcmp(kind, "noise3d") || !strcmp(kind, "triplanar_checker")) &&
        !strcmp(port, "coordinates"))
        return 0;
    if (!strcmp(kind, "multiply") || !strcmp(kind, "mix") || !strcmp(kind,"roughness_mix")) {
        if (!strcmp(port, "a"))
            return 0;
        if (!strcmp(port, "b"))
            return 1;
        if (!strcmp(kind, "mix") && !strcmp(port, "factor"))
            return 2;
        if(!strcmp(kind,"roughness_mix") && !strcmp(port,"mask"))return 2;
    }
    return -1;
}
bool SceneEditorDocumentSurfaceGraphConnect(int index, const char *node_id, const char *port,
                                            const char *source_node_id, unsigned long long revision,
                                            char *diagnostic, size_t size) {
    json_object *graph = material_graph_copy(index, revision, diagnostic, size);
    if (!graph)
        return false;
    json_object *node = material_find_node(graph, node_id, NULL);
    int input = material_named_port(node, port);
    if (input < 0 || !material_find_node(graph, source_node_id, NULL)) {
        json_object_put(graph);
        document_diag(diagnostic, size, "select an existing node and a supported named input port");
        return false;
    }
    json_object_array_put_idx(document_member(node, "inputs"), (size_t)input,
                              json_object_new_string(source_node_id));
    return material_publish_graph(index, graph, revision, diagnostic, size);
}
bool SceneEditorDocumentSurfaceGraphSetOutput(int index, const char *output,
                                              const char *source_node_id,
                                              unsigned long long revision, char *diagnostic,
                                              size_t size) {
    json_object *graph = material_graph_copy(index, revision, diagnostic, size);
    if (!graph)
        return false;
    if (!output || (strcmp(output, "base_color") && strcmp(output, "roughness")) ||
        (!source_node_id && strcmp(output, "roughness")) ||
        (source_node_id && !material_find_node(graph, source_node_id, NULL))) {
        json_object_put(graph);
        document_diag(diagnostic, size,
                      "select base_color or roughness and an existing compatible node");
        return false;
    }
    json_object *outputs = document_member(graph, "outputs");
    if (source_node_id)
        json_object_object_add(outputs, output, json_object_new_string(source_node_id));
    else
        json_object_object_del(outputs, output);
    return material_publish_graph(index, graph, revision, diagnostic, size);
}
bool SceneEditorDocumentSurfaceGraphSetRegionOutput(int index, const char *face_role,
    const char *output, const char *source_node_id, unsigned long long revision,
    char *diagnostic, size_t size) {
    json_object *graph=material_graph_copy(index,revision,diagnostic,size);
    if(!graph)return false;
    json_object *object=document_object_for_scene_index(index,diagnostic,size);
    const char *type=json_object_get_string(document_member(object,"object_type"));
    const char *roles[]={"front","back","left","right","top","bottom"};
    int count=type && !strcmp(type,"rect_prism_primitive")?6:type && !strcmp(type,"plane_primitive")?1:0;
    bool valid=false;for(int i=0;i<count;++i)if(face_role && !strcmp(face_role,roles[i]))valid=true;
    if(!material_composition(graph) || !valid || !output || (strcmp(output,"base_color") && strcmp(output,"roughness"))) {
        json_object_put(graph);document_diag(diagnostic,size,"Choose a supported primitive face and composition output");return false;
    }
    if(source_node_id && !material_find_node(graph,source_node_id,NULL)) {
        json_object_put(graph);document_diag(diagnostic,size,"Output node does not exist");return false;
    }
    json_object *regions=document_member(graph,"regions"),*region=NULL;size_t at=0;
    for(;at<material_array_size(regions);++at){json_object *item=json_object_array_get_idx(regions,at);
        const char *role=json_object_get_string(document_member(item,"face_role"));
        if(role && !strcmp(role,face_role)){region=item;break;}}
    if(!region && source_node_id){
        if(!regions){regions=json_object_new_array();json_object_object_add(graph,"regions",regions);}
        region=json_object_new_object();json_object_object_add(region,"face_role",json_object_new_string(face_role));
        json_object_object_add(region,"outputs",json_object_new_object());json_object_array_add(regions,region);
    }
    if(region){json_object *outputs=document_member(region,"outputs");
        if(source_node_id)json_object_object_add(outputs,output,json_object_new_string(source_node_id));
        else json_object_object_del(outputs,output);
        if(!json_object_object_length(outputs))json_object_array_del_idx(regions,at,1);
        if(!json_object_array_length(regions))json_object_object_del(graph,"regions");}
    return material_publish_graph(index,graph,revision,diagnostic,size);
}
/* Source reset changes only known response/source parameters, never coordinates,
 * graph wiring, stable IDs, or optional producer metadata. */
bool SceneEditorDocumentMaterialResetSource(int index, unsigned long long revision,
                                            char *diagnostic, size_t size) {
    if (!material_guard(index, revision, diagnostic, size))
        return false;
    json_object *row = document_material_row(index);
    json_object *graph = document_member(row, "surface_graph");
    if (graph) {
        json_object *copy = material_clone(graph);
        if (!copy)
            return false;
        json_object *defaults = material_preset("noise3d");
        json_object *nodes = document_member(copy, "nodes");
        for (size_t i = 0; i < material_array_size(nodes); ++i) {
            json_object *node = json_object_array_get_idx(nodes, i);
            const char *kind = json_object_get_string(document_member(node, "kind"));
            const char *id = json_object_get_string(document_member(node, "id"));
            json_object *preset = material_find_node(defaults, id, NULL);
            if (!strcmp(kind, "scalar"))
                json_object_object_add(node, "value", json_object_new_double(.65));
            else if (!strcmp(kind, "color")) {
                json_object *value =
                    preset ? material_clone(document_member(preset, "value")) : NULL;
                if (!value || !json_object_is_type(value, json_type_array)) {
                    if (value)
                        json_object_put(value);
                    value = json_tokener_parse("[0.5,0.5,0.5]");
                }
                json_object_object_add(node, "value", value);
            } else if (!strcmp(kind, "noise3d"))
                json_object_object_add(node, "seed", json_object_new_int(17));
            else if (!strcmp(kind, "triplanar_checker"))
                json_object_object_add(node, "sharpness", json_object_new_double(2));
        }
        json_object_put(defaults);
        return material_publish_graph(index, copy, revision, diagnostic, size);
    }
    /* Retained stack/legacy graph reset is deliberately response-only. Pattern
     * family-specific parameters and pinned image/asset references stay intact. */
    if (!document_begin_command(diagnostic, size))
        return false;
    row = material_ensure_row(index);
    if (!row)
        return material_abort(diagnostic, size, "cannot find material response");
    json_object_object_add(row, "roughness", json_object_new_double(.65));
    json_object_object_add(row, "reflectivity", json_object_new_double(.35));
    json_object_object_add(row, "alpha", json_object_new_double(1));
    json_object_object_add(row, "emissive_strength", json_object_new_double(0));
    json_object *source = document_member(row, "material_texture_stack");
    if (!source)
        source = document_member(row, "materialTextureStack");
    json_object *layers = document_member(source, "layers");
    source = document_member(row, "material_graph");
    if (!source)
        source = document_member(row, "materialGraph");
    bool is_graph = source != NULL;
    if (is_graph)
        layers = document_member(source, "nodes");
    for (size_t i = 0; i < material_array_size(layers); ++i) {
        json_object *layer = json_object_array_get_idx(layers, i);
        if (is_graph)
            layer = document_member(layer, "layer");
        if (!layer)
            continue;
        json_object_object_add(layer, "opacity", json_object_new_double(1));
        const char *influences[] = {"roughness_influence", "reflectivity_influence",
                                    "specular_influence", "diffuse_influence"};
        for (size_t j = 0; j < sizeof(influences) / sizeof(*influences); ++j)
            if (material_has(layer, influences[j]))
                json_object_object_add(layer, influences[j], json_object_new_double(0));
    }
    return document_finish_command(diagnostic, size);
}
bool SceneEditorDocumentMaterialResetMapping(int index, unsigned long long revision,
                                             char *diagnostic, size_t size) {
    if (!material_guard(index, revision, diagnostic, size))
        return false;
    json_object *graph = document_member(document_material_row(index), "surface_graph");
    if (graph && !material_composition(graph)) {
        json_object *copy = material_clone(graph);
        if (!copy)
            return false;
        bool found = false;
        json_object *nodes = document_member(copy, "nodes");
        for (size_t i = 0; i < material_array_size(nodes); ++i) {
            json_object *node = json_object_array_get_idx(nodes, i);
            const char *kind = json_object_get_string(document_member(node, "kind"));
            if (kind && !strcmp(kind, "coordinate")) {
                found = true;
                json_object_object_add(node, "space", json_object_new_string("object_rest"));
                json_object_object_add(node, "scale_m", json_object_new_double(.2));
                json_object_object_add(node, "offset", json_tokener_parse("[0,0,0]"));
            }
        }
        if (!found) {
            json_object_put(copy);
            document_diag(diagnostic, size, "this source has no coordinate nodes to reset");
            return false;
        }
        return material_publish_graph(index, copy, revision, diagnostic, size);
    }
    json_object *object = document_object_for_scene_index(index, diagnostic, size);
    json_object *map = document_member(material_ray(object, false), "surface_mapping");
    if (!map) {
        document_diag(diagnostic, size, "this material has no explicit mapping to reset");
        return false;
    }
    if (!document_begin_command(diagnostic, size))
        return false;
    /* Chart identity, physical tile sizing and axes remain unchanged, including
     * axial/image period requirements. Reset only the chart's placement transform. */
    if (material_has(map, "rotation_rad"))
        json_object_object_add(map, "rotation_rad", json_object_new_double(0));
    if (material_has(map, "offset_m"))
        json_object_object_add(map, "offset_m", json_tokener_parse("[0,0]"));
    if (material_has(map, "pivot_m"))
        json_object_object_add(map, "pivot_m", json_tokener_parse("[0,0]"));
    if (material_has(map, "uv_scale"))
        json_object_object_add(map, "uv_scale", json_tokener_parse("[1,1]"));
    if (material_has(map, "uv_offset"))
        json_object_object_add(map, "uv_offset", json_tokener_parse("[0,0]"));
    return document_finish_command(diagnostic, size);
}
