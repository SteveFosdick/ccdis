#include "ccdis.h"
#include "cintcode_tabs.h"
#include "rom_tab.h"
#include <argp.h>
#include <fcntl.h>
#include <unistd.h>

uint16_t loc_index[MAX_FILE_SIZE];
unsigned char content[MAX_FILE_SIZE+1];
unsigned base_addr = 0;
static bool rom_flag = false;

static unsigned print_directive(unsigned addr, const char *type)
{
    unsigned len = content[addr];
    fputs(type, stdout);
    putchar(' ');
    fwrite(content+addr+1, len, 1, stdout);
    putchar('\n');
    return addr + 8;
}

static void setup_index(void)
{
    memset(loc_index, 0, sizeof(loc_index));
    if (rom_flag) {
        rom_entry *end = rom_table + ROM_TAB_SIZE;
        for (rom_entry *ptr = rom_table; ptr < end; ptr++)
            loc_index[ptr->romaddr] = ptr->locindx;
    }
    labels_apply();
}

static int disassemble(FILE *ofp, const unsigned char *content, unsigned start_addr, unsigned size)
{
    unsigned max_addr = start_addr + size;
    unsigned new_labels;

    // First scan through the intructions and find the jump destinations

    do {
        new_labels = 0;
        for (unsigned addr = start_addr; addr < max_addr; ) {
            unsigned usetype = loc_index[addr] & LOC_USETYPE;
            if (usetype == LOC_M6502)
                addr = mc_trace(content, start_addr, addr, max_addr, &new_labels);
            else if (usetype == LOC_CINTCODE)
                addr = cc_trace(content, start_addr, addr, max_addr, &new_labels);
            else
                addr++;
        }
    }
    while (new_labels > 0);

    // Now go back and disassemble */

    for (unsigned addr = start_addr; addr < max_addr; ) {
        unsigned usetype = loc_index[addr] & LOC_USETYPE;
        if (usetype == LOC_M6502)
            addr = mc_disassemble(ofp, content, addr, max_addr);
        else if (usetype == LOC_CINTCODE)
            addr = cc_disassemble(ofp, content, addr, max_addr);
        else if (usetype == LOC_DATA)
            addr = print_data(ofp, content, addr, max_addr);
        else
            addr++;
    }
    return 0;
}

static int one_hunk(FILE *ofp, const char *fn, unsigned addr, unsigned size)
{
    setup_index();

    /* Process the list of globals for this hunk by working backwards
     * from the end.  A word of zero is the delimiter.
     */

    const unsigned char *gmin = content + addr + 4;
    const unsigned char *gptr = gmin + size - 6;
    while (gptr > gmin) {
        gptr -= 4;
        unsigned offset = gptr[2] | (gptr[3] << 8);
        if (!offset)
            break;
        unsigned globno = gptr[0] | (gptr[1] << 8);
        if (globno < MAX_GLOB_NO)
            loc_index[offset] = LOC_CINTCODE|LOC_GLOBAL|globno;
        else {
            fprintf(stderr, "ccdis: file '%s': global number %u too big\n", fn, globno);
            return 5;
        }
    }

    /* Work forwards, printing section/needs/proc names. */

    size = gptr + 2 - gmin;
    for (unsigned taddr = addr; taddr < size; ) {
        unsigned b1 = content[taddr++];
        unsigned b2 = content[taddr++];
        unsigned data = b1 | (b2 << 8);
        if (data == 0xfddf)
            taddr = print_directive(taddr, "SECTION");
        else if (data == 0xfeed)
            taddr = print_directive(taddr, "NEEDS");
        else if (data == 0xdfdf)
            taddr = print_directive(taddr, "PROC");
        else
            break;
    }

    /* Disassemble the code. */

    disassemble(ofp, content + addr, 0, size);
    return 0;
}

static int check_hunks(const char *fn, unsigned size)
{
    /* Go through the chain of hunks to make sure this is a valid
     * CINTCODE hunk file.  This saves trying to disassemble the
     * wrong type of file.
     */

    unsigned const char *end = content + size - 2;
    for (const unsigned char *ptr = content; ptr <= end; ) {
        unsigned b1 = *ptr++;
        unsigned b2 = *ptr++;
        unsigned hunk = b1 | (b2 << 8);
        if (hunk == 992 && ptr >= end)
            return 0;
        if (ptr == end)
            break;
        b1 = *ptr++;
        b2 = *ptr++;
        unsigned blen = (b1 << 1) | (b2 << 9);
        const unsigned char *next = ptr + blen;
        if (blen == 0 || next > end)
            break;
        ptr = next;
    }
    fprintf(stderr, "ccdis: corrupt hunk chain, maybe '%s' is not a CINTCODE file\n", fn);
    return 6;
}

