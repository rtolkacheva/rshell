# 1 bg, fg, jobs, exit, Ctrl+z, Ctrl+d

```sh
./print1sec 1 & ./print1sec 2 &./print 3 & # starts all of them in the background
# 1 and 2 will print "1\n" and "2\n" every second
# 3 will stop after trying to print to terminal being in background
jobs 
# must print this:
# [1] Running ./print1sec 1 &
# [2] Running ./print1sec 2 &
# [3] Stopped ./print 3
fg # last job is 3
# must print this:
# ./print 3
# then 3 works
Ctrl+z
jobs
# the same output
fg 1 # moves 1 to fg
Ctrl+z
fg 2
Ctrl+z
jobs
# must print this:
# [1] Stopped ./print1sec 1
# [2] Stopped ./print1sec 2
# [3] Stopped ./print 3
bg # last job is not last runned in the foreground, so it's again 3
jobs
# will print the same
bg 1
# will start 1 in bg
bg 1
# will print that it's already running
fg 1
Ctrl+z # just to not influence terminal after that

exit
# will print warning about stopped jobs
# now you can attach with some tracing (I recomment in this place ltrace) of 2 or 3
Ctrl+d # equivalent for exit
# will exit and send SIGTERM to stopped jobs
```

# 2 signal handling

```sh
./returns1sec 4 10 & ./returns1sec 5 15 & ./returns1sec 6 20 & ./returns1sec 7 20 & ./print 8
# strats 4, 5, 6 and 7 in the bg, gives fg to 8
# you look for running processes (with ps, for example) in other terminal the next 20 seconds
# I prefer this:
# ps o pid,ppid,pgid,sid,tpgid,s,caught,cmd
# 4, 5, 6 and 7 must return and do not turn to zombie (defunct)
Ctrl+z
# There must be printed info about 4, 5, 6, and 7 like this:
# [1] Exit 4 ./returns1sec 4 10
# [2] Exit 5 ./returns1sec 5 15
# [3] Exit 6 ./returns1sec 6 20
# [4] Exit 7 ./returns1sec 7 20
# this example demonstrates that during execution of some long-running job the 
# rshell will handle zombies and save return info

exit
# warning again
exit
```

# 3 job status

```sh
./print 1 & ./returns1sec 0 10 & ./returns1sec 500 10 & ./returns1sec -1 10 & 
jobs
# [1] Stopped ./print 1
# [2] Running ./returns1sec 0 10
# [3] Running ./returns1sec 500 10 &
# [4] Running ./returns1sec -1 10 &
# in other termnal kill 1st job
jobs
# [1] Killed ./print 1
# [2] Running ./returns1sec 0 10
# [3] Running ./returns1sec 500 10 &
# [4] Running ./returns1sec -1 10 &
# wait for few seconds
jobs
# [2] Done ./returns1sec 0 10
# [3] Exit 244 ./returns1sec 500 10
# [4] Exit 255 ./returns1sec -1 10
exit
```

# 4 logic operators

```sh
./print 1 && ./print 2
# 1 in fg
Ctrl+d
# 2 in fg
Ctrl+d # just returns fg to shell
./print 3 && ./print 4
# 3 in fg
Ctrl+z
# 4 won't start

./print 5 || ./print 6
# 5 in fg
Ctrl+d
# 6 won't start
./print 7 || ./print 8
# 7 in fg
Ctrl+z
# 8 in fg

./print 9 || ./print 10 && ./print 11
# 9 in fg
Ctrl+d
# 11 in fg
Ctrl+z # just returns fg to shell

./print 12 && ./print 13 || ./print 14
# 12 in fg
Ctrl+z
# 14 in fg
Ctrl+d # just returns fg to shell

exit
# warning
exit
```

# 5 redirections

