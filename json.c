#include "json.h"

#include <stdarg.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <assert.h>

/*
    Baby steps:

        * Create an interface for objects to integers.
        * Create a ring-buffer allocator.
        * Think of way to simplify push.
        * Objects as skip lists.
        * Pack arrays with type elements and data elements.
        * Define a way to extract to a structure, visit and populate.
 */

static uint32_t wordsof (uint32_t bytes) {
    assert(bytes % sizeof(uint64_t) == 0);
    return bytes / sizeof(uint64_t);
}

void json_heap_init(json_heap_t* heap, void* memory, size_t length)
{
    uint32_t words = wordsof(length);
    heap->header = (json_heap_header_t*) memory;
    heap->memory = (uint64_t*) memory;
    heap->header->top = wordsof(sizeof(json_heap_header_t));
    heap->header->bottom = heap->header->length = words;
    json_node_t* root = (json_node_t*) heap->memory + heap->header->top;
    heap->header->top += wordsof(sizeof(json_node_t));
    root->type = Object;
    root->header.next = 0;
    root->value.object.head = 0;
}

static int has_reference (json_heap_t* heap, json_t* ref)
{
    json_t* iterator;
    iterator = heap->refs;
    while (iterator != NULL) {
        if (iterator == ref) {
            return 1;
        }
        iterator = iterator->next;
    }
    return 0;
}

static void join_ref (json_heap_t* heap, json_t* ref)
{
    if (! has_reference(heap, ref)) {
        ref->next = heap->refs;
        heap->refs = ref;
    }
}

void json_root (json_heap_t* heap, json_t* ref)
{
    join_ref(heap, ref);
    ref->heap = heap;
    ref->offset = wordsof(sizeof(json_heap_header_t));
}

static uint32_t json_alloc_node (json_heap_t* heap)
{
    const uint32_t allocated = heap->header->top;
    heap->header->top += 2;
    return allocated;
}

static json_node_t* json_dereference (json_heap_t* heap, uint32_t offset) {
    const uint8_t* memory = (uint8_t*) heap->header;
    return (json_node_t*) (memory + (offset * sizeof(uint64_t)));
}

typedef struct json_region_s json_region_t;

struct json_region_s
{
    uint32_t referrer;
    uint32_t length;
};

static int json_get_region (json_heap_t* heap, uint32_t offset, json_region_t** region) {
    const uint8_t* memory = (uint8_t*) heap->header;
    *region = (json_region_t*) (memory + (offset * sizeof(uint64_t)));
    return 0;
}

static void* json_get_region_buffer (json_region_t* region) {
    return ((uint8_t*) region) + sizeof(json_region_t);
}

static uint32_t json_word_length (uint32_t length) {
    return length + (sizeof(uint64_t) - 1) / sizeof(uint64_t);
}

void json_create_object (json_t* ref, const char* name)
{
    json_node_t* object = json_dereference(ref->heap, ref->offset);

    assert(object->type == Object);

    uint32_t offset = object->value.object.head;
    json_node_t* previous = NULL;
    while (ref->offset != offset) {
        assert(0);
    }

    const uint8_t key_offset = json_alloc_node(ref->heap);
    json_node_t* key = json_dereference(ref->heap, key_offset);

    if (previous == NULL) {
        key->header.next = ref->offset;
        object->value.object.head = key_offset;
    } else {
    }

    {
        const size_t length = strlen(name);
        const uint32_t words = json_word_length(length) + sizeof(json_region_t) / sizeof(uint64_t);
        const uint32_t region_offset = ref->heap->header->bottom -= words;
        json_region_t* region;
        json_get_region(ref->heap, region_offset, &region);
        region->referrer = key_offset;
        region->length = length;
        char* value = json_get_region_buffer(region);
        strcpy(value, name);
        key->value.key.string = region_offset;
    }

    const uint8_t object_offset = json_alloc_node(ref->heap);
    json_node_t* new_object = json_dereference(ref->heap, object_offset);

    new_object->type = Object;
    new_object->value.object.head = new_object->header.next = object_offset;

    new_object->header.next = key->header.next;
    key->header.next = object_offset;
}

static void json_unlink_ref (json_t* ref) {
    json_t* iterator = ref->heap->refs, *previous = NULL;
    while (iterator != ref) {
        assert(iterator != NULL);
        previous = iterator;
        iterator = iterator->next;
    }
    if (previous == NULL) {
        ref->heap->refs = iterator->next;
    } else {
        previous->next = iterator->next;
    }
}

static int json_get_node(json_heap_t* heap, uint32_t offset, json_node_t** node)
{
    *node = (json_node_t*) heap + offset;
    return 0;
}

