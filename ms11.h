#include <stdint.h>

const uint32_t IOBASE_18BIT = 0760000;
class MS11 {

  public:
    MS11() { charptr = reinterpret_cast<char *>(&intptr); }

    void write16(uint32_t a, uint16_t v);
    uint16_t read16(uint32_t a);

    // memory as words
    uint16_t intptr[IOBASE_18BIT >> 1];
    // memory as bytes
    char *charptr;
};
