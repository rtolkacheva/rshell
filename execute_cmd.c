#include "execute_cmd.h"

#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "command.h"
#include "jobs.h"
#include "redirection.h"
#include "sig.h"
#include "util/config.h"
#include "util/pperror.h"
#include "util/vec_string.h"

#define FAIL            -1
#define SUCCESS         0
#define NUMBASE         10
#define INFINITE_POLL   -1
#define INVALID_FD      -1

enum SKIP_STRATEGY {
    SKIP_NOSKIP,
    SKIP_ON_FAIL,
    SKIP_ON_SUCCESS,
};

// Pipe for input
static int pipe_in[2] = {INVALID_FD, INVALID_FD};
// Pipe for output
static int pipe_out[2] = {INVALID_FD, INVALID_FD};
// Flag that is needed for exit after warning if it isn't quite right to just quit.
// For example, if there are still stopped jobs.
static bool warning_given;
// Saved blocking masks that are used to block SIGCHLD in critical sections
static sigset_t nvar, ovar;
// Describes strategy to skip next
static int skip_stategy = SKIP_NOSKIP;
// Either FAIL or SUCCESS
static int last_result = SUCCESS;
// Marks that terminal should not be passed. Used in the fg function.
static bool do_not_pass_terminal;

// Bit mask of command modes
enum MODE {
    mode_pipe_in            = 01,
    mode_pipe_out           = 02,
    mode_bkgrnd             = 04,
// complex modes
    mode_simple             = 00,
    mode_pipe_in_out        = 03, 
    mode_pipe_in_bkgrnd     = 05,
    mode_pipe_out_bkgrnd    = 06, // alias to mode_pipe_out
    mode_pipe_in_out_bkgrnd = 07, // alias to mode_pipe_in_out
};

// Code of internal cmd
enum SHELL_CMD {
    SHELL_JOBS,
    SHELL_NOTCMD,
    SHELL_BG,
    SHELL_FG,
    SHELL_CD,
    SHELL_EXIT,
};

struct redirection_result {
    int result;
    bool stdin_redirected;
    bool stdout_redirected;
};

// Gets pointer to the current job from jobs. Returns NULL on error.
// May modify jobs.
static struct job* get_current_job();

// Redirects redirection. 
// Arg must be struct redirection_result*.
// fd is supposed to be equal to redirection->fd.
// This is special function for foreach(). 
static void redirect_fm_func(int fd, struct redirection* redirection, void* arg);

// Redirects all files specified in cmd.
// Redirects all pipes if needed. Closes pipes after redirection.
static int make_redirections(const struct command* cmd);

// Closes all fd except STDIN_FILENO, STDOUT_FILENO and STDERR_FILENO
static void close_redirection_fm_func(int fd, struct redirection*, void*);

// Close all redirections specified in cmd except STDIN_FILENO, STDOUT_FILENO 
// and STDERR_FILENO
static void close_redirections(const struct command* cmd);

// Internal execution function. 
// Forks, executes and sets current_job fields.
static int execute_cmd_internal(struct command* cmd, struct job* job);

// Moves cmd to job. Does modify jobs.
// Marking job as valid is not perfomed in the move_cmd_to_job() function so 
// SIGCHLD handler can find job with the returned pid, but all internal 
// functions will threat this job us invalid and won't try to pass it to 
// foreground or background.
static int move_cmd_to_job(struct command* cmd, struct job* job);

// Marks job as valid, if it becamed valid.
static void update_job_validity(struct job* job);

// Executes shell cmd.
// exit, bg, fg, jobs are supported
// May modify jobs.
static int execute_shell_cmd(int internal_command, const struct command* cmd, struct job* job);

// Prints current jobs
static void execute_shell_jobs();

// Runs stopped job(s) in the background
static void execute_shell_bg(const struct command* cmd);

// Moves job to the foreground
static void execute_shell_fg(const struct command* cmd, struct job* fg_job);

// Changes current working directory
static void execute_shell_cd(const struct command* cmd);

// Tries to exit shell
static int execute_shell_exit();

// Returns true iff there are stopped jobs
static bool has_stopped_jobs();

// Kills all stopped jobs with SIGTERM
static void kill_stopped_jobs();

