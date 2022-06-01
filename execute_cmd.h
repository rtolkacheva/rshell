#ifndef OS_LABS_RSHELL_EXECUTE_CMD_H_
#define OS_LABS_RSHELL_EXECUTE_CMD_H_

#include <stdbool.h>

struct command;

#define EXIT_MSG    "\nexit\n"

// Executes simple command. Waits until it finishes.
// Returns 0 on success and -1 on error with errno set properly.
int execute_cmd(struct command* cmd);

// Releases all resources that are still acquired.
// It must be called after some exception caught.
// It may fail if there are any stopped jobs, but if you call it again, it will 
// free everything successfully.
int end_execution(bool print_msg);

#endif // OS_LABS_RSHELL_EXECUTE_CMD_H_
