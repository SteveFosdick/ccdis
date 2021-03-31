#include "ccdis.h"

typedef enum {
    IMP,    // Implied.
    IMPA,   // Implied with A as the implied operand.
    IMM,    // Immediate, 8 bit
    ZP,     // Zero page, known as Direct Page on the 65816.
    ZPX,    // Zero (direct) page indexed by X.
    ZPY,    // Zero (direct) page indexed by Y (for LDX).
    INDX,   // Zero (direct) page indexed (by X) indirect.
    INDY,   // Zero (direct) page indirect indexed (by Y).
    IND,    // Zero (direct) page indirect.
    ABS,    // Absolute.
    ABSX,   // Absolute indexed by X
    ABSY,   // Absolute indexed by Y
    IND16,  // Indirect 16bit (for JMP).
    IND1X,  // Indexed (by X) indirect (for JMP)
    PCR    // PC-relative.  8bit signed offset from PC for branch instructions.
} addr_mode_t;

typedef enum {
    UND,   ADC,   ANC,   AND,   ANE,   ARR,   ASL,   ASR,   BCC,   BCS,   BEQ,
    BIT,   BMI,   BNE,   BPL,   BRA,   BRK,   BRL,   BVC,   BVS,   CLC,   CLD,
    CLI,   CLV,   CMP,   COP,   CPX,   CPY,   DCP,   DEC,   DEX,   DEY,   EOR,
    HLT,   INC,   INX,   INY,   ISB,   JML,   JMP,   JSL,   JSR,   LAS,   LAX,
    LDA,   LDX,   LDY,   LSR,   LXA,   MVN,   MVP,   NOP,   ORA,   PEA,   PEI,
    PER,   PHA,   PHB,   PHD,   PHK,   PHP,   PHX,   PHY,   PLA,   PLB,   PLD,
    PLP,   PLX,   PLY,   REP,   RLA,   ROL,   ROR,   RRA,   RTI,   RTL,   RTS,
    SAX,   SBC,   SBX,   SEC,   SED,   SEI,   SEP,   SHA,   SHS,   SHX,   SHY,
    SLO,   SRE,   STA,   STP,   STX,   STY,   STZ,   TAX,   TAY,   TCD,   TCS,
    TDC,   TRB,   TSB,   TSC,   TSX,   TXA,   TXS,   TXY,   TYA,   TYX,   WAI,
    WDM,   XBA,   XCE
} op_t;

static const char op_names[113][4] = {
    "---", "ADC", "ANC", "AND", "ANE", "ARR", "ASL", "ASR", "BCC", "BCS", "BEQ",
    "BIT", "BMI", "BNE", "BPL", "BRA", "BRK", "BRL", "BVC", "BVS", "CLC", "CLD",
    "CLI", "CLV", "CMP", "COP", "CPX", "CPY", "DCP", "DEC", "DEX", "DEY", "EOR",
    "HLT", "INC", "INX", "INY", "ISB", "JML", "JMP", "JSL", "JSR", "LAS", "LAX",
    "LDA", "LDX", "LDY", "LSR", "LXA", "MVN", "MVP", "NOP", "ORA", "PEA", "PEI",
    "PER", "PHA", "PHB", "PHD", "PHK", "PHP", "PHX", "PHY", "PLA", "PLB", "PLD",
    "PLP", "PLX", "PLY", "REP", "RLA", "ROL", "ROR", "RRA", "RTI", "RTL", "RTS",
    "SAX", "SBC", "SBX", "SEC", "SED", "SEI", "SEP", "SHA", "SHS", "SHX", "SHY",
    "SLO", "SRE", "STA", "STP", "STX", "STY", "STZ", "TAX", "TAY", "TCD", "TCS",
    "TDC", "TRB", "TSB", "TSC", "TSX", "TXA", "TXS", "TXY", "TYA", "TYX", "WAI",
    "WDM", "XBA", "XCE"
};

