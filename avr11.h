// interrupts
enum {
    INTBUS = 0004,
    INTINVAL = 0010,
    INTDEBUG = 0014,
    INTIOT = 0020,
    INTTTYIN = 0060,
    INTTTYOUT = 0064,
    INTFAULT = 0250,
    INTCLOCK = 0100,
    INTRK = 0220
};

enum {
    PRINTSTATE = false,
    INSTR_TIMING = true,
    DEBUG_INTER = false,
    DEBUG_RK05 = false,
    DEBUG_MMU = false,
    ENABLE_LKS = true,
};

void printstate();
void disasm(uint32_t ia);

uint16_t trap(uint16_t num);


namespace pdp11 {
struct intr {
    uint8_t vec;
    uint8_t pri;
};
}; // namespace pdp11

#define ITABN 8

enum { FLAGN = 8, FLAGZ = 4, FLAGV = 2, FLAGC = 1 };
