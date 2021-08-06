#include "fake6502.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLAG_CARRY 0x01
#define FLAG_ZERO 0x02
#define FLAG_INTERRUPT 0x04
#define FLAG_DECIMAL 0x08
#define FLAG_BREAK 0x10
#define FLAG_CONSTANT 0x20
#define FLAG_OVERFLOW 0x40
#define FLAG_SIGN 0x80

#define CHECK(var, shouldbe)                                                   \
    if (cpu.var != shouldbe)                                                   \
        return printf("line %d: " #var " should've been %04x but was %04x\n",  \
                      __LINE__, shouldbe, cpu.var);
#define CHECKMEM(var, shouldbe)                                                \
    if (mem_read(&cpu, var) != (shouldbe))                                     \
        return printf("line %d: memory location " #var                         \
                      " should've been %02x but was %02x\n",                   \
                      __LINE__, shouldbe, mem_read(&cpu, var));

#define CHECKFLAG(flag, shouldbe)                                              \
    if (!!(cpu.flags & flag) != !!shouldbe)                                    \
        return printf(                                                         \
            "line %d: " #flag " should be %sset but isn't, [ %02x, %02x] \n",  \
            __LINE__, shouldbe ? "" : "re", (cpu.flags & flag), shouldbe);

#define CHECKCYCLES(r, w)                                                      \
    if (reads != r)                                                            \
        return printf("line %d: %d reads instead of %d\n", __LINE__, reads,    \
                      r);                                                      \
    if (writes != w)                                                           \
        return printf("line %d: %d writes instead of %d\n", __LINE__, writes,  \
                      w);

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

void exec_instruction(context_t *cpu, uint8_t opcode, uint8_t op1,
                      uint8_t op2) {
    mem_write(cpu, cpu->pc, opcode);
    mem_write(cpu, cpu->pc + 1, op1);
    mem_write(cpu, cpu->pc + 2, op2);

    cpu->clockticks = reads = writes = 0;

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

    CHECKFLAG(FLAG_INTERRUPT, 1);

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

    CHECKFLAG(FLAG_INTERRUPT, 1);

    // The NMI may fire even when the Interrupt flag is set
    nmi6502(&cpu);
    CHECK(s, 0x00f7);
    CHECK(pc, 0x4000);

    CHECKFLAG(FLAG_INTERRUPT, 1);

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
    CHECK(a, 0x98);

    exec_instruction(&cpu, 0x38, 0x00, 0x00); // SEC
    CHECK(pc, 0x208);

    exec_instruction(&cpu, 0xe9, 0x10, 0x00); // SBC #$10
    CHECK(pc, 0x20a);
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
    CHECK(a, 0x9c);

    exec_instruction(&cpu, 0x38, 0x00, 0x00); // SEC
    CHECK(pc, 0x208);

    exec_instruction(&cpu, 0xe9, 0x10, 0x00); // SBC #$10
    CHECK(pc, 0x20a);
    CHECK(a, 0x8c);

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

    cpu.a = 0x01;
    cpu.flags = 0x00;
    cpu.pc = 0x0200;
    exec_instruction(&cpu, 0x4a, 0x00, 0x00); // LSR A
    CHECK(a, 0x00);
    CHECK(pc, 0x0201);
    exec_instruction(&cpu, 0x4a, 0x00, 0x00); // LSR A
    CHECK(a, 0x00);
    CHECK(pc, 0x0202);
    return 0;
}

int incdec() {
    context_t cpu;
    cpu.x = cpu.y = 0x80;

    mem_write(&cpu, 0x00, 0x00);
    cpu.flags = 0x00;
    cpu.pc = 0x0200;

    exec_instruction(&cpu, 0xc6, 0x00, 0x00); // DEC $0
    CHECK(pc, 0x0202);
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 1);

    exec_instruction(&cpu, 0xe6, 0x00, 0x00); // INC $0
    CHECK(pc, 0x0204);
    CHECKFLAG(FLAG_ZERO, 1);
    CHECKFLAG(FLAG_SIGN, 0);

    exec_instruction(&cpu, 0xe8, 0x00, 0x00); // INX
    CHECK(pc, 0x0205);
    CHECK(x, 0x81);
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 1);

    exec_instruction(&cpu, 0x88, 0x00, 0x00); // DEY
    CHECK(pc, 0x0206);
    CHECK(y, 0x7f);
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 0);

    cpu.x = 0x00;
    exec_instruction(&cpu, 0xca, 0x00, 0x00); // DEX
    CHECK(pc, 0x0207);
    CHECK(y, 0x7f);
    CHECK(x, 0xff);
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 1);

    cpu.y = 0x00;
    exec_instruction(&cpu, 0xc8, 0x00, 0x00); // INY
    CHECK(pc, 0x0208);
    CHECK(y, 0x01);
    CHECK(x, 0xff);
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 0);

    return 0;
}

