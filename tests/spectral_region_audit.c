/* Qualification harness only. Reuses production Region with the full projector,
 * pixel coordinates, RNG and map. Its NPY packs six 8x8 regions vertically;
 * it is not a full-frame ordinary-job artifact. See the audit manifest. */
#include "render/runtime_spectral_surface_path_internal.h"
static const int regions[][2]={{118,138},{152,32},{118,116},{210,60},{97,138},{84,84}};
bool RuntimeSpectralSurfacePath_Render(RuntimeSpectralSurfacePath *state,
 const RuntimeNative3DPreparedFrame *frame,uint8_t *pixels,RuntimeSpectralReadout *out,
 const RuntimeNative3DTileSchedulerCancelToken *cancel,RuntimeNative3DTileSchedulerProgressCallback callback,
 void *user,char *diag,size_t size) {
 if(frame->width!=256 || frame->height!=192 || !state->component_path[0]) {
  snprintf(diag,size,"audit requires exact 256x192 film and component output");return false;
 }
 state->scene=frame->traceScene?frame->traceScene:&frame->scene;
 out->surface_path=(RuntimeSpectralSurfacePathReadout){0};
 RuntimeSpectralSurfacePath_GetMetadata(state,out);
 state->component_writer=spectral_components_begin(state->component_path,8,48);
 if(!state->component_writer) {snprintf(diag,size,"audit output open failed");return false;}
 double classes[6][64][5][3];
 for(int i=0;i<6;++i) {
  int x=regions[i][0]+4,y=regions[i][1]+4;
  for(int py=0;py<8;++py)for(int px=0;px<8;++px) {
   Vec3 before[5];for(int k=0;k<5;++k)before[k]=out->surface_path.film_nonscattered_indirect[k];
   if(!RuntimeSpectralSurfacePath_Region(state,frame,x+px,y+py,x+px+1,y+py+1,pixels,NULL,0,out,cancel,diag,size))goto failed;
   for(int k=0;k<5;++k) {Vec3 d=vec3_sub(out->surface_path.film_nonscattered_indirect[k],before[k]);
    classes[i][py*8+px][k][0]=d.x;classes[i][py*8+px][k][1]=d.y;classes[i][py*8+px][k][2]=d.z;}
  }
  IntegratorTile tile={.originX=x,.originY=y,.width=8,.height=8};
  RuntimeNative3DTileSchedulerProgress progress={.dirtyTiles=&tile,.dirtyTileCount=1,.startedSubpasses=1,
   .completedSubpasses=i==5?1:0,.totalSubpasses=1,.completedTilesInSubpass=i+1,.totalTilesInSubpass=6};
  if(callback && !callback(&progress,user))goto failed;
 }
 if(RuntimeNative3DTileSchedulerCancelToken_IsRequested(cancel))goto failed;
 bool ok=spectral_components_finish(state->component_writer,state->component_path,state->component_overwrite,&out->surface_path);
 state->component_writer=NULL;
 if(ok) {
  char path[4200];snprintf(path,sizeof(path),"%s.classes.json",state->component_path);
  FILE *f=fopen(path,"wx");if(!f)return false;
  fprintf(f,"{\"shape\":[6,64,5,3],\"measure\":\"unexposed XYZ pixel means; class order matches nonscattered_indirect\",\"values\":[");
  for(int i=0;i<6*64*5*3;++i)fprintf(f,"%s%.17g",i?",":"",((double *)classes)[i]);
  fprintf(f,"]}\n");if(fclose(f)!=0)return false;
 }
 snprintf(diag,size,"%s",ok?"audit ROI capture; exact full-sensor rays":"audit publication failed");return ok;
failed:
 spectral_components_abort(state->component_writer);state->component_writer=NULL;return false;
}
