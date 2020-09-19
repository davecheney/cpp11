#include <cstdlib>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "avr11.h"
#include "kb11.h"

KB11 cpu;

void setup(char * disk) {
    struct termios old_terminal_settings, new_terminal_settings;

    // Get the current terminal settings
    if (tcgetattr(0, &old_terminal_settings) < 0)
        perror("tcgetattr()");

    memcpy(&new_terminal_settings, &old_terminal_settings,
           sizeof(struct termios));

    // disable canonical mode processing in the line discipline driver
    new_terminal_settings.c_lflag &= ~ICANON;
    new_terminal_settings.c_lflag &= ~ECHO;

    // apply our new settings
    if (tcsetattr(0, TCSANOW, &new_terminal_settings) < 0)
        perror("tcsetattr ICANON");
    cpu.unibus.rk11.rkdata = fopen(disk, "r+");
    cpu.reset();
    printf("Ready\n");
}

jmp_buf trapbuf;

void loop0();

void trap(uint16_t vec) {
    longjmp(trapbuf, vec);
}

void loop() {
    auto vec = setjmp(trapbuf);
    if (vec == 0) {
        loop0();
    } else {
        cpu.trapat(vec);
    }
}

void loop0() {
    while (true) {
        if ((cpu.itab[0].vec > 0) && (cpu.itab[0].pri >= ((cpu.PS >> 5) & 7))) {
            cpu.handleinterrupt();
            uint8_t i;
            for (i = 0; i < cpu.itab.size() - 1; i++) {
                cpu.itab[i] = cpu.itab[i + 1];
            }
            cpu.itab[cpu.itab.size() - 1].vec = 0;
            cpu.itab[cpu.itab.size() - 1].pri = 0;
            return; // exit from loop to reset trapbuf
        }
        cpu.step();
        cpu.unibus.cons.poll();
    }
}

int main(int argc, char* argv[]) {
    assert(argc > 1);
    setup(argv[1]);
    while (1)
        loop();
}

void panic() {
    cpu.printstate();
    std::abort();
}
