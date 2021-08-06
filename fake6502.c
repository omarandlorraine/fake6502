/* Fake6502 CPU emulator core v1.1 *******************
 * (c)2011 Mike Chambers (miker00lz@gmail.com)       *
 *****************************************************
 * v1.1 - Small bugfix in BIT opcode, but it was the *
 *        difference between a few games in my NES   *
 *        emulator working and being broken!         *
 *        I went through the rest carefully again    *
 *        after fixing it just to make sure I didn't *
 *        have any other typos! (Dec. 17, 2011)      *
 *                                                   *
 * v1.0 - First release (Nov. 24, 2011)              *
 *****************************************************
 * LICENSE: This source code is released into the    *
 * public domain, but if you use it please do give   *
 * credit. I put a lot of effort into writing this!  *
 *                                                   *
 *****************************************************
 *                                                   *
 * If you do discover an error in timing accuracy,   *
 * or operation in general please e-mail me at the   *
 * address above so that I can fix it. Thank you!    *
 *                                                   *
 *****************************************************
 * Usage:                                            *
 *                                                   *
 * Fake6502 requires you to provide two external     *
 * functions:                                        *
 *                                                   *
 * uint8_t mem_read(uint16_t address)                *
 * void mem_write(uint16_t address, uint8_t value)   *
 *                                                   *
 *****************************************************
 * Useful functions in this emulator:                *
 *                                                   *
 * void reset6502()                                  *
 *   - Call this once before you begin execution.    *
 *                                                   *
 * void step6502()                                   *
 *   - Execute a single instrution.                  *
 *                                                   *
 * void irq6502()                                    *
 *   - Trigger a hardware IRQ in the 6502 core.      *
 *                                                   *
 * void nmi6502()                                    *
 *   - Trigger an NMI in the 6502 core.              *
 *                                                   *
 *****************************************************/

#include "fake6502.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* #define NES_CPU
 * when this is defined, the binary-coded decimal (BCD) status flag is not
 * honored by ADC and SBC. the 2A03 CPU in the Nintendo Entertainment System
 * does not support BCD operation.
 */

/* #define NMOS6502
 * #define CMOS6502
 * define one or other of these. This will configure the emulator to emulate
 * either the NMOS or CMOS variants (CMOS adds bugfixes and several
 * instructions)
 */

#define FLAG_CARRY 0x01
#define FLAG_ZERO 0x02
#define FLAG_INTERRUPT 0x04
#define FLAG_DECIMAL 0x08
#define FLAG_BREAK 0x10
#define FLAG_CONSTANT 0x20
#define FLAG_OVERFLOW 0x40
#define FLAG_SIGN 0x80

#define BASE_STACK 0x100

// flag modifier macros
#define setcarry(c) c->flags |= FLAG_CARRY
#define clearcarry(c) c->flags &= (~FLAG_CARRY)
#define setzero(c) c->flags |= FLAG_ZERO
#define clearzero(c) c->flags &= (~FLAG_ZERO)
#define setinterrupt(c) c->flags |= FLAG_INTERRUPT
#define clearinterrupt(c) c->flags &= (~FLAG_INTERRUPT)
#define setdecimal(c) c->flags |= FLAG_DECIMAL
#define cleardecimal(c) c->flags &= (~FLAG_DECIMAL)
#define setoverflow(c) c->flags |= FLAG_OVERFLOW
#define clearoverflow(c) c->flags &= (~FLAG_OVERFLOW)
#define setsign(c) c->flags |= FLAG_SIGN
#define clearsign(c) c->flags &= (~FLAG_SIGN)
#define saveaccum(c, n) c->a = (uint8_t)((n)&0x00FF)

// flag calculation macros
#define zerocalc(c, n)                                                         \
    {                                                                          \
        if ((n)&0x00FF)                                                        \
            clearzero(c);                                                      \
        else                                                                   \
            setzero(c);                                                        \
    }

#define signcalc(c, n)                                                         \
    {                                                                          \
        if ((n)&0x0080)                                                        \
            setsign(c);                                                        \
        else                                                                   \
            clearsign(c);                                                      \
    }

#define carrycalc(c, n)                                                        \
    {                                                                          \
        if ((n)&0xFF00)                                                        \
            setcarry(c);                                                       \
        else                                                                   \
            clearcarry(c);                                                     \
    }

#define overflowcalc(c, n, m, o)                                               \
    { /* n = result, m = accumulator, o = memory */                            \
        if (((n) ^ (uint16_t)(m)) & ((n) ^ (o)) & 0x0080)                      \
            setoverflow(c);                                                    \
        else                                                                   \
            clearoverflow(c);                                                  \
    }

typedef struct {
    void (*addr_mode)(context_t *c);
    void (*opcode)(context_t *c);
    int clockticks;
} opcode_t;
static opcode_t opcodes[256];

// a few general functions used by various other functions
void push6502_8(context_t *c, uint8_t pushval) {
    mem_write(c, BASE_STACK + c->s--, pushval);
}

void push6502_16(context_t *c, uint16_t pushval) {
    push6502_8(c, (pushval >> 8) & 0xFF);
    push6502_8(c, pushval & 0xFF);
}

uint8_t pull6502_8(context_t *c) { return (mem_read(c, BASE_STACK + ++c->s)); }

