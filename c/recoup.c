#include <stdint.h>

#include "recoup.h"

#include <stdarg.h>
#include <memory.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>

typedef struct recoup_node_s recoup_node_t;

struct recoup_node_s
{
    uint32_t packed;
    union {
        uint32_t next;
        uint32_t key;
    } link;
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
            uint32_t value;
        } key;
        struct {
            uint32_t offset;
            uint32_t bytes;
        } alloc;
        double number;
        uint64_t integer;
    } value;
};

#define recoup_set_node_type(node, value) (recoup_set_node_packed_field(node, 27, 5, value))
#define recoup_get_node_type(node) (recoup_get_node_packed_field(node, 27, 5))
#define recoup_set_node_alloc_failure(node, value) (recoup_set_node_packed_field(node, 26, 1, value))
#define recoup_get_node_alloc_failure(node) (recoup_get_node_packed_field(node, 26, 1))

// Debugger steppable for now, inline function later.

//
static uint32_t recoup_bit_field_set (uint32_t ui, uint32_t offset, uint32_t bits, uint32_t value)
{
    uint32_t mask = 0Xffffffff;
    mask = mask >> (32 - bits);
    mask = mask << offset;
    ui = ui & ~mask;
    value = value << offset;
    return ui | value;
}

static uint32_t recoup_bit_field_get (uint32_t ui, uint32_t offset, uint32_t bits)
{
    uint32_t mask = 0Xffffffff;
    mask = mask >> (32 - bits);
    return (ui >> offset) & mask;
}

static void recoup_set_node_packed_field (recoup_node_t* node, uint32_t offset, uint32_t bits, uint32_t value)
{
    node->packed = recoup_bit_field_set(node->packed, offset, bits, value);
}

static uint32_t recoup_get_node_packed_field (recoup_node_t* node, uint32_t offset, uint32_t bits)
{
    return recoup_bit_field_get(node->packed, offset, bits);
}

/*
    Baby steps:

        * Create a ring-buffer allocator.
        * Think of way to simplify push.
        * Objects as skip lists.
        * Pack arrays with type elements and data elements.
        * Define a way to extract to a structure, visit and populate.
 */

typedef struct recoup_region_s recoup_region_t;

struct recoup_region_s
{
    uint32_t referrer;
    uint32_t length;
};

static uint32_t wordsof (uint32_t bytes) {
    assert(bytes % sizeof(uint64_t) == 0);
    return bytes / sizeof(uint64_t);
}

static recoup_node_t* recoup_get_node_ (recoup_heap_t* heap, uint32_t offset)
{
    return (recoup_node_t*) (heap->memory + offset);
}

static uint32_t recoup_alloc_node (recoup_heap_t* heap)
{
    const uint32_t allocated = heap->header->top;
    heap->header->top += 2;
    recoup_node_t* node = recoup_get_node_(heap, allocated);
    node->packed = 0;
    node->link.next = 0;
    node->value.integer = 0;
    return allocated;
}

static uint32_t recoup_get_node_offset (recoup_heap_t* heap, recoup_node_t* node)
{
    return ((uint64_t*) node) - heap->memory;
}

static recoup_node_t* recoup_get_system_node (recoup_heap_t* heap, uint32_t type)
{
    uint32_t root_offset = wordsof(sizeof(recoup_heap_header_t));

    recoup_node_t* node = recoup_get_node_(heap, root_offset);

    while (recoup_get_node_type(node) != type) {
        node = recoup_get_node_(heap, node->link.next);
    }

    return node;
}

void recoup_heap_init(recoup_heap_t* heap, void* memory, size_t length)
{
    uint32_t words = wordsof(length);
    heap->header = (recoup_heap_header_t*) memory;
    heap->memory = (uint64_t*) memory;
    heap->refs = NULL;
    heap->header->top = wordsof(sizeof(recoup_heap_header_t));
    heap->header->bottom = heap->header->length = words;

    uint32_t root_offset = recoup_alloc_node(heap);
    recoup_node_t* root = recoup_get_node_(heap, root_offset);

    uint32_t free_list_offset = recoup_alloc_node(heap);
    recoup_node_t* free_list = recoup_get_node_(heap, free_list_offset);

    uint32_t stack_offset = recoup_alloc_node(heap);
    recoup_node_t* stack = recoup_get_node_(heap, stack_offset);

    recoup_set_node_type(root, JSON_OBJECT);
    root->link.next = free_list_offset;
    root->value.list.head = root_offset;

    recoup_set_node_type(free_list, JSON_FREE_LIST);
    free_list->link.next = stack_offset;

    recoup_set_node_type(stack, JSON_STACK);
    stack->link.next = root_offset;
    stack->value.list.head = stack_offset;
}

