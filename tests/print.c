#include <stdio.h>

int main(int argc, char** argv) 
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s string\n", argv[0]);
        return -1;
    }
    char c;
    while (scanf("%c", &c) && !feof(stdin) && c != '.') {
        printf("%s\n", argv[1]);
        fflush(stdout);
    }
}