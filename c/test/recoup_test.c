#include <stdint.h>
#include "ok.h"
#include "recoup.h"
#include <string.h>
#include <stdio.h>

#define foo .type = JSON_STRING
#define recoup_key_string(key_, value_) .type = JSON_STRING, .key = key_, .value.string = value_
#define recoup_key_string2(key_, value_)  { .type = JSON_STRING, .key = key_, .value.string = value_ }
#define recoup_key_number(key_, value_) .type = JSON_NUMBER, .key = key_, .value.number = value_
#define recoup_key_object(key_) .type = JSON_OBJECT, .key = key_, .value.object = (recoup_variant_t[])
#define recoup_string_(value_) .type = JSON_STRING, .value.string = value_
#define recoup_number(value_) .type = JSON_STRING, .value.number = value_
#define recoup_object .type = JSON_OBJECT, .value.object = (recoup_variant_t[])
#define recoup_end .type = JSON_END

#define recoup_key_object_(key_, body_) (recoup_variant_t) { \
    .type = JSON_OBJECT, .key = key_, .value.object = (recoup_variant_t[]) { \
        body_\
      , { recoup_end } } \
}

int main()
{
    plan(5);

    char memory[1024 * 1024];
    recoup_heap_t heap;
    recoup_t root = {};
    recoup_t object = {};

    recoup_heap_init(&heap, memory, sizeof(memory));
    recoup_root(&heap, &root);
    ok(recoup_set_object(&root, "object", NULL) == 0, "set object");

    recoup_get_object(&root, &object, "object", NULL);

    ok(recoup_set_number(&object, "number", 1) == 0, "set number");

    // Assume it is easier to check before you proceed, so all the gets will
    // assert on type, no coercion, so you must check types before you
    // dereference when information is coming from the outside in.

    double number = recoup_get_number(&object, "number", NULL).number;

    ok(number == 1, "set and get number");

    ok(recoup_set_string(&object, "string", "hello, world") == 0, "set string");

    recoup_t string;
    recoup_get_string(&object, &string, "string", NULL);

    ok(strcmp(recoup_string(&string), "hello, world") == 0, "get and set string");

    const char* key = "key";
    const char* value = "value";

    recoup_set_object(&object, "variant", (recoup_variant_t[]) {
        { recoup_key_string("string", "hello, world") },
        { recoup_key_number("number", 2) },
        /* { recoup_key_object("object") { { recoup_end } } }, */
        { recoup_end }
    });

    return 0;
}
