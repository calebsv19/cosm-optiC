/* Constant resource bytes provide an oracle independent of sampler and graph math. */
static void composition_t3_inputs(const HitInfo3D* hit,const CoreSurfaceGraph* graph,CoreSurfaceGraphInputs* expected) {
    CoreSurfaceGraphInputs actual={0};memset(expected,0,sizeof(*expected));
    assert(RuntimeSurfaceSamplingGraphInputs(hit,graph,&actual));
    for(size_t i=0;i<graph->count;++i) {
        const CoreSurfaceGraphNode* node=&graph->nodes[i];
        if(node->kind!=CORE_SG_IMAGE_COLOR && node->kind!=CORE_SG_IMAGE_SCALAR)continue;
        CoreSurfaceGraphInput* e=&expected->nodes[i];e->valid=true;e->kind=node->kind;
        if(node->kind==CORE_SG_IMAGE_COLOR) {e->value[0]=64./255;e->value[1]=128./255;e->value[2]=192./255;}
        else e->value[0]=node->resource==CORE_SG_RESOURCE_BASE_COLOR_ALPHA?128./255:153./255;
        assert(actual.nodes[i].valid && actual.nodes[i].kind==e->kind);
        for(int a=0;a<(node->kind==CORE_SG_IMAGE_COLOR?3:1);++a)
            assert(isfinite(actual.nodes[i].value[a]) && fabs(actual.nodes[i].value[a]-e->value[a])<2e-6);
    }
}
static CoreSurfaceGraphResult composition_t3_position_oracle(const HitInfo3D* hit,const RuntimeTriangle3D* tri,const double w[3],const CoreSurfaceGraph* graph,const CoreSurfaceGraphInputs* inputs) {
    const CoreMeshAssetRuntimeDocument* asset=&ray_tracing_runtime_mesh_assets_last()->assets[0].document;
    CoreMeshAssetRuntimeTriangle source=asset->triangles[tri->localTriangleIndex];
    size_t indices[]={source.a,source.b,source.c};
    CoreSurfaceGraphQuery q={0};q.position[1][0]=hit->position.x;q.position[1][1]=hit->position.y;q.position[1][2]=hit->position.z;
    /* Asset barycentrics reconstruct rest meters independently of the runtime
     * graph's inverse transform. A point query avoids sharing derivative logic. */
    for(int k=0;k<3;++k) {
        CoreObjectVec3 p=asset->vertices[indices[k]].position;
        q.position[0][0]+=w[k]*p.x;q.position[0][1]+=w[k]*p.y;q.position[0][2]+=w[k]*p.z;
    }
    CoreSurfaceGraphResult result;assert(core_surface_graph_evaluate_with_inputs(graph,&q,inputs,&result));return result;
}
static double composition_t3_height_pixel(int i) {
    i%=64;if(i<0)i+=64;return floor(i*255./63+.5)/255;
}
static double composition_t3_height_sample(double u) {
    double x=(u-floor(u))*64-.5,f=x-floor(x);int i=(int)floor(x);
    return composition_t3_height_pixel(i)*(1-f)+composition_t3_height_pixel(i+1)*f;
}
static void composition_t3_height_oracle(HitInfo3D hit) {
    hit.hasPixelFootprint=false;hit.footprintUnbounded=false;
    RuntimeMaterialPayload3D p;assert(RuntimeMaterialPayload3D_ResolveFromHit(&hit,&p));
    double step=1./64;
    double slope=.1*(composition_t3_height_sample(hit.surfaceUV[0]+step)-composition_t3_height_sample(hit.surfaceUV[0]-step))/(2*step);
    Vec3 n=hit.shadingNormal;
    Vec3 u=vec3_sub(hit.surfaceDpDu,vec3_scale(n,vec3_dot(hit.surfaceDpDu,n)));
    Vec3 v=vec3_sub(hit.surfaceDpDv,vec3_scale(n,vec3_dot(hit.surfaceDpDv,n)));
    /* Independent geometric construction: the normal of displaced tangents. */
    Vec3 expected=vec3_normalize(vec3_cross(vec3_add(u,vec3_scale(n,slope)),v));
    if(vec3_dot(expected,n)<0)expected=vec3_scale(expected,-1);
    assert(p.hasMicrodetailNormal && vec3_length(vec3_sub(expected,p.microdetailShadingNormal))<3e-6);
    HitInfo3D applied=hit;assert(RuntimeMaterialPayload3D_ApplyShadingNormal(&p,&applied));
    RuntimeMaterialPayload3D again;assert(RuntimeMaterialPayload3D_ResolveFromHit(&applied,&again));
    assert(again.hasMicrodetailNormal && vec3_length(vec3_sub(expected,again.microdetailShadingNormal))<3e-6);
}
/* Same normal bytes must have separate cached decoder identities: legacy M5
 * maps 128 to 1/255; T3 maps it to exactly zero. Reloading either must reuse its
 * own immutable resource without leaking the other interpretation. */
