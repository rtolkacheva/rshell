#include "pperror.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

#define SET_ERRNO(val)      (errno = (val))
#define GET_ERRNO           (errno)

void pperrorf(const char* default_msg, const char* format, ...)
{
    int saved_errno = GET_ERRNO;

    va_list args;
    va_start(args, format);

    char buff[PRETTY_LINE_MAXLEN];
    const char* perror_msg = default_msg;
    int bufflen = 0;

    // If the format string and arguments are correct and not too long, 
    // sets message to formatted string.
    if (format
        && (bufflen = vsnprintf(buff, PRETTY_LINE_MAXLEN, format, args)) > 0 
        && bufflen < PRETTY_LINE_MAXLEN)
    {
        perror_msg = buff;
    }

    SET_ERRNO(saved_errno);
    perror(perror_msg);
    va_end(args);
}
