#include <stdint.h>

#include "json.h"

#include <stdarg.h>
#include <memory.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>

typedef struct json_node_s json_node_t;

struct json_node_s
{
    uint32_t packed;
    uint32_t next;
    union {
        struct {
            uint32_t head;
        } list;
        struct {
            uint32_t offset;
            uint32_t previous;
        } ref;
        struct {
            uint32_t string;
        } key;
        struct {
            uint32_t offset;
            uint32_t bytes;
        } alloc;
        double number;
        uint64_t integer;
    } value;
};

#define json_set_node_type(node, value) (json_set_node_packed_field(node, 27, 5, value))
#define json_get_node_type(node) (json_get_node_packed_field(node, 27, 5))
#define json_set_node_alloc_failure(node, value) (json_set_node_packed_field(node, 26, 1, value))
#define json_get_node_alloc_failure(node) (json_get_node_packed_field(node, 26, 1))

// Debugger steppable for now, inline function later.

//
static uint32_t json_bit_field_set (uint32_t ui, uint32_t offset, uint32_t bits, uint32_t value)
{
    uint32_t mask = 0Xffffffff;
    mask = mask >> (32 - bits);
    mask = mask << offset;
    ui = ui & ~mask;
    value = value << offset;
    return ui | value;
}

static uint32_t json_bit_field_get (uint32_t ui, uint32_t offset, uint32_t bits)
{
    uint32_t mask = 0Xffffffff;
    mask = mask >> (32 - bits);
    return (ui >> offset) & mask;
}

static void json_set_node_packed_field (json_node_t* node, uint32_t offset, uint32_t bits, uint32_t value)
{
    node->packed = json_bit_field_set(node->packed, offset, bits, value);
}

static uint32_t json_get_node_packed_field (json_node_t* node, uint32_t offset, uint32_t bits)
{
    return json_bit_field_get(node->packed, offset, bits);
}

/*
    Baby steps:

        * Create a ring-buffer allocator.
        * Think of way to simplify push.
        * Objects as skip lists.
        * Pack arrays with type elements and data elements.
        * Define a way to extract to a structure, visit and populate.
 */

typedef struct json_region_s json_region_t;

struct json_region_s
{
    uint32_t referrer;
    uint32_t length;
};

static uint32_t wordsof (uint32_t bytes) {
    assert(bytes % sizeof(uint64_t) == 0);
    return bytes / sizeof(uint64_t);
}

static json_node_t* json_get_node_ (json_heap_t* heap, uint32_t offset)
{
    return (json_node_t*) (heap->memory + offset);
}

static uint32_t json_alloc_node (json_heap_t* heap)
{
    const uint32_t allocated = heap->header->top;
    heap->header->top += 2;
    json_node_t* node = json_get_node_(heap, allocated);
    node->packed = 0;
    node->next = 0;
    node->value.integer = 0;
    return allocated;
}

static uint32_t json_get_node_offset (json_heap_t* heap, json_node_t* node)
{
    return ((uint64_t*) node) - heap->memory;
}

static json_node_t* json_get_system_node (json_heap_t* heap, uint32_t type)
{
    uint32_t root_offset = wordsof(sizeof(json_heap_header_t));

    json_node_t* node = json_get_node_(heap, root_offset);

    while (json_get_node_type(node) != type) {
        node = json_get_node_(heap, node->next);
    }

    return node;
}

void json_heap_init(json_heap_t* heap, void* memory, size_t length)
{
    uint32_t words = wordsof(length);
    heap->header = (json_heap_header_t*) memory;
    heap->memory = (uint64_t*) memory;
    heap->refs = NULL;
    heap->header->top = wordsof(sizeof(json_heap_header_t));
    heap->header->bottom = heap->header->length = words;

    uint32_t root_offset = json_alloc_node(heap);
    json_node_t* root = json_get_node_(heap, root_offset);

    uint32_t free_list_offset = json_alloc_node(heap);
    json_node_t* free_list = json_get_node_(heap, free_list_offset);

    uint32_t stack_offset = json_alloc_node(heap);
    json_node_t* stack = json_get_node_(heap, stack_offset);

    json_set_node_type(root, JSON_OBJECT);
    root->next = free_list_offset;
    root->value.list.head = root_offset;

    json_set_node_type(free_list, JSON_FREE_LIST);
    free_list->next = stack_offset;

    json_set_node_type(stack, JSON_STACK);
    stack->next = root_offset;
    stack->value.list.head = stack_offset;
}

static int has_reference (json_heap_t* heap, json_t* ref)
{
    json_t* iterator = heap->refs;
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
        ref->heap = heap;
    }
}

