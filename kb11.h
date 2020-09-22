#pragma once
#include "kt11.h"
#include "unibus.h"
#include <array>
#include <stdint.h>

enum { FLAGN = 8, FLAGZ = 4, FLAGV = 2, FLAGC = 1 };

// interrupts
enum {
    INTBUS = 0004,
    INTINVAL = 0010,
    INTDEBUG = 0014,
    INTIOT = 0020,
    INTTTYIN = 0060,
    INTTTYOUT = 0064,
    INTFAULT = 0250,
    INTCLOCK = 0100,
    INTRK = 0220
};

class KB11 {
  public:
    void step();
    void reset();

    void trapat(uint16_t vec);

    // interrupt schedules an interrupt.
    void interrupt(uint8_t vec, uint8_t pri);
    void printstate();

    // mode returns the current cpu mode.
    // 0: kernel, 1: supervisor, 2: illegal, 3: user
    uint16_t currentmode();

    // previousmode returns the previous cpu mode.
    // 0: kernel, 1: supervisor, 2: illegal, 3: user
    uint16_t previousmode();

    // returns the current CPU interrupt priority.
    uint16_t priority();

    // pop the top interrupt off the itab.
    void popirq();

    struct intr {
        uint8_t vec;
        uint8_t pri;
    };

    std::array<intr, 8> itab;

    KT11 mmu;
    UNIBUS unibus;

  private:
    std::array<uint16_t, 8> R; // R0-R7
    uint16_t PC;               // holds R[7] during instruction execution
    uint16_t PSW;              // processor status word
    uint16_t stacklimit, switchregister;
    std::array<uint16_t, 4>
        stackpointer; // Alternate R6 (kernel, super, illegal, user)

    inline bool N() { return PSW & FLAGN; }
    inline bool Z() { return PSW & FLAGZ; }
    inline bool V() { return PSW & FLAGV; }
    inline bool C() { return PSW & FLAGC; }
    void setZ(const bool b);

    inline uint16_t fetch16();
    inline uint16_t read16(uint16_t va);
    inline void write16(uint16_t va, uint16_t v);
    void push(const uint16_t v);
    uint16_t pop();

    inline uint16_t SA(uint16_t instr) {
        auto reg = (instr >> 6) & 077;
        auto len = (2 - (instr >> 15));
        return aget(reg, len);
    }

    inline uint16_t DA(uint16_t instr) {
        auto reg = instr & 077;
        auto len = (2 - (instr >> 15));
        return aget(reg, len);
    }

    uint16_t aget(uint8_t v, uint8_t l);
    void branch(int16_t o);

    // kernelmode pushes the current processor mode and switchs to kernel.
    void kernelmode();
    void writePSW(uint16_t psw);

    inline bool isReg(const uint16_t a) { return (a & 0177770) == 0170000; }

    template <uint8_t l> inline uint16_t read(const uint16_t va) {
        if (l == 2) {
            return read16(va);
        }
        if (va & 1) {
            return read16(va & ~1) >> 8;
        }
        return read16(va & ~1) & 0xFF;
    }

    template <uint8_t l> inline void write(const uint16_t va, uint16_t v) {
        if (l == 2) {
            write16(va, v);
            return;
        }
        if (va & 1) {
            write16(va & ~1, (read16(va & ~1) & 0xFF) | (v & 0xFF) << 8);
        } else {
            write16(va, (read16(va) & 0xFF00) | (v & 0xFF));
        }
    }

    template <uint8_t l> uint16_t memread(uint16_t a) {
        if (isReg(a)) {
            if (l == 2) {
                return R[a & 7];
            } else {
                return R[a & 7] & 0xFF;
            }
        }
        return read<l>(a);
    }

    template <uint8_t l> void memwrite(uint16_t a, uint16_t v) {
        if (isReg(a)) {
            const uint8_t r = a & 7;
            if (l == 2) {
                R[r] = v;
            } else {
                R[r] &= 0xFF00;
                R[r] |= v;
            }
            return;
        }
        write<l>(a, v);
    }

