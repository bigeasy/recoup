#include <stdlib.h>

typedef struct json_heap_s json_heap_t;
typedef struct json_heap_header_s json_heap_header_t;
typedef struct json_heap_s json_heap_t;
typedef struct json_s json_t;
typedef struct json_node_s json_node_t;
typedef struct json_heap_block_s json_heap_block_t;
typedef struct json_s json_t;
typedef struct json_number_s json_number_t;

enum json_node_type
{
    End, Object, Key, Entry, Array, Element, String, Float, Integer, Boolean, Null, Number,
    JSON_STRING,
    JSON_NUMBER,
    JSON_OBJECT,
    JSON_ARRAY,
    JSON_NULL,
    JSON_BOOLEAN,
    JSON_END,
    JSON_KEY,
    JSON_INTEGER
};

struct json_node_s
{
    enum json_node_type type;
    union {
        uint32_t next;
    } header;
    union {
        struct {
            uint32_t head;
        } object;
        struct {
            uint32_t next;
            uint32_t string;
        } key;
        double number;
    } value;
};

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
    enum json_node_type type;
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
void json_root(json_heap_t*, json_t*);
void json_create_object(json_t*, const char*);

int json_set_number(json_t* object, const char* name, double number);

void json_set_integer(json_t, const char*, uint32_t);
json_number_t json_get_number(json_t*, ...);
int json_get_object (json_t* object, json_t* result, ...);
