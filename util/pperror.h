#ifndef OS_LABS_RSHELL_UTIL_PPERROR_H_
#define OS_LABS_RSHELL_UTIL_PPERROR_H_

// Maximum length of the pretty format line
#define PRETTY_LINE_MAXLEN  1024

// Writes pretty perror message with formatting.
// Tries to call perror with formatted <format> string and variable argument 
// list. If fails, calls perror with <default_msg>.
void pperrorf(const char* default_msg, const char* format, ...)
    __attribute__ ((format (printf, 2, 3)));

#endif // OS_LABS_RSHELL_UTIL_PPERROR_H_
