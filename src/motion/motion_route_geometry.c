#include "motion/motion_route_geometry.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct Cubic {double p[4][3];} Cubic;
static double norm(const double v[3]){return hypot(hypot(v[0],v[1]),v[2]);}
static void cross(const double a[3],const double b[3],double c[3]){c[0]=a[1]*b[2]-a[2]*b[1];c[1]=a[2]*b[0]-a[0]*b[2];c[2]=a[0]*b[1]-a[1]*b[0];}
static Cubic controls(const MotionPath *p,size_t i) {
    Cubic c={0};
    for(int k=0;k<3;++k) {
        c.p[0][k]=0;c.p[3][k]=p->points[i+1].position[k]-p->points[i].position[k];
        c.p[1][k]=c.p[0][k]+p->points[i].outgoing[k];
        c.p[2][k]=c.p[3][k]+p->points[i+1].incoming[k];
        if(p->points[i].linear) {
            c.p[1][k]=c.p[0][k]+(c.p[3][k]-c.p[0][k])/3;
            c.p[2][k]=c.p[0][k]+2*(c.p[3][k]-c.p[0][k])/3;
        }
    }
    return c;
}
static void split(const Cubic *c,Cubic *l,Cubic *r) {
    *l=*c;*r=*c;
    for(int k=0;k<3;++k) {
        double a=(c->p[0][k]+c->p[1][k])*.5,b=(c->p[1][k]+c->p[2][k])*.5,d=(c->p[2][k]+c->p[3][k])*.5;
        l->p[1][k]=a;l->p[2][k]=(a+b)*.5;r->p[1][k]=(b+d)*.5;r->p[2][k]=d;
        l->p[3][k]=r->p[0][k]=(l->p[2][k]+r->p[1][k])*.5;
    }
}
static double curvature(const Cubic *c) {
    double d[3][3],e[2][3],lower[3]={0},cross_bound=0;
    for(int j=0;j<3;++j)for(int k=0;k<3;++k)d[j][k]=3*(c->p[j+1][k]-c->p[j][k]);
    for(int j=0;j<2;++j)for(int k=0;k<3;++k)e[j][k]=2*(d[j+1][k]-d[j][k]);
    for(int k=0;k<3;++k) {
        double lo=fmin(d[0][k],fmin(d[1][k],d[2][k])),hi=fmax(d[0][k],fmax(d[1][k],d[2][k]));
        lower[k]=lo>0?lo:hi<0?-hi:0;
    }
    for(int i=0;i<3;++i)for(int j=0;j<2;++j){double v[3];cross(d[i],e[j],v);double n=norm(v);if(!isfinite(n))return INFINITY;cross_bound=fmax(cross_bound,n);}
    double speed=norm(lower);
    if(speed>0)return cross_bound/speed/speed/speed;
    /* Exactly collinear, consistently directed controls admit a constant
     * tangent even with zero endpoint handles. Backtracking is singular. */
    double chord[3];for(int k=0;k<3;++k)chord[k]=c->p[3][k]-c->p[0][k];
    if(cross_bound==0&&norm(chord)>0) {
        bool monotone=true;for(int i=0;i<3;++i){double dot=0;for(int k=0;k<3;++k)dot+=d[i][k]*chord[k];if(dot<0)monotone=false;}
        if(monotone)return 0;
    }
    return INFINITY;
}
static double arc_integral(const Cubic *c) {
    static const double x[5]={0,.538469310105683091,-.538469310105683091,.906179845938663993,-.906179845938663993};
    static const double w[5]={.568888888888888889,.478628670499366468,.478628670499366468,.236926885056189088,.236926885056189088};
    double length=0;
    for(int i=0;i<5;++i) {
        double t=(x[i]+1)*.5,s=1-t,d[3];
        for(int k=0;k<3;++k)d[k]=3*(s*s*(c->p[1][k]-c->p[0][k])+2*s*t*(c->p[2][k]-c->p[1][k])+t*t*(c->p[3][k]-c->p[2][k]));
        length+=w[i]*norm(d)*.5;
    }
    return length;
}
static Cubic prefix(const Cubic *c,double t) {
    Cubic left=*c;double s=1-t;
    for(int k=0;k<3;++k) {
        double a=s*c->p[0][k]+t*c->p[1][k],b=s*c->p[1][k]+t*c->p[2][k],d=s*c->p[2][k]+t*c->p[3][k];
        left.p[1][k]=a;left.p[2][k]=s*a+t*b;left.p[3][k]=s*left.p[2][k]+t*(s*b+t*d);
    }
    return left;
}
static MotionRouteStatus append(MotionRouteGeometry *g,const Cubic *c,double begin,double end,double tolerance,int depth) {
    double chord[3],polygon=0;
    for(int k=0;k<3;++k)chord[k]=c->p[3][k]-c->p[0][k];
    for(int j=0;j<3;++j){double edge[3];for(int k=0;k<3;++k)edge[k]=c->p[j+1][k]-c->p[j][k];polygon+=norm(edge);}
    double lower=norm(chord),gap=fmax(0,polygon-lower),bound=curvature(c);
    if(!isfinite(polygon)||!isfinite(lower))return MOTION_ROUTE_NUMERIC;
    if((gap>tolerance || (!isfinite(bound)&&depth<16)) && depth<24) {
        Cubic l,r;split(c,&l,&r);double mid=(begin+end)*.5;
        MotionRouteStatus status=append(g,&l,begin,mid,tolerance*.5,depth+1);
        if(status!=MOTION_ROUTE_OK)return status;
        return append(g,&r,mid,end,tolerance*.5,depth+1);
    }
    if(gap>tolerance)return MOTION_ROUTE_NUMERIC;
    if(g->count==MOTION_ROUTE_LEAF_CAPACITY)return MOTION_ROUTE_CAPACITY;
    double length=arc_integral(c);
    if(!isfinite(length))return MOTION_ROUTE_NUMERIC;
    length=fmax(lower,fmin(polygon,length));
    double error=fmax(length-lower,polygon-length);
    if(length<=0)return MOTION_ROUTE_NUMERIC;
    g->leaves[g->count++]=(MotionRouteLeaf){begin,end,g->length,length,error,bound};
    g->length+=length;g->length_error+=error;g->curvature_bound=fmax(g->curvature_bound,bound);
    if(!isfinite(bound))g->singular=true;
    return MOTION_ROUTE_OK;
}
static bool tangent(const Cubic *c,bool end,double v[3]) {
    for(int j=1;j<=3;++j) {
        for(int k=0;k<3;++k)v[k]=end?c->p[3][k]-c->p[3-j][k]:c->p[j][k]-c->p[0][k];
        double n=norm(v);if(n>0&&isfinite(n)){for(int k=0;k<3;++k)v[k]/=n;return true;}
    }
    return false;
}
/* For c'(u)=u*d(u), K(u)*arc(u) <= max|d|*max|d x d'|/(2*min|d|^3).
 * This covers a zero endpoint handle without pretending curvature is finite.
 * At a required rest endpoint v^2 <= 2*a*arc bounds normal acceleration. */
