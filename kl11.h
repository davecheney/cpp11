#pragma once
#include <stdint.h>

class KL11 {

  public:
    KL11();

    void clearterminal();
    void poll();
    uint16_t read16(uint32_t a);
    void write16(uint32_t a, uint16_t v);

  private:
    uint16_t rcsr;
    uint16_t rbuf;
    uint16_t xcsr;
    uint8_t xbuf;
    uint16_t count;

    inline bool rcvrdone() { return rcsr & 0x80; }
    inline bool xmitready() { return xcsr & 0x80; }
};