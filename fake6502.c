
/*!
\file
\anchor file_fake6502_c

\section f6502_about About

Fake6502 is a 6502 emulator writen in ANSI C.

It was originally created by:

(c) 2011 Mike Chambers (miker00lz@gmail.com) <br/>
Fake6502 CPU emulator core


In 2020, this fork was created:

<a href='https://github.com/omarandlorraine/fake6502'>https://github.com/omarandlorraine/fake6502</a>


See: <a href='./CHANGELOG.md'>CHANGELOG.md</a>

- - -

\section f6502_version Version

v2.4.0 - 19-07-2022

- - -

\section f6502_license License

<a href='./LICENSE'>GNU General Public License v2.0</a>

- - -

\section f6502_building Building this emulator

Fake6502 requires you to provide two external functions:

uint8_t fake6502_mem_read(uint16_t address)

void fake6502_mem_write(uint16_t address, uint8_t value)


There are a couple of compile-time options:

 - NES_CPU

when this is defined, the binary-coded decimal (BCD) status flag is not
honored by ADC and SBC. The 2A03 CPU in the Nintendo Entertainment System
does not support BCD operation.

See:
<a href='https://www.nesdev.com/2A03%20technical%20reference.txt'>2A03 technical reference</a>


 - NMOS6502 or CMOS6502

define one or other of these. This will configure the emulator to emulate
either the NMOS or CMOS variants (CMOS adds bugfixes and several
instructions)


- DECIMALMODE

when this is defined, BCD mode is implemented (in adc and rra).


- FAKE6502_OPS_STATIC

when this is defined, al the addressing and opcode functions are declared
as 'static'. This can be used to avoid any global name conflicts.
You can still access/call any opcode function, via the opcode table:

    fake6502_opcodes[opcode].addr_mode(c);
    fake6502_opcodes[opcode].opcode(c);

- - -

\section f6502_usage Using this emulator

There are only a few functions you need to call, to use this emulator.

\code{.unparsed}
void fake6502_reset()
\endcode

Call this once before you begin execution, to initialise the code.

\code{.unparsed}
void fake6502_step()
\endcode

Execute the next (single) instrution.

\code{.unparsed}
void fake6502_irq()
\endcode

Trigger an IRQ in the 6502 core.

\code{.unparsed}
void fake6502_nmi()
\endcode

Trigger an NMI in the 6502 core.

- - -

\section f6502_design Design of this emulator

The execution of a 6502 instruction is split into 2 parts/tasks :-

  - the addressing mode and resulting address to be used

  - the opperation to be performed

Each instance of these tasks are implemented in seperate functions,
and accessed via a table of function pointers,
indexed by the 6502 instruction opcode to be executed.

The memory accessing of the 6502 core (for all instructions
and data) is provided by the host code, via the functions
fake6502_mem_read() and fake6502_mem_write().

It is up to the host code to map the address provided,
into it's own 64K memory space. The host code has the use of
`void *state_host` in the `fake6502_context` struct, to pass its
own data structure through to the memory accessing functions.

The fn()'s: `fake6502_reset()`, `fake6502_irq()` and `fake6502_nmi()`,
all use the 6502 defined vector addresses at the top of memory
(0xfffa/b, 0xffc/d and 0xffe/f),
to setup the PC for execution,
ie. at the next call to fake6502_step().

- - -

*/


// -------------------------------------------------------------------
// include's
// -------------------------------------------------------------------

#include "fake6502.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>


// -------------------------------------------------------------------
// option's
// -------------------------------------------------------------------

// check the build options

#if defined(NES_CPU) && !defined(NMOS6502)
#error can not have NES_CPU without NMOS6502
#endif


#if defined(NES_CPU) && defined(DECIMALMODE)
#error can not have NES_CPU and DECIMALMODE
#endif


#if !defined(NMOS6502) && !defined(CMOS6502)
#error one of NMOS6502 or CMOS6502 must be defined
#endif


#if defined(NMOS6502) && defined(CMOS6502)
#error can not have both NMOS6502 and CMOS6502 defined
#endif


