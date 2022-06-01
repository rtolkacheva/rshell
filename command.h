#ifndef OS_LABS_RSHELL_COMMAND_H_
#define OS_LABS_RSHELL_COMMAND_H_

#include <stdbool.h>
#include <sys/types.h>

struct vec_string_t;
struct fm_redirection_t;

struct command {
    // This vector does not hold any resources
    struct vec_string_t* args;
    struct fm_redirection_t* redirections;
    pid_t pid;
    int status;

    // Flags to customize the execution.
    struct
    {
        bool bkgrnd                 : 1;
        bool pipe_out               : 1;
        bool pipe_in                : 1;
        bool skip_next_on_success   : 1;
        bool skip_next_on_fail      : 1;
    } flags;
};

#define VEC_UNDEF

#define vec_name    command
#define vec_elem_t  struct command
#include "util/vector.h"

#undef VEC_UNDEF

// Resets cmd to default values.
void reset_cmd(struct command* cmd);

// Releases all resources cmd had
void release_cmd(struct command* cmd);

// Copies cmd and pushes it to commands.
// cmd will be changed and release_cmd() should be called befor further use.
int push_cmd(struct command* cmd, struct vec_command_t* commands);

#endif // OS_LABS_RSHELL_COMMAND_H_
