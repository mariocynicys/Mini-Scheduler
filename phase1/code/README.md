# Phase 1
## Run these commands in order to test the project :
<br>

```
$ cd exe
$ make build
$ ./test_generator.o 70 procfile
$ ./process_generator.o procfile 5 1
```
`./test_generator.o 70 procfile`
<br>
generates 70 processes and writes them to a file called `procfile`.
<br>
<br>

`./process_generator.o procfile 5 1`
<br>
runs the project, using `procfile` as input file, `5` as the algorithm to be used ***Round Robin***, the third argument is optional (in this example, `1` refers to the Round Robin's quantum time)