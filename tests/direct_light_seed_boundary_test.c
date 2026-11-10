#include <assert.h>
#include <math.h>
#include "../src/render/runtime_direct_light_source_3d.c"
int main(void){
 assert(runtime_direct_light_3d_seed_coordinate(-1.)==0);
 assert(runtime_direct_light_3d_seed_coordinate(-INFINITY)==0);
 assert(runtime_direct_light_3d_seed_coordinate(NAN)==0);
 assert(runtime_direct_light_3d_seed_coordinate(1.)==4096);
 assert(runtime_direct_light_3d_seed_coordinate(1.125)==4608);
 assert(runtime_direct_light_3d_seed_coordinate(INFINITY)==UINT32_MAX);
 assert(runtime_direct_light_3d_seed_coordinate(1e300)==UINT32_MAX);
 assert(runtime_direct_light_3d_seed_coordinate((double)UINT32_MAX/4096.)==UINT32_MAX);
 return 0;
}
