/* Resource IO belongs to the editor command boundary, never the shading path. */
static json_object* resource_array(json_object* parent,const char* key) {
    json_object* value=document_member(parent,key);
    if(value)return json_object_is_type(value,json_type_array)?value:NULL;
    value=json_object_new_array();if(value)json_object_object_add(parent,key,value);return value;
}
static const char* resource_channels[]={"base_color","roughness","normal","height"};
static int resource_channel_index(const char* name) {
    for(int i=0;name && i<4;++i)if(!strcmp(name,resource_channels[i]))return i;
    return -1;
}
static json_object* resource_sampling(int index) {
    return document_member(material_ray(document_object_for_scene_index(index,NULL,0),false),"surface_sampling");
}
size_t SceneEditorDocumentSurfaceSamplingJSONSize(int index) {
    json_object* value=resource_sampling(index);if(!value)return 0;
    const char* text=json_object_to_json_string_ext(value,JSON_C_TO_STRING_PLAIN);
    return text?strlen(text)+1:0;
}
bool SceneEditorDocumentGetSurfaceSamplingJSON(int index,char* out,size_t size) {
    json_object* value=resource_sampling(index);if(!value || !out)return false;
    const char* text=json_object_to_json_string_ext(value,JSON_C_TO_STRING_PLAIN);
    if(!text || strlen(text)>=size)return false;
    memcpy(out,text,strlen(text)+1);return true;
}
bool SceneEditorDocumentSurfaceSamplingSupported(int index,char* diagnostic,size_t size) {
    json_object* object=document_object_for_scene_index(index,diagnostic,size);
    json_object* row=document_material_row(index);
    if(!object || !document_member(material_ray(object,false),"surface_mapping")) {
        document_diag(diagnostic,size,"Choose a supported chart in Coordinates before assigning images");return false;
    }
    bool composition=material_composition(document_member(row,"surface_graph"));
    const char* rejected[]={"surface_graph","material_graph","materialGraph","authored_texture","surface_material_binding"};
    for(size_t i=0;i<sizeof(rejected)/sizeof(*rejected);++i)if(material_has(row,rejected[i]) && !(composition && !strcmp(rejected[i],"surface_graph"))) {
        document_diag(diagnostic,size,"Image channels require a mapped brick/solid stack; this source needs explicit replacement");return false;
    }
    if(material_has(object,"procedural_solid_material_ref")) {
        document_diag(diagnostic,size,"Procedural solid assets require their producer workflow");return false;
    }
    json_object* stack=document_member(row,"material_texture_stack");
    if(!stack)stack=document_member(row,"materialTextureStack");
    if(composition && stack){document_diag(diagnostic,size,"Composition cannot retain a legacy stack");return false;}
    if(!stack && !composition) {document_diag(diagnostic,size,"Image channels require a retained brick/solid stack");return false;}
    document_diag(diagnostic,size,"Supported mapped image channels");return true;
}
static json_object* resource_defaults(int index) {
    json_object* existing=resource_sampling(index);if(existing)return material_clone(existing);
    json_object* value=json_tokener_parse("{\"version\":1,\"required_capability\":\"optic.surface_sampling_v1\",\"filter\":\"trilinear\",\"address\":\"repeat\",\"color_space\":\"linear\",\"period_tiles\":[8,8],\"normal_strength\":1,\"height_m\":0,\"channels\":{}}");
    CoreAuthoredSurfaceMapping map;
    if(RuntimeSurfaceMappingDefinition(index,&map) && map.version==2)
        json_object_array_put_idx(document_member(value,"period_tiles"),0,json_object_new_int((int)round(6.2831853071795864769*map.reference_radius_m/map.tile_m[0])));
    return value;
}
bool SceneEditorDocumentSetSurfaceSampling(int index,const char* text,unsigned long long revision,char* diagnostic,size_t size) {
    if(!material_guard(index,revision,diagnostic,size))return false;
    if(!text && material_composition(document_member(document_material_row(index),"surface_graph"))){document_diag(diagnostic,size,"Composition requires image sampling; remove individual unused channels instead");return false;}
    if(text && !SceneEditorDocumentSurfaceSamplingSupported(index,diagnostic,size))return false;
    json_object* value=text?json_tokener_parse(text):NULL;
    if(text && !json_object_is_type(value,json_type_object)) {
        if(value)json_object_put(value);document_diag(diagnostic,size,"Invalid sampling declaration");return false;
    }
    if(!document_begin_command(diagnostic,size)){if(value)json_object_put(value);return false;}
    json_object* ray=material_ray(document_object_for_scene_index(index,diagnostic,size),true);
    if(!ray){if(value)json_object_put(value);return material_abort(diagnostic,size,"Cannot retain resource declaration");}
    if(value)json_object_object_add(ray,"surface_sampling",value);
    else json_object_object_del(ray,"surface_sampling");
    return document_finish_command(diagnostic,size);
}
static bool resource_path(int index,const char* path,char* out,size_t capacity) {
    (void)index;
    if(!path || !path[0])return false;
    if(path[0]=='/')return snprintf(out,capacity,"%s",path)<(int)capacity;
    /* Retained relative paths are project-relative, never working-directory relative. */
    const char* part=path;
    for(const char* p=path;;++p)if(*p=='/' || !*p) {
        if(p==part || (p-part==2 && !strncmp(part,"..",2)))return false;
        if(!*p)break;part=p+1;
    }
    char parent[PATH_MAX],root[PATH_MAX],joined[PATH_MAX],resolved[PATH_MAX];
    if(!document_parent_path(s_document.path,parent,sizeof(parent)) || !realpath(parent,root) ||
       snprintf(joined,sizeof(joined),"%s/%s",root,path)>=(int)sizeof(joined) ||
       !realpath(joined,resolved))return false;
    size_t length=strlen(root);
    if(strncmp(root,resolved,length) || (length>1 && resolved[length]!='/'))return false;
    return snprintf(out,capacity,"%s",resolved)<(int)capacity;
}
static bool resource_directory(const char* path) {
    struct stat st;
    if(mkdir(path,0755) && errno!=EEXIST)return false;
    return !lstat(path,&st) && S_ISDIR(st.st_mode);
}
static bool resource_import(const char* source,char relative[128],char destination[PATH_MAX],char digest[65],bool* created,char* diagnostic,size_t size) {
    struct stat st,existing;char parent[PATH_MAX],assets[PATH_MAX],materials[PATH_MAX],input[PATH_MAX];
    char temporary[PATH_MAX],old_digest[65]={0};bool replacing=false,retained=false;
    *created=false;
    if(!resource_path(0,source,input,sizeof(input)) || stat(input,&st) || !S_ISREG(st.st_mode) || st.st_size<=0 || st.st_size>16*1024*1024 ||
       !ray_tracing_sha256_file(input,digest)) {
        document_diag(diagnostic,size,"Select a readable PNG no larger than 16 MiB");return false;
    }
    if(!document_parent_path(s_document.path,parent,sizeof(parent)) ||
       snprintf(assets,sizeof(assets),"%s/assets",parent)>=(int)sizeof(assets) ||
       snprintf(materials,sizeof(materials),"%s/materials",assets)>=(int)sizeof(materials) ||
       !resource_directory(assets) || !resource_directory(materials)) {
        document_diag(diagnostic,size,"Cannot create project material resource directory");return false;
    }
    snprintf(relative,128,"assets/materials/%s.png",digest);
    if(snprintf(destination,PATH_MAX,"%s/%s",parent,relative)>=PATH_MAX ||
       snprintf(temporary,sizeof(temporary),"%s/.import-XXXXXX",materials)>=(int)sizeof(temporary))return false;
    if(!lstat(destination,&existing)) {
        if(!S_ISREG(existing.st_mode) || !ray_tracing_sha256_file(destination,old_digest)) {
            document_diag(diagnostic,size,"Pinned destination must be a readable regular file");return false;
        }
        if(!strcmp(old_digest,digest))return true;
        replacing=true;
    } else if(errno!=ENOENT) {
        document_diag(diagnostic,size,"Cannot inspect pinned resource destination");return false;
    }
    /* A missing file may already be referenced. Recovery must survive a later scene rejection. */
    for(int i=0;i<SceneEditorDocumentObjectCount() && !retained;++i) {
        SceneEditorDocumentObjectInfo info;if(!SceneEditorDocumentObjectAt(i,&info) || info.runtime_index<0)continue;
        json_object* channels=document_member(resource_sampling(info.runtime_index),"channels");
        for(int c=0;c<4;++c) {
            json_object* entry=document_member(channels,resource_channels[c]);
            const char *path=json_object_get_string(document_member(entry,"path")),*pin=json_object_get_string(document_member(entry,"sha256"));
            if(path && pin && !strcmp(pin,digest) && (!strcmp(path,relative) || !strcmp(path,destination))){retained=true;break;}
        }
    }
    int output=mkstemp(temporary);
    if(output<0){document_diag(diagnostic,size,"Cannot stage project material resource");return false;}
    int input_fd=open(input,O_RDONLY);bool ok=input_fd>=0;size_t total=0;
    char buffer[16384];ssize_t count=0;
    while(ok && (count=read(input_fd,buffer,sizeof(buffer)))>0) {
        total+=(size_t)count;if(total>16u*1024u*1024u){ok=false;break;}
        for(ssize_t written=0;written<count;) {
            ssize_t n=write(output,buffer+written,(size_t)(count-written));
            if(n<=0){ok=false;break;}written+=n;
        }
    }
    if(ok && count<0)ok=false;
    if(input_fd>=0)close(input_fd);
    if(fchmod(output,0644) || fsync(output))ok=false;if(close(output))ok=false;
    char actual[65];
    if(ok)ok=ray_tracing_sha256_file(temporary,actual) && !strcmp(actual,digest);
    int directory=ok?open(materials,O_RDONLY):-1;
    if(directory<0)ok=false;
    bool published=false;
    if(ok && replacing) {
        /* Do not overwrite a destination whose identity or bytes changed while copying. */
        ok=!lstat(destination,&st) && S_ISREG(st.st_mode) && st.st_dev==existing.st_dev &&
            st.st_ino==existing.st_ino && st.st_size==existing.st_size && st.st_mtime==existing.st_mtime &&
            ray_tracing_sha256_file(destination,actual) && !strcmp(actual,old_digest);
        if(ok){ok=rename(temporary,destination)==0;published=ok;}
    } else if(ok) {
        if(link(temporary,destination)==0){published=true;*created=!retained;}
        else if(errno==EEXIST)ok=!lstat(destination,&st) && S_ISREG(st.st_mode) &&
            ray_tracing_sha256_file(destination,actual) && !strcmp(actual,digest);
        else ok=false;
    }
    unlink(temporary);
    if(directory>=0){if(published && fsync(directory))ok=false;close(directory);}
    if(!ok){*created=false;document_diag(diagnostic,size,"Resource import failed, source changed, or destination changed during publication");}
    return ok;
}
bool SceneEditorDocumentSurfaceSamplingSetChannel(int index,const char* channel,const char* path,const char* encoding,unsigned long long revision,char* diagnostic,size_t size) {
    int c=resource_channel_index(channel);
    if(!material_guard(index,revision,diagnostic,size) || !SceneEditorDocumentSurfaceSamplingSupported(index,diagnostic,size))return false;
    if(c<0 || (path && (!encoding || (c==0 ? strcmp(encoding,"srgb") && strcmp(encoding,"linear") : strcmp(encoding,"data"))))) {
        document_diag(diagnostic,size,"Choose a supported channel and its color/data encoding");return false;
    }
    json_object* declaration=resource_defaults(index);
    if(!declaration){document_diag(diagnostic,size,"Cannot allocate sampling declaration");return false;}
    json_object* channels=material_ensure_object(declaration,"channels");
    if(!channels){json_object_put(declaration);document_diag(diagnostic,size,"Invalid retained channel metadata");return false;}
    if(path && ((c==2 && material_has(channels,"height")) || (c==3 && material_has(channels,"normal")))) {
        json_object_put(declaration);document_diag(diagnostic,size,"Normal and height are exclusive; remove the other channel first");return false;
    }
    char destination[PATH_MAX]={0},relative[128],digest[65];bool created=false;
    json_object* previous=document_member(channels,channel);
    if(path) {
        if(!resource_import(path,relative,destination,digest,&created,diagnostic,size)){json_object_put(declaration);return false;}
        json_object* entry=previous?material_clone(previous):json_object_new_object();
        if(!json_object_is_type(entry,json_type_object)){if(entry)json_object_put(entry);json_object_put(declaration);if(created)unlink(destination);return false;}
        if(previous) {
            json_object* history=resource_array(entry,"replaced_resources");
            json_object* archived=material_clone(previous);
            if(!history || !archived){if(archived)json_object_put(archived);json_object_put(entry);json_object_put(declaration);if(created)unlink(destination);document_diag(diagnostic,size,"Cannot preserve channel provenance");return false;}
            json_object_object_del(archived,"replaced_resources");json_object_array_add(history,archived);
        }
        json_object_object_add(entry,"path",json_object_new_string(relative));
        json_object_object_add(entry,"sha256",json_object_new_string(digest));
        json_object_object_add(entry,"color_space",json_object_new_string(encoding));
        json_object_object_add(channels,channel,entry);
    } else {
        json_object_object_del(channels,channel);
        if(c==3)json_object_object_add(declaration,"height_m",json_object_new_double(0));
    }
    bool ok=SceneEditorDocumentSetSurfaceSampling(index,json_object_to_json_string_ext(declaration,JSON_C_TO_STRING_PLAIN),revision,diagnostic,size);
    json_object_put(declaration);
    if(!ok && created)unlink(destination);
    return ok;
}
bool SceneEditorDocumentSurfaceSamplingSetValue(int index,const char* property,double value,unsigned long long revision,char* diagnostic,size_t size) {
    if(!material_guard(index,revision,diagnostic,size) || !SceneEditorDocumentSurfaceSamplingSupported(index,diagnostic,size))return false;
    bool period=property && (!strcmp(property,"period_u") || !strcmp(property,"period_v"));
    if(!isfinite(value) || !property || (period ? value<1 || value>32 || floor(value)!=value :
       (!strcmp(property,"normal_strength") ? value<0 || value>2 : !strcmp(property,"height_m") ? value<0 || value>1 : true))) {
        document_diag(diagnostic,size,"Use period 1..32 (integer), normal strength 0..2 or height 0..1 meters");return false;
    }
    json_object* declaration=resource_defaults(index);
    if(!declaration){document_diag(diagnostic,size,"Cannot allocate sampling declaration");return false;}
    if(period)json_object_array_put_idx(document_member(declaration,"period_tiles"),!strcmp(property,"period_v"),json_object_new_int((int)value));
    else json_object_object_add(declaration,property,json_object_new_double(value));
    bool ok=SceneEditorDocumentSetSurfaceSampling(index,json_object_to_json_string_ext(declaration,JSON_C_TO_STRING_PLAIN),revision,diagnostic,size);
    json_object_put(declaration);return ok;
}
bool SceneEditorDocumentSurfaceSamplingCheckChannel(int index,const char* channel,char* diagnostic,size_t size) {
    if(resource_channel_index(channel)<0)return false;
    json_object* entry=document_member(document_member(resource_sampling(index),"channels"),channel);
    const char* path=json_object_get_string(document_member(entry,"path"));
    const char* expected=json_object_get_string(document_member(entry,"sha256"));
    char resolved[PATH_MAX],actual[65];
    bool ok=path && ray_tracing_sha256_is_valid_hex(expected) && resource_path(index,path,resolved,sizeof(resolved)) && ray_tracing_sha256_file(resolved,actual) && !strcmp(actual,expected);
    document_diag(diagnostic,size,ok?"Ready: pinned image digest verified":"Missing or changed resource: select Relink to import the intended image");
    return ok;
}