static int has_reference (recoup_heap_t* heap, recoup_t* ref)
{
    recoup_t* iterator = heap->refs;
    while (iterator != NULL) {
        if (iterator == ref) {
            return 1;
        }
        iterator = iterator->next;
    }
    return 0;
}

static void join_ref (recoup_heap_t* heap, recoup_t* ref)
{
    if (! has_reference(heap, ref)) {
        ref->next = heap->refs;
        heap->refs = ref;
        ref->heap = heap;
    }
}

static uint32_t recoup_word_length (uint32_t length) {
    uint32_t go_over = length + sizeof(uint32_t);
    go_over++;
    uint32_t rounded = go_over / sizeof(uint64_t);
    return rounded;
    // return (length + (sizeof(uint64_t) - 1)) / sizeof(uint64_t);
}

static int recoup_get_region (recoup_heap_t* heap, uint32_t offset, recoup_region_t** region) {
    const uint8_t* memory = (uint8_t*) heap->header;
    *region = (recoup_region_t*) (memory + (offset * sizeof(uint64_t)));
    return 0;
}

static int recoup_alloc_region (recoup_heap_t* heap, uint32_t referrer_offset, uint32_t length)
{
    const uint32_t words = recoup_word_length(length) + wordsof(sizeof(recoup_region_t));
    heap->header->bottom -= words;
    const uint32_t region_offset = heap->header->bottom;
    recoup_region_t* region;
    recoup_get_region(heap, region_offset, &region);
    region->referrer = referrer_offset;
    region->length = length;
    return region_offset;
}

static void recoup_list_link (recoup_heap_t* heap, recoup_node_t* list, recoup_node_t* node)
{
    node->link.next = list->value.list.head;
    list->value.list.head = recoup_get_node_offset(heap, node);
}

static recoup_node_t* recoup_alloc_push (recoup_heap_t* heap) {
    uint32_t alloc_offset = recoup_get_system_node(heap, JSON_FREE_LIST)->link.next;
    recoup_node_t* alloc = recoup_get_node_(heap, alloc_offset);

    const uint32_t node_offset = heap->header->top;
    heap->header->top += wordsof(sizeof(recoup_node_t));

    recoup_set_node_alloc_failure(alloc, 0);

    recoup_node_t* node = recoup_get_node_(heap, node_offset);
    node->packed = 0;
    recoup_set_node_type(node, JSON_ALLOC_NODE);
    node->value.alloc.offset = 0;
    node->value.alloc.bytes = 0;

    recoup_list_link(heap, alloc, node);

    return node;
}

static void recoup_alloc_push_node (recoup_heap_t* heap)
{
    recoup_alloc_push(heap);
}

static void recoup_alloc_push_region (recoup_heap_t* heap, uint32_t bytes)
{
    recoup_node_t* node = recoup_alloc_push(heap);
    if (node != NULL) {
        node->value.alloc.bytes = bytes;
        node->value.alloc.offset = recoup_alloc_region(heap, recoup_get_node_offset(heap, node), node->value.alloc.bytes);
        if (node->value.alloc.offset == 0) {
            uint32_t alloc_offset = recoup_get_system_node(heap, JSON_FREE_LIST)->link.next;
            recoup_node_t* alloc = recoup_get_node_(heap, alloc_offset);
            recoup_set_node_alloc_failure(alloc, 1);
        }
    }
}

static void recoup_free_node (recoup_heap_t* heap, recoup_node_t* node)
{
    uint32_t node_offset = recoup_get_node_offset(heap, node);
    recoup_node_t* root = recoup_get_system_node(heap, JSON_OBJECT);
    recoup_node_t* free_list = recoup_get_node_(heap, root->link.next);
    recoup_list_link(heap, free_list, node);
}

static void recoup_alloc_unwind (recoup_heap_t* heap)
{
    uint32_t stack_offset = recoup_get_system_node(heap, JSON_FREE_LIST)->link.next;
    recoup_node_t* stack = recoup_get_node_(heap, stack_offset);

    recoup_node_t* root = recoup_get_node_(heap, wordsof(sizeof(recoup_heap_header_t)));
    uint32_t free_list_offset = root->link.next;
    recoup_node_t* free_list = recoup_get_node_(heap, root->link.next);

    while (stack->value.list.head != stack_offset) {
        uint32_t top_offset = stack->value.list.head;
        recoup_node_t* top = recoup_get_node_(heap, top_offset);
        uint32_t next_offset = top->link.next;
        if (free_list->value.list.head == free_list_offset) {
            top->link.next = free_list_offset;
        } else {
            top->link.next = free_list->value.list.head;
        }
        free_list->value.list.head = top_offset;
        stack->value.list.head = next_offset;
    }
}