int branches() {
    context_t cpu;
    cpu.flags = 0x00;
    cpu.pc = 0x0200;
    exec_instruction(&cpu, 0x10, 0x60, 0x00); // BPL *+$60
    CHECK(pc, 0x0262);
    exec_instruction(&cpu, 0x30, 0x10, 0x00); // BMI *+$10
    CHECK(pc, 0x0264);
    exec_instruction(&cpu, 0x50, 0x70, 0x00); // BVC *+$70
    CHECK(pc, 0x02d6);
    exec_instruction(&cpu, 0x90, 0x70, 0x00); // BCC *+$70
    CHECK(pc, 0x0348);
    exec_instruction(&cpu, 0xb0, 0x70, 0x00); // BCS *+$70
    CHECK(pc, 0x034a);
    exec_instruction(&cpu, 0x70, 0xfa, 0x00); // BVS *-$06
    CHECK(pc, 0x034c);
    exec_instruction(&cpu, 0xd0, 0xfa, 0x00); // BNE *-$06
    CHECK(pc, 0x0348);
    exec_instruction(&cpu, 0xf0, 0xfa, 0x00); // BEQ *-$06
    CHECK(pc, 0x034a);

    cpu.flags = FLAG_CARRY | FLAG_ZERO | FLAG_SIGN | FLAG_OVERFLOW;
    exec_instruction(&cpu, 0xb0, 0x70, 0x00); // BCS *+$70
    CHECK(pc, 0x03bc);
    exec_instruction(&cpu, 0xf0, 0xfa, 0x00); // BEQ *-$06
    CHECK(pc, 0x03b8);
    exec_instruction(&cpu, 0x30, 0x10, 0x00); // BMI *+$10
    CHECK(pc, 0x03ca);
    exec_instruction(&cpu, 0x70, 0xfa, 0x00); // BVS *-$06
    CHECK(pc, 0x03c6);
    return 0;
}

int comparisons() {
    context_t cpu;
    cpu.flags = 0x00;
    cpu.pc = 0x0200;
    cpu.a = 0x50;
    cpu.x = 0x00;
    cpu.y = 0xc0;

    exec_instruction(&cpu, 0xc9, 0x00, 0x00);
    CHECKFLAG(FLAG_CARRY, 1);
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 0);

    exec_instruction(&cpu, 0xc9, 0x51, 0x00);
    CHECKFLAG(FLAG_CARRY, 0);
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 1);

    exec_instruction(&cpu, 0xe0, 0x00, 0x00);
    CHECKFLAG(FLAG_CARRY, 1);
    CHECKFLAG(FLAG_ZERO, 1);
    CHECKFLAG(FLAG_SIGN, 0);

    exec_instruction(&cpu, 0xc0, 0x01, 0x00);
    CHECKFLAG(FLAG_CARRY, 1);
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 1);

    return 0;
}

int absolute() {
    context_t cpu;
    cpu.clockticks = 0;
    cpu.pc = 0x200;
    exec_instruction(&cpu, 0xad, 0x60, 0x00);
    CHECK(ea, 0x0060);
    CHECK(pc, 0x0203);
    CHECK(clockticks, 4);
    return 0;
}

int absolute_x() {
    context_t cpu;
    cpu.pc = 0x200;
    cpu.x = 0x80;
    cpu.clockticks = 0;

    exec_instruction(&cpu, 0xbd, 0x60, 0x00);
    CHECK(ea, 0x00e0);
    CHECK(pc, 0x0203);
    CHECK(clockticks, 4);

    // Takes another cycle because of page-crossing
    cpu.clockticks = 0;
    exec_instruction(&cpu, 0xbd, 0xa0, 0x00);
    CHECK(ea, 0x0120);
    CHECK(pc, 0x0206);
    CHECK(clockticks, 5);

    // Should NOT take another cycle because of page-crossing
    cpu.clockticks = 0;
    exec_instruction(&cpu, 0x9d, 0x60, 0x00);
    CHECK(ea, 0x00e0);
    CHECK(pc, 0x0209);
    CHECK(clockticks, 5);

    cpu.clockticks = 0;
    exec_instruction(&cpu, 0x9d, 0xa0, 0x00);
    CHECK(ea, 0x0120);
    CHECK(pc, 0x020c);
    CHECK(clockticks, 5);
    return 0;
}

