#include <stdint.h>

#define D(x) (x & 077)
#define S(x) ((x & 07700) >> 6)
#define L(x) (2 - (x >> 15))
#define SA(x) (aget(S(x), L(x)))
#define DA(x) (aget(D(x), L(x)))

class KB11 {
  public:
    uint16_t PC;
    uint16_t PS;
    uint16_t USP;
    uint16_t KSP;
    uint16_t LKS;
    bool curuser;
    bool prevuser;

    void step();
    void reset();

    void trapat(uint16_t vec);
    void interrupt(uint8_t vec, uint8_t pri);
    void handleinterrupt();
    void switchmode(const bool newm);

    uint16_t R[8];

    pdp11::intr itab[ITABN];

  private:
    bool N();
    bool Z();
    void setZ(const bool b);
    bool V();
    bool C();

    uint16_t fetch16();
    void push(const uint16_t v);
    uint16_t pop();
    uint16_t aget(uint8_t v, uint8_t l);
    void branch(int16_t o);
    void popirq();

    template <bool wr> inline uint16_t access(uint16_t addr, uint16_t v = 0) {
        return unibus::access<wr>(mmu::decode(addr, wr, curuser), v);
    }

    inline bool isReg(const uint16_t a) { return (a & 0177770) == 0170000; }

    template <uint8_t l> uint16_t memread(uint16_t a) {
        if (isReg(a)) {
            if (l == 2) {
                return R[a & 7];
            } else {
                return R[a & 7] & 0xFF;
            }
        }
        return unibus::read<l>(mmu::decode(a, false, curuser));
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
        unibus::write<l>(mmu::decode(a, true, curuser), v);
    }