    template <uint8_t l> void MOV(const uint16_t instr) {
        const uint16_t msb = l == 2 ? 0x8000 : 0x80;
        uint16_t uval = memread<l>(SA(instr));
        const uint16_t da = DA(instr);
        PSW &= 0xFFF1;
        if (uval & msb) {
            PSW |= FLAGN;
        }
        setZ(uval == 0);
        if ((isReg(da)) && (l == 1)) {
            if (uval & msb) {
                uval |= 0xFF00;
            }
            memwrite<2>(da, uval);
            return;
        }
        memwrite<l>(da, uval);
    }

    // CMP 02SSDD, CMPB 12SSDD
    template <uint8_t l> void CMP(const uint16_t instr) {
        const uint16_t msb = l == 2 ? 0x8000 : 0x80;
        const uint16_t max = l == 2 ? 0xFFFF : 0xff;
        const uint16_t val1 = memread<l>(SA(instr));
        const uint16_t da = DA(instr);
        const uint16_t val2 = memread<l>(da);
        const int32_t sval = (val1 - val2) & max;
        PSW &= 0xFFF0;
        setZ(sval == 0);
        if (sval & msb) {
            PSW |= FLAGN;
        }
        if (((val1 ^ val2) & msb) && (!((val2 ^ sval) & msb))) {
            PSW |= FLAGV;
        }
        if (val1 < val2) {
            PSW |= FLAGC;
        }
    }

    template <uint8_t l> void BIT(const uint16_t instr) {
        const uint16_t msb = l == 2 ? 0x8000 : 0x80;
        const uint16_t val1 = memread<l>(SA(instr));
        const uint16_t da = DA(instr);
        const uint16_t val2 = memread<l>(da);
        const uint16_t uval = val1 & val2;
        PSW &= 0xFFF1;
        setZ(uval == 0);
        if (uval & msb) {
            PSW |= FLAGN;
        }
    }

    template <uint8_t l> void BIC(const uint16_t instr) {
        const uint16_t msb = l == 2 ? 0x8000 : 0x80;
        const uint16_t max = l == 2 ? 0xFFFF : 0xff;
        const uint16_t val1 = memread<l>(SA(instr));
        const uint16_t da = DA(instr);
        const uint16_t val2 = memread<l>(da);
        const uint16_t uval = (max ^ val1) & val2;
        PSW &= 0xFFF1;
        setZ(uval == 0);
        if (uval & msb) {
            PSW |= FLAGN;
        }
        memwrite<l>(da, uval);
    }

    template <uint8_t l> void BIS(const uint16_t instr) {
        const uint16_t msb = l == 2 ? 0x8000 : 0x80;
        const uint16_t val1 = memread<l>(SA(instr));
        const uint16_t da = DA(instr);
        const uint16_t val2 = memread<l>(da);
        const uint16_t uval = val1 | val2;
        PSW &= 0xFFF1;
        setZ(uval == 0);
        if (uval & msb) {
            PSW |= FLAGN;
        }
        memwrite<l>(da, uval);
    }

    template <uint8_t l> void CLR(const uint16_t instr) {
        PSW &= 0xFFF0;
        PSW |= FLAGZ;
        const uint16_t da = DA(instr);
        memwrite<l>(da, 0);
    }

    template <uint8_t l> void COM(const uint16_t instr) {
        uint16_t msb = l == 2 ? 0x8000 : 0x80;
        uint16_t max = l == 2 ? 0xFFFF : 0xff;
        uint16_t da = DA(instr);
        uint16_t uval = memread<l>(da) ^ max;
        PSW &= 0xFFF0;
        PSW |= FLAGC;
        if (uval & msb) {
            PSW |= FLAGN;
        }
        setZ(uval == 0);
        memwrite<l>(da, uval);
    }

