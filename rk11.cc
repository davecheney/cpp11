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
        // 777400 Drive Status
        return rkds;
    case 002:
        // 777402 Error Register
        return rker;
    case 004:
        // 777404 Control Status
        return rkcs & 0xfffe; // go bit is read only
    default:
        printf("rk11::read16 invalid read %06o\n", a);
        std::abort();
    }
}

void RK11::rknotready() {
    rkds &= ~(1 << 6);
    rkcs &= ~(1 << 7);
}

void RK11::rkready() {
    rkds |= 1 << 6;
    rkcs |= 1 << 7;
    rkcs &= ~1; // no go
}

void RK11::step() {
    if ((rkcs & 01) == 0) {
        // no GO bit
        return;
    }

    switch ((rkcs >> 1) & 7) {
    case 0:
        // controller reset
        reset();
        break;
    case 1: // write
    case 2: // read
    case 3: // check
        if (drive != 0) {
            rker |= 0x8080; // NXD
            break;
        }
        if (cylinder > 0312) {
            rker |= 0x8040; // NXC
            break;
        }
        if (sector > 013) {
            rker |= 0x8020; // NXS
            break;
        }
        seek();
        readwrite();
        return;
    case 6: // Drive Reset - falls through to be finished as a seek
        rker = 0;
    case 4: // Seek (and drive reset) - complete immediately
        printf("rk11: seek: cylinder: %03o sector: %03o\n", cylinder, sector);
        seek();
        rkcs &= ~0x2000; // Clear search complete - reset by rk11_seekEnd
        rkcs |= 0x80;    // set done - ready to accept new command
        cpu.interrupt(INTRK, 5);
        break;
    case 5: // Read Check
        break;
    case 7: // Write Lock - not implemented :-(
        break;
    default:
        printf("unimplemented RK05 operation %06o\n", ((rkcs & 017) >> 1));
        std::abort();
    }
    
}

void RK11::readwrite() {
    if (rkwc == 0) {
        rkready();
        if (rkcs & (1 << 6)) {
            cpu.interrupt(INTRK, 5);
        }
        return;
    }

    bool w = ((rkcs >> 1) & 7) == 1;
    if (0) {
        printf("rk11: step: RKCS: %06o RKBA: %06o RKWC: %06o cylinder: %03o sector: %03o "
               "write: %x\n",
               rkcs, rkba, rkwc, cylinder, sector, w);
    }

    for (uint16_t i = 0; i < 256 && rkwc != 0; i++) {
        if (w) {
            auto val = cpu.unibus.read16(rkba);
            uint8_t buf[2] = {static_cast<uint8_t>(val & 0xff),
                              static_cast<uint8_t>(val >> 8)};
            assert(fwrite(&buf, 1, 2, rkdata) == 2);
        } else {
            uint8_t buf[2];
            assert(fread(&buf, 1, 2, rkdata) == 2);
            cpu.unibus.write16(rkba, static_cast<uint16_t>(buf[0]) |
                                         static_cast<uint16_t>(buf[1]) << 8);
        }
        rkba += 2;
        rkwc++;
    }
    sector++;
    if (sector > 013) {
        sector = 0;
        surface++;
        if (surface > 1) {
            surface = 0;
            cylinder++;
            if (cylinder > 0312) {
                rker |= RKOVR;
                return;
            }
        }
    }


}

void RK11::seek() {
    uint32_t pos = (cylinder * 24 + surface * 12 + sector) * 512;
    if (fseek(rkdata, pos, SEEK_SET)) {
        printf("rkstep: failed to seek\n");
        std::abort();
    }
}

void RK11::write16(uint32_t a, uint16_t v) {
    switch (a & 017) {
    case 004:
        rkcs =
            (v & ~0xf080) | (rkcs & 0xf080); // Bits 7 and 12 - 15 are read only
        break;
    case 006:
        rkwc = v;
        break;
    case 010:
        rkba = v;
        break;
    case 012:
        drive = v >> 13;
        cylinder = (v >> 5) & 0377;
        surface = (v >> 4) & 1;
        sector = v & 15;
        break;
    default:
        printf("rk11::write16 invalid write %06o: %06o\n", a, v);
        std::abort();
    }
}

void RK11::reset() {
    rkds = 04700; // Set bits 6, 7, 8, 11
    rker = 0;
    rkcs = 0200;
    rkwc = 0;
    rkba = 0;
    drive = cylinder = surface = sector = 0;
}
