#include <stdarg.h>
#include <stdio.h>

static int count;

void plan (int count)
{
    printf("1..%d\n", count);
}

void ok (int condition, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    printf("%sok %d ", condition ? "" : "not ", ++count);
    vprintf(format, args);
    printf("\n");
    va_end(args);
}
