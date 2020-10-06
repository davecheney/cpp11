#include <cstdlib>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "kb11.h"
#include "kl11.h"

extern KB11 cpu;

bool keypressed = false;

static void sigioHandler(int) { keypressed = true; }

KL11::KL11() {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = sigioHandler;
    if (sigaction(SIGIO, &sa, NULL) == -1)
        perror("sigaction");

    /* Set owner process that is to receive "I/O possible" signal */

    if (fcntl(STDIN_FILENO, F_SETOWN, getpid()) == -1)
        perror("fcntl(F_SETOWN)");

    /* Enable "I/O possible" signaling and make I/O nonblocking
       for file descriptor */

    auto flags = fcntl(STDIN_FILENO, F_GETFL);
    if (fcntl(STDIN_FILENO, F_SETFL, flags | O_ASYNC | O_NONBLOCK) == -1)
        perror("fcntl(F_SETFL)");
}

void KL11::clearterminal() {
    rcsr = 0;
    xcsr = 0x80;
    rbuf = 0;
    xbuf = 0;
    count = 0;
}

int getchar() {
    char ch;
    while (read(STDIN_FILENO, &ch, 1) > 0) {
        printf("read %06o\n", ch);
        break;
    }
    return ch;
}

void KL11::poll() {
    if (!rcvrdone()) {
        // unit not busy
        if (keypressed) {
            char ch;
            if (read(STDIN_FILENO, &ch, 1) > 0) {
                rbuf = ch & 0x7f;
                rcsr |= 0x80;
                if (rcsr & 0x40) {
                    cpu.interrupt(INTTTYIN, 4);
                }
            } else {
                keypressed = false;
            }
        }
    }

    if (xbuf) {
        write(STDERR_FILENO, &xbuf, 1);
	xbuf = 0;
        count = 32;
    }

    if (!xmitready() && (--count == 0)) {
        xcsr |= 0x80;
        if (xcsr & 0x40) {
            cpu.interrupt(INTTTYOUT, 4);
        }
    }
}

uint16_t KL11::read16(uint32_t a) {
    switch (a) {
    case 0777560:
        return rcsr;
    case 0777562:
        rcsr &= ~0x80;
        return rbuf;
    case 0777564:
        return xcsr;
    case 0777566:
        return 0;
    default:
        printf("kl11: read from invalid address %06o\n", a);
        std::abort();
    }
}

void KL11::write16(uint32_t a, uint16_t v) {
    switch (a) {
    case 0777560:
        rcsr |= v & (0x40);
        break;
    case 0777562:
        rcsr &= ~0x80;
        break;
    case 0777564:
        // printf("kl11:write16: %06o %06o\n", a, v);
        xcsr |= v & (0x40);
        break;
    case 0777566:
        xbuf = v & 0x7f;
        xcsr &= ~0x80;
        break;
    default:
        printf("kl11: write to invalid address %06o\n", a);
        std::abort();
    }
}