typedef struct recoup_pop_s recoup_pop_t;

struct recoup_pop_s
{
    recoup_node_t* node;
    uint32_t offset;
};

static recoup_node_t* recoup_list_pop (recoup_heap_t* heap, recoup_node_t* list)
{
    if (list->value.list.head == recoup_get_node_offset(heap, list)) {
        return NULL;
    }
    recoup_node_t* node = recoup_get_node_(heap, list->value.list.head);
    list->value.list.head = node->link.next;
    return node;
}

static void recoup_list_unlink (recoup_heap_t* heap, recoup_node_t* list, recoup_node_t* node)
{
    uint32_t node_offset = recoup_get_node_offset(heap, node);
    if (list->value.list.head == node_offset) {
        list->value.list.head = node->link.next;
    } else {
        recoup_node_t* iterator = recoup_get_node_(heap, list->value.list.head);
        while (iterator->link.next != node->link.next) {
            iterator = recoup_get_node_(heap, list->value.list.head);
        }
        iterator->link.next = node->link.next;
    }
}

static int recoup_alloc (recoup_heap_t* heap)
{
    uint32_t alloc_offset = recoup_get_system_node(heap, JSON_FREE_LIST)->link.next;
    recoup_node_t* alloc = recoup_get_node_(heap, alloc_offset);

    if (recoup_get_node_alloc_failure(alloc)) {
        // **TODO** Unwind.
        return 0;
    }

    return 1;
}

static recoup_node_t* recoup_alloc_pop (recoup_heap_t* heap)
{
    uint32_t alloc_offset = recoup_get_system_node(heap, JSON_FREE_LIST)->link.next;
    recoup_node_t* alloc = recoup_get_node_(heap, alloc_offset);
    recoup_node_t* top = recoup_list_pop(heap, alloc);
    top->packed = 0;
    return top;
}

static uint32_t recoup_alloc_free (recoup_heap_t* heap, uint32_t stack_offset)
{
    return 0;
}

void recoup_root (recoup_heap_t* heap, recoup_t* ref)
{
    join_ref(heap, ref);
    ref->offset = wordsof(sizeof(recoup_heap_header_t));
}

static recoup_region_t* recoup_get_region_ (recoup_heap_t* heap, uint32_t offset) {
    return (recoup_region_t*) (heap->memory + offset);
}

static void* recoup_get_buffer (recoup_region_t* region)
{
    return ((uint8_t*) region) + sizeof(recoup_region_t);
}

static int recoup_get_property (
    recoup_heap_t* heap, const uint32_t object_offset, const char* name,
    uint32_t* referrer_offset, uint32_t* found_offset
) {
    int err;
    uint32_t key_offset;
    *found_offset = 0;
    recoup_node_t* object = recoup_get_node_(heap, object_offset);
    recoup_node_t* iterator = recoup_get_node_(heap, object->value.list.head);
    *referrer_offset = object_offset;
    key_offset = object->value.list.head;
    while (iterator != object) {
        recoup_region_t* region;
        err = recoup_get_region(heap, iterator->value.key.string, &region);
        if (err != 0) {
            return err;
        }
        const char* current_name = recoup_get_buffer(region);
        int compare = strcmp(name, current_name);
        if (compare == 0) {
            *found_offset = key_offset;
            break;
        } else if (compare > 0) {
            break;
        } else {
            *referrer_offset = key_offset;
            key_offset = iterator->link.next;
            iterator = recoup_get_node_(heap, iterator->link.next);
        }
    }
    return 0;
}

static void recoup_set_property_ (recoup_heap_t* heap, recoup_node_t* object, recoup_node_t* key)
{
    const char* name = recoup_get_buffer(recoup_get_region_(heap, key->value.key.string));

    uint32_t referrer_offset, key_offset;
    recoup_get_property(heap, recoup_get_node_offset(heap, object), name, &referrer_offset, &key_offset);

    recoup_node_t* value = recoup_get_node_(heap, key->link.next);

    if (recoup_get_node_offset(heap, object) == referrer_offset) {
        value->link.next = referrer_offset;
        object->value.list.head = recoup_get_node_offset(heap, key);
    } else {
        recoup_node_t* referrer = recoup_get_node_(heap, referrer_offset);
        value->link.next = referrer->link.next;
        referrer->link.next = recoup_get_node_offset(heap, key);
    }
}

