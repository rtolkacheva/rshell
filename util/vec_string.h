#ifndef OS_LABS_RSHELL_UTIL_VEC_STRING_H_
#define OS_LABS_RSHELL_UTIL_VEC_STRING_H_

#define VEC_UNDEF

// String-like vector
#define vec_name    char
#define vec_elem_t  char
#include "vector.h"

// Vector of c-strings
#define vec_name    string
#define vec_elem_t  char*
#include "vector.h"

#undef VEC_UNDEF

// Shared ptr for the vec_string_t
#define sp_name     string
#define sp_elem_t   struct vec_char_t
#define sp_dstr     vec_char_delete
#include "shared_ptr.h"

#endif // OS_LABS_RSHELL_UTIL_VEC_STRING_H_
