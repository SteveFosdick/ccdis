#include "ccdis.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

static const unsigned char *ccdir(FILE *ofp, const unsigned char *start, const char *type) {
    int len = *start;
    fputs(type, ofp);
    putc(' ', ofp);
    fwrite(start+1, len, 1, ofp);
    putc('\n', ofp);
    return start + 8;
}

static void disassemble(FILE *ofp, const unsigned char *content, size_t size, int16_t *glob_index)
{
    int new_labels;

    // First scan through the intructions and find the jump destinations

    do {
        new_labels = 0;
        for (size_t addr = 0; addr < size; ) {
            int16_t glob = glob_index[addr];
            if (glob < 0 || glob == GLOB_DATA)
                addr++;
            else if (glob == GLOB_MC6502)
                addr = mc_trace(content, size, glob_index, addr, &new_labels);
            else
                addr = cc_trace(content, size, glob_index, addr, &new_labels);
        }
    }
    while (new_labels > 0);

    // Now go back and disassemble */

    for (size_t addr = 0; addr < size; ) {
        int16_t glob = glob_index[addr];
        if (glob < 0)
            addr++;
        else if (glob == GLOB_DATA)
            addr = print_data(ofp, content, size, glob_index, addr);
        else if (glob == GLOB_MC6502)
            addr = mc_disassemble(ofp, content, size, glob_index, addr);
        else
            addr = cc_disassemble(ofp, content, size, glob_index, addr);
    }
}

static void one_hunk(FILE *ofp, const unsigned char *content, size_t size)
{
    // Search backwards from the end to find the zero marker
    // that separates the code from the list of globals.

    const unsigned char *gend = content + size;
    const unsigned char *gptr = gend;
    do
        gptr -= 4;
    while (gptr > content && (gptr[0] || gptr[1]));
    const unsigned char *code_end = gptr;
    uint16_t code_size = code_end - content;

    // Allocate and populate an index for the code space that indicates
    // for any address which global, if any, points there.

    int16_t *glob_index = malloc(code_size * sizeof(uint16_t));
    if (glob_index) {
        int16_t *glob_ptr = glob_index;
        int16_t *glob_end = glob_index + code_size;
        while (glob_ptr < glob_end)
            *glob_ptr++ = 0xffff;
        for (gptr += 2, gend -= 2; gptr < gend; ) {
            int16_t globno = *gptr++;
            globno |= (*gptr++) << 8;
            uint16_t bytepos = *gptr++;
            bytepos |= (*gptr++) << 8;
            if (bytepos < code_size)
                glob_index[bytepos] = globno;
        }

        // Work forwards, printing section/needs/proc names.

        for (const unsigned char *dptr = content;;) {
            uint16_t data = *dptr++;
            data |= (*dptr++) << 8;
            if (data == 0xfddf)
                dptr = ccdir(ofp, dptr, "SECTION");
            else if (data == 0xfeed)
                dptr = ccdir(ofp, dptr, "NEEDS");
            else if (data == 0xdfdf)
                dptr = ccdir(ofp, dptr, "PROC");
            else
                break;
        }

        // Disassemble the code.

        disassemble(ofp, content, code_size, glob_index);
        free(glob_index);
    }
    else
        fputs("ccdis: out of memory allocating global index\n", stderr);
}

static int do_hunks(FILE *ofp, unsigned char *content, size_t size)
{
    unsigned addr = 0;

    while (size >= 2) {
        unsigned hunk = *content++;
        hunk |= (*content++) << 8;
        if (hunk == 992 || size < 4)
            break;
        unsigned blen = *content++ << 1;
        blen |= (*content++) << 9;
        unsigned ilen = blen + 4;
        unsigned naddr = addr + ilen;
        switch(hunk) {
            case 1000:
                fprintf(ofp, "found CINTCODE hunk from %04X to %04X, len=%u\n", addr, naddr, blen);
                one_hunk(ofp, content, blen);
                break;
            case 1001:
                fprintf(ofp, "found MC hunk from %04X to %04X, len=%u\n", addr, naddr, blen);
                one_hunk(ofp, content, blen);
                break;
            case 1002:
                fprintf(ofp, "found relocation hunk from %04X to %04X, len=%u\n", addr, naddr, blen);
                break;
            default:
                fprintf(ofp, "found other hunk %u from %04X to %04X, len=%u\n", hunk, addr, naddr, blen);
        }
        content += blen;
        addr = naddr;
        size -= ilen;
    }
    return 0;
}

static int do_flat(FILE *ofp, const unsigned char *content, int argc, char **argv)
{
    size_t ix_bytes = MAX_FILE_SIZE * sizeof(int16_t);
    int16_t *glob_index = malloc(ix_bytes);
    if (glob_index) {
        memset(glob_index, 0xff, ix_bytes);
        while (argc--) {
            int16_t glob = GLOB_CINTCODE;
            const char *arg = *argv++;
            char *end;
            long addr = strtol(arg, &end, 0);
            if (*end == ':') {
                int ch = *++end;
                if (ch == 'm' || ch == 'M')
                    glob = GLOB_MC6502;
                else if (ch == 'd' || ch == 'D')
                    glob = GLOB_DATA;
            }
            if (addr < MAX_FILE_SIZE)
                glob_index[addr] = glob;
            else {
                fprintf(stderr, "ccdis: start address %#04lx too large\n", addr);
                free(glob_index);
                return 4;
            }
        }
        disassemble(stdout, content, MAX_FILE_SIZE, glob_index);
        free(glob_index);
    }
    else {
        fputs("ccdis: out of memory allocating global index\n", stderr);
        return 3;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc == 3) {
        fputs("Usage: ccdis <file>\n"
              "       ccdis <file> <load-addr> <start> [ <start> ... ]\n", stderr);
        return 1;
    }
    unsigned load_addr = 0;
    if (argc > 2) {
        load_addr = strtol(argv[2], NULL, 0);
        if (load_addr > MAX_FILE_SIZE) {
            fprintf(stderr, "ccdis: load address %#04x is too high\n", load_addr);
            return 1;
        }
    }
    int status;
    const char *filename = argv[1];
    FILE *ifp = fopen(filename, "rb");
    if (ifp) {
        unsigned char *content = malloc(MAX_FILE_SIZE);
        if (content) {
            size_t avail = MAX_FILE_SIZE - load_addr;
            size_t bytes = fread(content + load_addr, 1, avail, ifp);
            if (ferror(ifp)) {
                fprintf(stderr, "ccdis: read error on file '%s': %s\n", filename, strerror(errno));
                status = 2;
            }
            else if (bytes < avail || feof(ifp)) {
                if (argc > 2)
                    status = do_flat(stdout, content, argc - 3, argv + 3);
                else
                    status = do_hunks(stdout, content, bytes);
            }
            else {
                fprintf(stderr, "ccdis: file '%s' is too big to load at address %#04x\n", filename, load_addr);
                status = 3;
            }
            free(content);
        }
        else {
            fprintf(stderr, "ccdis: out of memory loading file '%s'\n", filename);
            status = 3;
        }
        fclose(ifp);
    }
    else {
        fprintf(stderr, "ccdis: unable to open file '%s' for reading: %s\n", filename, strerror(errno));
        status = 2;
    }
    return status;
}
