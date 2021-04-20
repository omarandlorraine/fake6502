CC=clang
CFLAGS=-g -Werror -pedantic
GCOV=-fprofile-arcs -ftest-coverage
OUTDIR=build/

.PHONY: default
default: $(OUTDIR)/fake6502.o $(OUTDIR)/fake2a03.o $(OUTDIR)/fake65c02.o

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(OUTDIR)/fake6502.o: $(OUTDIR) fake6502.c
	$(CC) -DNMOS6502 -c $(CFLAGS) fake6502.c -o $@
$(OUTDIR)/fake65c02.o: fake6502.c
	$(CC) -DCMOS6502 -c $(CFLAGS) fake6502.c -o $@
$(OUTDIR)/fake2a03.o: fake6502.c
	$(CC) -DNES_CPU -DNMOS6502 -c $(CFLAGS) fake6502.c -o $@

$(OUTDIR)/tests: fake6502.c tests.c $(OUTDIR)
	gcc $(GCOV) $(CFLAGS) tests.c -c -o $(OUTDIR)/tests.o
	gcc $(GCOV) -DNMOS6502 -c $(CFLAGS) fake6502.c -o $(OUTDIR)/fake6502.o
	gcc -lgcov --coverage $(OUTDIR)/tests.o $(OUTDIR)/fake6502.o -o $(OUTDIR)/tests

.PHONY: test
test: $(OUTDIR)/tests
	./$(OUTDIR)/tests
	gcov fake6502.c

lcov: $(OUTDIR)
	lcov --zerocounters -d $(OUTDIR)/
	lcov --capture --initial -d $(OUTDIR)/ --output-file $(OUTDIR)/coverage.info
	make test
	lcov --capture -d $(OUTDIR)/ --output-file $(OUTDIR)/coverage.info
	mkdir -p $(OUTDIR)/coverage
	cd $(OUTDIR)/coverage && genhtml ../coverage.info
	sensible-browser $(OUTDIR)/coverage/index.html

cppcheck:
	cppcheck --enable=all fake6502.c tests.c

.PHONY: clean
clean:
	rm -rf build/
	