static int iterate_hunks(FILE *ofp, const char *fn, unsigned start_addr, unsigned size)
{
    int status;

    if ((status = check_hunks(fn, size)) == 0) {
        /* Now go back and disassemble the hunks */

        for (unsigned addr = start_addr; size >= 2; ) {
            unsigned hunk = content[addr] | (content[addr+1] << 8);
            if (hunk == 992 || size < 4)
                break;
            unsigned blen = (content[addr+2] << 1) | (content[addr+3] << 9);
            unsigned ilen = blen + 4;
            unsigned naddr = addr + ilen;
            switch(hunk) {
                case 1000:
                    fprintf(ofp, "found CINTCODE hunk from %04X to %04X, len=%04X\n", addr, naddr, blen);
                    status = one_hunk(ofp, fn, addr+4, blen);
                    break;
                case 1001:
                    fprintf(ofp, "found MC hunk from %04X to %04X, len=%04X\n", addr, naddr, blen);
                    status = one_hunk(ofp, fn, addr+4, blen);
                    break;
                case 1002:
                    fprintf(ofp, "found relocation hunk from %04X to %04X, len=%04X\n", addr, naddr, blen);
                    break;
                default:
                    fprintf(ofp, "found other hunk %u from %04X to %04X, len=%04X\n", hunk, addr, naddr, blen);
            }
            if (status)
                break;
            addr = naddr;
            size -= ilen;
        }
    }
    return status;
}

static int simple_file(FILE *ofp, const char *fn, unsigned start_addr, unsigned size)
{
    setup_index();
    return disassemble(ofp, content, base_addr, size);
}

static int (*file_func)(FILE *ofp, const char *fn, unsigned start_addr, unsigned size) = iterate_hunks;

static int process_file(FILE *ofp, const char *fn)
{
    int status;
    int fd = open(fn, O_RDONLY);
    if (fd >= 0) {
        /* Try to read one more character than max size to detect too big */
        size_t max_bytes = MAX_FILE_SIZE - base_addr + 1;
        ssize_t got_bytes = read(fd, content + base_addr, max_bytes);
        if (got_bytes < 0) {
            fprintf(stderr, "ccdis: read error on file '%s': %s\n", fn, strerror(errno));
            status = 2;
        }
        else if (got_bytes == max_bytes) {
            fprintf(stderr, "ccdis: file '%s' is too big\n", fn);
            status = 3;
        }
        else
            status = file_func(ofp, fn, base_addr, got_bytes);
        close(fd);
    }
    else {
        fprintf(stderr, "ccdis: unable to open file '%s' for reading: %s\n", fn, strerror(errno));
        status = 2;
    }
    return status;
}

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    switch(key) {
        case 'b':
            if ((base_addr = strtoul(arg, NULL, 0)) > MAX_FILE_SIZE) {
                argp_error(state, "base address %04X is too high", base_addr);
                return EINVAL;
            }
            break;
        case 'h':
            file_func = iterate_hunks;
            break;
        case 'l':
            return labels_load(arg);
        case 'r':
            rom_flag = true;
            break;
        case 's':
            file_func = simple_file;
            break;
        case ARGP_KEY_ARG:
            process_file(stdout, arg);
            break;
        case ARGP_KEY_NO_ARGS:
            argp_error(state, "nothing to do");
            return EINVAL;
    }
    return 0;
}

static const struct argp_option opts[] = {
    { "base",   'b', "address", 0, "base address to load subsequent files at" },
    { "hunk",   'h', NULL,      0, "following files are in CINTCODE hunk format" },
    { "labels", 'l', "file",    0, "file of labels" },
    { "rom",    'r', NULL,      0, "pre-load the BCPL ROM globals" },
    { "simple", 's', NULL,      0, "following files are simple binary" },
    { 0 }
};

static const struct argp parser = { opts, parse_opt };

int main(int argc, char **argv)
{
    int err = argp_parse(&parser, argc, argv, 0, NULL, NULL);
    return err == 0 ? 0 : err == EINVAL ? 1 : err == ENOMEM ? 2 : 3;
}
