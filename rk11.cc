#include <stdint.h>
#include <stdio.h>

#include "avr11.h"

namespace rk11 {

FILE *rkdata;

uint32_t RKBA, RKDS, RKER, RKCS, RKWC;
uint32_t drive, sector, surface, cylinder;

uint16_t read16(uint32_t a) {
  switch (a) {
  case 0777400:
    return RKDS;
  case 0777402:
    return RKER;
  case 0777404:
    return RKCS | (RKBA & 0x30000) >> 12;
  case 0777406:
    return RKWC;
  case 0777410:
    return RKBA & 0xFFFF;
  case 0777412:
    return (sector) | (surface << 4) | (cylinder << 5) | (drive << 13);
  default:
    printf("rk11::read16 invalid read\n");
    panic();
    return 0; // not reached
  }
}

void rknotready() {
  RKDS &= ~(1 << 6);
  RKCS &= ~(1 << 7);
}

void rkready() {
  RKDS |= 1 << 6;
  RKCS |= 1 << 7;
}

void rkerror(uint16_t e) {
	printf("rk11: error %06o\n", e);
	panic();
}

void step() {
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
    panic();
    return; // unreached
  }

  if (DEBUG_RK05) {
    printf("rkstep: RKBA: %06o RKWC: %06o cylinder: %03o sector: %03o write: %s\n",
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
    panic();
  }

  uint16_t i;
  uint16_t val;
  for (i = 0; i < 256 && RKWC != 0; i++) {
    if (w) {
      val = unibus::read16(RKBA);
      fputc(val & 0xFF, rkdata);
      fputc((val >> 8) & 0xFF, rkdata);
    } else {
      unibus::write16(RKBA, fgetc(rkdata) | (fgetc(rkdata) << 8));
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
      cpu::interrupt(INTRK, 5);
    }
  } else {
    goto again;
  }
}

void write16(uint32_t a, uint16_t v) {
  // printf("rkwrite: %06o\n",a);
  switch (a) {
  case 0777400:
    break;
  case 0777402:
    break;
  case 0777404:
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
      default:
        printf("unimplemented RK05 operation %06o", ((RKCS & 017) >> 1));
        panic();
      }
    }
    break;
  case 0777406:
    RKWC = v;
    break;
  case 0777410:
    RKBA = (RKBA & 0x30000) | (v);
    break;
  case 0777412:
    drive = v >> 13;
    cylinder = (v >> 5) & 0377;
    surface = (v >> 4) & 1;
    sector = v & 15;
    break;
  default:
    printf("rkwrite16: invalid write");
    panic();
  }
}

void reset() {
  RKDS = (1 << 11) | (1 << 7) | (1 << 6);
  RKER = 0;
  RKCS = 1 << 7;
  RKWC = 0;
  RKBA = 0;
}
};
