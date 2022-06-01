#include "promptline.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "prompt.h"
#include "sig.h"
#include "util/config.h"
#include "util/vec_string.h"

#define FAIL            -1
#define SUCCESS         0
#define DEFAULT_IOLEN   1024
#define DEFAULT_PROMPT  ">"
#define WHITESPACES     " \f\n\r\t\v"
#define CMD_DELIMETERS  "&|;"
#define COMMENT_SYMBOLS "#"

// Prints prompt to out stream.
// Return 0 os success and -1 on fail.
static void print_prompt(const char* prompt);

// Reads whole line from in fd until newline symbol.
// Appends read string to vector.
// Newline symbol will be replaced with null symbol so the line will be 
// well-formatted.
// Returns -1 on error and 0 on success.
static int read_until_newline(int in, struct vec_char_t* line);

// Returns true iff the str has pipe symbol ('|') at the end, maybe followed
// by whitespaces.
// Treats size like number of valid bytes, length + 1, so checking starts from
// size - 2 and continues down to 0.
static bool must_have_next_command(const char* str, size_t size);

// Checks that str has odd number of '\' symbols at the end.
// Treats size like number of valid bytes, length + 1, so checking starts from
// size - 2 and continues down to 0.
static bool has_line_prolongation(const char* str, size_t size);

// Removes all whitespaces at the end and leaves only one.
// Resizes the vector according to changes.
static void delete_end_whitespaces(struct vec_char_t* line);

// Removes all characters starting from the first COMMENT_SYMBOLS in the line 
static void remove_comment(struct vec_char_t* line);

// Checks that every &, &&, ||, |, ; has a token before it and after previous 
// such delimeter.
static bool has_syntax_error(const char* str, size_t size);

// Prints newline to make interaction better.
static void print_newline(int signo);

int prompt_line(struct vec_char_t* line)
{
    _shell_assert(line);

    // Clears line
    vec_char_clear(line);

    struct sigaction nact = {.sa_handler = print_newline, .sa_flags = 0};
    struct sigaction oact;
    if (sigaction(SIGINT, &nact, &oact) == FAIL) {
        _shell_pperror("failed to set signals for prompt");
        return FAIL;
    }

    // True iff the input ended with &&, ||, |
    bool wait_next_cmd = false;
    // True iff the libe ended with '\\'
    bool ask_next_line = true;
    int retval = SUCCESS;
    const char* prompt = NULL;

    // Always stops if it was asked to end prompt (with SIGINT signal).
    // If not, continues if either:
    // - Previous line ended with '\\'
    // - There is an unfinished &&, || or |
    while (ask_next_line || wait_next_cmd) {
        ask_next_line = false;

        // Starts from the beginning of the line, prints prompt.
        print_prompt(prompt);
        // Any line after the first one shold start from DEFAULT_PROMPT so user 
        // could easily understand that it's the shell input, not some program's
        prompt = DEFAULT_PROMPT;

        int readval = read_until_newline(STDIN_FILENO, line);
        if (readval != SUCCESS) {
            retval = readval;
            goto RESET_SIGNALS;
        }

        if (has_line_prolongation(vec_data(line), vec_size(line))) {
            /// To avoid cases like "|\|" to be parsed like "||" a space is 
            // placed.
            vec_char_put(line, vec_size(line) - 2, ' ');
            ask_next_line = true;
        }
        remove_comment(line);
        delete_end_whitespaces(line);

        if (has_syntax_error(vec_data(line), vec_size(line))) {
            _shell_flush_fputs("syntax error\n");
            retval = FAIL;
            goto RESET_SIGNALS;
        }

        wait_next_cmd = must_have_next_command(vec_data(line), vec_size(line));        
    }

RESET_SIGNALS:
    if (sigaction(SIGINT, &oact, NULL) == FAIL) {
        _shell_pperror("failed to reset signals after prompt");
    }

    return retval;
}

static void print_prompt(const char* prompt)
{
    if (!prompt)
        print_pretty_prompt();
    else
        fprintf(shell_outstream, "%s ", prompt);
}

