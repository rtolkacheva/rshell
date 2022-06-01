#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#define CAUGHT "SIGTSTP caught"

void catch_tstp(int signo)
{
    write(STDOUT_FILENO, CAUGHT, sizeof(CAUGHT) - 1);
}

int main(int argc, char** argv) 
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s string\n", argv[0]);
        return -1;
    }
    struct sigaction act = {.sa_handler = catch_tstp};
    sigaction(SIGTSTP, &act, NULL);
    char c;
    while (scanf("%c", &c) && !feof(stdin) && c != '.') {
        printf("%s\n", argv[1]);
    }
}