uint16_t pull6502_16(context_t *c) {
    uint8_t t;
    t = pull6502_8(c);
    return pull6502_8(c) << 8 | t;
}

uint16_t mem_read16(context_t *c, uint16_t addr) {
    // Read two consecutive bytes from memory
    return ((uint16_t)mem_read(c, addr) |
            ((uint16_t)mem_read(c, addr + 1) << 8));
}

void reset6502(context_t *c) {
    // The 6502 normally does some fake reads after reset because
    // reset is a hacked-up version of NMI/IRQ/BRK
    // See https://www.pagetable.com/?p=410
    mem_read(c, 0x00ff);
    mem_read(c, 0x00ff);
    mem_read(c, 0x00ff);
    mem_read(c, 0x0100);
    mem_read(c, 0x01ff);
    mem_read(c, 0x01fe);
    c->pc = mem_read16(c, 0xfffc);
    c->s = 0xfd;
    c->flags |= FLAG_CONSTANT | FLAG_INTERRUPT;
}

// addressing mode functions, calculates effective addresses
static void imp(context_t *c) { // implied
}

static void acc(context_t *c) { // accumulator
}

static void imm(context_t *c) { // immediate
    c->ea = c->pc++;
}

static void zp(context_t *c) { // zero-page
    c->ea = (uint16_t)mem_read(c, (uint16_t)c->pc++);
}

static void zpx(context_t *c) { // zero-page,X
    c->ea = ((uint16_t)mem_read(c, (uint16_t)c->pc++) + (uint16_t)c->x) &
            0xFF; // zero-page wraparound
}

static void zpy(context_t *c) { // zero-page,Y
    c->ea = ((uint16_t)mem_read(c, (uint16_t)c->pc++) + (uint16_t)c->y) &
            0xFF; // zero-page wraparound
}

static void rel(context_t *c) { // relative for branch ops (8-bit immediate
                                // value, sign-extended)
    uint16_t rel = (uint16_t)mem_read(c, c->pc++);
    if (rel & 0x80)
        rel |= 0xFF00;
    c->ea = c->pc + rel;
}

static void abso(context_t *c) { // absolute
    c->ea = mem_read16(c, c->pc);
    c->pc += 2;
}

static void absx(context_t *c) { // absolute,X
    c->ea = mem_read16(c, c->pc);
    c->ea += (uint16_t)c->x;

    c->pc += 2;
}

static void absx_p(context_t *c) { // absolute,X with cycle penalty
    uint16_t startpage;
    c->ea = mem_read16(c, c->pc);
    startpage = c->ea & 0xFF00;
    c->ea += (uint16_t)c->x;
    if (startpage != (c->ea & 0xff00))
        c->clockticks++;

    c->pc += 2;
}

static void absxi(context_t *c) { // (absolute,X)
    c->ea = mem_read16(c, c->pc);
    c->ea += (uint16_t)c->x;
    c->ea = mem_read16(c, c->ea);

    c->pc += 2;
}

static void absy(context_t *c) { // absolute,Y
    c->ea = mem_read16(c, c->pc);
    c->ea += (uint16_t)c->y;

    c->pc += 2;
}

static void absy_p(context_t *c) { // absolute,Y
    uint16_t startpage;
    c->ea = mem_read16(c, c->pc);
    startpage = c->ea & 0xFF00;
    c->ea += (uint16_t)c->y;
    if (startpage != (c->ea & 0xff00))
        c->clockticks++;

    c->pc += 2;
}

#ifdef NMOS6502
static void ind(context_t *c) { // indirect
    uint16_t eahelp, eahelp2;
    eahelp = mem_read16(c, c->pc);
    eahelp2 =
        (eahelp & 0xFF00) |
        ((eahelp + 1) & 0x00FF); // replicate 6502 page-boundary wraparound bug
    c->ea =
        (uint16_t)mem_read(c, eahelp) | ((uint16_t)mem_read(c, eahelp2) << 8);
    c->pc += 2;
}
#endif

#ifdef CMOS6502
static void ind(context_t *c) { // indirect
    uint16_t eahelp;
    eahelp = mem_read16(c, c->pc);
    if ((eahelp & 0x00ff) == 0xff)
        c->clockticks++;
    c->ea = mem_read16(c, eahelp);
    c->pc += 2;
}
#endif

static void indx(context_t *c) { // (indirect,X)
    uint16_t eahelp;
    eahelp = (uint16_t)(((uint16_t)mem_read(c, c->pc++) + (uint16_t)c->x) &
                        0xFF); // zero-page wraparound for table pointer
    c->ea = (uint16_t)mem_read(c, eahelp & 0x00FF) |
            ((uint16_t)mem_read(c, (eahelp + 1) & 0x00FF) << 8);
}

static void indy(context_t *c) { // (indirect),Y
    uint16_t eahelp, eahelp2;
    eahelp = (uint16_t)mem_read(c, c->pc++);
    eahelp2 =
        (eahelp & 0xFF00) | ((eahelp + 1) & 0x00FF); // zero-page wraparound
    c->ea =
        (uint16_t)mem_read(c, eahelp) | ((uint16_t)mem_read(c, eahelp2) << 8);
    c->ea += (uint16_t)c->y;
}

