#include <stdlib.h>

typedef struct json_heap_s json_heap_t;
typedef struct json_s json_t;
typedef union json_entity_u json_entity_t;
typedef struct json_heap_block_s json_heap_block_t;
typedef uint32_t json_reference_t;

enum json_entity_type
{
    Object, Entry, Array, Element, String, Float, Integer, Boolean, Null
};

struct json_s
{
    json_heap_t* heap;
    json_reference_t address;
    json_entity_t* entity;
};

union json_entity_u
{
    enum json_entity_type type;
    struct
    {
        json_reference_t head;
        uint64_t size;
    } object;
    struct
    {
        uint64_t key;
        union
        {
            uint64_t integer;
        } value;
    } entry;
};

struct json_heap_block_s
{
    json_heap_block_t* next;
    uint32_t id;
    size_t size;
    size_t top;
};

struct json_heap_s
{
    json_heap_block_t* block;
    uint32_t _next_id;
};

void json_heap_initialize(json_heap_t*);
json_t json_create_object(json_heap_t*);

void json_set_integer(json_t, const char*, uint32_t);
uint64_t* json_get_integer(json_t, const char*);
