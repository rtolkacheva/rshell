
/**
 * vec_name -- vector name for all functions and structures
 * vec_elem_t -- type of vector element
 */

#define vec_cat(a, b) vec##_##a##_##b
#define vec_ecat(a, b) vec_cat(a, b)
#define _vec(x) vec_ecat(vec_name, x)

#ifndef VEC_HEADER
#define VEC_HEADER

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifndef VEC_TYPEDEFS
#define VEC_TYPEDEFS

typedef size_t vec_size_t;

#endif // VEC_TYPEDEFS

#define VEC_FAIL            -1
#define VEC_SUCCESS         0

struct _vec(t) {
    vec_elem_t* data;
    vec_size_t size;
    vec_size_t capacity;
};

#define vec_size(vec)       ( (vec)->size       )
#define vec_empty(vec)      ( (vec)->size == 0  )
#define vec_capacity(vec)   ( (vec)->capacity   )
#define vec_begin(vec)      ( (vec)->data       )
#define vec_end(vec)        ( (vec)->data + (vec)->size     )
#define vec_at(vec, i)      ( (vec)->data[i]    )
#define vec_data(vec)       ( (vec)->data       )
#define vec_at_ptr(vec, i)  ( (vec)->data + i   )
#define vec_front(vec)      ( (vec)->data[0]    )
#define vec_back(vec)       ( (vec)->data[(vec)->size - 1]  )

// Allocates new vector that must be freed with _vec(delete)()
struct _vec(t)* _vec(new)();
// Frees memory that was allocated by vector.
// Does not free any elements inside. To free them use _vec(foreach)()
// It does not free element to make the vector faster.
void _vec(delete)(struct _vec(t)* vec);

// Pushes elem to the end. Increases size, may be resized after that.
int _vec(push_back)(struct _vec(t)* vec, vec_elem_t elem);
int _vec(push_back_by_ptr)(struct _vec(t)* vec, vec_elem_t* elem);
// Removes last element if there is such.
void _vec(pop_back)(struct _vec(t)* vec);
// Shanges size. If the new size is greater than previous, it may contain garbage.
int _vec(resize)(struct _vec(t)* vec, vec_size_t size);
// Cleares the vector. Does not free any memory. Be advised to use _vec(foreach)()
void _vec(clear)(struct _vec(t)* vec);
// Calls func() to every element in the vector. It's safe to change them
void _vec(foreach)(struct _vec(t)* vec, void (*func)(vec_elem_t*));
// Puts elem at pos in the vector.
void _vec(put)(struct _vec(t)* vec, vec_size_t pos, vec_elem_t elem);

#endif // VEC_HEADER

#ifdef VEC_SOURCE

// Creates new vector
struct _vec(t)* _vec(new)()
{
    return (struct _vec(t)*)calloc(sizeof(struct _vec(t)), 1);
}

// Frees all memory of the vector
void _vec(delete)(struct _vec(t)* vec)
{
    if (vec != NULL)
        free(vec->data);
    free(vec);
}

// Adds elem to the end of the vector, resizes if needed.
// Returns VEC_SUCCESS on success, otherwise returns VEC_FAIL.
int _vec(push_back)(struct _vec(t)* vec, vec_elem_t elem)
{
    if (!vec)
        return VEC_FAIL;

    // If there is enough of space, adds the elem to the end
    if (vec->size < vec->capacity) {
        vec->data[vec->size++] = elem;
        return VEC_SUCCESS;
    }
    
    // If there is not enough of space, tries to resize
    if (_vec(resize)(vec, vec->size + 1) == VEC_FAIL)
        return VEC_FAIL;

    // Size was changed in the resize() function
    vec->data[vec->size - 1] = elem;
    return VEC_SUCCESS;
}

int _vec(push_back_by_ptr)(struct _vec(t)* vec, vec_elem_t* elem)
{
    if (!vec || !elem)
        return VEC_FAIL;

    // If there is enough of space, adds the elem to the end
    if (vec->size < vec->capacity) {
        vec->data[vec->size++] = *elem;
        return VEC_SUCCESS;
    }
    
    // If there is not enough of space, tries to resize
    if (_vec(resize)(vec, vec->size + 1) == VEC_FAIL)
        return VEC_FAIL;

    // Size was changed in the resize() function
    vec->data[vec->size - 1] = *elem;
    return VEC_SUCCESS;
}

// Tries to resize vector. Does nothing if the current capacity is bigger.
// Size will be changed to capacity on success.
// Returns VEC_SUCCESS on success, otherwise returns VEC_FAIL.
int _vec(resize)(struct _vec(t)* vec, vec_size_t size)
{
    if (!vec)
        return VEC_FAIL;

    // Does nothing if 
    if (vec->capacity >= size) {
        vec->size = size;
        return VEC_SUCCESS;
    }

    vec_size_t smart_capacity = size << 1;
    while (smart_capacity & (smart_capacity - 1))
        smart_capacity = smart_capacity & (smart_capacity - 1);

    assert(smart_capacity >= size); // check overflow

    vec_elem_t* new_addr = (vec_elem_t*)realloc(vec->data, smart_capacity * sizeof(vec_elem_t));
    if (new_addr == NULL) {
        // Last try
        new_addr = (vec_elem_t*)realloc(vec->data, size * sizeof(vec_elem_t));
        if (new_addr == NULL)
            return VEC_FAIL;

        smart_capacity = size;
    }

    // Finally, if everything is OK, updates values in the structure
    vec->data = new_addr;
    vec->size = size;
    vec->capacity = smart_capacity;

    return VEC_SUCCESS;
}

// Removes last element or does nothing, if there is no such element.
void _vec(pop_back)(struct _vec(t)* vec)
{
    if (!vec)
        return;
    
    if (vec->size) {
        vec->size--;
    }
}

// Sets vector so it would have 0 elements.
// Because C does not have destructors, use _vec(foreach) to free acquired
// resources.
void _vec(clear)(struct _vec(t)* vec)
{
    if (!vec)
        return;

    vec->size = 0;
}

// Calls func() for every element in the vector.
void _vec(foreach)(struct _vec(t)* vec, void (*func)(vec_elem_t*))
{
    if (!vec || !func)
        return;

    for (vec_size_t i = 0; i < vec->size; ++i) {
        func(&vec->data[i]);
    }
}

void _vec(put)(struct _vec(t)* vec, vec_size_t pos, vec_elem_t elem)
{
    if (!vec || pos >= vec->size)
        return;

    vec->data[pos] = elem;    
}

#endif // VEC_SOURCE

#if defined(VEC_UNDEF) || defined(VEC_SOURCE)
#   undef VEC_HEADER
#   undef vec_name
#   undef vec_elem_t
#endif // VEC_UNDEF

#undef vec_cat
#undef vec_ecat
#undef _vec
#undef VEC_FAIL
#undef VEC_SUCCESS