static void indy_p(context_t *c) { // (indirect),Y
    uint16_t eahelp, eahelp2, startpage;
    eahelp = (uint16_t)mem_read(c, c->pc++);
    eahelp2 =
        (eahelp & 0xFF00) | ((eahelp + 1) & 0x00FF); // zero-page wraparound
    c->ea =
        (uint16_t)mem_read(c, eahelp) | ((uint16_t)mem_read(c, eahelp2) << 8);
    startpage = c->ea & 0xFF00;
    c->ea += (uint16_t)c->y;
    if (startpage != (c->ea & 0xff00))
        c->clockticks++;
}

static void zpi(context_t *c) { // (zp)
    uint16_t eahelp, eahelp2;
    eahelp = (uint16_t)mem_read(c, c->pc++);
    eahelp2 =
        (eahelp & 0xFF00) | ((eahelp + 1) & 0x00FF); // zero-page wraparound
    c->ea =
        (uint16_t)mem_read(c, eahelp) | ((uint16_t)mem_read(c, eahelp2) << 8);
}

static uint16_t getvalue(context_t *c) {
    if (opcodes[c->opcode].addr_mode == acc)
        return ((uint16_t)c->a);
    else
        return ((uint16_t)mem_read(c, c->ea));
}

static void putvalue(context_t *c, uint16_t saveval) {
    if (opcodes[c->opcode].addr_mode == acc)
        c->a = (uint8_t)(saveval & 0x00FF);
    else
        mem_write(c, c->ea, (saveval & 0x00FF));
}

uint8_t add8(context_t *c, uint16_t a, uint16_t b, bool carry) {
    uint16_t result = a + b + (uint16_t)(carry ? 1 : 0);

    carrycalc(c, result);
    zerocalc(c, result);
    overflowcalc(c, result, a, b);
    signcalc(c, result);

#ifdef DECIMALMODE
    if (c->flags & FLAG_DECIMAL) {
        clearcarry(c);

        if ((result & 0x0F) > 0x09) {
            result += 0x06;
        }
        if ((result & 0xF0) > 0x90) {
            result += 0x60;
            setcarry(c);
        }

        c->clockticks++;
    }
#endif
    return result;
}

uint8_t rotate_right(context_t *c, uint16_t value) {
    uint16_t result = (value >> 1) | ((c->flags & FLAG_CARRY) << 7);

    if (value & 1)
        setcarry(c);
    else
        clearcarry(c);
    zerocalc(c, result);
    signcalc(c, result);

    return result;
}

uint8_t rotate_left(context_t *c, uint16_t value) {
    uint16_t result = (value << 1) | (c->flags & FLAG_CARRY);

    carrycalc(c, result);
    zerocalc(c, result);
    signcalc(c, result);

    return result;
}

uint8_t logical_shift_right(context_t *c, uint8_t value) {
    uint16_t result = value >> 1;
    if (value & 1)
        setcarry(c);
    else
        clearcarry(c);
    zerocalc(c, result);
    signcalc(c, result);

    return result;
}

uint8_t arithmetic_shift_left(context_t *c, uint8_t value) {
    uint16_t result = value << 1;

    carrycalc(c, result);
    zerocalc(c, result);
    signcalc(c, result);
    return result;
}

uint8_t exclusive_or(context_t *c, uint8_t a, uint8_t b) {
    uint16_t result = a ^ b;

    zerocalc(c, result);
    signcalc(c, result);

    return result;
}

uint8_t boolean_and(context_t *c, uint8_t a, uint8_t b) {
    uint16_t result = (uint16_t)a & b;

    zerocalc(c, result);
    signcalc(c, result);
    return result;
}

// instruction handler functions
void adc(context_t *c) {
    uint16_t value = getvalue(c);
    saveaccum(c, add8(c, c->a, value, c->flags & FLAG_CARRY));
}

void and (context_t * c) {
    uint8_t m = getvalue(c);
    saveaccum(c, boolean_and(c, c->a, m));
}

void asl(context_t *c) { putvalue(c, arithmetic_shift_left(c, getvalue(c))); }

void bra(context_t *c) {
    uint16_t oldpc = c->pc;
    c->pc = c->ea;
    if ((oldpc & 0xFF00) != (c->pc & 0xFF00))
        c->clockticks += 2; // check if jump crossed a page boundary
    else
        c->clockticks++;
}

void bcc(context_t *c) {
    if ((c->flags & FLAG_CARRY) == 0)
        bra(c);
}

void bcs(context_t *c) {
    if ((c->flags & FLAG_CARRY) == FLAG_CARRY)
        bra(c);
}

void beq(context_t *c) {
    if ((c->flags & FLAG_ZERO) == FLAG_ZERO)
        bra(c);
}

void bit(context_t *c) {
    uint8_t value = getvalue(c);
    uint8_t result = (uint16_t)c->a & value;

    zerocalc(c, result);
    c->flags = (c->flags & 0x3F) | (uint8_t)(value & 0xC0);
}

void bit_imm(context_t *c) {
    uint8_t value = getvalue(c);
    uint8_t result = (uint16_t)c->a & value;

    zerocalc(c, result);
}

void bmi(context_t *c) {
    if ((c->flags & FLAG_SIGN) == FLAG_SIGN)
        bra(c);
}

