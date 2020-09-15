#pragma once
#include <stdint.h>
#include "kb11.h"

class DL11 {

  public:
    void clearterminal();
    void poll();
    uint16_t read16(uint32_t a);
    void write16(uint32_t a, uint16_t v);

    KB11 &cpu;

    DL11(KB11 &kb11) : cpu(kb11){};

  private:
    uint16_t tks;
    uint16_t tkb;
    uint16_t tps;
    uint16_t tpb;
    uint8_t count;

    void addchar(char c);
};