int absolute_y() {
    context_t cpu;
    cpu.pc = 0x200;
    cpu.y = 0x80;
    cpu.clockticks = 0;

    exec_instruction(&cpu, 0xb9, 0x60, 0x00);
    CHECK(ea, 0x00e0);
    CHECK(pc, 0x0203);
    CHECK(clockticks, 4);

    // Takes another cycle because of page-crossing
    cpu.clockticks = 0;
    exec_instruction(&cpu, 0xb9, 0xa0, 0x00);
    CHECK(ea, 0x0120);
    CHECK(pc, 0x0206);
    CHECK(clockticks, 5);

    // Should NOT take another cycle because of page-crossing
    cpu.clockticks = 0;
    exec_instruction(&cpu, 0x99, 0x60, 0x00);
    CHECK(ea, 0x00e0);
    CHECK(pc, 0x0209);
    CHECK(clockticks, 5);

    cpu.clockticks = 0;
    exec_instruction(&cpu, 0x99, 0xa0, 0x00);
    CHECK(ea, 0x0120);
    CHECK(pc, 0x020c);
    CHECK(clockticks, 5);
    return 0;
}

int indirect() {
    context_t cpu;
    cpu.pc = 0x200;
    mem_write(&cpu, 0x8000, 0x01);
    mem_write(&cpu, 0x80fe, 0x02);
    mem_write(&cpu, 0x80ff, 0x03);
    mem_write(&cpu, 0x8100, 0x04);

    cpu.clockticks = 0;
    exec_instruction(&cpu, 0x6c, 0xff, 0x80);
    CHECK(pc, 0x0103);
    CHECK(clockticks, 5);

    return 0;
}

int indirect_y() {
    context_t cpu;
    cpu.pc = 0x200;
    cpu.y = 0x80;
    mem_write(&cpu, 0x20, 0x81);
    mem_write(&cpu, 0x21, 0x20);

    // Takes another cycle because of page-crossing
    cpu.clockticks = 0;
    exec_instruction(&cpu, 0xb1, 0x20, 0x00);
    CHECK(ea, 0x2101);
    CHECK(pc, 0x0202);
    CHECK(clockticks, 6);

    // Should NOT take another cycle because of page-crossing
    cpu.clockticks = 0;
    cpu.y = 0x10;
    exec_instruction(&cpu, 0xb1, 0x20, 0x00);
    CHECK(ea, 0x2091);
    CHECK(pc, 0x0204);
    CHECK(clockticks, 5);

    // Takes 6 cycles regardless of page-crossing or not
    cpu.clockticks = 0;
    cpu.y = 0x80;
    exec_instruction(&cpu, 0x91, 0x20, 0x00);
    CHECK(ea, 0x2101);
    CHECK(pc, 0x0206);
    CHECK(clockticks, 6);

    // Takes 6 cycles regardless of page-crossing or not
    cpu.clockticks = 0;
    cpu.y = 0x10;
    exec_instruction(&cpu, 0x91, 0x20, 0x00);
    CHECK(ea, 0x2091);
    CHECK(pc, 0x0208);
    CHECK(clockticks, 6);

    return 0;
}

int indirect_x() {
    context_t cpu;
    cpu.pc = 0x200;
    cpu.x = 0x00;
    mem_write(&cpu, 0x20, 0x81);
    mem_write(&cpu, 0x21, 0x20);
    mem_write(&cpu, 0xff, 0x81);
    mem_write(&cpu, 0x00, 0x20);

    exec_instruction(&cpu, 0xa1, 0x20, 0x00);
    CHECK(ea, 0x2081);
    CHECK(pc, 0x0202);
    CHECK(clockticks, 6);

    cpu.x = 0x40;
    exec_instruction(&cpu, 0xa1, 0xe0, 0x00);
    CHECK(ea, 0x2081);
    CHECK(clockticks, 6);

    cpu.x = 0;
    exec_instruction(&cpu, 0xa1, 0x20, 0x00);
    CHECK(ea, 0x2081);
    CHECK(clockticks, 6);

    exec_instruction(&cpu, 0x81, 0xff, 0x00);
    CHECK(ea, 0x2081);
    CHECK(clockticks, 6);

    exec_instruction(&cpu, 0x81, 0x20, 0x00);
    CHECK(ea, 0x2081);
    CHECK(clockticks, 6);

    return 0;
}

