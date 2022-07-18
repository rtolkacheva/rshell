# RSHELL

This is rshell --- command shell for our Operatin System course 
in the Novosibirsk State University.

Its work is similar to bash', but realisation differs.

## Building

Either with CMake:

```sh
mkdir build && cd build && cmake .. && make
```

or with `build.sh`:

```sh
sh build.sh
```

## Features

### Program execution

The most important feature of any command shell --- running other
programms.
It's done quite well --- you can write any number of commands 
in one line with  these separators: `&`, `|`, `&&`, `||`, `;`.

`&&` and `||` are logic operators for branches. 
They work like in bash.

In folder `tests` there are some programs that may help you to
see how good my rshell is. 
See their documentation below.

Try this:
```sh
[1] ./print 1 && ./print 2
[2] ./print 3 || ./print 4
[3] ./print 5 || ./print 6 && ./print 7
[4] ./print 8 && ./print 9 || ./print 10 
```

`cmd1 ; cmd2` : `;` separator just separates two commands that
should be executed anyway no matter what first command returns.

`cmd1 | cmd2` : `|` is a pipe that redirects standard output of
the first command to the standard input of the second command,
if they were not redirected to any file. 
Reference for this behavior is bash, also I find thoughtful to 
use well-known behavior instead of mine own. 

`cmd1 &` : `&` rund command in the background. 
It's OK to put it in the end of the line, rshell will stay in
the foreground.

If any `&&`, `||`, `|` has no token after them, rshell will 
prompt again and again until some token passed.

### Redirections

From all possible redirections only `<`, `>` and `>>` were
implemented. 
With, of course, variant of them with file descriptor: `i<`,
`i>` and `i>>`.

If standard output or input were redirected and there is input
or output pipe, the pipe is ignored in redirection, but program
will be part of the pipeline.

### Pipeline

Pipeline is a serial of programs that are connected with pipes
like that: `cmd1 | cmd2 | cmd3 | cmd4`. 
They are considered as a single job (like in bash), every 
program in a pipeline has the same process group ID (PGID) and 
the result of the last command in the pipeline will be 
threatened like result of execution of the whole job.

Single command is a kind of pipleine too, just with lenght
 equals to 1.

### Terminal usage

For every program that must be executed in the foreground the
terminal is passed and returned back to rshell when the job
stops or exit.

### Internal commands

Some of the usual bash commands were implemented: `cd`, `fg`,
 `bg`, `jobs`, `exit`.

Output on error may be redirected to file, but not to any pipe
 since it prints to stderr.

#### CD --- Change directory

Changes current working directory as usual.

#### FG --- Move job to foreground

Wakes program with SIGCONT and passes the terminal to it and 
waits until it ends or stops.

#### BG --- Move job to the background

Wakes program with SIGCONT without passing the terminal or
waiting.

#### JOBS --- Prints list of the current jobs

Prints jobs that are running right now.

Possible output:
```sh
[1] 	Stopped         cmd1 arg1 arg2 arg3
[3] 	Running         cmd2 arg4 | cmd3 arg5 arg6 | cmd4 &
[4] 	Stopped         cmd5
[5]     Exit 1          cmd6 arg7
```

#### EXIT --- exits rshell

If there are stopped jobs, prints warning abount them.
If the next prompt returns again EOF (by default, it's possible with 
Ctrl+d) or exit, then all stopped processes will get SIGTERM and rshell
will exit.

### Signal handling 

Signals SIGCHLD, SIGQUIT, SIGTERM, SIGTSTP, SIGTTIN, 
and SIGTTOU are ignored all the time.
SIGINT may be caught during prompt and then prompt returns empty line.

SIGCHLD is being caught and in a cycle statuses of the processes will
be changed, instead of wait I use poll(2) and pipe(2). 

## Known bugs

1. `bg | bg | bg` writes to stdout concurrently

## Possible improvements

1. Improved prompt with navigation and colours
2. Command line options for rshell
3. Parse environment variables
4. Make vector macroses inline functions
5. Try to not reset SIGINT handler but set some rule in terminal attributes.
6. Validate redirections
   1. Check if there was both input and output to the same file
   2. Check that 0/1/2 descriptors were redirected correctly (0 is input, 
      1/2 output)
7. Improve argument recognition with " and '
   1. prompt will wait until second " or ' met (the same as at the beginning)
   2. parse it like one long argument
      1. but not the 0th argument that is command name
8. `jobs` may get arguments -- job numbers to print and in which order
9. Add more logging
10. Print that program was stopped just after it was stopped without waiting for other commands in line
11. Write status of job if it ended in fg but with signal or dump
12. Codestyle: make all `if` bodies surrounded with curly braces
13. Add `history` function\
14. Codestyle: refactor parser

## Testing

### Programs in `tests`

`binsearch_test.c` runs tests for `util/binsearch.c`

`catch_tstp` catches SIGTSTP and prints message. 
Default keyboard shortscut for generating SIGTSTP is Ctrl+z. 
This program may be terminated with Ctrl+\ (SIGQUIT) or Ctrl+c (SIGINT).

`print` print first command line argument for every input character in the 
infinite cycle.

`print1sec` prints first argument 100 times with gap equals 1 second.

`returns` returns first argument casted to integer (with atoi) immidiately.

`returns1sec` does the same as `returns`, but returns it in some number of 
seconds specified with second argument.

`print_fds` prints all available file descriptors from 1 to 1000 to test
if some descriptors were not closed.

`look_for_child` runs program specified by arguments (first argument is a 
program itself, the sunsequent are arguments for that command) and print
changes in its state that were caught with SIGCHLD handler.

### Useful commands

`ps o pid,ppid,pgid,sid,tpgid,s,caught,cmd` to see processes created by the 
rshell.

`xlogo` is a simple program which behavior differs if it was stopped or runs
in the background.

`vim`, `top`, `htop`, `nano`, `man` to check how program that take all terminal 
bahave.

`grep` to check pipes.
