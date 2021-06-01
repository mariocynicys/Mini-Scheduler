# Phase 2
## Run these commands in order to test the project :

<br>

```
$ cd exe
$ make build
$ ./test_generator.o 70 procfile
$ ./process_generator.o procfile -sch 5 -q 1 -mem 3
```
`./test_generator.o 70 procfile`  
generates 70 processes and writes them to a file called `procfile`.

<br>

`./process_generator.o procfile -sch 5 -q 1 -mem 3`  
runs the project, using `procfile` as input file, `5` as the algorithm to be used ***Round Robin***, the third argument is optional (in this example, `1` refers to the Round Robin's quantum time),
the `3` refers to the selected memory allocation policy.

<br>

## Notes:
- Arguments `-sch` & `-mem` must be given.

- Argument `-q` is optional. If it isn't specified, it defaults to `1` when needed.

- Arguments can be given in any order.

- Every process needs at least ***1 block*** of memory for its code segment. (i.e memory for each process should be ***> zero***)

- You can run the project from any directory (e.g `omar@lenovo:~/project$ exe/process_generator.o procfile -sch 5 -q 3 -m 4`), the `process_generator` uses the `CWD` to run the other processes relative to it self, but it uses the path provided for the input `procfile`, this means that the project is in `~/project/exe/` and the input `procfile` is in `~/project/`.

- If a process take 0 seconds to execute, its WTA is calculated as if it took 1 second.

- If multiple consequent processes take 0 seconds to execute, our sechduler will be able to manage and execute them all in the same clock cycle.