int zpi() {
    context_t cpu;
    cpu.pc = 0x200;
    cpu.x = 0x59;
    mem_write(&cpu, 0x20, 0x81);
    mem_write(&cpu, 0x21, 0x20);
    mem_write(&cpu, 0xff, 0x81);
    mem_write(&cpu, 0x00, 0x20);

    exec_instruction(&cpu, 0xb2, 0x20, 0x00);
    CHECK(ea, 0x2081);
    CHECK(pc, 0x0202);
    CHECK(clockticks, 5);

    cpu.x = 0;
    exec_instruction(&cpu, 0xb2, 0x20, 0x00);
    CHECK(ea, 0x2081);
    CHECK(clockticks, 5);

    exec_instruction(&cpu, 0x92, 0xff, 0x00);
    CHECK(ea, 0x2081);
    CHECK(clockticks, 5);

    exec_instruction(&cpu, 0x92, 0x20, 0x00);
    CHECK(ea, 0x2081);
    CHECK(clockticks, 5);

    return 0;
}

int flags() {
    context_t cpu;
    cpu.pc = 0x200;
    cpu.flags = 0xff;

    exec_instruction(&cpu, 0x18, 0x00, 0x00);
    CHECKFLAG(FLAG_CARRY, 0);
    exec_instruction(&cpu, 0x38, 0x00, 0x00);
    CHECKFLAG(FLAG_CARRY, 1);

    exec_instruction(&cpu, 0x58, 0x00, 0x00);
    CHECKFLAG(FLAG_INTERRUPT, 0);
    exec_instruction(&cpu, 0x78, 0x00, 0x00);
    CHECKFLAG(FLAG_INTERRUPT, 1);

    exec_instruction(&cpu, 0xd8, 0x00, 0x00);
    CHECKFLAG(FLAG_DECIMAL, 0);
    exec_instruction(&cpu, 0xf8, 0x00, 0x00);
    CHECKFLAG(FLAG_DECIMAL, 1);

    exec_instruction(&cpu, 0xb8, 0x00, 0x00);
    CHECKFLAG(FLAG_OVERFLOW, 0);

    return 0;
}

int loads() {
    context_t cpu;
    cpu.pc = 0x200;

    exec_instruction(&cpu, 0xa0, 0x00, 0x00); // ldy #$00
    CHECKFLAG(FLAG_ZERO, 1);
    CHECKFLAG(FLAG_SIGN, 0);

    exec_instruction(&cpu, 0xa0, 0x80, 0x00); // ldy #$80
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 1);

    exec_instruction(&cpu, 0xa0, 0x7f, 0x00); // ldy #$7f
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 0);

    exec_instruction(&cpu, 0xa2, 0x00, 0x00); // ldx #$ff
    CHECKFLAG(FLAG_ZERO, 1);
    CHECKFLAG(FLAG_SIGN, 0);

    return 0;
}

int transfers() {
    context_t cpu;
    cpu.pc = 0x200;
    cpu.flags = 0x00;

    cpu.x = 0x80;

    exec_instruction(&cpu, 0x9a, 0x00, 0x00); // txs
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 0);
    CHECK(s, 0x80);

    exec_instruction(&cpu, 0x8a, 0x00, 0x00); // txa
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 1);
    CHECK(a, 0x80);

    cpu.a = 0x1;
    exec_instruction(&cpu, 0xaa, 0x00, 0x00); // tax
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 0);
    CHECK(x, 0x01);

    exec_instruction(&cpu, 0xba, 0x80, 0x00); // tsx
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 1);
    CHECK(x, 0x80);

    exec_instruction(&cpu, 0xa8, 0x00, 0x00); // tay
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 0);
    CHECK(y, 0x01);

    cpu.a = 0x80;
    exec_instruction(&cpu, 0x98, 0x00, 0x00); // tya
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 0);
    CHECK(a, 0x01);

    return 0;
}

int and_opcode() {
    context_t cpu;

    cpu.a = 0xff;
    cpu.flags = 0x00;
    cpu.pc = 0x200;

    exec_instruction(&cpu, 0x29, 0xe1, 0x00);
    CHECK(a, 0xe1);
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 1);
    CHECK(pc, 0x0202);

    exec_instruction(&cpu, 0x29, 0x71, 0x00);
    CHECK(a, 0x61);
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 0);

    exec_instruction(&cpu, 0x29, 0x82, 0x00);
    CHECK(a, 0x00);
    CHECKFLAG(FLAG_ZERO, 1);
    CHECKFLAG(FLAG_SIGN, 0);

    return 0;
}

