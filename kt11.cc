#include "kt11.h"
#include "avr11.h"
#include "kb11.h"
#include <stdint.h>
#include <stdio.h>

extern KB11 cpu;

uint16_t KT11::page::addr() { return par & 07777; }
uint8_t KT11::page::len() { return (pdr >> 8) & 0x7f; }
bool KT11::page::read() { return pdr & 2; }
bool KT11::page::write() { return pdr & 6; };
bool KT11::page::ed() { return pdr & 8; }

template <bool wr> uint32_t KT11::decode(uint16_t a, uint16_t mode) {
    if ((SR0 & 1) == 0) {
        return a > 0167777 ? ((uint32_t)a) + 0600000 : a;
    }
    auto i = (a >> 13);
    if (wr && !pages[mode][i].write()) {
        SR0 = (1 << 13) | 1;
        SR0 |= (a >> 12) & ~1;
        if (mode) {
            SR0 |= (1 << 5) | (1 << 6);
        }
        // SR2 = cpu.PC;

        printf("mmu::decode write to read-only page %06o\n", a);
        trap(INTFAULT);
    }
    if (!pages[mode][i].read()) {
        SR0 = (1 << 15) | 1;
        SR0 |= (a >> 12) & ~1;
        if (mode) {
            SR0 |= (1 << 5) | (1 << 6);
        }
        // SR2 = cpu.PC;
        printf("mmu::decode read from no-access page %06o\n", a);
        trap(INTFAULT);
    }
    uint8_t block = (a >> 6) & 0177;
    uint8_t disp = a & 077;
    // if ((p.ed() && (block < p.len())) || (!p.ed() && (block > p.len()))) {
    if (pages[mode][i].ed() ? (block < pages[mode][i].len()) : (block > pages[mode][i].len())) {
        SR0 = (1 << 14) | 1;
        SR0 |= (a >> 12) & ~1;
        if (mode) {
            SR0 |= (1 << 5) | (1 << 6);
        }
        // SR2 = cpu.PC;
        printf(
            "page length exceeded, address %06o (block %03o) is beyond length "
            "%03o\r\n",
            a, block, pages[mode][i].len());
        trap(INTFAULT);
    }
    if (wr) {
        pages[mode][i].pdr |= 1 << 6;
    }
    // danger, this can be cast to a uint16_t if you aren't careful
    uint32_t aa = pages[mode][i].par & 07777;
    aa += block;
    aa <<= 6;
    aa += disp;
    // printf("decode: slow %06o -> %06o\n", a, aa);
    return aa;
}

template uint32_t KT11::decode<true>(uint16_t, uint16_t);
template uint32_t KT11::decode<false>(uint16_t, uint16_t);

uint16_t KT11::read16(uint32_t a) {
    // printf("kt11:read16: %06o\n", a);
    auto i = ((a & 017) >> 1);
    switch (a & ~037) {
    case 0772300:
        return pages[00][i].pdr;
    case 0772340:
        return pages[00][i].par;
    case 0777600:
        return pages[03][i].pdr;
    case 0777640:
        return pages[03][i].par;
    default:
        printf("mmu::read16 invalid read from %06o\n", a);
        trap(INTBUS);
    }
}

void KT11::write16(uint32_t a, uint16_t v) {
    //  printf("kt11:write16: %06o %06o\n", a, v);
    auto i = ((a & 017) >> 1);
    switch (a & ~037) {
    case 0772300:
        pages[00][i].pdr = v;
        break;
    case 0772340:
        pages[00][i].par = v;
        break;
    case 0777600:
        pages[03][i].pdr = v;
        break;
    case 0777640:
        pages[03][i].par = v;
        break;
    default:
        printf("mmu::write16 write to invalid address %06o\n", a);
        trap(INTBUS);
    }
}
