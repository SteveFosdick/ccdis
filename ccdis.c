#include "ccdis.h"
#include "cintcode_tabs.h"

static void jump_switch(unsigned base_addr, unsigned addr, unsigned max_addr, unsigned *new_labels)
{
    if (addr >= base_addr && addr < max_addr) {
        unsigned loc = loc_index[addr];
        unsigned usetype = loc & LOC_USETYPE;
        if (usetype == 0 || usetype == LOC_DATA) {
            loc_index[addr] = (loc & ~LOC_USETYPE) | LOC_CINTCODE;
            (*new_labels)++;
        }
    }
}

static unsigned trace_swb(const unsigned char *content, unsigned base_addr, unsigned addr, unsigned max_addr, unsigned *new_labels)
{
    unsigned dest = max_addr;
    if ((max_addr - addr) >= 4) {
        unsigned pos = (addr + 1 ) & 0xfffe;
        unsigned b2 = content[pos++];
        unsigned b3 = content[pos++];
        unsigned entries = b2 | (b3 << 8);
        addr = pos + entries * 2;
        if (addr < max_addr) {
            b2 = content[pos++];
            b3 = content[pos++];
            dest = (pos + (b2 | (b3 << 8)) - 2) & 0xffff;
            while (entries--) {
                pos += 2;
                b2 = content[pos++];
                b3 = content[pos++];
                unsigned w2 = (b2 | (b3 << 8));
                jump_switch(base_addr, (pos + w2 - 2) & 0xffff, max_addr, new_labels);
            }
        }
    }
    return dest;
}

static unsigned trace_swl(const unsigned char *content, unsigned base_addr, unsigned addr, unsigned max_addr, unsigned *new_labels)
{
    unsigned dest = max_addr;
    if ((max_addr - addr) >= 6) {
        unsigned pos = (addr + 1 ) & 0xfffe;
        unsigned b2 = content[pos++];
        unsigned b3 = content[pos++];
        unsigned entries = (b2 | (b3 << 8));
        b2 = content[pos++];
        b3 = content[pos++];
        dest = (pos + (b2 | (b3 << 8)) - 2) & 0xffff;
        pos += 2;
        while (entries--) {
            b2 = content[pos++];
            b3 = content[pos++];
            jump_switch(base_addr, (pos + (b2 | (b3 << 8)) - 2) & 0xffff, max_addr, new_labels);
        }
        addr = pos;
    }
    return dest;
}

static void jump_full(const cintcode_op *opent, unsigned base_addr, unsigned addr, unsigned max_addr, unsigned *new_labels)
{
    if (addr >= base_addr && addr < max_addr) {
        unsigned loc = loc_index[addr];
        unsigned usetype = loc & LOC_USETYPE;
        switch (opent->cc_it) {
            case CIT_CJMP:
            case CIT_UJMP:
                if (usetype == 0 || usetype == LOC_DATA) {
                    loc = (loc & ~LOC_USETYPE) | LOC_CINTCODE;
                    (*new_labels)++;
                }
                break;
            case CIT_CALL:
                if (usetype == 0 || usetype == LOC_DATA) {
                    loc = (loc & ~LOC_USETYPE) | LOC_CINTCODE | LOC_CALL;
                    (*new_labels)++;
                }
                break;
            default:
                if (usetype == 0) {
                    loc = loc | LOC_DATA;
                    (*new_labels)++;
                }
        }
        loc_index[addr] = loc;
    }
}

unsigned cc_trace(const unsigned char *content, unsigned base_addr, unsigned addr, unsigned max_addr, unsigned *new_labels)
{
    const cintcode_op *opent;
    do {
        unsigned dest;
        unsigned b2, b3;
        unsigned b1 = content[addr++];
        opent = cintcode_ops + b1;
        if (opent->cc_it == CIT_MCOD) {
            loc_index[addr] = (loc_index[addr] & ~LOC_USETYPE) | LOC_M6502;
            return addr;
        }
        switch(opent->cc_am) {
            case CAM_BREL:
                if (addr == max_addr)
                    return addr;
                b2 = content[addr++];
                dest = addr + b2 - 0x80;
                break;
            case CAM_BIND:
                if (addr == max_addr)
                    return addr;
                b2 = content[addr++];
                dest = (addr + ((b2<<1)|1)) & 0xfffe;
                if (dest >= base_addr && dest < max_addr) {
                    unsigned loc = loc_index[dest];
                    unsigned use_type = loc & LOC_USETYPE;
                    if (use_type == 0)
                        loc_index[dest] = loc | LOC_DATA;
                    b2 = content[dest++];
                    b3 = content[dest];
                    dest = (dest + (b2 | (b3 << 8))) & 0xfffe;
                } else
                    dest = max_addr;
                break;
            case CAM_WORD:
                addr++;
            case CAM_BYTE:
            case CAM_GLB0:
            case CAM_GLB1:
            case CAM_GLB2:
                addr++;
            default:
                dest = max_addr;
                break;
            case CAM_SWB:
                dest = trace_swb(content, base_addr, addr, max_addr, new_labels);
                break;
            case CAM_SWL:
                dest = trace_swl(content, base_addr, addr, max_addr, new_labels);
                break;
        }
        jump_full(opent, base_addr, dest, max_addr, new_labels);
    } while (addr < max_addr && opent->cc_it != CIT_RETN && opent->cc_it != CIT_UJMP);

    return addr;
}

