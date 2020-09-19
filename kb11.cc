#include <cstdlib>
#include <sched.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>

#include "avr11.h"
#include "bootrom.h"
#include "kb11.h"

extern jmp_buf trapbuf;

extern UNIBUS unibus;

void KB11::reset() {
    uint16_t i;
    for (i = 0; i < 29; i++) {
        unibus.access<1>(02000 + (i * 2), bootrom[i]);
    }
    R[7] = 002002;
    unibus.reset();
}

bool KB11::N() { return PS & FLAGN; }
bool KB11::Z() { return PS & FLAGZ; }
bool KB11::V() { return PS & FLAGV; }
bool KB11::C() { return PS & FLAGC; }

template <bool wr> uint16_t KB11::access(uint16_t addr, uint16_t v) {
    return unibus.access<wr>(mmu.decode<wr>(addr, curuser), v);
}

inline uint16_t KB11::fetch16() {
    const uint16_t val = access<0>(R[7]);
    R[7] += 2;
    return val;
}

inline void KB11::push(const uint16_t v) {
    R[6] -= 2;
    access<1>(R[6], v);
}

inline uint16_t KB11::pop() {
    const uint16_t val = access<0>(R[6]);
    R[6] += 2;
    return val;
}

// aget resolves the operand to a vaddress.
// if the operand is a register, an address in
// the range [0170000,0170007). This address range is
// technically a valid IO page, but unibus doesn't map
// any addresses here, so we can safely do this.
uint16_t KB11::aget(uint8_t v, uint8_t l) {
    if ((v & 070) == 000) {
        return 0170000 | (v & 7);
    }
    if (((v & 7) >= 6) || (v & 010)) {
        l = 2;
    }
    uint16_t addr = 0;
    switch (v & 060) {
    case 000:
        v &= 7;
        addr = R[v & 7];
        break;
    case 020:
        addr = R[v & 7];
        R[v & 7] += l;
        break;
    case 040:
        R[v & 7] -= l;
        addr = R[v & 7];
        break;
    case 060:
        addr = fetch16();
        addr += R[v & 7];
        break;
    }
    if (v & 010) {
        addr = access<0>(addr);
    }
    return addr;
}

void KB11::branch(int16_t o) {
    if (o & 0x80) {
        o = -(((~o) + 1) & 0xFF);
    }
    o <<= 1;
    R[7] += o;
}

void KB11::setZ(const bool b) {
    if (b)
        PS |= FLAGZ;
}

void KB11::ADD(const uint16_t instr) {
    const uint16_t val1 = memread<2>(aget(S(instr), 2));
    const uint16_t da = aget(D(instr), 2);
    const uint16_t val2 = memread<2>(da);
    const uint16_t uval = (val1 + val2) & 0xFFFF;
    PS &= 0xFFF0;
    setZ(uval == 0);
    if (uval & 0x8000) {
        PS |= FLAGN;
    }
    if (!((val1 ^ val2) & 0x8000) && ((val2 ^ uval) & 0x8000)) {
        PS |= FLAGV;
    }
    if ((val1 + val2) >= 0xFFFF) {
        PS |= FLAGC;
    }
    memwrite<2>(da, uval);
}

void KB11::SUB(const uint16_t instr) {
    const uint16_t val1 = memread<2>(aget(S(instr), 2));
    const uint16_t da = aget(D(instr), 2);
    const uint16_t val2 = memread<2>(da);
    const uint16_t uval = (val2 - val1) & 0xFFFF;
    PS &= 0xFFF0;
    setZ(uval == 0);
    if (uval & 0x8000) {
        PS |= FLAGN;
    }
    if (((val1 ^ val2) & 0x8000) && (!((val2 ^ uval) & 0x8000))) {
        PS |= FLAGV;
    }
    if (val1 > val2) {
        PS |= FLAGC;
    }
    memwrite<2>(da, uval);
}

void KB11::JSR(const uint16_t instr) {
    const uint16_t uval = DA(instr);
    if (isReg(uval)) {
        printf("JSR called on registeri\n");
        std::abort();
    }
    push(R[S(instr) & 7]);
    R[S(instr) & 7] = R[7];
    R[7] = uval;
}

