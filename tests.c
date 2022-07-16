
// -------------------------------------------------------------------
// include's
// -------------------------------------------------------------------

#include "fake6502.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// -------------------------------------------------------------------
// define's
// -------------------------------------------------------------------



// -------------------------------------------------------------------
// macro's
// -------------------------------------------------------------------

#define CHECK(var, shouldbe)                                                   \
    if (f6502.var != shouldbe)                                                 \
        return( printf("line %d: " #var " should've been %04x but was %04x\n", \
                      __LINE__, shouldbe, f6502.var) );

#define CHECKMEM(var, shouldbe)                                                \
    if (fake6502_mem_read(&f6502, var) != (shouldbe))                                   \
        return( printf("line %d: memory location " #var                        \
                      " should've been %02x but was %02x\n",                   \
                      __LINE__, shouldbe, fake6502_mem_read(&f6502, var)) );

#define CHECKFLAG(flag, shouldbe)                                              \
    if (!!(f6502.cpu.flags & flag) != !!shouldbe)                              \
        return( printf(                                                        \
            "line %d: " #flag " should be %sset but isn't, [ %02x, %02x] \n",  \
            __LINE__, shouldbe ? "" : "re", (f6502.cpu.flags & flag), shouldbe) );

#define CHECKCYCLES(r, w)                                                      \
    if (reads != r)                                                            \
        return printf("line %d: %d reads instead of %d\n", __LINE__, reads,    \
                      r);                                                      \
    if (writes != w)                                                           \
        return printf("line %d: %d writes instead of %d\n", __LINE__, writes,  \
                      w);


// -------------------------------------------------------------------
// typedef's
// -------------------------------------------------------------------

// all/any data needed by the host code
// eg. the passing of the 64K memory space

typedef struct test_host_state {
    uint8_t *memory;
    // any other data your host might need
} test_host_state;


// a test entry

typedef struct test_t {
    char *testname;
    int (*fp)();
} test_t;


// -------------------------------------------------------------------
// global's
// -------------------------------------------------------------------

uint8_t mem[65536];

int reads, writes;

test_host_state test_data;


// -------------------------------------------------------------------
// function's
// -------------------------------------------------------------------

// emulator support

uint8_t fake6502_mem_read(fake6502_context *c, uint16_t addr) {
    reads++;
    return( ((test_host_state*)c->state_host)->memory[addr] );
}

void fake6502_mem_write(fake6502_context *c, uint16_t addr, uint8_t val) {
    writes++;
    ((test_host_state*)c->state_host)->memory[addr] = val;
}


// -------------------------------------------------------------------

// testing support

void test_init(fake6502_context *cpu) {

    test_data.memory = mem;
    cpu->state_host = (void*)&test_data;

    fake6502_reset(cpu);
}

void exec_instruction(fake6502_context *cpu, uint8_t opcode, uint8_t op1,
                      uint8_t op2) {
    fake6502_mem_write(cpu, cpu->cpu.pc, opcode);
    fake6502_mem_write(cpu, cpu->cpu.pc + 1, op1);
    fake6502_mem_write(cpu, cpu->cpu.pc + 2, op2);

    cpu->emu.instructions = cpu->emu.clockticks = reads = writes = 0;

    fake6502_step(cpu);
}

int interrupt() {
    fake6502_context f6502;

    test_init(&f6502);

    // It doesn't matter what we set f6502.cpu.flags to here, but valgrind checks
    // that each bit is initialised.
    f6502.cpu.flags = 0xff;

    // Populate the interrupt vectors
    fake6502_mem_write(&f6502, 0xfffa, 0x00);
    fake6502_mem_write(&f6502, 0xfffb, 0x40);
    fake6502_mem_write(&f6502, 0xfffc, 0x00);
    fake6502_mem_write(&f6502, 0xfffd, 0x50);
    fake6502_mem_write(&f6502, 0xfffe, 0x00);
    fake6502_mem_write(&f6502, 0xffff, 0x60);

    // On reset, a 6502 initialises the stack pointer to 0xFD, and jumps to the
    // address at 0xfffc. Also, the interrupt flag is set.
    fake6502_interrupt_clear(&f6502);
    fake6502_reset(&f6502);
    CHECK(cpu.s, 0x00fd);
    CHECK(cpu.pc, 0x5000);

    CHECKFLAG(FAKE6502_INTERRUPT_FLAG, 1);

    // This IRQ shouldn't fire because the interrupts are disabled
    fake6502_irq(&f6502);
    CHECK(cpu.s, 0x00fd);
    CHECK(cpu.pc, 0x5000);

    // Enable interrupts and try again
    fake6502_interrupt_clear(&f6502);
    fake6502_irq(&f6502);

    // On IRQ, a 6502 pushes the PC and flags onto the stack and then fetches PC
    // from the vector at 0xFFFE
    CHECK(cpu.s, 0x00fa);
    CHECK(cpu.pc, 0x6000);

    CHECKMEM(0x01fd, 0x50);
    CHECKMEM(0x01fc, 0x00);
    CHECKMEM(0x01fb, f6502.cpu.flags & 0xeb);

    CHECKFLAG(FAKE6502_INTERRUPT_FLAG, 1);

    // The NMI may fire even when the Interrupt flag is set
    fake6502_nmi(&f6502);
    CHECK(cpu.s, 0x00f7);
    CHECK(cpu.pc, 0x4000);

    CHECKFLAG(FAKE6502_INTERRUPT_FLAG, 1);

    return(0);
}


