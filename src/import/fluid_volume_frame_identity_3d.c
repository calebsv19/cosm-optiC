#include "import/fluid_volume_import_3d.h"
#include "fluid_volume_header_3d.h"
#include "core_pack.h"
#include <stdio.h>

bool fluid_volume_import_3d_single_frame_index(const char* path,RuntimeVolume3DSourceKind kind,uint64_t* out) {
    if(!path || !out) return false;
    if(kind==RUNTIME_VOLUME_3D_SOURCE_RAW_VF3D) {
        VolumeFrameHeaderVf3dV1 header;FILE* file=fopen(path,"rb");if(!file) return false;
        bool ok=fread(&header,sizeof(header),1,file)==1;fclose(file);
        if(!ok || header.magic!=(('V'<<24)|('F'<<16)|('3'<<8)|'D') || header.version!=1) return false;
        *out=header.frame_index;return true;
    }
    if(kind==RUNTIME_VOLUME_3D_SOURCE_PACK) {
        CorePackReader reader={0};CorePackChunkInfo chunk;Vf3dHeaderCanonical header;
        if(core_pack_reader_open(path,&reader).code!=CORE_OK) return false;
        bool ok=core_pack_reader_find_chunk(&reader,"VF3H",0,&chunk).code==CORE_OK &&
            chunk.size==sizeof(header) && core_pack_reader_read_chunk_data(&reader,&chunk,&header,sizeof(header)).code==CORE_OK;
        if(core_pack_reader_close(&reader).code!=CORE_OK) ok=false;
        if(!ok || header.version!=1) return false;
        *out=header.frame_index;return true;
    }
    return false;
}
