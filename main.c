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
        for (uint16_t addr = 0; addr < size; ) {
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

    for (uint16_t addr = 0; addr < size; ) {
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

static void do_hunks(FILE *ofp, const unsigned char *content, size_t size)
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
}

static void do_simple(const unsigned char *content, size_t size, const char *arg) {
    unsigned start = strtoul(arg, NULL, 0);
    if (start < size) {
        size_t ix_bytes = size * sizeof(int16_t);
        int16_t *glob_index = malloc(ix_bytes);
        if (glob_index) {
            memset(glob_index, 0xff, ix_bytes);
            glob_index[start] = GLOB_CINTCODE;
            disassemble(stdout, content, size, glob_index);
            free(glob_index);
        }
        else
            fputs("ccdis: out of memory allocating global index\n", stderr);
    }
    else
        fprintf(stderr, "ccdis: start %d (%04X) exceeds size %ld (%04lX)\n", start, start, size, size);
}

int main(int argc, char **argv) {
    int status;

    if (argc < 2 || argc > 3) {
        fputs("Usage: ccdis <file> [ <start> ]\n", stderr);
        status = 1;
    } else {
        const char *filename = argv[1];
        int fd = open(filename, O_RDONLY);
        if (fd >= 0) {
            struct stat stb;
            if (fstat(fd, &stb) == 0) {
                size_t size = stb.st_size;
                unsigned char *content = malloc(size);
                if (content) {
                    if (read(fd, content, size) == size) {
                        close(fd);
                        fprintf(stderr, "ccdis: read file %s, %lu bytes\n", filename, size);
                        status = 0;
                        if (argc == 3)
                            do_simple(content, size, argv[2]);
                        else
                            do_hunks(stdout, content, size);
                    } else {
                        fprintf(stderr, "ccdis: error reading file '%s': %s\n", filename, strerror(errno));
                        status = 5;
                        close(fd);
                    }
                } else {
                    fprintf(stderr, "ccdis: not enough memory for file '%s'\n", filename);
                    status = 4;
                    close(fd);
                }
            } else {
                fprintf(stderr, "ccdis: unable to stat file '%s': %s\n",  filename, strerror(errno));
                status = 3;
                close(fd);
            }
        } else {
            fprintf(stderr, "ccdis: unable to open file '%s': %s\n",  filename, strerror(errno));
            status = 2;
        }
    }
    return status;
}
