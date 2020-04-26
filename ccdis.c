#include "ccdis.h"
#include "cintcode_tabs.h"

unsigned cc_trace(const unsigned char *content, unsigned size, int16_t *glob_index, unsigned addr, int *new_labels)
{
    const cintcode_op *opent;

    do {
        unsigned dest;
        unsigned b2, b3;
        unsigned b1 = content[addr++];
        opent = cintcode_ops + b1;
        if (opent->cc_it == CIT_MCOD) {
            glob_index[addr] = GLOB_MC6502;
            return addr;
        }
        switch(opent->cc_am) {
            case CAM_BREL:
                if (addr == size)
                    return addr;
                b2 = content[addr++];
                dest = addr + b2 - 0x80;
                break;
            case CAM_BIND:
                if (addr == size)
                    return addr;
                b2 = content[addr++];
                dest = (addr + ((b2<<1)|1)) & 0xfffe;
                if (dest < size) {
                    glob_index[dest] = GLOB_DATA;
                    b2 = content[dest++];
                    b3 = content[dest];
                    dest = (dest + (b2 | (b3 << 8))) & 0xfffe;
                } else
                    dest = 0xffff;
                break;
            case CAM_WORD:
                addr++;
            case CAM_BYTE:
            case CAM_GLB0:
            case CAM_GLB1:
            case CAM_GLB2:
                addr++;
            default:
                dest = size;
                break;
            case CAM_SWB:
                dest = size;
                if ((size - addr) >= 4) {
                    unsigned pos = (addr + 1 ) & 0xfffe;
                    b2 = content[pos++];
                    b3 = content[pos++];
                    unsigned entries = b2 | (b3 << 8);
                    addr = pos + entries * 2;
                    if (addr < size) {
                        b2 = content[pos++];
                        b3 = content[pos++];
                        dest = (pos + (b2 | (b3 << 8)) - 2) & 0xffff;
                        while (entries--) {
                            pos += 2;
                            b2 = content[pos++];
                            b3 = content[pos++];
                            unsigned w2 = (b2 | (b3 << 8));
                            unsigned sdest = (pos + w2 - 2) & 0xffff;
                            if (sdest < size) {
                                int glob = glob_index[sdest];
                                if (glob < 0 || glob == GLOB_DATA) {
                                    glob_index[sdest] = GLOB_CINTCODE;
                                    (*new_labels)++;
                                }
                            }
                        }
                    }
                }
                break;
            case CAM_SWL:
                dest = size;
                if ((size - addr) >= 6) {
                    unsigned pos = (addr + 1 ) & 0xfffe;
                    b2 = content[pos++];
                    b3 = content[pos++];
                    unsigned entries = (b2 | (b3 << 8));
                    b2 = content[pos++];
                    b3 = content[pos++];
                    unsigned addr = (pos + (b2 | (b3 << 8)) - 2) & 0xffff;
                    pos += 2;
                    while (entries--) {
                        b2 = content[pos++];
                        b3 = content[pos++];
                        addr = (pos + (b2 | (b3 << 8)) - 2) & 0xffff;
                        if (addr < size) {
                            int glob = glob_index[addr];
                            if (glob < 0 || glob == GLOB_DATA) {
                                glob_index[addr] = GLOB_CINTCODE;
                                (*new_labels)++;
                            }
                        }
                    }
                    addr = pos;
                }
                break;
        }
        if (dest < size && glob_index[dest] == -1) {
            int16_t glob = glob_index[dest];
            switch (opent->cc_it) {
                case CIT_CJMP:
                case CIT_UJMP:
                    if (glob < 0 || glob == GLOB_DATA) {
                        glob_index[dest] = GLOB_CINTCODE;
                        (*new_labels)++;
                    }
                    break;
                default:
                    if (glob < 0) {
                        glob_index[dest] = GLOB_DATA;
                        (*new_labels)++;
                    }
            }
        }
    } while (addr < size && opent->cc_it != CIT_RETN && opent->cc_it != CIT_UJMP);

    return addr;
}

static void ccdis_glob(FILE *ofp, const cintcode_op *opent, int globno) {
    if (globno > 0 && globno < CINTCODE_NGLOB && *cintocde_globs[globno].name)
        fprintf(ofp, "%-7s %d (%s)\n", opent->mnemonic, globno, cintocde_globs[globno].name);
    else
        fprintf(ofp, "%-7s %d\n", opent->mnemonic, globno);
}