// Starts job in the background. Supportive function for the execute_shell_bg()
static void start_job_in_background(vec_size_t job, const char* jobnostr);

// Checks if the command is provided by shell.
static int is_shell_cmd(const char* cmd);

// Gives terminal to processes with group pgrp.
static int give_terminal_to(pid_t pgrp, const struct termios* nattr, struct termios* oattr);

// Takes terminal back to shell
static int get_terminal_back(struct termios* oattr);

// Passes foreground to the job and waits until it stops or ends.
static int pass_foreground(struct job* job);

// Prints job number and pid
static void print_background_info(const struct job* job);

// Waits untill job is done
static int wait_for_job(struct job* job);

// Marks cmd as continued
static void mark_command_continued_vec_func(struct command* cmd);

int execute_cmd(struct command* cmd)
{
    _shell_assert(cmd);
    BLOCK_CHILD(nvar, ovar);

    // Skips if the current job must be skipped.
    if ((skip_stategy == SKIP_ON_SUCCESS && last_result == SUCCESS)
            || (skip_stategy == SKIP_ON_FAIL && last_result == FAIL)) {
        // If it's not the last command of the job, does not update skip_strategy
        if (!cmd->flags.pipe_out) {
            skip_stategy = cmd->flags.skip_next_on_fail ? SKIP_ON_FAIL :    
                           cmd->flags.skip_next_on_success ? SKIP_ON_SUCCESS :
                           SKIP_NOSKIP;
        }

        goto ERROR_HANDLER;
    }

    int retval = SUCCESS;
    int mode = (cmd->flags.bkgrnd ? mode_bkgrnd : 0) 
               | (cmd->flags.pipe_out ? mode_pipe_out : 0)
               | (cmd->flags.pipe_in ? mode_pipe_in : 0);

    struct job* job = get_current_job();

    if (!job) {
        retval = FAIL;
        goto ERROR_HANDLER;
    }

    // Creates new pipe for output if needed
    if (cmd->flags.pipe_out) {
        if (pipe(pipe_out) == FAIL) {
            _shell_pperror("Failed to create pipe");
            retval = FAIL;
            goto ERROR_HANDLER;
        }
    }

    do_not_pass_terminal = false;
    if (execute_cmd_internal(cmd, job) == FAIL) {
        retval = FAIL;
        goto ERROR_HANDLER;
    }

    if (internal_executing)
        goto ERROR_HANDLER;
    
    switch (mode) {
    case mode_pipe_in_bkgrnd:
    case mode_bkgrnd:
        // Prints pid
        print_background_info(job);
        break;
    case mode_pipe_in_out:
    case mode_pipe_out:
        // Starts in the background, foreground was not given
        break;
    case mode_pipe_in:
    case mode_simple:
        if (!do_not_pass_terminal && pass_foreground(job) == FAIL)
            retval = FAIL;
        break;
    default:
        break;
    }
    
ERROR_HANDLER:
    UNBLOCK_CHILD(ovar);

    // Closes input pipe
    if (pipe_in[0] != INVALID_FD) {
        close(pipe_in[0]);
        close(pipe_in[1]);
    }
    // Moves output pipe to input pipe
    pipe_in[0] = pipe_out[0];
    pipe_in[1] = pipe_out[1];
    // Resets output pipe
    pipe_out[0] = INVALID_FD;
    pipe_out[1] = INVALID_FD;

    return retval;
}

int end_execution(bool print_msg)
{
    sigset_t nset, oset;

    if (print_msg) {
        fprintf(shell_outstream, "exit\n");
    }

    BLOCK_CHILD(nset, oset);
    bool give_warning = !warning_given && has_stopped_jobs();
    UNBLOCK_CHILD(oset);

    if (give_warning) {
        if (print_msg) {
            fprintf(shell_outstream, "There are stopped jobs\n");
        }
        warning_given = true;
        return FAIL;
    }

    if (pipe_in[0] != INVALID_FD) {
        close(pipe_in[0]);
        close(pipe_in[1]);
        // Null them so it would be OK to call end_execution() again
        pipe_in[0] = INVALID_FD;
        pipe_in[1] = INVALID_FD;
    }
    if (pipe_out[0] != INVALID_FD) {
        close(pipe_out[0]);
        close(pipe_out[1]);
        // Null them so it would be OK to call end_execution() again
        pipe_out[0] = INVALID_FD;
        pipe_out[1] = INVALID_FD;
    }

    // Doesn't kill children if it's not the parent process
    BLOCK_CHILD(nset, oset);
    if (!internal_executing) {
        kill_stopped_jobs();
    }
    UNBLOCK_CHILD(oset);

    return SUCCESS;
}

