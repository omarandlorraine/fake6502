CC=clang
CFLAGS=-g -Werror -pedantic
GCOV=-fprofile-arcs -ftest-coverage

.PHONY: default
default: fake6502.o fake2a03.o fake65c02.o

fake6502.o: fake6502.c
	$(CC) -DNMOS6502 -c $(CFLAGS) fake6502.c -o $@
fake65c02.o: fake6502.c
	$(CC) -DCMOS6502 -c $(CFLAGS) fake6502.c -o $@
fake2a03.o: fake6502.c
	$(CC) -DNES_CPU -DNMOS6502 -c $(CFLAGS) fake6502.c -o $@

tests: fake6502.c tests.c
	gcc $(GCOV) $(CFLAGS) tests.c -c -o tests.o
	gcc $(GCOV) -DNMOS6502 -c $(CFLAGS) fake6502.c -o fake6502.o
	gcc -lgcov --coverage tests.o fake6502.o -o tests

test: tests
	./tests
	gcov fake6502.c

.PHONY: clean
clean:
	rm *.o *.gcov *.gcda *.gcno tests
	
