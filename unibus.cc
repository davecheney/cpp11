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
        core.write16(a, v);
        return;
    }
    switch (a) {
    case 0777546:
        kw11.write16(a, v);
        return;
    case 0777572:
        cpu.mmu.SR0 = v;
        return;
    }
    if ((a & 0777770) == 0777560) {
        cons.write16(a, v);
        return;
    }

    switch (a & ~077) {
    case 0777600: // MMU user mode 3 Map
    case 0772300: // MMU kernel mode 0 Map
        cpu.mmu.write16(a, v);
        return;
    case 0777400:
        rk11.write16(a, v);
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
        return core.intptr[a >> 1];
    }

    if (a == 0777546) {
        return kw11.read16(a);
    }

    if (a == 0777572) {
        return cpu.mmu.SR0;
    }

    if (a == 0777574) {
        return cpu.mmu.SR1;
    }

    if (a == 0777576) {
        return cpu.mmu.SR2;
    }

    if ((a & 0777770) == 0777550) {
        return ptr.read16(a);
    }

    if ((a & 0777770) == 0777560) {
        return cons.read16(a);
    }

    switch (a & ~077) {
    case 0777600: // MMU user mode 3 Map
    case 0772300: // MMU kernel mode 0 Map
        return cpu.mmu.read16(a);
    case 0777400:
        return rk11.read16(a);
    default:
        printf("unibus: read from invalid address %06o\n", a);
        trap(INTBUS);
    }
}

void UNIBUS::reset() {
    cons.clearterminal();
    rk11.reset();
    kw11.write16(0777546, 0x00); // disable line clock INTR
}
