# Project 1: Reader / Writer locks

Overview
--------
In this project, you will modify a reader/writer lock:
- Version 1 uses a mutex and a counter to provide a reader/writer lock with writer priority.
- You must profile the lock and determine the hotspot
- Version 2 will use a distributed counter to implement a reader/writer lock with writer priority.

The benchmark tests performance and correctness when atomically modifiying an array of items, with up to 16 threads (8 cores and 2 hyperthreads per core on CADE).


 
Your Contributions
--------


Version 1:
* `lock.c`: You have been given a project with `rw_lock_init`, `read_lock`, `read_unlock`, `write_lock`, and `write_unlock` filled out. You should be able to compile and run `benchmark.c` without triggering a panic.

Profiling: Using perf, gdb, or your favorite debugging suite, identify the hotspot in the code

Version 2:
* `lock.c` / `lock.h`: After profiling, use a distributed counter to fix the hotspot.

Build
-------

TThe makefile builds with clang or clang++ and should work out-of-the-box on CADE.

The test file is `benchmark.c`. To build, run

```bash
 $ make benchmark
 $ ./benchmark 100 5 10000 100
```

The arguments to the benchmark are, in order:

* nreaders: How many reader threads to launch
* nwriters: How many writer threads to launch
* nitems: # of items in the array
* niters: # of iterations. In each iteration a thread acquires the lock once.

All arguments must be >= 1.

Debugging
------------
To build the project with debug flags, add the `D=1` flag when building:

```
 $ make clean && make D=1
```

After that you can use gdb to set breakpoints and step through the code. For more info on debugging in C, check out the man page (`man gdb` or https://man7.org/linux/man-pages/man1/gdb.1.html), or come to office hours!