#if defined(CMOS6502) && !defined(DECIMALMODE)
#error can not have CMOS6502 without DECIMALMODE
#endif


// -------------------------------------------------------------------
// global's
// -------------------------------------------------------------------



// -------------------------------------------------------------------
// function's
// -------------------------------------------------------------------

// a few general functions used by various other functions

void fake6502_push_8(fake6502_context *c, uint8_t pushval)
{
    fake6502_mem_write(c, FAKE6502_STACK_BASE + c->cpu.s--, pushval);
}

void fake6502_push_16(fake6502_context *c, uint16_t pushval)
{
    fake6502_push_8(c, (pushval >> 8) & 0xFF);
    fake6502_push_8(c, pushval & 0xFF);
}

uint8_t fake6502_pull_8(fake6502_context *c)
{ return(fake6502_mem_read(c, FAKE6502_STACK_BASE + ++c->cpu.s)); }

uint16_t fake6502_pull_16(fake6502_context *c)
{
    uint8_t t;
    t = fake6502_pull_8(c);
    return(fake6502_pull_8(c) << 8 | t);
}

uint16_t fake6502_mem_read16(fake6502_context *c, uint16_t addr)
{
    // Read two consecutive bytes from memory
    return((uint16_t)fake6502_mem_read(c, addr) |
           ((uint16_t)fake6502_mem_read(c, addr + 1) << 8));
}


// -------------------------------------------------------------------

// supporting addressing mode functions,
// calculates effective addresses (ea)

FAKE6502_FN_ADDR_MODE(imp)
{ // implied
}

FAKE6502_FN_ADDR_MODE(acc)
{ // accumulator
}

FAKE6502_FN_ADDR_MODE(imm)
{ // immediate
    c->emu.ea = c->cpu.pc++;
}

FAKE6502_FN_ADDR_MODE(zp)
{ // zero-page
    c->emu.ea = (uint16_t)fake6502_mem_read(c, (uint16_t)c->cpu.pc++);
}

FAKE6502_FN_ADDR_MODE(zpx)
{ // zero-page,X
    // do zero-page wraparound
    c->emu.ea = ((uint16_t)fake6502_mem_read(c, (uint16_t)c->cpu.pc++) + (uint16_t)c->cpu.x) &
            0xFF;
}

FAKE6502_FN_ADDR_MODE(zpy)
{ // zero-page,Y
    // do zero-page wraparound
    c->emu.ea = ((uint16_t)fake6502_mem_read(c, (uint16_t)c->cpu.pc++) + (uint16_t)c->cpu.y) &
            0xFF;
}

FAKE6502_FN_ADDR_MODE(rel)
{ // relative for branch ops (8-bit immediate value, sign-extended)
    uint16_t rel = (uint16_t)fake6502_mem_read(c, c->cpu.pc++);
    if (rel & 0x80)
        rel |= 0xFF00;
    c->emu.ea = c->cpu.pc + rel;
}

FAKE6502_FN_ADDR_MODE(abso)
{ // absolute
    c->emu.ea = fake6502_mem_read16(c, c->cpu.pc);
    c->cpu.pc += 2;
}

FAKE6502_FN_ADDR_MODE(absx)
{ // absolute,X
    c->emu.ea = fake6502_mem_read16(c, c->cpu.pc);
    c->emu.ea += (uint16_t)c->cpu.x;

    c->cpu.pc += 2;
}

FAKE6502_FN_ADDR_MODE(absx_p)
{ // absolute,X with cycle penalty
    uint16_t startpage;
    c->emu.ea = fake6502_mem_read16(c, c->cpu.pc);
    startpage = c->emu.ea & 0xFF00;
    c->emu.ea += (uint16_t)c->cpu.x;
    if (startpage != (c->emu.ea & 0xff00))
        c->emu.clockticks++;

    c->cpu.pc += 2;
}

FAKE6502_FN_ADDR_MODE(absxi)
{ // (absolute,X)
    c->emu.ea = fake6502_mem_read16(c, c->cpu.pc);
    c->emu.ea += (uint16_t)c->cpu.x;
    c->emu.ea = fake6502_mem_read16(c, c->emu.ea);

    c->cpu.pc += 2;
}

