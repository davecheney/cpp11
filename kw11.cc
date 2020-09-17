#include "kw11.h"
#include "kb11.h"

extern KB11 cpu;

void KW11::tick() {
    if (++clkcounter > 39999) {
            clkcounter = 0;
            csr |= (1 << 7);
            if (csr & (1 << 6)) {
                cpu.interrupt(INTCLOCK, 6);
            }
        }
}