#include "fake6502.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#define CHECK(var, shouldbe) if(cpu. var != shouldbe) return printf("line %d: " #var " should've been %04x but was %04x\n", __LINE__, shouldbe, cpu. var);
#define CHECKMEM(var, shouldbe) if(mem_read(&cpu, var ) != (shouldbe)) return printf( "memory location " #var " should've been %02x but was %02x\n", shouldbe, mem_read(&cpu, var ));

uint8_t mem[65536];

int reads, writes;

uint8_t mem_read(context_t * c, uint16_t addr) {
	reads++;
	return mem[addr];
}

void mem_write(context_t * c, uint16_t addr, uint8_t val) {
	writes++;
	mem[addr] = val;
}

void print_address(uint16_t addr) {
	printf("    %04x: %02x\n", addr, mem_read((context_t *)NULL, addr));
}

void print_stack() {
	for(int i = 0xff; i; i--) {
		print_address(0x0100 + i);
	}
}

int interrupt() {
	context_t cpu;

	// It doesn't matter what we set cpu.flags to here, but valgrind checks
	// that each bit is initialised.
	cpu.flags = 0xff;
	
	// Populate the interrupt vectors
	mem_write(&cpu, 0xfffa, 0x00);
	mem_write(&cpu, 0xfffb, 0x40);
	mem_write(&cpu, 0xfffc, 0x00);
	mem_write(&cpu, 0xfffd, 0x50);
	mem_write(&cpu, 0xfffe, 0x00);
	mem_write(&cpu, 0xffff, 0x60);
	
	// On reset, a 6502 initialises the stack pointer to 0xFD, and jumps to the
	// address at 0xfffc. Also, the interrupt flag is cleared.
	cpu.flags &= ~0x04;
	reset6502(&cpu);
	CHECK(s,  0x00fd);
	CHECK(pc, 0x5000);

	if(!(cpu.flags & 0x04))
		return printf("the reset did not set the interrupt flag\n");

	// This IRQ shouldn't fire because the interrupts are disabled
	irq6502(&cpu);
	CHECK(s,  0x00fd);
	CHECK(pc, 0x5000);

	// Enable interrupts and try again
	cpu.flags &= ~0x04;
	irq6502(&cpu);

	// On IRQ, a 6502 pushes the PC and flags onto the stack and then fetches PC
	// from the vector at 0xFFFE
	CHECK(s,  0x00fa);
	CHECK(pc, 0x6000);

	CHECKMEM(0x01fd, 0x50);
	CHECKMEM(0x01fc, 0x00);
	CHECKMEM(0x01fb, cpu.flags & 0xeb);

	if(!(cpu.flags & 0x04))
		return printf("the irq did not set the interrupt flag\n");

	// The NMI may fire even when the Interrupt flag is set
	nmi6502(&cpu);
	CHECK(s,  0x00f7);
	CHECK(pc, 0x4000);

	if(!(cpu.flags & 0x04))
		return printf("the nmi did not set the interrupt flag\n");

	return 0;
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

int zpx() {
	context_t cpu;

	cpu.x = 1;
	cpu.y = 1;

	// lda $03,x
	mem_write(&cpu, 0x200, 0xb5);
	mem_write(&cpu, 0x201, 0x03);
	cpu.pc = 0x200;
	step(&cpu);

	CHECK(pc, 0x0202);
	CHECK(ea, 0x0004);

	// lda $ff,x
	mem_write(&cpu, 0x201, 0xff);
	cpu.pc = 0x200;
	step(&cpu);

	CHECK(pc, 0x0202);
	CHECK(ea, 0x0000);

	// ldx $03,y
	mem_write(&cpu, 0x200, 0xb6);
	mem_write(&cpu, 0x201, 0x03);
	cpu.pc = 0x200;
	step(&cpu);

	CHECK(pc, 0x0202);
	CHECK(ea, 0x0004);


	// ldx $ff,y
	mem_write(&cpu, 0x200, 0xb6);
	mem_write(&cpu, 0x201, 0xff);
	cpu.pc = 0x200;
	step(&cpu);

	CHECK(pc, 0x0202);
	CHECK(ea, 0x0000);


	return 0;
}

int decimal_mode() {
	context_t cpu;

	cpu.a = 0x89;
	cpu.pc = 0x200;
	cpu.flags = 0x08; // Turn on decimal mode, clear carry flag

	mem_write(&cpu, 0x200, 0x69); // LDA immediate
	mem_write(&cpu, 0x201, 0x01); // operand, 0x01;
	mem_write(&cpu, 0x202, 0x69); // LDA immediate
	mem_write(&cpu, 0x203, 0x10); // operand, 0x10;
	mem_write(&cpu, 0x204, 0x18); // CLC
	mem_write(&cpu, 0x205, 0xe9); // SBC immediate
	mem_write(&cpu, 0x206, 0x01); // operand, 0x01;
	mem_write(&cpu, 0x207, 0xe9); // SBC immediate
	mem_write(&cpu, 0x208, 0x10); // operand, 0x10;

	step(&cpu);
	CHECK(pc, 0x202);
	CHECK(a,  0x90);

	step(&cpu);
	CHECK(pc, 0x204);
	CHECK(a,  0x00);
	
	step(&cpu);
	CHECK(pc, 0x205);
	
	step(&cpu);
	CHECK(pc, 0x207);
	CHECK(a,  0x99);
	
	step(&cpu);
	CHECK(pc, 0x209);
	CHECK(a,  0x88);
	
	return 0;
}

int binary_mode() {
	context_t cpu;

	cpu.a = 0x89;
	cpu.pc = 0x200;
	cpu.flags = 0x00; // Turn off decimal mode, clear carry flag

	mem_write(&cpu, 0x200, 0x69); // LDA immediate
	mem_write(&cpu, 0x201, 0x01); // operand, 0x01;
	mem_write(&cpu, 0x202, 0x69); // LDA immediate
	mem_write(&cpu, 0x203, 0x14); // operand, 0x10;
	mem_write(&cpu, 0x204, 0x18); // CLC
	mem_write(&cpu, 0x205, 0xe9); // SBC immediate
	mem_write(&cpu, 0x206, 0x01); // operand, 0x01;
	mem_write(&cpu, 0x207, 0xe9); // SBC immediate
	mem_write(&cpu, 0x208, 0x10); // operand, 0x10;

	step(&cpu);
	CHECK(pc, 0x202);
	CHECK(a,  0x8a);

	step(&cpu);
	CHECK(pc, 0x204);
	CHECK(a,  0x9e);
	
	step(&cpu);
	CHECK(pc, 0x205);
	
	step(&cpu);
	CHECK(pc, 0x207);
	CHECK(a,  0x9d);
	
	step(&cpu);
	CHECK(pc, 0x209);
	CHECK(a,  0x8d);
	
	return 0;
}

int rra_opcode() {
	context_t cpu;

	cpu.a = 0x3;
	cpu.flags &= 0xfe; // Turn off the carry flag
	mem_write(&cpu, 0x01, 0x02);

	mem_write(&cpu, 0x200, 0x67);
	mem_write(&cpu, 0x201, 0x01);
	cpu.pc = 0x200;
	reads = 0;
	writes = 0;
	step(&cpu);

	if(reads != 3)
		return printf("rra zero-page did %d reads instead of 3\n", reads);
	if(writes != 2)
		return printf("rra zero-page did %d writes instead of 2\n", writes);
	if(mem_read(&cpu, 0x01) != 0x01)
		return printf("the memory location didn't get rotated");

	CHECK(pc, 0x0202);
	CHECK(ea, 0x0001);
	CHECK(a,  0x04);
	return 0;
}

/*
   See this document:
   http://www.zimmers.net/anonftp/pub/cbm/documents/chipdata/6502-NMOS.extra.opcodes
   for information about how illegal opcodes work
*/

struct { char * testname; int (*fp)(); } tests[] = { 
	{"interrupts", &interrupt},
	{"zero page addressing", &zp},
	{"indexed zero page addressing", &zpx},
	{"decimal mode", decimal_mode},
	{"binary mode", binary_mode},
	{"rra", &rra_opcode},
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
