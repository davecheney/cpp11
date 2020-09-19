#include <cstdlib>
#include <sched.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>

#include "bootrom.h"
#include "kb11.h"

void disasm(uint32_t ia);

extern jmp_buf trapbuf;

void KB11::reset() {
    uint16_t i;
    for (i = 0; i < 29; i++) {
        unibus.access<1>(02000 + (i * 2), bootrom[i]);
    }
    R[7] = 002002;
    stacklimit = 0xff;
    switchregister = 0x0; //  0173030;
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
    auto val = access<0>(R[7]);
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
    auto instr = fetch16();

    // printstate();

    switch (instr >> 12) {    // xxSSDD Mostly double operand instructions
    case 0:                   // 00xxxx mixed group
        switch (instr >> 8) { // 00xxxx 8 bit instructions first (branch & JSR)
        case 0:               // 000xXX Misc zero group
            switch (instr >> 6) { // 000xDD group (4 case full decode)
            case 0:               // 0000xx group
                switch (instr) {
                case 0: // HALT 000000
                    printf("HALT\n");
                    printstate();
                    std::abort();
                case 1: // WAIT 000001
                    sched_yield();
                    return;
                case 3:             // BPT  000003
                    trapat(014); // Trap 14 - BPT
                    return;
                case 4: // IOT  000004
                    EMTX(instr);
                    return;
                case 5: // RESET 000005
                    RESET();
                    return;
                case 2: // RTI 000002
                case 6: // RTT 000006
                    _RTT();
                    return;
                case 7:  // MFPI
                default: // We don't know this 0000xx instruction
                    printf("unknown 0000xx instruction\n");
                    printstate();
                    trapat(INTINVAL);
                    return;
                }
            case 1: // JMP 0001DD
                JMP(instr);
                return;
            case 2:                         // 00002xR single register group
                switch ((instr >> 3) & 7) { // 00002xR register or CC
                case 0:                     // RTS 00020R
                    RTS(instr);
                    return;
                case 3: // SPL 00023N
                    PS = (PS & 0xf81f) | ((instr & 7) << 5);
                    return;
                case 4: // CLR CC 00024C Part 1 without N
                case 5: // CLR CC 00025C Part 2 with N
                    PS &= ~instr & 017;
                    return;
                case 6: // SET CC 00026C Part 1 without N
                case 7: // SET CC 00027C Part 2 with N
                    PS |= instr & 017;
                    return;
                default: // We don't know this 00002xR instruction
                    printf("unknown 0002xR instruction\n");
                    printstate();
                    trapat(INTINVAL);
                    return;
                }
            case 3: // SWAB 0003DD
                SWAB<2>(instr);
                return;
            default:
                printf("unknown 000xDD instruction\n");
                printstate();
                trapat(INTINVAL);
                return;
            }
        case 1: // BR 0004 offset
            branch(instr & 0xff);
            return;
        case 2: // BNE 0010 offset
            if (!Z()) {
                branch(instr & 0xff);
            }
            return;
        case 3: // BEQ 0014 offset
            if (Z()) {
                branch(instr & 0xff);
            }
            return;
        case 4: // BGE 0020 offset
            if (!(N() xor V())) {
                branch(instr & 0xFF);
            }
            return;
        case 5: // BLT 0024 offset
            if (N() xor V()) {
                branch(instr & 0xFF);
            }
            return;
        case 6: // BGT 0030 offset
            if ((!(N() xor V())) && (!Z())) {
                branch(instr & 0xFF);
            }
            return;
        case 7: // BLE 0034 offset
            if ((N() xor V()) || Z()) {
                branch(instr & 0xFF);
            }
            return;
        case 8: // JSR 004RDD In two parts
        case 9: // JSR 004RDD continued (9 bit instruction so use 2 x 8 bit
            JSR(instr);
            return;
        default: // Remaining 0o00xxxx instructions where xxxx >= 05000
            switch (instr >> 6) { // 00xxDD
            case 050:             // CLR 0050DD
                CLR<2>(instr);
                return;
            case 051: // COM 0051DD
                COM<2>(instr);
                return;
            case 052: // INC 0052DD
                INC<2>(instr);
                return;
            case 053: // DEC 0053DD
                _DEC<2>(instr);
                return;
            case 054: // NEG 0054DD
                NEG<2>(instr);
                return;
            case 055: // ADC 0055DD
                _ADC<2>(instr);
                return;
            case 056: // SBC 0056DD
                SBC<2>(instr);
                return;
            case 057: // TST 0057DD
                TST<2>(instr);
                return;
            case 060: // ROR 0060DD
                ROR<2>(instr);
                return;
            case 061: // ROL 0061DD
                ROL<2>(instr);
                return;
            case 062: // ASR 0062DD
                ASR<2>(instr);
                return;
            case 063: // ASL 0063DD
                ASL<2>(instr);
                return;
            case 064: // MARK 0064nn
                MARK(instr);
                return;
            case 065: // MFPI 0065SS
                MFPI(instr);
                return;
            case 066: // MTPI 0066DD
                MTPI(instr);
                return;
            case 067: // SXT 0067DD
                SXT<2>(instr);
                return;
            default: // We don't know this 0o00xxDD instruction
                printf("unknown 00xxDD instruction\n");
                printstate();
                trapat(INTINVAL);
            }
        }
    case 1: // MOV  01SSDD
        MOV<2>(instr);
        return;
    case 2: // CMP 02SSDD
        CMP<2>(instr);
        return;
    case 3: // BIT 03SSDD
        BIT<2>(instr);
        return;
    case 4: // BIC 04SSDD
        BIC<2>(instr);
        return;
    case 5: // BIS 05SSDD
        BIS<2>(instr);
        return;
    case 6: // ADD 06SSDD
        ADD(instr);
        return;
    case 7:                         // 07xRSS instructions
        switch ((instr >> 9) & 7) { // 07xRSS
        case 0:                     // MUL 070RSS
            MUL(instr);
            return;
        case 1: // DIV 071RSS
            DIV(instr);
            return;
        case 2: // ASH 072RSS
            ASH(instr);
            return;
        case 3: // ASHC 073RSS
            ASHC(instr);
            return;
        case 4: // XOR 074RSS
            XOR(instr);
            return;
        case 7: // SOB 077Rnn
            SOB(instr);
            return;
        default: // We don't know this 07xRSS instruction
            printf("unknown 07xRSS instruction\n");
            printstate();
            trapat(INTINVAL);
            return;
        }
    case 8:                           // 10xxxx instructions
        switch ((instr >> 8) & 0xf) { // 10xxxx 8 bit instructions first
        case 0:                       // BPL 1000 offset
            if (!N()) {
                branch(instr & 0xFF);
            }
            return;
        case 1: // BMI 1004 offset
            if (N()) {
                branch(instr & 0xFF);
            }
            return;
        case 2: // BHI 1010 offset
            if ((!C()) && (!Z())) {
                branch(instr & 0xFF);
            }
            return;
        case 3: // BLOS 1014 offset
            if (C() || Z()) {
                branch(instr & 0xFF);
            }
            return;
        case 4: // BVC 1020 offset
            if (!V()) {
                branch(instr & 0xFF);
            }
            return;
        case 5: // BVS 1024 offset
            if (V()) {
                branch(instr & 0xFF);
            }
            return;
        case 6: // BCC 1030 offset
            if (!C()) {
                branch(instr & 0xFF);
            }
            return;
        case 7: // BCS 1034 offset
            if (C()) {
                branch(instr & 0xFF);
            }
            return;
        case 8:        // EMT 1040 operand
            trapat(030); // Trap 30 - EMT instruction
            return;
        case 9:        // TRAP 1044 operand
            trapat(034); // Trap 34 - TRAP instruction
            return;
        default: // Remaining 10xxxx instructions where xxxx >= 05000
            switch ((instr >> 6) & 077) { // 10xxDD group
            case 050:                     // CLRB 1050DD
                CLR<1>(instr);
                return;
            case 051: // COMB 1051DD
                COM<1>(instr);
                return;
            case 052: // INCB 1052DD
                INC<1>(instr);
                return;
            case 053: // DECB 1053DD
                _DEC<1>(instr);
                return;
            case 054: // NEGB 1054DD
                NEG<1>(instr);
                return;
            case 055: // ADCB 01055DD
                _ADC<1>(instr);
                return;
            case 056: // SBCB 01056DD
                SBC<1>(instr);
                return;
            case 057: // TSTB 1057DD
                TST<1>(instr);
                return;
            case 060: // RORB 1060DD
                ROR<1>(instr);
                return;
            case 061: // ROLB 1061DD
                ROL<1>(instr);
                return;
            case 062: // ASRB 1062DD
                ASR<1>(instr);
                return;
            case 063: // ASLB 1063DD
                ASL<1>(instr);
                return;
            // case 0o64: // MTPS 1064SS
            // case 0o65: // MFPD 1065DD
            // case 0o66: // MTPD 1066DD
            // case 0o67: // MTFS 1064SS
            default: // We don't know this 0o10xxDD instruction
                printf("unknown 0o10xxDD instruction\n");
                printstate();
                trapat(INTINVAL);
                return;
            }
        }
    case 9: // MOVB 11SSDD
        MOV<1>(instr);
        return;
    case 10: // CMPB 12SSDD
        CMP<1>(instr);
        return;
    case 11: // BITB 13SSDD
        BIT<1>(instr);
        return;
    case 12: // BICB 14SSDD
        BIC<1>(instr);
        return;
    case 13: // BISB 15SSDD
        BIS<1>(instr);
        return;
    case 14: // SUB 16SSDD
        SUB(instr);
        return;
    case 15:
        if (instr == 0170011) {
            // SETD ; not needed by UNIX, but used; therefore ignored
            return;
        }
    default: // 15  17xxxx FPP instructions
        printf("invalid 17xxxx FPP instruction\n");
        printstate();
        trapat(INTINVAL);
    }
}

void KB11::trapat(uint16_t vec) {
    if (vec & 1) {
        printf("Thou darst calling trapat() with an odd vector number?\n");
        std::abort();
    }
    // printf("trap: %03o\n", vec);

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
    auto vec = itab[0].vec;
    // printf("IRQ: %x\n", vec);
    
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
    printf("R0 %06o R1 %06o R2 %06o R3 %06o R4 %06o R5 %06o R6 %06o R7 "
           "%06o\r\n",
           R[0], R[1], R[2], R[3], R[4], R[5], R[6], R[7]);
    printf("[%s%s%s%s%s%s", prevuser ? "u" : "k", curuser ? "U" : "K",
           PS & FLAGN ? "N" : " ", PS & FLAGZ ? "Z" : " ",
           PS & FLAGV ? "V" : " ", PS & FLAGC ? "C" : " ");
    printf("]  instr %06o: %06o\t ", PC,
           unibus.read16(mmu.decode<false>(PC, curuser)));
    disasm(mmu.decode<false>(PC, curuser));
    printf("\n");
}
