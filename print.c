#include "ccdis.h"
#include "cintcode_tabs.h"

void print_label(FILE *ofp, int16_t *glob_index, uint16_t addr)
{
    int16_t glob = glob_index[addr];
    if (glob >= 0) {
        if (glob == GLOB_CINTCODE || glob == GLOB_MC6502)
            fprintf(ofp, "     L%04X: ", addr);
        else if (glob == GLOB_DATA)
            fprintf(ofp, "     D%04X: ", addr);
        else if (glob < CINTCODE_NGLOB && *cintocde_globs[glob].name)
            fprintf(ofp, " %9.9s: ", cintocde_globs[glob].name);
        else
            fprintf(ofp, "      G%03d: ", glob);
    }
    else
        fputs("            ", ofp);
}

static const char xdigs[16] = "0123456789ABCDEF";

uint16_t print_data(FILE *ofp, const unsigned char *content, uint16_t code_size, int16_t *glob_index, uint16_t addr)
{
    do {
        char line[80];
        char *hptr = line + 8;
        char *aptr = hptr + 48;

        fprintf(ofp, "%04X:          ", addr);
        print_label(ofp, glob_index, addr);

        memcpy(line, "DB      ", 8);
        *aptr++ = ';';
        *aptr++ = ' ';

        for (int i = 0; i < 16 && addr < code_size; i++) {
            uint8_t val = content[addr++];
            *hptr++ = xdigs[val >> 4];
            *hptr++ = xdigs[val & 0x0f];
            *hptr++ = ' ';
            *aptr++ = (val >= 0x20 && val <= 0x7e) ? val : '.';
            if (glob_index[addr] >= 0) {
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
    while (addr < code_size && glob_index[addr] < 0);
    return addr;
}