static struct job* get_current_job()
{
    // Checks jobs
    if (!jobs && !(jobs = vec_job_new()))
        return NULL;
    // Checks if last job is not yet finished or even set
    if (vec_size(jobs) && vec_at_ptr(jobs, vec_size(jobs) - 1)->state != JOB_VALID) {
        return vec_at_ptr(jobs, vec_size(jobs) - 1);
    }
    // Resizes jobs
    if (vec_job_resize(jobs, vec_size(jobs) + 1) == FAIL) 
        return NULL;
    struct job* job = vec_end(jobs) - 1;
    clear_job(job);
    return job;
}

static int execute_cmd_internal(struct command* cmd, struct job* job) 
{
    _shell_assert(cmd);
    
    int shell_cmd = is_shell_cmd(vec_front(cmd->args));

    // If it is not "exit" command, the flag for warning will be unset
    if (shell_cmd != SHELL_EXIT) {
        warning_given = false;
    }
    
    cmd->pid = fork();

    if (cmd->pid == FAIL) {
        _shell_pperror("fork");
        return FAIL;
    }

    if (setpgid(cmd->pid, job->pgid) == FAIL)
        _shell_pperrorf("setpgid(%d, %d) from %d", cmd->pid, job->pgid, getpid());

    // Child
    if (cmd->pid == 0) {
        UNBLOCK_CHILD(ovar);
        internal_executing = true;
        set_child_signals();
        if (make_redirections(cmd) == FAIL) {
            return FAIL;
        }
        // Execute internal shell cmd
        if (shell_cmd != SHELL_NOTCMD) {
            return execute_shell_cmd(shell_cmd, cmd, job);
        }
        // Execute something else as program
        if (execvp(vec_front(cmd->args), vec_data(cmd->args)) == FAIL) {
            _shell_flush_fprintf("Command '%s' not found\n", vec_at(cmd->args, 0));
            close_redirections(cmd);
            return FAIL;
        }
        _shell_unreachable();
    }
    
    // Parent
    int retval = SUCCESS;
    if ((retval = move_cmd_to_job(cmd, job)) == FAIL) 
        goto RELEASE_RESOURCES;

    cmd = vec_end(job->pipeline) - 1;
    
    if (shell_cmd != SHELL_NOTCMD && execute_shell_cmd(shell_cmd, cmd, job) == FAIL) {
        retval = FAIL;
    }

    update_job_validity(job);

RELEASE_RESOURCES:
    if (pipe_in[0] != INVALID_FD) {
        close(pipe_in[0]);
        close(pipe_in[1]);
        pipe_in[0] = INVALID_FD;
        pipe_in[1] = INVALID_FD;
    }

    return retval;
}

static int move_cmd_to_job(struct command* cmd, struct job* job)
{
    _shell_assert(cmd);
    _shell_assert(job);

    // First cmd in pipeline
    if (!cmd->flags.pipe_in) {
        job->pgid = cmd->pid;
        if (!(job->pipeline = vec_command_new()))
            return FAIL;
        job->line = sp_string_add_link(parsing_line);
    }
    // Updates skip strategy
    if (!cmd->flags.pipe_out) {
        skip_stategy = cmd->flags.skip_next_on_fail ? SKIP_ON_FAIL :    
                       cmd->flags.skip_next_on_success ? SKIP_ON_SUCCESS :
                       SKIP_NOSKIP;
    }
    job->pid = cmd->pid;
    job->state = JOB_CONSTRUCTING;

    cmd->status = CLD_CONTINUED;

    if (vec_command_push_back_by_ptr(job->pipeline, cmd) == FAIL)
        return FAIL;

    // Cleares cmd's values so the resources pushed to job would not released
    cmd->args = NULL;
    cmd->redirections = NULL;

    return SUCCESS;
}

