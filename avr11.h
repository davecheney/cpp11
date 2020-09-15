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

const uint32_t MEMSIZE = 1<<18;

namespace unibus {

// operations on uint32_t types are insanely expensive
union addr {
    uint8_t bytes[4];
    uint32_t value;
};
void init();

uint16_t read8(uint32_t addr);
void write8(uint32_t a, uint16_t v);

uint16_t read16(uint32_t addr);
void write16(uint32_t addr, uint16_t v);

template <bool wr> inline uint16_t access(uint32_t addr, uint16_t v = 0) {
    if (wr) {
        write16(addr, v);
        return 0;
    }
    return read16(addr);
}

template <uint8_t l> inline uint16_t read(const uint32_t a) {
    if (l == 2) {
        return read16(a);
    }
    return read8(a);
}

template <uint8_t l> inline void write(const uint32_t addr, uint16_t v) {
    if (l == 2) {
        return write16(addr, v);
    }
    return write8(addr, v);
}

}; // namespace unibus

namespace pdp11 {
struct intr {
    uint8_t vec;
    uint8_t pri;
};
}; // namespace pdp11

#define ITABN 8

enum { FLAGN = 8, FLAGZ = 4, FLAGV = 2, FLAGC = 1 };