    template <uint8_t l> void INC(const uint16_t instr) {
        const uint16_t msb = l == 2 ? 0x8000 : 0x80;
        const uint16_t max = l == 2 ? 0xFFFF : 0xff;
        const uint16_t da = DA(instr);
        const uint16_t uval = (memread<l>(da) + 1) & max;
        PSW &= 0xFFF1;
        if (uval & msb) {
            PSW |= FLAGN | FLAGV;
        }
        setZ(uval == 0);
        memwrite<l>(da, uval);
    }

    template <uint8_t l> void _DEC(const uint16_t instr) {
        uint16_t msb = l == 2 ? 0x8000 : 0x80;
        uint16_t max = l == 2 ? 0xFFFF : 0xff;
        uint16_t maxp = l == 2 ? 0x7FFF : 0x7f;
        uint16_t da = DA(instr);
        uint16_t uval = (memread<l>(da) - 1) & max;
        PSW &= 0xFFF1;
        if (uval & msb) {
            PSW |= FLAGN;
        }
        if (uval == maxp) {
            PSW |= FLAGV;
        }
        setZ(uval == 0);
        memwrite<l>(da, uval);
    }

    template <uint8_t l> void NEG(const uint16_t instr) {
        uint16_t msb = l == 2 ? 0x8000 : 0x80;
        uint16_t max = l == 2 ? 0xFFFF : 0xff;
        uint16_t da = DA(instr);
        int32_t sval = (-memread<l>(da)) & max;
        PSW &= 0xFFF0;
        if (sval & msb) {
            PSW |= FLAGN;
        }
        if (sval == 0) {
            PSW |= FLAGZ;
        } else {
            PSW |= FLAGC;
        }
        if (sval == 0x8000) {
            PSW |= FLAGV;
        }
        memwrite<l>(da, sval);
    }

    template <uint8_t l> void _ADC(const uint16_t instr) {
        uint16_t msb = l == 2 ? 0x8000 : 0x80;
        uint16_t max = l == 2 ? 0xFFFF : 0xff;
        uint16_t da = DA(instr);
        uint16_t uval = memread<l>(da);
        if (PSW & FLAGC) {
            PSW &= 0xFFF0;
            if ((uval + 1) & msb) {
                PSW |= FLAGN;
            }
            setZ(uval == max);
            if (uval == 0077777) {
                PSW |= FLAGV;
            }
            if (uval == 0177777) {
                PSW |= FLAGC;
            }
            memwrite<l>(da, (uval + 1) & max);
        } else {
            PSW &= 0xFFF0;
            if (uval & msb) {
                PSW |= FLAGN;
            }
            setZ(uval == 0);
        }
    }

    template <uint8_t l> void SBC(const uint16_t instr) {
        uint16_t msb = l == 2 ? 0x8000 : 0x80;
        uint16_t max = l == 2 ? 0xFFFF : 0xff;
        uint16_t da = DA(instr);
        int32_t sval = memread<l>(da);
        if (PSW & FLAGC) {
            PSW &= 0xFFF0;
            if ((sval - 1) & msb) {
                PSW |= FLAGN;
            }
            setZ(sval == 1);
            if (sval) {
                PSW |= FLAGC;
            }
            if (sval == 0100000) {
                PSW |= FLAGV;
            }
            memwrite<l>(da, (sval - 1) & max);
        } else {
            PSW &= 0xFFF0;
            if (sval & msb) {
                PSW |= FLAGN;
            }
            setZ(sval == 0);
            if (sval == 0100000) {
                PSW |= FLAGV;
            }
            PSW |= FLAGC;
        }
    }

    // TST 0057DD, TSTB 1057DD
    template <uint8_t l> void TST(const uint16_t instr) {
        uint16_t msb = l == 2 ? 0x8000 : 0x80;
        uint16_t uval = memread<l>(DA(instr));
        PSW &= 0xFFF0;
        if (uval & msb) {
            PSW |= FLAGN;
        }
        setZ(uval == 0);
    }

