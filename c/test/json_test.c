#include <stdint.h>
#include "ok.h"
#include "json.h"
#include <string.h>
#include <stdio.h>

#define foo .type = JSON_STRING
#define json_key_string(key_, value_) .type = JSON_STRING, .key = key_, .value.string = value_
#define json_key_string2(key_, value_)  { .type = JSON_STRING, .key = key_, .value.string = value_ }
#define json_key_number(key_, value_) .type = JSON_NUMBER, .key = key_, .value.number = value_
#define json_key_object(key_) .type = JSON_OBJECT, .key = key_, .value.object = (json_variant_t[])
#define json_string_(value_) .type = JSON_STRING, .value.string = value_
#define json_number(value_) .type = JSON_STRING, .value.number = value_
#define json_object .type = JSON_OBJECT, .value.object = (json_variant_t[])
#define json_end .type = JSON_END

#define json_key_object_(key_, body_) (json_variant_t) { \
    .type = JSON_OBJECT, .key = key_, .value.object = (json_variant_t[]) { \
        body_\
      , { json_end } } \
}

int main()
{
    plan(5);

    char memory[1024 * 1024];
    json_heap_t heap;
    json_t root = {};
    json_t object = {};

    json_heap_init(&heap, memory, sizeof(memory));
    json_root(&heap, &root);
    ok(json_set_object(&root, "object", NULL) == 0, "set object");

    json_get_object(&root, &object, "object", NULL);

    ok(json_set_number(&object, "number", 1) == 0, "set number");

    // Assume it is easier to check before you proceed, so all the gets will
    // assert on type, no coercion, so you must check types before you
    // dereference when information is coming from the outside in.

    double number = json_get_number(&object, "number", NULL).number;

    ok(number == 1, "set and get number");

    ok(json_set_string(&object, "string", "hello, world") == 0, "set string");

    json_t string;
    json_get_string(&object, &string, "string", NULL);

    ok(strcmp(json_string(&string), "hello, world") == 0, "get and set string");

    const char* key = "key";
    const char* value = "value";

    json_set_object(&object, "variant", (json_variant_t[]) {
        { json_key_string("string", "hello, world") },
        { json_key_number("number", 2) },
        /* { json_key_object("object") { { json_end } } }, */
        { json_end }
    });

    return 0;
}
