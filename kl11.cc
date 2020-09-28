#include <cstdlib>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/select.h>
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

void KL11::addchar(char c) {
    if (!(rcsr & 0x80)) {
        // unit not busy
        rbuf = c & 0x7f;
        rcsr |= 0x80;
        if (rcsr & 0x40) {
            cpu.interrupt(INTTTYIN, 4);
        }
    }
}

void KL11::clearterminal() {
    rcsr = 0;
    xcsr = 0x80;
    rbuf = 0;
    xbuf = 0;
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
    if (!(rcsr & 0x80)) {
        // unit not busy
        if (keypressed) {
            char ch;
            if (read(STDIN_FILENO, &ch, 1) > 0) {
                rbuf = ch & 0x7f;
                rcsr |= 0x80;
                if (rcsr & 0x40) {
                    cpu.interrupt(INTTTYIN, 4);
                }
                keypressed = false;
            }
        }
    }

    if ((xcsr & 0x80) == 0) {
        if (++count > 32) {
            fputc(xbuf & 0x7f, stderr);
            xcsr |= 0x80;
            if (xcsr & (1 << 6)) {
                cpu.interrupt(INTTTYOUT, 4);
            }
        }
    }
}

uint16_t KL11::read16(uint32_t a) {
    switch (a & 06) {
    case 00:
        return rcsr;
    case 02:
        if (rcsr & 0x80) {
            rcsr &= 0xff7e;
            return rbuf;
        }
        return 0;
    case 04:
        return xcsr;
    case 06:
        return 0;
    default:
        printf("consread16: read from invalid address %06o\n", a);
        std::abort();
    }
}

void KL11::write16(uint32_t a, uint16_t v) {
    switch (a & 06) {
    case 00:
        if (v & (1 << 6)) {
            rcsr |= 1 << 6;
        } else {
            rcsr &= ~(1 << 6);
        }
        break;
    case 04:
        if (v & (1 << 6)) {
            xcsr |= 1 << 6;
        } else {
            xcsr &= ~(1 << 6);
        }
        break;
    case 06:
        xbuf = v & 0x7f;
        xcsr &= 0xff7f;
        count = 0;
        break;
    default:
        printf("conswrite16: write to invalid address %06o\n", a);
        std::abort();
    }
}