static recoup_node_t* recoup_construct_key (recoup_heap_t* heap, const char* name, recoup_node_t* value)
{
    recoup_node_t* key = recoup_alloc_pop(heap);
    recoup_set_node_type(key, JSON_KEY);

    key->value.key.value = recoup_get_node_offset(heap, value);
    value->link.key = recoup_get_node_offset(heap, key);

    recoup_region_t* region = recoup_get_region_(heap, key->value.key.string);
    strcpy(recoup_get_buffer(region), name);

    return key;
}

static recoup_node_t* recoup_construct_number (recoup_heap_t* heap, double number)
{
    recoup_node_t* value = recoup_alloc_pop(heap);
    recoup_set_node_type(value, JSON_NUMBER);
    value->value.number = number;
    return value;
}

static void recoup_alloc_properties (recoup_heap_t* heap, recoup_variant_t* properties)
{
    recoup_variant_t* iterator = properties;
    uint32_t index = 0;
    do {
        switch (iterator->type) {
        case JSON_STRING:
            recoup_alloc_push_region(heap, strlen(iterator->key) + 1);
            recoup_alloc_push_region(heap, strlen(iterator->value.string) + 1);
            break;
        case JSON_NUMBER:
            recoup_alloc_push_region(heap, strlen(iterator->key) + 1);
            recoup_alloc_push_node(heap);
            break;
        }
    } while ((iterator++)->type != JSON_END);
}

static recoup_node_t* recoup_construct_string (recoup_heap_t* heap, const char* string)
{
    recoup_node_t* value = recoup_alloc_pop(heap);
    recoup_set_node_type(value, JSON_STRING);
    recoup_region_t* region = recoup_get_region_(heap, value->value.ref.offset);
    strcpy(recoup_get_buffer(region), string);
    return value;
}

static void recoup_construct_properties (recoup_heap_t* heap, recoup_node_t* object, recoup_variant_t* properties)
{
    recoup_variant_t* iterator = properties;
    while (iterator->type != JSON_END) {
        iterator++;
    }
    uint32_t previous = object->value.list.head;
    recoup_node_t* key;
    recoup_node_t* value;
    do {
        iterator--;
        switch (iterator->type) {
        case JSON_STRING: {
                value = recoup_construct_string(heap, iterator->value.string);
                key = recoup_construct_key(heap, iterator->key, value);
                value->link.next = previous;
                previous = recoup_get_node_offset(heap, key);
            }
            break;
        case JSON_NUMBER: {
                value = recoup_construct_number(heap, iterator->value.number);
                key = recoup_construct_key(heap, iterator->key, value);
            }
            break;
        }
        value->link.next = previous;
        previous = recoup_get_node_offset(heap, key);
    } while (iterator != properties);
}

// Could remove recursion with a parent and iterator pointer in the variant. We
// don't plan on using this other than declaring the properties on the stack,
// however so recursion is half dozen the other. Also, easier to read. Also,
// easier to reuse the functions.

static int recoup_set_object_ (recoup_heap_t* heap, uint32_t object_offset, const char* name, recoup_variant_t* properties)
{
    recoup_node_t* object = recoup_get_node_(heap, object_offset);

    assert(recoup_get_node_type(object) == JSON_OBJECT);

    recoup_alloc_push_region(heap, strlen(name) + 1);

    if (properties != NULL) {
        recoup_alloc_properties(heap, properties);
    }

    recoup_alloc_push_node(heap);

    if (!recoup_alloc(heap)) {
        return 1;
    }

    recoup_node_t* value = recoup_alloc_pop(heap);
    recoup_set_node_type(value, JSON_OBJECT);
    value->value.list.head = value->link.next = recoup_get_node_offset(heap, value);

    if (properties != NULL) {
        recoup_construct_properties(heap, object, properties);
    }

    recoup_node_t* key = recoup_construct_key(heap, name, value);

    recoup_set_property_(heap, object, key);

    return 0;
}

int recoup_set_object (recoup_t* object, const char* name, recoup_variant_t* properties)
{
    return recoup_set_object_ (object->heap, object->offset, name, properties);
}

