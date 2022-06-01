#ifndef OS_LABS_RSHELL_UTIL_CONFIG_H_
#define OS_LABS_RSHELL_UTIL_CONFIG_H_

#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <termios.h>

#include "pperror.h"

#define SHELL_VERSION "0.2.1"

#ifndef __has_builtin
#  define __has_builtin(x) 0
#endif

#ifdef RSHELL_NDEBUG
#   define _shell_assert(exp) ((void)0)
#   define _shell_unreachable() (if(0))
#else
#   include <assert.h>
#   define _shell_assert(exp) (assert(exp))
#   if __has_builtin(__builtin_unreachable) || defined(__GNUC__)
#       define _shell_unreachable() (assert(0), __builtin_unreachable())
#   else
#       define _shell_unreachable() (assert(0))
#   endif
#endif

#ifdef RSHELL_NLOG
#   define _shell_log_call(x)  if(0){}
#else
#   define _shell_log_call(x) x
#endif

// Output specific defines
#define SHELL "rshell"
#define _shell_pperror(str) pperrorf(str, "rshell: %s", str)
#define _shell_pperrorf(str, ...) pperrorf("rshell", "rshell: " str, __VA_ARGS__)
#define _shell_fprintf(str, ...) fprintf(shell_outstream, "rshell: " str, __VA_ARGS__)
#define _shell_flush_fprintf(str, ...) \
    do { \
        fprintf(shell_outstream, "rshell: " str, __VA_ARGS__); \
        fflush(shell_outstream); \
    } while (0) 

#define _shell_flush_fputs(str) \
    do { \
        fprintf(shell_outstream, "rshell: " str); \
        fflush(shell_outstream); \
    } while (0) 

// Forward declarations
struct vec_job_t;
struct sp_string_t;
struct termios;

// 0 if it's main shell, 1 if it's not. It's used to exit the child for 
// internal shell functions or to leave on SIGHUP.
extern bool internal_executing;
// Vector of jobs
extern struct vec_job_t* jobs;
// Shell's pgid
extern pid_t shell_pgrp;
// Shell's tty fd
extern int shell_tty;
// Shell's output fd (usually stderr's)
extern int shell_outfd;
// Shell's output file stream (usually stderr)
extern FILE* shell_outstream;
// Shared pointer for parsing line. It must be reset every time and released
// after every job that holds pointer is freed.
extern struct sp_string_t* parsing_line;
// Terminal attributes of the rshell
extern struct termios shell_attr;
// Attributes in the terminal that were set before shell was run
extern struct termios prev_attr;
// Pid that is being waited for
extern pid_t waited_pid;
// Pipe for passing statuses of the process with waited_pid
extern int waiting_pipe[2];

#endif // OS_LABS_RSHELL_UTIL_CONFIG_H_
