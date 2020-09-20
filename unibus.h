#pragma once
#include "ms11.h"
#include "kl11.h"
#include "rk11.h"
#include "kw11.h"
#include "pc11.h"
#include <stdint.h>

class UNIBUS {

  public:
    MS11 core;
    KL11 cons;
    RK11 rk11;
    KW11 kw11;
    PC11 ptr;

    void write16(uint32_t a, uint16_t v);
    uint16_t read16(uint32_t a);

    void reset();

};