// -------------------------------------------------------------------

// testing core

int zp() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.pc = 0x200;
    exec_instruction(&f6502, 0xa5, 0x03, 0x00);

    CHECK(cpu.pc, 0x0202);
    CHECK(emu.ea, 0x0003);

    return(0);
}

int zpx() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.x = 1;
    f6502.cpu.y = 1;
    f6502.cpu.pc = 0x200;

    exec_instruction(&f6502, 0xb5, 0x03, 0x00); // lda $03,x
    CHECK(cpu.pc, 0x0202);
    CHECK(emu.ea, 0x0004);

    exec_instruction(&f6502, 0xb5, 0xff, 0x00); // lda $ff,x
    CHECK(cpu.pc, 0x0204);
    CHECK(emu.ea, 0x0000);

    exec_instruction(&f6502, 0xb6, 0x03, 0x00); // ldx $03,y
    CHECK(cpu.pc, 0x0206);
    CHECK(emu.ea, 0x0004);

    exec_instruction(&f6502, 0xb6, 0xff, 0x00); // ldx $ff,y
    CHECK(cpu.pc, 0x0208);
    CHECK(emu.ea, 0x0000);

    return(0);
}

int decimal_mode() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.a = 0x89;
    f6502.cpu.pc = 0x200;
    f6502.cpu.flags = 0x00;

    // Turn on decimal mode, clear carry flag

    fake6502_decimal_set(&f6502);
    fake6502_carry_clear(&f6502);

    exec_instruction(&f6502, 0x69, 0x01, 0x00); // ADC #$01
    CHECK(cpu.pc, 0x202);
    CHECK(cpu.a, 0x90);

    exec_instruction(&f6502, 0x69, 0x10, 0x00); // ADC #$10
    CHECK(cpu.pc, 0x204);
    CHECK(cpu.a, 0x00);

    exec_instruction(&f6502, 0x18, 0x00, 0x00); // CLC
    CHECK(cpu.pc, 0x205);

    exec_instruction(&f6502, 0xe9, 0x01, 0x00); // SBC #$01
    CHECK(cpu.pc, 0x207);
    CHECK(cpu.a, 0x98);

    exec_instruction(&f6502, 0x38, 0x00, 0x00); // SEC
    CHECK(cpu.pc, 0x208);

    exec_instruction(&f6502, 0xe9, 0x10, 0x00); // SBC #$10
    CHECK(cpu.pc, 0x20a);
    CHECK(cpu.a, 0x88);

    return(0);
}

int binary_mode() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.a = 0x89;
    f6502.cpu.pc = 0x200;
    f6502.cpu.flags = 0x00;

    // Turn off decimal mode, clear carry flag

    fake6502_decimal_clear(&f6502);
    fake6502_carry_clear(&f6502);

    exec_instruction(&f6502, 0x69, 0x01, 0x00); // ADC #$01
    CHECK(cpu.pc, 0x202);
    CHECK(cpu.a, 0x8a);

    exec_instruction(&f6502, 0x69, 0x14, 0x00); // ADC #$10
    CHECK(cpu.pc, 0x204);
    CHECK(cpu.a, 0x9e);

    exec_instruction(&f6502, 0x18, 0x00, 0x00); // CLC
    CHECK(cpu.pc, 0x205);

    exec_instruction(&f6502, 0xe9, 0x01, 0x00); // SBC #$01
    CHECK(cpu.pc, 0x207);
    CHECK(cpu.a, 0x9c);

    exec_instruction(&f6502, 0x38, 0x00, 0x00); // SEC
    CHECK(cpu.pc, 0x208);

    exec_instruction(&f6502, 0xe9, 0x10, 0x00); // SBC #$10
    CHECK(cpu.pc, 0x20a);
    CHECK(cpu.a, 0x8c);

    return(0);
}

int pushpull() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.a = 0x89;
    f6502.cpu.s = 0xff;
    f6502.cpu.pc = 0x0200;
    exec_instruction(&f6502, 0xa9, 0x40, 0x00); // LDA #$40
    exec_instruction(&f6502, 0x48, 0x00, 0x00); // PHA
    CHECK(cpu.s, 0xfe);
    CHECKMEM(0x01ff, 0x40);
    exec_instruction(&f6502, 0xa9, 0x00, 0x00); // LDA #$00
    exec_instruction(&f6502, 0x48, 0x00, 0x00); // PHA
    CHECK(cpu.s, 0xfd);
    CHECKMEM(0x01fe, 0x00);
    exec_instruction(&f6502, 0x60, 0x00, 0x00); // RTS
    CHECK(cpu.s, 0xff);
    CHECK(cpu.pc, 0x4001);

    return(0);
}

int rotations() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.a = 0x01;
    f6502.cpu.flags = 0x00;
    f6502.cpu.pc = 0x0200;
    exec_instruction(&f6502, 0x6a, 0x00, 0x00); // ROR A
    CHECK(cpu.a, 0x00);
    CHECK(cpu.pc, 0x0201);
    exec_instruction(&f6502, 0x6a, 0x00, 0x00); // ROR A
    CHECK(cpu.a, 0x80);
    CHECK(cpu.pc, 0x0202);

    f6502.cpu.a = 0x01;
    f6502.cpu.flags = 0x00;
    f6502.cpu.pc = 0x0200;
    exec_instruction(&f6502, 0x4a, 0x00, 0x00); // LSR A
    CHECK(cpu.a, 0x00);
    CHECK(cpu.pc, 0x0201);
    exec_instruction(&f6502, 0x4a, 0x00, 0x00); // LSR A
    CHECK(cpu.a, 0x00);
    CHECK(cpu.pc, 0x0202);

    return(0);
}

