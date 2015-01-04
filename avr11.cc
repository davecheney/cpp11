#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#include "avr11.h"

void uart_init(void);

void setup() {
  uart_init();

  printf("Reset\n");
  cpu::reset();
  printf("Ready\n");
}

uint16_t clkcounter;
uint16_t instcounter;

jmp_buf trapbuf;

void loop0();

uint16_t trap(uint16_t vec) {
  longjmp(trapbuf, INTBUS);
  return vec; // not reached
}

void loop() {
  uint16_t vec = setjmp(trapbuf);
  if (vec == 0) {
    loop0();
  } else {
    cpu::trapat(vec);
  }
}

void loop0() {
  while (true) {
    if ((itab[0].vec > 0) && (itab[0].pri >= ((cpu::PS >> 5) & 7))) {
      cpu::handleinterrupt();
      uint8_t i;
      for (i = 0; i < ITABN - 1; i++) {
        itab[i] = itab[i + 1];
      }
      itab[ITABN - 1].vec = 0;
      itab[ITABN - 1].pri = 0;
      return; // exit from loop to reset trapbuf
    }
    cpu::step();
    if (++clkcounter > 39999) {
      clkcounter = 0;
      cpu::LKS |= (1 << 7);
      if (cpu::LKS & (1 << 6)) {
        cpu::interrupt(INTCLOCK, 6);
      }
    }
    cons::poll();
  }
}

int main() {
  setup();
  while (1)
    loop();
}

void panic() {
  printstate();
  abort();
}
