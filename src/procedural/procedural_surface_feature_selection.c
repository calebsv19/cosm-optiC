#include "procedural/procedural_surface_feature_selection.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
bool ProceduralSurfaceFeatureSelectionV1_Contains(const ProceduralSurfaceFeatureSelectionV1 *s,uint32_t id){if(!s||!id)return false;for(size_t i=0;i<s->feature_id_count;i++)if(s->feature_ids[i]==id)return true;return false;}
bool ProceduralSurfaceFeatureSelectionV1_Build(const ProceduralSurfaceFeatureFieldV1*f,double min_radius,ProceduralSurfaceFeatureSelectionV1*out){
 if(!f||!out||min_radius<0||!ProceduralSurfaceFeatureFieldV1_Validate(f)||!ProceduralSurfaceFeatureFieldV1_Digest(f,out->field_digest_sha256))return false;memset(out->feature_ids,0,sizeof(out->feature_ids));out->feature_id_count=0;for(size_t i=0;i<f->feature_count;i++){const ProceduralSurfaceFeatureRootV1*r=&f->features[i];if(r->radius>=min_radius){if(out->feature_id_count>=PROCEDURAL_SURFACE_FEATURE_SELECTION_MAX_IDS)return false;out->feature_ids[out->feature_id_count++]=r->feature_id;}}return out->feature_id_count>0;}
bool ProceduralSurfaceFeatureSelectionV1_BuildCarrierValues(const ProceduralSurfaceFeatureFieldV1*f,const ProceduralSurfaceFeatureSelectionV1*s,const CoreMeshAssetRuntimeVertex*v,size_t n,double*out){char digest[PROCEDURAL_SURFACE_FEATURE_FIELD_DIGEST_CAPACITY];if(!f||!s||!v||!out||!n||!ProceduralSurfaceFeatureFieldV1_Digest(f,digest)||strcmp(digest,s->field_digest_sha256)!=0)return false;for(size_t i=0;i<n;i++){ProceduralSurfaceFeatureSampleV1 sample;if(!ProceduralSurfaceFeatureFieldV1_Sample(f,(ProceduralSurfaceFeatureVec3){v[i].position.x,v[i].position.y,v[i].position.z},(ProceduralSurfaceFeatureVec3){v[i].normal.x,v[i].normal.y,v[i].normal.z},&sample))return false;out[i]=ProceduralSurfaceFeatureSelectionV1_Contains(s,sample.feature_id)?sample.coverage:0.0;}return true;}
bool ProceduralSurfaceFeatureSelectionV1_BuildRegion(const ProceduralImportedSurfaceRegionV1*base,const ProceduralSurfaceFeatureFieldV1*f,const ProceduralSurfaceFeatureSelectionV1*s,const CoreMeshAssetRuntimeVertex*v,size_t n,const char*id,ProceduralImportedSurfaceRegionV1*out){
 ProceduralImportedSurfaceRegionV1 r;
 if(!base||!f||!s||!v||!id||!out||n!=base->vertex_count||strlen(id)>=sizeof(r.region_id))return false;
 memset(&r,0,sizeof(r));r=*base;r.vertex_weights=calloc(n,sizeof(*r.vertex_weights));if(!r.vertex_weights)return false;
 if(!ProceduralSurfaceFeatureSelectionV1_BuildCarrierValues(f,s,v,n,r.vertex_weights)){free(r.vertex_weights);return false;}
 snprintf(r.region_id,sizeof(r.region_id),"%s",id);
 snprintf(r.recipe_digest_sha256,sizeof(r.recipe_digest_sha256),"%s",s->field_digest_sha256);
 if(!ProceduralImportedSurfaceRegionV1_RefreshValues(&r)){free(r.vertex_weights);return false;}*out=r;return true;
}