int incdec() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.x = f6502.cpu.y = 0x80;

    fake6502_mem_write(&f6502, 0x00, 0x00);
    f6502.cpu.flags = 0x00;
    f6502.cpu.pc = 0x0200;

    exec_instruction(&f6502, 0xc6, 0x00, 0x00); // DEC $0
    CHECK(cpu.pc, 0x0202);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 1);

    exec_instruction(&f6502, 0xe6, 0x00, 0x00); // INC $0
    CHECK(cpu.pc, 0x0204);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 1);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);

    exec_instruction(&f6502, 0xe8, 0x00, 0x00); // INX
    CHECK(cpu.pc, 0x0205);
    CHECK(cpu.x, 0x81);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 1);

    exec_instruction(&f6502, 0x88, 0x00, 0x00); // DEY
    CHECK(cpu.pc, 0x0206);
    CHECK(cpu.y, 0x7f);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);

    f6502.cpu.x = 0x00;
    exec_instruction(&f6502, 0xca, 0x00, 0x00); // DEX
    CHECK(cpu.pc, 0x0207);
    CHECK(cpu.y, 0x7f);
    CHECK(cpu.x, 0xff);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 1);

    f6502.cpu.y = 0x00;
    exec_instruction(&f6502, 0xc8, 0x00, 0x00); // INY
    CHECK(cpu.pc, 0x0208);
    CHECK(cpu.y, 0x01);
    CHECK(cpu.x, 0xff);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);

    return(0);
}

int branches() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.flags = 0x00;
    f6502.cpu.pc = 0x0200;
    exec_instruction(&f6502, 0x10, 0x60, 0x00); // BPL *+$60
    CHECK(cpu.pc, 0x0262);
    exec_instruction(&f6502, 0x30, 0x10, 0x00); // BMI *+$10
    CHECK(cpu.pc, 0x0264);
    exec_instruction(&f6502, 0x50, 0x70, 0x00); // BVC *+$70
    CHECK(cpu.pc, 0x02d6);
    exec_instruction(&f6502, 0x90, 0x70, 0x00); // BCC *+$70
    CHECK(cpu.pc, 0x0348);
    exec_instruction(&f6502, 0xb0, 0x70, 0x00); // BCS *+$70
    CHECK(cpu.pc, 0x034a);
    exec_instruction(&f6502, 0x70, 0xfa, 0x00); // BVS *-$06
    CHECK(cpu.pc, 0x034c);
    exec_instruction(&f6502, 0xd0, 0xfa, 0x00); // BNE *-$06
    CHECK(cpu.pc, 0x0348);
    exec_instruction(&f6502, 0xf0, 0xfa, 0x00); // BEQ *-$06
    CHECK(cpu.pc, 0x034a);

    f6502.cpu.flags = 0x00;

    // set the carry, zero, sign and overflow flags

    fake6502_carry_set(&f6502);
    fake6502_zero_set(&f6502);
    fake6502_sign_set(&f6502);
    fake6502_overflow_set(&f6502);

    exec_instruction(&f6502, 0xb0, 0x70, 0x00); // BCS *+$70
    CHECK(cpu.pc, 0x03bc);
    exec_instruction(&f6502, 0xf0, 0xfa, 0x00); // BEQ *-$06
    CHECK(cpu.pc, 0x03b8);
    exec_instruction(&f6502, 0x30, 0x10, 0x00); // BMI *+$10
    CHECK(cpu.pc, 0x03ca);
    exec_instruction(&f6502, 0x70, 0xfa, 0x00); // BVS *-$06
    CHECK(cpu.pc, 0x03c6);

    return(0);
}

int comparisons() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.flags = 0x00;
    f6502.cpu.pc = 0x0200;
    f6502.cpu.a = 0x50;
    f6502.cpu.x = 0x00;
    f6502.cpu.y = 0xc0;

    exec_instruction(&f6502, 0xc9, 0x00, 0x00);
    CHECKFLAG(FAKE6502_CARRY_FLAG, 1);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);

    exec_instruction(&f6502, 0xc9, 0x51, 0x00);
    CHECKFLAG(FAKE6502_CARRY_FLAG, 0);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 1);

    exec_instruction(&f6502, 0xe0, 0x00, 0x00);
    CHECKFLAG(FAKE6502_CARRY_FLAG, 1);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 1);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);

    exec_instruction(&f6502, 0xc0, 0x01, 0x00);
    CHECKFLAG(FAKE6502_CARRY_FLAG, 1);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 1);

    return(0);
}

int absolute() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.emu.clockticks = 0;
    f6502.cpu.pc = 0x200;
    exec_instruction(&f6502, 0xad, 0x60, 0x00);
    CHECK(emu.ea, 0x0060);
    CHECK(cpu.pc, 0x0203);
    CHECK(emu.clockticks, 4);

    return(0);
}