    template <uint8_t l> void ROR(const uint16_t instr) {
        int32_t max = l == 2 ? 0xFFFF : 0xff;
        uint16_t da = DA(instr);
        int32_t sval = memread<l>(da);
        if (PSW & FLAGC) {
            sval |= max + 1;
        }
        PSW &= 0xFFF0;
        if (sval & 1) {
            PSW |= FLAGC;
        }
        // watch out for integer wrap around
        if (sval & (max + 1)) {
            PSW |= FLAGN;
        }
        setZ(!(sval & max));
        if ((sval & 1) xor (sval & (max + 1))) {
            PSW |= FLAGV;
        }
        sval >>= 1;
        memwrite<l>(da, sval);
    }

    template <uint8_t l> void ROL(const uint16_t instr) {
        uint16_t msb = l == 2 ? 0x8000 : 0x80;
        int32_t max = l == 2 ? 0xFFFF : 0xff;
        uint16_t da = DA(instr);
        int32_t sval = memread<l>(da) << 1;
        if (PSW & FLAGC) {
            sval |= 1;
        }
        PSW &= 0xFFF0;
        if (sval & (max + 1)) {
            PSW |= FLAGC;
        }
        if (sval & msb) {
            PSW |= FLAGN;
        }
        setZ(!(sval & max));
        if ((sval ^ (sval >> 1)) & msb) {
            PSW |= FLAGV;
        }
        sval &= max;
        memwrite<l>(da, sval);
    }

    template <uint8_t l> void ASR(const uint16_t instr) {
        uint16_t msb = l == 2 ? 0x8000 : 0x80;
        uint16_t da = DA(instr);
        uint16_t uval = memread<l>(da);
        PSW &= 0xFFF0;
        if (uval & 1) {
            PSW |= FLAGC;
        }
        if (uval & msb) {
            PSW |= FLAGN;
        }
        if ((uval & msb) xor (uval & 1)) {
            PSW |= FLAGV;
        }
        uval = (uval & msb) | (uval >> 1);
        setZ(uval == 0);
        memwrite<l>(da, uval);
    }

    template <uint8_t l> void ASL(const uint16_t instr) {
        uint16_t msb = l == 2 ? 0x8000 : 0x80;
        uint16_t max = l == 2 ? 0xFFFF : 0xff;
        uint16_t da = DA(instr);
        // TODO(dfc) doesn't need to be an sval
        int32_t sval = memread<l>(da);
        PSW &= 0xFFF0;
        if (sval & msb) {
            PSW |= FLAGC;
        }
        if (sval & (msb >> 1)) {
            PSW |= FLAGN;
        }
        if ((sval ^ (sval << 1)) & msb) {
            PSW |= FLAGV;
        }
        sval = (sval << 1) & max;
        setZ(sval == 0);
        memwrite<l>(da, sval);
    }

    template <uint8_t l> void SXT(const uint16_t instr) {
        const uint16_t da = DA(instr);
        if (PSW & FLAGN) {
            const uint16_t max = l == 2 ? 0xFFFF : 0xff;
            memwrite<l>(da, max);
        } else {
            PSW |= FLAGZ;
            memwrite<l>(da, 0);
        }
    }

    template <uint8_t l> void SWAB(const uint16_t instr) {
        uint16_t da = DA(instr);
        uint16_t uval = memread<l>(da);
        uval = ((uval >> 8) | (uval << 8)) & 0xFFFF;
        PSW &= 0xFFF0;
        setZ(uval & 0xFF);
        if (uval & 0x80) {
            PSW |= FLAGN;
        }
        memwrite<l>(da, uval);
    }

    void ADD(const uint16_t instr);
    void SUB(const uint16_t instr);
    void JSR(const uint16_t instr);
    void MUL(const uint16_t instr);
    void DIV(const uint16_t instr);
    void ASH(const uint16_t instr);
    void ASHC(const uint16_t instr);
    void XOR(const uint16_t instr);
    void SOB(const uint16_t instr);
    void JMP(const uint16_t instr);
    void MARK(const uint16_t instr);
    void MFPI(const uint16_t instr);
    void MFPT();
    void MTPI(const uint16_t instr);
    void RTS(const uint16_t instr);
    void EMTX(const uint16_t instr);
    void RTT();
    void RESET();
};