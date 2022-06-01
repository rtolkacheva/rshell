#include "utils.h"

#include <stdlib.h>

void free_int(int p)
{
    (void)p;
}

int int_cmp(int a, int b)
{
    return (a == b) ? 0 : ((a < b) ? -1 : 1);
}
