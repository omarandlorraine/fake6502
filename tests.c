#include "fake6502.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(var, shouldbe)                                                   \
    if (cpu.var != shouldbe)                                                   \
        return printf("line %d: " #var " should've been %04x but was %04x\n",  \
                      __LINE__, shouldbe, cpu.var);
#define CHECKMEM(var, shouldbe)                                                \
    if (mem_read(&cpu, var) != (shouldbe))                                     \
        return printf("line %d: memory location " #var                         \
                      " should've been %02x but was %02x\n",                   \
                      __LINE__, shouldbe, mem_read(&cpu, var));

uint8_t mem[65536];

int reads, writes;

uint8_t mem_read(context_t *c, uint16_t addr) {
    reads++;
    return mem[addr];
}

void mem_write(context_t *c, uint16_t addr, uint8_t val) {
    writes++;
    mem[addr] = val;
}

void print_address(uint16_t addr) {
    printf("    %04x: %02x\n", addr, mem_read((context_t *)NULL, addr));
}

void print_stack() {
    for (int i = 0xff; i; i--) {
        print_address(0x0100 + i);
    }
}

void exec_instruction(context_t *cpu, uint8_t opcode, uint8_t op1,
                      uint8_t op2) {
    mem_write(cpu, cpu->pc, opcode);
    mem_write(cpu, cpu->pc + 1, op1);
    mem_write(cpu, cpu->pc + 2, op2);
    step(cpu);
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
    CHECK(s, 0x00fd);
    CHECK(pc, 0x5000);

    if (!(cpu.flags & 0x04))
        return printf("the reset did not set the interrupt flag\n");

    // This IRQ shouldn't fire because the interrupts are disabled
    irq6502(&cpu);
    CHECK(s, 0x00fd);
    CHECK(pc, 0x5000);

    // Enable interrupts and try again
    cpu.flags &= ~0x04;
    irq6502(&cpu);

    // On IRQ, a 6502 pushes the PC and flags onto the stack and then fetches PC
    // from the vector at 0xFFFE
    CHECK(s, 0x00fa);
    CHECK(pc, 0x6000);

    CHECKMEM(0x01fd, 0x50);
    CHECKMEM(0x01fc, 0x00);
    CHECKMEM(0x01fb, cpu.flags & 0xeb);

    if (!(cpu.flags & 0x04))
        return printf("the irq did not set the interrupt flag\n");

    // The NMI may fire even when the Interrupt flag is set
    nmi6502(&cpu);
    CHECK(s, 0x00f7);
    CHECK(pc, 0x4000);

    if (!(cpu.flags & 0x04))
        return printf("the nmi did not set the interrupt flag\n");

    return 0;
}

int zp() {
    context_t cpu;

    cpu.pc = 0x200;
    exec_instruction(&cpu, 0xa5, 0x03, 0x00);

    CHECK(pc, 0x0202);
    CHECK(ea, 0x0003);
    return 0;
}

int zpx() {
    context_t cpu;

    cpu.x = 1;
    cpu.y = 1;
    cpu.pc = 0x200;

    exec_instruction(&cpu, 0xb5, 0x03, 0x00); // lda $03,x
    CHECK(pc, 0x0202);
    CHECK(ea, 0x0004);

    exec_instruction(&cpu, 0xb5, 0xff, 0x00); // lda $ff,x
    CHECK(pc, 0x0204);
    CHECK(ea, 0x0000);

    exec_instruction(&cpu, 0xb6, 0x03, 0x00); // lda $03,x
    CHECK(pc, 0x0206);
    CHECK(ea, 0x0004);

    exec_instruction(&cpu, 0xb6, 0xff, 0x00); // ldx $ff,y
    CHECK(pc, 0x0208);
    CHECK(ea, 0x0000);

    return 0;
}

int decimal_mode() {
    context_t cpu;

    cpu.a = 0x89;
    cpu.pc = 0x200;
    cpu.flags = 0x08; // Turn on decimal mode, clear carry flag

    exec_instruction(&cpu, 0x69, 0x01, 0x00); // ADC #$01
    CHECK(pc, 0x202);
    CHECK(a, 0x90);

    exec_instruction(&cpu, 0x69, 0x10, 0x00); // ADC #$10
    CHECK(pc, 0x204);
    CHECK(a, 0x00);

    exec_instruction(&cpu, 0x18, 0x00, 0x00); // CLC
    CHECK(pc, 0x205);

    exec_instruction(&cpu, 0xe9, 0x01, 0x00); // SBC #$01
    CHECK(pc, 0x207);
    CHECK(a, 0x99);

    exec_instruction(&cpu, 0xe9, 0x10, 0x00); // SBC #$10
    CHECK(pc, 0x209);
    CHECK(a, 0x88);

    return 0;
}