FAKE6502_FN_ADDR_MODE(absy)
{ // absolute,Y
    c->emu.ea = fake6502_mem_read16(c, c->cpu.pc);
    c->emu.ea += (uint16_t)c->cpu.y;

    c->cpu.pc += 2;
}

FAKE6502_FN_ADDR_MODE(absy_p)
{ // absolute,Y
    uint16_t startpage;
    c->emu.ea = fake6502_mem_read16(c, c->cpu.pc);
    startpage = c->emu.ea & 0xFF00;
    c->emu.ea += (uint16_t)c->cpu.y;
    if (startpage != (c->emu.ea & 0xff00))
        c->emu.clockticks++;

    c->cpu.pc += 2;
}

#ifdef NMOS6502
FAKE6502_FN_ADDR_MODE(ind)
{ // indirect
    uint16_t eahelp, eahelp2;
    eahelp = fake6502_mem_read16(c, c->cpu.pc);

    // replicate 6502 page-boundary wraparound bug
    eahelp2 =
        (eahelp & 0xFF00) | ((eahelp + 1) & 0x00FF);
    c->emu.ea =
        (uint16_t)fake6502_mem_read(c, eahelp) | ((uint16_t)fake6502_mem_read(c, eahelp2) << 8);
    c->cpu.pc += 2;
}
#endif

#ifdef CMOS6502
FAKE6502_FN_ADDR_MODE(ind)
{ // indirect
    uint16_t eahelp;
    eahelp = fake6502_mem_read16(c, c->cpu.pc);
    if ((eahelp & 0x00ff) == 0xff)
        c->emu.clockticks++;
    c->emu.ea = fake6502_mem_read16(c, eahelp);
    c->cpu.pc += 2;
}
#endif

FAKE6502_FN_ADDR_MODE(indx)
{ // (indirect,X)
    uint16_t eahelp;

    // do zero-page wraparound, for table pointer
    eahelp = (uint16_t)(((uint16_t)fake6502_mem_read(c, c->cpu.pc++) + (uint16_t)c->cpu.x) &
                        0xFF);
    c->emu.ea = (uint16_t)fake6502_mem_read(c, eahelp & 0x00FF) |
            ((uint16_t)fake6502_mem_read(c, (eahelp + 1) & 0x00FF) << 8);
}

FAKE6502_FN_ADDR_MODE(indy)
{ // (indirect),Y
    uint16_t eahelp, eahelp2;
    eahelp = (uint16_t)fake6502_mem_read(c, c->cpu.pc++);

    // do zero-page wraparound
    eahelp2 =
        (eahelp & 0xFF00) | ((eahelp + 1) & 0x00FF);
    c->emu.ea =
        (uint16_t)fake6502_mem_read(c, eahelp) | ((uint16_t)fake6502_mem_read(c, eahelp2) << 8);
    c->emu.ea += (uint16_t)c->cpu.y;
}

FAKE6502_FN_ADDR_MODE(indy_p)
{ // (indirect),Y
    uint16_t eahelp, eahelp2, startpage;
    eahelp = (uint16_t)fake6502_mem_read(c, c->cpu.pc++);

    // do zero-page wraparound
    eahelp2 =
        (eahelp & 0xFF00) | ((eahelp + 1) & 0x00FF);
    c->emu.ea =
        (uint16_t)fake6502_mem_read(c, eahelp) | ((uint16_t)fake6502_mem_read(c, eahelp2) << 8);
    startpage = c->emu.ea & 0xFF00;
    c->emu.ea += (uint16_t)c->cpu.y;
    if (startpage != (c->emu.ea & 0xff00))
        c->emu.clockticks++;
}

FAKE6502_FN_ADDR_MODE(zpi)
{ // (zp)
    uint16_t eahelp, eahelp2;
    eahelp = (uint16_t)fake6502_mem_read(c, c->cpu.pc++);

    // do zero-page wraparound
    eahelp2 =
        (eahelp & 0xFF00) | ((eahelp + 1) & 0x00FF);
    c->emu.ea =
        (uint16_t)fake6502_mem_read(c, eahelp) | ((uint16_t)fake6502_mem_read(c, eahelp2) << 8);
}


