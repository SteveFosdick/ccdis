#include "ccdis.h"
#include "cintcode_tabs.h"

#define DATA_COLS 8

static unsigned print_str_default(FILE *ofp, const unsigned char *content, unsigned addr, unsigned max_addr);
static unsigned print_str_beebasm(FILE *ofp, const unsigned char *content, unsigned addr, unsigned max_addr);
static unsigned print_str_lancs(FILE *ofp, const unsigned char *content, unsigned addr, unsigned max_addr);

const print_cfg pf_orig = {
    .lab  = "%s:",
    .byte = "$%02x",
    .word = "$%04x",
    .imm  = "#$%02x",
    .acc  = "",
    .equ  = ".equ",
    .org  = ".org",
    .dfb  = ".byte",
    .dfw  = ".word",
    .str  = print_str_default
};

const print_cfg pf_beebasm = {
    .lab  = ".%s",
    .byte = "&%02X",
    .word = "&%04X",
    .imm  = "#&%02X",
    .acc  = "A",
    .equ  = "=",
    .org  = "ORG",
    .dfb  = "EQUB",
    .dfw  = "EQUW",
    .str  = print_str_beebasm
};

const print_cfg pf_lancs = {
    .lab  = "%s:",
    .byte = "$%02X",
    .word = "$%04X",
    .imm  = "#$%02X",
    .acc  = "",
    .equ  = "EQU",
    .org  = "ORG",
    .dfb  = "DFB",
    .dfw  = "DFW",
    .ddb  = "DDB",
    .str  = print_str_lancs
};

const print_cfg pf_ca65 = {
    .lab  = "%s:",
    .byte = "$%02X",
    .word = "$%04X",
    .imm  = "#$%02X",
    .acc  = "",
    .equ  = "=",
    .org  = ".org",
    .dfb  = ".byte",
    .dfw  = ".word",
    .ddb  = ".dbyt",
    .str  = print_str_default
};

const print_cfg pf_tpasm = {
    .lab  = "%s:",
    .byte = "$%02X",
    .word = "$%04X",
    .imm  = "#$%02X",
    .acc  = "",
    .equ  = ".equ",
    .org  = ".org",
    .dfb  = "db",
    .dfw  = "dw",
    .str  = print_str_default
};

const print_cfg *pf_current = &pf_orig;
unsigned label_width = MAX_LABEL_LEN;

void print_spaces(const char *spaces, int count, FILE *ofp)
{
    while (count >= MAX_LABEL_LEN) {
        fwrite(spaces, MAX_LABEL_LEN, 1, ofp);
        count -= MAX_LABEL_LEN;
    }
    if (count > 0)
        fwrite(spaces, count, 1, ofp);
}

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
                char globtxt[7];
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
    fwrite(label, size, 1, ofp);
    memset(label, ' ', MAX_LABEL_LEN);
    print_spaces(label, label_width - size, ofp);
}

