#include <stdlib.h>

typedef struct json_heap_s json_heap_t;
typedef struct json_heap_header_s json_heap_header_t;
typedef struct json_heap_s json_heap_t;
typedef struct json_s json_t;
typedef struct json_heap_block_s json_heap_block_t;
typedef struct json_s json_t;
typedef struct json_number_s json_number_t;

#define JSON_END 0
#define JSON_NULL 1
#define JSON_OBJECT 2
#define JSON_ARRAY 3
#define JSON_KEY 4
#define JSON_BOOLEAN 5
#define JSON_NUMBER 6
#define JSON_STRING 7
#define JSON_FREE_LIST 8
#define JSON_STACK 9
#define JSON_ALLOC_NODE 10
#define JSON_ALLOC_REGION 11

struct json_heap_header_s
{
    uint32_t references;
    uint32_t top;
    uint32_t bottom;
    uint32_t length;
};

struct json_heap_s
{
    json_heap_header_t* header;
    json_t* refs;
    uint64_t* memory;
};

struct json_s
{
    json_t* next;
    json_heap_t* heap;
    size_t offset;
};

struct json_number_s
{
    int err;
    double number;
};

typedef struct object_s object_t;
struct property_s
{
    uint32_t type;
    const char* key;
    union {
        double number;
        const char* string;
        object_t* object;
    } value;
};
typedef struct property_s property_t;

struct object_s
{
    property_t** properties;
};


void json_heap_init(json_heap_t* json_heap, void* memory, size_t length);

void json_heap_copacetic(json_heap_t* json_heap);

void json_root(json_heap_t*, json_t*);

int json_set_object(json_t*, const char*);

int json_set_number(json_t* object, const char* name, double number);

int json_set_string(json_t*, const char*, const char*);

json_number_t json_get_number(json_t*, ...);

int json_get_object (json_t* object, json_t* result, ...);
