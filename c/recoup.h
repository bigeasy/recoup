#include <stdlib.h>

typedef struct recoup_heap_s recoup_heap_t;
typedef struct recoup_heap_header_s recoup_heap_header_t;
typedef struct recoup_heap_s recoup_heap_t;
typedef struct recoup_s recoup_t;
typedef struct recoup_heap_block_s recoup_heap_block_t;
typedef struct recoup_s recoup_t;
typedef struct recoup_number_s recoup_number_t;

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

struct recoup_heap_header_s
{
    uint32_t references;
    uint32_t top;
    uint32_t bottom;
    uint32_t length;
};

struct recoup_heap_s
{
    recoup_heap_header_t* header;
    recoup_t* refs;
    uint64_t* memory;
};

struct recoup_s
{
    recoup_t* next;
    recoup_heap_t* heap;
    size_t offset;
};

struct recoup_number_s
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

typedef struct recoup_variant_s recoup_variant_t;
struct recoup_variant_s {
    uint32_t type;
    const char* key;
    union {
        const char* string;
        double number;
        uint64_t boolean;
        recoup_variant_t* object;
        recoup_variant_t* array;
    } value;
    uint32_t index;
    recoup_variant_t* parent;
};

void recoup_heap_init(recoup_heap_t* recoup_heap, void* memory, size_t length);

void recoup_heap_copacetic(recoup_heap_t* recoup_heap);

void recoup_root(recoup_heap_t*, recoup_t*);

int recoup_set_object(recoup_t*, const char*, recoup_variant_t*);

int recoup_set_number(recoup_t* object, const char* name, double number);

int recoup_set_string(recoup_t*, const char*, const char*);

recoup_number_t recoup_get_number(recoup_t*, ...);

int recoup_get_object (recoup_t* object, recoup_t* result, ...);

void recoup_get_string (recoup_t* object, recoup_t* string, ...);

const char* recoup_string (recoup_t* string);

int recoup_set_variant (recoup_t* object, const char * name, recoup_variant_t* variant);
