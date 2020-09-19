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

void trap(uint16_t num);