void bne(context_t *c) {
    if ((c->flags & FLAG_ZERO) == 0)
        bra(c);
}

void bpl(context_t *c) {
    if ((c->flags & FLAG_SIGN) == 0)
        bra(c);
}

void brk(context_t *c) {
    c->pc++;
    push6502_16(c, c->pc);                // push next instruction address onto stack
    push6502_8(c, c->flags | FLAG_BREAK); // push CPU flags to stack
    setinterrupt(c);                 // set interrupt flag
    c->pc = mem_read16(c, 0xfffe);
}

void bvc(context_t *c) {
    if ((c->flags & FLAG_OVERFLOW) == 0)
        bra(c);
}

void bvs(context_t *c) {
    if ((c->flags & FLAG_OVERFLOW) == FLAG_OVERFLOW)
        bra(c);
}

void clc(context_t *c) { clearcarry(c); }

void cld(context_t *c) { cleardecimal(c); }

void cli(context_t *c) { clearinterrupt(c); }

void clv(context_t *c) { clearoverflow(c); }

static void compare(context_t *c, uint16_t r) {
    uint16_t value = getvalue(c);
    uint16_t result = r - value;

    if (r >= (uint8_t)(value & 0x00FF))
        setcarry(c);
    else
        clearcarry(c);
    if (r == (uint8_t)(value & 0x00FF))
        setzero(c);
    else
        clearzero(c);
    signcalc(c, result);
}

void cmp(context_t *c) { compare(c, c->a); }

void cpx(context_t *c) { compare(c, c->x); }

void cpy(context_t *c) { compare(c, c->y); }

uint8_t decrement(context_t *c, uint8_t r) {
    uint16_t result = r - 1;
    zerocalc(c, result);
    signcalc(c, result);
    return result;
}

void dec(context_t *c) { putvalue(c, decrement(c, getvalue(c))); }

void dex(context_t *c) { c->x = decrement(c, c->x); }

void dey(context_t *c) { c->y = decrement(c, c->y); }

void eor(context_t *c) { saveaccum(c, exclusive_or(c, c->a, getvalue(c))); }

uint8_t increment(context_t *c, uint8_t r) {
    uint16_t result = r + 1;
    zerocalc(c, result);
    signcalc(c, result);
    return result;
}

void inc(context_t *c) { putvalue(c, increment(c, getvalue(c))); }

void inx(context_t *c) { c->x = increment(c, c->x); }

void iny(context_t *c) { c->y = increment(c, c->y); }

void jmp(context_t *c) { c->pc = c->ea; }

void jsr(context_t *c) {
    push6502_16(c, c->pc - 1);
    c->pc = c->ea;
}

void lda(context_t *c) {
    uint16_t value = getvalue(c);
    c->a = (uint8_t)(value & 0x00FF);

    zerocalc(c, c->a);
    signcalc(c, c->a);
}

void ldx(context_t *c) {
    uint16_t value = getvalue(c);
    c->x = (uint8_t)(value & 0x00FF);

    zerocalc(c, c->x);
    signcalc(c, c->x);
}

void ldy(context_t *c) {
    uint16_t value = getvalue(c);
    c->y = (uint8_t)(value & 0x00FF);

    zerocalc(c, c->y);
    signcalc(c, c->y);
}

void lsr(context_t *c) { putvalue(c, logical_shift_right(c, getvalue(c))); }

void nop(context_t *c) {}

void ora(context_t *c) {
    uint16_t value = getvalue(c);
    uint16_t result = (uint16_t)c->a | value;

    zerocalc(c, result);
    signcalc(c, result);

    saveaccum(c, result);
}

void pha(context_t *c) { push6502_8(c, c->a); }

void phx(context_t *c) { push6502_8(c, c->x); }

void phy(context_t *c) { push6502_8(c, c->y); }

void php(context_t *c) { push6502_8(c, c->flags | FLAG_BREAK); }

void pla(context_t *c) {
    c->a = pull6502_8(c);

    zerocalc(c, c->a);
    signcalc(c, c->a);
}

void plx(context_t *c) {
    c->x = pull6502_8(c);

    zerocalc(c, c->x);
    signcalc(c, c->x);
}

void ply(context_t *c) {
    c->y = pull6502_8(c);

    zerocalc(c, c->y);
    signcalc(c, c->y);
}

void plp(context_t *c) { c->flags = pull6502_8(c) | FLAG_CONSTANT | FLAG_BREAK; }

void rol(context_t *c) {
    uint16_t value = getvalue(c);

    putvalue(c, value);
    putvalue(c, rotate_left(c, value));
}

void ror(context_t *c) {
    uint16_t value = getvalue(c);

    putvalue(c, value);
    putvalue(c, rotate_right(c, value));
}

void rti(context_t *c) {
    c->flags = pull6502_8(c) | FLAG_CONSTANT | FLAG_BREAK;
    c->pc = pull6502_16(c);
}

void rts(context_t *c) { c->pc = pull6502_16(c) + 1; }

