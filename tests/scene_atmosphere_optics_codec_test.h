static void atmosphere_optics_codec(void) {
    SceneAtmosphereAttachment v,parsed;char diagnostic[256];
    SceneAtmosphereAttachmentDefaults(&v);v.present=true;strcpy(v.source_path,"assets/cache.vf3d");strcpy(v.source_kind,"vf3d");
    json_object* root=json_object_new_object();
    json_object_object_add(root,"atmosphere_attachment",SceneAtmosphereAttachmentToJson(&v));
    assert(SceneAtmosphereAttachmentParse(root,&parsed,diagnostic,sizeof(diagnostic)) && !parsed.optics.present);
    json_object* attachment=NULL;json_object_object_get_ex(root,"atmosphere_attachment",&attachment);
    json_object_object_add(attachment,"optics",NULL);
    assert(!SceneAtmosphereAttachmentParse(root,&parsed,diagnostic,sizeof(diagnostic)));
    v.optics.present=v.optics.physical=true;v.optics.sky_error_budget=.00001;
    json_object_object_add(root,"atmosphere_attachment",SceneAtmosphereAttachmentToJson(&v));
    assert(SceneAtmosphereAttachmentParse(root,&parsed,diagnostic,sizeof(diagnostic)));
    assert(parsed.optics.physical && parsed.optics.cell_centered && parsed.optics.sky_error_budget==.00001);
    SceneAtmosphereAttachment accepted=parsed;
    const char* invalid[]={
        "{\"profile\":\"unknown\"}",
        "{\"profile\":\"physical_smoke_v1\",\"sampling_layout\":\"cell_centered\",\"sky_illumination\":1}",
        "null"};
    for(size_t i=0;i<3;++i) {
        json_object_object_get_ex(root,"atmosphere_attachment",&attachment);
        json_object_object_add(attachment,"optics",json_tokener_parse(invalid[i]));
        assert(!SceneAtmosphereAttachmentParse(root,&parsed,diagnostic,sizeof(diagnostic)));
        assert(!memcmp(&parsed,&accepted,sizeof(parsed)));
    }
    const char* fields[]={"reference_concentration_kg_m3","mass_extinction_m2_kg","scattering_albedo","anisotropy","sky_error_budget","sky_illumination","sampling_layout","extra"};
    json_object* bad[]={json_object_new_double(0),json_object_new_double(-1),json_object_new_double(1.01),json_object_new_double(.96),json_object_new_double(.001),json_object_new_int(1),json_object_new_string("node"),json_object_new_int(1)};
    for(size_t i=0;i<8;++i) {
        json_object* optics=SceneAtmosphereOpticsToJson(&v.optics);
        json_object_object_add(optics,fields[i],bad[i]);json_object_object_add(attachment,"optics",optics);
        assert(!SceneAtmosphereAttachmentParse(root,&parsed,diagnostic,sizeof(diagnostic)));
        assert(!memcmp(&parsed,&accepted,sizeof(parsed)));
    }
    for(int type=0;type<2;++type) {
        json_object* optics=SceneAtmosphereOpticsToJson(&v.optics);
        json_object_object_add(optics,"scattering_albedo",type?json_object_new_boolean(true):json_object_new_string("0.65"));
        json_object_object_add(attachment,"optics",optics);
        assert(!SceneAtmosphereAttachmentParse(root,&parsed,diagnostic,sizeof(diagnostic)));
    }
    v.affects_lighting=false;assert(!SceneAtmosphereAttachmentValidate(&v,diagnostic,sizeof(diagnostic)));
    v.affects_lighting=true;v.optics.physical=false;
    json_object_object_add(root,"atmosphere_attachment",SceneAtmosphereAttachmentToJson(&v));
    assert(SceneAtmosphereAttachmentParse(root,&parsed,diagnostic,sizeof(diagnostic)) && !parsed.optics.physical && parsed.optics.sky_error_budget==.00001);
    v.optics.anisotropy=NAN;assert(!SceneAtmosphereAttachmentValidate(&v,diagnostic,sizeof(diagnostic)));
    v.optics.anisotropy=0;v.optics.reference_concentration_kg_m3=1e300;v.optics.mass_extinction_m2_kg=1e300;
    assert(!SceneAtmosphereAttachmentValidate(&v,diagnostic,sizeof(diagnostic)));
    json_object_put(root);puts("Retained optics codec PASS: absent legacy, typed fields, finite units, strict refusal preservation and dormant profile roundtrip");
}
