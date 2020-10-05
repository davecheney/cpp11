#include <cstdlib>
#include <stdint.h>
#include <stdio.h>

#include "avr11.h"
#include "kb11.h"
#include "unibus.h"

extern KB11 cpu;

void UNIBUS::write16(uint32_t a, uint16_t v) {
    if (a & 1) {
        printf("unibus: write16 to odd address %06o\n", a);
        trap(INTBUS);
    }
    if (a < 0760000) {
        core[a >> 1] = v;
        return;
    }
    switch (a & ~077) {
    case 0777400:
        rk11.write16(a, v);
        return;
    case 0777500:
        switch (a) {
        case 0777514:
        case 0777516:
            lp11.write16(a, v);
            return;
        case 0777546:
            kw11.write16(a, v);
            return;
        case 0777572:
            cpu.mmu.SR[0] = v;
            return;
        case 0777574:
            cpu.mmu.SR[1] = v;
            return;
        case 0777576:
<<<<<<< Updated upstream
            cpu.mmu.SR[2] = v;
=======
            // do nothing, SR2 is read only
>>>>>>> Stashed changes
            return;
        default:
            cons.write16(a, v);
            return;
        }
    case 0772200:
    case 0772300:
    case 0777600:
        cpu.mmu.write16(a, v);
        return;
    default:
        printf("unibus: write to invalid address %06o\n", a);
        trap(INTBUS);
    }
}

uint16_t UNIBUS::read16(uint32_t a) {
    if (a & 1) {
        printf("unibus: read16 from odd address %06o\n", a);
        trap(INTBUS);
    }
    if (a < 0760000) {
        return core[a >> 1];
    }
    switch (a & ~077) {
    case 0777400:
        return rk11.read16(a);
    case 0777500:
        switch (a) {
        case 0777514:
        case 0777516:
            return lp11.read16(a);
        case 0777546:
            return kw11.read16(a);
        case 0777572:
            return cpu.mmu.SR[0];
        case 0777574:
            return cpu.mmu.SR[1];
        case 0777576:
            return cpu.mmu.SR[2];
        default:
            return cons.read16(a);
        }
    case 0772200:
    case 0772300:
    case 0777600:
        return cpu.mmu.read16(a);
    default:
        printf("unibus: read from invalid address %06o\n", a);
        trap(INTBUS);
    }
}

void UNIBUS::reset() {
    cons.clearterminal();
    rk11.reset();
    kw11.write16(0777546, 0x00); // disable line clock INTR
    lp11.reset();
}