void KB11::MUL(const uint16_t instr) {
    int32_t val1 = R[S(instr) & 7];
    if (val1 & 0x8000) {
        val1 = -((0xFFFF ^ val1) + 1);
    }
    uint16_t da = DA(instr);
    int32_t val2 = memread<2>(da);
    if (val2 & 0x8000) {
        val2 = -((0xFFFF ^ val2) + 1);
    }
    const int32_t sval = val1 * val2;
    R[S(instr) & 7] = sval >> 16;
    R[(S(instr) & 7) | 1] = sval & 0xFFFF;
    PS &= 0xFFF0;
    if (sval & 0x80000000) {
        PS |= FLAGN;
    }
    setZ((sval & 0xFFFFFFFF) == 0);
    if ((sval < (1 << 15)) || (sval >= ((1L << 15) - 1))) {
        PS |= FLAGC;
    }
}

void KB11::DIV(const uint16_t instr) {
    const int32_t val1 = (R[S(instr) & 7] << 16) | (R[(S(instr) & 7) | 1]);
    const uint16_t da = DA(instr);
    const int32_t val2 = memread<2>(da);
    PS &= 0xFFF0;
    if (val2 == 0) {
        PS |= FLAGC;
        return;
    }
    if ((val1 / val2) >= 0x10000) {
        PS |= FLAGV;
        return;
    }
    R[S(instr) & 7] = (val1 / val2) & 0xFFFF;
    R[(S(instr) & 7) | 1] = (val1 % val2) & 0xFFFF;
    setZ(R[S(instr) & 7] == 0);
    if (R[S(instr) & 7] & 0100000) {
        PS |= FLAGN;
    }
    if (val1 == 0) {
        PS |= FLAGV;
    }
}

void KB11::ASH(const uint16_t instr) {
    const uint16_t val1 = R[S(instr) & 7];
    const uint16_t da = aget(D(instr), 2);
    uint16_t val2 = memread<2>(da) & 077;
    PS &= 0xFFF0;
    int32_t sval;
    if (val2 & 040) {
        val2 = (077 ^ val2) + 1;
        if (val1 & 0100000) {
            sval = 0xFFFF ^ (0xFFFF >> val2);
            sval |= val1 >> val2;
        } else {
            sval = val1 >> val2;
        }
        if (val1 & (1 << (val2 - 1))) {
            PS |= FLAGC;
        }
    } else {
        sval = (val1 << val2) & 0xFFFF;
        if (val1 & (1 << (16 - val2))) {
            PS |= FLAGC;
        }
    }
    R[S(instr) & 7] = sval;
    setZ(sval == 0);
    if (sval & 0100000) {
        PS |= FLAGN;
    }
    if ((sval & 0100000) xor (val1 & 0100000)) {
        PS |= FLAGV;
    }
}

void KB11::ASHC(const uint16_t instr) {
    const uint32_t val1 = R[S(instr) & 7] << 16 | R[(S(instr) & 7) | 1];
    const uint16_t da = aget(D(instr), 2);
    uint16_t val2 = memread<2>(da) & 077;
    PS &= 0xFFF0;
    int32_t sval;
    if (val2 & 040) {
        val2 = (077 ^ val2) + 1;
        if (val1 & 0x80000000) {
            sval = 0xFFFFFFFF ^ (0xFFFFFFFF >> val2);
            sval |= val1 >> val2;
        } else {
            sval = val1 >> val2;
        }
        if (val1 & (1 << (val2 - 1))) {
            PS |= FLAGC;
        }
    } else {
        sval = (val1 << val2) & 0xFFFFFFFF;
        if (val1 & (1 << (32 - val2))) {
            PS |= FLAGC;
        }
    }
    R[S(instr) & 7] = (sval >> 16) & 0xFFFF;
    R[(S(instr) & 7) | 1] = sval & 0xFFFF;
    setZ(sval == 0);
    if (sval & 0x80000000) {
        PS |= FLAGN;
    }
    if ((sval & 0x80000000) xor (val1 & 0x80000000)) {
        PS |= FLAGV;
    }
}

void KB11::XOR(const uint16_t instr) {
    const uint16_t val1 = R[S(instr) & 7];
    const uint16_t da = aget(D(instr), 2);
    const uint16_t val2 = memread<2>(da);
    const uint16_t uval = val1 ^ val2;
    PS &= 0xFFF1;
    setZ(uval == 0);
    if (uval & 0x8000) {
        PS |= FLAGN;
    }
    memwrite<2>(da, uval);
}

void KB11::SOB(const uint16_t instr) {
    uint8_t o = instr & 0xFF;
    R[S(instr) & 7]--;
    if (R[S(instr) & 7]) {
        o &= 077;
        o <<= 1;
        R[7] -= o;
    }
}