int absolute_x() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.pc = 0x200;
    f6502.cpu.x = 0x80;
    f6502.emu.clockticks = 0;

    exec_instruction(&f6502, 0xbd, 0x60, 0x00);
    CHECK(emu.ea, 0x00e0);
    CHECK(cpu.pc, 0x0203);
    CHECK(emu.clockticks, 4);

    // Takes another cycle because of page-crossing
    f6502.emu.clockticks = 0;
    exec_instruction(&f6502, 0xbd, 0xa0, 0x00);
    CHECK(emu.ea, 0x0120);
    CHECK(cpu.pc, 0x0206);
    CHECK(emu.clockticks, 5);

    // Should NOT take another cycle because of page-crossing
    f6502.emu.clockticks = 0;
    exec_instruction(&f6502, 0x9d, 0x60, 0x00);
    CHECK(emu.ea, 0x00e0);
    CHECK(cpu.pc, 0x0209);
    CHECK(emu.clockticks, 5);

    f6502.emu.clockticks = 0;
    exec_instruction(&f6502, 0x9d, 0xa0, 0x00);
    CHECK(emu.ea, 0x0120);
    CHECK(cpu.pc, 0x020c);
    CHECK(emu.clockticks, 5);

    return(0);
}

int absolute_y() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.pc = 0x200;
    f6502.cpu.y = 0x80;
    f6502.emu.clockticks = 0;

    exec_instruction(&f6502, 0xb9, 0x60, 0x00);
    CHECK(emu.ea, 0x00e0);
    CHECK(cpu.pc, 0x0203);
    CHECK(emu.clockticks, 4);

    // Takes another cycle because of page-crossing
    f6502.emu.clockticks = 0;
    exec_instruction(&f6502, 0xb9, 0xa0, 0x00);
    CHECK(emu.ea, 0x0120);
    CHECK(cpu.pc, 0x0206);
    CHECK(emu.clockticks, 5);

    // Should NOT take another cycle because of page-crossing
    f6502.emu.clockticks = 0;
    exec_instruction(&f6502, 0x99, 0x60, 0x00);
    CHECK(emu.ea, 0x00e0);
    CHECK(cpu.pc, 0x0209);
    CHECK(emu.clockticks, 5);

    f6502.emu.clockticks = 0;
    exec_instruction(&f6502, 0x99, 0xa0, 0x00);
    CHECK(emu.ea, 0x0120);
    CHECK(cpu.pc, 0x020c);
    CHECK(emu.clockticks, 5);

    return(0);
}

int indirect() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.pc = 0x200;
    fake6502_mem_write(&f6502, 0x8000, 0x01);
    fake6502_mem_write(&f6502, 0x80fe, 0x02);
    fake6502_mem_write(&f6502, 0x80ff, 0x03);
    fake6502_mem_write(&f6502, 0x8100, 0x04);

    f6502.emu.clockticks = 0;
    exec_instruction(&f6502, 0x6c, 0xff, 0x80);
    CHECK(cpu.pc, 0x0103);
    CHECK(emu.clockticks, 5);

    return(0);
}

int indirect_y() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.pc = 0x200;
    f6502.cpu.y = 0x80;
    fake6502_mem_write(&f6502, 0x20, 0x81);
    fake6502_mem_write(&f6502, 0x21, 0x20);

    // Takes another cycle because of page-crossing
    f6502.emu.clockticks = 0;
    exec_instruction(&f6502, 0xb1, 0x20, 0x00);
    CHECK(emu.ea, 0x2101);
    CHECK(cpu.pc, 0x0202);
    CHECK(emu.clockticks, 6);

    // Should NOT take another cycle because of page-crossing
    f6502.emu.clockticks = 0;
    f6502.cpu.y = 0x10;
    exec_instruction(&f6502, 0xb1, 0x20, 0x00);
    CHECK(emu.ea, 0x2091);
    CHECK(cpu.pc, 0x0204);
    CHECK(emu.clockticks, 5);

    // Takes 6 cycles regardless of page-crossing or not
    f6502.emu.clockticks = 0;
    f6502.cpu.y = 0x80;
    exec_instruction(&f6502, 0x91, 0x20, 0x00);
    CHECK(emu.ea, 0x2101);
    CHECK(cpu.pc, 0x0206);
    CHECK(emu.clockticks, 6);

    // Takes 6 cycles regardless of page-crossing or not
    f6502.emu.clockticks = 0;
    f6502.cpu.y = 0x10;
    exec_instruction(&f6502, 0x91, 0x20, 0x00);
    CHECK(emu.ea, 0x2091);
    CHECK(cpu.pc, 0x0208);
    CHECK(emu.clockticks, 6);

    return(0);
}

int indirect_x() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.pc = 0x200;
    f6502.cpu.x = 0x00;
    fake6502_mem_write(&f6502, 0x20, 0x81);
    fake6502_mem_write(&f6502, 0x21, 0x20);
    fake6502_mem_write(&f6502, 0xff, 0x81);
    fake6502_mem_write(&f6502, 0x00, 0x20);

    exec_instruction(&f6502, 0xa1, 0x20, 0x00);
    CHECK(emu.ea, 0x2081);
    CHECK(cpu.pc, 0x0202);
    CHECK(emu.clockticks, 6);

    f6502.cpu.x = 0x40;
    exec_instruction(&f6502, 0xa1, 0xe0, 0x00);
    CHECK(emu.ea, 0x2081);
    CHECK(emu.clockticks, 6);

    f6502.cpu.x = 0;
    exec_instruction(&f6502, 0xa1, 0x20, 0x00);
    CHECK(emu.ea, 0x2081);
    CHECK(emu.clockticks, 6);

    exec_instruction(&f6502, 0x81, 0xff, 0x00);
    CHECK(emu.ea, 0x2081);
    CHECK(emu.clockticks, 6);

    exec_instruction(&f6502, 0x81, 0x20, 0x00);
    CHECK(emu.ea, 0x2081);
    CHECK(emu.clockticks, 6);

    return(0);
}

