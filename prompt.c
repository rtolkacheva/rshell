#include "prompt.h"

#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "util/config.h"

#define USER_HOST_DELIMETER     "@"
#define HOST_PATH_DELIMETER     ":"
#define ROOT_PROMPT_END         "#"
#define USER_PROMPT_END         "$"
#define HOME_SYMBOL             '~'

#define ROOT_SIMPLE_PROMPT      "rshell #"
#define USER_SIMPLE_PROMPT      "rshell $"

#define FAIL                    -1
#define SUCCESS                 0
#define HOSTNAME_MAXLEN         256
#define CWD_MAXLEN              1024

void print_pretty_prompt()
{
    uid_t uid = getuid();
    struct passwd* passwd = getpwuid(uid);
    if (!passwd)
        goto ERROR;
    char hostname[HOSTNAME_MAXLEN];
    if (gethostname(hostname, HOSTNAME_MAXLEN) == FAIL)
        goto ERROR;
    char cwd[CWD_MAXLEN];
    if (getcwd(cwd, CWD_MAXLEN) == NULL)
        goto ERROR;
    
    char* home = passwd->pw_dir;
    if (home) {
        size_t homelen = strlen(home);
        if (!homelen || homelen >= CWD_MAXLEN)
            goto BREAK;
        char tmp = cwd[homelen];
        cwd[homelen] = '\0';
        if (strcmp(home, cwd) == 0) {
            cwd[0] = HOME_SYMBOL;
            cwd[homelen] = tmp;
            memmove(cwd + 1, cwd + homelen, strlen(cwd + homelen) + 1);
        }
        else {
            cwd[homelen] = tmp;
        }
    }
BREAK:

    fprintf(shell_outstream, "%s%s%s%s%s%s ", passwd->pw_name, USER_HOST_DELIMETER,
            hostname, HOST_PATH_DELIMETER, cwd, uid ? USER_PROMPT_END : ROOT_PROMPT_END);
    return;
    
ERROR:
    fprintf(shell_outstream, "%s ", uid ? USER_SIMPLE_PROMPT : ROOT_SIMPLE_PROMPT);
}
