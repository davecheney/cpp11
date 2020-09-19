#include "kw11.h"
#include "kb11.h"
#include <csignal>
#include <iostream>
#include <sys/time.h>
#include <unistd.h>

extern KB11 cpu;

void kw11alarm(int) {
    cpu.unibus.kw11.tick();
}

KW11::KW11() {
    signal(SIGALRM, kw11alarm);
    struct itimerval itv;
    itv.it_interval.tv_sec = 0;
    itv.it_interval.tv_usec = 20000;
    itv.it_value.tv_sec = 0;
    itv.it_value.tv_usec = 20000;
    setitimer(ITIMER_REAL, &itv, NULL);
}

void KW11::tick() {
    csr |= (1 << 7);
    if (csr & (1 << 6)) {
        cpu.interrupt(INTCLOCK, 6);
    }
}
