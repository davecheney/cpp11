#pragma once
#include "ms11.h"
#include "dl11.h"
#include <stdint.h>

class UNIBUS {

  public:
    MS11 core;
    DL11 cons;

    uint16_t read8(const uint32_t a);
    void write8(const uint32_t a, const uint16_t v);
    void write16(uint32_t a, uint16_t v);
    uint16_t read16(uint32_t a);

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
};