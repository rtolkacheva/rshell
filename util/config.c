#include "config.h"

bool internal_executing;
struct vec_job_t* jobs;
pid_t shell_pgrp;
int shell_tty;
int shell_outfd;
FILE* shell_outstream;
struct sp_string_t* parsing_line;
struct termios shell_attr;
struct termios prev_attr;
pid_t waited_pid;
int waiting_pipe[2];