int asl_opcode() {
    context_t cpu;

    cpu.a = 0x50;
    cpu.flags = 0x00;
    cpu.pc = 0x200;

    exec_instruction(&cpu, 0x0a, 0x00, 0x00);
    CHECK(a, 0xa0);
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 1);
    CHECKFLAG(FLAG_CARRY, 0);
    CHECK(pc, 0x0201);

    exec_instruction(&cpu, 0x0a, 0x00, 0x00);
    CHECK(a, 0x40);
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 0);
    CHECKFLAG(FLAG_CARRY, 1);

    exec_instruction(&cpu, 0x0a, 0x00, 0x00);
    CHECK(a, 0x80);
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 1);
    CHECKFLAG(FLAG_CARRY, 0);

    exec_instruction(&cpu, 0x0a, 0x00, 0x00);
    CHECK(a, 0x00);
    CHECKFLAG(FLAG_ZERO, 1);
    CHECKFLAG(FLAG_SIGN, 0);
    CHECKFLAG(FLAG_CARRY, 1);

    return 0;
}

int bit_opcode() {
    context_t cpu;

    cpu.a = 0x50;
    cpu.flags = 0x00;
    cpu.pc = 0x200;
    mem_write(&cpu, 0xff, 0x80);

    exec_instruction(&cpu, 0x24, 0xff, 0x00);
    CHECK(a, 0x50);
    CHECKFLAG(FLAG_ZERO, 1);
    CHECKFLAG(FLAG_SIGN, 1);
    CHECKFLAG(FLAG_CARRY, 0);
    CHECK(pc, 0x0202);

    cpu.a = 0x40;
    mem_write(&cpu, 0xff, 0x40);
    exec_instruction(&cpu, 0x2c, 0xff, 0x00);
    CHECK(a, 0x40);
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 0);
    CHECKFLAG(FLAG_CARRY, 0);
    CHECK(pc, 0x0205);

    return 0;
}

int bit_imm_opcode() {
    context_t cpu;

    cpu.a = 0x50;
    cpu.flags = 0x00;
    cpu.pc = 0x200;

    exec_instruction(&cpu, 0x89, 0xff, 0x00);
    CHECK(a, 0x50);
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 0);
    CHECKFLAG(FLAG_CARRY, 0);
    CHECK(pc, 0x0202);

    exec_instruction(&cpu, 0x89, 0x80, 0x00);
    CHECK(a, 0x50);
    CHECKFLAG(FLAG_ZERO, 1);
    CHECKFLAG(FLAG_SIGN, 0);
    CHECKFLAG(FLAG_CARRY, 0);
    CHECK(pc, 0x0204);

    return 0;
}

int brk_opcode() {
    context_t cpu;
    reset6502(&cpu);

    cpu.flags = FLAG_ZERO | FLAG_SIGN | FLAG_CARRY | FLAG_OVERFLOW;
    cpu.pc = 0x200;
    mem_write(&cpu, 0xfffe, 0x00);
    mem_write(&cpu, 0xffff, 0x60);

    exec_instruction(&cpu, 0x00, 0x00, 0x00);
    CHECKFLAG(FLAG_ZERO, 1);
    CHECKFLAG(FLAG_SIGN, 1);
    CHECKFLAG(FLAG_CARRY, 1);
    CHECKFLAG(FLAG_INTERRUPT, 1);
    CHECKFLAG(FLAG_OVERFLOW, 1);

    CHECK(s, 0x00fa);
    CHECK(pc, 0x6000);

    CHECKMEM(0x01fd, 0x02);
    CHECKMEM(0x01fc, 0x02);
    CHECKMEM(0x01fb, FLAG_ZERO | FLAG_SIGN | FLAG_CARRY | FLAG_OVERFLOW |
                         FLAG_BREAK | FLAG_CONSTANT);

    return 0;
}

int eor_opcode() {
    context_t cpu;
    reset6502(&cpu);

    cpu.flags = 0;
    cpu.pc = 0x200;
    cpu.a = 0xf0;

    exec_instruction(&cpu, 0x49, 0x43, 0x00);
    CHECKFLAG(FLAG_SIGN, 1);
    CHECKFLAG(FLAG_ZERO, 0);
    CHECK(a, 0xb3);

    exec_instruction(&cpu, 0x49, 0xb3, 0x00);
    CHECKFLAG(FLAG_SIGN, 0);
    CHECKFLAG(FLAG_ZERO, 1);
    CHECK(a, 0x00);

    return 0;
}

int jsr_opcode() {
    context_t cpu;
    reset6502(&cpu);

    cpu.flags = 0;
    cpu.pc = 0x300;

    exec_instruction(&cpu, 0x20, 0x43, 0x00); // JSR $0043
    CHECK(pc, 0x0043);

    // check the return address on the stack
    // (Because RTS increments the PC after fetching it, JSR actually pushes
    // PC-1, so we need to check that the return address is $0302)
    CHECK(s, 0xfb);
    CHECKMEM(0x01fd, 0x03);
    CHECKMEM(0x01fc, 0x02);

    exec_instruction(&cpu, 0x60, 0x00, 0x00); // RTS
    CHECK(s, 0xfd);
    // (RTS increments the return address, so it's $303, the location
    // immediately after the JSR instruction)
    CHECK(pc, 0x303);

    return 0;
}