static uint32_t json_word_length (uint32_t length) {
    uint32_t go_over = length + sizeof(uint32_t);
    go_over++;
    uint32_t rounded = go_over / sizeof(uint64_t);
    return rounded;
    // return (length + (sizeof(uint64_t) - 1)) / sizeof(uint64_t);
}

static int json_get_region (json_heap_t* heap, uint32_t offset, json_region_t** region) {
    const uint8_t* memory = (uint8_t*) heap->header;
    *region = (json_region_t*) (memory + (offset * sizeof(uint64_t)));
    return 0;
}

static int json_alloc_region (json_heap_t* heap, uint32_t referrer_offset, uint32_t length)
{
    const uint32_t words = json_word_length(length) + wordsof(sizeof(json_region_t));
    heap->header->bottom -= words;
    const uint32_t region_offset = heap->header->bottom;
    json_region_t* region;
    json_get_region(heap, region_offset, &region);
    region->referrer = referrer_offset;
    region->length = length;
    return region_offset;
}

static void json_list_link (json_heap_t* heap, json_node_t* list, json_node_t* node)
{
    node->next = list->value.list.head;
    list->value.list.head = json_get_node_offset(heap, node);
}

static json_node_t* json_alloc_push (json_heap_t* heap) {
    uint32_t alloc_offset = json_get_system_node(heap, JSON_FREE_LIST)->next;
    json_node_t* alloc = json_get_node_(heap, alloc_offset);

    const uint32_t node_offset = heap->header->top;
    heap->header->top += wordsof(sizeof(json_node_t));

    json_set_node_alloc_failure(alloc, 0);

    json_node_t* node = json_get_node_(heap, node_offset);
    node->packed = 0;
    json_set_node_type(node, JSON_ALLOC_NODE);
    node->value.alloc.offset = 0;
    node->value.alloc.bytes = 0;

    json_list_link(heap, alloc, node);

    return node;
}

static void json_alloc_push_node (json_heap_t* heap)
{
    json_alloc_push(heap);
}

static void json_alloc_push_region (json_heap_t* heap, uint32_t bytes)
{
    json_node_t* node = json_alloc_push(heap);
    if (node != NULL) {
        node->value.alloc.bytes = bytes;
        node->value.alloc.offset = json_alloc_region(heap, json_get_node_offset(heap, node), node->value.alloc.bytes);
        if (node->value.alloc.offset == 0) {
            uint32_t alloc_offset = json_get_system_node(heap, JSON_FREE_LIST)->next;
            json_node_t* alloc = json_get_node_(heap, alloc_offset);
            json_set_node_alloc_failure(alloc, 1);
        }
    }
}

static void json_free_node (json_heap_t* heap, json_node_t* node)
{
    uint32_t node_offset = json_get_node_offset(heap, node);
    json_node_t* root = json_get_system_node(heap, JSON_OBJECT);
    json_node_t* free_list = json_get_node_(heap, root->next);
    json_list_link(heap, free_list, node);
}

static void json_alloc_unwind (json_heap_t* heap)
{
    uint32_t stack_offset = json_get_system_node(heap, JSON_FREE_LIST)->next;
    json_node_t* stack = json_get_node_(heap, stack_offset);

    json_node_t* root = json_get_node_(heap, wordsof(sizeof(json_heap_header_t)));
    uint32_t free_list_offset = root->next;
    json_node_t* free_list = json_get_node_(heap, root->next);

    while (stack->value.list.head != stack_offset) {
        uint32_t top_offset = stack->value.list.head;
        json_node_t* top = json_get_node_(heap, top_offset);
        uint32_t next_offset = top->next;
        if (free_list->value.list.head == free_list_offset) {
            top->next = free_list_offset;
        } else {
            top->next = free_list->value.list.head;
        }
        free_list->value.list.head = top_offset;
        stack->value.list.head = next_offset;
    }
}

typedef struct json_pop_s json_pop_t;

struct json_pop_s
{
    json_node_t* node;
    uint32_t offset;
};

static json_node_t* json_list_pop (json_heap_t* heap, json_node_t* list)
{
    if (list->value.list.head == json_get_node_offset(heap, list)) {
        return NULL;
    }
    json_node_t* node = json_get_node_(heap, list->value.list.head);
    list->value.list.head = node->next;
    return node;
}

static void json_list_unlink (json_heap_t* heap, json_node_t* list, json_node_t* node)
{
    uint32_t node_offset = json_get_node_offset(heap, node);
    if (list->value.list.head == node_offset) {
        list->value.list.head = node->next;
    } else {
        json_node_t* iterator = json_get_node_(heap, list->value.list.head);
        while (iterator->next != node->next) {
            iterator = json_get_node_(heap, list->value.list.head);
        }
        iterator->next = node->next;
    }
}

