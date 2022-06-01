#ifndef OS_LABS_RSHELL_JOBS_H_
#define OS_LABS_RSHELL_JOBS_H_

#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>

struct vec_command_t;

// Structure that desribes job in the shell.
struct job {
    // Process group id
    pid_t pgid;
    // Pid whose return value is interested. Always the last process' pid
    pid_t pid;
    // Status from signals
    int status;
    // Pipeline of one or more commands
    struct vec_command_t* pipeline;
    // Shared ptr for the line
    struct sp_string_t* line;
    // Job state
    int state;
    // Terminal attributes
    struct termios tcattr;

    struct {
        // True iff the status has changed since last check and must be printed
        bool notify_status : 1;
        bool forced_running : 1;
    };
};

// Internal state of the structure
enum JOB_STATE {
    JOB_INVALID,    // empty job
    JOB_CONSTRUCTING, // unfinished pipeline
    JOB_VALID,   // pipeline is finished
};

// Status of process group, returns from get_job_status()
enum JOB_STATUS {
    // These statuses may be returned by get_job_status():
    JOB_TERMINATED,
    JOB_RUNNING,
    JOB_STOPPED,
    // Job is not yet finished so status makes no sense. 
    JOB_NOT_PRESENTED,
    // These jobs may not be returnd by get_job_status():
    JOB_EXITED, 
    JOB_DONE,
    JOB_KILLED,
    // Count of the statuses
    JOB_STATUS_COUNT,
};

#define VEC_UNDEF
#define vec_name job
#define vec_elem_t struct job
#include "util/vector.h"
#undef VEC_UNDEF

// Releases job's resources
void release_job(struct job* job);

// Similar to release_job(), but assumes all resources as garbage
void clear_job(struct job* job);

// Searches for a job that have cmd with the passed pid and returns pointer to it. 
// If there is no such, returns NULL.
struct job* find_job(pid_t pid, struct vec_job_t* jobs);

// Searches for a command in any of the job from jobs. Returns pointer to is
// if there is one, otherwise returns NULL.
struct command* find_cmd(pid_t pid, struct job* job);

// Returns job status
int get_job_status(const struct job* job);

// Prints status, all arguments and all redirections
void print_job(const struct job* job);

// Same as print_job(), but before printing cmd prints its status
void print_job_with_status(const struct job* job);

#endif // OS_LABS_RSHELL_JOBS_H_
