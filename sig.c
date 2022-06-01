#define VEC_SOURCE
#include "sig.h"
#undef VEC_SOURCE

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "command.h"
#include "jobs.h"
#include "util/config.h"

#define FAIL    -1
#define SUCCESS 0

#ifndef WAIT_ANY
#   define WAIT_ANY -1
#endif

// SIGCHLD handler. Resets values in jobs to the given status.
static void sigchld_handler(int signum);

void set_shell_signal_handlers()
{
    struct sigaction nact = {.sa_handler = sigchld_handler, .sa_flags = SA_RESTART};
    sigemptyset(&nact.sa_mask);
    sigaction(SIGCHLD, &nact, NULL);

    nact.sa_handler = SIG_IGN;
    nact.sa_flags = 0;
    sigaction(SIGINT, &nact, NULL);
    sigaction(SIGQUIT, &nact, NULL);
    sigaction(SIGQUIT, &nact, NULL);
    sigaction(SIGTERM, &nact, NULL);
    sigaction(SIGTSTP, &nact, NULL);
    sigaction(SIGTTIN, &nact, NULL);
    sigaction(SIGTTOU, &nact, NULL);
}

void set_child_signals()
{
    struct sigaction nact = {.sa_handler = SIG_DFL};
    sigemptyset(&nact.sa_mask);

    sigaction(SIGCHLD, &nact, NULL);
    sigaction(SIGINT, &nact, NULL);
    sigaction(SIGQUIT, &nact, NULL);
    sigaction(SIGTERM, &nact, NULL);
    sigaction(SIGTSTP, &nact, NULL);
    sigaction(SIGTTIN, &nact, NULL);
    sigaction(SIGTTOU, &nact, NULL);
}

static void sigchld_handler(int signum)
{    
    // Protects from data races
    int oerrno = errno;

    int status = 0;
    int pid = 0;
    int cld_code = CLD_CONTINUED;
    // Protects from resending status if the process updates again, because the
    // shell expects exactly one status passed for each wait.
    bool waited_pid_met = false;

    while ((pid = waitpid(WAIT_ANY, &status, WNOHANG | WUNTRACED | WCONTINUED)) != FAIL && pid != 0) {
        // If it's process that is being waited for, passes status to the pipe and 
        // continues.
        if (!waited_pid_met && pid == waited_pid && !WIFCONTINUED(status)) {
            write(waiting_pipe[1], &status, sizeof(status));
            waited_pid_met = true;
            continue;
        }

        // Checks that status represents interesting state to us
        if ((cld_code = transform_status(status)) == FAIL)
            continue;

        struct job* job = find_job(pid, jobs);
        struct command* cmd = find_cmd(pid, job);

        if (!cmd)
            continue;

        int job_status = get_job_status(job);
        // It's important to update cmd's status after status of job was asked
        cmd->status = cld_code;

        switch (cld_code) {
        case CLD_CONTINUED:
            goto CONTINUE;
        case CLD_STOPPED:
            if (job_status != JOB_STOPPED)
                job->notify_status = true; 
            break;
        default: 
            if (job_status != JOB_TERMINATED)
                job->notify_status = true; 
            break;
        }
        job->forced_running = false;
        if (job->pid == pid)
            job->status = status;

        CONTINUE:;
    }

    errno = oerrno;
}

int transform_status(int status)
{
    if (WIFCONTINUED(status))
        return CLD_CONTINUED;
    if (WIFSTOPPED(status))
        return CLD_STOPPED;
    if (WIFEXITED(status))
        return CLD_EXITED;
    if (WCOREDUMP(status))
        return CLD_DUMPED;
    if (WIFSIGNALED(status))
        return CLD_KILLED;

    return FAIL;
}
