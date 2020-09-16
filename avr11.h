#pragma once

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