static void update_job_validity(struct job* job)
{
    _shell_assert(job);

    struct command* cmd = vec_end(job->pipeline) - 1;

    if (!cmd->flags.pipe_out) {
        job->state = JOB_VALID;
    }
}

static void redirect_fm_func(int fd, struct redirection* redirection, void* arg)
{
    _shell_assert(arg);

    if (!redirection) {
        ((struct redirection_result*)arg)->result = FAIL;
        return;
    }

    if (redirect(redirection) == FAIL) {
        ((struct redirection_result*)arg)->result = FAIL;
    }
    if (redirection->fd == STDIN_FILENO)
        ((struct redirection_result*)arg)->stdin_redirected = true;
    if (redirection->fd == STDOUT_FILENO)
        ((struct redirection_result*)arg)->stdout_redirected = true;
}

static int make_redirections(const struct command* cmd)
{
    _shell_assert(cmd);
    close(shell_tty);
    close(waiting_pipe[0]);
    close(waiting_pipe[1]);

    struct redirection_result result = {.result = SUCCESS, 
                                        .stdin_redirected = false, 
                                        .stdout_redirected = false};
    fm_redirection_foreach(cmd->redirections, redirect_fm_func, &result);

    if (result.result == FAIL)
        return FAIL;

    if (cmd->flags.pipe_in) {
        if (!result.stdin_redirected) {
            struct redirection redirection = {.type = REDIRECTION_FD,
                                              .fd = STDIN_FILENO,  
                                              .file_fd = pipe_in[0]};
            if (redirect(&redirection) == FAIL) {
                return FAIL;
            }
        }
        close(pipe_in[0]);
        close(pipe_in[1]);
    }
    if (cmd->flags.pipe_out) {
        if (!result.stdout_redirected) {
            struct redirection redirection = {.type = REDIRECTION_FD,
                                              .fd = STDOUT_FILENO,  
                                              .file_fd = pipe_out[1]};
            if (redirect(&redirection) == FAIL) {
                return FAIL;
            }
        }
        close(pipe_out[0]);
        close(pipe_out[1]);
    }

    return SUCCESS;
}

static void close_redirection_fm_func(int fd, struct redirection* r, void* a)
{
    (void) r;
    (void) a;
    if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO)
        close(fd);
}

static void close_redirections(const struct command* cmd)
{
    _shell_assert(cmd);

    fm_redirection_foreach(cmd->redirections, close_redirection_fm_func, NULL);
}

static int get_terminal_back(struct termios* oattr)
{
    sigset_t set, oset;
    int oerrno = 0;
    int retval = SUCCESS;
    sigemptyset(&set);
    sigaddset(&set, SIGTTOU);
    sigaddset(&set, SIGTTIN);
    sigaddset(&set, SIGTSTP);
    sigaddset(&set, SIGCHLD);
    sigemptyset(&oset);
    sigprocmask(SIG_BLOCK, &set, &oset);

    if (tcsetpgrp(shell_tty, shell_pgrp) == FAIL) {
        oerrno = errno;
        retval = FAIL;
        goto EXIT;
    }

    if (oattr && tcgetattr(shell_tty, oattr) == FAIL) {
        oerrno = errno;
        retval = FAIL;
        goto EXIT;
    }

    if (tcsetattr(shell_tty, TCSADRAIN, &shell_attr) == FAIL) {
        oerrno = errno;
        retval = FAIL;
        goto EXIT;
    }
    tcflush(shell_tty, TCIFLUSH);

EXIT:
    sigprocmask(SIG_SETMASK, &oset, NULL);

    errno = oerrno;
    return retval;
}

