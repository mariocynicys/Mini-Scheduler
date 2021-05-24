build:
	gcc process_generator.c -o exe/process_generator.o
	gcc clk.c -o exe/clk.o
	gcc scheduler.c -o exe/scheduler.o
	gcc process.c -o exe/process.o
	gcc test_generator.c -o exe/test_generator.o

clean:
	rm -f exe/*.o

all: clean build