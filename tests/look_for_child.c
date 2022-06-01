#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#define FAIL    -1
#define SUCCESS 0

#define tmsg "Terminated\n"
#define smsg "Stopped\n"
#define cmsg "Continued\n"
#define umsg "Unknown\n"

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

void sigchld_siginfo_handler(int signum, siginfo_t *info, void *ucontext)
{
    printf("code is %d, pid is %d\n", info->si_code, info->si_pid);

    switch (info->si_code) {
    case CLD_EXITED:
    case CLD_KILLED:
    case CLD_DUMPED:
        write(STDERR_FILENO, tmsg, sizeof(tmsg) - 1);
        break;
    case CLD_STOPPED:
        write(STDERR_FILENO, smsg, sizeof(smsg) - 1);
        break;
    case CLD_CONTINUED:
        write(STDERR_FILENO, cmsg, sizeof(cmsg) - 1);
        break;
    default:
        write(STDERR_FILENO, umsg, sizeof(umsg) - 1);
        break;
    }
}

int main(int argc, char** argv)
{
    if (argc == 1) {
        fprintf(stderr, "Usage: %s prog args...\n", argv[0]);
        return FAIL;
    }
    sigset_t nvar, ovar;
    struct sigaction act = {.sa_sigaction = sigchld_siginfo_handler, .sa_flags = SA_RESTART | SA_SIGINFO};
    sigaction(SIGCHLD, &act, NULL);

    BLOCK_CHILD(nvar, ovar);

    int child_pid = fork();
    if (child_pid == FAIL) {
        perror("failed to fork()");
        return FAIL;
    }
    if (child_pid == 0) {
        if (execvp(argv[1], argv + 1) == FAIL) {
            perror("failed to exec()");
            return FAIL;
        }
        exit(SUCCESS);
    }
    UNBLOCK_CHILD(ovar);

    if (waitpid(child_pid, NULL, 0) == FAIL) {
        perror("error during execution");
        return FAIL;
    }
}
