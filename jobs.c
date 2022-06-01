#define VEC_SOURCE
#include "jobs.h"
#undef VEC_SOURCE

#include <fcntl.h>
#include <string.h>

#include "command.h"
#include "redirection.h"
#include "util/config.h"
#include "util/vec_string.h"

#define STATUS_INDENT   15
#define BUFLEN          128

static const char* job_status_msg[JOB_STATUS_COUNT] = {
    [JOB_TERMINATED]    = "Terminated",
    [JOB_RUNNING]       = "Running",
    [JOB_STOPPED]       = "Stopped",
    [JOB_EXITED]        = "Exit",
    [JOB_DONE]          = "Done",
    [JOB_KILLED]        = "Killed",
};

// Prints string if *str is not null
static void print_str(char** str);

// Prints redirection if format "fd> file_name"
static void print_redirection(int fd, struct redirection* redirection, void* arg);

// Returns wide range of possible statuses for function that prints status
static int get_job_status_internal(const struct job* job);

void release_job(struct job* job)
{
    if (!job)
        return;
    
    vec_command_foreach(job->pipeline, release_cmd);
    vec_command_delete(job->pipeline);
    if (job->line) {
        sp_string_release(job->line);
        if (sp_string_empty(job->line))
            sp_string_delete(job->line);
    }

    *job = (struct job){.pgid = 0, 
                        .pid = 0,
                        .pipeline = NULL, 
                        .line = NULL, 
                        .state = JOB_INVALID,
                        .tcattr = prev_attr,
                        .notify_status = false,
                        .forced_running = false};
}

void clear_job(struct job* job)
{
    if (!job)
        return;

    *job = (struct job){.pgid = 0,
                        .pid = 0, 
                        .pipeline = NULL, 
                        .line = NULL, 
                        .state = JOB_INVALID,
                        .tcattr = prev_attr,
                        .notify_status = false,
                        .forced_running = false};
}

struct job* find_job(pid_t pid, struct vec_job_t* jobs)
{
    if (!jobs)
        return NULL;
    for (vec_size_t i = 0; i < vec_size(jobs); ++i) {
        struct job* job = vec_at_ptr(jobs, i);
        if (find_cmd(pid, job))
            return job;
    }
    return NULL;
}

struct command* find_cmd(pid_t pid, struct job* job)
{
    if (!job || job->state == JOB_INVALID)
        return NULL;
    for (vec_size_t j = 0; j < vec_size(job->pipeline); ++j) {
        struct command* cmd = vec_at_ptr(job->pipeline, j);
        if (cmd->pid == pid)
            return cmd;
    }
    return NULL;
}

static int get_job_status_internal(const struct job* job)
{
    _shell_assert(job);

    if (job->state != JOB_VALID)
        return JOB_NOT_PRESENTED;

    if (job->forced_running) 
        return JOB_RUNNING;

    bool running = false;
    bool stopped = false;

    for (vec_size_t i = 0; i < vec_size(job->pipeline); ++i) {
        switch (vec_at_ptr(job->pipeline, i)->status) {
        case CLD_CONTINUED:
            running = true;
            break;
        case CLD_STOPPED:
            stopped = true;
            break;
        case CLD_EXITED:
        case CLD_DUMPED:
        case CLD_KILLED:
        case CLD_TRAPPED:
        default:
            break;
        }
    }
    if (running)
        return JOB_RUNNING;
    if (stopped)
        return JOB_STOPPED;

    if (WIFEXITED(job->status))
        return WEXITSTATUS(job->status) ? JOB_EXITED : JOB_DONE;
    else if (WIFSIGNALED(job->status))
        return JOB_KILLED;
    else 
        return JOB_TERMINATED;
}

int get_job_status(const struct job* job)
{
    _shell_assert(job);

    int status = get_job_status_internal(job);

    switch (status) {
    case JOB_STOPPED:
    case JOB_RUNNING:
        return status;
    case JOB_NOT_PRESENTED:
        return JOB_NOT_PRESENTED;
    default:
        return JOB_TERMINATED;
    }
}

static void print_str(char** str)
{
    if (*str) 
        fprintf(shell_outstream, "%s ", *str);
}

static void print_redirection(int fd, struct redirection* redirection, void* arg)
{
    (void) arg;
    int flags = redirection->flags;
    fprintf(shell_outstream, "%d%s %s ", 
            fd, // redirected fd
            (flags & O_APPEND) ? ">>" : (flags & O_WRONLY) ? ">" : "<", // mode
            redirection->file_name // file name
    );
}

void print_job(const struct job* job)
{
    if (!job || job->state != JOB_VALID)
        return;

    int job_state = get_job_status_internal(job);

    for (vec_size_t i = 0; i < vec_size(job->pipeline) - 1; ++i) {
        struct command* cmd = vec_at_ptr(job->pipeline, i);
        vec_string_foreach(cmd->args, print_str);
        fm_redirection_foreach(cmd->redirections, print_redirection, NULL);
        fprintf(shell_outstream, "| ");
    }
    struct command* cmd = vec_at_ptr(job->pipeline, vec_size(job->pipeline) - 1);
    vec_string_foreach(cmd->args, print_str);
    fm_redirection_foreach(cmd->redirections, print_redirection, NULL);

    if (job_state == JOB_RUNNING)
        fprintf(shell_outstream, "& ");
}

void print_job_with_status(const struct job* job)
{
    if (!job || job->state != JOB_VALID)
        return;

    int job_status = get_job_status_internal(job);
    char buff[BUFLEN];
    if (job_status == JOB_EXITED) {
        snprintf(buff, BUFLEN, "%s %d", job_status_msg[job_status], WEXITSTATUS(job->status));  
    }
    else {
        snprintf(buff, BUFLEN, "%s", job_status_msg[job_status]);  
    }
    fprintf(shell_outstream, "%-*s ", STATUS_INDENT, buff);
    print_job(job);
}
