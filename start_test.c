#include "ok.h"
#include "start.h"

int main()
{
    plan(1);

    int i = get_int();

    ok(i == 1, "did it: %d", i);
}
