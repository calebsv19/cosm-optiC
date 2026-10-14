#include "ui/shared_hud_text.h"
#include "kit_ui_text_edit.h"
#include "kit_ui_text_presentation.h"
#include "render/text_draw.h"
#include <string.h>
/* Shared layout, application-native painter. Existing text draw creates/caches
 * its texture synchronously; transient rows are never queued as borrowed text. */
typedef struct {SDL_Renderer *renderer;TTF_Font *font;SDL_Color ink;} Paint;
static CoreResult measure(void *user,const char *text,float *width) {Paint *p=user;int w=0;if(!ray_tracing_text_measure_utf8(p->renderer,p->font,text,&w,NULL))return (CoreResult){CORE_ERR_IO,"HUD font measurement failed"};*width=w;return core_result_ok();}
static CoreResult text(void *user,KitRenderVec2 at,const char *text) {Paint *p=user;return ray_tracing_text_draw_utf8_at(p->renderer,p->font,text,(int)at.x,(int)at.y,p->ink)?core_result_ok():(CoreResult){CORE_ERR_IO,"HUD text draw failed"};}
static CoreResult rectangle(void *user,KitRenderRect r,KitRenderColor c) {(void)user;(void)r;(void)c;return core_result_ok();}
void ray_tracing_shared_hud_text(SDL_Renderer *renderer,TTF_Font *font,SDL_Rect bounds,const char *label,SDL_Color ink) {
    if(!renderer || !font || !label || bounds.w<=0 || bounds.h<=0 || strlen(label)>=KIT_UI_TEXT_PRESENTATION_BYTES)return;
    char buffer[KIT_UI_TEXT_PRESENTATION_BYTES];memcpy(buffer,label,strlen(label)+1);KitUiTextEdit edit={0};if(kit_ui_text_bind(&edit,buffer,sizeof(buffer),0).code!=CORE_OK)return;
    Paint p={renderer,font,ink};int height=13;ray_tracing_text_line_height(renderer,font,&height);
    KitUiTextPresentation view;KitUiTextPresentationOptions options={{bounds.x,bounds.y,bounds.w,bounds.h},height,1,0,0,0,0};
    if(kit_ui_text_presentation_build(&view,&edit,&options,measure,&p).code!=CORE_OK)return;
    SDL_Rect previous,clip=bounds;SDL_bool clipped=SDL_RenderIsClipEnabled(renderer);SDL_RenderGetClipRect(renderer,&previous);if(clipped && !SDL_IntersectRect(&previous,&bounds,&clip))return;SDL_RenderSetClipRect(renderer,&clip);
    KitUiTextPainter painter={&p,rectangle,text};KitUiTextPresentationColors colors={0};(void)kit_ui_text_presentation_paint(&view,&colors,&painter);SDL_RenderSetClipRect(renderer,clipped?&previous:NULL);
}
