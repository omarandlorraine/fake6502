
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

#define FAKE6502_CARRY_FLAG             0x01
#define FAKE6502_ZERO_FLAG              0x02
#define FAKE6502_INTERRUPT_FLAG         0x04
#define FAKE6502_DECIMAL_FLAG           0x08
#define FAKE6502_BREAK_FLAG             0x10
#define FAKE6502_CONSTANT_FLAG          0x20
#define FAKE6502_OVERFLOW_FLAG          0x40
#define FAKE6502_SIGN_FLAG              0x80

#define FAKE6502_STACK_BASE             0x100


// -------------------------------------------------------------------
// macro's
// -------------------------------------------------------------------

// operator fn prototypes

#ifdef FAKE6502_OPS_STATIC

#define FAKE6502_FN_OPCODE(m_name)      \
  static void m_name(fake6502_context *c)

#define FAKE6502_FN_ADDR_MODE(m_name)   \
  static void m_name(fake6502_context *c)

#else

#define FAKE6502_FN_OPCODE(m_name)      \
  void m_name(fake6502_context *c)

#define FAKE6502_FN_ADDR_MODE(m_name)   \
  void m_name(fake6502_context *c)

#endif


// c = a pointer to the fake6502_context


// flag modifier macros

#define fake6502_carry_set(c)           (c)->cpu.flags |= FAKE6502_CARRY_FLAG
#define fake6502_carry_clear(c)         (c)->cpu.flags &= (~FAKE6502_CARRY_FLAG)
#define fake6502_zero_set(c)            (c)->cpu.flags |= FAKE6502_ZERO_FLAG
#define fake6502_zero_clear(c)          (c)->cpu.flags &= (~FAKE6502_ZERO_FLAG)
#define fake6502_interrupt_set(c)       (c)->cpu.flags |= FAKE6502_INTERRUPT_FLAG
#define fake6502_interrupt_clear(c)     (c)->cpu.flags &= (~FAKE6502_INTERRUPT_FLAG)
#define fake6502_decimal_set(c)         (c)->cpu.flags |= FAKE6502_DECIMAL_FLAG
#define fake6502_decimal_clear(c)       (c)->cpu.flags &= (~FAKE6502_DECIMAL_FLAG)
#define fake6502_overflow_set(c)        (c)->cpu.flags |= FAKE6502_OVERFLOW_FLAG
#define fake6502_overflow_clear(c)      (c)->cpu.flags &= (~FAKE6502_OVERFLOW_FLAG)
#define fake6502_sign_set(c)            (c)->cpu.flags |= FAKE6502_SIGN_FLAG
#define fake6502_sign_clear(c)          (c)->cpu.flags &= (~FAKE6502_SIGN_FLAG)

#define fake6502_accum_save(c, n)       (c)->cpu.a = (uint8_t)((n)&0x00FF)


// flag calculation macros

#define fake6502_zero_calc(c, n)        \
{                                       \
    if ((n) & 0x00FF)                   \
        fake6502_zero_clear(c);         \
    else                                \
        fake6502_zero_set(c);           \
}

#define fake6502_sign_calc(c, n)        \
{                                       \
    if ((n) & 0x0080)                   \
        fake6502_sign_set(c);           \
    else                                \
        fake6502_sign_clear(c);         \
}

#define fake6502_carry_calc(c, n)       \
{                                       \
    if ((n) & 0xFF00)                   \
        fake6502_carry_set(c);          \
    else                                \
        fake6502_carry_clear(c);        \
}


// n = result, m = accumulator, o = memory

#define fake6502_overflow_calc(c, n, m, o)  \
{                                       \
    if (((n) ^ (uint16_t)(m)) & ((n) ^ (o)) & 0x0080)  \
        fake6502_overflow_set(c);       \
    else                                \
        fake6502_overflow_clear(c);     \
}


// -------------------------------------------------------------------
// typedef's
// -------------------------------------------------------------------

typedef struct fake6502_cpu_state {
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t flags;
    uint8_t s;
    uint16_t pc;
} fake6502_cpu_state;

typedef struct fake6502_emu_state {
    int instructions;
    int clockticks;
    uint16_t ea;
    uint8_t opcode;
} fake6502_emu_state;

typedef struct fake6502_context {
    fake6502_cpu_state cpu;
    fake6502_emu_state emu;
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

extern fake6502_opcode fake6502_opcodes[];


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
