.PHONY: clean test1 test2 test3

OUT=main
CC=gcc
CFLAGS= -g -Wall -pedantic
LFLAGS= -lpthread -lm

main: main.o unboundedqueue.o
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)
	@echo ready to execute.

main.o: main.c unboundedqueue.h
	$(CC) $(CFLAGS) -c $< -o $@

unboundedqueue.o: unboundedqueue.c unboundedqueue.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OUT) *.o
	@echo cleaning completed successfully

test1: $(OUT)
	./$^ check 1

test2: $(OUT)
	./$^ check 5

test3: $(OUT)
	valgrind --leak-check=full ./$^ check 5
