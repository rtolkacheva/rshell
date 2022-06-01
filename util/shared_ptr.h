// sp_name -- name
// sp_elem_t -- type of pointer
// sp_dstr -- destructor

#define sp_cat(a, b) sp##_##a##_##b
#define sp_ecat(a, b) sp_cat(a, b)
#define _sp(x) sp_ecat(sp_name, x)

#ifndef SP_HEADER
#define SP_HEADER

#include <stdbool.h>
#include <stddef.h>

#ifndef SP_TYPEDEFS
#define SP_TYPEDEFS

typedef size_t sp_size_t;

#endif // SP_TYPEDEFS

struct _sp(t);

// Allocates shared ptr
struct _sp(t)* _sp(new)(sp_elem_t* ptr);
// Hard delete -- releases resources from self and underlying pointer
void _sp(delete)(struct _sp(t)* self);

// Returns pointer that self holds
sp_elem_t* _sp(get)(struct _sp(t)* self);
// Releases ptr from self
void _sp(release)(struct _sp(t)* self);
// Adds link and returns pointer to the structure
struct _sp(t)* _sp(add_link)(struct _sp(t)* self);
// Returns true iff the shared pointer is empty
bool _sp(empty)(struct _sp(t)* self);
// Returns number of links
sp_size_t _sp(count)(struct _sp(t)* self);

#endif // SP_HEADER

#ifdef SP_SOURCE

struct _sp(t) {
    sp_elem_t* ptr;
    sp_size_t link_count;
};

struct _sp(t)* _sp(new)(sp_elem_t* ptr)
{
    struct _sp(t)* self = (struct _sp(t)*)malloc(sizeof(struct _sp(t)));
    if (!self)
        return NULL;
    self->ptr = ptr;
    self->link_count = !!ptr;
    return self;
}

void _sp(delete)(struct _sp(t)* self)
{
    sp_dstr(self->ptr);
    free(self);
}

sp_elem_t* _sp(get)(struct _sp(t)* self)
{
    return self->ptr;
}

void _sp(release)(struct _sp(t)* self)
{
    if (self->link_count) {
        self->link_count--;
        if (!self->link_count) {
            sp_dstr(self->ptr);
            self->ptr = NULL;
        }
    }
}

struct _sp(t)* _sp(add_link)(struct _sp(t)* self)
{
    if (self && self->ptr) {
        self->link_count++;
        return self;
    }
    return NULL;
}

bool _sp(empty)(struct _sp(t)* self)
{
    return self->ptr == NULL;
}

sp_size_t _sp(count)(struct _sp(t)* self)
{
    return self->link_count;
}

#endif // SP_SOURCE

#if defined(SP_UNDEF) || defined(SP_SOURCE)
#   undef SP_HEADER
#   undef sp_name
#   undef sp_elem_t
#   undef sp_dstr
#endif // VEC_UNDEF

#undef sp_cat
#undef sp_ecat
#undef _sp

