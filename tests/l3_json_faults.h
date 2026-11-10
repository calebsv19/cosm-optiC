#include <json-c/json.h>
static int l3_json_index,l3_json_fail;
static inline int l3_json_fault(void){return ++l3_json_index==l3_json_fail;}
static inline json_object *l3_object(void){return l3_json_fault()?NULL:json_object_new_object();}
static inline json_object *l3_string(const char *s){return l3_json_fault()?NULL:json_object_new_string(s);}
static inline json_object *l3_int(int v){return l3_json_fault()?NULL:json_object_new_int(v);}
static inline json_object *l3_int64(int64_t v){return l3_json_fault()?NULL:json_object_new_int64(v);}
static inline json_object *l3_boolean(json_bool v){return l3_json_fault()?NULL:json_object_new_boolean(v);}
static inline int l3_add(json_object *o,const char *key,json_object *v){return l3_json_fault()?-1:json_object_object_add(o,key,v);}
#define json_object_new_object l3_object
#define json_object_new_string l3_string
#define json_object_new_int l3_int
#define json_object_new_int64 l3_int64
#define json_object_new_boolean l3_boolean
#define json_object_object_add l3_add