int rla_opcode() {
    context_t cpu;

    cpu.a = 0x23;
    cpu.flags = 0xf6; // Turn off the carry flag and decimal mode
    mem_write(&cpu, 0x01, 0x12);

    cpu.pc = 0x200;
    reads = 0;
    writes = 0;
    exec_instruction(&cpu, 0x27, 0x01, 0x00);

    CHECKCYCLES(3, 2);
    CHECKMEM(0x01, 0x24);

    CHECK(pc, 0x0202);
    CHECK(ea, 0x0001);
    CHECK(a, 0x20);
    return 0;
}

int rra_opcode() {
    context_t cpu;

    cpu.a = 0x3;
    cpu.flags = 0xf6; // Turn off the carry flag and decimal mode
    mem_write(&cpu, 0x01, 0x02);

    cpu.pc = 0x200;
    reads = 0;
    writes = 0;
    exec_instruction(&cpu, 0x67, 0x01, 0x00);

    CHECKCYCLES(3, 2);
    CHECKMEM(0x01, 0x01);

    CHECK(pc, 0x0202);
    CHECK(ea, 0x0001);
    CHECK(a, 0x04);
    return 0;
}

int nop_opcode() {
    context_t cpu;
    cpu.pc = 0x200;
    exec_instruction(&cpu, 0xea, 0x00, 0x00); // nop
    return 0;
}

int ora_opcode() {
    context_t cpu;
    cpu.pc = 0x200;
    cpu.a = 0x00;
    exec_instruction(&cpu, 0x09, 0x00, 0x00); // ora #$00
    CHECKFLAG(FLAG_SIGN, 0);
    CHECKFLAG(FLAG_ZERO, 1);
    CHECK(a, 0x00);
    exec_instruction(&cpu, 0x09, 0x01, 0x00); // ora #$01
    CHECKFLAG(FLAG_SIGN, 0);
    CHECKFLAG(FLAG_ZERO, 0);
    CHECK(a, 0x01);
    exec_instruction(&cpu, 0x09, 0x02, 0x00); // ora #$02
    CHECKFLAG(FLAG_SIGN, 0);
    CHECKFLAG(FLAG_ZERO, 0);
    CHECK(a, 0x03);
    exec_instruction(&cpu, 0x09, 0x82, 0x00); // ora #$82
    CHECKFLAG(FLAG_SIGN, 1);
    CHECKFLAG(FLAG_ZERO, 0);
    CHECK(a, 0x83);
    return 0;
}

int rol_opcode() {
    context_t cpu;
    cpu.pc = 0x200;
    cpu.a = 0x20;
    cpu.flags = 0x00; // Make sure Carry's clear

    exec_instruction(&cpu, 0x2a, 0x00, 0x00); // rol
    CHECKFLAG(FLAG_CARRY, 0);
    CHECKFLAG(FLAG_SIGN, 0);
    CHECK(a, 0x40);

    exec_instruction(&cpu, 0x2a, 0x00, 0x00); // rol
    CHECKFLAG(FLAG_CARRY, 0);
    CHECKFLAG(FLAG_SIGN, 1);
    CHECK(a, 0x80);

    exec_instruction(&cpu, 0x2a, 0x00, 0x00); // rol
    CHECKFLAG(FLAG_CARRY, 1);
    CHECKFLAG(FLAG_SIGN, 0);
    CHECK(a, 0x00);

    exec_instruction(&cpu, 0x2a, 0x00, 0x00); // rol
    CHECKFLAG(FLAG_CARRY, 0);
    CHECKFLAG(FLAG_SIGN, 0);
    CHECK(a, 0x01);
    return 0;
}

int rti_opcode() {
    context_t cpu;
    cpu.pc = 0x200;
    cpu.a = 0x20;
    cpu.flags = 0x00;
    reset6502(&cpu);

    CHECK(s, 0xfd);
    mem_write(&cpu, 0x1fe, 0x01);
    mem_write(&cpu, 0x1ff, 0x02);
    mem_write(&cpu, 0x100, 0x03);

    exec_instruction(&cpu, 0x40, 0x00, 0x00); // rti
    CHECK(s, 0x00);
    CHECK(pc, 0x0302);
    CHECKFLAG(FLAG_CARRY, 1);
    CHECKFLAG(FLAG_SIGN, 0);
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_INTERRUPT, 0);
    CHECKFLAG(FLAG_DECIMAL, 0);
    CHECKFLAG(FLAG_OVERFLOW, 0);

    return 0;
}

