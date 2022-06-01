cmake_minimum_required(VERSION 3.12)

project(rshell)

# enables c11
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wno-unused-variable 
                    -Wno-unused-parameter -Wunused-result -Wno-sign-compare -g)
add_compile_definitions(_LARGEFILE64_SOURCE)
                               
# creates executable files
set(sources main.c 
            shell.c promptline.c command.c parseline.c execute_cmd.c sig.c
            shell.h promptline.h command.h parseline.h execute_cmd.h sig.h   
            jobs.c redirection.c prompt.c
            jobs.h redirection.h prompt.h
            util/pperror.c util/vec_string.c util/config.c util/utils.c
            util/pperror.h util/vec_string.h util/config.h util/utils.h 
            util/binsearch.c 
            util/binsearch.h 
            util/flatmap.h util/vector.h util/shared_ptr.h)

add_executable(rshell ${sources})
# Test programs
add_executable(print tests/print.c)
add_executable(look_for_child tests/look_for_child.c)
add_executable(print_fds tests/print_fds.c)
add_executable(print1sec tests/print1sec.c)
add_executable(catch_tstp tests/catch_tstp.c)
add_executable(returns tests/returns.cc)
add_executable(returns1sec tests/returns1sec.cc)

target_compile_definitions(rshell PUBLIC RSHELL_NLOG)