void sbc(context_t *c) {
    uint16_t value = getvalue(c);
    uint16_t result =
        (uint16_t)c->a - value - ((c->flags & FLAG_CARRY) ? 0 : 1);

    carrycalc(c, result);
    zerocalc(c, result);
    overflowcalc(c, result, c->a, value);
    signcalc(c, result);

#ifndef NES_CPU
    if (c->flags & FLAG_DECIMAL) {
        clearcarry(c);

        if ((result & 0x0F) > 0x09) {
            result -= 0x06;
        }
        if ((result & 0xF0) > 0x90) {
            result -= 0x60;
            setcarry(c);
        }

        c->clockticks++;
    }
#endif

    saveaccum(c, result);
}

void sec(context_t *c) { setcarry(c); }

void sed(context_t *c) { setdecimal(c); }

void sei(context_t *c) { setinterrupt(c); }

void sta(context_t *c) { putvalue(c, c->a); }

void stx(context_t *c) { putvalue(c, c->x); }

void sty(context_t *c) { putvalue(c, c->y); }

void stz(context_t *c) { putvalue(c, 0); }

void tax(context_t *c) {
    c->x = c->a;

    zerocalc(c, c->x);
    signcalc(c, c->x);
}

void tay(context_t *c) {
    c->y = c->a;

    zerocalc(c, c->y);
    signcalc(c, c->y);
}

void tsx(context_t *c) {
    c->x = c->s;

    zerocalc(c, c->x);
    signcalc(c, c->x);
}

void trb(context_t *c) {
    uint16_t value = getvalue(c);
    uint16_t result = (uint16_t)c->a & ~value;
    putvalue(c, result);
    zerocalc(c, (c->a | result) & 0x00ff);
}

void tsb(context_t *c) {
    uint16_t value = getvalue(c);
    uint16_t result = (uint16_t)c->a | value;
    putvalue(c, result);
    zerocalc(c, (c->a | result) & 0x00ff);
}

void txa(context_t *c) {
    c->a = c->x;

    zerocalc(c, c->a);
    signcalc(c, c->a);
}

void txs(context_t *c) { c->s = c->x; }

void tya(context_t *c) {
    c->a = c->y;

    zerocalc(c, c->a);
    signcalc(c, c->a);
}

void lax(context_t *c) {
    uint16_t value = getvalue(c);
    c->x = c->a = (uint8_t)(value & 0x00FF);

    zerocalc(c, c->a);
    signcalc(c, c->a);
}

void sax(context_t *c) { putvalue(c, c->a & c->x); }

void dcp(context_t *c) {
    dec(c);
    cmp(c);
}

void isb(context_t *c) {
    inc(c);
    sbc(c);
}

void slo(context_t *c) {
    asl(c);
    ora(c);
}

void rla(context_t *c) {
    uint16_t value = getvalue(c);
    uint16_t result = rotate_left(c, value);
    putvalue(c, value);
    putvalue(c, result);
    saveaccum(c, boolean_and(c, c->a, result));
}

void sre(context_t *c) {
    uint16_t value = getvalue(c);
    uint16_t result = logical_shift_right(c, value);
    putvalue(c, value);
    putvalue(c, result);
    saveaccum(c, exclusive_or(c, c->a, result));
}

void rra(context_t *c) {
    uint16_t value = getvalue(c);
    uint16_t result = rotate_right(c, value);
    putvalue(c, value);
    putvalue(c, result);
    saveaccum(c, add8(c, c->a, result, c->flags & FLAG_CARRY));
}

