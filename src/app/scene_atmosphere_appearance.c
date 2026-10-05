#include "app/scene_atmosphere_appearance.h"
#include <math.h>

const char* SceneAtmosphereLookName(SceneAtmosphereLook look) {
    static const char* names[]={"Light cloud", "Soft mist", "Smoke"};
    return look>=0 && look<SCENE_ATMOSPHERE_LOOK_COUNT ? names[look] : "Custom";
}
bool SceneAtmosphereApplyLook(SceneAtmosphereAttachment* v, SceneAtmosphereLook look) {
    if(!v || look<0 || look>=SCENE_ATMOSPHERE_LOOK_COUNT) return false;
    /* These are art-directed single-scattering looks, not fluid initial states.
     * Low extinction and stronger scattering make cloud/mist less soot-like. */
    static const double settings[][5]={
        {0.8, 0.8, 0.015, 6.0, 0.65},
        {0.35, 1.0, 0.008, 3.0, 0.4},
        {1.4, 1.0, 0.2, 0.8, 1.0}
    };
    static const double tints[][3]={{1,1,1},{0.9,0.96,1},{0.55,0.58,0.62}};
    v->density_scale=settings[look][0];v->density_gamma=settings[look][1];
    v->absorption_gain=settings[look][2];v->scatter_gain=settings[look][3];
    v->opacity=settings[look][4];v->step_scale=1;
    for(int i=0;i<3;++i) v->tint[i]=tints[look][i];
    return true;
}
SceneAtmosphereLook SceneAtmosphereCurrentLook(const SceneAtmosphereAttachment* v) {
    for(int i=0;i<SCENE_ATMOSPHERE_LOOK_COUNT;++i) {
        SceneAtmosphereAttachment expected=*v;SceneAtmosphereApplyLook(&expected,i);
        const double actual[]={v->density_scale,v->density_gamma,v->absorption_gain,v->scatter_gain,v->opacity,v->step_scale,v->tint[0],v->tint[1],v->tint[2]};
        const double target[]={expected.density_scale,expected.density_gamma,expected.absorption_gain,expected.scatter_gain,expected.opacity,expected.step_scale,expected.tint[0],expected.tint[1],expected.tint[2]};
        bool match=true;
        for(int j=0;j<9;++j) if(fabs(actual[j]-target[j])>1e-9) match=false;
        if(match) return i;
    }
    return SCENE_ATMOSPHERE_LOOK_COUNT;
}
void SceneAtmosphereTintHSV(const SceneAtmosphereAttachment* v,double hsv[3]) {
    double r=v->tint[0],g=v->tint[1],b=v->tint[2];
    double high=fmax(r,fmax(g,b)),low=fmin(r,fmin(g,b)),span=high-low,h=0;
    if(span>1e-12) {
        if(high==r) h=60*fmod((g-b)/span,6);
        else if(high==g) h=60*((b-r)/span+2);
        else h=60*((r-g)/span+4);
    }
    hsv[0]=h<0?h+360:h;hsv[1]=high>0?span/high:0;hsv[2]=high;
}
bool SceneAtmosphereSetTintHSV(SceneAtmosphereAttachment* v,const double hsv[3]) {
    if(!v || !isfinite(hsv[0]) || !isfinite(hsv[1]) || !isfinite(hsv[2]) ||
       hsv[0]<0 || hsv[0]>360 || hsv[1]<0 || hsv[1]>1 || hsv[2]<0 || hsv[2]>100) return false;
    double h=fmod(hsv[0],360)/60,c=hsv[2]*hsv[1],x=c*(1-fabs(fmod(h,2)-1)),m=hsv[2]-c;
    double rgb[3]={0,0,0};
    switch((int)h) {
        case 0:rgb[0]=c;rgb[1]=x;break;case 1:rgb[0]=x;rgb[1]=c;break;
        case 2:rgb[1]=c;rgb[2]=x;break;case 3:rgb[1]=x;rgb[2]=c;break;
        case 4:rgb[0]=x;rgb[2]=c;break;default:rgb[0]=c;rgb[2]=x;break;
    }
    for(int i=0;i<3;++i) v->tint[i]=rgb[i]+m;
    return true;
}
