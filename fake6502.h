#include <stdint.h>
typedef struct c1 {
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t flags;
    uint8_t s;
    uint16_t pc;
    int clockticks;
    uint8_t mem[65536];
    uint16_t ea;
    uint8_t opcode;
} context_t;

void push6502_8(context_t *c, uint8_t pushval);
void push6502_16(context_t *c, uint16_t pushval);
uint8_t pull6502_8(context_t *c);
uint16_t pull6502_16(context_t *c);

void step(context_t * c);
void reset6502(context_t * c);
void irq6502(context_t * c);
void nmi6502(context_t * c);
uint8_t mem_read(context_t * c, uint16_t address);
void mem_write(context_t * c, uint16_t address, uint8_t val);