static double endpoint_factor(const Cubic *c,bool end) {
    Cubic r=*c;if(end)for(int i=0;i<4;++i)for(int k=0;k<3;++k)r.p[i][k]=c->p[3-i][k];
    double d[2][3],e[3],low[3],v[3];
    for(int k=0;k<3;++k) {
        if(r.p[1][k]!=r.p[0][k])return INFINITY;
        d[0][k]=6*(r.p[2][k]-r.p[0][k]);d[1][k]=3*(r.p[3][k]-r.p[2][k]);e[k]=d[1][k]-d[0][k];
        double lo=fmin(d[0][k],d[1][k]),hi=fmax(d[0][k],d[1][k]);low[k]=lo>0?lo:hi<0?-hi:0;
    }
    double minimum=norm(low);if(minimum<=0)return INFINITY;
    cross(d[0],e,v);double bound=norm(v);cross(d[1],e,v);bound=fmax(bound,norm(v));
    return .5*fmax(norm(d[0]),norm(d[1]))*bound/minimum/minimum/minimum;
}
static Cubic leaf_controls(Cubic c,double begin,double end) {
    /* Geometry leaves are dyadic subdivisions of one segment. */
    double lo=0,hi=1;
    for(int depth=0;depth<24 && (lo!=begin||hi!=end);++depth) {
        Cubic l,r;split(&c,&l,&r);double mid=(lo+hi)*.5;
        if(end<=mid){c=l;hi=mid;}else{c=r;lo=mid;}
    }
    return c;
}
static void stopping_bounds(MotionRouteGeometry *g) {
    g->stopping_certificate=true;
    for(size_t segment=0;segment+1<g->path.count;++segment) {
        g->curvature_stop_point[segment]=-1;
        Cubic c=controls(&g->path,segment);bool first=true,last=true;
        for(int k=0;k<3;++k){first=first&&c.p[0][k]==c.p[1][k];last=last&&c.p[2][k]==c.p[3][k];}
        /* Collinear eased segments have zero curvature and need no extra stop. */
        bool weighted=(first||last)&&curvature(&c)!=0;
        if(weighted){g->curvature_stop_point[segment]=(int)(first?segment:segment+1);if(first)g->required_stop[segment]=true;if(last)g->required_stop[segment+1]=true;}
        for(size_t i=0;i<g->count;++i) {
            MotionRouteLeaf *leaf=&g->leaves[i];if(leaf->begin<segment||leaf->begin>=segment+1)continue;
            if(i>0 && g->leaves[i-1].begin>=segment) {
                Cubic previous=leaf_controls(c,g->leaves[i-1].begin-segment,g->leaves[i-1].end-segment);
                Cubic current=leaf_controls(c,leaf->begin-segment,leaf->end-segment);
                double left[3],right[3],dot=0;
                if(!tangent(&previous,true,left)||!tangent(&current,false,right))g->stopping_certificate=false;
                else {for(int k=0;k<3;++k)dot+=left[k]*right[k];if(dot<1-32*DBL_EPSILON)g->stopping_certificate=false;}
            }
            if(!weighted)g->regular_curvature_bound=fmax(g->regular_curvature_bound,leaf->curvature_bound);
            else {
                double distance=first?leaf->distance+leaf->length-g->point_distances[segment]:g->point_distances[segment+1]-leaf->distance;
                double factor=leaf->curvature_bound*(distance+g->length_error);
                if(!isfinite(leaf->curvature_bound)) {
                    Cubic sub=leaf_controls(c,leaf->begin-segment,leaf->end-segment);
                    if(first&&leaf->begin==segment)factor=endpoint_factor(&sub,false);
                    else if(last&&leaf->end==segment+1)factor=endpoint_factor(&sub,true);
                }
                g->stop_curvature_factor=fmax(g->stop_curvature_factor,factor);
                if(!isfinite(factor))g->stopping_certificate=false;
            }
        }
    }
    if(!isfinite(g->regular_curvature_bound))g->stopping_certificate=false;
    for(size_t i=0;i<g->path.count;++i)g->required_stop[i]=g->required_stop[i]||g->corner[i];
}
MotionRouteStatus MotionRouteGeometryBuild(const MotionPath *path,double scale,MotionRouteGeometry *out) {
    if(!path||!out||path->count<2||path->count>MOTION_POINT_CAPACITY||!isfinite(scale)||scale<=0)return MOTION_ROUTE_INVALID;
    /* Large bounded table is heap-local during construction; publish atomically. */
    MotionRouteGeometry *g=calloc(1,sizeof(*g));if(!g)return MOTION_ROUTE_CAPACITY;
    g->path=*path;MotionRouteStatus status=MOTION_ROUTE_OK;
    for(size_t i=0;i<path->count;++i)for(int k=0;k<3;++k) {
        g->path.points[i].position[k]*=scale;g->path.points[i].incoming[k]*=scale;g->path.points[i].outgoing[k]*=scale;
        if(!isfinite(g->path.points[i].position[k])||!isfinite(g->path.points[i].incoming[k])||!isfinite(g->path.points[i].outgoing[k]))status=MOTION_ROUTE_INVALID;
    }
    for(size_t i=0;status==MOTION_ROUTE_OK&&i+1<path->count;++i) {
        Cubic c=controls(&g->path,i);double polygon=0;
        for(int j=0;j<3;++j){double v[3];for(int k=0;k<3;++k)v[k]=c.p[j+1][k]-c.p[j][k];polygon+=norm(v);}
        if(!isfinite(polygon)||polygon<=0){status=MOTION_ROUTE_INVALID;break;}
        g->point_distances[i]=g->length;
        if(i) {
            Cubic previous=controls(&g->path,i-1);double a[3],b[3],dot=0;
            if(!tangent(&previous,true,a)||!tangent(&c,false,b)){status=MOTION_ROUTE_INVALID;break;}
            for(int k=0;k<3;++k)dot+=a[k]*b[k];
            /* Small angular mismatch is still a corner: conservative stop. */
            g->corner[i]=dot<1-32*DBL_EPSILON;
        }
        status=append(g,&c,(double)i,(double)i+1,polygon*1e-6,0);
    }
    if(status==MOTION_ROUTE_OK) {
        g->point_distances[path->count-1]=g->length;
        if(!isfinite(g->length)||g->length<=0)status=MOTION_ROUTE_NUMERIC;
        else {stopping_bounds(g);*out=*g;}
    }
    free(g);return status;
}
static bool parameter_side(const MotionRouteGeometry *g,size_t i,double t,double p[3],double d[3],double dd[3]) {
    double s=1-t;
    Cubic c=controls(&g->path,i);
    for(int k=0;k<3;++k) {
        p[k]=g->path.points[i].position[k]+s*s*s*c.p[0][k]+3*s*s*t*c.p[1][k]+3*s*t*t*c.p[2][k]+t*t*t*c.p[3][k];
        d[k]=3*(s*s*(c.p[1][k]-c.p[0][k])+2*s*t*(c.p[2][k]-c.p[1][k])+t*t*(c.p[3][k]-c.p[2][k]));
        dd[k]=6*(s*(c.p[2][k]-2*c.p[1][k]+c.p[0][k])+t*(c.p[3][k]-2*c.p[2][k]+c.p[1][k]));
        if(!isfinite(p[k])||!isfinite(d[k])||!isfinite(dd[k]))return false;
    }
    return true;
}