int zpi() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.pc = 0x200;
    f6502.cpu.x = 0x59;
    fake6502_mem_write(&f6502, 0x20, 0x81);
    fake6502_mem_write(&f6502, 0x21, 0x20);
    fake6502_mem_write(&f6502, 0xff, 0x81);
    fake6502_mem_write(&f6502, 0x00, 0x20);

    exec_instruction(&f6502, 0xb2, 0x20, 0x00);
    CHECK(emu.ea, 0x2081);
    CHECK(cpu.pc, 0x0202);
    CHECK(emu.clockticks, 5);

    f6502.cpu.x = 0;
    exec_instruction(&f6502, 0xb2, 0x20, 0x00);
    CHECK(emu.ea, 0x2081);
    CHECK(emu.clockticks, 5);

    exec_instruction(&f6502, 0x92, 0xff, 0x00);
    CHECK(emu.ea, 0x2081);
    CHECK(emu.clockticks, 5);

    exec_instruction(&f6502, 0x92, 0x20, 0x00);
    CHECK(emu.ea, 0x2081);
    CHECK(emu.clockticks, 5);

    return(0);
}

int flags() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.pc = 0x200;
    f6502.cpu.flags = 0xff;

    exec_instruction(&f6502, 0x18, 0x00, 0x00);
    CHECKFLAG(FAKE6502_CARRY_FLAG, 0);
    exec_instruction(&f6502, 0x38, 0x00, 0x00);
    CHECKFLAG(FAKE6502_CARRY_FLAG, 1);

    exec_instruction(&f6502, 0x58, 0x00, 0x00);
    CHECKFLAG(FAKE6502_INTERRUPT_FLAG, 0);
    exec_instruction(&f6502, 0x78, 0x00, 0x00);
    CHECKFLAG(FAKE6502_INTERRUPT_FLAG, 1);

    exec_instruction(&f6502, 0xd8, 0x00, 0x00);
    CHECKFLAG(FAKE6502_DECIMAL_FLAG, 0);
    exec_instruction(&f6502, 0xf8, 0x00, 0x00);
    CHECKFLAG(FAKE6502_DECIMAL_FLAG, 1);

    exec_instruction(&f6502, 0xb8, 0x00, 0x00);
    CHECKFLAG(FAKE6502_OVERFLOW_FLAG, 0);

    return(0);
}

int loads() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.pc = 0x200;

    exec_instruction(&f6502, 0xa0, 0x00, 0x00); // ldy #$00
    CHECKFLAG(FAKE6502_ZERO_FLAG, 1);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);

    exec_instruction(&f6502, 0xa0, 0x80, 0x00); // ldy #$80
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 1);

    exec_instruction(&f6502, 0xa0, 0x7f, 0x00); // ldy #$7f
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);

    exec_instruction(&f6502, 0xa2, 0x00, 0x00); // ldx #$ff
    CHECKFLAG(FAKE6502_ZERO_FLAG, 1);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);

    return(0);
}

int transfers() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.pc = 0x200;
    f6502.cpu.flags = 0x00;

    f6502.cpu.x = 0x80;

    exec_instruction(&f6502, 0x9a, 0x00, 0x00); // txs
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);
    CHECK(cpu.s, 0x80);

    exec_instruction(&f6502, 0x8a, 0x00, 0x00); // txa
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 1);
    CHECK(cpu.a, 0x80);

    f6502.cpu.a = 0x1;
    exec_instruction(&f6502, 0xaa, 0x00, 0x00); // tax
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);
    CHECK(cpu.x, 0x01);

    exec_instruction(&f6502, 0xba, 0x80, 0x00); // tsx
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 1);
    CHECK(cpu.x, 0x80);

    exec_instruction(&f6502, 0xa8, 0x00, 0x00); // tay
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);
    CHECK(cpu.y, 0x01);

    f6502.cpu.a = 0x80;
    exec_instruction(&f6502, 0x98, 0x00, 0x00); // tya
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);
    CHECK(cpu.a, 0x01);

    return(0);
}

int and_opcode() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.a = 0xff;
    f6502.cpu.flags = 0x00;
    f6502.cpu.pc = 0x200;

    exec_instruction(&f6502, 0x29, 0xe1, 0x00);
    CHECK(cpu.a, 0xe1);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 1);
    CHECK(cpu.pc, 0x0202);

    exec_instruction(&f6502, 0x29, 0x71, 0x00);
    CHECK(cpu.a, 0x61);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);

    exec_instruction(&f6502, 0x29, 0x82, 0x00);
    CHECK(cpu.a, 0x00);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 1);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);

    return(0);
}

