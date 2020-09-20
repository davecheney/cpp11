#pragma once
#include <stdint.h>
#include <array>

const uint32_t IOBASE_18BIT = 0760000;
class MS11 {

  public:

    void write16(uint32_t a, uint16_t v);
    uint16_t read16(uint32_t a);

    // memory as words
    std::array<uint16_t,(IOBASE_18BIT >> 1)> intptr;
    
};
