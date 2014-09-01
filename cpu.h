// this is all kinds of wrong
#include <setjmp.h>

extern jmp_buf trapbuf;

namespace pdp11 {
struct intr {
  uint8_t vec;
  uint8_t pri;
};
};

#define ITABN 8

extern pdp11::intr itab[ITABN];


enum {
  FLAGN = 8,
  FLAGZ = 4,
  FLAGV = 2,
  FLAGC = 1
};

namespace cpu {

extern int32_t R[8];

extern uint16_t PC;
extern uint16_t PS;
extern uint16_t USP;
extern uint16_t KSP;
extern uint16_t LKS;
extern bool curuser;
extern bool prevuser;

void step();
void reset(void);
void switchmode(bool newm);

void trapat(uint16_t vec);
void interrupt(uint8_t vec, uint8_t pri);
void handleinterrupt();

};

namespace mmu {

    extern uint16_t SR0;
    extern uint16_t SR2;

    uint32_t decode(uint16_t a, uint8_t w, uint8_t user);
    uint16_t read16(uint32_t a);
    void write16(uint32_t a, uint16_t v);

};



