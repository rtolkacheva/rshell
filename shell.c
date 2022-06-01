#include "shell.h"

#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <termios.h>

#include "command.h"
#include "execute_cmd.h"
#include "jobs.h"
#include "parseline.h"
#include "promptline.h"
#include "redirection.h"
#include "sig.h"
#include "util/config.h"
#include "util/vec_string.h"

#define FAIL        -1
#define SUCCESS     0

// Prints all cmds to shell_outstream
__attribute__((__unused__))
static void print_cmds(struct vec_command_t* cmds);

// Print one redirection. This function will be passed to flatmap.
static void print_redirection(int fd, struct redirection* redirection, void* arg);

// Initialize shell's global variables
static int init_shell();

// Releases shell's resources
static void release_shell();

// Resets global parsing_line
static int reset_parsing_line();

// Prints changes in jobs and removes jobs from the end
static void process_jobs();

void start_shell(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    if (argc > 1) {
        _shell_flush_fputs("The program parameters are not supported.\n");
    }

    if (init_shell() == FAIL)
        return;

    struct vec_command_t* cmds = vec_command_new();

START:
    while (true) {
        if (reset_parsing_line() == FAIL) {
            goto RESOURCE_MANAGER;
        }
        int promptval = prompt_line(sp_string_get(parsing_line));
        if (promptval == FAIL) {
            goto PROCESS_JOBS;
        }
        // on EOF goes to exit
        else if (promptval == PROMPT_EOF) {
            fprintf(shell_outstream, "\n");
            goto PRETTY_EXIT;
        }

        if (parse_line(vec_data(sp_string_get(parsing_line)), cmds) == FAIL) {
            goto PROCESS_JOBS;
        }
        _shell_log_call(print_cmds(cmds));

        for (size_t i = 0; i < vec_size(cmds); ++i) {
            if (execute_cmd(vec_at_ptr(cmds, i)) == FAIL) {
                goto RESOURCE_MANAGER;
            }
            // Must finish correctly if it was the internal command that forked
            if (internal_executing)
                goto RESOURCE_MANAGER;
        }
        PROCESS_JOBS:;
        sigset_t nvar, ovar;
        BLOCK_CHILD(nvar, ovar);
        process_jobs();
        UNBLOCK_CHILD(ovar);
    }

PRETTY_EXIT:

    if (end_execution(true) == FAIL)
        goto START;

RESOURCE_MANAGER:;

    sigset_t nvar, ovar;
    BLOCK_CHILD(nvar, ovar);

    if (end_execution(false) == FAIL)
        end_execution(false);
    vec_command_foreach(cmds, release_cmd);
    vec_command_delete(cmds);
    release_shell();
}

static void print_cmds(struct vec_command_t* cmds)
{
    _shell_assert(cmds);

    for (size_t i = 0; i < vec_size(cmds); ++i) {
        for (size_t j = 0; j < vec_size(vec_at(cmds, i).args); ++j) {
            fprintf(shell_outstream, "cmd[%zu].args[%zu] = %s\n", i, j, 
                    vec_at(vec_at(cmds, i).args, j));
        }
        fprintf(shell_outstream, "cmd[%zu].flags = ", i);
        if (vec_at(cmds, i).flags.bkgrnd) 
            fprintf(shell_outstream, "bkgrnd ");
        if (vec_at(cmds, i).flags.pipe_in) 
            fprintf(shell_outstream, "pipe_in");
        if (vec_at(cmds, i).flags.pipe_out) 
            fprintf(shell_outstream, "pipe_out");
        if (vec_at(cmds, i).flags.skip_next_on_success) 
            fprintf(shell_outstream, "skip_next_on_success ");
        fprintf(shell_outstream, "\n");

        fm_redirection_foreach(vec_at(cmds, i).redirections, print_redirection, &i);
    }
}

static void print_redirection(int fd, struct redirection* redirection, void* arg)
{
    if (!redirection) {
        return;
    }
    fprintf(shell_outstream, "cmd[%d] redirects fd %d to \"%s\"\n", *(int*)arg, fd, redirection->file_name);
}

static int init_shell()
{
    if (!(jobs = vec_job_new())) {
        return FAIL;
    }
    shell_pgrp = getpgrp();
    shell_outfd = STDERR_FILENO;
    shell_outstream = stderr;

    // Shell must be interactive so it opens terminal anyway
    shell_tty = isatty(shell_outfd) == 1 ? dup(shell_outfd) : open("/dev/tty", O_RDWR|O_NONBLOCK);
    if (shell_tty == FAIL) {
        _shell_pperror("Failed to set tty");
        return FAIL;
    }
    if (tcgetattr(shell_tty, &prev_attr) == FAIL
        || tcgetattr(shell_tty, &shell_attr)) {
        _shell_pperror("tcgetattr: Failed to get terminal attributes");
        return FAIL;
    }
    if (pipe(waiting_pipe) == FAIL) {
        _shell_pperror("pipe2");
        return FAIL;
    }

    set_shell_signal_handlers();
    
#ifdef SHELL_VERSION
    fprintf(shell_outstream, "Rshell version " SHELL_VERSION "\n");
#endif
    return SUCCESS;
}

static void release_shell()
{
    vec_job_foreach(jobs, release_job);
    vec_job_delete(jobs);
    sp_string_delete(parsing_line);
    tcsetattr(shell_tty, TCSANOW, &prev_attr);
    close(waiting_pipe[0]);
    close(waiting_pipe[1]);
}

static int reset_parsing_line()
{
    // Releases global variable link
    if (parsing_line) {
        if (sp_string_count(parsing_line) == 1)
            return SUCCESS;
        sp_string_release(parsing_line);  
    }
    // Creates new link carefully. If fails to allocate structure, frees
    // vector.
    struct vec_char_t* vec = vec_char_new();
    if (!vec || !(parsing_line = sp_string_new(vec))) {
        vec_char_delete(vec);
        return FAIL;
    }
    return SUCCESS;
}

static void process_jobs()
{
    _shell_assert(jobs);

    vec_size_t new_size = 0;

    for (vec_size_t i = 0; i < vec_size(jobs); ++i) {
        struct job* job = vec_at_ptr(jobs, i);
        if (job->state == JOB_INVALID)
            continue;
        // Prints information only if the status of program has changed
        if (job->notify_status) {
            fprintf(shell_outstream, "[%zu] \t", i + 1);
            print_job_with_status(job);
            fprintf(shell_outstream, "\n");
            job->notify_status = false;
        }
        int job_status = get_job_status(job);
        _shell_assert(job_status != JOB_NOT_PRESENTED);

        // It either ended in foreground or its terminated status just printed,
        // so it's better to release resources. 
        if (job_status == JOB_TERMINATED) {
            release_job(job);
        }
        else {
            new_size = i + 1;
        }
    }
    fflush(shell_outstream);
    // Shrinks jobs to hold only jobs that are here.
    vec_job_resize(jobs, new_size);
}