static const int8_t op_cmos[256] =
{
/*       0     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F */
/*00*/  BRK,  ORA,  UND,  UND,  TSB,  ORA,  ASL,  UND,  PHP,  ORA,  ASL,  UND,  TSB,  ORA,  ASL,  UND,
/*10*/  BPL,  ORA,  ORA,  UND,  TRB,  ORA,  ASL,  UND,  CLC,  ORA,  INC,  UND,  TRB,  ORA,  ASL,  UND,
/*20*/  JSR,  AND,  UND,  UND,  BIT,  AND,  ROL,  UND,  PLP,  AND,  ROL,  UND,  BIT,  AND,  ROL,  UND,
/*30*/  BMI,  AND,  AND,  UND,  BIT,  AND,  ROL,  UND,  SEC,  AND,  DEC,  UND,  BIT,  AND,  ROL,  UND,
/*40*/  RTI,  EOR,  UND,  UND,  UND,  EOR,  LSR,  UND,  PHA,  EOR,  LSR,  UND,  JMP,  EOR,  LSR,  UND,
/*50*/  BVC,  EOR,  EOR,  UND,  UND,  EOR,  LSR,  UND,  CLI,  EOR,  PHY,  UND,  UND,  EOR,  LSR,  UND,
/*60*/  RTS,  ADC,  UND,  UND,  STZ,  ADC,  ROR,  UND,  PLA,  ADC,  ROR,  UND,  JMP,  ADC,  ROR,  UND,
/*70*/  BVS,  ADC,  ADC,  UND,  STZ,  ADC,  ROR,  UND,  SEI,  ADC,  PLY,  UND,  JMP,  ADC,  ROR,  UND,
/*80*/  BRA,  STA,  UND,  UND,  STY,  STA,  STX,  UND,  DEY,  BIT,  TXA,  UND,  STY,  STA,  STX,  UND,
/*90*/  BCC,  STA,  STA,  UND,  STY,  STA,  STX,  UND,  TYA,  STA,  TXS,  UND,  STZ,  STA,  STZ,  UND,
/*A0*/  LDY,  LDA,  LDX,  UND,  LDY,  LDA,  LDX,  UND,  TAY,  LDA,  TAX,  UND,  LDY,  LDA,  LDX,  UND,
/*B0*/  BCS,  LDA,  LDA,  UND,  LDY,  LDA,  LDX,  UND,  CLV,  LDA,  TSX,  UND,  LDY,  LDA,  LDX,  UND,
/*C0*/  CPY,  CMP,  UND,  UND,  CPY,  CMP,  DEC,  UND,  INY,  CMP,  DEX,  WAI,  CPY,  CMP,  DEC,  UND,
/*D0*/  BNE,  CMP,  CMP,  UND,  UND,  CMP,  DEC,  UND,  CLD,  CMP,  PHX,  STP,  UND,  CMP,  DEC,  UND,
/*E0*/  CPX,  SBC,  UND,  UND,  CPX,  SBC,  INC,  UND,  INX,  SBC,  NOP,  UND,  CPX,  SBC,  INC,  UND,
/*F0*/  BEQ,  SBC,  SBC,  UND,  UND,  SBC,  INC,  UND,  SED,  SBC,  PLX,  UND,  UND,  SBC,  INC,  UND,
 };

