#include <cstdlib>
#include <stdint.h>
#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>

#include "kb11.h"
#include "kl11.h"

extern KB11 cpu;

void KL11::addchar(char c) {
    if (!(rcsr & 0x80)) {
        // unit not busy
        rbuf = c;
        rcsr |= 0x80;
        if (rcsr & 0x40) {
            cpu.interrupt(INTTTYIN, 4);
        }
    }
}

int is_key_pressed(void) {
    timeval tv = {
        .tv_sec = 0,
        .tv_usec = 0,
    };
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    return FD_ISSET(STDIN_FILENO, &fds);
}

void KL11::clearterminal() {
    rcsr = 0;
    xcsr = 0x80;
    rbuf = 0;
    xbuf = 0;
}

void KL11::poll() {
    if (is_key_pressed())
        addchar(fgetc(stdin));

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
