#ifndef OS_LABS_RSHELL_SIG_H_
#define OS_LABS_RSHELL_SIG_H_

#include <signal.h>

#define BLOCK_SIGNAL(sig, nvar, ovar) \
do { \
    sigemptyset(&nvar); \
    sigaddset(&nvar, sig); \
    sigemptyset(&ovar); \
    sigprocmask(SIG_BLOCK, &nvar, &ovar); \
} while (0)

#define UNBLOCK_SIGNAL(ovar) sigprocmask (SIG_SETMASK, &ovar, (sigset_t*) NULL)

#define BLOCK_CHILD(nvar, ovar) BLOCK_SIGNAL (SIGCHLD, nvar, ovar)
#define UNBLOCK_CHILD(ovar) UNBLOCK_SIGNAL(ovar)

// Sets shell-specific signal handlers.
void set_shell_signal_handlers();

// Restores shell signal handlers for a child.
void set_child_signals();

// Transforms status from waitpid(2) format to waitid(2) si_code format
int transform_status(int status);

#endif // OS_LABS_RSHELL_SIG_H_
