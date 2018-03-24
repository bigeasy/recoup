#include <stdlib.h>

typedef struct json_heap_s json_heap_t;
typedef struct json_s json_t;

struct json_heap_s
{
};

struct json_s
{
    uint32_t type;
};

void json_heap_initialize(json_heap_t*);
json_t* json_create_object(json_heap_t*);

void json_set_integer(json_t*, const char*, uint32_t);
uint32_t json_get_integer(json_t*, const char*);
