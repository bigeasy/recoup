#include "ok.h"
#include "json.h"

#define json_template(_properties)          \
    (object_t[]) {{                         \
        .properties = (property_t* []) { \
            _properties \
            NULL \
        } \
    }}

#define json_object_2             \
    (object_t[]) {{                         \
        .properties = NULL \
    }}

#define json_string(_key, _value) \
    (property_t []) {{ .type = String, .key = _key, .value.string = _value }},

#define json_number(_key, _value) \
    (property_t []) {{ .type = Number, .key = _key, .value.number = _value }},

#define json_object(_key, _properties) \
    (property_t []) {{ \
        .type = Number, \
        .key = _key, \
        .value.object = (object_t[]) {{ \
            .properties = (property_t* []) { \
                _properties \
                NULL \
            } \
        } \
    }}

/*
static object_t* target = (object_t []) {{
    .properties = NULL
}};
*/

static object_t* macroed = json_template(
    json_string("key", "value")
    json_number("status", 0)
);

typedef struct object_alt_s object_x;

typedef struct json_variant_s json_variant_t;
struct json_variant_s {
    enum json_node_type type;
    const char* key;
    union {
        double number;
        const char* string;
        json_variant_t* object;
        json_variant_t* array;
    } value;
};

static object_t* template = (object_t []) {{
    .properties = (property_t*[]) {
        (property_t[]) {{ .type = JSON_STRING, .key = "string", .value.string = "string" }},
        (property_t[]) {{ .type = JSON_NUMBER, .key = "number", .value.number = 0 }},
        (property_t[]) {{
            .type = Object,
            .key = "object",
            .value.object = (object_t []) {}
        }},
        NULL
    }
}};

struct user_struct {
    const char* string;
    double number;
};

static json_variant_t* redux = (json_variant_t[]) {
    { .type = JSON_STRING, .key = "string", .value.string = "string" },
    { .type = JSON_NUMBER, .key = "number", .value.number = 0 },
    {
        .type = JSON_ARRAY,
        .key = "array",
        .value.array = (json_variant_t[]) {
            { .type = String, .value.string = "hello" },
            { .type = End }
        }
    },
    {
        .type = JSON_OBJECT,
        .key = "object",
        .value.object = (json_variant_t[]) {
            { .type = End }
        }
    },
    { .type = End }
};

int main()
{
    plan(2);

    char memory[1024 * 1024];
    json_heap_t heap;
    json_t root;
    json_t object;

    json_heap_init(&heap, memory, sizeof(memory));
    json_root(&heap, &object);
    json_create_object(&object, "object");

    json_get_object(&root, &object, "object", NULL);

    int error = json_set_number(&object, "number", 1);

    double number = json_get_number(&object, "number", NULL).number;

    return 0;
}
