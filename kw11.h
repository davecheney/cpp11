#pragma once
#include <stdint.h>

class KW11 {
    public:
    uint16_t csr;

    KW11();
    void tick();
};