// -------------------------------------------------------------------

// supporting instruction handler functions

uint16_t fake6502_get_value(fake6502_context *c)
{
    if (fake6502_opcodes[c->emu.opcode].addr_mode == acc)
        return((uint16_t)c->cpu.a);
    else
        return((uint16_t)fake6502_mem_read(c, c->emu.ea));
}

void fake6502_put_value(fake6502_context *c, uint16_t saveval)
{
    if (fake6502_opcodes[c->emu.opcode].addr_mode == acc)
        c->cpu.a = (uint8_t)(saveval & 0x00FF);
    else
        fake6502_mem_write(c, c->emu.ea, (saveval & 0x00FF));
}

uint8_t add8(fake6502_context *c, uint16_t a, uint16_t b, bool carry)
{
    uint16_t result = a + b + (uint16_t)(carry ? 1 : 0);

    fake6502_zero_calc(c, result);
    fake6502_overflow_calc(c, result, a, b);
    fake6502_sign_calc(c, result);

    #ifdef DECIMALMODE
    // Apply decimal mode fix from http://forum.6502.org/viewtopic.php?p=37758#p37758
    if(c->cpu.flags & FAKE6502_DECIMAL_FLAG)
        result += ((((result + 0x66) ^ (uint16_t)a ^ b) >> 3) & 0x22) * 3;
    #endif

    fake6502_carry_calc(c, result);

    return(result);

}

uint8_t rotate_right(fake6502_context *c, uint16_t value)
{
    uint16_t result = (value >> 1) | ((c->cpu.flags & FAKE6502_CARRY_FLAG) << 7);

    if (value & 1)
        fake6502_carry_set(c);
    else
        fake6502_carry_clear(c);
    fake6502_zero_calc(c, result);
    fake6502_sign_calc(c, result);

    return(result);
}

uint8_t rotate_left(fake6502_context *c, uint16_t value)
{
    uint16_t result = (value << 1) | (c->cpu.flags & FAKE6502_CARRY_FLAG);

    fake6502_carry_calc(c, result);
    fake6502_zero_calc(c, result);
    fake6502_sign_calc(c, result);

    return(result);
}

uint8_t logical_shift_right(fake6502_context *c, uint8_t value)
{
    uint16_t result = value >> 1;
    if (value & 1)
        fake6502_carry_set(c);
    else
        fake6502_carry_clear(c);
    fake6502_zero_calc(c, result);
    fake6502_sign_calc(c, result);

    return(result);
}

uint8_t arithmetic_shift_left(fake6502_context *c, uint8_t value)
{
    uint16_t result = value << 1;

    fake6502_carry_calc(c, result);
    fake6502_zero_calc(c, result);
    fake6502_sign_calc(c, result);
    return(result);
}

uint8_t exclusive_or(fake6502_context *c, uint8_t a, uint8_t b)
{
    uint16_t result = a ^ b;

    fake6502_zero_calc(c, result);
    fake6502_sign_calc(c, result);

    return(result);
}

uint8_t boolean_and(fake6502_context *c, uint8_t a, uint8_t b)
{
    uint16_t result = (uint16_t)a & b;

    fake6502_zero_calc(c, result);
    fake6502_sign_calc(c, result);
    return(result);
}

uint8_t increment(fake6502_context *c, uint8_t r)
{
    uint16_t result = r + 1;
    fake6502_zero_calc(c, result);
    fake6502_sign_calc(c, result);
    return(result);
}

uint8_t decrement(fake6502_context *c, uint8_t r)
{
    uint16_t result = r - 1;
    fake6502_zero_calc(c, result);
    fake6502_sign_calc(c, result);
    return(result);
}

void compare(fake6502_context *c, uint16_t r)
{
    uint16_t value = fake6502_get_value(c);
    uint16_t result = r - value;

    if (r >= (uint8_t)(value & 0x00FF))
        fake6502_carry_set(c);
    else
        fake6502_carry_clear(c);
    if (r == (uint8_t)(value & 0x00FF))
        fake6502_zero_set(c);
    else
        fake6502_zero_clear(c);
    fake6502_sign_calc(c, result);
}