static void recoup_unlink_ref (recoup_t* ref) {
    recoup_t* iterator = ref->heap->refs, *previous = NULL;
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

static int recoup_get_path (recoup_heap_t* heap, uint32_t object_offset, uint32_t* found_offset, va_list ap)
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
        err = recoup_get_property(heap, parent_offset, name, &referrer_offset, &child_offset);
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

int recoup_get_object (recoup_t* object, recoup_t* result, ...)
{
    int err;

    recoup_heap_t* heap = object->heap;
    recoup_node_t* parent;
    recoup_node_t* child;
    uint32_t child_offset = object->offset;
    uint32_t key_offset = 0;

    join_ref(object->heap, result);
    result->offset = 0;

    va_list ap;
    va_start(ap, result);
    err = recoup_get_path(heap, object->offset, &key_offset, ap);
    va_end(ap);

    if (err != 0 || key_offset == 0) {
        return err;
    }

    recoup_node_t* key = recoup_get_node_(heap, key_offset);
    recoup_node_t* value = recoup_get_node_(heap, key->value.key.value);

    if (recoup_get_node_type(value) != JSON_OBJECT) {
        return 0;
    }

    result->offset = value->value.list.head;

    return 0;
}

static int recoup_set_number_ (recoup_heap_t* heap, uint32_t object_offset, const char* name, double number)
{
    recoup_node_t* object = recoup_get_node_(heap, object_offset);

    assert(recoup_get_node_type(object) == JSON_OBJECT);

    recoup_alloc_push_region(heap, strlen(name) + 1);
    recoup_alloc_push_node(heap);

    if (!recoup_alloc(heap)) {
        return 1;
    }

    recoup_node_t* value = recoup_construct_number(heap, number);
    recoup_node_t* key = recoup_construct_key(heap, name, value);

    recoup_set_property_(heap, object, key);

    return 0;
}

int recoup_set_number (recoup_t* object, const char* name, double number)
{
    return recoup_set_number_(object->heap, object->offset, name, number);
}

recoup_number_t recoup_get_number (recoup_t* object, ...)
{
    int err;
    double number = 0;

    uint32_t key_offset = 0;

    recoup_heap_t* heap = object->heap;

    va_list ap;
    va_start(ap, object);
    err = recoup_get_path(heap, object->offset, &key_offset, ap);
    va_end(ap);

    if (key_offset == 0) {
        err = 1;
        goto exit;
    }

    recoup_node_t* key_node = recoup_get_node_(heap, key_offset);
    recoup_node_t* value_node = recoup_get_node_(heap, key_node->value.key.value);

    if (recoup_get_node_type(value_node) != JSON_NUMBER) {
        err = 1;
        goto exit;
    }

    number = value_node->value.number;

exit:
    return (recoup_number_t) { .err = err, .number = number };
}

static int recoup_set_string_ (recoup_heap_t* heap, uint32_t object_offset, const char* name, const char* string)
{
    recoup_node_t* object = recoup_get_node_(heap, object_offset);

    assert(recoup_get_node_type(object) == JSON_OBJECT);

    recoup_alloc_push_region(heap, strlen(name) + 1);
    recoup_alloc_push_region(heap, strlen(string) + 1);

    if (!recoup_alloc(heap)) {
        return 1;
    }

    recoup_node_t* value = recoup_construct_string(heap, string);
    recoup_node_t* key = recoup_construct_key(heap, name, value);

    recoup_set_property_(heap, object, key);

    return 0;
}

int recoup_set_string (recoup_t* object, const char* name, const char* string)
{
    return recoup_set_string_(object->heap, object->offset, name, string);
}

void recoup_get_string (recoup_t* object, recoup_t* ref, ...)
{
    int err;
    double number = 0;

    uint32_t key_offset = 0;

    recoup_heap_t* heap = object->heap;

    va_list ap;
    va_start(ap, ref);
    err = recoup_get_path(heap, object->offset, &key_offset, ap);
    va_end(ap);

    ref->offset = 0;

    if (key_offset == 0) {
        return;
    }

    recoup_node_t* key = recoup_get_node_(heap, key_offset);
    recoup_node_t* value = recoup_get_node_(heap, key->value.key.value);

    // **TODO** Do we coerce the way JSON does?
    uint32_t field = 0;
    field = recoup_bit_field_set(field, 27, 5, 4);
    if (recoup_get_node_type(value) != JSON_STRING) {
        return;
    }

    join_ref(heap, ref);

    ref->offset = recoup_get_node_offset(heap, value);
}

const char* _recoup_string (recoup_heap_t* heap, uint32_t string_offset)
{
    recoup_node_t* value = recoup_get_node_(heap, string_offset);
    assert(recoup_get_node_type(value) == JSON_STRING);
    return recoup_get_buffer(recoup_get_region_(heap, value->value.ref.offset));
}

const char* recoup_string (recoup_t* string)
{
    return _recoup_string(string->heap, string->offset);
}

static int recoup_set_variant_ (recoup_heap_t* heap, uint32_t object_offset, const char * name, recoup_variant_t* variant)
{
    return 0;
}

int recoup_set_variant (recoup_t* object, const char * name, recoup_variant_t* variant)
{
    return recoup_set_variant_(object->heap, object->offset, name, variant);
}
