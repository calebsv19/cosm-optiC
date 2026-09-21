/* Explicit triangle OBJ adapter. UVs are corner indexed; position sharing never
 * welds UV seams. Material-library assignment is outside this geometry adapter. */
static bool obj_grow(void **p,size_t *capacity,size_t count,size_t size) {
    if(count<*capacity) return true;
    size_t n=*capacity?*capacity*2:64;
    if(n>9000000u || n>SIZE_MAX/size) return false;
    void *next=realloc(*p,n*size);if(!next) return false;*p=next;*capacity=n;return true;
}
static bool obj_index(char **text,size_t count,size_t *out) {
    char *end;long value=strtol(*text,&end,10);
    if(end==*text || !value || count>LONG_MAX) return false;
    long index=value>0?value-1:(long)count+value;
    if(index<0 || (size_t)index>=count) return false;
    *out=(size_t)index;*text=end;return true;
}
static CoreResult imported_mesh_parse_obj(const char *path,const CoreMeshAssetImportedMeshSource *source,
    CoreObjectVec3 **out_vertices,size_t *out_vertex_count,CoreMeshCompileParsedTriangle **out_triangles,
    size_t *out_triangle_count,CoreMeshAssetSurfaceCorner **out_corners) {
    FILE *f=fopen(path,"rb");if(!f) return (CoreResult){CORE_ERR_IO,"OBJ open failed"};
    CoreObjectVec3 *vertices=NULL,*uvs=NULL,*normals=NULL;
    CoreMeshCompileParsedTriangle *triangles=NULL;CoreMeshAssetSurfaceCorner *corners=NULL;
    size_t nv=0,nu=0,nn=0,nt=0,vc=0,uc=0,nc=0,tc=0,cc=0;
    char line[4096];bool ok=true;
    while(ok && fgets(line,sizeof(line),f)) {
        if(!strchr(line,'\n') && !feof(f)) {ok=false;break;}
        char *comment=strchr(line,'#');if(comment) *comment=0;
        char *text=(char*)imported_mesh_skip_space(line);if(!*text) continue;
        if((text[0]=='v' && isspace((unsigned char)text[1])) ||
           (!strncmp(text,"vt",2) && isspace((unsigned char)text[2])) ||
           (!strncmp(text,"vn",2) && isspace((unsigned char)text[2]))) {
            bool uv=text[1]=='t',normal=text[1]=='n';text+=uv||normal?2:1;
            CoreObjectVec3 v={0};double *values[3]={&v.x,&v.y,&v.z};int count=uv?2:3;
            for(int k=0;k<count;++k) {char *end;*values[k]=strtod(text,&end);if(end==text || !isfinite(*values[k])) {ok=false;break;}text=end;}
            if(*imported_mesh_skip_space(text)) ok=false;
            if(!ok) break;
            CoreObjectVec3 **array=uv?&uvs:normal?&normals:&vertices;
            size_t *n=uv?&nu:normal?&nn:&nv,*cap=uv?&uc:normal?&nc:&vc;
            if(!obj_grow((void**)array,cap,*n,sizeof(**array))) {ok=false;break;}
            if(!uv && !normal) {v.x*=source->source_to_asset_scale;v.y*=source->source_to_asset_scale;v.z*=source->source_to_asset_scale;}
            (*array)[(*n)++]=v;
        } else if(text[0]=='f' && isspace((unsigned char)text[1])) {
            ++text;size_t positions[3];CoreMeshAssetSurfaceCorner face[3]={0};
            for(int k=0;k<3;++k) {
                text=(char*)imported_mesh_skip_space(text);size_t ui,ni;
                if(!obj_index(&text,nv,&positions[k]) || *text++!='/' || !obj_index(&text,nu,&ui)) {ok=false;break;}
                face[k].uv[0]=uvs[ui].x;face[k].uv[1]=uvs[ui].y;
                if(*text=='/') {++text;if(!obj_index(&text,nn,&ni)) {ok=false;break;}if(source->preserve_source_normals) face[k].normal=normals[ni];}
                if(*text && !isspace((unsigned char)*text)) {ok=false;break;}
            }
            if(!ok || *imported_mesh_skip_space(text) || nt>=CORE_MESH_COMPILE_IMPORTED_STL_MAX_TRIANGLES) {ok=false;break;}
            if(!obj_grow((void**)&triangles,&tc,nt,sizeof(*triangles)) || !obj_grow((void**)&corners,&cc,nt,sizeof(*corners)*3)) {ok=false;break;}
            triangles[nt]=(CoreMeshCompileParsedTriangle){positions[0],positions[1],positions[2]};
            memcpy(corners+3*nt,face,sizeof(face));++nt;
        } else if(!strncmp(text,"o ",2) || !strncmp(text,"g ",2) || !strncmp(text,"s ",2) ||
                  !strncmp(text,"usemtl ",7) || !strncmp(text,"mtllib ",7)) {
            /* Geometry-only import uses the explicitly supplied default surface group. */
        } else ok=false;
    }
    if(ferror(f)) ok=false;fclose(f);free(uvs);free(normals);
    if(!ok || !nv || !nt) {free(vertices);free(triangles);free(corners);return imported_mesh_invalid_arg("OBJ requires finite v/vt[/vn] triangles with valid indices; triangulate polygons before import");}
    *out_vertices=vertices;*out_triangles=triangles;*out_corners=corners;*out_vertex_count=nv;*out_triangle_count=nt;return core_result_ok();
}
