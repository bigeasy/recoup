#include "json.h"

#include <memory.h>
#include <string.h>

/*
    Baby steps:

        * Create an interface for objects to integers.
        * Create a ring-buffer allocator.
 */
void json_heap_initialize(json_heap_t* json_heap)
{
     json_heap->block = malloc(1024);
     json_heap->_next_id = 0;
     json_heap->block->id = json_heap->_next_id++;
     json_heap->block->size = 1024 / sizeof(uint64_t);
     json_heap->block->top = sizeof(json_heap_block_t);
}

json_t json_create_object(json_heap_t* json_heap)
{
    json_entity_t* entity = (json_entity_t*) (((uint8_t*) json_heap->block) + json_heap->block->top);
    json_reference_t address = json_heap->block->top;
    json_heap->block->top += sizeof(json_entity_t);
    entity->type = Object;
    entity->object.head = 0;
    entity->object.size = sizeof(json_entity_t);
    return (json_t) { .heap = json_heap, .address = address, .entity = entity };
}

void json_set_integer(json_t object, const char* key, uint32_t value)
{
    json_entity_t* entity;
    if (object.entity->object.head == 0) {
        entity = (json_entity_t*) (((uint64_t*) object.heap->block) + object.heap->block->top);
        object.heap->block->top += sizeof (json_entity_t) / sizeof (uint64_t);
        entity->type = Entry;
        entity->entry.key = object.heap->block->top;
        strcpy((char*) object.heap->block, key);
        entity->entry.value.integer = value;
    }
}

void* json_get_block(json_heap_t* heap, uint64_t location) {
    return heap->block + location;
}

uint64_t* json_get_integer(json_t object, const char* key)
{
    if (object.entity->object.head == 0) {
        return NULL;
    }
    char* existing_key = (char*) json_get_block(object.heap, object.entity->entry.key);
    if (strcmp(existing_key, key) == 0) {
        return &object.entity->entry.value.integer;
    }
    return NULL;
}