void print_asm_hdr(FILE *ofp, unsigned start_addr, unsigned size)
{
    char spaces[MAX_LABEL_LEN];
    memset(spaces, ' ', MAX_LABEL_LEN);
    print_spaces(spaces, label_width, ofp);
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

static const char spaces[8] = "        ";

enum str_state {
    SS_START,
    SS_INSTR,
    SS_BYTES
};

static unsigned print_str_common(FILE *ofp, const unsigned char *content, unsigned addr, unsigned max_addr, const char *str, size_t str_len)
{
    size_t dfb_len = strlen(pf_current->dfb);
    enum str_state state = SS_START;
    do {
        uint8_t val = content[addr];
        if (val >= ' ' && val <= 0x7e && val != '"') {
            switch(state) {
                case SS_BYTES:
                    putc('\n', ofp);
                    // FALLTHROUGH
                case SS_START:
                    if (!asm_mode)
                        fprintf(ofp, "%04X:          ", addr);
                    print_label(ofp, addr);
                    fwrite(str, str_len, 1, ofp);
                    fwrite(spaces, 8-str_len, 1, ofp);
                    putc('"', ofp);
                    state = SS_INSTR;
                    // FALLTHROUGH
                case SS_INSTR:
                    putc(val, ofp);
            }
        }
        else {
            switch(state) {
                case SS_INSTR:
                    fputs("\"\n", ofp);
                    // FALLTHROUGH
                case SS_START:
                    if (!asm_mode)
                        fprintf(ofp, "%04X:          ", addr);
                    print_label(ofp, addr);
                    fwrite(pf_current->dfb, dfb_len, 1, ofp);
                    fwrite(spaces, 8-dfb_len, 1, ofp);
                    fprintf(ofp, pf_current->byte, val);
                    state = SS_BYTES;
                    break;
                case SS_BYTES:
                    fwrite(", ", 2, 1, ofp);
                    fprintf(ofp, pf_current->byte, val);
            }
        }
        addr++;
    }
    while (addr < max_addr && !(loc_index[addr] & LOC_USETYPE));
    switch(state) {
        case SS_START:
            break;
        case SS_INSTR:
            fputs("\"\n", ofp);
            break;
        case SS_BYTES:
            putc('\n', ofp);
    }
    return addr;
}

static unsigned print_str_default(FILE *ofp, const unsigned char *content, unsigned addr, unsigned max_addr)
{
    return print_str_common(ofp, content, addr, max_addr, pf_current->dfb, strlen(pf_current->dfb));
}

static unsigned print_str_beebasm(FILE *ofp, const unsigned char *content, unsigned addr, unsigned max_addr)
{
    return print_str_common(ofp, content, addr, max_addr, "EQUS", 4);
}

static unsigned lancs_string(FILE *ofp, const unsigned char *content, unsigned addr, unsigned max_addr, const char *spaces, int quote)
{
    bool crterm = false;
    unsigned size = max_addr - addr;
    for (unsigned taddr = addr; taddr < max_addr; ) {
        uint8_t val = content[taddr++];
        if (val < ' ' || val >= 0xa0 || val == '|' || val == '^')
            size++;
    }
    if (size > 80) {
        fwrite("ASC", 3, 1, ofp);
        fwrite(spaces, 8-3, 1, ofp);
    }
    else if (content[addr] == (max_addr-addr)) {
        if (content[max_addr-1] == 0x0d) {
            fwrite("CSTR", 4, 1, ofp);
            max_addr--;
            crterm = true;
        }
        else
            fwrite("CASC", 4, 1, ofp);
        addr++;
        fwrite(spaces, 8-4, 1, ofp);
    }
    else {
        if (content[max_addr-1] == 0x0d) {
            fwrite("STR", 3, 1, ofp);
            max_addr--;
            crterm = true;
        }
        else
            fwrite("ASC", 3, 1, ofp);
        fwrite(spaces, 8-3, 1, ofp);
    }
    putc(quote, ofp);
    unsigned posn = 0;
    do {
        uint8_t val = content[addr++];
        if (val < ' ') {
            putc('|', ofp);
            putc(val + '@', ofp);
            posn += 2;
        }
        else if (val >= 0xa0) {
            putc('^', ofp);
            putc(val & 0x7f, ofp);
            posn += 2;
        }
        else {
            if (val == '|' || val == '^') {
                putc(val, ofp);
                posn++;
            }
            putc(val, ofp);
            posn++;
        }
    } while (addr < max_addr && posn <= 80);
    putc(quote, ofp);
    putc('\n', ofp);
    if (crterm)
        addr++;
    return addr;
}

static unsigned print_str_lancs(FILE *ofp, const unsigned char *content, unsigned addr, unsigned max_addr)
{
    char spaces[8];
    memset(spaces, ' ', sizeof(spaces));

    uint8_t val = content[addr];
    do {
        if (!asm_mode)
            fprintf(ofp, "%04X:          ", addr);
        print_label(ofp, addr);
        if (val >= ' ' && val < 0x7f) {
            unsigned taddr = addr;
            unsigned quote_sing = 0;
            unsigned quote_doub = 0;
            do {
                if (val == '"') {
                    if (!quote_doub)
                        quote_doub = taddr;
                }
                else if (val == '\'') {
                    if (!quote_sing)
                        quote_sing = taddr;
                }
                val = content[++taddr];
            }
            while ((val < 0x7f || (val >= 0xa0 && val < 0xff)) && taddr < max_addr && !(loc_index[taddr] & LOC_USETYPE));
            if (!quote_doub)
                addr = lancs_string(ofp, content, addr, taddr, spaces, '"');
            else if (!quote_sing)
                addr = lancs_string(ofp, content, addr, taddr, spaces, '\'');
            else if (quote_sing < quote_doub)
                addr = lancs_string(ofp, content, addr, quote_doub, spaces, '"');
            else
                addr = lancs_string(ofp, content, addr, quote_sing, spaces, '\'');
            val = content[addr];
        }
        else {
            fwrite(pf_current->dfb, 3, 1, ofp);
            fwrite(spaces, 8-3, 1, ofp);
            fprintf(ofp, pf_current->byte, val);
            while (++addr < max_addr && !(loc_index[addr] & LOC_USETYPE)) {
                val = content[addr];
                if (val >= ' ' && val < 0x7f)
                    break;
                putc(',', ofp);
                fprintf(ofp, pf_current->byte, val);
            }
            putc('\n', ofp);
        }
    }
    while (addr < max_addr && !(loc_index[addr] & LOC_USETYPE));
    return addr;
}

static unsigned print_words(FILE *ofp, const unsigned char *content, unsigned addr, unsigned max_addr)
{
    size_t dfw_len = strlen(pf_current->dfw);
    char spaces[8];
    memset(spaces, ' ', sizeof(spaces));
    do {
        if (!asm_mode)
            fprintf(ofp, "%04X:          ", addr);
        print_label(ofp, addr);
        fwrite(pf_current->dfw, dfw_len, 1, ofp);
        fwrite(spaces, 8-dfw_len, 1, ofp);
        uint16_t val = content[addr++];
        val |= (content[addr++]) << 8;
        print_dest_addr_nonl(ofp, val);
        for (int i = 0; i < (DATA_COLS-1) && addr < max_addr; i++) {
            if (loc_index[addr] & LOC_USETYPE)
                break;
            val = content[addr++];
            val |= (content[addr++]) << 8;
            fwrite(", ", 2, 1, ofp);
            print_dest_addr_nonl(ofp, val);
        }
        putc('\n', ofp);
    }
    while (addr < max_addr && !(loc_index[addr] & LOC_USETYPE));
    return addr;
}

static unsigned print_bytes(FILE *ofp, const unsigned char *content, unsigned addr, unsigned max_addr)
{
    size_t dfb_len = strlen(pf_current->dfb);
    do {
        if (!asm_mode)
            fprintf(ofp, "%04X:          ", addr);
        print_label(ofp, addr);
        fwrite(pf_current->dfb, dfb_len, 1, ofp);
        fwrite(spaces, 8-dfb_len, 1, ofp);

        unsigned taddr = addr;
        uint8_t val = content[taddr++];
        size_t size = fprintf(ofp, pf_current->byte, val);
        for (int i = 0; i < (DATA_COLS-1) && taddr < max_addr; i++) {
            if (loc_index[taddr] & LOC_USETYPE) {
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

static unsigned print_dbytes(FILE *ofp, const unsigned char *content, unsigned addr, unsigned max_addr)
{
    if (!pf_current->ddb[0])
        return print_bytes(ofp, content, addr, max_addr);
    size_t ddb_len = strlen(pf_current->ddb);
    do {
        if (!asm_mode)
            fprintf(ofp, "%04X:          ", addr);
        print_label(ofp, addr);
        uint16_t val = content[addr++] << 8;
        val |= (content[addr++]);
        fwrite(pf_current->ddb, ddb_len, 1, ofp);
        fwrite(spaces, 8-ddb_len, 1, ofp);
        print_dest_addr_nonl(ofp, val);
        for (int i = 0; i < (DATA_COLS-1) && addr < max_addr; i++) {
            if (loc_index[addr] & LOC_USETYPE)
                break;
            val = content[addr++] << 8;
            val |= (content[addr++]);
            fwrite(", ", 2, 1, ofp);
            print_dest_addr_nonl(ofp, val);
        }
        putc('\n', ofp);
    }
    while (addr < max_addr && !(loc_index[addr] & LOC_USETYPE));
    return addr;
}

unsigned print_data(FILE *ofp, const unsigned char *content, unsigned addr, unsigned max_addr)
{
    unsigned loc_use = loc_index[addr];
    if (loc_use & LOC_STRING)
        return pf_current->str(ofp, content, addr, max_addr);
    else if (loc_use & LOC_WORD)
        return print_words(ofp, content, addr, max_addr);
    else if (loc_use & LOC_DBYTE)
        return print_dbytes(ofp, content, addr, max_addr);
    else
        return print_bytes(ofp, content, addr, max_addr);
}
