#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main()
{
    for (int i = 0; i < 1000; ++i) {
        struct stat stat;
        if (fstat(i, &stat) != -1) {
            printf("%d is valid fd\n", i);
        }
    }
}