int asl_opcode() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.a = 0x50;
    f6502.cpu.flags = 0x00;
    f6502.cpu.pc = 0x200;

    exec_instruction(&f6502, 0x0a, 0x00, 0x00);
    CHECK(cpu.a, 0xa0);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 1);
    CHECKFLAG(FAKE6502_CARRY_FLAG, 0);
    CHECK(cpu.pc, 0x0201);

    exec_instruction(&f6502, 0x0a, 0x00, 0x00);
    CHECK(cpu.a, 0x40);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);
    CHECKFLAG(FAKE6502_CARRY_FLAG, 1);

    exec_instruction(&f6502, 0x0a, 0x00, 0x00);
    CHECK(cpu.a, 0x80);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 1);
    CHECKFLAG(FAKE6502_CARRY_FLAG, 0);

    exec_instruction(&f6502, 0x0a, 0x00, 0x00);
    CHECK(cpu.a, 0x00);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 1);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);
    CHECKFLAG(FAKE6502_CARRY_FLAG, 1);

    return(0);
}

int bit_opcode() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.a = 0x50;
    f6502.cpu.flags = 0x00;
    f6502.cpu.pc = 0x200;
    fake6502_mem_write(&f6502, 0xff, 0x80);

    exec_instruction(&f6502, 0x24, 0xff, 0x00);
    CHECK(cpu.a, 0x50);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 1);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 1);
    CHECKFLAG(FAKE6502_CARRY_FLAG, 0);
    CHECK(cpu.pc, 0x0202);

    f6502.cpu.a = 0x40;
    fake6502_mem_write(&f6502, 0xff, 0x40);
    exec_instruction(&f6502, 0x2c, 0xff, 0x00);
    CHECK(cpu.a, 0x40);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);
    CHECKFLAG(FAKE6502_CARRY_FLAG, 0);
    CHECK(cpu.pc, 0x0205);

    return(0);
}

int bit_imm_opcode() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.a = 0x50;
    f6502.cpu.flags = 0x00;
    f6502.cpu.pc = 0x200;

    exec_instruction(&f6502, 0x89, 0xff, 0x00);
    CHECK(cpu.a, 0x50);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);
    CHECKFLAG(FAKE6502_CARRY_FLAG, 0);
    CHECK(cpu.pc, 0x0202);

    exec_instruction(&f6502, 0x89, 0x80, 0x00);
    CHECK(cpu.a, 0x50);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 1);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);
    CHECKFLAG(FAKE6502_CARRY_FLAG, 0);
    CHECK(cpu.pc, 0x0204);

    return(0);
}

int brk_opcode() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.flags = 0x00;

    // set the carry, zero, sign and overflow flags

    fake6502_carry_set(&f6502);
    fake6502_zero_set(&f6502);
    fake6502_sign_set(&f6502);
    fake6502_overflow_set(&f6502);

    f6502.cpu.pc = 0x200;
    fake6502_mem_write(&f6502, 0xfffe, 0x00);
    fake6502_mem_write(&f6502, 0xffff, 0x60);

    exec_instruction(&f6502, 0x00, 0x00, 0x00);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 1);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 1);
    CHECKFLAG(FAKE6502_CARRY_FLAG, 1);
    CHECKFLAG(FAKE6502_INTERRUPT_FLAG, 1);
    CHECKFLAG(FAKE6502_OVERFLOW_FLAG, 1);

    CHECK(cpu.s, 0x00fa);
    CHECK(cpu.pc, 0x6000);

    CHECKMEM(0x01fd, 0x02);
    CHECKMEM(0x01fc, 0x02);
    CHECKMEM(0x01fb, FAKE6502_ZERO_FLAG | FAKE6502_SIGN_FLAG | FAKE6502_CARRY_FLAG | FAKE6502_OVERFLOW_FLAG |
                         FAKE6502_BREAK_FLAG | FAKE6502_CONSTANT_FLAG);

    return(0);
}

int eor_opcode() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.flags = 0;
    f6502.cpu.pc = 0x200;
    f6502.cpu.a = 0xf0;

    exec_instruction(&f6502, 0x49, 0x43, 0x00);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 1);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECK(cpu.a, 0xb3);

    exec_instruction(&f6502, 0x49, 0xb3, 0x00);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 1);
    CHECK(cpu.a, 0x00);

    return(0);
}

int jsr_opcode() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.flags = 0;
    f6502.cpu.pc = 0x300;

    exec_instruction(&f6502, 0x20, 0x43, 0x00); // JSR $0043
    CHECK(cpu.pc, 0x0043);

    // check the return address on the stack
    // (Because RTS increments the PC after fetching it, JSR actually pushes
    // PC-1, so we need to check that the return address is $0302)
    CHECK(cpu.s, 0xfb);
    CHECKMEM(0x01fd, 0x03);
    CHECKMEM(0x01fc, 0x02);

    exec_instruction(&f6502, 0x60, 0x00, 0x00); // RTS
    CHECK(cpu.s, 0xfd);
    // (RTS increments the return address, so it's $303, the location
    // immediately after the JSR instruction)
    CHECK(cpu.pc, 0x303);

    return(0);
}

int rla_opcode() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.a = 0x23;
    f6502.cpu.flags = 0xff;

    // Turn off the carry flag and decimal mode

    fake6502_carry_clear(&f6502);
    fake6502_decimal_clear(&f6502);

    fake6502_mem_write(&f6502, 0x01, 0x12);

    f6502.cpu.pc = 0x200;
    exec_instruction(&f6502, 0x27, 0x01, 0x00);

    CHECKCYCLES(3, 2);
    CHECKMEM(0x01, 0x24);

    CHECK(cpu.pc, 0x0202);
    CHECK(emu.ea, 0x0001);
    CHECK(cpu.a, 0x20);

    return(0);
}