bool MotionRouteGeometryParameter(const MotionRouteGeometry *g,double u,double p[3],double d[3],double dd[3]) {
    if(!g||!p||!d||!dd||!isfinite(u)||u<0||g->path.count<2||u>g->path.count-1)return false;
    size_t i=(size_t)u;if(i+1>=g->path.count)i=g->path.count-2;
    return parameter_side(g,i,u-i,p,d,dd);
}

bool MotionRouteGeometryDistanceSide(const MotionRouteGeometry *g,double distance,bool left_side,MotionRouteFrame *out) {
    if(!g||!out||!g->count||!isfinite(distance)||distance<0||distance>g->length)return false;
    size_t lo=0,hi=g->count;
    while(lo+1<hi){size_t mid=(lo+hi)/2;if(g->leaves[mid].distance<=distance)lo=mid;else hi=mid;}
    if(left_side&&lo>0&&distance==g->leaves[lo].distance)--lo;
    const MotionRouteLeaf *leaf=&g->leaves[lo];size_t segment=(size_t)leaf->begin;
    Cubic c=controls(&g->path,segment),sub=leaf_controls(c,leaf->begin-segment,leaf->end-segment);
    double target=distance-leaf->distance,a=0,b=1,u=fmax(0,fmin(1,target/leaf->length));
    for(int i=0;i<48;++i) {
        Cubic left=prefix(&sub,u);double value=arc_integral(&left);
        if(fabs(value-target)<=1e-13*g->length)break;
        if(value<target)a=u;else b=u;u=(a+b)*.5;
    }
    MotionRouteFrame frame={.parameter=leaf->begin+u*(leaf->end-leaf->begin)};
    if(distance==0)frame.parameter=0;if(distance==g->length)frame.parameter=g->path.count-1;
    double first[3],second[3];
    if(!parameter_side(g,segment,frame.parameter-segment,frame.position,first,second))return false;
    double speed=norm(first);frame.regular=speed>0;
    if(frame.regular) {
        double dot=0;for(int k=0;k<3;++k){frame.tangent[k]=first[k]/speed;dot+=frame.tangent[k]*second[k];}
        for(int k=0;k<3;++k)frame.curvature[k]=(second[k]-frame.tangent[k]*dot)/speed/speed;
    } else {
        bool end=frame.parameter-segment>=1;Cubic endpoint=controls(&g->path,segment);
        if(!tangent(&endpoint,end,frame.tangent))return false;
    }
    *out=frame;return true;
}

bool MotionRouteGeometryDistance(const MotionRouteGeometry *g,double distance,MotionRouteFrame *out) {
    return MotionRouteGeometryDistanceSide(g,distance,false,out);
}
