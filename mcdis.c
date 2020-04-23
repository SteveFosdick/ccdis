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

static void prt_bytes(FILE *ofp, const unsigned char *content, uint32_t addr)
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

static uint32_t prt_mnemonics(FILE *ofp, const unsigned char *content, uint32_t addr)
{
    uint8_t p1, p2;
    uint16_t temp;

    uint8_t op = content[addr++];
    uint8_t ni = op_cmos[op];
    addr_mode_t addr_mode = am_cmos[op];
    const char *op_name = op_names[ni];

    switch (addr_mode)
    {
        case IMP:
            fprintf(ofp, "%s         ", op_name);
            break;
        case IMPA:
            fprintf(ofp, "%s A       ", op_name);
            break;
        case IMM:
            p1 = content[addr++];
            fprintf(ofp, "%s #%02X     ", op_name, p1);
            break;
        case ZP:
            p1 = content[addr++];
            fprintf(ofp, "%s %02X      ", op_name, p1);
            break;
        case ZPX:
            p1 = content[addr++];
            fprintf(ofp, "%s %02X,X    ", op_name, p1);
            break;
        case ZPY:
            p1 = content[addr++];
            fprintf(ofp, "%s %02X,Y    ", op_name, p1);
            break;
        case IND:
            p1 = content[addr++];
            fprintf(ofp, "%s (%02X)    ", op_name, p1);
            break;
        case INDX:
            p1 = content[addr++];
            fprintf(ofp, "%s (%02X,X)  ", op_name, p1);
            break;
        case INDY:
            p1 = content[addr++];
            fprintf(ofp, "%s (%02X),Y  ", op_name, p1);
            break;
        case ABS:
            p1 = content[addr++];
            p2 = content[addr++];
            fprintf(ofp, "%s %02X%02X    ", op_name, p2, p1);
            break;
        case ABSX:
            p1 = content[addr++];
            p2 = content[addr++];
            fprintf(ofp, "%s %02X%02X,X  ", op_name, p2, p1);
            break;
        case ABSY:
            p1 = content[addr++];
            p2 = content[addr++];
            fprintf(ofp, "%s %02X%02X,Y  ", op_name, p2, p1);
            break;
        case IND16:
            p1 = content[addr++];
            p2 = content[addr++];
            fprintf(ofp, "%s (%02X%02X)  ", op_name, p2, p1);
            break;
        case IND1X:
            p1 = content[addr++];
            p2 = content[addr++];
            fprintf(ofp, "%s (%02X%02X,X)", op_name, p2, p1);
            break;
        case PCR:
            p1 = content[addr++];
            temp = addr + (signed char)p1;
            fprintf(ofp, "%s %04X    ", op_name, temp);
            break;
    }
    putc('\n', ofp);
    return addr;
}

void mcdis(FILE *ofp, const unsigned char *content, uint16_t code_size, int16_t *glob_index)
{
    int new_labels;

    // First scan through the intructions and find the jump destinations

    do {
        new_labels = 0;
        uint16_t dest = code_size;
        for (uint16_t bytepos = 0; bytepos < code_size; bytepos++) {
            int16_t glob = glob_index[bytepos];
            if (glob >= 0 && glob != 0x2000) {
                if (glob == 0x4000)
                    fprintf(ofp, "%04X: starting at local dest\n", bytepos);
                else
                    fprintf(ofp, "%04X: starting at global #%u\n", bytepos, glob);
                int ujump = 0;
                do {
                    int r;
                    unsigned b2, b3;
                    unsigned b1 = content[++bytepos];
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
                            bytepos++;
                            break;
                        case ABSX:
                        case ABSY:
                        case IND16:
                        case IND1X:
                            bytepos += 2;
                            break;
                        case ABS:
                            if (b1 == 0x4c) {
                                b2 = content[++bytepos];
                                b3 = content[++bytepos];
                                dest = (b3 << 8) | b2;
                            }
                            else
                                bytepos += 2;
                            break;
                        case PCR:
                            r = *(signed char *)(content + ++bytepos);
                            dest = bytepos + 1 + r;
                            fprintf(ofp, "%04X: relative jump %d to %04X\n", bytepos, r, dest);
                            if (b1 == 0x80)
                                ujump = 1;
                    }
                    if (dest < code_size && glob_index[dest] == -1) {
                        glob_index[dest] = 0x4000;
                        new_labels++;
                    }
                }
                while (!ujump);
            }
        }
        fprintf(ofp, "found %d new jump destinations\n", new_labels);
    } while (new_labels > 0);

    // Now go back and disassemble properly.

    for (uint16_t bytepos = 0; bytepos < code_size; bytepos++) {
        int16_t glob = glob_index[bytepos];
        if (glob >= 0 && glob != 0x2000) {
            do {
                fprintf(ofp, "%04X ", bytepos);
                if (glob >= 0) {
                    if (glob == 0x4000) {
                        prt_bytes(ofp, content, bytepos);
                        fprintf(ofp, " L%04X: ", bytepos);
                        bytepos = prt_mnemonics(ofp, content, bytepos);
                    }
                    else if (glob == 0x2000) {
                        fprintf(ofp, " D%04X: ", bytepos);
                        for (int i = 0; i < 8 && (!glob || glob == 0x2000); i++) {
                            fprintf(ofp, "%02X ", content[bytepos++]);
                            glob = glob_index[bytepos];
                        }
                        while (!glob || glob == 0x2000) {
                            fprintf(ofp, "%04X         ", bytepos);
                            for (int i = 0; i < 8 && (!glob || glob == 0x2000); i++) {
                                fprintf(ofp, "%02X ", content[bytepos++]);
                                glob = glob_index[bytepos];
                            }
                        }
                    }
                    else {
                        unsigned b1 = content[bytepos++];
                        fprintf(ofp, "%02X        G%03d:  EQUB %02X\n", b1, glob, b1);
                    }
                }
                else {
                    prt_bytes(ofp, content, bytepos);
                    fputs("        ", ofp);
                    bytepos = prt_mnemonics(ofp, content, bytepos);
                }
                glob = glob_index[bytepos];
            } while (bytepos < code_size && content[bytepos] != 0xdf);
        }
    }
}
