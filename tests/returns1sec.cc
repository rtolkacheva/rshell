#include <bits/stdc++.h>

#include <unistd.h>

int main(int argc, char** argv)
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s return time\n", argv[0]);
        return -1;
    }
    sleep(atoi(argv[2]));

    return atoi(argv[1]);    
}