// -------------------------------------------------------------------

// instruction handler functions

FAKE6502_FN_OPCODE(adc)
{
    uint16_t value = fake6502_get_value(c);
    fake6502_accum_save(c, add8(c, c->cpu.a, value, c->cpu.flags & FAKE6502_CARRY_FLAG));
}

FAKE6502_FN_OPCODE(and)
{
    uint8_t m = fake6502_get_value(c);
    fake6502_accum_save(c, boolean_and(c, c->cpu.a, m));
}

FAKE6502_FN_OPCODE(asl)
{ fake6502_put_value(c, arithmetic_shift_left(c, fake6502_get_value(c))); }

FAKE6502_FN_OPCODE(bra)
{
    uint16_t oldpc = c->cpu.pc;
    c->cpu.pc = c->emu.ea;

    // check if jump crossed a page boundary

    if ((oldpc & 0xFF00) != (c->cpu.pc & 0xFF00))
        c->emu.clockticks += 2;
    else
        c->emu.clockticks++;
}

FAKE6502_FN_OPCODE(bcc)
{
    if ((c->cpu.flags & FAKE6502_CARRY_FLAG) == 0)
        bra(c);
}

FAKE6502_FN_OPCODE(bcs)
{
    if ((c->cpu.flags & FAKE6502_CARRY_FLAG) == FAKE6502_CARRY_FLAG)
        bra(c);
}

FAKE6502_FN_OPCODE(beq)
{
    if ((c->cpu.flags & FAKE6502_ZERO_FLAG) == FAKE6502_ZERO_FLAG)
        bra(c);
}

FAKE6502_FN_OPCODE(bit)
{
    uint8_t value = fake6502_get_value(c);
    uint8_t result = (uint16_t)c->cpu.a & value;

    fake6502_zero_calc(c, result);
    c->cpu.flags = (c->cpu.flags & 0x3F) | (uint8_t)(value & 0xC0);
}

void bit_imm(fake6502_context *c)
{
    uint8_t value = fake6502_get_value(c);
    uint8_t result = (uint16_t)c->cpu.a & value;

    fake6502_zero_calc(c, result);
}

FAKE6502_FN_OPCODE(bmi)
{
    if ((c->cpu.flags & FAKE6502_SIGN_FLAG) == FAKE6502_SIGN_FLAG)
        bra(c);
}

FAKE6502_FN_OPCODE(bne)
{
    if ((c->cpu.flags & FAKE6502_ZERO_FLAG) == 0)
        bra(c);
}

FAKE6502_FN_OPCODE(bpl)
{
    if ((c->cpu.flags & FAKE6502_SIGN_FLAG) == 0)
        bra(c);
}

FAKE6502_FN_OPCODE(brk)
{
    c->cpu.pc++;

    // push next instruction address onto stack
    fake6502_push_16(c, c->cpu.pc);

    // push CPU flags to stack
    fake6502_push_8(c, c->cpu.flags | FAKE6502_BREAK_FLAG);

    // set interrupt flag
    fake6502_interrupt_set(c);

    c->cpu.pc = fake6502_mem_read16(c, 0xfffe);
}

FAKE6502_FN_OPCODE(bvc)
{
    if ((c->cpu.flags & FAKE6502_OVERFLOW_FLAG) == 0)
        bra(c);
}

FAKE6502_FN_OPCODE(bvs)
{
    if ((c->cpu.flags & FAKE6502_OVERFLOW_FLAG) == FAKE6502_OVERFLOW_FLAG)
        bra(c);
}

FAKE6502_FN_OPCODE(clc)
{ fake6502_carry_clear(c); }

FAKE6502_FN_OPCODE(cld)
{ fake6502_decimal_clear(c); }

FAKE6502_FN_OPCODE(cli)
{ fake6502_interrupt_clear(c); }

FAKE6502_FN_OPCODE(clv)
{ fake6502_overflow_clear(c); }

FAKE6502_FN_OPCODE(cmp)
{ compare(c, c->cpu.a); }

FAKE6502_FN_OPCODE(cpx)
{ compare(c, c->cpu.x); }