int rra_opcode() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.a = 0x3;
    f6502.cpu.flags = 0xff;

    // Turn off the carry flag and decimal mode

    fake6502_carry_clear(&f6502);
    fake6502_decimal_clear(&f6502);

    fake6502_mem_write(&f6502, 0x01, 0x02);

    f6502.cpu.pc = 0x200;
    exec_instruction(&f6502, 0x67, 0x01, 0x00);

    CHECKCYCLES(3, 2);
    CHECKMEM(0x01, 0x01);

    CHECK(cpu.pc, 0x0202);
    CHECK(emu.ea, 0x0001);
    CHECK(cpu.a, 0x04);

    return(0);
}

int nop_opcode() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.pc = 0x200;
    exec_instruction(&f6502, 0xea, 0x00, 0x00); // nop

    return(0);
}

int ora_opcode() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.pc = 0x200;
    f6502.cpu.a = 0x00;
    exec_instruction(&f6502, 0x09, 0x00, 0x00); // ora #$00
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 1);
    CHECK(cpu.a, 0x00);
    exec_instruction(&f6502, 0x09, 0x01, 0x00); // ora #$01
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECK(cpu.a, 0x01);
    exec_instruction(&f6502, 0x09, 0x02, 0x00); // ora #$02
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECK(cpu.a, 0x03);
    exec_instruction(&f6502, 0x09, 0x82, 0x00); // ora #$82
    CHECKFLAG(FAKE6502_SIGN_FLAG, 1);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECK(cpu.a, 0x83);

    return(0);
}

int rol_opcode() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.pc = 0x200;
    f6502.cpu.a = 0x20;
    f6502.cpu.flags = 0x00;

    // Make sure Carry's clear

    fake6502_carry_clear(&f6502);

    exec_instruction(&f6502, 0x2a, 0x00, 0x00); // rol
    CHECKFLAG(FAKE6502_CARRY_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);
    CHECK(cpu.a, 0x40);

    exec_instruction(&f6502, 0x2a, 0x00, 0x00); // rol
    CHECKFLAG(FAKE6502_CARRY_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 1);
    CHECK(cpu.a, 0x80);

    exec_instruction(&f6502, 0x2a, 0x00, 0x00); // rol
    CHECKFLAG(FAKE6502_CARRY_FLAG, 1);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);
    CHECK(cpu.a, 0x00);

    exec_instruction(&f6502, 0x2a, 0x00, 0x00); // rol
    CHECKFLAG(FAKE6502_CARRY_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);
    CHECK(cpu.a, 0x01);

    return(0);
}

int rti_opcode() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.pc = 0x200;
    f6502.cpu.a = 0x20;
    f6502.cpu.flags = 0x00;
    fake6502_reset(&f6502);

    CHECK(cpu.s, 0xfd);
    fake6502_mem_write(&f6502, 0x1fe, 0x01);
    fake6502_mem_write(&f6502, 0x1ff, 0x02);
    fake6502_mem_write(&f6502, 0x100, 0x03);

    exec_instruction(&f6502, 0x40, 0x00, 0x00); // rti
    CHECK(cpu.s, 0x00);
    CHECK(cpu.pc, 0x0302);
    CHECKFLAG(FAKE6502_CARRY_FLAG, 1);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_INTERRUPT_FLAG, 0);
    CHECKFLAG(FAKE6502_DECIMAL_FLAG, 0);
    CHECKFLAG(FAKE6502_OVERFLOW_FLAG, 0);

    return(0);
}

int sax_opcode() {
    fake6502_context f6502;
    uint8_t address;

    test_init(&f6502);

    f6502.cpu.pc = 0x200;
    f6502.cpu.a = 0x03;
    f6502.cpu.x = 0x06;
    f6502.cpu.flags = 0x00;
    address = 0xff;

    fake6502_mem_write(&f6502, address, 0x03);

    exec_instruction(&f6502, 0x87, address, 0x00); // sax address
    CHECK(cpu.pc, 0x0202);
    CHECKMEM(address, 0x02);

    return(0);
}

int sta_opcode() {
    fake6502_context f6502;
    uint8_t address;

    test_init(&f6502);

    f6502.cpu.pc = 0x200;
    f6502.cpu.a = 0x20;
    f6502.cpu.flags = 0x00;
    address = 0xff;

    fake6502_mem_write(&f6502, address, 0x03);

    exec_instruction(&f6502, 0x85, address, 0x00); // sta address
    CHECK(cpu.pc, 0x0202);
    CHECKMEM(address, 0x20);

    return(0);
}

int stx_opcode() {
    fake6502_context f6502;
    uint8_t address;

    test_init(&f6502);

    f6502.cpu.pc = 0x200;
    f6502.cpu.x = 0x80;
    f6502.cpu.flags = 0x00;
    address = 0xff;

    fake6502_mem_write(&f6502, address, 0x03);

    exec_instruction(&f6502, 0x86, address, 0x00); // sta address
    CHECK(cpu.pc, 0x0202);
    CHECKMEM(address, 0x80);

    return(0);
}

int sty_opcode() {
    fake6502_context f6502;
    uint8_t address;

    test_init(&f6502);

    f6502.cpu.pc = 0x200;
    f6502.cpu.y = 0x01;
    f6502.cpu.flags = 0x00;
    address = 0xff;

    fake6502_mem_write(&f6502, 0xff, 0x03);

    exec_instruction(&f6502, 0x84, address, 0x00); // sta address
    CHECK(cpu.pc, 0x0202);
    CHECKMEM(address, 0x01);

    return(0);
}

