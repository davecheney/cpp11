#pragma once

enum {
    PRINTSTATE = false,
    DEBUG_INTER = false,
    DEBUG_RK05 = false,
    DEBUG_MMU = false,

};

void disasm(uint32_t ia);

void trap(uint16_t num);