FAKE6502_FN_OPCODE(cpy)
{ compare(c, c->cpu.y); }

FAKE6502_FN_OPCODE(dec)
{ fake6502_put_value(c, decrement(c, fake6502_get_value(c))); }

FAKE6502_FN_OPCODE(dex)
{ c->cpu.x = decrement(c, c->cpu.x); }

FAKE6502_FN_OPCODE(dey)
{ c->cpu.y = decrement(c, c->cpu.y); }

FAKE6502_FN_OPCODE(eor)
{ fake6502_accum_save(c, exclusive_or(c, c->cpu.a, fake6502_get_value(c))); }

FAKE6502_FN_OPCODE(inc)
{ fake6502_put_value(c, increment(c, fake6502_get_value(c))); }

FAKE6502_FN_OPCODE(inx)
{ c->cpu.x = increment(c, c->cpu.x); }

FAKE6502_FN_OPCODE(iny)
{ c->cpu.y = increment(c, c->cpu.y); }

FAKE6502_FN_OPCODE(jmp)
{ c->cpu.pc = c->emu.ea; }

FAKE6502_FN_OPCODE(jsr)
{
    fake6502_push_16(c, c->cpu.pc - 1);
    c->cpu.pc = c->emu.ea;
}

FAKE6502_FN_OPCODE(lda)
{
    uint16_t value = fake6502_get_value(c);
    c->cpu.a = (uint8_t)(value & 0x00FF);

    fake6502_zero_calc(c, c->cpu.a);
    fake6502_sign_calc(c, c->cpu.a);
}

FAKE6502_FN_OPCODE(ldx)
{
    uint16_t value = fake6502_get_value(c);
    c->cpu.x = (uint8_t)(value & 0x00FF);

    fake6502_zero_calc(c, c->cpu.x);
    fake6502_sign_calc(c, c->cpu.x);
}

FAKE6502_FN_OPCODE(ldy)
{
    uint16_t value = fake6502_get_value(c);
    c->cpu.y = (uint8_t)(value & 0x00FF);

    fake6502_zero_calc(c, c->cpu.y);
    fake6502_sign_calc(c, c->cpu.y);
}

FAKE6502_FN_OPCODE(lsr)
{ fake6502_put_value(c, logical_shift_right(c, fake6502_get_value(c))); }

FAKE6502_FN_OPCODE(nop)
{}

FAKE6502_FN_OPCODE(ora)
{
    uint16_t value = fake6502_get_value(c);
    uint16_t result = (uint16_t)c->cpu.a | value;

    fake6502_zero_calc(c, result);
    fake6502_sign_calc(c, result);

    fake6502_accum_save(c, result);
}

FAKE6502_FN_OPCODE(pha)
{ fake6502_push_8(c, c->cpu.a); }

FAKE6502_FN_OPCODE(phx)
{ fake6502_push_8(c, c->cpu.x); }

FAKE6502_FN_OPCODE(phy)
{ fake6502_push_8(c, c->cpu.y); }

FAKE6502_FN_OPCODE(php)
{ fake6502_push_8(c, c->cpu.flags | FAKE6502_BREAK_FLAG); }

FAKE6502_FN_OPCODE(pla)
{
    c->cpu.a = fake6502_pull_8(c);

    fake6502_zero_calc(c, c->cpu.a);
    fake6502_sign_calc(c, c->cpu.a);
}

FAKE6502_FN_OPCODE(plx)
{
    c->cpu.x = fake6502_pull_8(c);

    fake6502_zero_calc(c, c->cpu.x);
    fake6502_sign_calc(c, c->cpu.x);
}

FAKE6502_FN_OPCODE(ply)
{
    c->cpu.y = fake6502_pull_8(c);

    fake6502_zero_calc(c, c->cpu.y);
    fake6502_sign_calc(c, c->cpu.y);
}

FAKE6502_FN_OPCODE(plp)
{ c->cpu.flags = fake6502_pull_8(c) | FAKE6502_CONSTANT_FLAG | FAKE6502_BREAK_FLAG; }

