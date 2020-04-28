#include "ccdis.h"
#include "cintcode_tabs.h"

void print_label(FILE *ofp, unsigned addr)
{
    if (addr < MAX_FILE_SIZE) {
        unsigned loc = loc_index[addr];
        if (loc & LOC_GLOBAL) {
            unsigned glob = loc & LOC_GLOBMASK;
            if (glob < CINTCODE_NGLOB)
                fprintf(ofp, " %9.9s: ", cintocde_globs[glob]);
            else
                fprintf(ofp, "      G%03d: ", glob);
        }
        else {
            unsigned usetype = loc & LOC_USETYPE;
            if (usetype == 0)
                fputs("            ", ofp);
            else {
                int label_char = 'D';
                if (usetype != LOC_DATA) {
                    if (loc & LOC_CALL)
                        label_char = 'S';
                    else
                        label_char = 'L';
                }
                fprintf(ofp, "     %c%04X: ", label_char, addr);
            }
        }
    }
}

void print_dest_addr(FILE *ofp, unsigned addr)
{
    if (addr < MAX_FILE_SIZE) {
        unsigned loc = loc_index[addr];
        if (loc & LOC_GLOBAL) {
            unsigned glob = loc & LOC_GLOBMASK;
            if (glob < CINTCODE_NGLOB)
                fprintf(ofp, "%s (G%03d)\n", cintocde_globs[glob], glob);
            else
                fprintf(ofp, "G%03d\n", glob);
        }
        else {
            unsigned usetype = loc & LOC_USETYPE;
            if (usetype == 0)
                fprintf(ofp, "%04X\n", addr);
            else {
                int label_char = 'D';
                if (usetype != LOC_DATA) {
                    if (loc & LOC_CALL)
                        label_char = 'S';
                    else
                        label_char = 'L';
                }
                fprintf(ofp, "%c%04X\n", label_char, addr);
            }
        }
    }
}

static const char xdigs[16] = "0123456789ABCDEF";

unsigned print_data(FILE *ofp, const unsigned char *content, unsigned addr, unsigned max_addr)
{
    do {
        char line[80];
        char *hptr = line + 8;
        char *aptr = hptr + 48;

        fprintf(ofp, "%04X:          ", addr);
        print_label(ofp, addr);

        memcpy(line, "DB      ", 8);
        *aptr++ = ';';
        *aptr++ = ' ';

        for (int i = 0; i < 16 && addr < max_addr; i++) {
            uint8_t val = content[addr++];
            *hptr++ = xdigs[val >> 4];
            *hptr++ = xdigs[val & 0x0f];
            *hptr++ = ' ';
            *aptr++ = (val >= 0x20 && val <= 0x7e) ? val : '.';
            if (loc_index[addr] & LOC_USETYPE) {
                while (++i < 16) {
                    *hptr++ = ' ';
                    *hptr++ = ' ';
                    *hptr++ = ' ';
                }
                break;
            }
        }
        *aptr++ = '\n';
        fwrite(line, aptr-line, 1, ofp);
    }
    while (addr < max_addr && !(loc_index[addr] & LOC_USETYPE));
    return addr;
}
