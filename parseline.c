#include "parseline.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "command.h"
#include "redirection.h"
#include "util/config.h"
#include "util/pperror.h"
#include "util/vec_string.h"

#define FAIL            -1
#define SUCCESS         0
#define DELIMETERS      "|&<>; \f\n\r\t\v"
#define FILE_OPEN_MODE  0664
#define NUM_BASE        10

enum REDIRECTION_INSERT_STRATEGY {
    REDIRECTION_INSERT_FIRST,
    REDIRECTION_INSERT_LAST,
};

// Returns either pointer to next not white-space character or 
// to the end of line character.
static char* replace_whitespaces(char* str, char c);

// Tries to open file with passed flags.
// Does not print any errors.
static int try_open_file(const char* file_name, int flags);

// Adds redirection to the cmd.
// If any error met, prints it.
static int add_redirection(struct command* cmd, struct redirection* redirection,
                           enum REDIRECTION_INSERT_STRATEGY strategy);

// Resets cmd and if there was pipe to output, sets pipe for input.
static void reset_cmd_and_pipes(struct command* cmd);

// *s remains unchanged
// If the last argument is a valid number, pops it back and returns number.
static int get_fd(const struct command* cmd, char* s);

int parse_line(char* line, struct vec_command_t* commands)
{
    _shell_assert(line);
    _shell_assert(commands);

    // Exit if the string is empty
    if (!line[0])
        return FAIL;

    // Clears commands
    vec_command_foreach(commands, release_cmd);
    vec_command_clear(commands);

    char* s = line; // iterator over line
    // True iff in the previous step an argument was passed.
    bool argument_just_pushed = false;

    struct command cmd = {0};
    reset_cmd(&cmd);

    // s may become NULL after strpbrk() call because there is no delimeters 
    // after token
    while (s && (s = replace_whitespaces(s, '\0')) && *s) {
        // Pointer to the beginning of the file name.
        char* file_name = NULL;
        // Buffer character for any replacement of s.
        // Used to pass valid c-string file names.
        char buff = 0;
        // Flags for open()
        int open_flags = 0;
        // Since the redirection is quite abstract, this flag exists only here
        // to determine open_flags.
        bool append = false;
        // Buffer for any fd that will be used.
        int fd = FAIL;
        // The next value of argument_just_pushed
        bool argument_pushed = false;
        // Buffer for redirection
        struct redirection* redirection = NULL;
        switch (*s) {
        // Redirects input or both input and output
        case '<':
            // Only if in the last step argument was placed because there was no
            // knowledge that it's fd.
            if (argument_just_pushed)
                fd = get_fd(&cmd, s);
            *s++ = '\0';
            // Process <> case
            // TODO: make it valid in redirection section
            bool rwfile = false;
            if (*s == '>'){
                rwfile = true;
                *s++ = '\0';
            }
            s = replace_whitespaces(s, '\0');
            // No input file after '<' symbol
            if (!*s) {
                _shell_flush_fputs("syntax error: Unspecified redirection\n");
                goto ERROR_HANDLER;
            }
            file_name = s;
        
            s = strpbrk(s, DELIMETERS);
            // This may be not only space, but also it may be valid delimeter
            // for specific shell need like |&<>; that will be changed to \0 
            // later.
            if (s) {
                buff = *s;
                *s = '\0';
            }

            // Checks that file is valid. Checking is hapenning for every file
            // even if there is no need in that.
            // TODO: support <>
            open_flags = O_RDONLY;
            if (try_open_file(file_name, open_flags) == FAIL) {
                _shell_pperror(file_name);
                goto ERROR_HANDLER;
            }
            // Input file is only the first one met.
            redirection = make_redirection(fd == FAIL ? STDIN_FILENO : fd, 
                                           file_name, open_flags, FILE_OPEN_MODE);
            if (add_redirection(&cmd, redirection, REDIRECTION_INSERT_FIRST) == FAIL) {
                free_redirection(redirection);
                goto ERROR_HANDLER;
            }
            // TODO: do something if it's <> case
            if (rwfile) {
                _shell_flush_fprintf("Does not support <>. File %s was added as "
                    "input file\n", file_name);
            }
            if (s)
                *s = buff;
            break;
        // Redirects output or redirects in append mode
        case '>':
            // Only if in the last step argument was placed because there was no
            // knowledge that it's fd.
            if (argument_just_pushed)
                fd = get_fd(&cmd, s);
            *s++ = '\0';
            // Proceeds >> case
            if (*s == '>') {
                append = true;
                *s++ = '\0';
            }
            s = replace_whitespaces(s, '\0');
            // No input file after '<' symbol
            if (!*s) {
                _shell_flush_fputs("syntax error: Unspecified redirection\n");
                goto ERROR_HANDLER;
            }
            file_name = s;

            s = strpbrk(s, DELIMETERS);
            // This may be not only space, but also it may be valid delimeter
            // for specific shell need like |&<>; that will be changed to '\0' 
            // later.
            if (s) {
                buff = *s;
                *s = '\0';
            }
            open_flags = O_CREAT | O_WRONLY | (append ? O_APPEND : O_TRUNC);
            if (try_open_file(file_name, open_flags) == FAIL) {
                _shell_pperror(file_name);
                goto ERROR_HANDLER;
            }
            redirection = make_redirection(fd == FAIL ? STDOUT_FILENO : fd, 
                                           file_name, open_flags, FILE_OPEN_MODE);
            if (add_redirection(&cmd, redirection, REDIRECTION_INSERT_LAST) == FAIL) {
                free_redirection(redirection);
                goto ERROR_HANDLER;
            }
            if (s)
                *s = buff;
            break;
        // Handles pipe
        case '|':
            *s++ = '\0';
            // May happen if all tokens before | were redirections. Prompt does
            // not check that.
            if (vec_empty(cmd.args)) {
                _shell_flush_fputs("syntax error: No command before |\n");
                goto ERROR_HANDLER;
            }
            // Proceeds || case
            if (*s == '|') {
                cmd.flags.skip_next_on_success = true;
                *s++ = '\0';
            }
            else {
                cmd.flags.pipe_out = true;
            }
            push_cmd(&cmd, commands);
            reset_cmd_and_pipes(&cmd);
            break;
        case ';':
            *s++ = '\0';
            // Check that it was a valid command, not just redirections
            if (vec_empty(cmd.args)) {
                _shell_flush_fputs("syntax error: No command before ;\n");
                goto ERROR_HANDLER;
            }
            push_cmd(&cmd, commands);
            reset_cmd_and_pipes(&cmd);
            break;
        case '&':
            *s++ = '\0';
            if (vec_empty(cmd.args)) {
                _shell_flush_fputs("syntax error: No command before &\n");
                goto ERROR_HANDLER;
            }
            // Proceeds && case
            if (*s == '&') {
                cmd.flags.skip_next_on_fail = true;
                *s++ = '\0';
            }
            else {
                cmd.flags.bkgrnd = true;
            }
            push_cmd(&cmd, commands);
            reset_cmd_and_pipes(&cmd);
            break;
        default:
            // default case is some token -- program or it's argument
            argument_pushed = true;
            vec_string_push_back(cmd.args, s);
            s = strpbrk(s, DELIMETERS);
            break;
        }
        argument_just_pushed = argument_pushed;
    }

    if (vec_size(cmd.args)) {
        push_cmd(&cmd, commands);
    }
    
    release_cmd(&cmd);
    return SUCCESS;

ERROR_HANDLER:

    release_cmd(&cmd);
    return FAIL;
}

