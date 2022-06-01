#ifndef OS_LABS_RSHELL_PARSELINE_H_
#define OS_LABS_RSHELL_PARSELINE_H_

#include <stddef.h>

struct vec_command_t; 

// Returns 0 on success or -1 on error and prints error to stderr.
// If line is empty, returns 0.
// Line may be edited and must live as long as commands.
// Command's args will be NULL-terminated according to exec(3) format.
int parse_line(char* line, struct vec_command_t* commands);

#endif // OS_LABS_RSHELL_PARSELINE_H_
