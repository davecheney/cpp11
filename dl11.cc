#include <stdint.h>
#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>
#include <cstdlib>

#include "dl11.h"
#include "avr11.h"
#include "kb11.h"

extern KB11 cpu;

void DL11::addchar(char c) {
  switch (c) {
  case 42:
    tkb = 4;
    break;
  case 19:
    tkb = 034;
    break;
  // case 46:
  //	TKB = 127;
  default:
    tkb = c;
    break;
  }
  tks |= 0x80;
  if (tks & (1 << 6)) {
    cpu.interrupt(INTTTYIN, 4);
  }
}

uint8_t count;

int is_key_pressed(void) {
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    return FD_ISSET(STDIN_FILENO, &fds);
}

void DL11::clearterminal() {
  tks = 0;
  tps = 1 << 7;
  tkb = 0;
  tpb = 0;
}

void DL11::poll() {
  if (is_key_pressed())
    addchar(fgetc(stdin));

  if ((tps & 0x80) == 0) {
    if (++count > 32) {
      fputc(tpb & 0x7f, stderr);
      tps |= 0x80;
      if (tps & (1 << 6)) {
        cpu.interrupt(INTTTYOUT, 4);
      }
    }
  }
}

uint16_t DL11::read16(uint32_t a) {
  switch (a) {
  case 0777560:
    return tks;
  case 0777562:
    if (tks & 0x80) {
      tks &= 0xff7e;
      return tkb;
    }
    return 0;
  case 0777564:
    return tps;
  case 0777566:
    return 0;
  default:
    printf("consread16: read from invalid address %06o\n", a);
    std::abort();
  }
}

void DL11::write16(uint32_t a, uint16_t v) {
  switch (a) {
  case 0777560:
    if (v & (1 << 6)) {
      tks |= 1 << 6;
    } else {
      tks &= ~(1 << 6);
    }
    break;
  case 0777564:
    if (v & (1 << 6)) {
      tps |= 1 << 6;
    } else {
      tps &= ~(1 << 6);
    }
    break;
  case 0777566:
    tpb = v & 0xff;
    tps &= 0xff7f;
    count = 0;
    break;
  default:
    printf("conswrite16: write to invalid address %06o\n", a);
    std::abort();
  }
}
