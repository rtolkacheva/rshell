#ifndef OS_LABS_RSHELL_PROMPTLINE_H_
#define OS_LABS_RSHELL_PROMPTLINE_H_

#include <stddef.h>
#include <stdio.h>

#define PROMPT_EOF 1

struct vec_char_t;

// Returns 0 on success or -1 on error and prints error to stderr.
// Prompts a line, firstly printing prompt, then reading until line end.
// Line end is new line symbol only if the previous character is not '\'
// character.
// During reading the line will be checked for syntax eligibility. Every
// ||, &, &&, | will be checked for tokens between, before and after. 
// If the reading will be inerrupted by SIGINT, returns FAIL.
int prompt_line(struct vec_char_t* line);

#endif // OS_LABS_RSHELL_PROMPTLINE_H_