int sax_opcode() {
    context_t cpu;
    cpu.pc = 0x200;
    cpu.a = 0x03;
    cpu.x = 0x06;
    cpu.flags = 0x00;
    uint8_t address = 0xff;

    mem_write(&cpu, address, 0x03);

    exec_instruction(&cpu, 0x87, address, 0x00); // sax address
    CHECK(pc, 0x0202);
    CHECKMEM(address, 0x02);

    return 0;
}

int sta_opcode() {
    context_t cpu;
    cpu.pc = 0x200;
    cpu.a = 0x20;
    cpu.flags = 0x00;
    uint8_t address = 0xff;

    mem_write(&cpu, address, 0x03);

    exec_instruction(&cpu, 0x85, address, 0x00); // sta address
    CHECK(pc, 0x0202);
    CHECKMEM(address, 0x20);

    return 0;
}

int stx_opcode() {
    context_t cpu;
    cpu.pc = 0x200;
    cpu.x = 0x80;
    cpu.flags = 0x00;
    uint8_t address = 0xff;

    mem_write(&cpu, address, 0x03);

    exec_instruction(&cpu, 0x86, address, 0x00); // sta address
    CHECK(pc, 0x0202);
    CHECKMEM(address, 0x80);

    return 0;
}

int sty_opcode() {
    context_t cpu;
    cpu.pc = 0x200;
    cpu.y = 0x01;
    cpu.flags = 0x00;
    uint8_t address = 0xff;

    mem_write(&cpu, 0xff, 0x03);

    exec_instruction(&cpu, 0x84, address, 0x00); // sta address
    CHECK(pc, 0x0202);
    CHECKMEM(address, 0x01);

    return 0;
}

int stz_opcode() {
    context_t cpu;
    cpu.pc = 0x200;
    cpu.flags = 0x00;
    uint8_t address = 0xff;

    mem_write(&cpu, 0xff, 0x03);

    exec_instruction(&cpu, 0x64, address, 0x00); // stz address
    CHECK(pc, 0x0202);
    CHECKMEM(address, 0);

    return 0;
}

int sre_opcode() {
    context_t cpu;

    cpu.a = 0x3;
    cpu.pc = 0x200;
    mem_write(&cpu, 0x01, 0x02);

    exec_instruction(&cpu, 0x47, 0x01, 0x00); // LSE $01
    CHECKMEM(0x01, 0x01);

    CHECK(pc, 0x0202);
    CHECK(ea, 0x0001);
    CHECK(a, 0x02);
    return 0;
}

int lax_opcode() {
    context_t cpu;
    cpu.pc = 0x200;

    mem_write(&cpu, 0xab, 0x00);
    exec_instruction(&cpu, 0xa7, 0xab, 0x00); // lax $ab
    CHECKFLAG(FLAG_ZERO, 1);
    CHECKFLAG(FLAG_SIGN, 0);

    mem_write(&cpu, 0xab, 0x7b);
    exec_instruction(&cpu, 0xa7, 0xab, 0x00); // lax $ab
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 0);

    mem_write(&cpu, 0xab, 0x8a);
    exec_instruction(&cpu, 0xa7, 0xab, 0x00); // lax $ab
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_SIGN, 1);

    return 0;
}

/*
   See this document:
   http://www.zimmers.net/anonftp/pub/cbm/documents/chipdata/6502-NMOS.extra.opcodes
   for information about how illegal opcodes work
*/

int cmos_jmp_indirect() {
    context_t cpu;
    cpu.pc = 0x200;
    mem_write(&cpu, 0x8000, 0x01);
    mem_write(&cpu, 0x80fe, 0x02);
    mem_write(&cpu, 0x80ff, 0x03);
    mem_write(&cpu, 0x8100, 0x04);

    cpu.clockticks = 0;
    exec_instruction(&cpu, 0x6c, 0xff, 0x80);
    CHECK(pc, 0x0403);
    CHECK(clockticks, 6);

    return 0;
}

int cmos_jmp_absxi() {
    context_t cpu;
    cpu.pc = 0x0200;
    cpu.x = 0xff;
    mem_write(&cpu, 0x1456, 0xcd);
    mem_write(&cpu, 0x1457, 0xab);
    exec_instruction(&cpu, 0x7c, 0x57, 0x13);
    CHECK(pc, 0xabcd);
    CHECK(clockticks, 6);
    return 0;
}