static const uint8_t am_cmos[256]=
{
/*       0     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F */
/*00*/  IMP,  INDX, IMP,  IMP,  ZP,   ZP,   ZP,   IMP,  IMP,  IMM,  IMPA, IMP,  ABS,  ABS,  ABS,  IMP,
/*10*/  PCR,  INDY, IND,  IMP,  ZP,   ZPX,  ZPX,  IMP,  IMP,  ABSY, IMPA, IMP,  ABS,  ABSX, ABSX, IMP,
/*20*/  ABS,  INDX, IMP,  IMP,  ZP,   ZP,   ZP,   IMP,  IMP,  IMM,  IMPA, IMP,  ABS,  ABS,  ABS,  IMP,
/*30*/  PCR,  INDY, IND,  IMP,  ZPX,  ZPX,  ZPX,  IMP,  IMP,  ABSY, IMPA, IMP,  ABSX, ABSX, ABSX, IMP,
/*40*/  IMP,  INDX, IMP,  IMP,  ZP,   ZP,   ZP,   IMP,  IMP,  IMM,  IMPA, IMP,  ABS,  ABS,  ABS,  IMP,
/*50*/  PCR,  INDY, IND,  IMP,  ZP,   ZPX,  ZPX,  IMP,  IMP,  ABSY, IMP,  IMP,  ABS,  ABSX, ABSX, IMP,
/*60*/  IMP,  INDX, IMP,  IMP,  ZP,   ZP,   ZP,   IMP,  IMP,  IMM,  IMPA, IMP,  IND16,ABS,  ABS,  IMP,
/*70*/  PCR,  INDY, IND,  IMP,  ZPX,  ZPX,  ZPX,  IMP,  IMP,  ABSY, IMP,  IMP,  IND1X,ABSX, ABSX, IMP,
/*80*/  PCR,  INDX, IMP,  IMP,  ZP,   ZP,   ZP,   IMP,  IMP,  IMM,  IMP,  IMP,  ABS,  ABS,  ABS,  IMP,
/*90*/  PCR,  INDY, IND,  IMP,  ZPX,  ZPX,  ZPY,  IMP,  IMP,  ABSY, IMP,  IMP,  ABS,  ABSX, ABSX, IMP,
/*A0*/  IMM,  INDX, IMM,  IMP,  ZP,   ZP,   ZP,   IMP,  IMP,  IMM,  IMP,  IMP,  ABS,  ABS,  ABS,  IMP,
/*B0*/  PCR,  INDY, IND,  IMP,  ZPX,  ZPX,  ZPY,  IMP,  IMP,  ABSY, IMP,  IMP,  ABSX, ABSX, ABSY, IMP,
/*C0*/  IMM,  INDX, IMP,  IMP,  ZP,   ZP,   ZP,   IMP,  IMP,  IMM,  IMP,  IMP,  ABS,  ABS,  ABS,  IMP,
/*D0*/  PCR,  INDY, IND,  IMP,  ZP,   ZPX,  ZPX,  IMP,  IMP,  ABSY, IMP,  IMP,  ABS,  ABSX, ABSX, IMP,
/*E0*/  IMM,  INDX, IMP,  IMP,  ZP,   ZP,   ZP,   IMP,  IMP,  IMM,  IMP,  IMP,  ABS,  ABS,  ABS,  IMP,
/*F0*/  PCR,  INDY, IND,  IMP,  ZP,   ZPX,  ZPX,  IMP,  IMP,  ABSY, IMP,  IMP,  ABS,  ABSX, ABSX, IMP,
};

unsigned mc_trace(const unsigned char *content, unsigned base_addr, unsigned addr, unsigned max_addr, unsigned *new_labels)
{
    int ujump = 0;
    do {
        int dest = max_addr;
        int r;
        unsigned b2, b3;
        unsigned b1 = content[addr++];
        if (b1 == 0xdf)
            break;
        switch(am_cmos[b1]) {
            case IMP:
                if (b1 == 0x60)
                    ujump = 1;
            case IMPA:
                break;
            case IMM:
            case ZP:
            case ZPX:
            case ZPY:
            case INDX:
            case INDY:
            case IND:
                addr++;
                break;
            case ABSX:
            case ABSY:
            case IND16:
            case IND1X:
                addr += 2;
                break;
            case ABS:
                b2 = content[addr++];
                b3 = content[addr++];
                if (b1 == 0x4c) {
                    dest = ((b3 << 8) | b2);
                    ujump = 1;
                }
                break;
            case PCR:
                r = *(signed char *)(content + addr++);
                dest = addr + r;
                if (b1 == 0x80)
                    ujump = 1;
        }
        if (dest >= base_addr && dest < max_addr) {
            unsigned loc = loc_index[dest];
            unsigned usetype = loc & LOC_USETYPE;
            if (usetype == 0 || usetype == LOC_DATA) {
                loc = (loc & ~LOC_USETYPE) | LOC_M6502;
                if (b1 == 0x20)
                    loc |= LOC_CALL;
                loc_index[dest] = loc;
                (*new_labels)++;
            }
        }
    }
    while (addr < max_addr && !ujump);

    return addr;
}

static void prt_bytes(FILE *ofp, const unsigned char *content, unsigned addr)
{
    uint8_t p1, p2;

    uint8_t op = content[addr];
    addr_mode_t addr_mode = am_cmos[op];

    switch (addr_mode)
    {
        case IMP:
        case IMPA:
            fprintf(ofp, "%02X       ", op);
            break;
        case IMM:
        case ZP:
        case ZPX:
        case ZPY:
        case IND:
        case INDX:
        case INDY:
        case PCR:
            p1 = content[++addr];
            fprintf(ofp, "%02X %02X    ", op, p1);
            break;
        case ABS:
        case ABSX:
        case ABSY:
        case IND16:
        case IND1X:
            p1 = content[++addr];
            p2 = content[++addr];
            fprintf(ofp, "%02X %02X %02X ", op, p1, p2);
    }
}

