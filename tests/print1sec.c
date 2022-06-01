#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s string\n", argv[0]);
        return -1;
    }
    for (int i = 0; i < 100; ++i) {
        printf("%s\n", argv[1]);
        sleep(1);
    }
}