#include "ccdis.h"
#include "cintcode_tabs.h"

static void ccdis_glob(FILE *ofp, const cintcode_op *opent, uint16_t globno) {
    if (globno < CINTCODE_NGLOB && cintocde_globs[globno])
        fprintf(ofp, "%-7s %d (%s)\n", opent->mnemonic, globno, cintocde_globs[globno]);
    else
        fprintf(ofp, "%-7s %d\n", opent->mnemonic, globno);
}

void ccdis(FILE *ofp, const unsigned char *content, uint16_t code_size, int16_t *glob_index)
{
    int new_labels;
    uint16_t bytepos, dest;
    unsigned b1, b2, b3;
    const cintcode_op *opent;

    // First scan through the intructions and find the jump destinations

    do {
        new_labels = 0;
        for (bytepos = 0; bytepos < code_size; bytepos++) {
            if (glob_index[bytepos] >= 0 && glob_index[bytepos] != 0x2000) {
                fprintf(ofp, "%04X: starting at global #%u\n", bytepos, glob_index[bytepos]);
                do {
                    b1 = content[bytepos++];
                    opent = cintcode_ops + b1;
                    fprintf(ofp, "%s am %d it %d\n", opent->mnemonic, opent->cc_am, opent->cc_it);
                    switch(opent->cc_am) {
                        case CAM_BREL:
                            if (bytepos == code_size)
                                return;
                            b2 = content[bytepos++];
                            if (b2 & 0x80)
                                dest = bytepos + (b2 & 0x7f);
                            else {
                                dest = bytepos + b2;
                                dest -= 0x80;
                            }
                            break;
                        case CAM_BIND:
                            if (bytepos == code_size)
                                return;
                            b2 = content[bytepos++];
                            dest = bytepos + (b2 << 1);
                            if (dest < code_size) {
                                glob_index[dest] = 0x2000;
                                b2 = content[dest++];
                                b3 = content[dest];
                                dest = bytepos + (b2 | (b3 << 8));
                            } else
                                dest = 0xffff;
                            break;
                        default:
                            dest = 0xffff;
                    }
                    if (dest < code_size && glob_index[dest] == -1) {
                        switch (opent->cc_it) {
                            case CIT_CJMP:
                            case CIT_UJMP:
                                fprintf(ofp, "jump from %04X to %04X\n", bytepos, dest);
                                glob_index[dest] = 0x4000;
                                new_labels++;
                                break;
                            default:
                                glob_index[dest] = 0x2000;
                                new_labels++;
                        }
                    }
                } while (opent->cc_it != CIT_RETN && opent->cc_it != CIT_UJMP);
                bytepos--;
            }
        }
        fprintf(ofp, "found %d new jump destinations\n", new_labels);
    } while (new_labels > 0);

    // Now go back and disassemble properly.

    for (bytepos = 0; bytepos < code_size; bytepos++) {
        if (glob_index[bytepos] >= 0 && glob_index[bytepos] != 0x2000) {
            do {
                b2 = b3 = 0;
                b1 = content[bytepos];
                opent = cintcode_ops + b1;
                switch(opent->cc_am) {
                    case CAM_IMP:
                        fprintf(ofp, "%04X %02X      ", bytepos, b1);
                        break;
                    case CAM_BYTE:
                    case CAM_BREL:
                    case CAM_BIND:
                    case CAM_GLB0:
                    case CAM_GLB1:
                    case CAM_GLB2:
                        dest = bytepos + 1;
                        if (dest == code_size)
                            return;
                        b2 = content[dest];
                        fprintf(ofp, "%04X %02X %02X   ", bytepos, b1, b2);
                        break;
                    case CAM_WORD:
                        dest = bytepos + 2;
                        if (dest == code_size)
                            return;
                        b3 = content[dest--];
                        b2 = content[dest];
                        fprintf(ofp, "%04X %02X %02X %02X", bytepos, b1, b2, b3);
                }
                if (glob_index[bytepos] >= 0) {
                    if (glob_index[bytepos] == 0x4000)
                        fprintf(ofp, " L%04X: ", bytepos);
                    else if (glob_index[bytepos] == 0x2000)
                        fprintf(ofp, " D%04X: ", bytepos);
                    else
                        fprintf(ofp, " G%03d:  ", glob_index[bytepos]);
                }
                else
                    fputs("        ", ofp);
                switch(opent->cc_am) {
                    case CAM_IMP:
                        fputs(opent->mnemonic, ofp);
                        putc('\n', ofp);
                        bytepos++;
                        break;
                    case CAM_BYTE:
                        fprintf(ofp, "%-7s %u\n", opent->mnemonic, b2);
                        bytepos += 2;
                        break;
                    case CAM_WORD:
                        fprintf(ofp, "%-7s %u\n", opent->mnemonic, (b3 << 8)|b2);
                        bytepos += 3;
                        break;
                    case CAM_BREL:
                        dest = bytepos + 2;
                        if (b2 & 0x80)
                            dest += b2 & 0x7f;
                        else {
                            dest += b2;
                            dest -= 0x80;
                        }
                        if (dest < code_size && glob_index[dest] >= 0) {
                            if (glob_index[dest] == 0x4000)
                                fprintf(ofp, "%-7s L%04X\n", opent->mnemonic, dest);
                            else
                                fprintf(ofp, "%-7s G%03d\n", opent->mnemonic, glob_index[dest]);
                        } else
                            fprintf(ofp, "%-7s %04X\n", opent->mnemonic, dest);
                        bytepos += 2;
                        break;
                    case CAM_BIND:
                        dest = bytepos + (b2 << 1);
                        fprintf(ofp, "%-7s (%04X)\n", opent->mnemonic, dest);
                        bytepos += 2;
                    case CAM_GLB0:
                        ccdis_glob(ofp, opent, b2);
                        bytepos += 2;
                        break;
                    case CAM_GLB1:
                        ccdis_glob(ofp, opent, 256 + b2);
                        bytepos += 2;
                        break;
                    case CAM_GLB2:
                        ccdis_glob(ofp, opent, 512 + b2);
                        bytepos += 2;
                        break;
                }
            } while (opent->cc_it != CIT_RETN && opent->cc_it != CIT_UJMP);
            bytepos--;
        }
    }
}
