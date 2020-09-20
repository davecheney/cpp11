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
    case 0777776:
        switch (v >> 14) {
        case 0:
            cpu.switchmode(false);
            break;
        case 3:
            cpu.switchmode(true);
            break;
        default:
            printf("invalid mode\n");
            std::abort();
        }
        switch ((v >> 12) & 3) {
        case 0:
            cpu.prevuser = false;
            break;
        case 3:
            cpu.prevuser = true;
            break;
        default:
            printf("invalid mode\n");
            std::abort();
        }
        cpu.PS = v;
        return;
    case 0777774:
        cpu.stacklimit = v;
        return;
    case 0777546:
        kw11.csr = v;
        return;
    case 0777572:
        cpu.mmu.SR0 = v;
        return;
    }
    if ((a & 0777770) == 0777560) {
        cons.write16(a, v);
        return;
    }
    if ((a & 0777700) == 0777400) {
        rk11.write16(a, v);
        return;
    }
    if (((a & 0777600) == 0772200) || ((a & 0777600) == 0777600)) {
        cpu.mmu.write16(a, v);
        return;
    }
    printf("unibus: write to invalid address %06o\n", a);
    trap(INTBUS);
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
        return kw11.csr;
    }

    if (a == 0777570) {
        return cpu.switchregister;
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

    if (a == 0777774) {
        return cpu.stacklimit;
    }

    if (a == 0777776) {
        return cpu.PS;
    }

    if ((a & 0777770) == 0777550) {
        return ptr.read16(a);
    }

    if ((a & 0777770) == 0777560) {
        return cons.read16(a);
    }

    if ((a & 0777760) == 0777400) {
        return rk11.read16(a);
    }

    if (((a & 0777600) == 0772200) || ((a & 0777600) == 0777600)) {
        return cpu.mmu.read16(a);
    }

    printf("unibus: read from invalid address %06o\n", a);
    trap(INTBUS);
}

void UNIBUS::reset() {
    cons.clearterminal();
    rk11.reset();
    kw11.csr = 0x80;
}
