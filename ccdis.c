#include "ccdis.h"
#include "cintcode_tabs.h"

static void ccdis_glob(FILE *ofp, const cintcode_op *opent, uint16_t globno) {
    if (globno < CINTCODE_NGLOB && cintocde_globs[globno])
        fprintf(ofp, "%-7s %d (%s)\n", opent->mnemonic, globno, cintocde_globs[globno]);
    else
        fprintf(ofp, "%-7s %d\n", opent->mnemonic, globno);
}

static void print_label(FILE *ofp, int16_t *glob_index, uint16_t addr)
{
    int16_t glob = glob_index[addr];
    if (glob >= 0) {
        if (glob == GLOB_JUMP)
            fprintf(ofp, " L%04X: ", addr);
        else if (glob == GLOB_DATA)
            fprintf(ofp, " D%04X: ", addr);
        else if (glob < CINTCODE_NGLOB && cintocde_globs[glob][0])
            fprintf(ofp, "%6.6s: ", cintocde_globs[glob]);
        else
            fprintf(ofp, " G%03d:  ", glob);
    }
    else
        fputs("        ", ofp);
}

static void print_dest_addr(FILE *ofp, int16_t *glob_index, uint16_t addr)
{
    int16_t glob = glob_index[addr];
    if (glob >= 0) {
        if (glob == GLOB_JUMP)
            fprintf(ofp, "L%04X\n", addr);
        else if (glob == GLOB_DATA)
            fprintf(ofp, "D%04X\n", addr);
        else if (glob < CINTCODE_NGLOB && cintocde_globs[glob][0])
            fprintf(ofp, "%s (G%03d)\n", cintocde_globs[glob], glob);
        else
            fprintf(ofp, "G%03d\n", glob);
    }
    else
        fprintf(ofp, "%04X\n", addr);
}

