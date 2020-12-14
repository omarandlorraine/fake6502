CC=clang
CFLAGS=-g -Werror -pedantic

fake6502.o: fake6502.c
	$(CC) -c $(CFLAGS) fake6502.c -o $@
fake2a02.o: fake6502.c
	$(CC) -DNESCPU -c $(CFLAGS) fake6502.c -o $@

