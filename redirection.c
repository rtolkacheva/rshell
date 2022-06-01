#define FM_SOURCE
#include "redirection.h"
#undef FM_SOURCE

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "util/config.h"

#define FAIL    -1
#define SUCCESS 0

struct redirection* make_redirection(int fd, const char* file_name, int flags, mode_t mode)
{
    struct redirection* redirection = (struct redirection*)malloc(sizeof(struct redirection));
    if (!redirection)
        return NULL;
    *redirection = (struct redirection) {.type = REDIRECTION_FILE_NAME,
                                         .fd = fd,
                                         .file_name = file_name,
                                         .flags = flags,
                                         .mode = mode};

    return redirection;
}

void free_redirection(struct redirection* self)
{
    free(self);
}

int redirect(const struct redirection* redirection)
{
    _shell_assert(redirection);

    switch (redirection->type) {
    case REDIRECTION_FILE_NAME:;
        int oldfd = open(redirection->file_name, redirection->flags, redirection->mode);
        if (oldfd == FAIL || dup2(oldfd, redirection->fd) == FAIL) {
            return FAIL;
        }
        close(oldfd);
        break;
    case REDIRECTION_FD:
        if (dup2(redirection->file_fd, redirection->fd) == FAIL) {
            return FAIL;
        }
        break;
    default:
        return FAIL;
    }    

    return SUCCESS;
}