static int json_alloc (json_heap_t* heap)
{
    uint32_t alloc_offset = json_get_system_node(heap, JSON_FREE_LIST)->next;
    json_node_t* alloc = json_get_node_(heap, alloc_offset);

    if (json_get_node_alloc_failure(alloc)) {
        // **TODO** Unwind.
        return 0;
    }

    return 1;
}

static json_node_t* json_alloc_pop (json_heap_t* heap)
{
    uint32_t alloc_offset = json_get_system_node(heap, JSON_FREE_LIST)->next;
    json_node_t* alloc = json_get_node_(heap, alloc_offset);
    json_node_t* top = json_list_pop(heap, alloc);
    top->packed = 0;
    return top;
}

static uint32_t json_alloc_free (json_heap_t* heap, uint32_t stack_offset)
{
    return 0;
}

void json_root (json_heap_t* heap, json_t* ref)
{
    join_ref(heap, ref);
    ref->offset = wordsof(sizeof(json_heap_header_t));
}

static json_region_t* json_get_region_ (json_heap_t* heap, uint32_t offset) {
    return (json_region_t*) (heap->memory + offset);
}

static void* json_get_buffer (json_region_t* region)
{
    return ((uint8_t*) region) + sizeof(json_region_t);
}

static int json_get_property (
    json_heap_t* heap, const uint32_t object_offset, const char* name,
    uint32_t* referrer_offset, uint32_t* found_offset
) {
    int err;
    uint32_t key_offset;
    *found_offset = 0;
    json_node_t* object = json_get_node_(heap, object_offset);
    json_node_t* iterator = json_get_node_(heap, object->value.list.head);
    *referrer_offset = object_offset;
    key_offset = object->value.list.head;
    while (iterator != object) {
        json_region_t* region;
        err = json_get_region(heap, iterator->value.key.string, &region);
        if (err != 0) {
            return err;
        }
        const char* current_name = json_get_buffer(region);
        int compare = strcmp(name, current_name);
        if (compare == 0) {
            *found_offset = key_offset;
            break;
        } else if (compare > 0) {
            break;
        } else {
            *referrer_offset = iterator->next;
            iterator = json_get_node_(heap, iterator->next);
            key_offset = iterator->next;
            iterator = json_get_node_(heap, iterator->next);
        }
    }
    return 0;
}

static void json_set_property_ (json_heap_t* heap, json_node_t* object, json_node_t* key) {
    const char* name = json_get_buffer(json_get_region_(heap, key->value.key.string));

    uint32_t referrer_offset, key_offset;
    json_get_property(heap, json_get_node_offset(heap, object), name, &referrer_offset, &key_offset);

    json_node_t* value = json_get_node_(heap, key->next);

    if (json_get_node_offset(heap, object) == referrer_offset) {
        value->next = referrer_offset;
        object->value.list.head = json_get_node_offset(heap, key);
    } else {
        json_node_t* referrer = json_get_node_(heap, referrer_offset);
        value->next = referrer->next;
        referrer->next = json_get_node_offset(heap, key);
    }
}

static json_node_t* json_construct_key (json_heap_t* heap, const char* name, json_node_t* value)
{
    json_node_t* key = json_alloc_pop(heap);
    json_set_node_type(key, JSON_KEY);

    key->next = json_get_node_offset(heap, value);

    json_region_t* region = json_get_region_(heap, key->value.key.string);
    strcpy(json_get_buffer(region), name);

    return key;
}

static int json_set_object_ (json_heap_t* heap, uint32_t object_offset, const char* name)
{
    json_node_t* object = json_get_node_(heap, object_offset);

    assert(json_get_node_type(object) == JSON_OBJECT);

    json_alloc_push_region(heap, strlen(name));
    json_alloc_push_node(heap);

    if (!json_alloc(heap)) {
        return 1;
    }

    json_node_t* value = json_alloc_pop(heap);
    json_set_node_type(value, JSON_OBJECT);
    value->value.list.head = value->next = json_get_node_offset(heap, value);

    json_node_t* key = json_construct_key(heap, name, value);

    json_set_property_(heap, object, key);

    return 0;
}

