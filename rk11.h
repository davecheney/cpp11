#pragma once
#include <stdint.h>
#include <stdio.h>

class RK11 {

  public:
    FILE *rkdata;

    uint16_t read16(uint32_t a);
    void write16(uint32_t a, uint16_t v);
    void reset();
    void step();

  private:
    uint16_t rkds, rker, rkcs, rkwc, rkba, rkda;
    uint32_t drive, sector, surface, cylinder;

    void rknotready();
    void rkready();
    void readwrite();
    void seek();
};