static int give_terminal_to(pid_t pgrp, const struct termios* nattr, struct termios* oattr)
{
    sigset_t set, oset;
    int oerrno = 0;
    int retval = SUCCESS;
    sigemptyset(&set);
    sigaddset(&set, SIGTTOU);
    sigaddset(&set, SIGTTIN);
    sigaddset(&set, SIGTSTP);
    sigaddset(&set, SIGCHLD);
    sigemptyset(&oset);
    sigprocmask(SIG_BLOCK, &set, &oset);

    if (oattr && tcgetattr(shell_tty, oattr) == FAIL) {
        oerrno = errno;
        retval = FAIL;
        goto EXIT;
    }

    if (nattr && tcsetattr(shell_tty, TCSADRAIN, nattr) == FAIL) {
        oerrno = errno;
        retval = FAIL;
        goto EXIT;
    }

    if (tcsetpgrp(shell_tty, pgrp) == FAIL) {
        oerrno = errno;
        retval = FAIL;
        goto EXIT;
    }

EXIT:
    sigprocmask(SIG_SETMASK, &oset, NULL);

    errno = oerrno;
    return retval;
}

static int execute_shell_cmd(int internal_command, const struct command* cmd, struct job* job)
{
    if (!cmd)
        return FAIL;

    switch (internal_command) {
    case SHELL_FG:
        execute_shell_fg(cmd, job);
        break;
    case SHELL_EXIT:
        return execute_shell_exit();
    case SHELL_BG:
        execute_shell_bg(cmd);
        break;
    case SHELL_JOBS:
        execute_shell_jobs();
        break;
    case SHELL_CD:
        execute_shell_cd(cmd);
        break;
    default:
        _shell_flush_fprintf("\"%s\" not implemented.\n", vec_at(cmd->args, 0));
        return FAIL;
        break;
    };

    return SUCCESS;
}

static int is_shell_cmd(const char* cmd)
{
    if (strcmp("fg", cmd) == 0)
        return SHELL_FG;
    if (strcmp("bg", cmd) == 0)
        return SHELL_BG;
    if (strcmp("jobs", cmd) == 0)
        return SHELL_JOBS;
    if (strcmp("cd", cmd) == 0)
        return SHELL_CD;
    if (strcmp("exit", cmd) == 0)
        return SHELL_EXIT;
    
    return SHELL_NOTCMD;
}

static void execute_shell_jobs()
{
    // Parent
    if (!internal_executing) {
        for (vec_size_t i = 0; i < vec_size(jobs); ++i) {
            struct job* job = vec_at_ptr(jobs, i);
            if (job->state != JOB_VALID)
                continue;
            // Sets in the shell that all statuses didn't change since 
            // last print
            job->notify_status = false;
        }
        return;
    }
    
    // Child
    {
        for (vec_size_t i = 0; i < vec_size(jobs); ++i) {
            struct job* job = vec_at_ptr(jobs, i);
            if (job->state != JOB_VALID)
                continue;
            // Prints every finished and not yet cleared job
            fprintf(shell_outstream, "[%zu] \t", i + 1);
            print_job_with_status(job);
            fprintf(shell_outstream, "\n");
            job->notify_status = false;
        }
        fflush(shell_outstream);
    }
}

static void start_job_in_background(vec_size_t jobno, const char* jobnostr)
{
    _shell_assert(jobnostr);
    _shell_assert(jobno <= vec_size(jobs));

    struct job* job = jobno ? vec_at_ptr(jobs, jobno - 1) : NULL;
    // Check that job is valid
    if (!job || job->state != JOB_VALID) {
        if (internal_executing)
            _shell_flush_fprintf("bg: %s: no such job\n", jobnostr);
    }
    // Do nothing if job is already running
    else if (get_job_status(job) == JOB_RUNNING) {
        if (internal_executing)
            _shell_flush_fprintf("bg: job %zu already in background\n", jobno);
    }
    else if (get_job_status(job) == JOB_TERMINATED) {
        if (internal_executing)
            _shell_flush_fputs("bg: job has terminated\n");
    }
    // Finally, invoke all alive processes of the job
    else {
        job->forced_running = true;

        if (internal_executing) {
            // Child kills because only child is able to write error message
            if (kill(-job->pgid, SIGCONT) == FAIL) {
                    _shell_pperror("bg: kill");
                return;
            }
            fprintf(shell_outstream, "[%zu] \t", jobno);
            print_job(job);
            fprintf(shell_outstream, "\n");
        }        
    }
}