static char* replace_whitespaces(char* str, char c)
{
    _shell_assert(str);

    while (isspace(*str)) {
        *str = c;
        ++str;
    }

    return str;
}

static int try_open_file(const char* file_name, int flags)
{
    _shell_assert(file_name);

    int fd = open(file_name, flags, FILE_OPEN_MODE);
    if (fd == FAIL) {
        return FAIL;
    }
    // File will be opened later, right now there is no need to keep
    // files open.
    close(fd);

    return SUCCESS;
}

static int add_redirection(struct command* cmd, struct redirection* redirection,
                           enum REDIRECTION_INSERT_STRATEGY strategy)
{
    _shell_assert(cmd);
    _shell_assert(redirection);

    // Checks that fd is valid
    struct rlimit rl = {0};
    if (getrlimit(RLIMIT_NOFILE, &rl) == FAIL) {
        perror(SHELL);
        return FAIL;
    }
    if (redirection->fd >= rl.rlim_cur) {
        errno = EBADF;
        _shell_pperrorf("%d", redirection->fd);
        return FAIL;
    }

    // If strategy is to redirect to the first (< case) and some other file is
    // already redirected to the fd, puts it.
    // TODO: print error if it was inserted previously in writing mode
    // TODO: name strategy as READ/WRITE
    if (strategy == REDIRECTION_INSERT_FIRST && 
        fm_redirection_find(cmd->redirections, redirection->fd, NULL)) {
        return SUCCESS;
    }
    
    // TODO: print error if it was inserted previously with incompatible
    // strategy (READ and WRITE simultaneously)
    if (!fm_redirection_insert(cmd->redirections, redirection->fd, redirection)) {
        perror(SHELL);
        return FAIL;
    }
    return SUCCESS;
}

static void reset_cmd_and_pipes(struct command* cmd)
{
    _shell_assert(cmd);

    bool pipe_out = cmd->flags.pipe_out;
    reset_cmd(cmd);

    if (pipe_out)
        cmd->flags.pipe_in = true;
}

static int get_fd(const struct command* cmd, char* s)
{
    _shell_assert(cmd);
    _shell_assert(s);

    // The only valid file redirection syntax is fd>file or fd> file, not 
    // fd > file, in this case fd will be treated as an argument.
    if (*(s - 1) == '\0')
        return FAIL;
        
    char buff = *s;
    *s = '\0';

    char* endptr;
    char* str = vec_back(cmd->args);
    int fd = strtol(str, &endptr, NUM_BASE);
    size_t len = strlen(str);
    *s = buff;

    if (len + str != endptr) {
        return FAIL;
    }

    // If fd is a valid number, return it
    vec_string_pop_back(cmd->args);

    return fd;
}
