#pragma once
#include <array>
#include <stdint.h>

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

    uint16_t SR0, SR1, SR2;

    template <bool wr> uint32_t decode(uint16_t a, uint16_t mode);
    uint16_t read16(uint32_t a);
    void write16(uint32_t a, uint16_t v);

  private:
    std::array<page, 16> pages;
    void dumppages();
};