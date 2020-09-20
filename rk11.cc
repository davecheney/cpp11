#include <cstdlib>
#include <stdint.h>
#include <stdio.h>

#include "avr11.h"
#include "kb11.h"
#include "rk11.h"

extern KB11 cpu;

enum {
    RKOVR = (1 << 14),
    RKNXD = (1 << 7),
    RKNXC = (1 << 6),
    RKNXS = (1 << 5)
};

uint16_t RK11::read16(uint32_t a) {
    switch (a & 0x17) {
    case 000:
        return RKDS;
    case 002:
        return RKER;
    case 004:
        return RKCS | (RKBA & 0x30000) >> 12;
    case 006:
        return RKWC;
    case 010:
        return RKBA & 0xFFFF;
    case 012:
        return (sector) | (surface << 4) | (cylinder << 5) | (drive << 13);
    default:
        printf("rk11::read16 invalid read\n");
        std::abort();
    }
}

void RK11::rknotready() {
    RKDS &= ~(1 << 6);
    RKCS &= ~(1 << 7);
}

void RK11::rkready() {
    RKDS |= 1 << 6;
    RKCS |= 1 << 7;
}

void RK11::rkerror(uint16_t e) { printf("rk11: error %06o\n", e); }

void RK11::step() {
again:
    bool w;
    switch ((RKCS & 017) >> 1) {
    case 0:
        return;
    case 1:
        w = true;
        break;
    case 2:
        w = false;
        break;
    default:
        printf("unimplemented RK05 operation %06o\n", ((RKCS & 017) >> 1));
        std::abort();
    }

    if (DEBUG_RK05) {
        printf("rkstep: RKBA: %06o RKWC: %06o cylinder: %03o sector: %03o "
               "write: %s\n",
               RKBA, RKWC, cylinder, sector, w ? "true" : "false");
    }

    if (drive != 0) {
        rkerror(RKNXD);
    }
    if (cylinder > 0312) {
        rkerror(RKNXC);
    }
    if (sector > 013) {
        rkerror(RKNXS);
    }

    uint32_t pos = (cylinder * 24 + surface * 12 + sector) * 512;
    if (fseek(rkdata, pos, SEEK_SET)) {
        printf("rkstep: failed to seek\n");
        std::abort();
    }

    for (uint16_t i = 0; i < 256 && RKWC != 0; i++) {
        if (w) {
            auto val = cpu.unibus.read16(RKBA);
            uint8_t buf[2] = { static_cast<uint8_t>(val &0xff), static_cast<uint8_t>(val >> 8) };
            assert(fwrite(&buf, 1, 2, rkdata) == 2);
        } else {
            uint8_t buf[2];
            assert(fread(&buf, 1, 2, rkdata) == 2);
            cpu.unibus.write16(RKBA, static_cast<uint16_t>(buf[0]) | static_cast<uint16_t>(buf[1]) << 8);
        }
        RKBA += 2;
        RKWC = (RKWC + 1) & 0xFFFF;
    }
    sector++;
    if (sector > 013) {
        sector = 0;
        surface++;
        if (surface > 1) {
            surface = 0;
            cylinder++;
            if (cylinder > 0312) {
                rkerror(RKOVR);
            }
        }
    }
    if (RKWC == 0) {
        rkready();
        if (RKCS & (1 << 6)) {
            cpu.interrupt(INTRK, 5);
        }
    } else {
        goto again;
    }
}

void RK11::write16(uint32_t a, uint16_t v) {
    // printf("rkwrite: %06o\n",a);
    switch (a & 017) {
    case 000:
        break;
    case 002:
        break;
    case 004:
        RKBA = (RKBA & 0xFFFF) | ((v & 060) << 12);
        v &= 017517; // writable bits
        RKCS &= ~017517;
        RKCS |= v & ~1; // don't set GO bit
        if (v & 1) {
            switch ((RKCS & 017) >> 1) {
            case 0:
                reset();
                break;
            case 1:
            case 2:
                rknotready();
                step();
                break;
            case 5:
                // read check, not implemented
            case 7:
                // write lock, not implemented
                break;
            default:
                printf("unimplemented RK05 operation %06o\n",
                       ((RKCS & 017) >> 1));
                cpu.printstate();
                std::abort();
            }
        }
        break;
    case 006:
        RKWC = v;
        break;
    case 010:
        RKBA = (RKBA & 0x30000) | (v);
        break;
    case 012:
        drive = v >> 13;
        cylinder = (v >> 5) & 0377;
        surface = (v >> 4) & 1;
        sector = v & 15;
        break;
    default:
        printf("rkwrite16: invalid write");
        std::abort();
    }
}

void RK11::reset() {
    RKDS = 04700; // Set bits 6, 7, 8, 11
    RKER = 0;
    RKCS = 0200;
    RKWC = 0;
    RKBA = 0;
}