static int json_get_property (
    json_heap_t* heap, const json_node_t* object, const char* name,
    uint32_t* referrer_offset, uint32_t* found_offset
) {
    int err;
    uint32_t key_offset;
    *found_offset = 0;
    json_node_t* iterator;
    err = json_get_node(heap, object->value.object.head, &iterator);
    if (err != 0) {
        return err;
    }
    *referrer_offset = object->value.object.head;
    while (iterator != object) {
        json_region_t* region;
        err = json_get_region(heap, iterator->value.key.string, &region);
        if (err != 0) {
            return err;
        }
        const char* current_name = json_get_region_buffer(region);
        int compare = strcmp(name, current_name);
        if (compare == 0) {
            *found_offset = key_offset;
        } else if (compare > 0) {
            break;
        } else {
            *referrer_offset = iterator->header.next;
            err = json_get_node(heap, iterator->header.next, &iterator);
            if (err != 0) {
                return err;
            }
            key_offset = iterator->header.next;
            err = json_get_node(heap, iterator->header.next, &iterator);
            if (err != 0) {
                return err;
            }
        }
    }
    return 0;
}

static int json_get_path (json_heap_t* heap, json_node_t* object, uint32_t* offset, va_list ap)
{
    int err;

    uint32_t referrer_offset;
    uint32_t child_offset;

    json_node_t* parent = object;
    json_node_t* child;

    *offset = 0;

    const char* name;

    for (;;) {
        name = va_arg(ap, const char*);
        if (name == NULL || child == NULL) {
            break;
        }
        parent = child;
        err = json_get_property(heap, parent, name, &referrer_offset, &child_offset);
        if (err != 0) {
            break;
        }
        if (child_offset != 0) {
            json_get_node(heap, child_offset, &child);
        } else {
            child = NULL;
        }
    }

    if (err != 0 || child == NULL) {
        return err;
    }

    *offset = child_offset;

    return 0;
}

// In order to reduce the amount of error validation, we only ever return a
// value if it is valid...

// We can break most iteration by returning null. If we encounter an error
// durring deferencing, we can leave an error in `heap`.

int json_get_object (json_t* object, json_t* result, ...)
{
    int err;

    json_heap_t* heap = object->heap;
    json_node_t* parent;
    json_node_t* child;
    uint32_t child_offset = 0;
    uint32_t referrer_offset = 0;

    join_ref(object->heap, result);
    result->offset = 0;

    err = json_get_node(object->heap, object->offset, &parent);
    if (err != 0) {
        return err;
    }

    const char* name;
    va_list ap;
    va_start(ap, result);

    for (;;) {
        name = va_arg(ap, const char*);
        if (name == NULL || child == NULL) {
            break;
        }
        parent = child;
        err = json_get_property(heap, parent, name, &referrer_offset, &child_offset);
        if (err != 0) {
            break;
        }
        if (child_offset != 0) {
            json_get_node(heap, child_offset, &child);
        } else {
            child = NULL;
        }
    }

    va_end(ap);

    if (err != 0) {
        return err;
    }

    if (child == NULL) {
        return 0;
    }

    json_node_t* value;
    err = json_get_node(heap, child->header.next, &value);

    if (value->type != Object) {
        return 0;
    }

    result->offset = child_offset;

    return 0;
}

int json_set_number (json_t* object, const char* name, double number)
{
    json_heap_t* heap = object->heap;
    json_node_t* object_node;
    json_get_node(heap, object->offset, &object_node);
    uint32_t referrer_offset, key_offset;
    json_get_property(heap, object_node, name, &referrer_offset, &key_offset);
    json_node_t* referrer;
    json_get_node(heap, referrer_offset, &referrer);
    if (key_offset != 0) {
    } else {
        // **TODO** json_can_alloc(2) so we don't have trouble releasing.
        uint32_t key_offset = json_alloc_node(heap);
        json_node_t* key;
        json_get_node(heap, key_offset, &key);
        uint32_t value_offset = json_alloc_node(heap);
        json_node_t* value;
        json_get_node(heap, value_offset, &value);
        key->header.next = value_offset;
        value->value.number = number;
        if (referrer->type == Key) {
        } else if (referrer->type == Object) {
            value->header.next = object_node->value.object.head;
            object_node->value.object.head = key_offset;
        }
    }
    return 0;
}

json_number_t json_get_number (json_t* object, ...)
{
    int err = 0;
    double number = 0;

    uint32_t key_offset = 0;

    json_heap_t* heap = object->heap;

    json_node_t* object_node;
    err = json_get_node(heap, object->offset, &object_node);
    if (err != 0) {
        goto exit;
    }

    va_list ap;
    va_start(ap, object);
    json_get_path(heap, object_node, &key_offset, ap);
    va_end(ap);

    if (key_offset == 0) {
        err = -1;
        goto exit;
    }

    json_node_t* key_node;
    json_get_node(heap, key_offset, &key_node);

    json_node_t* value_node;
    json_get_node(heap, key_node->header.next, &value_node);

    if (value_node->type != Number) {
        err = -1;
        goto exit;
    }

    number = value_node->value.number;

exit:
    return (json_number_t) { .err = err, .number = number };
}