void ccdis(FILE *ofp, const unsigned char *content, uint16_t code_size, int16_t *glob_index)
{
    int new_labels;

    // First scan through the intructions and find the jump destinations

    do {
        new_labels = 0;
        for (uint16_t bytepos = 0; bytepos < code_size; bytepos++) {
            int16_t glob = glob_index[bytepos];
            if (glob >= 0 && glob != GLOB_DATA) {
                const cintcode_op *opent;
                if (glob == GLOB_JUMP)
                    fprintf(ofp, "%04X: starting at jump destination\n", bytepos);
                else
                    fprintf(ofp, "%04X: starting at global #%u\n", bytepos, glob);
                do {
                    uint16_t dest;
                    unsigned b2, b3;
                    unsigned b1 = content[bytepos];
                    opent = cintcode_ops + b1;
                    fprintf(ofp, "%04X: %02X %-5s\n", bytepos++, b1, opent->mnemonic);
                    switch(opent->cc_am) {
                        case CAM_BREL:
                            if (bytepos == code_size)
                                return;
                            b2 = content[bytepos++];
                            dest = bytepos + b2 - 0x80;
                            break;
                        case CAM_BIND:
                            if (bytepos == code_size)
                                return;
                            b2 = content[bytepos++];
                            dest = (bytepos + ((b2<<1)|1)) & 0xfffe;
                            printf("indirect address = %04X\n", dest);
                            if (dest < code_size) {
                                glob_index[dest] = GLOB_DATA;
                                b2 = content[dest++];
                                b3 = content[dest];
                                dest = (dest + (b2 | (b3 << 8))) & 0xfffe;
                                printf("final address = %04X\n", dest);
                            } else
                                dest = 0xffff;
                            break;
                        case CAM_WORD:
                            bytepos++;
                        case CAM_BYTE:
                        case CAM_GLB0:
                        case CAM_GLB1:
                        case CAM_GLB2:
                            bytepos++;
                        default:
                            dest = 0xffff;
                            break;
                        case CAM_SWB:
                            dest = 0xffff;
                            if ((code_size - bytepos) >= 4) {
                                uint16_t pos = (bytepos + 1 ) & 0xfffe;
                                printf("swb word address=%04X\n", pos);
                                b2 = content[pos++];
                                b3 = content[pos++];
                                uint16_t entries = b2 | (b3 << 8);
                                printf("switch with %d entries\n", entries);
                                bytepos = pos + entries * 2;
                                if (bytepos < code_size) {
                                    b2 = content[pos++];
                                    b3 = content[pos++];
                                    dest = pos + (b2 | (b3 << 8)) - 2;
                                    printf("default address=%04X\n", dest);
                                    while (entries--) {
                                        b2 = content[pos++];
                                        b3 = content[pos++];
                                        uint16_t w1 = (b2 | (b3 << 8));
                                        b2 = content[pos++];
                                        b3 = content[pos++];
                                        uint16_t w2 = (b2 | (b3 << 8));
                                        uint16_t sdest = pos + w2 - 2;
                                        printf("switch entry A=%04X, O=%04X, dest=%04X\n", w1, w2, sdest);
                                        glob = glob_index[sdest];
                                        if (glob < 0 || glob == GLOB_DATA) {
                                            glob_index[sdest] = GLOB_JUMP;
                                            new_labels++;
                                        }
                                    }
                                }
                            }
                            break;
                        case CAM_SWL:
                            dest = 0xffff;
                            if ((code_size - bytepos) >= 6) {
                                uint16_t pos = (bytepos + 1 ) & 0xfffe;
                                printf("swb word address=%04X\n", pos);
                                b2 = content[pos++];
                                b3 = content[pos++];
                                uint16_t entries = (b2 | (b3 << 8));
                                b2 = content[pos++];
                                b3 = content[pos++];
                                uint16_t addr = pos + (b2 | (b3 << 8)) - 2;
                                b2 = content[pos++];
                                b3 = content[pos++];
                                uint16_t low = (b2 | (b3 << 8));
                                printf("range switch %d entries, low=%d, addr=%04X\n", entries, low, addr);
                                while (entries--) {
                                    b2 = content[pos++];
                                    b3 = content[pos++];
                                    addr = pos + (b2 | (b3 << 8)) - 2;
                                    printf("%04X: range switch entry addr=%04X\n", pos, addr);
                                    glob = glob_index[addr];
                                    if (glob < 0 || glob == GLOB_DATA) {
                                        glob_index[addr] = GLOB_JUMP;
                                        new_labels++;
                                    }
                                }
                                bytepos = pos;
                            }
                            break;
                    }
                    if (dest < code_size && glob_index[dest] == -1) {
                        glob = glob_index[dest];
                        switch (opent->cc_it) {
                            case CIT_CJMP:
                            case CIT_UJMP:
                                fprintf(ofp, "jump to %04X\n", dest);
                                if (glob < 0 || glob == GLOB_DATA) {
                                    glob_index[dest] = GLOB_JUMP;
                                    new_labels++;
                                }
                                break;
                            default:
                                if (glob < 0) {
                                    glob_index[dest] = GLOB_DATA;
                                    new_labels++;
                                }
                        }
                    }
                } while (bytepos < code_size && opent->cc_it != CIT_RETN && opent->cc_it != CIT_UJMP);
                fputs("hit end or unconditional jump\n", ofp);
                bytepos--;
            }
        }
        fprintf(ofp, "found %d new jump destinations\n", new_labels);
    } while (new_labels > 0);

    // Now go back and disassemble properly.

    for (uint16_t bytepos = 0; bytepos < code_size; bytepos++) {
        int16_t glob = glob_index[bytepos];
        if (glob >= 0) {
            if (glob == GLOB_DATA) {
                do {
                    fprintf(ofp, "%04X         ", bytepos);
                    print_label(ofp, glob_index, bytepos);
                    bytepos = print_data(ofp, content, code_size, glob_index, bytepos);
                    glob = glob_index[bytepos];
                }
                while (bytepos < code_size && (glob < 0 || glob == GLOB_DATA));
            }
            else {
                const cintcode_op *opent;
                do {
                    uint16_t dest, oppos = bytepos;
                    unsigned b1 = content[bytepos++];
                    unsigned b2 = 0;
                    unsigned b3 = 0;
                    opent = cintcode_ops + b1;
                    switch(opent->cc_am) {
                        case CAM_IMP:
                            fprintf(ofp, "%04X %02X      ", oppos, b1);
                            break;
                        case CAM_SWB:
                        case CAM_SWL:
                            fprintf(ofp, "%04X %02X ...  ", oppos, b1);
                            break;
                        case CAM_BYTE:
                        case CAM_BREL:
                        case CAM_BIND:
                        case CAM_GLB0:
                        case CAM_GLB1:
                        case CAM_GLB2:
                            if (bytepos < code_size) {
                                b2 = content[bytepos++];
                                fprintf(ofp, "%04X %02X %02X   ", oppos, b1, b2);
                            }
                            break;
                        case CAM_WORD:
                            if ((bytepos + 1) < code_size) {
                                b2 = content[bytepos++];
                                b3 = content[bytepos++];
                                fprintf(ofp, "%04X %02X %02X %02X", bytepos, b1, b2, b3);
                            }
                    }

                    print_label(ofp, glob_index, oppos);

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
                            dest = bytepos + b2 - 0x80;
                            if (dest < code_size && glob_index[dest] >= 0) {
                                if (glob_index[dest] == GLOB_JUMP)
                                    fprintf(ofp, "%-7s L%04X\n", opent->mnemonic, dest);
                                else
                                    fprintf(ofp, "%-7s G%03d\n", opent->mnemonic, glob_index[dest]);
                            } else
                                fprintf(ofp, "%-7s %04X\n", opent->mnemonic, dest);
                            break;
                        case CAM_BIND:
                            dest = (bytepos + ((b2<<1)|1)) & 0xfffe;
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
                            if ((code_size - bytepos) >= 4) {
                                uint16_t pos = (bytepos + 1 ) & 0xfffe;
                                b2 = content[pos++];
                                b3 = content[pos++];
                                uint16_t entries = b2 | (b3 << 8);
                                bytepos = pos + entries * 2;
                                if (bytepos < code_size) {
                                    b2 = content[pos++];
                                    b3 = content[pos++];
                                    dest = pos - 2 + (b2 | (b3 << 8));
                                    fprintf(ofp, "%-7s %04X\n", opent->mnemonic, entries);
                                    while (entries--) {
                                        b2 = content[pos++];
                                        b3 = content[pos++];
                                        uint16_t tval = (b2 | (b3 << 8));
                                        b2 = content[pos++];
                                        b3 = content[pos++];
                                        fprintf(ofp, "        %04X -> ", tval);
                                        print_dest_addr(ofp, glob_index, pos + (b2 | (b3 << 8)) - 2);
                                    }
                                    fputs("     default -> ", ofp);
                                    print_dest_addr(ofp, glob_index, dest);
                                }
                            }
                            break;
                        case CAM_SWL:
                            if ((code_size - bytepos) >= 6) {
                                uint16_t pos = (bytepos + 1 ) & 0xfffe;
                                b2 = content[pos++];
                                b3 = content[pos++];
                                uint16_t entries = (b2 | (b3 << 8));
                                bytepos = pos + entries * 2 + 4;
                                if (bytepos < code_size) {
                                    b2 = content[pos++];
                                    b3 = content[pos++];
                                    uint16_t addr = pos + (b2 | (b3 << 8)) - 2;
                                    b2 = content[pos++];
                                    b3 = content[pos++];
                                    uint16_t low = (b2 | (b3 << 8));
                                    fprintf(ofp, "%-7s %04X %04X\n", opent->mnemonic, entries, low);
                                    while (entries--) {
                                        b2 = content[pos++];
                                        b3 = content[pos++];
                                        fprintf(ofp, "        %04X -> ", low++);
                                        print_dest_addr(ofp, glob_index, pos + (b2 | (b3 << 8)) - 2);
                                    }
                                    fputs("     default -> ", ofp);
                                    print_dest_addr(ofp, glob_index, addr);
                                }
                            }
                            break;
                    }
                } while (opent->cc_it != CIT_RETN && opent->cc_it != CIT_UJMP);
                bytepos--;
            }
        }
    }
}