void KB11::JMP(const uint16_t instr) {
    const uint16_t uval = aget(D(instr), 2);
    if (isReg(uval)) {
        printf("JMP called with register dest\n");
        std::abort();
    }
    R[7] = uval;
}

void KB11::MARK(const uint16_t instr) {
    R[6] = R[7] + ((instr & 077) << 1);
    R[7] = R[5];
    R[5] = pop();
}

void KB11::MFPI(const uint16_t instr) {
    uint16_t da = aget(D(instr), 2);
    uint16_t uval;
    if (da == 0170006) {
        // val = (curuser == prevuser) ? R[6] : (prevuser ? k.USP : KSP);
        if (curuser == prevuser) {
            uval = R[6];
        } else {
            if (prevuser) {
                uval = USP;
            } else {
                uval = KSP;
            }
        }
    } else if (isReg(da)) {
        printf("invalid MFPI instruction\n");
        std::abort();
    } else {
        uval = unibus.read16(mmu.decode<false>((uint16_t)da, prevuser));
    }
    push(uval);
    PS &= 0xFFF0;
    PS |= FLAGC;
    setZ(uval == 0);
    if (uval & 0x8000) {
        PS |= FLAGN;
    }
}

void KB11::MTPI(const uint16_t instr) {
    uint16_t da = aget(D(instr), 2);
    uint16_t uval = pop();
    if (da == 0170006) {
        if (curuser == prevuser) {
            R[6] = uval;
        } else {
            if (prevuser) {
                USP = uval;
            } else {
                KSP = uval;
            }
        }
    } else if (isReg(da)) {
        printf("invalid MTPI instrution\n");
        std::abort();
    } else {
        unibus.write16(mmu.decode<true>((uint16_t)da, prevuser), uval);
    }
    PS &= 0xFFF0;
    PS |= FLAGC;
    setZ(uval == 0);
    if (uval & 0x8000) {
        PS |= FLAGN;
    }
}

void KB11::RTS(const uint16_t instr) {
    R[7] = R[D(instr) & 7];
    R[D(instr) & 7] = pop();
}

void KB11::EMTX(const uint16_t instr) {
    uint16_t uval;
    if ((instr & 0177400) == 0104000) {
        uval = 030;
    } else if ((instr & 0177400) == 0104400) {
        uval = 034;
    } else if (instr == 3) {
        uval = 014;
    } else {
        uval = 020;
    }
    uint16_t prev = PS;
    switchmode<false>();
    push(prev);
    push(R[7]);
    R[7] = unibus.read16(uval);
    PS = unibus.read16(uval + 2);
    if (prevuser) {
        PS |= (1 << 13) | (1 << 12);
    }
}

void KB11::_RTT() {
    R[7] = pop();
    uint16_t uval = pop();
    if (curuser) {
        uval &= 047;
        uval |= PS & 0177730;
    }
    unibus.write16(0777776, uval);
}

void KB11::RESET() {
    if (curuser) {
        return;
    }
    unibus.reset();
}

