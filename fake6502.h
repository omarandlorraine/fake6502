
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

#define FAKE6502_FLAG_CARRY             0x01
#define FAKE6502_FLAG_ZERO              0x02
#define FAKE6502_FLAG_INTERRUPT         0x04
#define FAKE6502_FLAG_DECIMAL           0x08
#define FAKE6502_FLAG_BREAK             0x10
#define FAKE6502_FLAG_CONSTANT          0x20
#define FAKE6502_FLAG_OVERFLOW          0x40
#define FAKE6502_FLAG_SIGN              0x80

#define FAKE6502_BASE_STACK             0x100


// -------------------------------------------------------------------
// macro's
// -------------------------------------------------------------------

// c = a pointer to the fake6502_context


// flag modifier macros

#define fake6502_set_carry(c)           (c)->cpu.flags |= FAKE6502_FLAG_CARRY
#define fake6502_clear_carry(c)         (c)->cpu.flags &= (~FAKE6502_FLAG_CARRY)
#define fake6502_set_zero(c)            (c)->cpu.flags |= FAKE6502_FLAG_ZERO
#define fake6502_clear_zero(c)          (c)->cpu.flags &= (~FAKE6502_FLAG_ZERO)
#define fake6502_set_interrupt(c)       (c)->cpu.flags |= FAKE6502_FLAG_INTERRUPT
#define fake6502_clear_interrupt(c)     (c)->cpu.flags &= (~FAKE6502_FLAG_INTERRUPT)
#define fake6502_set_decimal(c)         (c)->cpu.flags |= FAKE6502_FLAG_DECIMAL
#define fake6502_clear_decimal(c)       (c)->cpu.flags &= (~FAKE6502_FLAG_DECIMAL)
#define fake6502_set_overflow(c)        (c)->cpu.flags |= FAKE6502_FLAG_OVERFLOW
#define fake6502_clear_overflow(c)      (c)->cpu.flags &= (~FAKE6502_FLAG_OVERFLOW)
#define fake6502_set_sign(c)            (c)->cpu.flags |= FAKE6502_FLAG_SIGN
#define fake6502_clear_sign(c)          (c)->cpu.flags &= (~FAKE6502_FLAG_SIGN)
#define fake6502_save_accum(c, n)       (c)->cpu.a = (uint8_t)((n)&0x00FF)


// flag calculation macros

#define fake6502_zero_calc(c, n)        \
{                                       \
    if ((n)&0x00FF)                     \
        fake6502_clear_zero(c);         \
    else                                \
        fake6502_set_zero(c);           \
}

#define fake6502_sign_calc(c, n)        \
{                                       \
    if ((n)&0x0080)                     \
        fake6502_set_sign(c);           \
    else                                \
        fake6502_clear_sign(c);         \
}

#define fake6502_carry_calc(c, n)       \
{                                       \
    if ((n)&0xFF00)                     \
        fake6502_set_carry(c);          \
    else                                \
        fake6502_clear_carry(c);        \
}


// n = result, m = accumulator, o = memory

#define fake6502_overflow_calc(c, n, m, o)  \
{                                       \
    if (((n) ^ (uint16_t)(m)) & ((n) ^ (o)) & 0x0080)  \
        fake6502_set_overflow(c);       \
    else                                \
        fake6502_clear_overflow(c);     \
}


// -------------------------------------------------------------------
// typedef's
// -------------------------------------------------------------------

typedef struct fake6502_state_cpu {
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t flags;
    uint8_t s;
    uint16_t pc;
} fake6502_state_cpu;

typedef struct fake6502_state_emu {
    int instructions;
    int clockticks;
    uint16_t ea;
    uint8_t opcode;
} fake6502_state_emu;

typedef struct fake6502_context {
    fake6502_state_cpu cpu;
    fake6502_state_emu emu;
    void *state_host;
} fake6502_context;


typedef struct fake6502_opcode {
    void (*addr_mode)(fake6502_context *c);
    void (*opcode)(fake6502_context *c);
    int clockticks;
} fake6502_opcode;


// -------------------------------------------------------------------
// global's
// -------------------------------------------------------------------

extern fake6502_opcode opcodes[];


// -------------------------------------------------------------------
// prototype's
// -------------------------------------------------------------------

extern void fake6502_push_8(fake6502_context *c, uint8_t pushval);
extern void fake6502_push_16(fake6502_context *c, uint16_t pushval);
extern uint8_t fake6502_pull_8(fake6502_context *c);
extern uint16_t fake6502_pull_16(fake6502_context *c);

extern uint16_t fake6502_get_value(fake6502_context *c);
extern void fake6502_put_value(fake6502_context *c, uint16_t saveval);

extern void fake6502_reset(fake6502_context *c);
extern void fake6502_irq(fake6502_context *c);
extern void fake6502_nmi(fake6502_context *c);
extern void fake6502_step(fake6502_context *c);

extern uint8_t fake6502_mem_read(fake6502_context *c, uint16_t address);
extern void fake6502_mem_write(fake6502_context *c, uint16_t address, uint8_t val);


// -------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

// -------------------------------------------------------------------

#endif

// -------------------------------------------------------------------