static const char* resource_receipt_string(json_object* receipt,const char* name) {
    json_object* value=document_member(receipt,name);
    return json_object_is_type(value,json_type_string)?json_object_get_string(value):NULL;
}
static bool resource_receipt_true(json_object* receipt,const char* name) {
    json_object* value=document_member(receipt,name);
    return json_object_is_type(value,json_type_boolean) && json_object_get_boolean(value);
}
static bool resource_receipt_preview(json_object* receipt,json_object* validation) {
    const char* root=resource_receipt_string(receipt,"review_root");
    const char* frame=resource_receipt_string(validation,"preview_frame");
    const char* expected=resource_receipt_string(validation,"preview_sha256");
    char relative[PATH_MAX],path[PATH_MAX],canonical[PATH_MAX],parent[PATH_MAX],canonical_parent[PATH_MAX],actual[65];
    if(!root || !frame || root[0]=='/' || frame[0]=='/' ||
       !ray_tracing_sha256_is_valid_hex(expected) ||
       snprintf(relative,sizeof(relative),"%s/%s",root,frame)>=(int)sizeof(relative) ||
       !resource_path(0,relative,path,sizeof(path)) || !realpath(path,canonical) ||
       !document_parent_path(s_document.path,parent,sizeof(parent)) || !realpath(parent,canonical_parent))return false;
    size_t length=strlen(canonical_parent);
    return !strncmp(canonical,canonical_parent,length) && canonical[length]=='/' &&
        ray_tracing_sha256_file(canonical,actual) && !strcmp(actual,expected);
}
bool SceneEditorDocumentAdoptSurfaceCandidate(const char* candidate_path,
    const char* receipt_path,char* diagnostic,size_t size) {
    if(!s_document.root || !s_document.baseline_bytes || s_document.dirty) {
        document_diag(diagnostic,size,"Save the current document before reviewing and adopting a surface candidate");return false;
    }
    struct stat info;
    if(!receipt_path || stat(receipt_path,&info)!=0 || !S_ISREG(info.st_mode) ||
       info.st_size<=0 || info.st_size>65536) {
        document_diag(diagnostic,size,"Missing or oversized surface candidate review receipt");return false;
    }
    json_object* receipt=json_object_from_file(receipt_path);
    const char* schema=resource_receipt_string(receipt,"schema");
    const char* state=resource_receipt_string(receipt,"state");
    const char* source_hash=resource_receipt_string(receipt,"source_scene_sha256");
    const char* candidate_hash=resource_receipt_string(receipt,"candidate_sha256");
    json_object* validation=document_member(receipt,"validation");
    bool reviewed=schema && !strcmp(schema,"optic_surface_uv_candidate_v1") &&
        state && !strcmp(state,"reviewed_candidate_not_adopted") &&
        resource_receipt_true(receipt,"geometry_unchanged") &&
        resource_receipt_true(validation,"preflight") && resource_receipt_true(validation,"preview_render") &&
        resource_receipt_preview(receipt,validation);
    char baseline_hash[65],disk_hash[65],actual_candidate_hash[65];
    bool pins=reviewed && ray_tracing_sha256_is_valid_hex(source_hash) &&
        ray_tracing_sha256_is_valid_hex(candidate_hash) && candidate_path &&
        ray_tracing_sha256_bytes(s_document.baseline_bytes,s_document.baseline_size,baseline_hash) &&
        ray_tracing_sha256_file(s_document.path,disk_hash) &&
        !strcmp(baseline_hash,source_hash) && !strcmp(disk_hash,source_hash) &&
        ray_tracing_sha256_file(candidate_path,actual_candidate_hash) &&
        !strcmp(actual_candidate_hash,candidate_hash);
    if(!pins) {
        if(receipt)json_object_put(receipt);
        document_diag(diagnostic,size,reviewed?
            "Surface candidate is stale: source or candidate bytes no longer match the review receipt":
            "Surface candidate requires a valid reviewed receipt with successful preflight and preview render");
        return false;
    }
    bool adopted=document_adopt_candidate(candidate_path,candidate_hash,diagnostic,size);
    json_object_put(receipt);
    return adopted;
}