static const char opsep[] = "     (";

static unsigned prt_mnemonics(FILE *ofp, const unsigned char *content, unsigned addr)
{
    uint8_t p1, p2;
    unsigned dest;

    uint8_t op = content[addr++];
    uint8_t ni = op_cmos[op];
    addr_mode_t addr_mode = am_cmos[op];
    const char *op_name = op_names[ni];
    fwrite(op_name, 3, 1, ofp);

    switch (addr_mode)
    {
        case IMP:
            putc('\n', ofp);
            break;
        case IMPA:
            if (pf_current->acc) {
                fwrite(opsep, 5, 1, ofp);
                fputs(pf_current->acc, ofp);
            }
            putc('\n', ofp);
            break;
        case IMM:
            p1 = content[addr++];
            fwrite(opsep, 5, 1, ofp);
            fprintf(ofp, pf_current->imm, p1);
            putc('\n', ofp);
            break;
        case ZP:
            p1 = content[addr++];
            fwrite(opsep, 5, 1, ofp);
            fprintf(ofp, pf_current->byte, p1);
            putc('\n', ofp);
            break;
        case ZPX:
            p1 = content[addr++];
            fwrite(opsep, 5, 1, ofp);
            fprintf(ofp, pf_current->byte, p1);
            fputs(",X\n", ofp);
            break;
        case ZPY:
            p1 = content[addr++];
            fwrite(opsep, 5, 1, ofp);
            fprintf(ofp, pf_current->byte, p1);
            fputs(",Y\n", ofp);
            break;
        case IND:
            p1 = content[addr++];
            fwrite(opsep, 6, 1, ofp);
            fprintf(ofp, pf_current->byte, p1);
            fputs(")\n", ofp);
            break;
        case INDX:
            p1 = content[addr++];
            fwrite(opsep, 6, 1, ofp);
            fprintf(ofp, pf_current->byte, p1);
            fputs(",X)\n", ofp);
            break;
        case INDY:
            p1 = content[addr++];
            fwrite(opsep, 6, 1, ofp);
            fprintf(ofp, pf_current->byte, p1);
            fputs("),Y\n", ofp);
            break;
        case ABS:
            p1 = content[addr++];
            p2 = content[addr++];
            fwrite(opsep, 5, 1, ofp);
            print_dest_addr(ofp, p1 | (p2 << 8));
            break;
        case ABSX:
            p1 = content[addr++];
            p2 = content[addr++];
            fwrite(opsep, 5, 1, ofp);
            print_dest_addr_nonl(ofp, p1 | (p2 << 8));
            fputs(",X\n", ofp);
            break;
        case ABSY:
            p1 = content[addr++];
            p2 = content[addr++];
            fwrite(opsep, 5, 1, ofp);
            print_dest_addr_nonl(ofp, p1 | (p2 << 8));
            fputs(",Y\n", ofp);
            break;
        case IND16:
            p1 = content[addr++];
            p2 = content[addr++];
            fwrite(opsep, 6, 1, ofp);
            print_dest_addr_nonl(ofp, p1 | (p2 << 8));
            fputs(")\n", ofp);
            break;
        case IND1X:
            p1 = content[addr++];
            p2 = content[addr++];
            fwrite(opsep, 6, 1, ofp);
            print_dest_addr_nonl(ofp, p1 | (p2 << 8));
            fputs(",X)\n", ofp);
            break;
        case PCR:
            p1 = content[addr++];
            dest = addr + (signed char)p1;
            fwrite(opsep, 5, 1, ofp);
            print_dest_addr(ofp, dest);
            break;
    }
    return addr;
}

unsigned mc_disassemble(FILE *ofp, const unsigned char *content, unsigned addr, unsigned max_addr)
{
    unsigned usetype;

    do {
        if (!asm_mode) {
            fprintf(ofp, "%04X: ", addr);
            prt_bytes(ofp, content, addr);
        }
        print_label(ofp, addr);
        addr = prt_mnemonics(ofp, content, addr);
        usetype = loc_index[addr] & LOC_USETYPE;
    } while (addr < max_addr && usetype == 0 && content[addr] != 0xdf);

    return addr;
}
