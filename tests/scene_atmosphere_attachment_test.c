#include "app/scene_atmosphere_attachment.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    char d[256];
    SceneAtmosphereAttachment v, decoded;
    SceneAtmosphereAttachmentDefaults(&v); v.present=true;
    strcpy(v.source_path,"assets/atmosphere/cache/manifest.json");
    assert(SceneAtmosphereAttachmentValidate(&v,d,sizeof(d)));
    json_object* author=json_object_new_object();
    json_object_object_add(author,"atmosphere_attachment",SceneAtmosphereAttachmentToJson(&v));
    assert(SceneAtmosphereAttachmentParse(author,&decoded,d,sizeof(d)));
    assert(decoded.present && !decoded.animated && decoded.scatter_gain==1);
    assert(!strcmp(decoded.source_path,v.source_path));
    json_object* o=NULL; json_object_object_get_ex(author,"atmosphere_attachment",&o);
    SceneAtmosphereAttachment unchanged=decoded;
    json_object_object_add(o,"enabled",json_object_new_string("false"));
    assert(!SceneAtmosphereAttachmentParse(author,&decoded,d,sizeof(d)));
    assert(!memcmp(&decoded,&unchanged,sizeof(decoded)));
    json_object_object_add(o,"enabled",json_object_new_boolean(false));
    assert(SceneAtmosphereAttachmentParse(author,&decoded,d,sizeof(d)) && !decoded.enabled);
    json_object_put(author);
    const char* paths[]={"/tmp/cache.vf3d","assets/../cache.vf3d","assets//cache.vf3d","assets/./x","assets/","C:\\x",""};
    for(size_t i=0;i<sizeof(paths)/sizeof(paths[0]);++i) {
        SceneAtmosphereAttachment bad=v;snprintf(bad.source_path,sizeof(bad.source_path),"%s",paths[i]);
        assert(!SceneAtmosphereAttachmentValidate(&bad,d,sizeof(d)));
    }
    SceneAtmosphereAttachment bad=v;bad.scale[1]=0;assert(!SceneAtmosphereAttachmentValidate(&bad,d,sizeof(d)));
    bad=v;bad.density_scale=NAN;assert(!SceneAtmosphereAttachmentValidate(&bad,d,sizeof(d)));
    bad=v;bad.animated=true;strcpy(bad.source_kind,"vf3d");assert(!SceneAtmosphereAttachmentValidate(&bad,d,sizeof(d)));
    v.animated=true;v.source_rate=(TimelineRate){30000,1001};v.timeline_start=100;v.source_start=2;
    const int frames[]={100,104,102,99,105,100};
    const int expected[]={2,6,4,0,8,2};
    for(size_t i=0;i<sizeof(frames)/sizeof(frames[0]);++i) {
        TimelineEvaluationContext f;
        assert(TimelineEvaluationContextBuild((TimelineRate){24,1},(TimelineRange){0,200},
            (TimelineSample){frames[i],0,1},&f)==TIMELINE_STATUS_OK);
        int64_t mapped=-999;
        assert(SceneAtmosphereAttachmentRequestedFrame(&v,&f,&mapped,d,sizeof(d)));
        assert(mapped==expected[i]);
    }
    TimelineEvaluationContext f;
    assert(TimelineEvaluationContextBuild((TimelineRate){24,1},(TimelineRange){0,200},
        (TimelineSample){100,1,2},&f)==TIMELINE_STATUS_OK);
    int64_t mapped=-999;
    assert(SceneAtmosphereAttachmentRequestedFrame(&v,&f,&mapped,d,sizeof(d)) && mapped==2);
    v.animated=false;assert(SceneAtmosphereAttachmentRequestedFrame(&v,&f,&mapped,d,sizeof(d)) && mapped==2);
    const int64_t ids[]={0,2,4};
    const int64_t requests[]={-2,0,1,2,3,4,8};
    const int64_t selected[]={0,0,0,2,2,4,4};
    for(size_t i=0;i<7;++i) {
        assert(SceneAtmosphereAttachmentSelectFrame(&v,requests[i],ids,3,&mapped,d,sizeof(d)));
        assert(mapped==selected[i]);
    }
    v.missing_is_error=true;mapped=987;
    assert(!SceneAtmosphereAttachmentSelectFrame(&v,3,ids,3,&mapped,d,sizeof(d)) && mapped==987);
    assert(SceneAtmosphereAttachmentSelectFrame(&v,4,ids,3,&mapped,d,sizeof(d)) && mapped==4);
    const int64_t duplicate[]={0,2,2};assert(!SceneAtmosphereAttachmentSelectFrame(&v,2,duplicate,3,&mapped,d,sizeof(d)));
    f.sample.subframe_denominator=0;
    assert(!SceneAtmosphereAttachmentRequestedFrame(&v,&f,&mapped,d,sizeof(d)));
    assert(SceneAtmosphereAttachmentParse(NULL,&decoded,d,sizeof(d)) && !decoded.present);
    puts("Atmosphere attachment contract PASS: codec, refusal preservation, rational/subframe/seek mapping, sparse actual IDs");
    return 0;
}
