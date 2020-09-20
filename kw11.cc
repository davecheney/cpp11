#include "kw11.h"
#include "avr11.h"
#include "kb11.h"
#include <csignal>
#include <iostream>
#include <sys/time.h>
#include <unistd.h>

extern KB11 cpu;

void kw11alarm(int) { cpu.unibus.kw11.tick(); }

KW11::KW11() {
    signal(SIGALRM, kw11alarm);
    struct itimerval itv = {
        .it_interval =
            {
                .tv_sec = 0,
                .tv_usec = 20000LL,
            },
        .it_value =
            {
                .tv_sec = 0,
                .tv_usec = 20000LL,
            },
    };
    setitimer(ITIMER_REAL, &itv, NULL);
}

void KW11::write16(uint32_t a, uint16_t v) {
    switch (a) {
    case 0777546:
        printf("kw11: csr write %06o\n", v);
        csr = v;
        return;
    default:
        printf("kw11: write to invalid address %06o\n", a);
        trap(INTBUS);
    }
}

uint16_t KW11::read16(uint32_t a) {
    switch (a) {
    case 0777546:
        printf("kw11: csr read %06o\n", csr);
        return csr;
    default:
        printf("kw11: read from invalid address %06o\n", a);
        trap(INTBUS);
    }
}

void KW11::tick() {
    csr |= (1 << 7);
    if (csr & (1 << 6)) {
        cpu.interrupt(INTCLOCK, 6);
    }
}
