#include "ccdis.h"
#include "cintcode_tabs.h"

#define DATA_COLS 8

const print_cfg pf_orig = {
    .lab  = "%s:",
    .byte = "$%02x",
    .word = "$%04x",
    .imm  = "#$%02x",
    .acc  = "",
    .equ  = ".equ",
    .org  = ".org",
    .data = ".byte"
};

const print_cfg pf_beebasm = {
    .lab  = ".%s",
    .byte = "&%02X",
    .word = "&%04X",
    .imm  = "#&%02X",
    .acc  = "A",
    .equ  = "=",
    .org  = "ORG",
    .data = "EQUB"
};

const print_cfg pf_lancs = {
    .lab  = "%s:",
    .byte = "$%02X",
    .word = "$%04X",
    .imm  = "#$%02X",
    .acc  = "",
    .equ  = "EQU",
    .org  = "ORG",
    .data = "DFB"
};

const print_cfg pf_ca65 = {
    .lab  = "%s:",
    .byte = "$%02X",
    .word = "$%04X",
    .imm  = "#$%02X",
    .acc  = "",
    .equ  = "=",
    .org  = ".org",
    .data = ".byte"
};

const print_cfg *pf_current = &pf_orig;
unsigned label_width = MAX_LABEL_LEN;

void print_label(FILE *ofp, unsigned addr)
{
    char label[MAX_LABEL_LEN+1];
    int size = 0;
    if (addr < MAX_FILE_SIZE) {
        unsigned loc = loc_index[addr];
        if (loc & LOC_GLOBAL) {
            unsigned glob = loc & LOC_GLOBMASK;
            if (glob < CINTCODE_NGLOB)
                size = snprintf(label, sizeof(label), pf_current->lab, cintocde_globs[glob]);
            else {
                char globtxt[6];
                snprintf(globtxt, sizeof(globtxt), "G%03d", glob);
                size = snprintf(label, sizeof(label), pf_current->lab, globtxt);
            }
        }
        else if (loc & LOC_LABEL) {
            unsigned labl = loc & LOC_GLOBMASK;
            if (labl < label_names_used)
                size = snprintf(label, sizeof(label), pf_current->lab, label_names[labl]);
        }
        else {
            unsigned usetype = loc & LOC_USETYPE;
            if (usetype != 0) {
                char genlab[7];
                int label_char = 'D';
                if (usetype != LOC_DATA) {
                    if (loc & LOC_CALL)
                        label_char = 'S';
                    else
                        label_char = 'L';
                }
                snprintf(genlab, sizeof(genlab), "%c%04X", label_char, addr);
                size = snprintf(label, sizeof(label), pf_current->lab, genlab);
            }
        }
    }
    if (size < label_width)
        memset(label+size, ' ', label_width-size);
    fwrite(label, label_width, 1, ofp);
}

void print_asm_hdr(FILE *ofp, unsigned start_addr, unsigned size)
{
    char spaces[MAX_LABEL_LEN];
    memset(spaces, ' ', label_width);
    fwrite(spaces, label_width, 1, ofp);
    size_t len = strlen(pf_current->org);
    fwrite(pf_current->org, len, 1, ofp);
    fwrite(spaces, 8-len, 1, ofp);
    fprintf(ofp, pf_current->word, start_addr);
    putc('\n', ofp);

    unsigned end_addr = start_addr + size;
    label_entry *ptr = label_entries;
    label_entry *end = ptr + label_entries_used;
    len = strlen(pf_current->equ);
    while (ptr < end) {
        if (ptr->addr < start_addr || ptr->addr > end_addr) {
            unsigned lnum = ptr->indx & LOC_GLOBMASK;
            if (lnum < label_names_used) {
                const char *lname = label_names[lnum];
                size_t lsize = strlen(lname);
                fwrite(lname, lsize, 1, ofp);
                if (lsize < label_width)
                    fwrite(spaces, label_width - lsize, 1, ofp);
                fwrite(pf_current->equ, len, 1, ofp);
                fwrite(spaces, 8-len, 1, ofp);
                fprintf(ofp, pf_current->word, ptr->addr);
                putc('\n', ofp);
            }
        }
        ptr++;
    }
}

void print_dest_addr_nonl(FILE *ofp, unsigned addr)
{
    if (addr < MAX_FILE_SIZE) {
        unsigned loc = loc_index[addr];
        if (loc & LOC_GLOBAL) {
            unsigned glob = loc & LOC_GLOBMASK;
            if (glob < CINTCODE_NGLOB)
                fprintf(ofp, "%s (G%03d)", cintocde_globs[glob], glob);
            else
                fprintf(ofp, "G%03d", glob);
        }
        else if (loc & LOC_LABEL) {
            unsigned labl = loc & LOC_GLOBMASK;
            if (labl < label_names_used)
                fputs(label_names[labl], ofp);
        }
        else {
            unsigned usetype = loc & LOC_USETYPE;
            if (usetype == 0)
                fprintf(ofp, pf_current->word, addr);
            else {
                int label_char = 'D';
                if (usetype != LOC_DATA) {
                    if (loc & LOC_CALL)
                        label_char = 'S';
                    else
                        label_char = 'L';
                }
                fprintf(ofp, "%c%04X", label_char, addr);
            }
        }
    }
}

void print_dest_addr(FILE *ofp, unsigned addr)
{
    print_dest_addr_nonl(ofp, addr);
    putc('\n', ofp);
}

unsigned print_data(FILE *ofp, const unsigned char *content, unsigned addr, unsigned max_addr)
{
    size_t len = strlen(pf_current->data);
    char spaces[8];
    memset(spaces, ' ', sizeof(spaces));
    do {
        if (!asm_mode)
            fprintf(ofp, "%04X:          ", addr);
        print_label(ofp, addr);
        fwrite(pf_current->data, len, 1, ofp);
        fwrite(spaces, 8-len, 1, ofp);

        unsigned taddr = addr;
        uint8_t val = content[taddr++];
        size_t size = fprintf(ofp, pf_current->byte, val);
        for (int i = 0; i < (DATA_COLS-1) && taddr < max_addr; i++) {
            if (loc_index[addr] & LOC_USETYPE) {
                size += 2;
                while (++i < DATA_COLS)
                    fwrite(spaces, size, 1, ofp);
                break;
            }
            uint8_t val = content[taddr++];
            fwrite(", ", 2, 1, ofp);
            fprintf(ofp, pf_current->byte, val);
        }
        fputs(" ; ", ofp);
        for (int i = 0; i < DATA_COLS && addr < max_addr; i++) {
            uint8_t val = content[addr++];
            if (val < 0x20 || val > 0x7e)
                val = '.';
            putc(val, ofp);
            if (loc_index[addr] & LOC_USETYPE)
                break;
        }
        putc('\n', ofp);
    }
    while (addr < max_addr && !(loc_index[addr] & LOC_USETYPE));
    return addr;
}