static int read_until_newline(int in, struct vec_char_t* line)
{
    _shell_assert(line);

    size_t readcount = vec_size(line);
    // vec_size() is number of bytes in the string, so it includes '\0' at the 
    // end. To avoid cases like "|\n|" to be parsed like "||" a space is placed.
    // Also to avoid placing a lot of spaces at the end, places space
    // only if before there is not a space.
    // Excess whitespaces will be deleted after this function.
    if (readcount) {
        vec_char_put(line, readcount - 1, ' ');
    }

    while (true) {
        if (vec_char_resize(line, readcount + DEFAULT_IOLEN) == FAIL) {
            _shell_pperror("Failed to resize prompt");
            return FAIL;
        }

        ssize_t readcountdiff = 0;
        if ((readcountdiff = read(in, vec_data(line) + readcount, DEFAULT_IOLEN)) == FAIL) {
            if (errno == EINTR) {
                return FAIL;
            }
            _shell_pperror("Failed to read prompt response");
            return FAIL;
        }
        if (!readcountdiff) {
            return PROMPT_EOF;
        }

        readcount += readcountdiff;

        // On endline removes endline symbol(s) and places '\0' at the end to 
        // make valid c-string.
        if (readcount && vec_at(line, readcount - 1) == '\n') {
            if (readcount > 1 && vec_at(line, readcount - 2) == '\r') {
                readcount--;
            }
            vec_char_resize(line, readcount); // shrinks unused characters
            vec_char_put(line, readcount - 1, '\0');
            return SUCCESS;
        }
    }

    return FAIL;
}

static bool must_have_next_command(const char* str, size_t size)
{
    _shell_assert(str);

    if (size < 2) 
        return false;

    for (size_t i = size - 1; i > 0; --i) {
        if (isspace(str[i - 1]))
            continue;
        // |, ||, && will need some command after them
        if (str[i - 1] == '|' || (str[i - 1] == '&' && i > 1 && str[i - 2] == '&'))
            return true;
        break;
    }
    return false;
}

static bool has_line_prolongation(const char* str, size_t size)
{
    _shell_assert(str);

    if (size < 2) 
        return false;

    bool count_odd = false; 
    for (size_t i = size - 1; i > 0 && str[i - 1] == '\\'; --i) {
        count_odd = !count_odd;
    }
    return count_odd;
}

static void delete_end_whitespaces(struct vec_char_t* line)
{
    _shell_assert(line);

    size_t len = vec_size(line) - 1;

    if (!len)
        return;

    // Iterates through whitespaces until the beginning of the line or not a 
    // whitespace character.
    while (len && isspace(vec_at(line, len - 1))) 
        len--;

    // Saves only one whitespace at the end, resets both end of c-string and
    // size of vector.
    if (len + 1 < vec_size(line)) {
        vec_char_put(line, len + 1, '\0');
        vec_char_resize(line, len + 2);
    }
}

static void remove_comment(struct vec_char_t* line)
{
    _shell_assert(line);

    size_t linelen = vec_size(line) - 1;

    if (!linelen)
        return;

    size_t not_comment_len = strcspn(vec_data(line), COMMENT_SYMBOLS);
    vec_char_put(line, not_comment_len, '\0');
    vec_char_resize(line, not_comment_len + 1);
}

static bool has_syntax_error(const char* str, size_t size)
{
    _shell_assert(str);

    if (!size)
        return false;

    // True iff since the line beginning or ||, &&, |, &, ; there was a 
    // string token.
    bool token_met = false;
    for (size_t i = 0; i < size - 1; ++i) {
        if (isspace(str[i]))
            continue;
        if (strchr(CMD_DELIMETERS, str[i])) {
            // Before any of delimeters must be a token
            if (!token_met)
                return true;
            // proceeds || and &&
            if (str[i] != ';' && i + 1 < size && str[i] == str[i + 1])
                ++i;

            token_met = false;
            continue;
        }
        token_met = true;
    }

    return false;
}

static void print_newline(int signo)
{
    (void) write(shell_outfd, "\n", 1);
}
