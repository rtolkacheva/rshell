#include "binsearch.h"

#include <assert.h>
#include <stdio.h>

void* binsearch(const void* target, void* begin, void* end, size_t size, 
                int (*cmp)(const void*, const void*))
{
    assert(target);
    assert(begin);
    assert(end);
    assert(begin <= end);
    assert(size);
    assert(((char*)end - (char*)begin) % size == 0);
    assert(cmp);

    if (begin == end) 
        return end;

    size_t left = 0;
    size_t right = ((char*)end - (char*)begin) / size;

    // target is always in [left, right) if there is target
    while (left + 1 < right) {
        size_t middle = (right + left) / 2;
        if (cmp(target, (char*)begin + middle * size) >= 0) {
            left = middle;
        }
        else {
            right = middle;
        }
    }
    if (cmp(target, (char*)begin + left * size) <= 0) {
        return (char*)begin + left * size;
    }
    return (char*)begin + right * size;
}