void KB11::step() {
    PC = R[7];
    uint16_t instr = access<0>(PC);
    R[7] += 2;

    if (PRINTSTATE)
        printstate();

    switch (instr) {
    case 0010000 ... 0017777: // MOV
        MOV<2>(instr);
        return;
    case 0110000 ... 0117777: // MOVB
        MOV<1>(instr);
        return;
    case 0020000 ... 0027777: // CMP
        CMP<2>(instr);
        return;
    case 0120000 ... 0127777: // CMPB
        CMP<1>(instr);
        return;
    case 0030000 ... 0037777: // BIT
        BIT<2>(instr);
        return;
    case 0130000 ... 0137777: // BITB
        BIT<1>(instr);
        return;
    case 0040000 ... 0047777: // BIC
        BIC<2>(instr);
        return;
    case 0140000 ... 0147777: // BICB
        BIC<1>(instr);
        return;
    case 0050000 ... 0057777: // BIS
        BIS<2>(instr);
        return;
    case 0150000 ... 0157777: // BISB
        BIS<1>(instr);
        return;
    case 0060000 ... 0067777: // ADD
        ADD(instr);
        return;
    case 0160000 ... 0167777: // SUB
        SUB(instr);
        return;
    case 0004000 ... 0004777: // JSR
        JSR(instr);
        return;
    case 0070000 ... 0070777: // MUL
        MUL(instr);
        return;
    case 0071000 ... 0071777: // DIV
        DIV(instr);
        return;
    case 0072000 ... 0072777: // ASH
        ASH(instr);
        return;
    case 0073000 ... 0073777: // ASHC
        ASHC(instr);
        return;
    case 0074000 ... 0074777: // XOR
        XOR(instr);
        return;
    case 0077000 ... 0077777: // SOB
        SOB(instr);
        return;
    case 0005000 ... 0005077: // CLR
        CLR<2>(instr);
        return;
    case 0105000 ... 0105077: // CLRB
        CLR<1>(instr);
        return;
    case 0005100 ... 0005177: // COM
        COM<2>(instr);
        return;
    case 0105100 ... 0105177: // COMB
        COM<1>(instr);
        return;
    case 0005200 ... 0005277: // INC
        INC<2>(instr);
        return;
    case 0105200 ... 0105277: // INCB
        INC<1>(instr);
        return;
    case 0005300 ... 0005377: // DEC
        _DEC<2>(instr);
        return;
    case 0105300 ... 0105377: // DECB
        _DEC<1>(instr);
        return;
    case 0005400 ... 0005477: // NEG
        NEG<2>(instr);
        return;
    case 0105400 ... 0105477: // NEGB
        NEG<1>(instr);
        return;
    case 0005500 ... 0005577: // ADC
        _ADC<2>(instr);
        return;
    case 0105500 ... 0105577: // ADCB
        _ADC<1>(instr);
        return;
    case 0005600 ... 0005677: // SBC
        SBC<2>(instr);
        return;
    case 0105600 ... 0105677: // SBCB
        SBC<1>(instr);
        return;
    case 0005700 ... 0005777: // TST
        TST<2>(instr);
        return;
    case 0105700 ... 0105777: // TSTB
        TST<1>(instr);
        return;
    case 0006000 ... 0006077: // ROR
        ROR<2>(instr);
        return;
    case 0106000 ... 0106077: // RORB
        ROR<1>(instr);
        return;
    case 0006100 ... 0006177: // ROL
        ROL<2>(instr);
        return;
    case 0106100 ... 0106177: // ROLB
        ROL<1>(instr);
        return;
    case 0006200 ... 0006277: // ASR
        ASR<2>(instr);
        return;
    case 0106200 ... 0106277: // ASRB
        ASR<1>(instr);
        return;
    case 0006300 ... 0006377: // ASL
        ASL<2>(instr);
        return;
    case 0106300 ... 0106377: // ASLB
        ASL<1>(instr);
        return;
    case 0006700 ... 0006777: // SXT
        SXT<2>(instr);
        return;
    case 0000100 ... 0000177: // JMP
        JMP(instr);
        return;
    case 0000300 ... 0000377: // SWAB
        SWAB<2>(instr);
        return;
    case 0006400 ... 0006477: // MARK
        MARK(instr);
        break;
    case 0006500 ... 0006577: // MFPI
        MFPI(instr);
        return;
    case 0006600 ... 0006677: // MTPI
        MTPI(instr);
        return;
    case 0000200 ... 0000207: // RTS
        RTS(instr);
        return;
    }

    switch (instr & 0177400) {
    case 0000400:
        branch(instr & 0xFF);
        return;
    case 0001000:
        if (!Z()) {
            branch(instr & 0xFF);
        }
        return;
    case 0001400:
        if (Z()) {
            branch(instr & 0xFF);
        }
        return;
    case 0002000:
        if (!(N() xor V())) {
            branch(instr & 0xFF);
        }
        return;
    case 0002400:
        if (N() xor V()) {
            branch(instr & 0xFF);
        }
        return;
    case 0003000:
        if ((!(N() xor V())) && (!Z())) {
            branch(instr & 0xFF);
        }
        return;
    case 0003400:
        if ((N() xor V()) || Z()) {
            branch(instr & 0xFF);
        }
        return;
    case 0100000:
        if (!N()) {
            branch(instr & 0xFF);
        }
        return;
    case 0100400:
        if (N()) {
            branch(instr & 0xFF);
        }
        return;
    case 0101000:
        if ((!C()) && (!Z())) {
            branch(instr & 0xFF);
        }
        return;
    case 0101400:
        if (C() || Z()) {
            branch(instr & 0xFF);
        }
        return;
    case 0102000:
        if (!V()) {
            branch(instr & 0xFF);
        }
        return;
    case 0102400:
        if (V()) {
            branch(instr & 0xFF);
        }
        return;
    case 0103000:
        if (!C()) {
            branch(instr & 0xFF);
        }
        return;
    case 0103400:
        if (C()) {
            branch(instr & 0xFF);
        }
        return;
    }
    if (((instr & 0177000) == 0104000) || (instr == 3) ||
        (instr == 4)) { // EMT TRAP IOT BPT
        EMTX(instr);
        return;
    }
    if ((instr & 0177740) == 0240) { // CL?, SE?
        if (instr & 020) {
            PS |= instr & 017;
        } else {
            PS &= ~instr & 017;
        }
        return;
    }
    switch (instr & 7) {
    case 00: // HALT
        if (curuser) {
            break;
        }
        printf("HALT\n");
        std::abort();
    case 01: // WAIT
        if (curuser) {
            break;
        }
        sched_yield();
        return;
    case 02: // RTI

    case 06: // RTT
        _RTT();
        return;
    case 05: // RESET
        RESET();
        return;
    }
    if (instr == 0170011) {
        // SETD ; not needed by UNIX, but used; therefore ignored
        return;
    }
    printf("invalid instruction\n");
    trap(INTINVAL);
}

