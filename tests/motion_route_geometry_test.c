#include "motion/motion_route_geometry.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static double norm(const double v[3]){return hypot(hypot(v[0],v[1]),v[2]);}
static double speed(const MotionRouteGeometry *g,double t) {
    double p[3],d[3],dd[3];assert(MotionRouteGeometryParameter(g,t,p,d,dd));return norm(d);
}
int main(void) {
    MotionRouteGeometry *g=malloc(sizeof(*g)),*scaled=malloc(sizeof(*scaled));assert(g&&scaled);
    MotionPath path={.count=3};path.points[0].linear=true;path.points[1].linear=true;
    path.points[1].position[0]=3;path.points[2].position[0]=3;path.points[2].position[1]=4;
    assert(MotionRouteGeometryBuild(&path,2,g)==MOTION_ROUTE_OK);assert(fabs(g->length-14)<1e-12);
    assert(g->corner[1]&&!g->singular&&g->curvature_bound<1e-12);assert(g->point_distances[1]==6);
    path.points[2].position[0]=6;path.points[2].position[1]=0;
    assert(MotionRouteGeometryBuild(&path,1,g)==MOTION_ROUTE_OK&&!g->corner[1]);
    path.points[2].position[0]=0;assert(MotionRouteGeometryBuild(&path,1,g)==MOTION_ROUTE_OK&&g->corner[1]);
    /* Regular 3D cubic; independent high-resolution Simpson integration. */
    path=(MotionPath){.count=2};path.points[0].outgoing[0]=1;path.points[0].outgoing[1]=1;
    path.points[1].position[0]=3;path.points[1].position[2]=2;path.points[1].incoming[0]=-1;path.points[1].incoming[1]=1;
    assert(MotionRouteGeometryBuild(&path,1,g)==MOTION_ROUTE_OK&&!g->singular);
    const int n=20000;double integral=speed(g,0)+speed(g,1);
    for(int i=1;i<n;++i)integral+=(i%2?4:2)*speed(g,(double)i/n);integral/=3*n;
    assert(fabs(g->length-integral)<=g->length_error+1e-11);
    for(size_t i=0;i<g->count;++i)for(int j=0;j<5;++j) {
        double u=g->leaves[i].begin+(g->leaves[i].end-g->leaves[i].begin)*j/4;
        double p[3],d[3],dd[3],c[3];assert(MotionRouteGeometryParameter(g,u,p,d,dd));
        c[0]=d[1]*dd[2]-d[2]*dd[1];c[1]=d[2]*dd[0]-d[0]*dd[2];c[2]=d[0]*dd[1]-d[1]*dd[0];
        double k=norm(c)/pow(norm(d),3);assert(k<=g->leaves[i].curvature_bound*(1+1e-8));
    }
    assert(MotionRouteGeometryBuild(&path,3,scaled)==MOTION_ROUTE_OK);
    assert(fabs(scaled->length-3*g->length)<1e-9);
    assert(fabs(scaled->curvature_bound*3-g->curvature_bound)<1e-6);
    for(size_t i=0;i<path.count;++i)for(int k=0;k<3;++k)path.points[i].position[k]+=1e6;
    assert(MotionRouteGeometryBuild(&path,1,scaled)==MOTION_ROUTE_OK);
    assert(fabs(scaled->length-g->length)<1e-9);
    /* Curved zero endpoint derivative is flagged, never certified as zero curvature. */
    path.points[0].outgoing[0]=path.points[0].outgoing[1]=0;
    assert(MotionRouteGeometryBuild(&path,1,g)==MOTION_ROUTE_OK&&g->singular&&isinf(g->curvature_bound));
    assert(g->stopping_certificate&&g->required_stop[0]&&isfinite(g->stop_curvature_factor));
    path=(MotionPath){.count=2};path.points[1].position[0]=3;
    assert(MotionRouteGeometryBuild(&path,1,g)==MOTION_ROUTE_OK&&!g->singular&&g->curvature_bound==0);
    double before=g->length;assert(MotionRouteGeometryBuild(&path,NAN,g)==MOTION_ROUTE_INVALID&&g->length==before);
    path.points[1].position[0]=0;assert(MotionRouteGeometryBuild(&path,1,g)==MOTION_ROUTE_INVALID&&g->length==before);
    path=(MotionPath){.count=2};path.points[0].outgoing[0]=1;path.points[1].incoming[0]=1;
    assert(MotionRouteGeometryBuild(&path,1,g)==MOTION_ROUTE_OK&&!g->stopping_certificate);
    free(g);free(scaled);puts("M5.4 geometry foundation PASS: length brackets, curvature bounds, corner/reversal, scale/translation, singular classification and atomic refusal");
}