static void ccdis_glob(FILE *ofp, const cintcode_op *opent, int globno)
{
    if (globno > 0 && globno < CINTCODE_NGLOB)
        fprintf(ofp, "%-7s %s\n", opent->mnemonic, cintocde_globs[globno]);
    else
        fprintf(ofp, "%-7s %d\n", opent->mnemonic, globno);
}

static void print_swb(FILE *ofp, const unsigned char *content, const cintcode_op *opent, unsigned addr, unsigned max_addr)
{
    if ((max_addr - addr) >= 4) {
        unsigned pos = (addr + 1 ) & 0xfffe;
        unsigned b2 = content[pos++];
        unsigned b3 = content[pos++];
        unsigned entries = b2 | (b3 << 8);
        addr = pos + entries * 2;
        if (addr < max_addr) {
            b2 = content[pos++];
            b3 = content[pos++];
            unsigned dest = (pos - 2 + (b2 | (b3 << 8))) & 0xffff;
            fprintf(ofp, "%-7s %04X\n", opent->mnemonic, entries);
            while (entries--) {
                b2 = content[pos++];
                b3 = content[pos++];
                unsigned tval = (b2 | (b3 << 8));
                b2 = content[pos++];
                b3 = content[pos++];
                fprintf(ofp, "        %04X -> ", tval);
                print_dest_addr(ofp, (pos + (b2 | (b3 << 8)) - 2) & 0xffff);
            }
            fputs("     default -> ", ofp);
            print_dest_addr(ofp, dest);
        }
    }
}

static void print_swl(FILE *ofp, const unsigned char *content, const cintcode_op *opent, unsigned addr, unsigned max_addr)
{
    if ((max_addr - addr) >= 6) {
        unsigned pos = (addr + 1 ) & 0xfffe;
        unsigned b2 = content[pos++];
        unsigned b3 = content[pos++];
        unsigned entries = (b2 | (b3 << 8));
        addr = pos + entries * 2 + 4;
        if (addr < max_addr) {
            b2 = content[pos++];
            b3 = content[pos++];
            unsigned addr = (pos + (b2 | (b3 << 8)) - 2) & 0xffff;
            b2 = content[pos++];
            b3 = content[pos++];
            unsigned low = (b2 | (b3 << 8));
            fprintf(ofp, "%-7s %04X %04X\n", opent->mnemonic, entries, low);
            while (entries--) {
                b2 = content[pos++];
                b3 = content[pos++];
                fprintf(ofp, "        %04X -> ", low++);
                print_dest_addr(ofp, (pos + (b2 | (b3 << 8)) - 2) & 0xffff);
            }
            fputs("     default -> ", ofp);
            print_dest_addr(ofp, addr);
        }
    }
}

unsigned cc_disassemble(FILE *ofp, const unsigned char *content, unsigned addr, unsigned max_addr)
{
    const cintcode_op *opent;

    do {
        unsigned dest, oppos = addr;
        unsigned b1 = content[addr++];
        unsigned b2 = 0;
        unsigned b3 = 0;
        opent = cintcode_ops + b1;
        switch(opent->cc_am) {
            case CAM_IMP:
                if (!asm_mode)
                    fprintf(ofp, "%04X: %02X       ", oppos, b1);
                break;
            case CAM_SWB:
            case CAM_SWL:
                if (!asm_mode)
                    fprintf(ofp, "%04X: %02X ...   ", oppos, b1);
                break;
            case CAM_BYTE:
            case CAM_BREL:
            case CAM_BIND:
            case CAM_GLB0:
            case CAM_GLB1:
            case CAM_GLB2:
                if (addr < max_addr) {
                    b2 = content[addr++];
                    if (!asm_mode)
                        fprintf(ofp, "%04X: %02X %02X    ", oppos, b1, b2);
                }
                break;
            case CAM_WORD:
                if ((addr + 1) < max_addr) {
                    b2 = content[addr++];
                    b3 = content[addr++];
                    if (!asm_mode)
                        fprintf(ofp, "%04X: %02X %02X %02X ", oppos, b1, b2, b3);
                }
        }

        print_label(ofp, oppos);

        switch(opent->cc_am) {
            case CAM_IMP:
                fprintf(ofp, "%s\n", opent->mnemonic);
                break;
            case CAM_BYTE:
                fprintf(ofp, "%-7s %u\n", opent->mnemonic, b2);
                break;
            case CAM_WORD:
                fprintf(ofp, "%-7s %u\n", opent->mnemonic, (b3 << 8)|b2);
                break;
            case CAM_BREL:
                fprintf(ofp, "%-7s ", opent->mnemonic);
                dest = addr + b2 - 0x80;
                print_dest_addr(ofp, dest);
                break;
            case CAM_BIND:
                dest = (addr + ((b2<<1)|1)) & 0xfffe;
                fprintf(ofp, "%-7s ", opent->mnemonic);
                print_dest_addr(ofp, dest);
                break;
            case CAM_GLB0:
                ccdis_glob(ofp, opent, b2);
                break;
            case CAM_GLB1:
                ccdis_glob(ofp, opent, 256 + b2);
                break;
            case CAM_GLB2:
                ccdis_glob(ofp, opent, 512 + b2);
                break;
            case CAM_SWB:
                print_swb(ofp, content, opent, addr, max_addr);
                break;
            case CAM_SWL:
                print_swl(ofp, content, opent, addr, max_addr);
                break;
        }
    } while (opent->cc_it != CIT_RETN && opent->cc_it != CIT_UJMP && opent->cc_it != CIT_MCOD);

    return addr;
}