int stz_opcode() {
    fake6502_context f6502;
    uint8_t address;

    test_init(&f6502);

    f6502.cpu.pc = 0x200;
    f6502.cpu.flags = 0x00;
    address = 0xff;

    fake6502_mem_write(&f6502, 0xff, 0x03);

    exec_instruction(&f6502, 0x64, address, 0x00); // stz address
    CHECK(cpu.pc, 0x0202);
    CHECKMEM(address, 0);

    return(0);
}

int sre_opcode() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.a = 0x3;
    f6502.cpu.pc = 0x200;
    fake6502_mem_write(&f6502, 0x01, 0x02);

    exec_instruction(&f6502, 0x47, 0x01, 0x00); // LSE $01
    CHECKMEM(0x01, 0x01);

    CHECK(cpu.pc, 0x0202);
    CHECK(emu.ea, 0x0001);
    CHECK(cpu.a, 0x02);

    return(0);
}

int lax_opcode() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.pc = 0x200;

    fake6502_mem_write(&f6502, 0xab, 0x00);
    exec_instruction(&f6502, 0xa7, 0xab, 0x00); // lax $ab
    CHECKFLAG(FAKE6502_ZERO_FLAG, 1);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);

    fake6502_mem_write(&f6502, 0xab, 0x7b);
    exec_instruction(&f6502, 0xa7, 0xab, 0x00); // lax $ab
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);

    fake6502_mem_write(&f6502, 0xab, 0x8a);
    exec_instruction(&f6502, 0xa7, 0xab, 0x00); // lax $ab
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 1);

    return(0);
}

/*
   See this document:
   http://www.zimmers.net/anonftp/pub/cbm/documents/chipdata/6502-NMOS.extra.opcodes
   for information about how illegal opcodes work
*/

int cmos_jmp_indirect() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.pc = 0x200;
    fake6502_mem_write(&f6502, 0x8000, 0x01);
    fake6502_mem_write(&f6502, 0x80fe, 0x02);
    fake6502_mem_write(&f6502, 0x80ff, 0x03);
    fake6502_mem_write(&f6502, 0x8100, 0x04);

    f6502.emu.clockticks = 0;
    exec_instruction(&f6502, 0x6c, 0xff, 0x80);
    CHECK(cpu.pc, 0x0403);
    CHECK(emu.clockticks, 6);

    return(0);
}

int cmos_jmp_absxi() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.pc = 0x0200;
    f6502.cpu.x = 0xff;
    fake6502_mem_write(&f6502, 0x1456, 0xcd);
    fake6502_mem_write(&f6502, 0x1457, 0xab);
    exec_instruction(&f6502, 0x7c, 0x57, 0x13);
    CHECK(cpu.pc, 0xabcd);
    CHECK(emu.clockticks, 6);

    return(0);
}

int pushme_pullyou() {
    fake6502_context f6502;

    test_init(&f6502);

    f6502.cpu.pc = 0x0200;
    f6502.cpu.x = 0xff;
    f6502.cpu.y = 0x01;
    f6502.cpu.a = 0x00;
    f6502.cpu.flags = 0x00;
    exec_instruction(&f6502, 0xda, 0x0, 0x0); // phx
    CHECK(cpu.s, 0xfc);
    CHECKMEM(0x1fd, 0xff);
    exec_instruction(&f6502, 0x5a, 0x0, 0x0); // phy
    CHECK(cpu.s, 0xfb);
    CHECKMEM(0x1fc, 0x01);
    exec_instruction(&f6502, 0x48, 0x0, 0x0); // pha
    CHECK(cpu.s, 0xfa);
    CHECKMEM(0x1fb, 0x00);
    exec_instruction(&f6502, 0x7a, 0x0, 0x0); // ply
    CHECK(cpu.s, 0xfb);
    CHECK(cpu.y, 0x00);
    exec_instruction(&f6502, 0x28, 0x0, 0x0); // plp
    CHECK(cpu.s, 0xfc);
    CHECKFLAG(FAKE6502_CARRY_FLAG, 1);
    CHECKFLAG(FAKE6502_ZERO_FLAG, 0);
    CHECKFLAG(FAKE6502_INTERRUPT_FLAG, 0);
    CHECKFLAG(FAKE6502_DECIMAL_FLAG, 0);
    CHECKFLAG(FAKE6502_SIGN_FLAG, 0);
    CHECKFLAG(FAKE6502_OVERFLOW_FLAG, 0);
    exec_instruction(&f6502, 0x68, 0x0, 0x0); // pla
    CHECK(cpu.s, 0xfd);
    CHECK(cpu.a, 0xff);
    exec_instruction(&f6502, 0x08, 0x0, 0x0); // php
    CHECK(cpu.s, 0xfc);
    exec_instruction(&f6502, 0xfa, 0x0, 0x0); // plx
    CHECK(cpu.s, 0xfd);
    CHECK(cpu.x, (FAKE6502_SIGN_FLAG | FAKE6502_CARRY_FLAG | FAKE6502_CONSTANT_FLAG | FAKE6502_BREAK_FLAG));

    return(0);
}


// -------------------------------------------------------------------

// testing code

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

  if (argc >= 2) {
    if (!strcmp(argv[1], "cmos"))
        run_tests(cmos_tests);

    if (!strcmp(argv[1], "nmos"))
        run_tests(nmos_tests);
  }

  return(0);
}


// -------------------------------------------------------------------
