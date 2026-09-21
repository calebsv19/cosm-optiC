/* Included by runtime_document.c: shared optional surface attribute ownership. */
static CoreObjectVec3 surface_sub(CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){a.x-b.x,a.y-b.y,a.z-b.z};
}
static double surface_dot(CoreObjectVec3 a, CoreObjectVec3 b) {
    return a.x*b.x+a.y*b.y+a.z*b.z;
}
static CoreObjectVec3 surface_cross(CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
static CoreObjectVec3 surface_unit(CoreObjectVec3 v) {
    double n=hypot(hypot(v.x,v.y),v.z);
    return n>1e-15 && isfinite(n)?(CoreObjectVec3){v.x/n,v.y/n,v.z/n}:(CoreObjectVec3){0};
}
CoreResult core_mesh_asset_surface_allocate(CoreMeshAssetRuntimeDocument *d,const char *id) {
    if(!d || !id || !id[0] || strlen(id)>=sizeof(d->uv_set_id) ||
       !d->triangle_count || d->triangle_count>SIZE_MAX/(3*sizeof(CoreMeshAssetSurfaceCorner)))
        return runtime_doc_invalid_arg("invalid UV set identity or corner count");
    CoreMeshAssetSurfaceCorner *p=calloc(d->triangle_count*3,sizeof(*p));
    if(!p) return (CoreResult){CORE_ERR_OUT_OF_MEMORY,"surface corners allocation failed"};
    core_free(d->surface_corners);d->surface_corners=p;d->surface_corner_count=d->triangle_count*3;
    memcpy(d->uv_set_id,id,strlen(id)+1);return core_result_ok();
}
CoreResult core_mesh_asset_surface_validate(const CoreMeshAssetRuntimeDocument *d) {
    if(!d) return runtime_doc_invalid_arg("null surface document");
    if(!d->surface_corner_count) {
        if(d->surface_corners || d->uv_set_id[0]) return runtime_doc_invalid_arg("incomplete surface attributes");
        return core_result_ok();
    }
    if(!d->surface_corners || !d->uv_set_id[0] || !memchr(d->uv_set_id,0,sizeof(d->uv_set_id)) ||
       d->triangle_count>SIZE_MAX/3 || d->surface_corner_count!=d->triangle_count*3)
        return runtime_doc_invalid_arg("surface corners must match triangle corners");
    for(size_t i=0;i<d->surface_corner_count;++i) {
        const CoreMeshAssetSurfaceCorner *c=&d->surface_corners[i];
        if(!isfinite(c->uv[0]) || !isfinite(c->uv[1]) || !runtime_doc_vec3_is_finite(c->normal) ||
           fabs(surface_dot(c->normal,c->normal)-1)>1e-6 || !runtime_doc_vec3_is_finite(c->tangent) || !isfinite(c->handedness))
            return runtime_doc_invalid_arg("invalid surface coordinate or normal");
        if(c->tangent_valid) {
            if(fabs(surface_dot(c->tangent,c->tangent)-1)>1e-6 || fabs(surface_dot(c->normal,c->tangent))>1e-6 ||
               (c->handedness!=1 && c->handedness!=-1)) return runtime_doc_invalid_arg("invalid tangent frame");
        } else if(surface_dot(c->tangent,c->tangent)!=0 || c->handedness!=0)
            return runtime_doc_invalid_arg("invalid tangent must have zero frame");
    }
    return core_result_ok();
}
CoreResult core_mesh_asset_surface_generate_tangents(CoreMeshAssetRuntimeDocument *d) {
    if(!d || !d->vertices || !d->triangles || !d->surface_corners || d->triangle_count>SIZE_MAX/3 || d->surface_corner_count!=d->triangle_count*3)
        return runtime_doc_invalid_arg("surface corners are not allocated");
    for(size_t t=0;t<d->triangle_count;++t) {
        const CoreMeshAssetRuntimeTriangle *tri=&d->triangles[t];size_t ids[3]={tri->a,tri->b,tri->c};
        if(ids[0]>=d->vertex_count || ids[1]>=d->vertex_count || ids[2]>=d->vertex_count)
            return runtime_doc_invalid_arg("surface triangle index invalid");
        CoreObjectVec3 e1=surface_sub(d->vertices[ids[1]].position,d->vertices[ids[0]].position);
        CoreObjectVec3 e2=surface_sub(d->vertices[ids[2]].position,d->vertices[ids[0]].position);
        CoreObjectVec3 face=surface_unit(surface_cross(e1,e2));
        CoreMeshAssetSurfaceCorner *c=&d->surface_corners[3*t];
        double du1=c[1].uv[0]-c[0].uv[0],dv1=c[1].uv[1]-c[0].uv[1];
        double du2=c[2].uv[0]-c[0].uv[0],dv2=c[2].uv[1]-c[0].uv[1];
        double det=du1*dv2-du2*dv1;
        double scale=fmax(fabs(du1*dv2),fabs(du2*dv1));
        bool regular=isfinite(det) && fabs(det)>fmax(1e-30,scale*1e-12);
        CoreObjectVec3 tangent={0},bitangent={0};
        if(regular) {
            tangent=(CoreObjectVec3){(e1.x*dv2-e2.x*dv1)/det,(e1.y*dv2-e2.y*dv1)/det,(e1.z*dv2-e2.z*dv1)/det};
            bitangent=(CoreObjectVec3){(e2.x*du1-e1.x*du2)/det,(e2.y*du1-e1.y*du2)/det,(e2.z*du1-e1.z*du2)/det};
        }
        for(size_t k=0;k<3;++k) {
            CoreObjectVec3 n=c[k].normal;
            if(surface_dot(n,n)==0) n=d->vertex_normal_count?d->vertices[ids[k]].normal:face;
            n=surface_unit(n);c[k].normal=n;c[k].tangent_valid=false;c[k].tangent=(CoreObjectVec3){0};c[k].handedness=0;
            if(regular) {
                double dot=surface_dot(tangent,n);
                CoreObjectVec3 tx=surface_unit((CoreObjectVec3){tangent.x-dot*n.x,tangent.y-dot*n.y,tangent.z-dot*n.z});
                if(surface_dot(tx,tx)>.5) {
                    c[k].tangent=tx;c[k].tangent_valid=true;
                    c[k].handedness=surface_dot(surface_cross(n,tx),bitangent)<0?-1:1;
                }
            }
        }
    }
    return core_mesh_asset_surface_validate(d);
}
static bool surface_write(FILE *f,const CoreMeshAssetRuntimeDocument *d) {
    if(!d->surface_corner_count) return true;
    if(fprintf(f,",\"surface_attributes\":{\"version\":1,\"uv_set_id\":")<0 ||
       !runtime_doc_write_json_string(f,d->uv_set_id) || fprintf(f,",\"corners\":[")<0) return false;
    for(size_t i=0;i<d->surface_corner_count;++i) {
        const CoreMeshAssetSurfaceCorner *c=&d->surface_corners[i];
        if(fprintf(f,"%s{\"uv\":[%.17g,%.17g],\"normal\":",i?",":"",c->uv[0],c->uv[1])<0 ||
           !runtime_doc_write_vec3(f,c->normal) || fprintf(f,",\"tangent\":")<0 || !runtime_doc_write_vec3(f,c->tangent) ||
           fprintf(f,",\"handedness\":%.17g,\"tangent_valid\":%s}",c->handedness,c->tangent_valid?"true":"false")<0) return false;
    }
    return fprintf(f,"]}")>=0;
}
static CoreResult surface_read(const cJSON *mesh,CoreMeshAssetRuntimeDocument *d) {
    const cJSON *s=cJSON_GetObjectItemCaseSensitive(mesh,"surface_attributes");if(!s) return core_result_ok();
    const cJSON *version=cJSON_GetObjectItemCaseSensitive(s,"version"),*id=cJSON_GetObjectItemCaseSensitive(s,"uv_set_id");
    const cJSON *corners=cJSON_GetObjectItemCaseSensitive(s,"corners");
    if(!cJSON_IsNumber(version) || version->valuedouble!=1 || !cJSON_IsString(id) || !cJSON_IsArray(corners) ||
       d->triangle_count>INT_MAX/3 || (size_t)cJSON_GetArraySize(corners)!=d->triangle_count*3)
        return runtime_doc_invalid_arg("unsupported/incomplete surface attributes");
    CoreResult r=core_mesh_asset_surface_allocate(d,id->valuestring);if(r.code!=CORE_OK) return r;
    size_t i=0;const cJSON *item;
    cJSON_ArrayForEach(item,corners) {
        const cJSON *uv=cJSON_GetObjectItemCaseSensitive(item,"uv"),*sign=cJSON_GetObjectItemCaseSensitive(item,"handedness"),*valid=cJSON_GetObjectItemCaseSensitive(item,"tangent_valid");
        const cJSON *u=cJSON_GetArrayItem(uv,0),*v=cJSON_GetArrayItem(uv,1);
        if(!cJSON_IsArray(uv) || cJSON_GetArraySize(uv)!=2 || !cJSON_IsNumber(u) || !cJSON_IsNumber(v) ||
           !cJSON_IsNumber(sign) || !cJSON_IsBool(valid) ||
           !runtime_doc_vec3_from_json(cJSON_GetObjectItemCaseSensitive(item,"normal"),&d->surface_corners[i].normal) ||
           !runtime_doc_vec3_from_json(cJSON_GetObjectItemCaseSensitive(item,"tangent"),&d->surface_corners[i].tangent))
            return runtime_doc_invalid_arg("malformed surface corner");
        d->surface_corners[i].uv[0]=u->valuedouble;d->surface_corners[i].uv[1]=v->valuedouble;
        d->surface_corners[i].handedness=sign->valuedouble;d->surface_corners[i].tangent_valid=cJSON_IsTrue(valid);++i;
    }
    return core_mesh_asset_surface_validate(d);
}
