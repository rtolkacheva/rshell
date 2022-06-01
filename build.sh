mkdir -p build

gcc -O2 -std=gnu11 -DRSHELL_NLOG *.c util/*c -o build/rshell

gcc -O2 -std=gnu11 tests/print.c -o build/print
gcc -O2 -std=gnu11 tests/look_for_child.c -o build/look_for_child
gcc -O2 -std=gnu11 tests/print_fds.c -o build/print_fds
gcc -O2 -std=gnu11 tests/print1sec.c -o build/print1sec
gcc -O2 -std=gnu11 tests/catch_tstp.c -o build/catch_tstp
g++ -O2 -std=c++11 tests/returns.cc -o build/returns
g++ -O2 -std=c++11 tests/returns1sec.cc -o build/returns1sec
