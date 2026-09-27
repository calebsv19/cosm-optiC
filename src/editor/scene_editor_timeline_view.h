#ifndef SCENE_EDITOR_TIMELINE_VIEW_H
#define SCENE_EDITOR_TIMELINE_VIEW_H
#include "animation/timeline_document.h"
#include <SDL2/SDL.h>
/* Presentation-only time window. Never changes the authored range or rate. */
typedef struct { double first, span; bool valid; } TimelineView;
typedef enum {
    TL_START, TL_PREVIOUS, TL_PLAY, TL_NEXT, TL_END, TL_FRAME, TL_ADD,
    TL_KEYS, TL_CURVES, TL_FIT, TL_FIT_CHANNEL, TL_ZOOM_OUT, TL_ZOOM_IN,
    TL_VALUE, TL_INTERPOLATION, TL_DELETE, TL_KEY_FRAME, TL_CONTROL_COUNT
} TimelineControl;
typedef struct {
    SDL_Rect panel, toolbar, ruler, gutter, body, grid, footer;
    SDL_Rect controls[TL_CONTROL_COUNT];
    int row_height;
} TimelineLayout;
TimelineLayout SceneEditorTimelineLayout(SDL_Rect panel);
void TimelineViewFit(TimelineView* view,TimelineRange range);
void TimelineViewZoom(TimelineView* view,double factor,double anchor);
void TimelineViewPan(TimelineView* view,double frames);
double TimelineViewFrame(const TimelineView* view,SDL_Rect grid,int x);
int TimelineViewX(const TimelineView* view,SDL_Rect grid,double frame);
double TimelineViewTick(const TimelineView* view,int width);
const char* TimelineChannelLabel(const char* property);
#endif
