#include "ms11.h"

void MS11::write16(uint32_t a, uint16_t v) {
    intptr[a>> 1] = v;
}

uint16_t MS11::read16(uint32_t a) {
    return intptr[a>>1];
}