#include <stdbool.h>
#include <stdio.h>

#include "binsearch.h"

int int_cmp(const void* lhs, const void* rhs)
{
    return *(int*)lhs - *(int*)rhs;
}

int main()
{
    int arr1[] = {-10, 1, 2, 4, 9, 10, 100500};
    int arr2[] = {0, 1, 4, 5, 6, 7, 9, 10};
    int targets[] = {-900, -10, 1, 2, 3, 9, 10, 100, 100501};
    size_t arr1_t[] = {0, 0, 1 ,2, 3, 4, 5, 6, 7};
    size_t arr2_t[] = {0, 0, 1, 2, 2, 6, 7, 8, 8};
    bool ok = true;
    for (int i = 0; i < sizeof(targets) / sizeof(int); ++i) {
        size_t res = (int*)binsearch(targets + i, arr1, arr1 + sizeof(arr1) / sizeof(int), sizeof(int),
            int_cmp) - arr1;
        if (res != arr1_t[i]) {
            ok = false;
            printf("binsearch fail : %d in arr1 returns %zu instead of %zu\n",
                targets[i], res, arr1_t[i]);
        }
    }
    for (int i = 0; i < sizeof(targets) / sizeof(int); ++i) {
        size_t res = (int*)binsearch(targets + i, arr2, arr2 + sizeof(arr2) / sizeof(int), sizeof(int),
            int_cmp) - arr2;
        if (res != arr2_t[i]) {
            ok = false;
            printf("binsearch fail : %d in arr1 returns %zu instead of %zu\n",
                targets[i], res, arr2_t[i]);
        }
    }
    if (ok)
        printf("Everything is ok!\n");
}
