#ifndef OS_LABS_RSHELL_REDIRECT_H_
#define OS_LABS_RSHELL_REDIRECT_H_

#include <sys/types.h>

#include "util/utils.h"

enum REDIRECTION_TYPE {
    REDIRECTION_FILE_NAME,
    REDIRECTION_FD,
};

struct redirection {
    // Type of redirection
    int type;
    // File descriptor that will be set
    int fd;
    union {
        const char* file_name;
        int file_fd;
    };
    int flags; // flags for open
    mode_t mode; // mode for open
};

// Allocates memory for a redirection with passed parameters.
// The returned redirection must be freed with free_redirection().
struct redirection* make_redirection(int fd, const char* file_name, int flags, mode_t mode);

// Redirects file tpecified in the redirection structure.
int redirect(const struct redirection* redirection);

// Frees redirection.
// Works with NULL.
void free_redirection(struct redirection* self);

#define FM_UNDEF

#define fm_name         redirection
#define fm_key_t        int
#define fm_free_key     free_int
#define fm_key_cmp      int_cmp
#define fm_data_t       struct redirection*
#define fm_free_data    free_redirection
#include "util/flatmap.h"

#undef FM_UNDEF

#endif 