int json_set_object (json_t* object, const char* name)
{
    return json_set_object_ (object->heap, object->offset, name);
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

static int json_get_path (json_heap_t* heap, uint32_t object_offset, uint32_t* found_offset, va_list ap)
{
    int err;

    uint32_t referrer_offset;
    uint32_t child_offset = object_offset;
    uint32_t parent_offset;

    *found_offset = 0;

    const char* name;

    for (;;) {
        name = va_arg(ap, const char*);
        if (name == NULL || child_offset == 0) {
            break;
        }
        parent_offset = child_offset;
        err = json_get_property(heap, parent_offset, name, &referrer_offset, &child_offset);
        if (err != 0) {
            break;
        }
    }

    if (err != 0 || child_offset == 0) {
        return err;
    }

    *found_offset = child_offset;

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
    uint32_t child_offset = object->offset;
    uint32_t key_offset = 0;

    join_ref(object->heap, result);
    result->offset = 0;

    va_list ap;
    va_start(ap, result);
    err = json_get_path(heap, object->offset, &key_offset, ap);
    va_end(ap);

    if (err != 0 || key_offset == 0) {
        return err;
    }

    json_node_t* key = json_get_node_(heap, key_offset);

    json_node_t* value = json_get_node_(heap, key->next);

    if (json_get_node_type(value) != JSON_OBJECT) {
        return 0;
    }

    result->offset = value->value.list.head;

    return 0;
}

static int json_set_number_ (json_heap_t* heap, uint32_t object_offset, const char* name, double number)
{
    json_node_t* object = json_get_node_(heap, object_offset);

    assert(json_get_node_type(object) == JSON_OBJECT);

    json_alloc_push_region(heap, strlen(name));
    json_alloc_push_node(heap);

    if (!json_alloc(heap)) {
        return 1;
    }

    json_node_t* value = json_alloc_pop(heap);
    json_set_node_type(value, JSON_NUMBER);
    value->value.number = number;

    json_node_t* key = json_construct_key(heap, name, value);

    json_set_property_(heap, object, key);

    return 0;
}

int json_set_number (json_t* object, const char* name, double number)
{
    return json_set_number_(object->heap, object->offset, name, number);
}

json_number_t json_get_number (json_t* object, ...)
{
    int err;
    double number = 0;

    uint32_t key_offset = 0;

    json_heap_t* heap = object->heap;

    va_list ap;
    va_start(ap, object);
    err = json_get_path(heap, object->offset, &key_offset, ap);
    va_end(ap);

    if (key_offset == 0) {
        err = -1;
        goto exit;
    }

    json_node_t* key_node = json_get_node_(heap, key_offset);

    json_node_t* value_node = json_get_node_(heap, key_node->next);

    if (json_get_node_type(value_node) != JSON_NUMBER) {
        err = -1;
        goto exit;
    }

    number = value_node->value.number;

exit:
    return (json_number_t) { .err = err, .number = number };
}

static int json_set_string_ (json_heap_t* heap, uint32_t object_offset, const char* name, const char* string)
{
    json_node_t* object = json_get_node_(heap, object_offset);

    assert(json_get_node_type(object) == JSON_OBJECT);

    json_alloc_push_region(heap, strlen(name));
    json_alloc_push_region(heap, strlen(string));

    if (!json_alloc(heap)) {
        return 1;
    }

    // **TODO** Assert that the list is empty, dealloc pop list.
    json_node_t* value = json_alloc_pop(heap);
    json_set_node_type(value, JSON_STRING);
    json_region_t* region = json_get_region_(heap, value->value.ref.offset);
    strcpy(json_get_buffer(region), string);

    json_node_t* key = json_construct_key(heap, name, value);

    value->value.ref.previous = json_get_node_offset(heap, key);

    json_set_property_(heap, object, key);

    return 0;
}

int json_set_string (json_t* object, const char* name, const char* string)
{
    return json_set_string_(object->heap, object->offset, name, string);
}

void json_get_string (json_t* object, json_t* ref, ...)
{
    int err;
    double number = 0;

    uint32_t key_offset = 0;

    json_heap_t* heap = object->heap;

    va_list ap;
    va_start(ap, ref);
    err = json_get_path(heap, object->offset, &key_offset, ap);
    va_end(ap);

    ref->offset = 0;

    if (key_offset == 0) {
        return;
    }

    json_node_t* key = json_get_node_(heap, key_offset);

    json_node_t* value = json_get_node_(heap, key->next);

    // **TODO** Do we coerce the way JSON does?
    uint32_t field = 0;
    field = json_bit_field_set(field, 27, 5, 4);
    if (json_get_node_type(value) != JSON_STRING) {
        return;
    }

    join_ref(heap, ref);

    ref->offset = json_get_node_offset(heap, value);
}

const char* _json_string (json_heap_t* heap, uint32_t string_offset)
{
    json_node_t* value = json_get_node_(heap, string_offset);
    assert(json_get_node_type(value) == JSON_STRING);
    return json_get_buffer(json_get_region_(heap, value->value.ref.offset));
}

const char* json_string (json_t* string)
{
    return _json_string(string->heap, string->offset);
}