void KB11::trapat(uint16_t vec) { // , msg string) {
    if (vec & 1) {
        printf("Thou darst calling trapat() with an odd vector number?\n");
        std::abort();
    }
    printf("trap: %x\n", vec);

    /*var prev uint16
          defer func() {
                  t = recover()
                  switch t = t.(type) {
                  case trap:
                          writedebug("red stack trap!\n")
                          memory[0] = uint16(k.R[7])
                          memory[1] = prev
                          vec = 4
                          panic("fatal")
                  case nil:
                          break
                  default:
                          panic(t)
                  }
     */
    auto prev = PS;
    switchmode<false>();
    push(prev);
    push(R[7]);

    R[7] = unibus.read16(vec);
    PS = unibus.read16(vec + 2);
    if (prevuser) {
        PS |= (1 << 13) | (1 << 12);
    }
}

void KB11::interrupt(uint8_t vec, uint8_t pri) {
    if (vec & 1) {
        printf("Thou darst calling interrupt() with an odd vector number?\n");
        std::abort();
    }
    // fast path
    if (itab[0].vec == 0) {
        itab[0].vec = vec;
        itab[0].pri = pri;
        return;
    }
    uint8_t i;
    for (i = 0; i < itab.size(); i++) {
        if ((itab[i].vec == 0) || (itab[i].pri < pri)) {
            break;
        }
    }
    for (; i < itab.size(); i++) {
        if ((itab[i].vec == 0) || (itab[i].vec >= vec)) {
            break;
        }
    }
    if (i >= itab.size()) {
        printf("interrupt table full\n");
        std::abort();
    }
    uint8_t j;
    for (j = i + 1; j < itab.size(); j++) {
        itab[j] = itab[j - 1];
    }
    itab[i].vec = vec;
    itab[i].pri = pri;
}

// pop the top interrupt off the itab.
void KB11::popirq() {
    for (uint8_t i = 0; i < itab.size() - 1; i++) {
        itab[i] = itab[i + 1];
    }
    itab[itab.size() - 1].vec = 0;
    itab[itab.size() - 1].pri = 0;
}

void KB11::handleinterrupt() {
    uint8_t vec = itab[0].vec;
    if (DEBUG_INTER) {
        printf("IRQ: %x\n", vec);
    }
    uint16_t vv = setjmp(trapbuf);
    if (vv == 0) {
        uint16_t prev = PS;
        switchmode<false>();
        push(prev);
        push(R[7]);
    } else {
        trapat(vv);
    }

    R[7] = unibus.read16(vec);
    PS = unibus.read16(vec + 2);
    if (prevuser) {
        PS |= (1 << 13) | (1 << 12);
    }
    popirq();
}

void KB11::printstate() {
    printf(
        "R0 %06o R1 %06o R2 %06o R3 %06o R4 %06o R5 %06o R6 %06o R7 %06o\r\n",
        R[0], R[1], R[2], R[3], R[4], R[5], R[6], R[7]);
    printf("[%s%s%s%s%s%s", prevuser ? "u" : "k", curuser ? "U" : "K",
           PS & FLAGN ? "N" : " ", PS & FLAGZ ? "Z" : " ",
           PS & FLAGV ? "V" : " ", PS & FLAGC ? "C" : " ");
    printf("]  instr %06o: %06o\t ", PC,
           unibus.read16(mmu.decode<false>(PC, curuser)));
    disasm(mmu.decode<false>(PC, curuser));
    printf("\n");
}
