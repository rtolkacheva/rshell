#define VEC_SOURCE
#include "command.h"
#undef VEC_SOURCE

#include <sys/wait.h>

#include "redirection.h"
#include "util/config.h"
#include "util/vec_string.h"

void reset_cmd(struct command* cmd)
{
    if (!cmd)
        return;

    cmd->pid = 0;
    cmd->status = CLD_CONTINUED;
    cmd->flags.bkgrnd = false;
    cmd->flags.pipe_out = false;
    cmd->flags.pipe_in = false;
    cmd->flags.skip_next_on_success = false;
    cmd->flags.skip_next_on_fail = false;

    if (cmd->args)
        vec_string_resize(cmd->args, 0);
    else
        cmd->args = vec_string_new();
    if (cmd->redirections)
        fm_redirection_clear(cmd->redirections);
    else
        cmd->redirections = fm_redirection_new();
}

void release_cmd(struct command* cmd)
{
    if (!cmd)
        return;
    vec_string_delete(cmd->args);
    fm_redirection_delete(cmd->redirections);
}

int push_cmd(struct command* cmd, struct vec_command_t* commands)
{
    _shell_assert(cmd);
    _shell_assert(commands);

    struct command cmd_copy = *cmd;
    cmd->args = NULL;
    cmd->redirections = NULL;
    vec_string_push_back(cmd_copy.args, NULL);

    return vec_command_push_back(commands, cmd_copy);
}