static void execute_shell_bg(const struct command* cmd)
{
    _shell_assert(cmd);

    // BG must get terminal
    if (cmd->flags.bkgrnd || cmd->flags.pipe_out || cmd->flags.pipe_in) {
        if (internal_executing)
            _shell_flush_fputs("bg: no job control\n");
        return;
    }

    // Current job, actually the job with the biggest jobno
    if (vec_size(cmd->args) == 2) {
        vec_size_t last_finished_job = 0;
        for (vec_size_t i = 0; i < vec_size(jobs); ++i) {
            if (vec_at_ptr(jobs, i)->state == JOB_VALID)
                last_finished_job = i + 1;
        }
        start_job_in_background(last_finished_job, "current");
        return;
    }

    // list of job numbers
    for (vec_size_t i = 1; i < vec_size(cmd->args) - 1; ++i) {
        char* jobnostr = vec_at(cmd->args, i);
        char* endptr;
        unsigned long long jobno = strtoull(jobnostr, &endptr, NUMBASE);
        if (endptr != jobnostr + strlen(jobnostr) || jobno > vec_size(jobs)) {
            jobno = 0;
        }
        start_job_in_background(jobno, jobnostr);
    }

    if (internal_executing)
        fflush(shell_outstream);
}

static void execute_shell_fg(const struct command* cmd, struct job* fg_job)
{
    _shell_assert(cmd);

    // FG must get terminal
    if (cmd->flags.bkgrnd || cmd->flags.pipe_out || cmd->flags.pipe_in) {
        if (internal_executing)
            _shell_flush_fputs("fg: no job control\n");
        return;
    }

    char* jobnostr = "current";
    vec_size_t jobno = 0;

    // current job, actually the job with the biggest jobno
    if (vec_size(cmd->args) == 2) {
        for (vec_size_t i = 0; i < vec_size(jobs); ++i) {
            struct job* job = vec_at_ptr(jobs, i);
            if (job->state == JOB_VALID)
                jobno = i + 1;
        }
    }
    else {
        // Moves to foreground the argument with jobno specified int the 1st argument 
        jobnostr = vec_at(cmd->args, 1);
        char* endptr;
        jobno = strtoull(jobnostr, &endptr, NUMBASE);
        if (endptr != jobnostr + strlen(jobnostr)) {
            jobno = 0;
        }
    }

    if (jobno <= 0 || jobno > vec_size(jobs)) {
        jobno = 0;
    }
    struct job* job = jobno ? vec_at_ptr(jobs, jobno - 1) : NULL;

    // Child -- prints error on error
    if (internal_executing) {
        if (!job || job->state != JOB_VALID)
            _shell_flush_fprintf("fg: %s: no such job\n", jobnostr);
        else if (get_job_status(job) == JOB_TERMINATED)
            _shell_flush_fputs("fg: job has terminated\n");
        return;
    }

    // Parent -- passes foreground if any appropriate job found
    if (job && job->state == JOB_VALID && get_job_status(job) != JOB_TERMINATED) {
        // Waits for the child here because during pass_foreground() to another
        // job this little process may die and release its resources, so
        // it would be unappropriate to either wait for it or pass foreground.
        wait_for_job(fg_job);
        do_not_pass_terminal = true;
        if (get_job_status(job) == JOB_TERMINATED)
            return;
        // Prints pipeline of commands to help user understand what was just run
        print_job(job);
        fprintf(shell_outstream, "\n");
        pass_foreground(job);
    }
}

static void execute_shell_cd(const struct command* cmd)
{
    _shell_assert(cmd);

    char* dir = vec_at(cmd->args, 1);
    if (!dir)
        dir = getenv("HOME");
    if (!dir)
        return;

    // Child changes directory too to see the same permission errors as the main 
    // process.
    if (internal_executing) {
        if (chdir(dir) == FAIL)
            _shell_pperrorf("cd: %s", dir);
        return;
    }

    (void) chdir(dir);
}

static int execute_shell_exit()
{
    // If exiting was successful, shell must exit too so it returns FAIL
    return end_execution(internal_executing) == SUCCESS ? FAIL : SUCCESS;
}