static Vec3 composition_t3_cached_normal(bool composition) {
    RuntimeScene3D scene;RuntimeScene3D_Init(&scene);assert(RuntimeScene3DBuilder_BuildFromBridgeSeeds(&scene));
    assert(scene.triangleMesh.triangleCount>0);const RuntimeTriangle3D* tri=&scene.triangleMesh.triangles[0];
    Vec3 point=vec3_scale(vec3_add(vec3_add(tri->p0,tri->p1),tri->p2),1./3);
    Vec3 face=vec3_normalize(vec3_cross(vec3_sub(tri->p1,tri->p0),vec3_sub(tri->p2,tri->p0)));
    Ray3D ray=RuntimeRay3D_Make(vec3_add(point,vec3_scale(face,2)),vec3_scale(face,-1));
    HitInfo3D hit;assert(RuntimeRay3D_TraceSceneFirstHit(&scene,&ray,1e-5,4,&hit));
    RuntimeMaterialPayload3D p;assert(RuntimeMaterialPayload3D_ResolveFromHit(&hit,&p));assert(p.hasMicrodetailNormal);
    Vec3 n=hit.shadingNormal,t=vec3_normalize(vec3_sub(hit.surfaceDpDu,vec3_scale(n,vec3_dot(hit.surfaceDpDu,n))));
    Vec3 b=vec3_scale(vec3_cross(n,t),vec3_dot(vec3_cross(n,t),hit.surfaceDpDv)<0?-1:1);
    Vec3 expected=vec3_normalize(vec3_add(vec3_add(vec3_scale(t,composition?64./127:129./255),vec3_scale(b,composition?32./127:65./255)),vec3_scale(n,221./255)));
    assert(vec3_length(vec3_sub(p.microdetailShadingNormal,expected))<2e-6);
    Vec3 result=p.microdetailShadingNormal;RuntimeScene3D_Free(&scene);return result;
}
static void composition_t3_decoder_cache(void) {
    char source[PATH_MAX],legacy[PATH_MAX];snprintf(source,sizeof(source),"%s",SceneEditorDocumentPath());
    snprintf(legacy,sizeof(legacy),"%s",source);char* slash=strrchr(legacy,'/');assert(slash);
    snprintf(slash+1,sizeof(legacy)-(size_t)(slash+1-legacy),"t3-legacy-cache-probe.json");
    json_object *root=json_object_from_file(source),*extensions=NULL,*ray=NULL,*authoring=NULL,*rows=NULL;assert(root);
    assert(json_object_object_get_ex(root,"extensions",&extensions));assert(json_object_object_get_ex(extensions,"ray_tracing",&ray));
    assert(json_object_object_get_ex(ray,"authoring",&authoring));assert(json_object_object_get_ex(authoring,"object_materials",&rows));
    json_object* row=json_object_array_get_idx(rows,0);assert(row);json_object_object_del(row,"surface_graph");
    json_object* stack=json_tokener_parse("{\"layers\":[{\"id\":\"legacy\",\"kind\":\"brick\",\"placement\":{\"scale\":1,\"strength\":1}}]}");assert(stack);
    json_object_object_add(row,"material_texture_stack",stack);
    assert(!json_object_to_file_ext(legacy,root,JSON_C_TO_STRING_PRETTY));json_object_put(root);
    RuntimeSurfaceSamplingCacheStats before,first,reused;RuntimeSurfaceSamplingGetCacheStats(&before);
    RuntimeSceneBridgePreflight summary={0};
    assert(runtime_scene_bridge_apply_file(legacy,&summary));Vec3 m5=composition_t3_cached_normal(false);
    RuntimeSurfaceSamplingGetCacheStats(&first);
    assert(first.decodes==before.decodes+1 && first.image_builds==before.image_builds+1);
    assert(runtime_scene_bridge_apply_file(source,&summary));Vec3 t3=composition_t3_cached_normal(true);
    assert(vec3_length(vec3_sub(m5,t3))>1e-4);
    assert(runtime_scene_bridge_apply_file(legacy,&summary));Vec3 m5_again=composition_t3_cached_normal(false);
    assert(vec3_length(vec3_sub(m5_again,m5))<1e-12);
    assert(runtime_scene_bridge_apply_file(source,&summary));Vec3 t3_again=composition_t3_cached_normal(true);
    assert(vec3_length(vec3_sub(t3_again,t3))<1e-12);
    RuntimeSurfaceSamplingGetCacheStats(&reused);
    assert(reused.decodes==first.decodes && reused.image_builds==first.image_builds && reused.cache_hits>first.cache_hits);
}
