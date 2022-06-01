// fm_name -- flatmap name

// fm_key_t -- type of key
// fm_free_key -- frees key
// fm_key_cmp -- comporator for keys

// fm_data_t -- type of data
// fm_free_data -- frees data

#define fm_etcat(a, b, c)   a##_##b##_##c
#define fm_tcat(a, b, c)    fm_etcat(a, b, c)
#define fm_cat(a, b)        fm##_##a##_##b
#define fm_ecat(a, b)       fm_cat(a, b)
#define _fm(x)              fm_ecat(fm_name, x)
#define FAIL                -1
#define SUCCESS             0

#ifndef FM_HEADER
#define FM_HEADER

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "binsearch.h"

struct _fm(node_t);
struct _fm(t);

// Allocates new flatmap. It must be freed with _fm(delete)()
struct _fm(t)* _fm(new)();
// Frees all memory that is located in the flatmap. 
void _fm(delete)(struct _fm(t)* self);

// Returns true iff there is data for the passed key.
// If such element found and data is not NULL, it will be filled with the
// corresponding data.
bool _fm(find)(struct _fm(t)* self, const fm_key_t key, fm_data_t* data);
// Erases element assosiated with key. If there is no such, does nothing.
void _fm(erase)(struct _fm(t)* self, const fm_key_t key);
// Inserts element. Returns true iff the element was successfully inserted.
bool _fm(insert)(struct _fm(t)* self, fm_key_t key, fm_data_t data);
// Cleares all the elements with calling corresponding free functions for key 
// and data.
void _fm(clear)(struct _fm(t)* self);
// Calls func() from every element. 
// Trying to free memory of key or data will cause double free.
void _fm(foreach)(struct _fm(t)* self, void (*func)(const fm_key_t, fm_data_t, void*), 
                  void* arg);

#endif // FM_HEADER

#ifdef FM_SOURCE

struct _fm(node_t) {
    fm_key_t key;
    fm_data_t data;
};

#define VEC_SOURCE
#define vec_name _fm(vec)
#define vec_elem_t struct _fm(node_t)
#include "vector.h"
#undef VEC_SOURCE

#define vec_call(x) fm_tcat(vec, _fm(vec), x)

// Compares two nodes
static int _fm(node_cmp)(const void* lhs, const void* rhs)
{
    if (!lhs && !rhs)
        return 0;
    if (!lhs)
        return -1;
    if (!rhs)
        return 1;

    return fm_key_cmp(((const struct _fm(node_t)*)lhs)->key, 
                      ((const struct _fm(node_t)*)rhs)->key);
}

static void _fm(free_node)(struct _fm(node_t)* node)
{
    fm_free_key(node->key);
    fm_free_data(node->data);
}

struct _fm(t) {
    struct vec_call(t)* vec;
};

struct _fm(t)* _fm(new)()
{
    struct _fm(t)* self = (struct _fm(t)*)malloc(sizeof(struct _fm(t)));
    if (!self)
        return NULL;
    self->vec = vec_call(new)();
    if (!self->vec) {
        free(self);
        return NULL;
    }
    return self;
}

void _fm(delete)(struct _fm(t)* self)
{
    if (!self)
        return;

    vec_call(foreach)(self->vec, _fm(free_node));
    vec_call(delete)(self->vec);
    free(self);
}

bool _fm(find)(struct _fm(t)* self, const fm_key_t key, fm_data_t* data)
{
    assert(self);

    struct _fm(node_t) target = {.key = key};

    struct _fm(node_t)* found = vec_end(self->vec);
    
    if (vec_size(self->vec)) 
        found = (struct _fm(node_t)*)binsearch(&target,
            vec_begin(self->vec), vec_end(self->vec), sizeof(struct _fm(node_t)),
            _fm(node_cmp));
    
    // Not found
    if (found == vec_end(self->vec) || _fm(node_cmp)(&target, found) != 0) {
        return false;
    }

    if (data)
        *data = found->data;
    return true;
}

void _fm(erase)(struct _fm(t)* self, const fm_key_t key)
{
    assert(self);

    struct _fm(node_t) target = {.key = key};

    struct _fm(node_t)* found = vec_end(self->vec);
    
    if (vec_size(self->vec)) 
        found = (struct _fm(node_t)*)binsearch(&target,
            vec_begin(self->vec), vec_end(self->vec), sizeof(struct _fm(node_t)),
            _fm(node_cmp));
    
    // Not found
    if (found == vec_end(self->vec) || _fm(node_cmp)(&target, found) != 0) {
        return;
    }

    fm_free_key(found->key);
    fm_free_data(found->data);

    size_t pos = found - vec_begin(self->vec);
    // Copies from [pos + 1, size - 1] to [pos, size - 1]
    memmove(vec_at_ptr(self->vec, pos), 
            vec_at_ptr(self->vec, pos + 1),
            (vec_size(self->vec) - pos - 1) * sizeof(struct _fm(node_t)));
    vec_call(pop_back)(self->vec);
}

bool _fm(insert)(struct _fm(t)* self, fm_key_t key, fm_data_t data)
{
    assert(self);

    struct _fm(node_t) target = {.key = key, .data = data};

    struct _fm(node_t)* found = vec_end(self->vec);
    
    if (vec_size(self->vec)) 
        found = (struct _fm(node_t)*)binsearch(&target,
            vec_begin(self->vec), vec_end(self->vec), sizeof(struct _fm(node_t)),
            _fm(node_cmp));

    // Pushes node back if it must be places to the end
    if (found == vec_end(self->vec)) {
        if (vec_call(push_back)(self->vec, target) == FAIL) {
            return false;
        }
        return true;
    }
    
    // Inserts node to any place in the vector
    if (_fm(node_cmp)(&target, found) != 0) {
        // Remembers relative position because vector pointers may change during
        // resize.
        size_t pos = found - vec_begin(self->vec);
        if (vec_call(resize)(self->vec, vec_size(self->vec) + 1) == FAIL) {
            return false;
        }
        // Moves [pos, size - 2] to [pos + 1, size - 1].
        // size - 2 because it was resized and the last node in the vector holds
        // garbage.
        memmove(vec_at_ptr(self->vec, pos + 1), 
                vec_at_ptr(self->vec, pos),
                (vec_size(self->vec) - pos - 1) * sizeof(struct _fm(node_t)));
        *vec_at_ptr(self->vec, pos) = (struct _fm(node_t)){.key = key,
                                                          .data = data};
        return true;
    }

    // Replaces data in the found node
    fm_free_data(found->data);
    found->data = data;
    return true;
}

void _fm(clear)(struct _fm(t)* self)
{
    assert(self);

    // Frees memory in the vector
    vec_call(foreach)(self->vec, _fm(free_node));
    // Clears vector
    vec_call(clear)(self->vec);
}

void _fm(foreach)(struct _fm(t)* self, void (*func)(const fm_key_t, fm_data_t, void*), 
                  void* arg)
{
    if (!self || !func)
        return;
    for (size_t i = 0; i < vec_size(self->vec); ++i) {
        func(vec_at(self->vec, i).key, vec_at(self->vec, i).data, arg);
    }
}

#undef vec_call
#endif // FM_SOURCE

#if defined(FM_UNDEF) || defined(FM_SOURCE)
#   undef FM_HEADER
#   undef fm_name
#   undef fm_key_t
#   undef fm_free_key
#   undef fm_key_cmp
#   undef fm_data_t
#   undef fm_free_data
#endif 

#undef fm_tcat
#undef fm_cat
#undef fm_ecat
#undef _fm
#undef FAIL
#undef SUCCESS