static int pass_foreground(struct job* job)
{
    _shell_assert(job);

    if (give_terminal_to(job->pgid, &job->tcattr, &shell_attr) == FAIL) {
        _shell_pperror("Passing terminal to child");
        return FAIL;
    }

    if (kill(-job->pgid, SIGCONT) == FAIL) {
        // _shell_pperror("fg: kill");
        goto GET_TERMINAL_BACK;
    }

    vec_command_foreach(job->pipeline, mark_command_continued_vec_func);

    wait_for_job(job);

GET_TERMINAL_BACK:

    if (get_terminal_back(&job->tcattr) == FAIL)
        return FAIL;

    if (get_job_status(job) == JOB_TERMINATED)
        release_job(job);
    // Just makes format more pretty
    else if (get_job_status(job) == JOB_STOPPED)
        fprintf(shell_outstream, "\n");

    return SUCCESS;
}

static void print_background_info(const struct job* job)
{
    _shell_assert(job);
    fprintf(shell_outstream, "[%zu] \t%jd\n", vec_size(jobs), (intmax_t)job->pid);
}

static bool has_stopped_jobs()
{
    _shell_assert(jobs);

    for (vec_size_t i = 0; i < vec_size(jobs); ++i) {
        if (get_job_status(vec_at_ptr(jobs, i)) == JOB_STOPPED)
            return true;
    }
    return false;
}

static void kill_stopped_jobs()
{
    _shell_assert(jobs);

    for (vec_size_t i = 0; i < vec_size(jobs); ++i) {
        if (get_job_status(vec_at_ptr(jobs, i)) == JOB_STOPPED) {
            kill(-vec_at_ptr(jobs, i)->pgid, SIGTERM);
            kill(-vec_at_ptr(jobs, i)->pgid, SIGCONT);
        }
    }
}

static int wait_for_job(struct job* job)
{
    _shell_assert(job);

    int retval = SUCCESS;
    last_result = FAIL;
    
    // Waits for every process of the pipeline
    for (vec_size_t i = 0; i < vec_size(job->pipeline); ++i) {
        struct command* cmd = vec_at_ptr(job->pipeline, i);
        // If the process is not dead, waits for it
        if (cmd->status != CLD_EXITED && cmd->status != CLD_KILLED 
            && cmd->status != CLD_DUMPED && cmd->status != CLD_STOPPED) {
            // siginfo_t status;
            int status = 0;

            waited_pid = cmd->pid;
            // Unblocks SIGCHLD only after waited_pid set. It's done
            // so while long-running process executes, other processes may 
            // be freed.
            UNBLOCK_CHILD(ovar);
            struct pollfd pollfd = {.fd = waiting_pipe[0], .events = POLLIN};

            int pollretval = 0;

            // Waits SIGCHLD to pass status for the waited pid.
            while ((pollretval = poll(&pollfd, 1, INFINITE_POLL)) == FAIL && errno == EINTR) ;

            // Exactly one fd must chnage its state out of 1
            if (pollretval != 1) {
                _shell_pperror("poll wait for child");
                retval = FAIL;
                goto BLOCK;
            }
            // Some strange thing happened to the pipe
            if (!(pollfd.revents & POLLIN)) {
                _shell_pperror("pipe error");
                retval = FAIL;
                goto BLOCK;
            }
            // Checks for programmer errors. Exactly sizeof(int) shound be written
            // to and read from the pipe.
            if (read(waiting_pipe[0], &status, sizeof(status)) != sizeof(status)) {
                _shell_pperror("pipe transmission error");
                retval = FAIL;
                goto BLOCK;
            }

            BLOCK_CHILD(nvar, ovar);

            cmd->status = transform_status(status);
            if (cmd->pid == job->pid)
                job->status = status;
        }
    }

    // Job's status was monitored, so it's status is already known by the user

    switch (get_job_status(job)) {
    case JOB_STOPPED:
        job->notify_status = true;
        break;
    case JOB_TERMINATED:
        last_result = WEXITSTATUS(job->status) ? FAIL : SUCCESS;
        __attribute__((fallthrough));
    default:
        job->notify_status = false;
        break;
    }

    waited_pid = 0;

    return retval;

BLOCK:
    BLOCK_CHILD(nvar, ovar);
    waited_pid = 0;

    return retval;
}

static void mark_command_continued_vec_func(struct command* cmd)
{
    cmd->status = CLD_CONTINUED;
}
