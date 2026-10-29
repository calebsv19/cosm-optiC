#include "import/runtime_rigid_asset_metadata.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool read(const char *s,uint32_t expected) {
  uint32_t count=0;char id[64],sha[65];double c[3],b[9],m[3];
  bool ok=RuntimeRigidCompoundMetadata(s,strlen(s),&count,id,sha,c,b,m);
  return ok&&count==expected;
}
int main(void) {
  const char *head="ball_collision_representation_artifact_v1\nsource bracket_source 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\nframe_center 0x0p+0 0x0p+0 0x0p+0\nframe_matrix 1 0 0 0 1 0 0 0 1\nframe_moments 1 2 3\n";
  char s[2048];
  snprintf(s,sizeof(s),"%scompound ref 2\npart a a 8 12 0 0 0 1 0 0 0\npart b b 8 12 0 0 0 1 0 0 0\nend\n",head);assert(read(s,2));
  snprintf(s,sizeof(s),"%scompound ref 4\npart a a 8 12\nend\n",head);assert(!read(s,4));
  snprintf(s,sizeof(s),"%scompound ref 2\npart a a 8 12\nend\n",head);assert(!read(s,2));
  snprintf(s,sizeof(s),"%scompound ref 1\npart a a 8 12\nframe_center 0 0 0\nend\n",head);assert(!read(s,1));
  snprintf(s,sizeof(s),"%scompound ref 1\npart a a 1000000 12\nend\n",head);assert(!read(s,1));
  snprintf(s,sizeof(s),"%scompound ref 1\npart a a 8 12\nframe_moments nan 1 1\nend\n",head);assert(!read(s,1));
  snprintf(s,sizeof(s),"%scompound ref 1 trailing\npart a a 8 12\nend\n",head);assert(!read(s,1));
  assert(!read("bad\n",1));
  puts("compound metadata: 8 checks passed");return 0;
}