static void print_dest_addr(FILE *ofp, int16_t *glob_index, unsigned size, unsigned base_addr, unsigned addr)
{
    if (addr < size) {
        int glob = glob_index[addr];
        if (glob >= 0) {
            if (glob == GLOB_CINTCODE) {
                fprintf(ofp, "L%04X\n", base_addr + addr);
                return;
            }
            else if (glob == GLOB_DATA) {
                fprintf(ofp, "D%04X\n", base_addr + addr);
                return;
            }
            else if (glob < CINTCODE_NGLOB && *cintocde_globs[glob].name) {
                fprintf(ofp, "%s (G%03d)\n", cintocde_globs[glob].name, glob);
                return;
            }
            else {
                fprintf(ofp, "G%03d\n", glob);
                return;
            }
        }
    }
    fprintf(ofp, "%04X\n", base_addr + addr);
}

unsigned cc_disassemble(FILE *ofp, const unsigned char *content, unsigned size, int16_t *glob_index, unsigned base_addr, unsigned addr)
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
                fprintf(ofp, "%04X: %02X       ", base_addr + oppos, b1);
                break;
            case CAM_SWB:
            case CAM_SWL:
                fprintf(ofp, "%04X: %02X ...   ", base_addr + oppos, b1);
                break;
            case CAM_BYTE:
            case CAM_BREL:
            case CAM_BIND:
            case CAM_GLB0:
            case CAM_GLB1:
            case CAM_GLB2:
                if (addr < size) {
                    b2 = content[addr++];
                    fprintf(ofp, "%04X: %02X %02X    ", base_addr + oppos, b1, b2);
                }
                break;
            case CAM_WORD:
                if ((addr + 1) < size) {
                    b2 = content[addr++];
                    b3 = content[addr++];
                    fprintf(ofp, "%04X: %02X %02X %02X ", base_addr + oppos, b1, b2, b3);
                }
        }

        print_label(ofp, glob_index, base_addr, oppos);

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
                print_dest_addr(ofp, glob_index, size, base_addr, dest);
                break;
            case CAM_BIND:
                dest = (addr + ((b2<<1)|1)) & 0xfffe;
                fprintf(ofp, "%-7s (%04X)\n", opent->mnemonic, dest);
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
                if ((size - addr) >= 4) {
                    unsigned pos = (addr + 1 ) & 0xfffe;
                    b2 = content[pos++];
                    b3 = content[pos++];
                    unsigned entries = b2 | (b3 << 8);
                    addr = pos + entries * 2;
                    if (addr < size) {
                        b2 = content[pos++];
                        b3 = content[pos++];
                        dest = (pos - 2 + (b2 | (b3 << 8))) & 0xffff;
                        fprintf(ofp, "%-7s %04X\n", opent->mnemonic, entries);
                        while (entries--) {
                            b2 = content[pos++];
                            b3 = content[pos++];
                            unsigned tval = (b2 | (b3 << 8));
                            b2 = content[pos++];
                            b3 = content[pos++];
                            fprintf(ofp, "        %04X -> ", tval);
                            print_dest_addr(ofp, glob_index, size, base_addr, (pos + (b2 | (b3 << 8)) - 2) & 0xffff);
                        }
                        fputs("     default -> ", ofp);
                        print_dest_addr(ofp, glob_index, size, base_addr, dest);
                    }
                }
                break;
            case CAM_SWL:
                if ((size - addr) >= 6) {
                    unsigned pos = (addr + 1 ) & 0xfffe;
                    b2 = content[pos++];
                    b3 = content[pos++];
                    unsigned entries = (b2 | (b3 << 8));
                    addr = pos + entries * 2 + 4;
                    if (addr < size) {
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
                            print_dest_addr(ofp, glob_index, size, base_addr, (pos + (b2 | (b3 << 8)) - 2) & 0xffff);
                        }
                        fputs("     default -> ", ofp);
                        print_dest_addr(ofp, glob_index, size, base_addr, addr);
                    }
                }
                break;
        }
    } while (opent->cc_it != CIT_RETN && opent->cc_it != CIT_UJMP && opent->cc_it != CIT_MCOD);

    return addr;
}
