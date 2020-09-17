#pragma once
#include <stdint.h>

class KW11 {
    public:
    uint16_t csr;
    void tick();

    private:
    uint16_t clkcounter;
};