    template <uint8_t l> void MOV(const uint16_t instr) {
        const uint16_t msb = l == 2 ? 0x8000 : 0x80;
        uint16_t uval = memread<l>(aget(S(instr), l));
        const uint16_t da = DA(instr);
        PS &= 0xFFF1;
        if (uval & msb) {
            PS |= FLAGN;
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

    template <uint8_t l> void CMP(const uint16_t instr) {
        const uint16_t msb = l == 2 ? 0x8000 : 0x80;
        const uint16_t max = l == 2 ? 0xFFFF : 0xff;
        const uint16_t val1 = memread<l>(aget(S(instr), l));
        const uint16_t da = DA(instr);
        const uint16_t val2 = memread<l>(da);
        const int32_t sval = (val1 - val2) & max;
        PS &= 0xFFF0;
        setZ(sval == 0);
        if (sval & msb) {
            PS |= FLAGN;
        }
        if (((val1 ^ val2) & msb) && (!((val2 ^ sval) & msb))) {
            PS |= FLAGV;
        }
        if (val1 < val2) {
            PS |= FLAGC;
        }
    }

    template <uint8_t l> void BIT(const uint16_t instr) {
        const uint16_t msb = l == 2 ? 0x8000 : 0x80;
        const uint16_t val1 = memread<l>(SA(instr));
        const uint16_t da = DA(instr);
        const uint16_t val2 = memread<l>(da);
        const uint16_t uval = val1 & val2;
        PS &= 0xFFF1;
        setZ(uval == 0);
        if (uval & msb) {
            PS |= FLAGN;
        }
    }

    template <uint8_t l> void BIC(const uint16_t instr) {
        const uint16_t msb = l == 2 ? 0x8000 : 0x80;
        const uint16_t max = l == 2 ? 0xFFFF : 0xff;
        const uint16_t val1 = memread<l>(SA(instr));
        const uint16_t da = DA(instr);
        const uint16_t val2 = memread<l>(da);
        const uint16_t uval = (max ^ val1) & val2;
        PS &= 0xFFF1;
        setZ(uval == 0);
        if (uval & msb) {
            PS |= FLAGN;
        }
        memwrite<l>(da, uval);
    }

    template <uint8_t l> void BIS(const uint16_t instr) {
        const uint16_t msb = l == 2 ? 0x8000 : 0x80;
        const uint16_t val1 = memread<l>(SA(instr));
        const uint16_t da = DA(instr);
        const uint16_t val2 = memread<l>(da);
        const uint16_t uval = val1 | val2;
        PS &= 0xFFF1;
        setZ(uval == 0);
        if (uval & msb) {
            PS |= FLAGN;
        }
        memwrite<l>(da, uval);
    }

    template <uint8_t l> void CLR(const uint16_t instr) {
        PS &= 0xFFF0;
        PS |= FLAGZ;
        const uint16_t da = DA(instr);
        memwrite<l>(da, 0);
    }

    template <uint8_t l> void COM(const uint16_t instr) {
        uint16_t msb = l == 2 ? 0x8000 : 0x80;
        uint16_t max = l == 2 ? 0xFFFF : 0xff;
        uint16_t da = DA(instr);
        uint16_t uval = memread<l>(da) ^ max;
        PS &= 0xFFF0;
        PS |= FLAGC;
        if (uval & msb) {
            PS |= FLAGN;
        }
        setZ(uval == 0);
        memwrite<l>(da, uval);
    }

    template <uint8_t l> void INC(const uint16_t instr) {
        const uint16_t msb = l == 2 ? 0x8000 : 0x80;
        const uint16_t max = l == 2 ? 0xFFFF : 0xff;
        const uint16_t da = DA(instr);
        const uint16_t uval = (memread<l>(da) + 1) & max;
        PS &= 0xFFF1;
        if (uval & msb) {
            PS |= FLAGN | FLAGV;
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
        PS &= 0xFFF1;
        if (uval & msb) {
            PS |= FLAGN;
        }
        if (uval == maxp) {
            PS |= FLAGV;
        }
        setZ(uval == 0);
        memwrite<l>(da, uval);
    }

    template <uint8_t l> void NEG(const uint16_t instr) {
        uint16_t msb = l == 2 ? 0x8000 : 0x80;
        uint16_t max = l == 2 ? 0xFFFF : 0xff;
        uint16_t da = DA(instr);
        int32_t sval = (-memread<l>(da)) & max;
        PS &= 0xFFF0;
        if (sval & msb) {
            PS |= FLAGN;
        }
        if (sval == 0) {
            PS |= FLAGZ;
        } else {
            PS |= FLAGC;
        }
        if (sval == 0x8000) {
            PS |= FLAGV;
        }
        memwrite<l>(da, sval);
    }

    template <uint8_t l> void _ADC(const uint16_t instr) {
        uint16_t msb = l == 2 ? 0x8000 : 0x80;
        uint16_t max = l == 2 ? 0xFFFF : 0xff;
        uint16_t da = DA(instr);
        uint16_t uval = memread<l>(da);
        if (PS & FLAGC) {
            PS &= 0xFFF0;
            if ((uval + 1) & msb) {
                PS |= FLAGN;
            }
            setZ(uval == max);
            if (uval == 0077777) {
                PS |= FLAGV;
            }
            if (uval == 0177777) {
                PS |= FLAGC;
            }
            memwrite<l>(da, (uval + 1) & max);
        } else {
            PS &= 0xFFF0;
            if (uval & msb) {
                PS |= FLAGN;
            }
            setZ(uval == 0);
        }
    }

    template <uint8_t l> void SBC(const uint16_t instr) {
        uint16_t msb = l == 2 ? 0x8000 : 0x80;
        uint16_t max = l == 2 ? 0xFFFF : 0xff;
        uint16_t da = DA(instr);
        int32_t sval = memread<l>(da);
        if (PS & FLAGC) {
            PS &= 0xFFF0;
            if ((sval - 1) & msb) {
                PS |= FLAGN;
            }
            setZ(sval == 1);
            if (sval) {
                PS |= FLAGC;
            }
            if (sval == 0100000) {
                PS |= FLAGV;
            }
            memwrite<l>(da, (sval - 1) & max);
        } else {
            PS &= 0xFFF0;
            if (sval & msb) {
                PS |= FLAGN;
            }
            setZ(sval == 0);
            if (sval == 0100000) {
                PS |= FLAGV;
            }
            PS |= FLAGC;
        }
    }

    template <uint8_t l> void TST(const uint16_t instr) {
        uint16_t msb = l == 2 ? 0x8000 : 0x80;
        uint16_t uval = memread<l>(aget(D(instr), l));
        PS &= 0xFFF0;
        if (uval & msb) {
            PS |= FLAGN;
        }
        setZ(uval == 0);
    }

    template <uint8_t l> void ROR(const uint16_t instr) {
        int32_t max = l == 2 ? 0xFFFF : 0xff;
        uint16_t da = DA(instr);
        int32_t sval = memread<l>(da);
        if (PS & FLAGC) {
            sval |= max + 1;
        }
        PS &= 0xFFF0;
        if (sval & 1) {
            PS |= FLAGC;
        }
        // watch out for integer wrap around
        if (sval & (max + 1)) {
            PS |= FLAGN;
        }
        setZ(!(sval & max));
        if ((sval & 1) xor (sval & (max + 1))) {
            PS |= FLAGV;
        }
        sval >>= 1;
        memwrite<l>(da, sval);
    }

    template <uint8_t l> void ROL(const uint16_t instr) {
        uint16_t msb = l == 2 ? 0x8000 : 0x80;
        int32_t max = l == 2 ? 0xFFFF : 0xff;
        uint16_t da = DA(instr);
        int32_t sval = memread<l>(da) << 1;
        if (PS & FLAGC) {
            sval |= 1;
        }
        PS &= 0xFFF0;
        if (sval & (max + 1)) {
            PS |= FLAGC;
        }
        if (sval & msb) {
            PS |= FLAGN;
        }
        setZ(!(sval & max));
        if ((sval ^ (sval >> 1)) & msb) {
            PS |= FLAGV;
        }
        sval &= max;
        memwrite<l>(da, sval);
    }

    template <uint8_t l> void ASR(const uint16_t instr) {
        uint16_t msb = l == 2 ? 0x8000 : 0x80;
        uint16_t da = DA(instr);
        uint16_t uval = memread<l>(da);
        PS &= 0xFFF0;
        if (uval & 1) {
            PS |= FLAGC;
        }
        if (uval & msb) {
            PS |= FLAGN;
        }
        if ((uval & msb) xor (uval & 1)) {
            PS |= FLAGV;
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
        PS &= 0xFFF0;
        if (sval & msb) {
            PS |= FLAGC;
        }
        if (sval & (msb >> 1)) {
            PS |= FLAGN;
        }
        if ((sval ^ (sval << 1)) & msb) {
            PS |= FLAGV;
        }
        sval = (sval << 1) & max;
        setZ(sval == 0);
        memwrite<l>(da, sval);
    }

    template <uint8_t l> void SXT(const uint16_t instr) {
        const uint16_t max = l == 2 ? 0xFFFF : 0xff;
        const uint16_t da = DA(instr);
        if (PS & FLAGN) {
            memwrite<l>(da, max);
        } else {
            PS |= FLAGZ;
            memwrite<l>(da, 0);
        }
    }

    template <uint8_t l> void SWAB(const uint16_t instr) {
        uint16_t da = DA(instr);
        uint16_t uval = memread<l>(da);
        uval = ((uval >> 8) | (uval << 8)) & 0xFFFF;
        PS &= 0xFFF0;
        setZ(uval & 0xFF);
        if (uval & 0x80) {
            PS |= FLAGN;
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
    void MTPI(const uint16_t instr);
    void RTS(const uint16_t instr);
    void EMTX(const uint16_t instr);
    void _RTT();
    void RESET();
};