```sh
echo line1 > text.txt
echo line2 >> text.txt
cat text.txt
# line1
# line2
./print_fds 45<text.txt 450>text2.txt 3>text3.txt
cp text.txt text4.txt
./print_fds 45<text.txt 450>text2.txt 3>text3.txt 30<text4.txt 600>text5.txt
# will write about 0, 1, 2, 3, 45, 450
./print_fds
# will write about 0, 1, 2
rm text.txt text2.txt text3.txt text4.txt text5.txt

exit
```

# 6 internal commands

## 6.1 cd

```sh
pwd
ls

# assume, that there are dir1 and dir2
cd dir1
ls
pwd

cd ../dir2
ls
pwd

cd
ls
pwd

cd /
ls
pwd

cd directory_that_does_not_exists

exit
```

## 6.2 fg

Working with stopped or bg jobs is in other tests.

```sh
# must write errors in all these cases:
fg
fg 1
fg 23424233424352
fg -1
fg 1.0
fg sdsdsgsg
fg job
fg 1^@!@*!@
fg &
fg | fg
fg & fg

exit
```

## 6.3 bg

```sh
# must write errors in all these cases:
bg
bg 1
bg 23424233424352
bg -1
bg 1.0
bg sdsdsgsg
bg job
bg 1^@!@*!@
bg | bg
bg | bg | bg
bg & bg

./print 1 &
bg
# will stop anyway

./print1sec 2
Ctrl+z
# does not print anything
bg
# will print 2 every second
bg
# will print error that it's already in the background

exit
# running in bg program will get SIGHUP
```

## 6.4 jobs

```sh
# print nothing if there is no jobs:
jobs

exit
```

# 7 redirection of internal commands

```sh
fg 2> internal.txt
fg 1 2>> internal.txt
fg -1fd 2>> internal.txt
bg 2>> internal.txt
bg 1 2>> internal.txt
bg -edwe 2>> internal.txt
fg & jobs 2>> internal.txt
bg & jobs 2>> internal.txt
./returns1sec 2 10
Crtl+z
exit 2>> internal.txt
cat internal.txt

rm internal.txt

exit
exit
```

# 8 pipelines

```sh
echo word1 | echo word2
# word2

echo word1 | cat
#word1

# usual pipelines used in the work
man 7 signal | grep mask
ls -l /usr/bin | less
ls /bin /usr/bin | sort | less
ls /bin /usr/bin | sort | wc -l
ls /bin /usr/bin | sort | uniq | less
ls /bin /usr/bin | sort | uniq | wc -l
ls /bin /usr/bin | sort | uniq | grep zip
ls /usr/bin | tail -n 5

# Testing job manipulation
./print 1 | ./print 2 | ./print 3 | ./print 4
# will print 8 of 4
Ctrl+z
# must return
jobs
# [1] Stopped ./print 1 | ./print 2 | ./print 3 | ./print 4
bg
jobs
# [1] Stopped ./print 1 | ./print 2 | ./print 3 | ./print 4
fg
# must print again 8 of 4
Ctrl+d
# must successfuly exit

exit
```

# 9 redirection with pipelines

The process' stdin/stdout won't be redirected if stdin/stdout are already 
redirected to some file.

```sh
echo word1 > words.txt | cat
# prints nothing
cat words.txt
# word1
echo word2 >> words.txt | cat < words.txt
# word1
# word2, may be not printed
echo word3 | cat < words.txt
# word1
# word2

rm words.txt

exit
```

# 10 saving terminal attributes

I don't know any programs that would broke shell iff the attributes won't be reset
or which would break if shell does not return their previous atrributes in fg.

# 11 restoring signal handlers

```sh
./look_for_child ./returns1sec 1 5
Ctrl+z
jobs
# wait some seconds
fg
# lfc must print that child stopped, continued and terminated
./look_for_child ./print 2
# prints 2
Ctrl+z
fg
# lfc must print that child stopped and continued
Ctrl+d
# lfc must print that child terminated

./catch_tstp 3
Ctrl+z
# must print message for SIGTSTP
Ctrl+\ # or Ctrl+d or Ctrl+c

exit
```
