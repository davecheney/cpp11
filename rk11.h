#include <stdint.h>

class RK11 {

    public:
    uint32_t RKBA, RKDS, RKER, RKCS, RKWC;
    FILE* rkdata;

    uint16_t read16(uint32_t a);
    void write16(uint32_t a, uint16_t v);
    void reset();
    void step();

    private:
    uint32_t drive, sector, surface, cylinder;
    

    void rknotready();
    void rkready();
    void rkerror(uint16_t e);
};
