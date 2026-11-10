#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
static int fault,creates,capacity_mode,entered,remaining,release_scans;
static pthread_mutex_t gate_mu=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate_cv=PTHREAD_COND_INITIALIZER;
static void *(*capacity_fn)(void*);
static void *capacity_thread(void *p){pthread_mutex_lock(&gate_mu);entered++;pthread_cond_broadcast(&gate_cv);while(!release_scans)pthread_cond_wait(&gate_cv,&gate_mu);pthread_mutex_unlock(&gate_mu);void *result=capacity_fn(p);pthread_mutex_lock(&gate_mu);remaining--;pthread_cond_broadcast(&gate_cv);pthread_mutex_unlock(&gate_mu);return result;}
static void *probe_calloc(size_t n,size_t size){return fault==4?NULL:calloc(n,size);}
#define calloc probe_calloc

static int probe_init(pthread_attr_t *a){return fault==1?ENOMEM:pthread_attr_init(a);}
static int probe_detach(pthread_attr_t *a,int mode){return fault==2?EINVAL:pthread_attr_setdetachstate(a,mode);}
static int probe_create(pthread_t *t,const pthread_attr_t *a,void *(*f)(void*),void *p){creates++;if(capacity_mode){capacity_fn=f;remaining++;return pthread_create(t,a,capacity_thread,p);}return fault?EAGAIN:pthread_create(t,a,f,p);}
#define pthread_attr_init probe_init
#define pthread_attr_setdetachstate probe_detach
#define pthread_create probe_create
#include "../src/ui/menu/menu_catalog_discovery.c"
#undef calloc
#undef pthread_attr_init
#undef pthread_attr_setdetachstate
#undef pthread_create
#define main existing_catalog_main
#include "menu_catalog_discovery_test.c"
#undef main
int main(void){
 for(fault=1;fault<=4;fault++){
  creates=0;menu_catalog_discovery_request(NULL,0,"","","","","");
  if(fault<3 || fault==4)assert(creates==0);
  for(size_t i=0;i<DISCOVERY_SLOTS;i++)assert(!scans[i]);
 }
 fault=0;capacity_mode=1;creates=0;
 char names[41][40];const char *roots[41];for(int i=0;i<41;i++){snprintf(names[i],sizeof(names[i]),"capacity-%d",i);roots[i]=names[i];}
 menu_catalog_discovery_request(roots,41,"","","","","");assert(creates==40);
 pthread_mutex_lock(&gate_mu);while(entered<40)pthread_cond_wait(&gate_cv,&gate_mu);pthread_mutex_unlock(&gate_mu);
 strcpy(names[0],"caller-mutated");assert(!strcmp(scans[1]->root,"capacity-0"));
 menu_catalog_discovery_request(roots+1,40,"","","","","");assert(creates==40);
 pthread_mutex_lock(&gate_mu);release_scans=1;pthread_cond_broadcast(&gate_cv);while(remaining)pthread_cond_wait(&gate_cv,&gate_mu);pthread_mutex_unlock(&gate_mu);
 for(size_t i=0;i<DISCOVERY_SLOTS;i++){assert(scans[i] && !scans[i]->running);free(scans[i]);scans[i]=NULL;}
 capacity_mode=0;puts("PASS catalog allocation refusal, 40 running slots, overflow/dedup, copied inputs and drained reclamation");
 puts("PASS catalog attr/creation refusal retains no scan slot");return existing_catalog_main();
}
