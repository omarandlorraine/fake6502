
// -------------------------------------------------------------------

#ifndef FAKE6502_H
#define FAKE6502_H

// -------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

// -------------------------------------------------------------------
// include's
// -------------------------------------------------------------------

#include <stdint.h>


// -------------------------------------------------------------------
// define's
// -------------------------------------------------------------------

#define FLAG_CARRY                      0x01
#define FLAG_ZERO                       0x02
#define FLAG_INTERRUPT                  0x04
#define FLAG_DECIMAL                    0x08
#define FLAG_BREAK                      0x10
#define FLAG_CONSTANT                   0x20
#define FLAG_OVERFLOW                   0x40
#define FLAG_SIGN                       0x80

#define BASE_STACK                      0x100


// -------------------------------------------------------------------
// macro's
// -------------------------------------------------------------------

// c = a pointer to the context_6502


// flag modifier macros

#define setcarry(c)                     c->cpu.flags |= FLAG_CARRY
#define clearcarry(c)                   c->cpu.flags &= (~FLAG_CARRY)
#define setzero(c)                      c->cpu.flags |= FLAG_ZERO
#define clearzero(c)                    c->cpu.flags &= (~FLAG_ZERO)
#define setinterrupt(c)                 c->cpu.flags |= FLAG_INTERRUPT
#define clearinterrupt(c)               c->cpu.flags &= (~FLAG_INTERRUPT)
#define setdecimal(c)                   c->cpu.flags |= FLAG_DECIMAL
#define cleardecimal(c)                 c->cpu.flags &= (~FLAG_DECIMAL)
#define setoverflow(c)                  c->cpu.flags |= FLAG_OVERFLOW
#define clearoverflow(c)                c->cpu.flags &= (~FLAG_OVERFLOW)
#define setsign(c)                      c->cpu.flags |= FLAG_SIGN
#define clearsign(c)                    c->cpu.flags &= (~FLAG_SIGN)
#define saveaccum(c, n)                 c->cpu.a = (uint8_t)((n)&0x00FF)


// flag calculation macros

#define zerocalc(c, n)                  \
{                                       \
    if ((n)&0x00FF)                     \
        clearzero(c);                   \
    else                                \
        setzero(c);                     \
}

#define signcalc(c, n)                  \
{                                       \
    if ((n)&0x0080)                     \
        setsign(c);                     \
    else                                \
        clearsign(c);                   \
}

#define carrycalc(c, n)                 \
{                                       \
    if ((n)&0xFF00)                     \
        setcarry(c);                    \
    else                                \
        clearcarry(c);                  \
}


// n = result, m = accumulator, o = memory

#define overflowcalc(c, n, m, o)        \
{                                       \
    if (((n) ^ (uint16_t)(m)) & ((n) ^ (o)) & 0x0080)  \
        setoverflow(c);                 \
    else                                \
        clearoverflow(c);               \
}


// -------------------------------------------------------------------
// typedef's
// -------------------------------------------------------------------

typedef struct state_6502_cpu {
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t flags;
    uint8_t s;
    uint16_t pc;
} state_6502_cpu;

typedef struct state_6502_emu {
    int instructions;
    int clockticks;
    uint16_t ea;
    uint8_t opcode;
} state_6502_emu;

typedef struct context_6502 {
    state_6502_cpu cpu;
    state_6502_emu emu;
    void *state_host;
} context_6502;


typedef struct opcode_6502 {
    void (*addr_mode)(context_6502 *c);
    void (*opcode)(context_6502 *c);
    int clockticks;
} opcode_6502;


// -------------------------------------------------------------------
// global's
// -------------------------------------------------------------------

extern opcode_6502 opcodes[];


// -------------------------------------------------------------------
// prototype's
// -------------------------------------------------------------------

extern void push6502_8(context_6502 *c, uint8_t pushval);
extern void push6502_16(context_6502 *c, uint16_t pushval);
extern uint8_t pull6502_8(context_6502 *c);
extern uint16_t pull6502_16(context_6502 *c);

extern uint16_t getvalue(context_6502 *c);
extern void putvalue(context_6502 *c, uint16_t saveval);

extern void reset6502(context_6502 *c);
extern void irq6502(context_6502 *c);
extern void nmi6502(context_6502 *c);
extern void step6502(context_6502 *c);

extern uint8_t mem_read(context_6502 *c, uint16_t address);
extern void mem_write(context_6502 *c, uint16_t address, uint8_t val);


// -------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

// -------------------------------------------------------------------

#endif

// -------------------------------------------------------------------