#ifdef NMOS6502
static opcode_t opcodes[256] = {
    /* 00 */
    {imp, brk, 7},
    {indx, ora, 6},
    {imp, nop, 2},
    {indx, slo, 8},
    {zp, nop, 3},
    {zp, ora, 3},
    {zp, asl, 5},
    {zp, slo, 5},
    {imp, php, 3},
    {imm, ora, 2},
    {acc, asl, 2},
    {imm, nop, 2},
    {abso, nop, 4},
    {abso, ora, 4},
    {abso, asl, 6},
    {abso, slo, 6},
    /* 01 */
    {rel, bpl, 2},
    {indy_p, ora, 5},
    {imp, nop, 2},
    {indy, slo, 8},
    {zpx, nop, 4},
    {zpx, ora, 4},
    {zpx, asl, 6},
    {zpx, slo, 6},
    {imp, clc, 2},
    {absy_p, ora, 4},
    {imp, nop, 2},
    {absy, slo, 7},
    {absx, nop, 4},
    {absx_p, ora, 4},
    {absx, asl, 7},
    {absx, slo, 7},
    /* 02 */
    {abso, jsr, 6},
    {indx, and, 6},
    {imp, nop, 2},
    {indx, rla, 8},
    {zp, bit, 3},
    {zp, and, 3},
    {zp, rol, 5},
    {zp, rla, 5},
    {imp, plp, 4},
    {imm, and, 2},
    {acc, rol, 2},
    {imm, nop, 2},
    {abso, bit, 4},
    {abso, and, 4},
    {abso, rol, 6},
    {abso, rla, 6},
    /* 30 */
    {rel, bmi, 2},
    {indy_p, and, 5},
    {imp, nop, 2},
    {indy, rla, 8},
    {zpx, nop, 4},
    {zpx, and, 4},
    {zpx, rol, 6},
    {zpx, rla, 6},
    {imp, sec, 2},
    {absy_p, and, 4},
    {imp, nop, 2},
    {absy, rla, 7},
    {absx, nop, 4},
    {absx_p, and, 4},
    {absx, rol, 7},
    {absx, rla, 7},
    /* 40 */
    {imp, rti, 6},
    {indx, eor, 6},
    {imp, nop, 2},
    {indx, sre, 8},
    {zp, nop, 3},
    {zp, eor, 3},
    {zp, lsr, 5},
    {zp, sre, 5},
    {imp, pha, 3},
    {imm, eor, 2},
    {acc, lsr, 2},
    {imm, nop, 2},
    {abso, jmp, 3},
    {abso, eor, 4},
    {abso, lsr, 6},
    {abso, sre, 6},
    /* 50 */
    {rel, bvc, 2},
    {indy_p, eor, 5},
    {imp, nop, 2},
    {indy, sre, 8},
    {zpx, nop, 4},
    {zpx, eor, 4},
    {zpx, lsr, 6},
    {zpx, sre, 6},
    {imp, cli, 2},
    {absy_p, eor, 4},
    {imp, nop, 2},
    {absy, sre, 7},
    {absx, nop, 4},
    {absx_p, eor, 4},
    {absx, lsr, 7},
    {absx, sre, 7},
    /* 60 */
    {imp, rts, 6},
    {indx, adc, 6},
    {imp, nop, 2},
    {indx, rra, 8},
    {zp, nop, 3},
    {zp, adc, 3},
    {zp, ror, 5},
    {zp, rra, 5},
    {imp, pla, 4},
    {imm, adc, 2},
    {acc, ror, 2},
    {imm, nop, 2},
    {ind, jmp, 5},
    {abso, adc, 4},
    {abso, ror, 6},
    {abso, rra, 6},
    /* 70 */
    {rel, bvs, 2},
    {indy_p, adc, 5},
    {imp, nop, 2},
    {indy, rra, 8},
    {zpx, nop, 4},
    {zpx, adc, 4},
    {zpx, ror, 6},
    {zpx, rra, 6},
    {imp, sei, 2},
    {absy_p, adc, 4},
    {imp, nop, 2},
    {absy, rra, 7},
    {absx, nop, 4},
    {absx_p, adc, 4},
    {absx, ror, 7},
    {absx, rra, 7},
    /* 80*/
    {imm, nop, 2},
    {indx, sta, 6},
    {imm, nop, 2},
    {indx, sax, 6},
    {zp, sty, 3},
    {zp, sta, 3},
    {zp, stx, 3},
    {zp, sax, 3},
    {imp, dey, 2},
    {imm, nop, 2},
    {imp, txa, 2},
    {imm, nop, 2},
    {abso, sty, 4},
    {abso, sta, 4},
    {abso, stx, 4},
    {abso, sax, 4},
    /*90*/
    {rel, bcc, 2},
    {indy, sta, 6},
    {imp, nop, 2},
    {indy, nop, 6},
    {zpx, sty, 4},
    {zpx, sta, 4},
    {zpy, stx, 4},
    {zpy, sax, 4},
    {imp, tya, 2},
    {absy, sta, 5},
    {imp, txs, 2},
    {absy, nop, 5},
    {absx, nop, 5},
    {absx, sta, 5},
    {absy, nop, 5},
    {absy, nop, 5},
    /* A0 */
    {imm, ldy, 2},
    {indx, lda, 6},
    {imm, ldx, 2},
    {indx, lax, 6},
    {zp, ldy, 3},
    {zp, lda, 3},
    {zp, ldx, 3},
    {zp, lax, 3},
    {imp, tay, 2},
    {imm, lda, 2},
    {imp, tax, 2},
    {imm, nop, 2},
    {abso, ldy, 4},
    {abso, lda, 4},
    {abso, ldx, 4},
    {abso, lax, 4},
    /* B0 */
    {rel, bcs, 2},
    {indy_p, lda, 5},
    {imp, nop, 2},
    {indy_p, lax, 5},
    {zpx, ldy, 4},
    {zpx, lda, 4},
    {zpy, ldx, 4},
    {zpy, lax, 4},
    {imp, clv, 2},
    {absy_p, lda, 4},
    {imp, tsx, 2},
    {absy_p, lax, 4},
    {absx_p, ldy, 4},
    {absx_p, lda, 4},
    {absy_p, ldx, 4},
    {absy_p, lax, 4},
    /* C0 */
    {imm, cpy, 2},
    {indx, cmp, 6},
    {imm, nop, 2},
    {indx, dcp, 8},
    {zp, cpy, 3},
    {zp, cmp, 3},
    {zp, dec, 5},
    {zp, dcp, 5},
    {imp, iny, 2},
    {imm, cmp, 2},
    {imp, dex, 2},
    {imm, nop, 2},
    {abso, cpy, 4},
    {abso, cmp, 4},
    {abso, dec, 6},
    {abso, dcp, 6},
    /* D0 */
    {rel, bne, 2},
    {indy_p, cmp, 5},
    {imp, nop, 2},
    {indy, dcp, 8},
    {zpx, nop, 4},
    {zpx, cmp, 4},
    {zpx, dec, 6},
    {zpx, dcp, 6},
    {imp, cld, 2},
    {absy_p, cmp, 4},
    {imp, nop, 2},
    {absy, dcp, 7},
    {absx, nop, 4},
    {absx_p, cmp, 4},
    {absx, dec, 7},
    {absx, dcp, 7},
    /* E0 */
    {imm, cpx, 2},
    {indx, sbc, 6},
    {imm, nop, 2},
    {indx, isb, 8},
    {zp, cpx, 3},
    {zp, sbc, 3},
    {zp, inc, 5},
    {zp, isb, 5},
    {imp, inx, 2},
    {imm, sbc, 2},
    {imp, nop, 2},
    {imm, sbc, 2},
    {abso, cpx, 4},
    {abso, sbc, 4},
    {abso, inc, 6},
    {abso, isb, 6},
    /* F0 */
    {rel, beq, 2},
    {indy_p, sbc, 5},
    {imp, nop, 2},
    {indy, isb, 8},
    {zpx, nop, 4},
    {zpx, sbc, 4},
    {zpx, inc, 6},
    {zpx, isb, 6},
    {imp, sed, 2},
    {absy_p, sbc, 4},
    {imp, nop, 2},
    {absy, isb, 7},
    {absx, nop, 4},
    {absx_p, sbc, 4},
    {absx, inc, 7},
    {absx, isb, 7}};