FAKE6502_FN_OPCODE(rol)
{
    uint16_t value = fake6502_get_value(c);

    fake6502_put_value(c, value);
    fake6502_put_value(c, rotate_left(c, value));
}

FAKE6502_FN_OPCODE(ror)
{
    uint16_t value = fake6502_get_value(c);

    fake6502_put_value(c, value);
    fake6502_put_value(c, rotate_right(c, value));
}

FAKE6502_FN_OPCODE(rti)
{
    c->cpu.flags = fake6502_pull_8(c) | FAKE6502_CONSTANT_FLAG | FAKE6502_BREAK_FLAG;
    c->cpu.pc = fake6502_pull_16(c);
}

FAKE6502_FN_OPCODE(rts)
{ c->cpu.pc = fake6502_pull_16(c) + 1; }

FAKE6502_FN_OPCODE(sbc)
{
    uint16_t value = fake6502_get_value(c) ^ 0x00ff; // ones complement

    #ifdef DECIMALMODE
        // Apply decimal mode fix from http://forum.6502.org/viewtopic.php?p=37758#p37758
        if(c->cpu.flags & FAKE6502_DECIMAL_FLAG)
          value -= 0x0066; // use nines complement for BCD
    #endif

    fake6502_accum_save(c, add8(c, c->cpu.a, value, c->cpu.flags & FAKE6502_CARRY_FLAG));
}

FAKE6502_FN_OPCODE(sec)
{ fake6502_carry_set(c); }

FAKE6502_FN_OPCODE(sed)
{ fake6502_decimal_set(c); }

FAKE6502_FN_OPCODE(sei)
{ fake6502_interrupt_set(c); }

FAKE6502_FN_OPCODE(sta)
{ fake6502_put_value(c, c->cpu.a); }

FAKE6502_FN_OPCODE(stx)
{ fake6502_put_value(c, c->cpu.x); }

FAKE6502_FN_OPCODE(sty)
{ fake6502_put_value(c, c->cpu.y); }

FAKE6502_FN_OPCODE(stz)
{ fake6502_put_value(c, 0); }

FAKE6502_FN_OPCODE(tax)
{
    c->cpu.x = c->cpu.a;

    fake6502_zero_calc(c, c->cpu.x);
    fake6502_sign_calc(c, c->cpu.x);
}

FAKE6502_FN_OPCODE(tay)
{
    c->cpu.y = c->cpu.a;

    fake6502_zero_calc(c, c->cpu.y);
    fake6502_sign_calc(c, c->cpu.y);
}

FAKE6502_FN_OPCODE(tsx)
{
    c->cpu.x = c->cpu.s;

    fake6502_zero_calc(c, c->cpu.x);
    fake6502_sign_calc(c, c->cpu.x);
}

FAKE6502_FN_OPCODE(trb)
{
    uint16_t value = fake6502_get_value(c);
    uint16_t result = (uint16_t)c->cpu.a & ~value;
    fake6502_put_value(c, result);
    fake6502_zero_calc(c, (c->cpu.a | result) & 0x00ff);
}

FAKE6502_FN_OPCODE(tsb)
{
    uint16_t value = fake6502_get_value(c);
    uint16_t result = (uint16_t)c->cpu.a | value;
    fake6502_put_value(c, result);
    fake6502_zero_calc(c, (c->cpu.a | result) & 0x00ff);
}

FAKE6502_FN_OPCODE(txa)
{
    c->cpu.a = c->cpu.x;

    fake6502_zero_calc(c, c->cpu.a);
    fake6502_sign_calc(c, c->cpu.a);
}

FAKE6502_FN_OPCODE(txs)
{ c->cpu.s = c->cpu.x; }

FAKE6502_FN_OPCODE(tya)
{
    c->cpu.a = c->cpu.y;

    fake6502_zero_calc(c, c->cpu.a);
    fake6502_sign_calc(c, c->cpu.a);
}

FAKE6502_FN_OPCODE(lax)
{
    uint16_t value = fake6502_get_value(c);
    c->cpu.x = c->cpu.a = (uint8_t)(value & 0x00FF);

    fake6502_zero_calc(c, c->cpu.a);
    fake6502_sign_calc(c, c->cpu.a);
}

