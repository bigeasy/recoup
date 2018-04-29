#include "ok.h"
#include "json.h"

int main()
{
    json_heap_t json_heap;

    json_heap_initialize(&json_heap);

    json_t object = json_create_object(&json_heap);

    json_set_integer(object, "a", 1);

    ok(*json_get_integer(object, "a") == 1, "get object");

    return 0;
}