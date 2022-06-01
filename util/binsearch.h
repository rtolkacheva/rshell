#ifndef OS_LABS_RSHELL_UTIL_BINSEARCH_H_
#define OS_LABS_RSHELL_UTIL_BINSEARCH_H_

#include <stddef.h>

// target -- the target of search
// begin -- pointer to the first element
// end -- pointer to the past-the-end element
// size -- size of one element in bytes
// cmp() -- function that returns 
//      <0 if first element is less
//      0 if the elements are equal
//      >0 if the first element is greater
// Returns pointer to where the target is placed or pointer to where it must be
// placed.
// For example:
// arr=[-4,0,2,6,9,900]
// if target=7, binsearch() = &arr[4]
// if target=2, binsearch() = &arr[2]
// if target=1000, binsearch() = &arr[6] <- notice that it's not a valid address
void* binsearch(const void* target, void* begin, void* end, size_t size, 
                int (*cmp)(const void*, const void*));

#endif // OS_LABS_RSHELL_UTIL_BINSEARCH_H_