FAKE6502_FN_OPCODE(sax)
{ fake6502_put_value(c, c->cpu.a & c->cpu.x); }

FAKE6502_FN_OPCODE(dcp)
{
    dec(c);
    cmp(c);
}

FAKE6502_FN_OPCODE(isb)
{
    inc(c);
    sbc(c);
}

FAKE6502_FN_OPCODE(slo)
{
    asl(c);
    ora(c);
}

FAKE6502_FN_OPCODE(rla)
{
    uint16_t value = fake6502_get_value(c);
    uint16_t result = rotate_left(c, value);
    fake6502_put_value(c, value);
    fake6502_put_value(c, result);
    fake6502_accum_save(c, boolean_and(c, c->cpu.a, result));
}

FAKE6502_FN_OPCODE(sre)
{
    uint16_t value = fake6502_get_value(c);
    uint16_t result = logical_shift_right(c, value);
    fake6502_put_value(c, value);
    fake6502_put_value(c, result);
    fake6502_accum_save(c, exclusive_or(c, c->cpu.a, result));
}

FAKE6502_FN_OPCODE(rra)
{
    uint16_t value = fake6502_get_value(c);
    uint16_t result = rotate_right(c, value);
    fake6502_put_value(c, value);
    fake6502_put_value(c, result);
    fake6502_accum_save(c, add8(c, c->cpu.a, result, c->cpu.flags & FAKE6502_CARRY_FLAG));
}


// -------------------------------------------------------------------
// global's
// -------------------------------------------------------------------

// the opcode table - NMOS version

#ifdef NMOS6502
fake6502_opcode fake6502_opcodes[256] = {
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


// -------------------------------------------------------------------

// the opcode table - CMOS version

#ifdef CMOS6502
fake6502_opcode fake6502_opcodes[256] = {
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


// -------------------------------------------------------------------

// fake 6502 - API

// the main functions, for use by the host

void fake6502_reset(fake6502_context *c)
{
    // The 6502 normally does some fake reads after reset because
    // reset is a hacked-up version of NMI/IRQ/BRK
    // See https://www.pagetable.com/?p=410
    fake6502_mem_read(c, 0x00ff);
    fake6502_mem_read(c, 0x00ff);
    fake6502_mem_read(c, 0x00ff);
    fake6502_mem_read(c, 0x0100);
    fake6502_mem_read(c, 0x01ff);
    fake6502_mem_read(c, 0x01fe);
    c->cpu.pc = fake6502_mem_read16(c, 0xfffc);
    c->cpu.s = 0xfd;
    c->cpu.flags |= FAKE6502_CONSTANT_FLAG | FAKE6502_INTERRUPT_FLAG;

    c->emu.instructions = 0;
    c->emu.clockticks = 0;
}

void fake6502_nmi(fake6502_context *c)
{
    fake6502_push_16(c, c->cpu.pc);
    fake6502_push_8(c, c->cpu.flags & ~FAKE6502_BREAK_FLAG);
    c->cpu.flags |= FAKE6502_INTERRUPT_FLAG;
    c->cpu.pc = fake6502_mem_read16(c, 0xfffa);
}

void fake6502_irq(fake6502_context *c)
{
    if ((c->cpu.flags & FAKE6502_INTERRUPT_FLAG) == 0)
    {
        fake6502_push_16(c, c->cpu.pc);
        fake6502_push_8(c, c->cpu.flags & ~FAKE6502_BREAK_FLAG);
        c->cpu.flags |= FAKE6502_INTERRUPT_FLAG;
        c->cpu.pc = fake6502_mem_read16(c, 0xfffe);
    }
}

void fake6502_step(fake6502_context *c)
{
    uint8_t opcode = fake6502_mem_read(c, c->cpu.pc++);
    c->emu.opcode = opcode;
    c->cpu.flags |= FAKE6502_CONSTANT_FLAG;

    fake6502_opcodes[opcode].addr_mode(c);
    fake6502_opcodes[opcode].opcode(c);
    c->emu.clockticks += fake6502_opcodes[opcode].clockticks;
}


// -------------------------------------------------------------------