#endif

#ifdef CMOS6502
static opcode_t opcodes[256] = {
    /* 00 */
    {imp, brk, 7},
    {indx, ora, 6},
    {imp, nop, 2},
    {indx, slo, 8},
    {zp, tsb, 5},
    {zp, ora, 3},
    {zp, asl, 5},
    {zp, slo, 5},
    {imp, php, 3},
    {imm, ora, 2},
    {acc, asl, 2},
    {imm, nop, 2},
    {abso, tsb, 6},
    {abso, ora, 4},
    {abso, asl, 6},
    {abso, slo, 6},
    /* 01 */
    {rel, bpl, 2},
    {indy_p, ora, 5},
    {zpi, ora, 5},
    {indy, slo, 8},
    {zp, trb, 5},
    {zpx, ora, 4},
    {zpx, asl, 6},
    {zpx, slo, 6},
    {imp, clc, 2},
    {absy_p, ora, 4},
    {acc, inc, 2},
    {absy, slo, 7},
    {abso, trb, 6},
    {absx_p, ora, 4},
    {absx, asl, 7},
    {absx, slo, 7},
    /* 02 */
    {abso, jsr, 6},
    {indx, and, 6},
    {imp, nop, 2},
    {indx, rla, 8},
    {zp, bit, 3},
    {zp, and, 3},
    {zp, rol, 5},
    {zp, rla, 5},
    {imp, plp, 4},
    {imm, and, 2},
    {acc, rol, 2},
    {imm, nop, 2},
    {abso, bit, 4},
    {abso, and, 4},
    {abso, rol, 6},
    {abso, rla, 6},
    /* 30 */
    {rel, bmi, 2},
    {indy_p, and, 5},
    {zpi, adc, 5},
    {indy, rla, 8},
    {zpx, bit, 4},
    {zpx, and, 4},
    {zpx, rol, 6},
    {zpx, rla, 6},
    {imp, sec, 2},
    {absy_p, and, 4},
    {acc, dec, 2},
    {absy, rla, 7},
    {absx_p, bit, 4},
    {absx_p, and, 4},
    {absx, rol, 7},
    {absx, rla, 7},
    /* 40 */
    {imp, rti, 6},
    {indx, eor, 6},
    {imp, nop, 2},
    {indx, sre, 8},
    {zp, nop, 3},
    {zp, eor, 3},
    {zp, lsr, 5},
    {zp, sre, 5},
    {imp, pha, 3},
    {imm, eor, 2},
    {acc, lsr, 2},
    {imm, nop, 2},
    {abso, jmp, 3},
    {abso, eor, 4},
    {abso, lsr, 6},
    {abso, sre, 6},
    /* 50 */
    {rel, bvc, 2},
    {indy_p, eor, 5},
    {zpi, eor, 5},
    {indy, sre, 8},
    {zpx, nop, 4},
    {zpx, eor, 4},
    {zpx, lsr, 6},
    {zpx, sre, 6},
    {imp, cli, 2},
    {absy_p, eor, 4},
    {imp, phy, 2},
    {absy, sre, 7},
    {absx, nop, 4},
    {absx_p, eor, 4},
    {absx, lsr, 7},
    {absx, sre, 7},
    /* 60 */
    {imp, rts, 6},
    {indx, adc, 6},
    {imp, nop, 2},
    {indx, rra, 8},
    {zp, stz, 3},
    {zp, adc, 3},
    {zp, ror, 5},
    {zp, rra, 5},
    {imp, pla, 4},
    {imm, adc, 2},
    {acc, ror, 2},
    {imm, nop, 2},
    {ind, jmp, 5},
    {abso, adc, 4},
    {abso, ror, 6},
    {abso, rra, 6},
    /* 70 */
    {rel, bvs, 2},
    {indy_p, adc, 5},
    {zpi, adc, 5},
    {indy, rra, 8},
    {zpx, stz, 4},
    {zpx, adc, 4},
    {zpx, ror, 6},
    {zpx, rra, 6},
    {imp, sei, 2},
    {absy_p, adc, 4},
    {imp, ply, 6},
    {absy, rra, 7},
    {absxi, jmp, 6},
    {absx_p, adc, 4},
    {absx, ror, 7},
    {absx, rra, 7},
    /* 80 */
    {rel, bra, 3},
    {indx, sta, 6},
    {imm, nop, 2},
    {indx, sax, 6},
    {zp, sty, 3},
    {zp, sta, 3},
    {zp, stx, 3},
    {zp, sax, 3},
    {imp, dey, 2},
    {imm, bit_imm, 2},
    {imp, txa, 2},
    {imm, nop, 2},
    {abso, sty, 4},
    {abso, sta, 4},
    {abso, stx, 4},
    {abso, sax, 4},
    /* 90 */
    {rel, bcc, 2},
    {indy, sta, 6},
    {zpi, sta, 5},
    {indy, nop, 6},
    {zpx, sty, 4},
    {zpx, sta, 4},
    {zpy, stx, 4},
    {zpy, sax, 4},
    {imp, tya, 2},
    {absy, sta, 5},
    {imp, txs, 2},
    {absy, nop, 5},
    {abso, stz, 4},
    {absx, sta, 5},
    {absx, stz, 5},
    {absy, nop, 5},
    /* A0 */
    {imm, ldy, 2},
    {indx, lda, 6},
    {imm, ldx, 2},
    {indx, lax, 6},
    {zp, ldy, 3},
    {zp, lda, 3},
    {zp, ldx, 3},
    {zp, lax, 3},
    {imp, tay, 2},
    {imm, lda, 2},
    {imp, tax, 2},
    {imm, nop, 2},
    {abso, ldy, 4},
    {abso, lda, 4},
    {abso, ldx, 4},
    {abso, lax, 4},
    /* B0 */
    {rel, bcs, 2},
    {indy_p, lda, 5},
    {zpi, lda, 5},
    {indy_p, lax, 5},
    {zpx, ldy, 4},
    {zpx, lda, 4},
    {zpy, ldx, 4},
    {zpy, lax, 4},
    {imp, clv, 2},
    {absy_p, lda, 4},
    {imp, tsx, 2},
    {absy_p, lax, 4},
    {absx_p, ldy, 4},
    {absx_p, lda, 4},
    {absy_p, ldx, 4},
    {absy_p, lax, 4},
    /* C0 */
    {imm, cpy, 2},
    {indx, cmp, 6},
    {imm, nop, 2},
    {indx, dcp, 8},
    {zp, cpy, 3},
    {zp, cmp, 3},
    {zp, dec, 5},
    {zp, dcp, 5},
    {imp, iny, 2},
    {imm, cmp, 2},
    {imp, dex, 2},
    {imm, nop, 2},
    {abso, cpy, 4},
    {abso, cmp, 4},
    {abso, dec, 6},
    {abso, dcp, 6},
    /* D0 */
    {rel, bne, 2},
    {indy_p, cmp, 5},
    {zpi, cmp, 5},
    {indy, dcp, 8},
    {zpx, nop, 4},
    {zpx, cmp, 4},
    {zpx, dec, 6},
    {zpx, dcp, 6},
    {imp, cld, 2},
    {absy_p, cmp, 4},
    {imp, phx, 3},
    {absy, dcp, 7},
    {absx, nop, 4},
    {absx_p, cmp, 4},
    {absx, dec, 7},
    {absx, dcp, 7},
    /* E0 */
    {imm, cpx, 2},
    {indx, sbc, 6},
    {imm, nop, 2},
    {indx, isb, 8},
    {zp, cpx, 3},
    {zp, sbc, 3},
    {zp, inc, 5},
    {zp, isb, 5},
    {imp, inx, 2},
    {imm, sbc, 2},
    {imp, nop, 2},
    {imm, sbc, 2},
    {abso, cpx, 4},
    {abso, sbc, 4},
    {abso, inc, 6},
    {abso, isb, 6},
    /* F0 */
    {rel, beq, 2},
    {indy_p, sbc, 5},
    {zpi, sbc, 5},
    {indy, isb, 8},
    {zpx, nop, 4},
    {zpx, sbc, 4},
    {zpx, inc, 6},
    {zpx, isb, 6},
    {imp, sed, 2},
    {absy_p, sbc, 4},
    {imp, plx, 2},
    {absy, isb, 7},
    {absx, nop, 4},
    {absx_p, sbc, 4},
    {absx, inc, 7},
    {absx, isb, 7}};
#endif

void nmi6502(context_t *c) {
    push6502_16(c, c->pc);
    push6502_8(c, c->flags & ~FLAG_BREAK);
    c->flags |= FLAG_INTERRUPT;
    c->pc = mem_read16(c, 0xfffa);
}

void irq6502(context_t *c) {
    if ((c->flags & FLAG_INTERRUPT) == 0) {
        push6502_16(c, c->pc);
        push6502_8(c, c->flags & ~FLAG_BREAK);
        c->flags |= FLAG_INTERRUPT;
        c->pc = mem_read16(c, 0xfffe);
    }
}

void step(context_t *c) {
    uint8_t opcode = mem_read(c, c->pc++);
    c->opcode = opcode;
    c->flags |= FLAG_CONSTANT;

    opcodes[opcode].addr_mode(c);
    opcodes[opcode].opcode(c);
    c->clockticks += opcodes[opcode].clockticks;
}
