#include "core_authored_surface_graph.h"
#include <assert.h>
#include <stdio.h>
static double sample(const CoreSurfaceGraph *g, double x) {
    CoreSurfaceGraphQuery q={0};CoreSurfaceGraphResult r;
    q.position[0][0]=x;q.position[0][1]=.37;q.position[0][2]=.51;
    assert(core_surface_graph_evaluate(g,&q,&r));return r.roughness;
}
int main(void) {
    CoreSurfaceGraph g={0};g.count=3;g.color_output=2;g.roughness_output=1;
    g.nodes[0]=(CoreSurfaceGraphNode){.kind=CORE_SG_COORDINATE,.scale_m=1};
    g.nodes[1]=(CoreSurfaceGraphNode){.kind=CORE_SG_NOISE3D,.inputs={0},.seed=123};
    g.nodes[2]=(CoreSurfaceGraphNode){.kind=CORE_SG_COLOR,.value={1,1,1}};
    assert(core_surface_graph_prepare(&g,NULL,0));
    printf("{\"seed\":123,\"negative_x\":%.15g,\"wrapped_negative_x\":%.15g,\"seam_left\":%.15g,\"seam_right\":%.15g}\n",sample(&g,-.25),sample(&g,1048576-.25),sample(&g,1048576-1e-6),sample(&g,1048576+1e-6));return 0;
}