int binary_mode() {
    context_t cpu;

    cpu.a = 0x89;
    cpu.pc = 0x200;
    cpu.flags = 0x00; // Turn off decimal mode, clear carry flag

    exec_instruction(&cpu, 0x69, 0x01, 0x00); // ADC #$01
    CHECK(pc, 0x202);
    CHECK(a, 0x8a);

    exec_instruction(&cpu, 0x69, 0x14, 0x00); // ADC #$10
    CHECK(pc, 0x204);
    CHECK(a, 0x9e);

    exec_instruction(&cpu, 0x18, 0x00, 0x00); // CLC
    CHECK(pc, 0x205);

    exec_instruction(&cpu, 0xe9, 0x01, 0x00); // SBC #$01
    CHECK(pc, 0x207);
    CHECK(a, 0x9d);

    exec_instruction(&cpu, 0xe9, 0x10, 0x00); // SBC #$10
    CHECK(pc, 0x209);
    CHECK(a, 0x8d);

    return 0;
}

int pushpull() {
    context_t cpu;
    cpu.a = 0x89;
    cpu.s = 0xff;
    cpu.pc = 0x0200;
    exec_instruction(&cpu, 0xa9, 0x40, 0x00); // LDA #$40
    exec_instruction(&cpu, 0x48, 0x00, 0x00); // PHA
    CHECK(s, 0xfe);
    CHECKMEM(0x01ff, 0x40);
    exec_instruction(&cpu, 0xa9, 0x00, 0x00); // LDA #$00
    exec_instruction(&cpu, 0x48, 0x00, 0x00); // PHA
    CHECK(s, 0xfd);
    CHECKMEM(0x01fe, 0x00);
    exec_instruction(&cpu, 0x60, 0x00, 0x00); // RTS
    CHECK(s, 0xff);
    CHECK(pc, 0x4001);
    return 0;
}

int rotations() {
	context_t cpu;
	cpu.a = 0x01;
	cpu.flags = 0x00;
	cpu.pc = 0x0200;
	exec_instruction(&cpu, 0x6a, 0x00, 0x00); // ROR A
	CHECK(a, 0x00);
	CHECK(pc, 0x0201);
	exec_instruction(&cpu, 0x6a, 0x00, 0x00); // ROR A
	CHECK(a, 0x80);
	CHECK(pc, 0x0202);
	return 0;
}

int rra_opcode() {
    context_t cpu;

    cpu.a = 0x3;
    cpu.flags &= 0xf6; // Turn off the carry flag and decimal mode
    mem_write(&cpu, 0x01, 0x02);

    mem_write(&cpu, 0x200, 0x67);
    mem_write(&cpu, 0x201, 0x01);
    cpu.pc = 0x200;
    reads = 0;
    writes = 0;
    step(&cpu);

    if (reads != 3)
        return printf("rra zero-page did %d reads instead of 3\n", reads);
    if (writes != 2)
        return printf("rra zero-page did %d writes instead of 2\n", writes);
    if (mem_read(&cpu, 0x01) != 0x01)
        return printf("the memory location didn't get rotated");

    CHECK(pc, 0x0202);
    CHECK(ea, 0x0001);
    CHECK(a, 0x04);
    return 0;
}

/*
   See this document:
   http://www.zimmers.net/anonftp/pub/cbm/documents/chipdata/6502-NMOS.extra.opcodes
   for information about how illegal opcodes work
*/

struct {
    char *testname;
    int (*fp)();
} tests[] = {{"interrupts", &interrupt},
             {"zero page addressing", &zp},
             {"indexed zero page addressing", &zpx},
             {"decimal mode", decimal_mode},
             {"binary mode", binary_mode},
             {"rra", &rra_opcode},
             {"push & pull", &pushpull},
             {"rotations", &rotations},
             {NULL, NULL}};

int main() {
    for (int i = 0; tests[i].fp; i++) {
        int result = tests[i].fp();
        printf("%s %s\n", tests[i].testname, result ? "failed!" : "okay");
        if (result)
            exit(result);
    }
}