int pushme_pullyou() {
    context_t cpu;
    reset6502(&cpu);
    cpu.pc = 0x0200;
    cpu.x = 0xff;
    cpu.y = 0x01;
    cpu.a = 0x00;
    cpu.flags = 0x00;
    exec_instruction(&cpu, 0xda, 0x0, 0x0); // phx
    CHECK(s, 0xfc);
    CHECKMEM(0x1fd, 0xff);
    exec_instruction(&cpu, 0x5a, 0x0, 0x0); // phy
    CHECK(s, 0xfb);
    CHECKMEM(0x1fc, 0x01);
    exec_instruction(&cpu, 0x48, 0x0, 0x0); // pha
    CHECK(s, 0xfa);
    CHECKMEM(0x1fb, 0x00);
    exec_instruction(&cpu, 0x7a, 0x0, 0x0); // ply
    CHECK(s, 0xfb);
    CHECK(y, 0x00);
    exec_instruction(&cpu, 0x28, 0x0, 0x0); // plp
    CHECK(s, 0xfc);
    CHECKFLAG(FLAG_CARRY, 1);
    CHECKFLAG(FLAG_ZERO, 0);
    CHECKFLAG(FLAG_INTERRUPT, 0);
    CHECKFLAG(FLAG_DECIMAL, 0);
    CHECKFLAG(FLAG_SIGN, 0);
    CHECKFLAG(FLAG_OVERFLOW, 0);
    exec_instruction(&cpu, 0x68, 0x0, 0x0); // pla
    CHECK(s, 0xfd);
    CHECK(a, 0xff);
    exec_instruction(&cpu, 0x08, 0x0, 0x0); // php
    CHECK(s, 0xfc);
    exec_instruction(&cpu, 0xfa, 0x0, 0x0); // plx
    CHECK(s, 0xfd);
    CHECK(x, (FLAG_SIGN | FLAG_CARRY | FLAG_CONSTANT | FLAG_BREAK));

    return 0;
}

typedef struct {
    char *testname;
    int (*fp)();
} test_t;

test_t tests[] = {{"interrupts", &interrupt},
                  {"zero page addressing", &zp},
                  {"indexed zero page addressing", &zpx},
                  {"absolute addressing", &absolute},
                  {"absolute,x addressing", &absolute_x},
                  {"absolute,y addressing", &absolute_y},
                  {"indirect,y addressing", &indirect_y},
                  {"indirect,x addressing", &indirect_x},
                  {"decimal mode", decimal_mode},
                  {"flags set & reset", flags},
                  {"binary mode", binary_mode},
                  {"push & pull", &pushpull},
                  {"rotations", &rotations},
                  {"branches", &branches},
                  {"comparisons", &comparisons},
                  {"increments and decrements", &incdec},
                  {"loads", &loads},
                  {"transfers", &transfers},
                  {"and", &and_opcode},
                  {"asl", &asl_opcode},
                  {"bit", &bit_opcode},
                  {"brk", &brk_opcode},
                  {"eor", &eor_opcode},
                  {"jsr & rts", &jsr_opcode},
                  {"nop", &nop_opcode},
                  {"ora", &ora_opcode},
                  {"rol", &rol_opcode},
                  {"rti", &rti_opcode},
                  {"sta", &sta_opcode},
                  {"stx", &stx_opcode},
                  {"sty", &sty_opcode},
                  {NULL, NULL}};

test_t nmos_tests[] = {{"indirect addressing", &indirect},
                       {"rra", &rra_opcode},
                       {"rla", &rla_opcode},
                       {"sre", &sre_opcode},
                       {"sax", &sax_opcode},
                       {"lax", &lax_opcode},
                       {NULL, NULL}};

test_t cmos_tests[] = {{"CMOS jmp indirect", &cmos_jmp_indirect},
                       {"Immediate BIT", &bit_imm_opcode},
                       {"(absolute,x)", &cmos_jmp_absxi},
                       {"(zp) addressing", &zpi},
                       {"pushes and pulls", &pushme_pullyou},
                       {"stz", &stz_opcode},
                       {NULL, NULL}};

int run_tests(test_t tests[]) {
    for (int i = 0; tests[i].fp; i++) {
        if (tests[i].fp()) {
            printf("\033[0;31m%s failed\033[0m\n", tests[i].testname);
            exit(1);
        }
        printf("\033[0;33m%s okay\033[0m\n", tests[i].testname);
    }
}

int main(int argc, char **argv) {
    run_tests(tests);
    if (!strcmp(argv[1], "cmos"))
        run_tests(cmos_tests);
    if (!strcmp(argv[1], "nmos"))
        run_tests(nmos_tests);
}
