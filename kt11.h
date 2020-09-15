#include <stdint.h>
#pragma once

class KT11 {

  public:
    class page {
      public:
        uint16_t par, pdr;

        uint16_t addr();
        uint8_t len();
        bool read();
        bool write();
        bool ed();
    };

    uint16_t SR0, SR2;

    uint32_t decode(uint16_t a, uint8_t w, uint8_t user);
    uint16_t read16(uint32_t a);
    void write16(uint32_t a, uint16_t v);

  private:
    page pages[16];
    void dumppages();
};