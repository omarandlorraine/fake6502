#include "fake6502.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#define CHECK(var, shouldbe) if(cpu. var != shouldbe) return printf( #var " should've been %d but was %d\n", shouldbe, cpu. var);

uint8_t mem[65536];

uint8_t mem_read(context_t * c, uint16_t addr) {
	return mem[addr];
}

void mem_write(context_t * c, uint16_t addr, uint8_t val) {
	mem[addr] = val;
}

int zp() {
	context_t cpu;

	mem_write(&cpu, 0x200, 0xa5);
	mem_write(&cpu, 0x201, 0x03);
	cpu.pc = 0x200;
	step(&cpu);

	CHECK(pc, 0x0202);
	CHECK(ea, 0x0003);
	return 0;

}



struct { char * testname; int (*fp)(); } tests[] = { 
	{"zero page addressing", &zp},
	{NULL, NULL}
};

int main() {
	for(int i = 0; tests[i].fp; i++) {
		int result = tests[i].fp();
		printf("%s %s\n", tests[i].testname, result ? "failed!" : "okay");
		if(result)
			exit(result);
	}
}
